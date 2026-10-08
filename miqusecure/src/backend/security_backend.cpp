#include "security_backend.hpp"
#include <array>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <deque>
#include <unordered_set>
#include <unordered_map>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <unistd.h>

#include <mutex>
#include <miqutoolkit/core/string_utils.hpp>

namespace fs = std::filesystem;

namespace miqusecure {

static std::string run_cmd_capture(const std::string& cmd) {
    std::array<char, 256> buffer;
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }
    return result;
}

static std::string trim(const std::string& str) {
    return miqu::StringUtils::trim(str);
}

std::string SecurityBackend::get_active_connection() {
    std::string out = run_cmd_capture("nmcli -t -f NAME,TYPE connection show --active 2>/dev/null | grep -v \":loopback\" | head -n 1 | cut -d: -f1");
    return trim(out);
}

FirewallInfo SecurityBackend::read_firewall() {
    FirewallInfo info;

    // 1. Read /etc/ufw/ufw.conf
    std::ifstream conf("/etc/ufw/ufw.conf");
    if (conf.is_open()) {
        std::string line;
        while (std::getline(conf, line)) {
            line = trim(line);
            if (line.rfind("ENABLED=", 0) == 0) {
                std::string val = line.substr(8);
                info.active = (val == "yes" || val == "YES");
            }
        }
    } else {
        std::string active_str = run_cmd_capture("systemctl is-active ufw 2>/dev/null");
        info.active = (active_str == "active");
    }

    info.status_text = info.active ? "Active" : "Inactive";

    // 2. Read /etc/default/ufw for default incoming/outgoing policies
    std::ifstream def_conf("/etc/default/ufw");
    if (def_conf.is_open()) {
        std::string line;
        while (std::getline(def_conf, line)) {
            line = trim(line);
            if (line.rfind("DEFAULT_INPUT_POLICY=", 0) == 0) {
                std::string val = line.substr(21);
                if (!val.empty() && val.front() == '"') val = val.substr(1);
                if (!val.empty() && val.back() == '"') val.pop_back();
                info.default_incoming = val;
            } else if (line.rfind("DEFAULT_OUTPUT_POLICY=", 0) == 0) {
                std::string val = line.substr(22);
                if (!val.empty() && val.front() == '"') val = val.substr(1);
                if (!val.empty() && val.back() == '"') val.pop_back();
                info.default_outgoing = val;
            }
        }
    }

    info.strict_mode = (info.default_incoming == "REJECT");
    info.active_network_name = get_active_connection();
    if (!info.active_network_name.empty()) {
        std::string metered_str = run_cmd_capture("nmcli -t -f connection.metered connection show \"" + info.active_network_name + "\" 2>/dev/null | cut -d: -f2");
        metered_str = trim(metered_str);
        info.metered = (metered_str == "yes");
    }

    // 3. Read /etc/ufw/user.rules
    std::ifstream rules_file("/etc/ufw/user.rules");
    if (rules_file.is_open()) {
        std::string line;
        int rule_id = 1;
        while (std::getline(rules_file, line)) {
            line = trim(line);
            if (line.empty() || line[0] == '#' || line[0] == '*') continue;

            if (line.rfind("-A ufw-user-input", 0) == 0) {
                FirewallRule rule;
                rule.id = rule_id++;
                rule.raw_line = line;
                rule.direction = "IN";

                if (line.find("-j ACCEPT") != std::string::npos) {
                    rule.action = "ALLOW";
                } else if (line.find("-j DROP") != std::string::npos) {
                    rule.action = "DENY";
                } else if (line.find("-j REJECT") != std::string::npos) {
                    rule.action = "REJECT";
                }

                size_t proto_pos = line.find("-p ");
                if (proto_pos != std::string::npos) {
                    size_t space_pos = line.find(' ', proto_pos + 3);
                    if (space_pos != std::string::npos) {
                        rule.protocol = line.substr(proto_pos + 3, space_pos - (proto_pos + 3));
                    }
                }

                size_t dport_pos = line.find("--dport ");
                if (dport_pos != std::string::npos) {
                    size_t space_pos = line.find(' ', dport_pos + 8);
                    if (space_pos != std::string::npos) {
                        rule.port_or_service = line.substr(dport_pos + 8, space_pos - (dport_pos + 8));
                    } else {
                        rule.port_or_service = line.substr(dport_pos + 8);
                    }
                } else {
                    rule.port_or_service = "All Traffic";
                }

                size_t s_pos = line.find("-s ");
                if (s_pos != std::string::npos) {
                    size_t space_pos = line.find(' ', s_pos + 3);
                    if (space_pos != std::string::npos) {
                        rule.source = line.substr(s_pos + 3, space_pos - (s_pos + 3));
                    } else {
                        rule.source = line.substr(s_pos + 3);
                    }
                } else {
                    rule.source = "Anywhere";
                }

                if (rule.action == "ALLOW") {
                    if (rule.port_or_service == "22" || rule.port_or_service == "ssh") info.ssh_allowed = true;
                    if (rule.port_or_service == "8080") info.web_dev_allowed = true;
                    if (rule.port_or_service == "22000") info.syncthing_allowed = true;
                    if (rule.port_or_service == "445" || rule.port_or_service == "samba") info.samba_allowed = true;
                }

                info.rules.push_back(rule);
            }
        }
    }

    // Determine active network protection mode
    const char* home_env = getenv("HOME");
    std::string cfg_dir = home_env ? (std::string(home_env) + "/.config/miqusecure") : "/tmp";
    std::string mode_file = cfg_dir + "/network_mode.txt";
    std::string saved_mode;
    std::ifstream mode_in(mode_file);
    if (mode_in.is_open()) {
        std::getline(mode_in, saved_mode);
        saved_mode = trim(saved_mode);
    }

    if (saved_mode == "lockdown") {
        info.current_mode = NetworkMode::Lockdown;
        info.lockdown_active = true;
        info.active_preset = "Extreme Lockdown (Isolated)";
    } else if (saved_mode == "hotspot" || info.metered) {
        info.current_mode = NetworkMode::MobileHotspot;
        info.active_preset = "Mobile Hotspot (Data-Saver)";
    } else if (saved_mode == "public" || info.strict_mode) {
        info.current_mode = NetworkMode::PublicWifi;
        info.active_preset = "Public Wi-Fi (Stealth)";
    } else if (info.default_incoming == "DROP" || info.default_incoming == "DENY" || saved_mode == "home") {
        info.current_mode = NetworkMode::Home;
        info.active_preset = "Home Wi-Fi (Trusted)";
    } else {
        info.current_mode = NetworkMode::Home;
        info.active_preset = "Home Wi-Fi (Trusted)";
    }

    // 4. Query recent blocked logs from journalctl
    std::string logs = run_cmd_capture("journalctl -k -g \"UFW BLOCK\" -n 4 --no-pager 2>/dev/null");
    if (!logs.empty()) {
        std::istringstream iss(logs);
        std::string entry;
        while (std::getline(iss, entry)) {
            if (!entry.empty()) info.recent_blocks.push_back(entry);
        }
    }

    return info;
}

