#include "apps_backend.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <unordered_set>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <unistd.h>

namespace miqu {

namespace fs = std::filesystem;

struct PipeCloser {
    void operator()(FILE* fp) const {
        if (fp) pclose(fp);
    }
};

struct DesktopEntry {
    std::string id;
    std::string name;
    std::string generic_name;
    std::string comment;
    std::string exec_cmd;
    std::string icon_name;
    std::string categories;
    std::string desktop_path;
    bool no_display = false;
};

std::string AppsBackend::run_cmd(const std::string& cmd) {
    std::string result;
    std::unique_ptr<FILE, PipeCloser> pipe(popen(cmd.c_str(), "r"));
    if (!pipe) return result;

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe.get())) {
        result += buffer;
    }
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }
    return result;
}

static bool parse_desktop_file(const std::string& path, DesktopEntry& entry) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    bool in_entry = false;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;

        if (line[0] == '[') {
            in_entry = (line == "[Desktop Entry]");
            continue;
        }
        if (!in_entry) continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        if (key == "Name") entry.name = val;
        else if (key == "GenericName") entry.generic_name = val;
        else if (key == "Comment") entry.comment = val;
        else if (key == "Exec") entry.exec_cmd = val;
        else if (key == "Icon") entry.icon_name = val;
        else if (key == "Categories") entry.categories = val;
        else if (key == "NoDisplay" && val == "true") entry.no_display = true;
    }
    entry.desktop_path = path;
    return !entry.name.empty() && !entry.no_display;
}

std::string AppsBackend::resolve_desktop_path(const std::string& desktop_id) {
    const char* home = getenv("HOME");
    std::vector<std::string> search_dirs;
    if (home) search_dirs.push_back(std::string(home) + "/.local/share/applications");
    search_dirs.push_back("/usr/share/applications");
    search_dirs.push_back("/usr/local/share/applications");
    search_dirs.push_back("/var/lib/flatpak/exports/share/applications");

    for (const auto& dir : search_dirs) {
        std::string path = dir + "/" + desktop_id;
        if (fs::exists(path)) return path;
    }
    return "";
}

std::string AppsBackend::escape_regex(const std::string& str) {
    std::string out;
    out.reserve(str.size() * 2);
    for (char c : str) {
        if (c == '.' || c == '^' || c == '$' || c == '*' || c == '+' ||
            c == '?' || c == '(' || c == ')' || c == '[' || c == ']' ||
            c == '{' || c == '}' || c == '|' || c == '\\') {
            out.push_back('\\');
        }
        out.push_back(c);
    }
    return out;
}

std::string AppsBackend::resolve_real_executable(const std::string& path) {
    if (path.empty()) return "";
    std::error_code ec;
    if (!fs::exists(path, ec)) return path;

    // Follow symlinks to destination
    std::string current = path;
    if (fs::is_symlink(current, ec)) {
        auto target = fs::canonical(current, ec);
        if (!ec && !target.empty()) {
            current = target.string();
        }
    }

    // Check if the file is a shell wrapper script with an exec line
    std::ifstream file(current);
    if (file.is_open()) {
        std::string first_line;
        if (std::getline(file, first_line) && first_line.rfind("#!", 0) == 0) {
            std::string line;
            int count = 0;
            while (std::getline(file, line) && count++ < 80) {
                size_t start = line.find_first_not_of(" \t");
                if (start == std::string::npos) continue;
                std::string trimmed = line.substr(start);
                if (trimmed.rfind("exec ", 0) == 0) {
                    std::string target = trimmed.substr(5);
                    size_t tstart = target.find_first_not_of(" \t\"'");
                    if (tstart != std::string::npos) {
                        target = target.substr(tstart);
                        size_t tend = target.find_first_of(" \t\"'");
                        if (tend != std::string::npos) {
                            target = target.substr(0, tend);
                        }
                        if (!target.empty() && target.front() == '/' && fs::exists(target, ec)) {
                            return resolve_real_executable(target);
                        }
                    }
                }
            }
        }
    }

    return current;
}

