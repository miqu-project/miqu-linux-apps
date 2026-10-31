#include "config/power_config.hpp"
#include <miqutoolkit/core/config.hpp>
#include <miqutoolkit/core/fs_utils.hpp>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <iostream>

namespace miqu {

namespace fs = std::filesystem;

static PowerConfig s_power_config;

PowerConfig& PowerConfig::get() {
    return s_power_config;
}

static std::string trim_str(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n\"'");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n\"'");
    return str.substr(first, (last - first + 1));
}

static bool parse_bool(const std::string& val) {
    std::string s = val;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return (s == "true" || s == "1" || s == "yes" || s == "on");
}

void PowerConfig::load(const std::string& custom_path) {
    std::string target_path;
    if (!custom_path.empty() && fs::exists(custom_path)) {
        target_path = custom_path;
    } else {
        std::string user_conf = FsUtils::get_user_config_file("miqupower");
        target_path = fs::exists(user_conf) ? user_conf : FsUtils::get_system_config_file("miqupower");
    }

    if (target_path.empty() || !fs::exists(target_path)) {
        return;
    }

    // 1. Synchronize toolkit theme
    Config::get()->load_from_file(target_path);

    // 2. Parse options
    std::ifstream file(target_path);
    if (!file.is_open()) return;

    std::string line;
    std::string current_section = "";

    while (std::getline(file, line)) {
        line = trim_str(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;

        if (line.front() == '[' && line.back() == ']') {
            current_section = trim_str(line.substr(1, line.size() - 2));
            std::transform(current_section.begin(), current_section.end(), current_section.begin(), ::tolower);
            continue;
        }

        auto eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue;

        std::string key = trim_str(line.substr(0, eq_pos));
        std::string val = trim_str(line.substr(eq_pos + 1));
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);

        size_t c_pos = val.find('#');
        if (c_pos != std::string::npos) {
            val = trim_str(val.substr(0, c_pos));
        }

        if (key == "width") {
            try { width = std::max(320, std::stoi(val)); } catch (...) {}
        } else if (key == "height") {
            try { height = std::max(160, std::stoi(val)); } catch (...) {}
        } else if (key == "dim_backdrop" || key == "dim") {
            dim_backdrop = parse_bool(val);
        } else if (key == "close_on_click_outside") {
            close_on_click_outside = parse_bool(val);
        } else if (key == "close_on_escape") {
            close_on_escape = parse_bool(val);
        } else if (key == "warn_running_apps") {
            warn_running_apps = parse_bool(val);
        } else if (key == "max_running_apps") {
            try { max_running_apps = std::max(1, std::stoi(val)); } catch (...) {}
        } else if (key == "graceful_timeout_ms") {
            try { graceful_timeout_ms = std::max(500U, static_cast<uint32_t>(std::stoul(val))); } catch (...) {}
        } else if (key == "lock_before_suspend") {
            lock_before_suspend = parse_bool(val);
        } else if (key == "enable_shutdown") {
            enable_shutdown = parse_bool(val);
        } else if (key == "enable_reboot") {
            enable_reboot = parse_bool(val);
        } else if (key == "enable_suspend") {
            enable_suspend = parse_bool(val);
        } else if (key == "enable_lock") {
            enable_lock = parse_bool(val);
        } else if (key == "enable_logout") {
            enable_logout = parse_bool(val);
        } else if (key == "enable_hibernate") {
            enable_hibernate = parse_bool(val);
        } else if (key == "lock_cmd") {
            lock_cmd = val;
        } else if (key == "logout_cmd") {
            logout_cmd = val;
        } else if (key == "shutdown_cmd") {
            shutdown_cmd = val;
        } else if (key == "reboot_cmd") {
            reboot_cmd = val;
        } else if (key == "suspend_cmd") {
            suspend_cmd = val;
        } else if (key == "hibernate_cmd") {
            hibernate_cmd = val;
        }
    }
}

} // namespace miqu