DnsInfo SecurityBackend::read_dns() {
    DnsInfo info;
    info.active_connection_name = get_active_connection();

    std::string which_dnscrypt = run_cmd_capture("which dnscrypt-proxy 2>/dev/null");
    info.dnscrypt_installed = !which_dnscrypt.empty();

    std::string dnscrypt_active = run_cmd_capture("systemctl is-active dnscrypt-proxy 2>/dev/null");
    info.dnscrypt_active = (dnscrypt_active == "active");

    std::ifstream resolv("/etc/resolv.conf");
    if (resolv.is_open()) {
        std::string line;
        while (std::getline(resolv, line)) {
            line = trim(line);
            if (line.rfind("nameserver ", 0) == 0) {
                std::string ns = trim(line.substr(11));
                if (!ns.empty()) info.nameservers.push_back(ns);
            }
        }
    }

    bool has_quad9 = false;
    bool has_cloudflare = false;
    bool has_mullvad = false;
    bool has_adguard = false;

    for (const auto& ns : info.nameservers) {
        if (ns == "9.9.9.9" || ns == "149.112.112.112") has_quad9 = true;
        if (ns == "1.1.1.1" || ns == "1.0.0.1") has_cloudflare = true;
        if (ns == "194.242.2.4" || ns == "194.242.2.5") has_mullvad = true;
        if (ns == "94.140.14.14" || ns == "94.140.15.15") has_adguard = true;
    }

    if (info.dnscrypt_active) {
        info.encrypted = true;
        info.mode = "DNSCrypt-Proxy (DoH / DNSSEC)";
        info.active_provider = "DNSCrypt Local Proxy";
    } else if (has_quad9) {
        info.encrypted = true;
        info.mode = "Quad9 Threat-Filtered DNS";
        info.active_provider = "Quad9";
    } else if (has_cloudflare) {
        info.encrypted = true;
        info.mode = "Cloudflare High-Speed DNS";
        info.active_provider = "Cloudflare 1.1.1.1";
    } else if (has_mullvad) {
        info.encrypted = true;
        info.mode = "Mullvad Privacy DNS";
        info.active_provider = "Mullvad";
    } else if (has_adguard) {
        info.encrypted = true;
        info.mode = "AdGuard Ad-Blocking DNS";
        info.active_provider = "AdGuard";
    } else {
        bool has_local = false;
        for (const auto& ns : info.nameservers) {
            if (ns.rfind("127.", 0) == 0) {
                has_local = true;
                break;
            }
        }
        if (has_local) {
            info.mode = "Local Resolver (127.0.0.1)";
            info.active_provider = "Local Daemon";
        } else {
            info.mode = "Standard Router DNS (Unencrypted)";
            info.active_provider = "ISP / Router Default";
        }
    }

    return info;
}

LsmInfo SecurityBackend::read_lsm() {
    LsmInfo info;

    std::ifstream lsm_file("/sys/kernel/security/lsm");
    if (lsm_file.is_open()) {
        std::string line;
        if (std::getline(lsm_file, line)) {
            std::stringstream ss(line);
            std::string item;
            while (std::getline(ss, item, ',')) {
                item = trim(item);
                if (!item.empty()) {
                    info.active_lsms.push_back(item);
                    if (item == "landlock") info.landlock_active = true;
                    if (item == "yama") info.yama_active = true;
                    if (item == "apparmor") info.apparmor_active = true;
                }
            }
        }
    }

    if (info.apparmor_active) {
        std::ifstream profiles_file("/sys/kernel/security/apparmor/profiles");
        if (profiles_file.is_open()) {
            std::string line;
            while (std::getline(profiles_file, line)) {
                if (line.find("(enforce)") != std::string::npos) {
                    info.enforcing_profiles++;
                } else if (line.find("(complain)") != std::string::npos) {
                    info.complain_profiles++;
                }
            }
        }
    }

    std::string which_flatpak = run_cmd_capture("which flatpak 2>/dev/null");
    info.flatpak_installed = !which_flatpak.empty();
    if (info.flatpak_installed) {
        std::string apps_out = run_cmd_capture("flatpak list --app --columns=application,name 2>/dev/null");
        if (!apps_out.empty()) {
            std::istringstream iss(apps_out);
            std::string line;
            while (std::getline(iss, line)) {
                size_t tab = line.find('\t');
                if (tab != std::string::npos) {
                    FlatpakAppInfo app;
                    app.id = trim(line.substr(0, tab));
                    app.name = trim(line.substr(tab + 1));
                    info.flatpak_apps.push_back(app);
                }
            }
        }
    }

    return info;
}

static std::string format_file_size(uintmax_t bytes) {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << (bytes / 1024.0) << " KB";
        return oss.str();
    }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << (bytes / (1024.0 * 1024.0)) << " MB";
    return oss.str();
}