std::string AppsBackend::resolve_binary_path(const std::string& exec_cmd) {
    if (exec_cmd.empty()) return "";
    std::stringstream ss(exec_cmd);
    std::string token;
    ss >> token;
    if (token.empty()) return "";

    if (token.front() == '"' || token.front() == '\'') token.erase(0, 1);
    if (!token.empty() && (token.back() == '"' || token.back() == '\'')) token.pop_back();

    std::string candidate;
    if (token.front() == '/') {
        candidate = fs::exists(token) ? token : "";
    } else if (fs::exists("/usr/bin/" + token)) {
        candidate = "/usr/bin/" + token;
    } else if (fs::exists("/usr/local/bin/" + token)) {
        candidate = "/usr/local/bin/" + token;
    } else if (fs::exists("/bin/" + token)) {
        candidate = "/bin/" + token;
    } else if (const char* h = getenv("HOME"); h && fs::exists(std::string(h) + "/.local/bin/" + token)) {
        candidate = std::string(h) + "/.local/bin/" + token;
    } else {
        candidate = run_cmd("which " + token + " 2>/dev/null");
    }

    if (candidate.empty()) return "";
    return resolve_real_executable(candidate);
}

std::string AppsBackend::format_bytes(uint64_t bytes) {
    if (bytes == 0) return "0 B";
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1);
    if (bytes >= 1024ULL * 1024ULL * 1024ULL) {
        ss << (bytes / (1024.0 * 1024.0 * 1024.0)) << " GB";
    } else if (bytes >= 1024ULL * 1024ULL) {
        ss << (bytes / (1024.0 * 1024.0)) << " MB";
    } else if (bytes >= 1024ULL) {
        ss << (bytes / 1024.0) << " KB";
    } else {
        ss << bytes << " B";
    }
    return ss.str();
}

std::string AppsBackend::get_icon_for_app(const std::string& categories, const std::string& name, const std::string& id) {
    std::string lower = categories + " " + name + " " + id;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower.find("firefox") != std::string::npos) return "󰈹";
    if (lower.find("chrome") != std::string::npos || lower.find("chromium") != std::string::npos ||
        lower.find("brave") != std::string::npos || lower.find("webbrowser") != std::string::npos) return "󰖟";
    if (lower.find("kitty") != std::string::npos || lower.find("terminal") != std::string::npos) return "󰄛";
    if (lower.find("code") != std::string::npos || lower.find("development") != std::string::npos ||
        lower.find("ide") != std::string::npos) return "󰨞";
    if (lower.find("music") != std::string::npos || lower.find("audioplayer") != std::string::npos) return "󰝚";
    if (lower.find("player") != std::string::npos || lower.find("video") != std::string::npos ||
        lower.find("vlc") != std::string::npos) return "󰕼";
    if (lower.find("gimp") != std::string::npos || lower.find("image") != std::string::npos ||
        lower.find("graphics") != std::string::npos || lower.find("photo") != std::string::npos) return "󰽉";
    if (lower.find("filemanager") != std::string::npos || lower.find("files") != std::string::npos) return "󰉋";
    if (lower.find("settings") != std::string::npos || lower.find("preferences") != std::string::npos) return "󰒓";
    if (lower.find("security") != std::string::npos || lower.find("firewall") != std::string::npos) return "󰒍";
    if (lower.find("monitor") != std::string::npos || lower.find("system") != std::string::npos) return "󰍹";
    if (lower.find("office") != std::string::npos || lower.find("texteditor") != std::string::npos) return "󰏫";
    return "󰏖";
}

