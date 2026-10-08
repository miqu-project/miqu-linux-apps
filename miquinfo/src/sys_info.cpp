#include "sys_info.hpp"
#include <miqutoolkit/system/output_manager.hpp>
#include <sys/utsname.h>
#include <sys/statvfs.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <filesystem>
#include <unistd.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

namespace fs = std::filesystem;

namespace miquinfo {

static std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n\"'");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n\"'");
    return s.substr(start, end - start + 1);
}

static std::string read_file_trimmed(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::string line;
    if (std::getline(file, line)) {
        return trim(line);
    }
    return "";
}

SystemInfo SysInfoReader::gather() {
    SystemInfo info;

    // 1. Operating System (/etc/os-release)
    {
        std::ifstream os_file("/etc/os-release");
        if (!os_file.is_open()) os_file.open("/usr/lib/os-release");
        std::string line;
        std::string logo;
        while (std::getline(os_file, line)) {
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string key = trim(line.substr(0, eq));
            std::string val = trim(line.substr(eq + 1));
            if (key == "PRETTY_NAME") {
                info.os_name = val;
            } else if (key == "NAME" && info.os_name.empty()) {
                info.os_name = val;
            } else if (key == "ID") {
                info.os_id = val;
            } else if (key == "LOGO") {
                logo = val;
            }
        }
        if (info.os_name.empty()) info.os_name = "Linux";

        if (!logo.empty()) {
            info.logo_icon = logo;
        } else if (!info.os_id.empty()) {
            info.logo_icon = "distributor-logo-" + info.os_id;
        } else {
            info.logo_icon = "distributor-logo";
        }
    }

    // 2. Host / Hardware Model
    {
        std::string product = read_file_trimmed("/sys/devices/virtual/dmi/id/product_name");
        if (product.empty() || product == "System Product Name" || product == "Default string") {
            product = read_file_trimmed("/sys/class/dmi/id/product_name");
        }
        std::string version = read_file_trimmed("/sys/devices/virtual/dmi/id/product_version");
        if (!product.empty()) {
            info.host_name = product;
            if (!version.empty() && version != "None" && version != "Default string") {
                info.host_name += " (" + version + ")";
            }
        } else {
            char hostname[256] = {0};
            if (gethostname(hostname, sizeof(hostname) - 1) == 0) {
                info.host_name = hostname;
            } else {
                info.host_name = "Generic PC";
            }
        }
    }

    // 3. Kernel & Architecture
    {
        struct utsname uts;
        if (uname(&uts) == 0) {
            info.kernel = std::string(uts.sysname) + " " + uts.release;
            info.arch = uts.machine;
        } else {
            info.kernel = "Unknown Kernel";
            info.arch = "x86_64";
        }
    }

    // 4. Uptime (/proc/uptime)
    {
        std::ifstream uptime_file("/proc/uptime");
        double sec = 0.0;
        if (uptime_file >> sec) {
            uint64_t total_secs = static_cast<uint64_t>(sec);
            uint64_t days = total_secs / 86400;
            uint64_t hours = (total_secs % 86400) / 3600;
            uint64_t minutes = (total_secs % 3600) / 60;

            std::stringstream ss;
            if (days > 0) ss << days << "d ";
            if (hours > 0 || days > 0) ss << hours << "h ";
            ss << minutes << "m";
            info.uptime = ss.str();
        } else {
            info.uptime = "Unknown";
        }
    }

    // 5. Load Average & Tasks (/proc/loadavg)
    {
        std::ifstream load_file("/proc/loadavg");
        if (load_file.is_open()) {
            std::string proc_tasks;
            if (load_file >> info.load_1m >> info.load_5m >> info.load_15m >> proc_tasks) {
                auto slash = proc_tasks.find('/');
                if (slash != std::string::npos) {
                    try {
                        info.running_tasks = std::stoi(proc_tasks.substr(0, slash));
                        info.total_tasks = std::stoi(proc_tasks.substr(slash + 1));
                    } catch (...) {}
                }
            }
        }
    }

    // 6. CPU (/proc/cpuinfo & sysfs)
    {
        std::ifstream cpu_file("/proc/cpuinfo");
        std::string line;
        int count = 0;
        int cpu_cores_dmi = 0;
        while (std::getline(cpu_file, line)) {
            auto colon = line.find(':');
            if (colon == std::string::npos) continue;
            std::string key = trim(line.substr(0, colon));
            std::string val = trim(line.substr(colon + 1));
            if (key == "model name" && info.cpu_model.empty()) {
                info.cpu_model = val;
            } else if (key == "processor") {
                count++;
            } else if (key == "cpu cores" && cpu_cores_dmi == 0) {
                try { cpu_cores_dmi = std::stoi(val); } catch (...) {}
            }
        }
        info.cpu_threads = count > 0 ? count : 1;
        info.cpu_cores = cpu_cores_dmi > 0 ? cpu_cores_dmi : info.cpu_threads;

        if (info.cpu_threads > 0) {
            info.cpu_load_fraction = info.load_1m / static_cast<float>(info.cpu_threads);
            if (info.cpu_load_fraction > 1.0f) info.cpu_load_fraction = 1.0f;
        }

        std::string cur_freq = read_file_trimmed("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
        std::string max_freq = read_file_trimmed("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq");
        std::string min_freq = read_file_trimmed("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_min_freq");
        std::string gov = read_file_trimmed("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor");
        try {
            if (!cur_freq.empty()) info.cpu_freq_cur_mhz = std::stoi(cur_freq) / 1000;
            if (!max_freq.empty()) info.cpu_freq_max_mhz = std::stoi(max_freq) / 1000;
            if (!min_freq.empty()) info.cpu_freq_min_mhz = std::stoi(min_freq) / 1000;
        } catch (...) {}
        info.cpu_governor = gov.empty() ? "powersave" : gov;

        info.cpu_cache_l1 = read_file_trimmed("/sys/devices/system/cpu/cpu0/cache/index0/size");
        info.cpu_cache_l2 = read_file_trimmed("/sys/devices/system/cpu/cpu0/cache/index2/size");
        info.cpu_cache_l3 = read_file_trimmed("/sys/devices/system/cpu/cpu0/cache/index3/size");
    }

    // 7. Motherboard & BIOS
    {
        info.mb_vendor = read_file_trimmed("/sys/class/dmi/id/board_vendor");
        info.mb_model = read_file_trimmed("/sys/class/dmi/id/board_name");
        info.bios_vendor = read_file_trimmed("/sys/class/dmi/id/bios_vendor");
        info.bios_version = read_file_trimmed("/sys/class/dmi/id/bios_version");
        info.bios_date = read_file_trimmed("/sys/class/dmi/id/bios_date");

        if (fs::exists("/sys/firmware/efi")) {
            info.boot_mode = "UEFI";
            std::error_code ec;
            if (fs::exists("/sys/firmware/efi/efivars", ec)) {
                for (const auto& entry : fs::directory_iterator("/sys/firmware/efi/efivars", ec)) {
                    if (entry.path().filename().string().rfind("SecureBoot-", 0) == 0) {
                        std::ifstream sb_file(entry.path(), std::ios::binary);
                        if (sb_file.is_open()) {
                            char buf[5] = {0};
                            if (sb_file.read(buf, 5)) {
                                info.secure_boot = (buf[4] == 1);
                            }
                        }
                        break;
                    }
                }
            }
        } else {
            info.boot_mode = "Legacy BIOS";
            info.secure_boot = false;
        }
    }

    // 8. GPU Detection & Graphics Stack
    {
        FILE* pipe = popen("lspci 2>/dev/null | grep -E 'VGA|3D|Display'", "r");
        if (pipe) {
            char buffer[512] = {0};
            if (fgets(buffer, sizeof(buffer) - 1, pipe)) {
                std::string line = trim(buffer);
                auto colon = line.find(':');
                if (colon != std::string::npos) {
                    auto second_colon = line.find(':', colon + 1);
                    if (second_colon != std::string::npos) {
                        line = trim(line.substr(second_colon + 1));
                    }
                }
                auto rev_pos = line.find("(rev");
                if (rev_pos != std::string::npos) {
                    line = trim(line.substr(0, rev_pos));
                }
                info.gpu_model = line;
            }
            pclose(pipe);
        }
        if (info.gpu_model.empty()) info.gpu_model = "Standard Graphics Adapter";

        // GPU Driver
        std::error_code ec;
        for (int i = 0; i < 4; ++i) {
            std::string drm_card = "/sys/class/drm/card" + std::to_string(i) + "/device/uevent";
            if (fs::exists(drm_card, ec)) {
                std::ifstream uevent(drm_card);
                std::string uline;
                while (std::getline(uevent, uline)) {
                    if (uline.rfind("DRIVER=", 0) == 0) {
                        info.gpu_driver = trim(uline.substr(7));
                        break;
                    }
                }
                if (!info.gpu_driver.empty()) break;
            }
        }
        if (info.gpu_driver.empty()) info.gpu_driver = "drm/generic";

        // Mesa Version
        FILE* mpipe = popen("pacman -Q mesa 2>/dev/null | awk '{print $2}'", "r");
        if (mpipe) {
            char mbuf[128] = {0};
            if (fgets(mbuf, sizeof(mbuf) - 1, mpipe)) {
                info.mesa_version = trim(mbuf);
            }
            pclose(mpipe);
        }
        if (info.mesa_version.empty()) info.mesa_version = "Standard Mesa/GL";
    }

    // 9. Memory & Swap (/proc/meminfo)
    {
        std::ifstream mem_file("/proc/meminfo");
        std::string line;
        uint64_t total_kb = 0;
        uint64_t avail_kb = 0;
        uint64_t free_kb = 0;
        uint64_t buffers_kb = 0;
        uint64_t cached_kb = 0;
        uint64_t swap_total_kb = 0;
        uint64_t swap_free_kb = 0;

        while (std::getline(mem_file, line)) {
            auto colon = line.find(':');
            if (colon == std::string::npos) continue;
            std::string key = trim(line.substr(0, colon));
            std::string val = trim(line.substr(colon + 1));
            std::stringstream ss(val);
            uint64_t kb = 0;
            ss >> kb;
            if (key == "MemTotal") total_kb = kb;
            else if (key == "MemAvailable") avail_kb = kb;
            else if (key == "MemFree") free_kb = kb;
            else if (key == "Buffers") buffers_kb = kb;
            else if (key == "Cached") cached_kb = kb;
            else if (key == "SwapTotal") swap_total_kb = kb;
            else if (key == "SwapFree") swap_free_kb = kb;
        }

        if (total_kb > 0) {
            uint64_t used_kb = (avail_kb > 0 && total_kb > avail_kb) ? (total_kb - avail_kb) : 0;
            info.ram_total_mb = total_kb / 1024;
            info.ram_used_mb = used_kb / 1024;
            info.ram_free_mb = free_kb / 1024;
            info.ram_buffers_mb = buffers_kb / 1024;
            info.ram_cached_mb = cached_kb / 1024;
            info.ram_fraction = static_cast<float>(used_kb) / static_cast<float>(total_kb);

            double used_gib = static_cast<double>(used_kb) / (1024.0 * 1024.0);
            double total_gib = static_cast<double>(total_kb) / (1024.0 * 1024.0);
            int pct = static_cast<int>(info.ram_fraction * 100.0f);

            std::stringstream ss;
            ss << std::fixed << std::setprecision(1) << used_gib << " GiB / "
               << std::setprecision(1) << total_gib << " GiB (" << pct << "%)";
            info.ram_formatted = ss.str();
        }

        if (swap_total_kb > 0) {
            uint64_t swap_used_kb = swap_total_kb > swap_free_kb ? (swap_total_kb - swap_free_kb) : 0;
            info.swap_total_mb = swap_total_kb / 1024;
            info.swap_used_mb = swap_used_kb / 1024;
            info.swap_fraction = static_cast<float>(swap_used_kb) / static_cast<float>(swap_total_kb);

            double used_gib = static_cast<double>(swap_used_kb) / (1024.0 * 1024.0);
            double total_gib = static_cast<double>(swap_total_kb) / (1024.0 * 1024.0);
            int pct = static_cast<int>(info.swap_fraction * 100.0f);

            std::stringstream ss;
            ss << std::fixed << std::setprecision(1) << used_gib << " GiB / "
               << std::setprecision(1) << total_gib << " GiB (" << pct << "%)";
            info.swap_formatted = ss.str();
        } else {
            info.swap_formatted = "None";
        }

        // Check ZRAM
        std::ifstream swaps("/proc/swaps");
        std::string sline;
        while (std::getline(swaps, sline)) {
            if (sline.find("zram") != std::string::npos) {
                info.swap_is_zram = true;
                break;
            }
        }
    }

    // 10. Storage Partitions & Multi-Mount (/proc/mounts)
    {
        std::ifstream mounts("/proc/mounts");
        std::string mline;
        std::vector<std::string> seen_mounts;
        while (std::getline(mounts, mline)) {
            std::istringstream iss(mline);
            std::string dev, target, fstype;
            if (iss >> dev >> target >> fstype) {
                if (dev.rfind("/dev/", 0) == 0 && fstype != "devtmpfs" && fstype != "squashfs") {
                    if (std::find(seen_mounts.begin(), seen_mounts.end(), target) != seen_mounts.end()) continue;
                    seen_mounts.push_back(target);

                    struct statvfs st;
                    if (statvfs(target.c_str(), &st) == 0 && st.f_blocks > 0) {
                        uint64_t total_bytes = static_cast<uint64_t>(st.f_blocks) * st.f_frsize;
                        uint64_t free_bytes = static_cast<uint64_t>(st.f_bavail) * st.f_frsize;
                        uint64_t used_bytes = total_bytes > free_bytes ? (total_bytes - free_bytes) : 0;

                        PartitionInfo p;
                        p.mount_point = target;
                        p.device = dev;
                        p.fs_type = fstype;
                        p.total_mb = total_bytes / (1024 * 1024);
                        p.used_mb = used_bytes / (1024 * 1024);
                        p.fraction = static_cast<float>(used_bytes) / static_cast<float>(total_bytes);

                        double total_gb = static_cast<double>(total_bytes) / (1024.0 * 1024.0 * 1024.0);
                        double used_gb = static_cast<double>(used_bytes) / (1024.0 * 1024.0 * 1024.0);
                        int pct = static_cast<int>(p.fraction * 100.0f);

                        std::stringstream ss;
                        ss << std::fixed << std::setprecision(1) << used_gb << " / "
                           << std::setprecision(1) << total_gb << " GiB (" << pct << "%)";
                        p.formatted = ss.str();

                        if (target == "/") {
                            info.disk_total_gb = static_cast<uint64_t>(total_gb);
                            info.disk_used_gb = static_cast<uint64_t>(used_gb);
                            info.disk_fraction = p.fraction;
                            info.disk_formatted = p.formatted;
                        }

                        info.partitions.push_back(p);
                    }
                }
            }
        }
    }

    // 11. Thermals & Cooling (/sys/class/hwmon)
    {
        std::error_code ec;
        if (fs::exists("/sys/class/hwmon", ec)) {
            for (const auto& entry : fs::directory_iterator("/sys/class/hwmon", ec)) {
                std::string name = read_file_trimmed(entry.path() / "name");
                if (name == "coretemp" && info.cpu_temp_c == 0) {
                    std::string t = read_file_trimmed(entry.path() / "temp1_input");
                    try { if (!t.empty()) info.cpu_temp_c = std::stoi(t) / 1000; } catch (...) {}
                } else if (name.rfind("pch", 0) == 0 && info.pch_temp_c == 0) {
                    std::string t = read_file_trimmed(entry.path() / "temp1_input");
                    try { if (!t.empty()) info.pch_temp_c = std::stoi(t) / 1000; } catch (...) {}
                } else if (name.rfind("iwlwifi", 0) == 0 && info.wifi_temp_c == 0) {
                    std::string t = read_file_trimmed(entry.path() / "temp1_input");
                    try { if (!t.empty()) info.wifi_temp_c = std::stoi(t) / 1000; } catch (...) {}
                }
                if (info.fan_rpm == 0) {
                    std::string f = read_file_trimmed(entry.path() / "fan1_input");
                    try { if (!f.empty()) info.fan_rpm = std::stoi(f); } catch (...) {}
                }
            }
        }
        if (info.cpu_temp_c == 0) {
            std::string t0 = read_file_trimmed("/sys/class/thermal/thermal_zone0/temp");
            try { if (!t0.empty()) info.cpu_temp_c = std::stoi(t0) / 1000; } catch (...) {}
        }
    }

    // 12. Power Profile & AC
    {
        info.power_profile = read_file_trimmed("/sys/firmware/acpi/platform_profile");
        if (info.power_profile.empty()) info.power_profile = "balanced";

        std::string ac = read_file_trimmed("/sys/class/power_supply/AC/online");
        info.ac_online = (ac == "1");
    }

    // 13. Battery Diagnostics (/sys/class/power_supply/BAT*)
    {
        std::error_code ec;
        if (fs::exists("/sys/class/power_supply", ec)) {
            for (const auto& entry : fs::directory_iterator("/sys/class/power_supply", ec)) {
                std::string name = entry.path().filename().string();
                if (name.rfind("BAT", 0) == 0) {
                    info.has_battery = true;
                    std::string cap = read_file_trimmed(entry.path() / "capacity");
                    std::string st = read_file_trimmed(entry.path() / "status");
                    std::string model = read_file_trimmed(entry.path() / "model_name");
                    std::string tech = read_file_trimmed(entry.path() / "technology");
                    std::string cyc = read_file_trimmed(entry.path() / "cycle_count");

                    std::string full = read_file_trimmed(entry.path() / "charge_full");
                    std::string design = read_file_trimmed(entry.path() / "charge_full_design");
                    std::string now = read_file_trimmed(entry.path() / "charge_now");
                    std::string vnow = read_file_trimmed(entry.path() / "voltage_now");
                    std::string inow = read_file_trimmed(entry.path() / "current_now");

                    try {
                        if (!cap.empty()) info.battery_percent = std::stoi(cap);
                        info.battery_status = st.empty() ? "Discharging" : st;
                        info.battery_formatted = std::to_string(info.battery_percent) + "% (" + info.battery_status + ")";
                        info.battery_model = model.empty() ? "Standard Battery" : model;
                        info.battery_technology = tech.empty() ? "Li-ion" : tech;
                        if (!cyc.empty()) info.battery_cycle_count = std::stoi(cyc);

                        if (!full.empty()) info.battery_charge_full_mah = std::stoull(full) / 1000;
                        if (!design.empty()) info.battery_charge_design_mah = std::stoull(design) / 1000;
                        if (!now.empty()) info.battery_charge_now_mah = std::stoull(now) / 1000;

                        if (info.battery_charge_design_mah > 0 && info.battery_charge_full_mah > 0) {
                            info.battery_health_percent = static_cast<int>(
                                (static_cast<double>(info.battery_charge_full_mah) / static_cast<double>(info.battery_charge_design_mah)) * 100.0
                            );
                        } else {
                            info.battery_health_percent = 100;
                        }

                        if (!vnow.empty()) {
                            info.battery_voltage_v = std::stof(vnow) / 1000000.0f;
                        }
                        if (!inow.empty()) {
                            float amps = std::stof(inow) / 1000000.0f;
                            info.battery_power_watts = info.battery_voltage_v * amps;
                        }
                    } catch (...) {}
                    break;
                }
            }
        }
    }

    // 14. Displays (via miqutoolkit OutputManager)
    {
        auto outputs = miqu::OutputManager::get()->get_outputs();
        if (outputs.empty()) {
            info.displays = "Wayland Default Display";
        } else {
            std::stringstream ss;
            for (size_t i = 0; i < outputs.size(); ++i) {
                if (i > 0) ss << ", ";
                const auto& out = outputs[i];
                ss << (out.name.empty() ? "Display" : out.name) << " ("
                   << out.width << "x" << out.height;
                if (out.refresh_rate > 0) {
                    ss << "@" << (out.refresh_rate / 1000) << "Hz";
                }
                if (out.scale > 1) {
                    ss << ", " << out.scale << "x";
                }
                ss << ")";
            }
            info.displays = ss.str();
        }
    }

    // 15. Shell, Terminal & Desktop Themes
    {
        const char* shell_env = getenv("SHELL");
        if (shell_env) {
            info.shell = fs::path(shell_env).filename().string();
        } else {
            info.shell = "sh";
        }

        const char* term_env = getenv("TERM");
        info.terminal = term_env ? term_env : "wayland-term";

        const char* wd = getenv("WAYLAND_DISPLAY");
        info.wayland_display = wd ? wd : "wayland-0";

        const char* ct = getenv("XCURSOR_THEME");
        info.cursor_theme = ct ? ct : "redglass";

        info.desktop_theme = "Material 3 Dark";
        info.font_name = "Inter / Sans-Serif";
    }

    // 16. Package Count (pacman, flatpak)
    {
        std::error_code ec;
        int pacman_count = 0;
        if (fs::exists("/var/lib/pacman/local", ec)) {
            for (const auto& entry : fs::directory_iterator("/var/lib/pacman/local", ec)) {
                if (entry.is_directory(ec)) pacman_count++;
            }
        }

        int flatpak_count = 0;
        if (fs::exists("/var/lib/flatpak/app", ec)) {
            for (const auto& entry : fs::directory_iterator("/var/lib/flatpak/app", ec)) {
                if (entry.is_directory(ec)) flatpak_count++;
            }
        }
        const char* home = getenv("HOME");
        if (home) {
            std::string user_flatpak = std::string(home) + "/.local/share/flatpak/app";
            if (fs::exists(user_flatpak, ec)) {
                for (const auto& entry : fs::directory_iterator(user_flatpak, ec)) {
                    if (entry.is_directory(ec)) flatpak_count++;
                }
            }
        }

        std::stringstream ss;
        if (pacman_count > 0) {
            ss << pacman_count << " (pacman)";
        }
        if (flatpak_count > 0) {
            if (ss.tellp() > 0) ss << ", ";
            ss << flatpak_count << " (flatpak)";
        }
        if (ss.tellp() == 0) {
            info.packages = "N/A";
        } else {
            info.packages = ss.str();
        }
    }

    // 17. Network & IP
    {
        std::string default_if = "";
        std::ifstream route_file("/proc/net/route");
        if (route_file.is_open()) {
            std::string line;
            std::getline(route_file, line);
            while (std::getline(route_file, line)) {
                std::istringstream iss(line);
                std::string iface, dest;
                if (iss >> iface >> dest && dest == "00000000") {
                    default_if = iface;
                    break;
                }
            }
        }

        struct ifaddrs* ifaddr = nullptr;
        if (getifaddrs(&ifaddr) != -1) {
            for (auto* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
                if (!ifa->ifa_addr) continue;
                if (ifa->ifa_addr->sa_family == AF_INET) {
                    char host[NI_MAXHOST];
                    if (getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, nullptr, 0, NI_NUMERICHOST) == 0) {
                        std::string name = ifa->ifa_name;
                        if (!default_if.empty() && name == default_if) {
                            info.network_interface = name;
                            info.ip_address = host;
                            break;
                        } else if (name != "lo" && info.ip_address.empty()) {
                            info.network_interface = name;
                            info.ip_address = host;
                        }
                    }
                }
            }
            freeifaddrs(ifaddr);
        }

        if (info.network_interface.empty()) info.network_interface = "Disconnected";
        if (info.ip_address.empty()) info.ip_address = "None";

        if (info.network_interface.rfind("wl", 0) == 0) {
            info.network_type = "Wi-Fi (Wireless)";
        } else if (info.network_interface.rfind("en", 0) == 0 || info.network_interface.rfind("eth", 0) == 0) {
            info.network_type = "Ethernet (Wired)";
        } else {
            info.network_type = "Network";
        }
    }

    // 18. Bluetooth
    {
        std::error_code ec;
        info.bluetooth_available = fs::exists("/sys/class/bluetooth/hci0", ec);
    }

    // 19. Audio Server
    {
        const char* xdg_runtime = getenv("XDG_RUNTIME_DIR");
        std::string runtime_dir = xdg_runtime ? xdg_runtime : ("/run/user/" + std::to_string(getuid()));
        std::error_code ec;
        if (fs::exists(runtime_dir + "/pipewire-0", ec)) {
            info.audio_server = "PipeWire (with WirePlumber)";
        } else if (fs::exists(runtime_dir + "/pulse/native", ec)) {
            info.audio_server = "PulseAudio";
        } else {
            info.audio_server = "ALSA";
        }
    }

    return info;
}