std::vector<CleanableFile> SecurityBackend::scan_recent_cleanable_files() {
    std::vector<CleanableFile> files;
    const char* home = std::getenv("HOME");
    if (!home) return files;

    std::vector<std::string> search_dirs = {
        std::string(home) + "/Downloads",
        std::string(home) + "/Pictures"
    };

    struct ScannedEntry {
        CleanableFile file;
        fs::file_time_type mtime;
    };
    std::vector<ScannedEntry> scanned;

    for (const auto& dir_str : search_dirs) {
        std::error_code ec;
        if (!fs::exists(dir_str, ec) || !fs::is_directory(dir_str, ec)) continue;

        for (const auto& entry : fs::directory_iterator(dir_str, ec)) {
            if (ec || !entry.is_regular_file()) continue;

            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            std::string type = "";
            if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".webp" || ext == ".gif") {
                type = "Image";
            } else if (ext == ".pdf") {
                type = "PDF Document";
            } else if (ext == ".docx" || ext == ".odt" || ext == ".pptx" || ext == ".xlsx") {
                type = "Office Document";
            } else if (ext == ".mp3" || ext == ".flac" || ext == ".mp4") {
                type = "Media";
            }

            if (!type.empty()) {
                ScannedEntry se;
                se.file.filename = entry.path().filename().string();
                se.file.full_path = entry.path().string();
                se.file.size_str = format_file_size(entry.file_size(ec));
                se.file.type = type;
                se.mtime = entry.last_write_time(ec);
                scanned.push_back(se);
            }
        }
    }

    std::sort(scanned.begin(), scanned.end(), [](const ScannedEntry& a, const ScannedEntry& b) {
        return a.mtime > b.mtime;
    });

    for (size_t i = 0; i < scanned.size() && i < 4; ++i) {
        files.push_back(scanned[i].file);
    }

    return files;
}

static uintmax_t calculate_dir_size(const fs::path& dir_path) {
    if (!fs::exists(dir_path)) return 0;
    uintmax_t size = 0;
    std::error_code ec;
    for (const auto& entry : fs::recursive_directory_iterator(dir_path, fs::directory_options::skip_permission_denied, ec)) {
        if (ec) break;
        if (entry.is_regular_file(ec)) {
            size += entry.file_size(ec);
        }
    }
    return size;
}

static std::string format_byte_size(uintmax_t bytes) {
    if (bytes == 0) return "Empty";
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0f);
        return buf;
    }
    if (bytes < 1024 * 1024 * 1024) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0f * 1024.0f));
        return buf;
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f GB", bytes / (1024.0f * 1024.0f * 1024.0f));
    return buf;
}

static uintmax_t clean_directory_contents(const fs::path& dir_path) {
    if (!fs::exists(dir_path)) return 0;
    uintmax_t freed = 0;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(dir_path, fs::directory_options::skip_permission_denied, ec)) {
        if (ec) break;
        uintmax_t s = 0;
        if (entry.is_regular_file(ec)) s = entry.file_size(ec);
        else if (entry.is_directory(ec)) s = calculate_dir_size(entry.path());
        if (fs::remove_all(entry.path(), ec) > 0) {
            freed += s;
        }
    }
    return freed;
}

CleanerInfo SecurityBackend::read_cleaner() {
    CleanerInfo info;
    std::string which_mat2 = run_cmd_capture("which mat2 2>/dev/null");
    info.mat2_installed = !which_mat2.empty();
    if (info.mat2_installed) {
        info.version = trim(run_cmd_capture("mat2 --version 2>/dev/null"));
    }
    info.recent_files = scan_recent_cleanable_files();

    const char* home = getenv("HOME");
    if (home) {
        fs::path h(home);
        uintmax_t thumb_sz = calculate_dir_size(h / ".cache/thumbnails");
        info.thumbnails_size = format_byte_size(thumb_sz);

        uintmax_t browser_sz = 0;
        browser_sz += calculate_dir_size(h / ".cache/google-chrome");
        browser_sz += calculate_dir_size(h / ".cache/chromium");
        browser_sz += calculate_dir_size(h / ".cache/BraveSoftware");
        browser_sz += calculate_dir_size(h / ".cache/mozilla");
        info.browser_cache_size = format_byte_size(browser_sz);

        uintmax_t trash_sz = 0;
        trash_sz += calculate_dir_size(h / ".local/share/Trash");
        trash_sz += calculate_dir_size(h / ".cache/fontconfig");
        trash_sz += calculate_dir_size(h / ".cache/mesa_shader_cache");
        info.trash_size = format_byte_size(trash_sz);

        uintmax_t hist_sz = 0;
        std::error_code ec;
        if (fs::exists(h / ".bash_history", ec)) hist_sz += fs::file_size(h / ".bash_history", ec);
        if (fs::exists(h / ".zsh_history", ec)) hist_sz += fs::file_size(h / ".zsh_history", ec);
        if (fs::exists(h / ".python_history", ec)) hist_sz += fs::file_size(h / ".python_history", ec);
        info.bash_history_size = format_byte_size(hist_sz);
    }

    return info;
}

bool SecurityBackend::clean_thumbnails(std::string& output_log) {
    const char* home = getenv("HOME");
    if (!home) {
        output_log = "Error: HOME directory not found.";
        return false;
    }
    fs::path thumb_dir = fs::path(home) / ".cache/thumbnails";
    uintmax_t freed = clean_directory_contents(thumb_dir);
    output_log = "Cleaned thumbnail cache (" + format_byte_size(freed) + " reclaimed).";
    return true;
}

bool SecurityBackend::clean_browser_caches(std::string& output_log) {
    const char* home = getenv("HOME");
    if (!home) {
        output_log = "Error: HOME directory not found.";
        return false;
    }
    fs::path h(home);
    uintmax_t freed = 0;
    freed += clean_directory_contents(h / ".cache/google-chrome/Default/Cache");
    freed += clean_directory_contents(h / ".cache/google-chrome/Default/Code Cache");
    freed += clean_directory_contents(h / ".cache/chromium/Default/Cache");
    freed += clean_directory_contents(h / ".cache/chromium/Default/Code Cache");
    freed += clean_directory_contents(h / ".cache/BraveSoftware/Brave-Browser/Default/Cache");
    freed += clean_directory_contents(h / ".cache/BraveSoftware/Brave-Browser/Default/Code Cache");

    // Mozilla Firefox cache2
    std::error_code ec;
    fs::path ff_dir = h / ".cache/mozilla/firefox";
    if (fs::exists(ff_dir, ec)) {
        for (const auto& entry : fs::directory_iterator(ff_dir, ec)) {
            if (entry.is_directory(ec)) {
                freed += clean_directory_contents(entry.path() / "cache2");
            }
        }
    }
    output_log = "Cleaned browser web caches (" + format_byte_size(freed) + " reclaimed).";
    return true;
}