std::string AppsBackend::get_icon_for_pkg(const std::string& pkg_name) {
    std::string lower = pkg_name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower.find("git") != std::string::npos || lower.find("clang") != std::string::npos ||
        lower.find("gcc") != std::string::npos || lower.find("cmake") != std::string::npos ||
        lower.find("python") != std::string::npos || lower.find("rust") != std::string::npos) return "󰘦";
    if (lower.find("zsh") != std::string::npos || lower.find("bash") != std::string::npos ||
        lower.find("tmux") != std::string::npos || lower.find("fish") != std::string::npos) return "󰆍";
    if (lower.find("bluez") != std::string::npos || lower.find("network") != std::string::npos ||
        lower.find("curl") != std::string::npos || lower.find("wget") != std::string::npos ||
        lower.find("opensnitch") != std::string::npos || lower.find("ssh") != std::string::npos) return "󰒍";
    if (lower.find("alsa") != std::string::npos || lower.find("audio") != std::string::npos ||
        lower.find("pipewire") != std::string::npos || lower.find("sound") != std::string::npos) return "󰝚";
    if (lower.find("acpi") != std::string::npos || lower.find("systemd") != std::string::npos ||
        lower.find("brightness") != std::string::npos || lower.find("btop") != std::string::npos ||
        lower.find("htop") != std::string::npos) return "󰍹";
    return "󰏖";
}

std::vector<std::shared_ptr<SettingItem>> AppsBackend::get_desktop_app_items(bool force_refresh) {
    static std::vector<std::shared_ptr<SettingItem>> s_cached_desktop_apps;
    if (!s_cached_desktop_apps.empty() && !force_refresh) {
        return s_cached_desktop_apps;
    }

    const char* home = getenv("HOME");
    std::vector<std::string> search_dirs;
    if (home) search_dirs.push_back(std::string(home) + "/.local/share/applications");
    search_dirs.push_back("/usr/share/applications");
    search_dirs.push_back("/usr/local/share/applications");
    search_dirs.push_back("/var/lib/flatpak/exports/share/applications");

    std::unordered_set<std::string> seen_ids;
    std::vector<std::shared_ptr<SettingItem>> items;
    std::error_code ec;

    for (const auto& dir : search_dirs) {
        if (!fs::exists(dir, ec)) continue;
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".desktop") continue;
            std::string id = entry.path().filename().string();
            if (seen_ids.count(id)) continue;

            DesktopEntry de;
            if (!parse_desktop_file(entry.path().string(), de)) continue;
            seen_ids.insert(id);

            std::string safe_id = "app_" + id;
            for (char& c : safe_id) {
                if (!std::isalnum(c) && c != '_') c = '_';
            }

            std::string icon = get_icon_for_app(de.categories, de.name, id);
            auto item = SettingItem::create_leaf(safe_id, de.name, "", icon);
            item->is_installed_app = true;

            item->app_info.app_name = de.name;
            item->app_info.description = !de.comment.empty() ? de.comment : de.generic_name;
            item->app_info.category = de.categories;
            item->app_info.desktop_file = de.desktop_path;
            item->app_info.exec_cmd = de.exec_cmd;
            item->app_info.binary_path = resolve_binary_path(de.exec_cmd);
            item->app_info.permissions = "Wayland Protocol, PipeWire Audio, Storage (~/)";
            item->app_info.publisher = "Arch Linux / Open Source";

            if (de.desktop_path.find("/.local/share/applications") != std::string::npos) {
                item->app_info.package_type = "User Application";
                item->app_info.is_user_app = true;
            } else if (de.desktop_path.find("/flatpak/") != std::string::npos) {
                item->app_info.package_type = "Flatpak";
                item->app_info.is_flatpak = true;
            } else {
                item->app_info.package_type = "Arch Linux Package (pacman)";
            }

            items.push_back(std::move(item));
        }
    }

    std::sort(items.begin(), items.end(), [](const std::shared_ptr<SettingItem>& a, const std::shared_ptr<SettingItem>& b) {
        std::string a_lower = a->title;
        std::string b_lower = b->title;
        std::transform(a_lower.begin(), a_lower.end(), a_lower.begin(), ::tolower);
        std::transform(b_lower.begin(), b_lower.end(), b_lower.begin(), ::tolower);
        return a_lower < b_lower;
    });

    s_cached_desktop_apps = items;
    return items;
}