std::string SystemInfo::to_plain_text() const {
    std::stringstream ss;
    ss << "==========================================\n";
    ss << " System Information (miquinfo)\n";
    ss << "==========================================\n";
    ss << "OS:          " << os_name << " (" << arch << ")\n";
    ss << "Host:        " << host_name << "\n";
    ss << "Motherboard: " << mb_vendor << " " << mb_model << "\n";
    ss << "BIOS:        " << bios_vendor << " " << bios_version << " (" << bios_date << ") [" << boot_mode;
    if (boot_mode == "UEFI") ss << (secure_boot ? ", Secure Boot: ON" : ", Secure Boot: OFF");
    ss << "]\n";
    ss << "Kernel:      " << kernel << "\n";
    ss << "Uptime:      " << uptime << "\n";
    ss << "Load:        " << load_1m << ", " << load_5m << ", " << load_15m << " (" << running_tasks << "/" << total_tasks << " tasks)\n";
    ss << "Compositor:  " << compositor << " [" << wayland_display << "]\n";
    ss << "Toolkit:     " << toolkit << "\n";
    ss << "Shell:       " << shell << " (" << terminal << ")\n";
    ss << "Packages:    " << packages << "\n";
    ss << "Processor:   " << cpu_model << " (" << cpu_cores << "C / " << cpu_threads << "T)\n";
    if (cpu_freq_cur_mhz > 0) {
        ss << "CPU Clock:   " << cpu_freq_cur_mhz << " MHz (Max " << cpu_freq_max_mhz << " MHz) [" << cpu_governor << "]\n";
    }
    if (!cpu_cache_l3.empty()) {
        ss << "CPU Cache:   L1: " << cpu_cache_l1 << " | L2: " << cpu_cache_l2 << " | L3: " << cpu_cache_l3 << "\n";
    }
    ss << "Graphics:    " << gpu_model << " [" << gpu_driver << "]\n";
    ss << "Mesa/GL:     " << mesa_version << "\n";
    ss << "Display:     " << displays << "\n";
    ss << "Memory:      " << ram_formatted << "\n";
    ss << "Swap:        " << swap_formatted << (swap_is_zram ? " [zram compressed]" : "") << "\n";
    ss << "Storage (/): " << disk_formatted << "\n";
    for (const auto& p : partitions) {
        if (p.mount_point != "/") {
            ss << "  └ " << p.mount_point << " (" << p.fs_type << "): " << p.formatted << "\n";
        }
    }
    if (cpu_temp_c > 0 || fan_rpm > 0) {
        ss << "Thermals:    CPU: " << cpu_temp_c << "°C";
        if (pch_temp_c > 0) ss << " | PCH: " << pch_temp_c << "°C";
        if (fan_rpm > 0) ss << " | Fan: " << fan_rpm << " RPM";
        ss << "\n";
    }
    if (has_battery) {
        ss << "Battery:     " << battery_formatted << " [Health: " << battery_health_percent << "%]\n";
        if (battery_power_watts > 0.05f) {
            ss << "Power:       " << std::fixed << std::setprecision(1) << battery_power_watts << " W ("
               << battery_voltage_v << " V) | AC: " << (ac_online ? "Connected" : "Disconnected")
               << " | Profile: " << power_profile << "\n";
        }
    }
    ss << "Network:     " << network_interface << " (" << ip_address << ") [" << network_type << "]\n";
    ss << "Bluetooth:   " << (bluetooth_available ? "Available" : "Not Detected") << "\n";
    ss << "Audio:       " << audio_server << "\n";
    ss << "==========================================\n";
    return ss.str();
}