bool SecurityBackend::clean_trash_and_temp(std::string& output_log) {
    const char* home = getenv("HOME");
    if (!home) {
        output_log = "Error: HOME directory not found.";
        return false;
    }
    fs::path h(home);
    uintmax_t freed = 0;
    freed += clean_directory_contents(h / ".local/share/Trash/files");
    freed += clean_directory_contents(h / ".local/share/Trash/info");
    freed += clean_directory_contents(h / ".cache/fontconfig");
    freed += clean_directory_contents(h / ".cache/mesa_shader_cache");
    output_log = "Cleaned Trash bin & temporary caches (" + format_byte_size(freed) + " reclaimed).";
    return true;
}

bool SecurityBackend::clean_shell_history(std::string& output_log) {
    const char* home = getenv("HOME");
    if (!home) {
        output_log = "Error: HOME directory not found.";
        return false;
    }
    fs::path h(home);
    std::vector<std::string> hist_files = {".bash_history", ".zsh_history", ".python_history", ".lesshst"};
    for (const auto& f : hist_files) {
        fs::path p = h / f;
        std::error_code ec;
        if (fs::exists(p, ec)) {
            std::ofstream ofs(p, std::ios::trunc);
        }
    }
    output_log = "Cleared shell and terminal history files.";
    return true;
}

bool SecurityBackend::clean_all_caches(std::string& output_log) {
    std::string l1, l2, l3, l4;
    clean_thumbnails(l1);
    clean_browser_caches(l2);
    clean_trash_and_temp(l3);
    clean_shell_history(l4);
    output_log = "Successfully performed deep clean across all system and privacy caches!";
    return true;
}

HardwareInfo SecurityBackend::read_hardware() {
    HardwareInfo info;

    // 1. Wi-Fi status via nmcli
    std::string wifi_out = run_cmd_capture("nmcli -t -f WIFI radio 2>/dev/null");
    wifi_out = trim(wifi_out);
    info.wifi_enabled = (wifi_out == "enabled");

    // 2. Bluetooth status via bluetoothctl or rfkill
    std::string bt_out = run_cmd_capture("bluetoothctl show 2>/dev/null | grep \"Powered:\" | cut -d: -f2");
    bt_out = trim(bt_out);
    if (!bt_out.empty()) {
        info.bluetooth_enabled = (bt_out == "yes");
    } else {
        std::string rf_bt = run_cmd_capture("rfkill list bluetooth 2>/dev/null | grep \"Soft blocked:\" | head -n 1 | cut -d: -f2");
        rf_bt = trim(rf_bt);
        info.bluetooth_enabled = (rf_bt == "no");
    }

    // 3. Airplane mode: both wifi and bluetooth are disabled
    info.airplane_mode = (!info.wifi_enabled && !info.bluetooth_enabled);

    // 4. Microphone Mute state via wpctl or amixer
    std::string mic_vol = run_cmd_capture("wpctl get-volume @DEFAULT_AUDIO_SOURCE@ 2>/dev/null");
    if (!mic_vol.empty()) {
        info.mic_muted = (mic_vol.find("[MUTED]") != std::string::npos);
    } else {
        std::string amix = run_cmd_capture("amixer get Capture 2>/dev/null | grep -E '\\[off\\]'");
        info.mic_muted = !amix.empty();
    }

    // 5. Camera sensor detection & active usage via /dev/video*
    std::string vid_dev = run_cmd_capture("ls /dev/video* 2>/dev/null | head -n 1");
    vid_dev = trim(vid_dev);
    info.camera_detected = !vid_dev.empty();
    if (info.camera_detected) {
        info.camera_device_name = vid_dev;
        std::string fuser_out = run_cmd_capture("fuser /dev/video* 2>/dev/null");
        fuser_out = trim(fuser_out);
        info.camera_in_use = !fuser_out.empty();
        if (info.camera_in_use) {
            info.camera_active_proc = "Process PID: " + fuser_out;
        }
    }

    return info;
}

bool SecurityBackend::set_wifi_enabled(bool enabled) {
    std::string cmd = "nmcli radio wifi " + std::string(enabled ? "on" : "off") + " 2>/dev/null";
    int ret = std::system(cmd.c_str());
    return (ret == 0);
}

bool SecurityBackend::set_bluetooth_enabled(bool enabled) {
    std::string cmd = "bluetoothctl power " + std::string(enabled ? "on" : "off") + " 2>/dev/null";
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        std::string rf_cmd = "rfkill " + std::string(enabled ? "unblock" : "block") + " bluetooth 2>/dev/null";
        ret = std::system(rf_cmd.c_str());
    }
    return (ret == 0);
}

bool SecurityBackend::set_airplane_mode(bool enabled) {
    if (enabled) {
        std::system("nmcli radio all off 2>/dev/null");
        std::system("rfkill block all 2>/dev/null");
    } else {
        std::system("rfkill unblock all 2>/dev/null");
        std::system("nmcli radio all on 2>/dev/null");
    }
    return true;
}

bool SecurityBackend::set_mic_muted(bool muted) {
    std::string cmd = "wpctl set-mute @DEFAULT_AUDIO_SOURCE@ " + std::string(muted ? "1" : "0") + " 2>/dev/null";
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        std::string amix = "amixer set Capture " + std::string(muted ? "nocap" : "cap") + " 2>/dev/null";
        ret = std::system(amix.c_str());
    }
    return (ret == 0);
}

bool SecurityBackend::set_firewall_enabled(bool enabled) {
    std::string cmd = "pkexec ufw --force ";
    cmd += (enabled ? "enable" : "disable");
    int ret = std::system(cmd.c_str());
    return (ret == 0);
}

bool SecurityBackend::set_strict_mode(bool strict) {
    std::string cmd = "pkexec ufw default " + std::string(strict ? "reject" : "deny") + " incoming";
    int ret = std::system(cmd.c_str());
    return (ret == 0);
}