std::vector<std::shared_ptr<SettingItem>> AppsBackend::get_system_package_items(bool force_refresh) {
    static std::vector<std::shared_ptr<SettingItem>> s_cached_pkgs;
    if (!s_cached_pkgs.empty() && !force_refresh) {
        return s_cached_pkgs;
    }

    std::string out = run_cmd("pacman -Qe 2>/dev/null");
    std::stringstream ss(out);
    std::string line;
    std::vector<std::shared_ptr<SettingItem>> items;

    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        std::stringstream line_ss(line);
        std::string pkg_name, pkg_version;
        line_ss >> pkg_name >> pkg_version;
        if (pkg_name.empty()) continue;

        std::string safe_id = "pkg_" + pkg_name;
        for (char& c : safe_id) {
            if (!std::isalnum(c) && c != '_') c = '_';
        }

        std::string icon = get_icon_for_pkg(pkg_name);
        auto item = SettingItem::create_leaf(safe_id, pkg_name, "", icon);
        item->is_installed_app = true;

        item->app_info.app_name = pkg_name;
        item->app_info.version = pkg_version;
        item->app_info.package_name = pkg_name;
        item->app_info.package_type = "Arch Linux System Package (pacman)";
        item->app_info.category = "System & Command Line";
        item->app_info.permissions = "System Process, POSIX System Calls";
        item->app_info.publisher = "Arch Linux Packaging Team";

        std::string bin_path;
        if (fs::exists("/usr/bin/" + pkg_name)) {
            bin_path = "/usr/bin/" + pkg_name;
        } else if (fs::exists("/usr/local/bin/" + pkg_name)) {
            bin_path = "/usr/local/bin/" + pkg_name;
        } else if (fs::exists("/bin/" + pkg_name)) {
            bin_path = "/bin/" + pkg_name;
        }
        item->app_info.binary_path = bin_path;
        item->app_info.exec_cmd = bin_path;

        items.push_back(std::move(item));
    }

    s_cached_pkgs = items;
    return items;
}

void AppsBackend::populate_app_details(InstalledAppInfo& info, std::vector<TelemetryEntry>& telemetry) {
    if (info.binary_path.empty() && !info.package_name.empty()) {
        std::string ql = run_cmd("pacman -Ql " + info.package_name + " 2>/dev/null | grep '/usr/bin/' | head -n 1");
        size_t sp = ql.find(' ');
        if (sp != std::string::npos) {
            std::string candidate = ql.substr(sp + 1);
            if (fs::is_regular_file(candidate)) {
                info.binary_path = candidate;
                info.exec_cmd = candidate;
            }
        }
    }

    if (info.package_name.empty() && !info.desktop_file.empty()) {
        if (info.is_flatpak) {
            query_flatpak_metadata(info, telemetry);
        } else if (!info.is_user_app) {
            query_pacman_metadata(info, telemetry);
        }
    } else if (!info.package_name.empty() && telemetry.empty()) {
        query_pacman_metadata(info, telemetry);
    }
    calculate_storage_sizes(info);
}

void AppsBackend::query_flatpak_metadata(InstalledAppInfo& info, std::vector<TelemetryEntry>& telemetry) {
    std::string id = fs::path(info.desktop_file).stem().string();
    std::string out = run_cmd("flatpak info " + id + " 2>/dev/null");
    if (out.empty()) return;

    std::stringstream ss(out);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.find("Version:") != std::string::npos) {
            info.version = line.substr(line.find(':') + 1);
            while (!info.version.empty() && info.version.front() == ' ') info.version.erase(0, 1);
        } else if (line.find("Installed:") != std::string::npos) {
            info.size_total = line.substr(line.find(':') + 1);
            while (!info.size_total.empty() && info.size_total.front() == ' ') info.size_total.erase(0, 1);
        }
    }
    telemetry.push_back({"Application Runtime", "Flatpak Sandboxed Container"});
    telemetry.push_back({"Runtime Environment", "org.freedesktop.Platform"});
}