std::string SystemInfo::to_markdown() const {
    std::stringstream ss;
    ss << "# System Information Report\n\n";
    ss << "**Generated by miquinfo on " << host_name << "**\n\n";
    ss << "### System & Desktop\n";
    ss << "- **OS**: " << os_name << " (" << arch << ")\n";
    ss << "- **Kernel**: " << kernel << "\n";
    ss << "- **Uptime**: " << uptime << "\n";
    ss << "- **Compositor**: " << compositor << " (`" << wayland_display << "`)\n";
    ss << "- **Shell & Terminal**: " << shell << " (" << terminal << ")\n";
    ss << "- **Packages**: " << packages << "\n\n";

    ss << "### Hardware & Firmware\n";
    ss << "- **Host / Model**: " << host_name << "\n";
    ss << "- **Motherboard**: " << mb_vendor << " " << mb_model << "\n";
    ss << "- **BIOS/UEFI**: " << bios_vendor << " " << bios_version << " (" << bios_date << ") — " << boot_mode << (secure_boot ? " (Secure Boot Enabled)" : "") << "\n";
    ss << "- **Processor**: " << cpu_model << " (" << cpu_cores << " Cores / " << cpu_threads << " Threads)\n";
    if (cpu_freq_cur_mhz > 0) {
        ss << "- **Frequency**: " << cpu_freq_cur_mhz << " MHz (Max " << cpu_freq_max_mhz << " MHz) — Governor: `" << cpu_governor << "`\n";
    }
    ss << "- **Graphics**: " << gpu_model << " (Driver: `" << gpu_driver << "`, Mesa: " << mesa_version << ")\n";
    ss << "- **Displays**: " << displays << "\n\n";

    ss << "### Memory & Storage\n";
    ss << "- **RAM**: " << ram_formatted << "\n";
    ss << "- **Swap**: " << swap_formatted << (swap_is_zram ? " (zram compressed)" : "") << "\n";
    for (const auto& p : partitions) {
        ss << "- **Partition** `" << p.mount_point << "` (" << p.fs_type << "): " << p.formatted << "\n";
    }
    ss << "\n";

    if (cpu_temp_c > 0 || has_battery) {
        ss << "### Power & Thermals\n";
        if (cpu_temp_c > 0) ss << "- **CPU Temp**: " << cpu_temp_c << "°C\n";
        if (fan_rpm > 0) ss << "- **Fan Speed**: " << fan_rpm << " RPM\n";
        if (has_battery) {
            ss << "- **Battery**: " << battery_formatted << " — Health: " << battery_health_percent << "%\n";
            ss << "- **AC Status**: " << (ac_online ? "Connected" : "Battery") << " — Profile: `" << power_profile << "`\n";
        }
        ss << "\n";
    }

    ss << "### Connectivity & Audio\n";
    ss << "- **Network**: " << network_interface << " (" << ip_address << ") — " << network_type << "\n";
    ss << "- **Audio**: " << audio_server << "\n";
    return ss.str();
}

} // namespace miquinfo