bool SecurityBackend::set_network_mode(NetworkMode mode) {
    std::string conn = get_active_connection();
    bool ok = true;

    const char* home_env = getenv("HOME");
    std::string cfg_dir = home_env ? (std::string(home_env) + "/.config/miqusecure") : "/tmp";
    std::error_code ec;
    fs::create_directories(cfg_dir, ec);
    std::string mode_file = cfg_dir + "/network_mode.txt";
    std::string mode_str = "home";

    switch (mode) {
        case NetworkMode::Home: {
            mode_str = "home";
            ok = set_strict_mode(false); // sets ufw default deny incoming
            if (!conn.empty()) {
                std::string cmd = "nmcli connection modify \"" + conn + "\" connection.metered no 2>/dev/null";
                std::system(cmd.c_str());
            }
            break;
        }
        case NetworkMode::MobileHotspot: {
            mode_str = "hotspot";
            ok = set_strict_mode(true); // sets ufw default reject incoming
            if (!conn.empty()) {
                std::string cmd = "nmcli connection modify \"" + conn + "\" connection.metered yes 2>/dev/null";
                std::system(cmd.c_str());
            }
            break;
        }
        case NetworkMode::PublicWifi: {
            mode_str = "public";
            ok = set_strict_mode(true); // sets ufw default reject incoming
            if (!conn.empty()) {
                std::string cmd = "nmcli connection modify \"" + conn + "\" connection.metered no 2>/dev/null";
                std::system(cmd.c_str());
            }
            break;
        }
        case NetworkMode::Lockdown: {
            mode_str = "lockdown";
            ok = set_strict_mode(true); // sets ufw default reject incoming
            // Lockdown shuts down any open inbound service ports
            toggle_service("22", false);
            toggle_service("8080", false);
            toggle_service("22000", false);
            toggle_service("445", false);
            if (!conn.empty()) {
                std::string cmd = "nmcli connection modify \"" + conn + "\" connection.metered yes 2>/dev/null";
                std::system(cmd.c_str());
            }
            break;
        }
        default:
            mode_str = "home";
            break;
    }

    std::ofstream out(mode_file);
    if (out.is_open()) {
        out << mode_str << "\n";
    }

    return ok;
}

bool SecurityBackend::toggle_service(const std::string& port, bool enable) {
    std::string cmd = enable ?
        ("pkexec ufw allow " + port + "/tcp") :
        ("pkexec ufw delete allow " + port + "/tcp");
    int ret = std::system(cmd.c_str());
    return (ret == 0);
}

bool SecurityBackend::apply_firewall_preset(const std::string& preset) {
    std::string cmd;
    if (preset == "standard") {
        cmd = "pkexec ufw default deny incoming && pkexec ufw default allow outgoing && pkexec ufw --force enable";
    } else if (preset == "strict") {
        cmd = "pkexec ufw default reject incoming && pkexec ufw default allow outgoing && pkexec ufw --force enable";
    } else {
        cmd = "pkexec ufw --force enable";
    }
    int ret = std::system(cmd.c_str());
    return (ret == 0);
}

bool SecurityBackend::add_firewall_rule(const std::string& port, const std::string& proto, const std::string& action) {
    std::string act = (action == "ALLOW") ? "allow" : (action == "DENY" ? "deny" : "reject");
    std::string cmd = "pkexec ufw " + act + " " + port;
    if (!proto.empty() && proto != "any") {
        cmd += "/" + proto;
    }
    int ret = std::system(cmd.c_str());
    return (ret == 0);
}

bool SecurityBackend::delete_firewall_rule(const FirewallRule& rule) {
    std::string act = (rule.action == "DENY") ? "deny" : ((rule.action == "REJECT") ? "reject" : "allow");
    std::string cmd;
    if (!rule.port_or_service.empty() && rule.port_or_service != "All Traffic") {
        if (!rule.source.empty() && rule.source != "Anywhere" && rule.source != "0.0.0.0/0" && rule.source != "::/0") {
            cmd = "pkexec ufw delete " + act + " from " + rule.source + " to any port " + rule.port_or_service;
        } else {
            cmd = "pkexec ufw delete " + act + " " + rule.port_or_service;
        }
        if (!rule.protocol.empty() && rule.protocol != "any") {
            cmd += "/" + rule.protocol;
        }
    } else if (rule.id > 0) {
        cmd = "pkexec ufw --force delete " + std::to_string(rule.id);
    } else {
        return false;
    }
    int ret = std::system(cmd.c_str());
    return (ret == 0);
}

bool SecurityBackend::delete_firewall_rule(int rule_index) {
    if (rule_index <= 0) return false;
    std::string cmd = "pkexec ufw --force delete " + std::to_string(rule_index);
    int ret = std::system(cmd.c_str());
    return (ret == 0);
}

bool SecurityBackend::apply_dns(const std::string& primary_ip, const std::string& secondary_ip) {
    std::string conn = get_active_connection();
    if (conn.empty()) return false;

    std::string cmd = "nmcli connection modify \"" + conn + "\" ipv4.dns \"" + primary_ip + " " + secondary_ip + "\" ipv4.ignore-auto-dns yes && nmcli connection up \"" + conn + "\" 2>/dev/null";
    int ret = std::system(cmd.c_str());
    return (ret == 0);
}

bool SecurityBackend::restore_default_dns() {
    std::string conn = get_active_connection();
    if (conn.empty()) return false;

    std::string cmd = "nmcli connection modify \"" + conn + "\" ipv4.dns \"\" ipv4.ignore-auto-dns no && nmcli connection up \"" + conn + "\" 2>/dev/null";
    int ret = std::system(cmd.c_str());
    return (ret == 0);
}