void AppsBackend::query_pacman_metadata(InstalledAppInfo& info, std::vector<TelemetryEntry>& telemetry) {
    if (info.package_name.empty() && !info.desktop_file.empty()) {
        std::string q_out = run_cmd("pacman -Qo " + info.desktop_file + " 2>/dev/null");
        size_t owned_pos = q_out.find("is owned by ");
        if (owned_pos != std::string::npos) {
            std::string pkg_str = q_out.substr(owned_pos + 12);
            size_t sp = pkg_str.find(' ');
            if (sp != std::string::npos) {
                info.package_name = pkg_str.substr(0, sp);
                info.version = pkg_str.substr(sp + 1);
            } else {
                info.package_name = pkg_str;
            }
        }
    }

    if (info.package_name.empty()) return;

    std::string i_out = run_cmd("pacman -Qi " + info.package_name + " 2>/dev/null");
    std::stringstream ss(i_out);
    std::string line, licenses, arch, packager, url, depends;
    while (std::getline(ss, line)) {
        if (line.rfind("Installed Size  :", 0) == 0) {
            info.size_total = line.substr(17);
            while (!info.size_total.empty() && info.size_total.front() == ' ') info.size_total.erase(0, 1);
        } else if (line.rfind("Description     :", 0) == 0 && info.description.empty()) {
            info.description = line.substr(17);
            while (!info.description.empty() && info.description.front() == ' ') info.description.erase(0, 1);
        } else if (line.rfind("Packager        :", 0) == 0) {
            packager = line.substr(17);
            while (!packager.empty() && packager.front() == ' ') packager.erase(0, 1);
            info.publisher = packager;
        } else if (line.rfind("Licenses        :", 0) == 0) {
            licenses = line.substr(17);
            while (!licenses.empty() && licenses.front() == ' ') licenses.erase(0, 1);
        } else if (line.rfind("Architecture    :", 0) == 0) {
            arch = line.substr(17);
            while (!arch.empty() && arch.front() == ' ') arch.erase(0, 1);
        } else if (line.rfind("URL             :", 0) == 0) {
            url = line.substr(17);
            while (!url.empty() && url.front() == ' ') url.erase(0, 1);
        } else if (line.rfind("Depends On      :", 0) == 0) {
            depends = line.substr(17);
            while (!depends.empty() && depends.front() == ' ') depends.erase(0, 1);
        }
    }

    telemetry.clear();
    if (!arch.empty()) telemetry.push_back({"Binary Architecture", arch + " (Linux ELF)"});
    if (!licenses.empty()) telemetry.push_back({"Open Source License", licenses});
    if (!url.empty()) telemetry.push_back({"Upstream Project", url});
    if (!depends.empty()) telemetry.push_back({"Core Dependencies", depends});
    if (!packager.empty()) telemetry.push_back({"Package Maintainer", packager});
}

void AppsBackend::calculate_storage_sizes(InstalledAppInfo& info) {
    std::error_code ec;
    if (!info.binary_path.empty() && fs::exists(info.binary_path, ec)) {
        auto sz = fs::file_size(info.binary_path, ec);
        info.size_binary = ec ? "N/A" : format_bytes(sz);
    } else {
        info.size_binary = "N/A";
    }

    uint64_t cache_bytes = 0;
    const char* home = getenv("HOME");
    if (home) {
        std::string bin_name = fs::path(info.binary_path).filename().string();
        std::vector<std::string> paths;
        if (!bin_name.empty()) paths.push_back(std::string(home) + "/.cache/" + bin_name);
        if (!info.package_name.empty() && info.package_name != bin_name) {
            paths.push_back(std::string(home) + "/.cache/" + info.package_name);
        }

        for (const auto& p : paths) {
            if (fs::exists(p, ec) && fs::is_directory(p, ec)) {
                for (const auto& f : fs::recursive_directory_iterator(p, ec)) {
                    if (f.is_regular_file(ec)) cache_bytes += f.file_size(ec);
                }
            }
        }
    }

    info.size_data = format_bytes(cache_bytes);
    if (info.size_total.empty() || info.size_total == "0 B") {
        info.size_total = info.size_binary;
    }
}