bool SecurityBackend::clean_metadata(const std::string& filepath, std::string& output_log) {
    if (!fs::exists(filepath)) {
        output_log = "File not found: " + filepath;
        return false;
    }

    fs::path p(filepath);
    std::string ext = p.extension().string();
    std::string ext_lower = ext;
    std::transform(ext_lower.begin(), ext_lower.end(), ext_lower.begin(), ::tolower);

    // 1. Detect true file MIME type using file --mime-type
    std::string detected_mime = trim(run_cmd_capture("file -b --mime-type \"" + filepath + "\" 2>/dev/null"));

    // Determine the expected canonical extension for this MIME type
    std::string canonical_ext;
    if (detected_mime == "image/jpeg") {
        if (ext_lower != ".jpg" && ext_lower != ".jpeg") canonical_ext = ".jpg";
    } else if (detected_mime == "image/png") {
        if (ext_lower != ".png") canonical_ext = ".png";
    } else if (detected_mime == "application/pdf") {
        if (ext_lower != ".pdf") canonical_ext = ".pdf";
    } else if (detected_mime == "image/webp") {
        if (ext_lower != ".webp") canonical_ext = ".webp";
    } else if (detected_mime == "image/gif") {
        if (ext_lower != ".gif") canonical_ext = ".gif";
    } else if (detected_mime == "image/tiff") {
        if (ext_lower != ".tiff" && ext_lower != ".tif") canonical_ext = ".tiff";
    } else if (detected_mime == "image/svg+xml") {
        if (ext_lower != ".svg") canonical_ext = ".svg";
    } else if (detected_mime == "audio/mpeg") {
        if (ext_lower != ".mp3") canonical_ext = ".mp3";
    } else if (detected_mime == "audio/flac") {
        if (ext_lower != ".flac") canonical_ext = ".flac";
    } else if (detected_mime == "video/mp4") {
        if (ext_lower != ".mp4") canonical_ext = ".mp4";
    }

    // If extension is mismatched, mat2 fails because python mimetypes.guess_type
    // maps strictly by extension. Handle by pointing mat2 through a temporary symlink in same dir.
    if (!canonical_ext.empty()) {
        std::string stem = p.stem().string();
        std::string parent = p.parent_path().string();
        std::string tmp_link = parent.empty() ?
            (".tmp_mat2_" + stem + canonical_ext) :
            (parent + "/.tmp_mat2_" + stem + canonical_ext);

        std::error_code ec;
        fs::remove(tmp_link, ec);
        fs::create_symlink(filepath, tmp_link, ec);

        std::string cmd = "mat2 \"" + tmp_link + "\" 2>&1";
        std::string raw_log = run_cmd_capture(cmd);
        fs::remove(tmp_link, ec);

        std::string tmp_cleaned = parent.empty() ?
            (".tmp_mat2_" + stem + ".cleaned" + canonical_ext) :
            (parent + "/.tmp_mat2_" + stem + ".cleaned" + canonical_ext);

        if (fs::exists(tmp_cleaned)) {
            std::string dest_cleaned = parent.empty() ?
                (stem + ".cleaned" + canonical_ext) :
                (parent + "/" + stem + ".cleaned" + canonical_ext);

            fs::rename(tmp_cleaned, dest_cleaned, ec);
            if (ec) {
                fs::copy_file(tmp_cleaned, dest_cleaned, fs::copy_options::overwrite_existing, ec);
                fs::remove(tmp_cleaned, ec);
            }

            // Also create a copy with the original extension so software expecting it finds it
            std::string dest_orig_ext = parent.empty() ?
                (stem + ".cleaned" + ext) :
                (parent + "/" + stem + ".cleaned" + ext);
            fs::copy_file(dest_cleaned, dest_orig_ext, fs::copy_options::overwrite_existing, ec);

            output_log = "Detected actual format is " + detected_mime + " (corrected " + ext + " -> " + canonical_ext + "). Cleaned copy saved.";
            return true;
        } else {
            output_log = raw_log.empty() ? "mat2 failed to clean file." : raw_log;
            return false;
        }
    }

    // Direct invocation
    std::string cmd = "mat2 \"" + filepath + "\" 2>&1";
    output_log = run_cmd_capture(cmd);

    std::string parent = p.parent_path().string();
    std::string stem = p.stem().string();
    std::string expected_cleaned = parent.empty() ?
        (stem + ".cleaned" + ext) :
        (parent + "/" + stem + ".cleaned" + ext);

    if (fs::exists(expected_cleaned)) {
        output_log = "Successfully scrubbed metadata! Created '.cleaned" + ext + "' copy.";
        return true;
    }

    if (output_log.find("was cleaned to") != std::string::npos || output_log.empty()) {
        return true;
    }
    return false;
}

static bool parse_endpoint(const std::string& ep, std::string& out_ip, int& out_port) {
    size_t last_colon = ep.rfind(':');
    if (last_colon == std::string::npos) return false;
    try {
        out_port = std::stoi(ep.substr(last_colon + 1));
    } catch (...) {
        out_port = 0;
    }
    out_ip = ep.substr(0, last_colon);
    if (!out_ip.empty() && out_ip.front() == '[' && out_ip.back() == ']') {
        out_ip = out_ip.substr(1, out_ip.size() - 2);
    }
    size_t pct = out_ip.find('%');
    if (pct != std::string::npos) {
        out_ip = out_ip.substr(0, pct);
    }
    return true;
}

static std::string resolve_host_cached(const std::string& ip) {
    if (ip.empty() || ip == "*" || ip == "0.0.0.0") return "*";
    if (ip == "127.0.0.1" || ip == "::1" || ip.rfind("127.", 0) == 0) return "localhost";
    if (ip == "9.9.9.9" || ip == "149.112.112.112") return "dns.quad9.net (Quad9)";
    if (ip == "1.1.1.1" || ip == "1.0.0.1") return "one.one.one.one (Cloudflare)";
    if (ip == "8.8.8.8" || ip == "8.8.4.4") return "dns.google (Google)";
    if (ip == "94.140.14.14" || ip == "94.140.15.15") return "dns.adguard.com (AdGuard)";
    if (ip == "194.242.2.4" || ip == "194.242.2.5") return "dns.mullvad.net (Mullvad)";
    if (ip.rfind("192.168.", 0) == 0 || ip.rfind("10.", 0) == 0 || ip.rfind("172.16.", 0) == 0) {
        return "Local Network (LAN)";
    }
    return ip;
}

static std::mutex s_traffic_mutex;
static std::deque<NetworkConnection> s_recent_requests;
static std::unordered_set<std::string> s_prev_conn_ids;
static uint64_t s_prev_rx_bytes = 0;
static uint64_t s_prev_tx_bytes = 0;
static std::chrono::steady_clock::time_point s_prev_rate_time = std::chrono::steady_clock::now();

TrafficReport SecurityBackend::read_traffic() {
    std::lock_guard<std::mutex> lock(s_traffic_mutex);
    TrafficReport report;

    // 1. Calculate bandwidth rate from /proc/net/dev
    std::ifstream net_dev("/proc/net/dev");
    if (net_dev.is_open()) {
        std::string line;
        uint64_t total_rx = 0;
        uint64_t total_tx = 0;
        while (std::getline(net_dev, line)) {
            size_t col = line.find(':');
            if (col == std::string::npos) continue;
            std::string iface = trim(line.substr(0, col));
            if (iface == "lo") continue;

            std::istringstream iss(line.substr(col + 1));
            uint64_t rx = 0, tx = 0, dummy = 0;
            if (iss >> rx) {
                for (int i = 0; i < 7; ++i) iss >> dummy;
                if (iss >> tx) {
                    total_rx += rx;
                    total_tx += tx;
                }
            }
        }
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - s_prev_rate_time).count();
        if (elapsed > 0.4 && s_prev_rx_bytes > 0) {
            report.rx_rate_kb = static_cast<float>((total_rx >= s_prev_rx_bytes ? (total_rx - s_prev_rx_bytes) : 0) / elapsed / 1024.0);
            report.tx_rate_kb = static_cast<float>((total_tx >= s_prev_tx_bytes ? (total_tx - s_prev_tx_bytes) : 0) / elapsed / 1024.0);
        }
        s_prev_rx_bytes = total_rx;
        s_prev_tx_bytes = total_tx;
        s_prev_rate_time = now;
    }

    // 2. Parse ss -tupn -H
    std::string ss_out = run_cmd_capture("ss -tupn -H 2>/dev/null");
    std::istringstream ss(ss_out);
    std::string line;

    std::unordered_set<std::string> current_ids;
    std::unordered_set<std::string> active_procs;

    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream time_oss;
    time_oss << std::put_time(&tm, "%H:%M:%S");
    std::string cur_time = time_oss.str();

    while (std::getline(ss, line)) {
        line = trim(line);
        if (line.empty()) continue;

        std::istringstream liness(line);
        std::string proto, state, recv_q, send_q, local_ep, peer_ep;
        if (!(liness >> proto >> state >> recv_q >> send_q >> local_ep >> peer_ep)) {
            continue;
        }

        std::string rest;
        std::getline(liness, rest);

        NetworkConnection conn;
        conn.protocol = (proto == "tcp") ? "TCP" : "UDP";
        conn.state = state;
        parse_endpoint(local_ep, conn.local_addr, conn.local_port);
        parse_endpoint(peer_ep, conn.remote_addr, conn.remote_port);

        size_t p_open = rest.find("((\"");
        if (p_open != std::string::npos) {
            size_t p_close = rest.find('"', p_open + 3);
            if (p_close != std::string::npos) {
                conn.process_name = rest.substr(p_open + 3, p_close - (p_open + 3));
            }
        }
        size_t pid_pos = rest.find("pid=");
        if (pid_pos != std::string::npos) {
            try {
                conn.pid = std::stoi(rest.substr(pid_pos + 4));
            } catch (...) {
                conn.pid = 0;
            }
        }
        if (conn.process_name.empty()) {
            conn.process_name = (conn.pid > 0) ? ("PID " + std::to_string(conn.pid)) : "System / Kernel";
        }

        conn.is_loopback = (conn.remote_addr == "127.0.0.1" || conn.remote_addr == "::1" || conn.remote_addr.rfind("127.", 0) == 0 || conn.remote_addr == "*");
        conn.is_outbound = !conn.is_loopback && !conn.remote_addr.empty() && conn.remote_addr != "0.0.0.0";

        int port = (conn.remote_port > 0) ? conn.remote_port : conn.local_port;
        if (port == 443) {
            conn.service_name = "HTTPS";
            conn.is_encrypted = true;
        } else if (port == 80) {
            conn.service_name = "HTTP";
            conn.is_encrypted = false;
        } else if (port == 53) {
            conn.service_name = "DNS";
            conn.is_dns = true;
            conn.is_encrypted = false;
        } else if (port == 853) {
            conn.service_name = "DoT";
            conn.is_dns = true;
            conn.is_encrypted = true;
        } else if (port == 22) {
            conn.service_name = "SSH";
            conn.is_encrypted = true;
        } else if (port == 6600) {
            conn.service_name = "MPD";
        } else if (port == 22000) {
            conn.service_name = "Syncthing";
        } else if (port == 67 || port == 68) {
            conn.service_name = "DHCP";
        } else {
            conn.service_name = "Port " + std::to_string(port);
        }

        conn.remote_host = resolve_host_cached(conn.remote_addr);
        conn.id = conn.protocol + ":" + conn.local_addr + ":" + std::to_string(conn.local_port) + "->" + conn.remote_addr + ":" + std::to_string(conn.remote_port);
        conn.timestamp = cur_time;

        current_ids.insert(conn.id);
        if (conn.is_outbound) {
            report.total_outbound++;
            active_procs.insert(conn.process_name);
            if (conn.is_encrypted) report.total_encrypted++;
            else report.total_plaintext++;
        } else {
            report.total_inbound++;
        }

        if (!s_prev_conn_ids.empty() && s_prev_conn_ids.find(conn.id) == s_prev_conn_ids.end()) {
            s_recent_requests.push_front(conn);
            if (s_recent_requests.size() > 25) {
                s_recent_requests.pop_back();
            }
        }

        report.active_connections.push_back(conn);
    }

    if (s_recent_requests.empty()) {
        for (const auto& c : report.active_connections) {
            if (c.is_outbound) {
                s_recent_requests.push_back(c);
                if (s_recent_requests.size() >= 15) break;
            }
        }
    }

    s_prev_conn_ids = current_ids;
    report.recent_events.assign(s_recent_requests.begin(), s_recent_requests.end());
    report.total_apps = static_cast<int>(active_procs.size());

    return report;
}