int AppsBackend::find_running_pid(const std::string& binary_path) {
    if (binary_path.empty()) return 0;
    std::string bin_name = fs::path(binary_path).filename().string();
    if (bin_name.empty()) return 0;

    std::error_code ec;
    for (const auto& entry : fs::directory_iterator("/proc", ec)) {
        if (!entry.is_directory()) continue;
        std::string dirname = entry.path().filename().string();
        if (dirname.empty() || !std::isdigit(dirname[0])) continue;

        int pid = std::atoi(dirname.c_str());
        if (pid <= 0) continue;

        std::ifstream comm_file(entry.path() / "comm");
        if (comm_file.is_open()) {
            std::string comm;
            std::getline(comm_file, comm);
            if (comm == bin_name || (bin_name.size() > 15 && comm == bin_name.substr(0, 15))) {
                return pid;
            }
        }

        auto exe_path = fs::read_symlink(entry.path() / "exe", ec);
        if (!ec && (exe_path.string() == binary_path || exe_path.filename().string() == bin_name)) {
            return pid;
        }
    }
    return 0;
}

bool AppsBackend::launch_app(const InstalledAppInfo& info) {
    std::string cmd = !info.exec_cmd.empty() ? info.exec_cmd : info.binary_path;
    if (cmd.empty()) return false;

    // Strip XDG field codes (%u, %F, etc.)
    size_t pct = cmd.find('%');
    while (pct != std::string::npos) {
        size_t next_sp = cmd.find(' ', pct);
        if (next_sp != std::string::npos) {
            cmd.erase(pct, next_sp - pct + 1);
        } else {
            cmd.erase(pct);
        }
        pct = cmd.find('%');
    }

    std::string launch_cmd = cmd + " &";
    int ret = std::system(launch_cmd.c_str());
    return ret == 0;
}

bool AppsBackend::kill_app(const InstalledAppInfo& info) {
    if (info.pid > 0) {
        kill(info.pid, SIGTERM);
        return true;
    }
    std::string bin_name = fs::path(info.binary_path).filename().string();
    if (!bin_name.empty()) {
        run_cmd("pkill -x " + bin_name + " 2>/dev/null");
        return true;
    }
    return false;
}

bool AppsBackend::uninstall_app(const InstalledAppInfo& info, std::string& error_msg) {
    if (info.is_user_app) {
        if (!info.desktop_file.empty() && fs::exists(info.desktop_file)) {
            std::error_code ec;
            fs::remove(info.desktop_file, ec);
            if (ec) {
                error_msg = ec.message();
                return false;
            }
            get_desktop_app_items(true);
            return true;
        }
        error_msg = "Desktop file not found on disk.";
        return false;
    }

    if (info.is_flatpak) {
        std::string flatpak_id = fs::path(info.desktop_file).stem().string();
        std::string cmd = "flatpak uninstall -y " + flatpak_id;
        int ret = std::system(cmd.c_str());
        if (ret == 0) {
            get_desktop_app_items(true);
            return true;
        }
        error_msg = "Flatpak uninstallation failed.";
        return false;
    }

    if (!info.package_name.empty()) {
        std::string cmd = "pkexec pacman -R --noconfirm " + info.package_name;
        int ret = std::system(cmd.c_str());
        if (ret == 0) {
            get_desktop_app_items(true);
            get_system_package_items(true);
            return true;
        }
        error_msg = "pacman removal exited with code " + std::to_string(ret);
        return false;
    }

    error_msg = "Package name could not be resolved.";
    return false;
}

bool AppsBackend::clear_cache(const InstalledAppInfo& info, std::string& error_msg) {
    const char* home = getenv("HOME");
    if (!home) {
        error_msg = "HOME directory not found";
        return false;
    }

    std::string bin_name = fs::path(info.binary_path).filename().string();
    std::vector<std::string> paths;
    if (!bin_name.empty()) paths.push_back(std::string(home) + "/.cache/" + bin_name);
    if (!info.package_name.empty() && info.package_name != bin_name) {
        paths.push_back(std::string(home) + "/.cache/" + info.package_name);
    }

    std::error_code ec;
    for (const auto& p : paths) {
        if (fs::exists(p, ec)) {
            fs::remove_all(p, ec);
        }
    }
    return true;
}

std::string AppsBackend::query_network_policy(const std::string& binary_path) {
    if (binary_path.empty()) return "Allow";
    const std::string rules_dir = "/etc/opensnitchd/rules";
    std::error_code ec;
    if (!fs::exists(rules_dir, ec)) return "Allow";

    std::string bin_name = fs::path(binary_path).filename().string();
    std::string real_bin = resolve_real_executable(binary_path);
    std::string real_name = !real_bin.empty() ? fs::path(real_bin).filename().string() : bin_name;

    for (const auto& entry : fs::directory_iterator(rules_dir, ec)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;

        std::string fname = entry.path().filename().string();
        bool matches_file = (fname == "miqu-" + bin_name + ".json" ||
                             fname == "miqu-block-" + bin_name + ".json" ||
                             fname == "miqu-local-" + bin_name + ".json" ||
                             (!real_name.empty() && (fname == "miqu-" + real_name + ".json" ||
                                                     fname == "miqu-block-" + real_name + ".json" ||
                                                     fname == "miqu-local-" + real_name + ".json")));

        std::ifstream file(entry.path());
        if (!file.is_open()) continue;
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();

        bool matches_content = (content.find(binary_path) != std::string::npos ||
                                content.find("\"" + bin_name + "\"") != std::string::npos ||
                                (!real_bin.empty() && content.find(real_bin) != std::string::npos) ||
                                (!real_name.empty() && content.find("\"" + real_name + "\"") != std::string::npos));

        if (!matches_file && !matches_content) {
            continue;
        }

        if (content.find("\"enabled\": false") != std::string::npos ||
            content.find("\"enabled\":false") != std::string::npos) {
            continue;
        }

        if (content.find("\"action\": \"deny\"") != std::string::npos ||
            content.find("\"action\":\"deny\"") != std::string::npos ||
            content.find("\"action\": \"reject\"") != std::string::npos ||
            content.find("\"action\":\"reject\"") != std::string::npos) {
            return "Block";
        }
        if (content.find("dest.network") != std::string::npos ||
            content.find("192.168.") != std::string::npos ||
            content.find("10.0.0.0") != std::string::npos) {
            return "Local Only";
        }
        return "Allow";
    }
    return "Allow";
}

std::string AppsBackend::query_network_policy(const InstalledAppInfo& info) {
    if (!info.binary_path.empty()) {
        std::string pol = query_network_policy(info.binary_path);
        if (pol != "Allow") return pol;
    }
    if (!info.exec_cmd.empty()) {
        std::string resolved = resolve_binary_path(info.exec_cmd);
        if (!resolved.empty() && resolved != info.binary_path) {
            std::string pol = query_network_policy(resolved);
            if (pol != "Allow") return pol;
        }
    }
    return "Allow";
}

bool AppsBackend::set_network_policy(const InstalledAppInfo& info, const std::string& policy, std::string& message) {
    std::string bin_path = !info.binary_path.empty() ? info.binary_path : resolve_binary_path(info.exec_cmd);
    bool ret = set_network_policy(bin_path, policy, message);
    if (ret && (info.is_running || info.pid > 0) && policy == "Block") {
        message += " (Restart app to close active connections)";
    }
    return ret;
}