bool SecurityBackend::terminate_process(int pid, bool force) {
    if (pid <= 1) return false;
    return (kill(pid, force ? SIGKILL : SIGTERM) == 0);
}

DistroboxInfo SecurityBackend::read_testbed() {
    DistroboxInfo info;
    std::string dbox = run_cmd_capture("which distrobox 2>/dev/null");
    info.distrobox_installed = !dbox.empty();

    std::string pod = run_cmd_capture("which podman 2>/dev/null");
    if (pod.empty()) pod = run_cmd_capture("which docker 2>/dev/null");
    info.podman_installed = !pod.empty();

    if (info.distrobox_installed && info.podman_installed) {
        std::string list_out = run_cmd_capture("distrobox list --no-color 2>/dev/null");
        std::istringstream iss(list_out);
        std::string line;
        bool header_skipped = false;
        while (std::getline(iss, line)) {
            line = trim(line);
            if (line.empty()) continue;
            if (!header_skipped && (line.find("ID") != std::string::npos || line.find("NAME") != std::string::npos)) {
                header_skipped = true;
                continue;
            }
            std::vector<std::string> parts;
            std::stringstream ss(line);
            std::string part;
            while (std::getline(ss, part, '|')) {
                parts.push_back(trim(part));
            }
            if (parts.size() >= 4) {
                TestboxItem item;
                item.name = parts[1];
                item.status = parts[2];
                item.image = parts[3];
                item.installed_packages = list_testbed_packages(item.name);
                info.boxes.push_back(item);
            }
        }
    }
    return info;
}

bool SecurityBackend::create_testbed(const std::string& name, const std::string& image) {
    if (name.empty()) return false;
    std::string img = image.empty() ? "archlinux:latest" : image;
    std::string cmd = "distrobox create -Y -n \"" + name + "\" --image \"" + img + "\" 2>&1";
    int ret = std::system(cmd.c_str());
    return (ret == 0);
}

bool SecurityBackend::launch_testbed_terminal(const std::string& name) {
    if (name.empty()) return false;
    std::vector<std::string> terms = {"foot", "alacritty", "kitty", "ghostty", "wezterm", "gnome-terminal", "xterm"};

    for (const auto& term : terms) {
        std::string check = run_cmd_capture("which " + term + " 2>/dev/null");
        if (!check.empty()) {
            std::string launch_cmd;
            if (term == "kitty") {
                launch_cmd = "kitty -T \"Testbed: " + name + "\" distrobox enter \"" + name + "\" &";
            } else if (term == "foot") {
                launch_cmd = "foot -T \"Testbed: " + name + "\" distrobox enter \"" + name + "\" &";
            } else if (term == "alacritty") {
                launch_cmd = "alacritty -T \"Testbed: " + name + "\" -e distrobox enter \"" + name + "\" &";
            } else if (term == "ghostty") {
                launch_cmd = "ghostty -e distrobox enter \"" + name + "\" &";
            } else if (term == "wezterm") {
                launch_cmd = "wezterm start -- distrobox enter \"" + name + "\" &";
            } else {
                launch_cmd = term + " -e distrobox enter \"" + name + "\" &";
            }
            std::system(launch_cmd.c_str());
            return true;
        }
    }
    return false;
}

bool SecurityBackend::destroy_testbed(const std::string& name) {
    if (name.empty()) return false;
    std::string cmd = "distrobox rm -f \"" + name + "\" 2>&1";
    int ret = std::system(cmd.c_str());
    return (ret == 0);
}

std::vector<TestbedPackage> SecurityBackend::list_testbed_packages(const std::string& box_name) {
    std::vector<TestbedPackage> list;
    if (box_name.empty()) return list;

    std::string cmd = "distrobox enter \"" + box_name + "\" -- pacman -Qe 2>/dev/null";
    std::string out = run_cmd_capture(cmd);
    std::istringstream iss(out);
    std::string line;

    static const std::unordered_set<std::string> base_pkgs = {
        "base", "bash-completion", "bc", "curl", "diffutils", "findutils",
        "glibc", "glibc-locales", "gnupg", "inetutils", "iputils", "keyutils",
        "less", "lsof", "man-db", "man-pages", "mesa", "mtr", "ncurses",
        "nss-mdns", "openssh", "pigz", "pinentry", "procps-ng", "rsync",
        "shadow", "sudo", "tcpdump", "time", "traceroute", "tree", "tzdata",
        "unzip", "util-linux", "util-linux-libs", "vte-common", "vulkan-intel",
        "vulkan-radeon", "wget", "words", "xorg-xauth", "zip"
    };

    while (std::getline(iss, line)) {
        line = trim(line);
        if (line.empty()) continue;
        size_t sp = line.find(' ');
        if (sp != std::string::npos) {
            std::string name = line.substr(0, sp);
            std::string ver = line.substr(sp + 1);
            if (base_pkgs.find(name) == base_pkgs.end()) {
                list.push_back({name, ver});
            }
        }
    }
    return list;
}

bool SecurityBackend::install_testbed_package(const std::string& box_name, const std::string& package_name, std::string& out_error) {
    if (box_name.empty() || package_name.empty()) return false;
    std::string cmd = "distrobox enter \"" + box_name + "\" -- sudo pacman -S --needed --noconfirm \"" + package_name + "\" 2>&1";
    std::string out = run_cmd_capture(cmd);
    if (out.find("error:") != std::string::npos || out.find("target not found") != std::string::npos) {
        out_error = out;
        return false;
    }
    return true;
}

bool SecurityBackend::remove_testbed_package(const std::string& box_name, const std::string& package_name) {
    if (box_name.empty() || package_name.empty()) return false;
    std::string cmd = "distrobox enter \"" + box_name + "\" -- sudo pacman -Rns --noconfirm \"" + package_name + "\" 2>&1";
    int ret = std::system(cmd.c_str());
    return (ret == 0);
}

bool SecurityBackend::launch_testbed_app(const std::string& box_name, const std::string& app_name) {
    if (box_name.empty() || app_name.empty()) return false;
    std::string cmd = "distrobox enter \"" + box_name + "\" -- " + app_name + " &";
    std::system(cmd.c_str());
    return true;
}

} // namespace miqusecure