bool AppsBackend::set_network_policy(const std::string& binary_path, const std::string& policy, std::string& message) {
    if (binary_path.empty()) {
        message = "Cannot set network policy: missing binary path";
        return false;
    }

    std::string bin_name = fs::path(binary_path).filename().string();
    std::string real_bin = resolve_real_executable(binary_path);
    std::string real_name = !real_bin.empty() ? fs::path(real_bin).filename().string() : bin_name;

    // The rule name in OpenSnitch MUST match the filename without .json ("miqu-" + bin_name)
    // so OpenSnitch deleteRule can cleanly unload the rule from memory when removed.
    std::string rule_file = "/etc/opensnitchd/rules/miqu-" + bin_name + ".json";
    std::string real_rule_file = "/etc/opensnitchd/rules/miqu-" + real_name + ".json";
    std::string legacy_block = "/etc/opensnitchd/rules/miqu-block-" + bin_name + ".json";
    std::string legacy_local = "/etc/opensnitchd/rules/miqu-local-" + bin_name + ".json";

    bool opensnitch_installed = !run_cmd("which opensnitchd 2>/dev/null").empty() || fs::exists("/etc/opensnitchd");
    if (!opensnitch_installed) {
        message = "Network policy set to " + policy + " (Install 'opensnitch' to enforce firewall rules)";
        return true;
    }

    if (policy == "Allow") {
        std::string rm_targets = rule_file + " " + real_rule_file + " " + legacy_block + " " + legacy_local;
        std::string cmd = "pkexec sh -c \"rm -f " + rm_targets + " 2>/dev/null ; systemctl try-restart opensnitchd 2>/dev/null || true\"";
        int ret = std::system(cmd.c_str());
        if (ret != 0) {
            message = "Authentication cancelled or failed";
            return false;
        }
        message = "Network access allowed (Firewall rule removed)";
        return true;
    }

    std::string op_type = "simple";
    std::string op_data = binary_path;
    if (!real_bin.empty() && real_bin != binary_path) {
        op_type = "regexp";
        op_data = "^(" + escape_regex(binary_path) + "|" + escape_regex(real_bin) + ")$";
    }

    std::string tmp_file = "/tmp/miqu-" + bin_name + ".json";
    std::ofstream out(tmp_file);
    if (!out.is_open()) {
        message = "Failed to write temporary rule file";
        return false;
    }

    if (policy == "Block") {
        out << "{\n"
            << "  \"name\": \"miqu-" << bin_name << "\",\n"
            << "  \"enabled\": true,\n"
            << "  \"action\": \"deny\",\n"
            << "  \"duration\": \"always\",\n"
            << "  \"operator\": {\n"
            << "    \"type\": \"" << op_type << "\",\n"
            << "    \"operand\": \"process.path\",\n"
            << "    \"data\": \"" << op_data << "\"\n"
            << "  }\n"
            << "}\n";
    } else if (policy == "Local Only") {
        out << "{\n"
            << "  \"name\": \"miqu-" << bin_name << "\",\n"
            << "  \"enabled\": true,\n"
            << "  \"action\": \"allow\",\n"
            << "  \"duration\": \"always\",\n"
            << "  \"operator\": {\n"
            << "    \"type\": \"list\",\n"
            << "    \"operand\": \"list\",\n"
            << "    \"list\": [\n"
            << "      {\n"
            << "        \"type\": \"" << op_type << "\",\n"
            << "        \"operand\": \"process.path\",\n"
            << "        \"data\": \"" << op_data << "\"\n"
            << "      },\n"
            << "      {\n"
            << "        \"type\": \"network\",\n"
            << "        \"operand\": \"dest.network\",\n"
            << "        \"data\": \"10.0.0.0/8,172.16.0.0/12,192.168.0.0/16,127.0.0.0/8,fe80::/10,::1/128\"\n"
            << "      }\n"
            << "    ]\n"
            << "  }\n"
            << "}\n";
    }
    out.close();

    std::string cmd = "pkexec sh -c \"install -m 644 " + tmp_file + " " + rule_file +
                      " ; rm -f " + legacy_block + " " + legacy_local +
                      " 2>/dev/null ; systemctl try-restart opensnitchd 2>/dev/null || true\"";
    int ret = std::system(cmd.c_str());
    std::error_code ec;
    fs::remove(tmp_file, ec);

    if (ret != 0) {
        message = "Authentication cancelled or failed";
        return false;
    }

    message = (policy == "Block") ? "Network access blocked via OpenSnitch"
                                  : "Network restricted to local subnets via OpenSnitch";
    return true;
}

} // namespace miqu
