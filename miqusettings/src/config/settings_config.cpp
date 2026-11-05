#include "config/settings_config.hpp"
#include <miqutoolkit/core/config.hpp>
#include <miqutoolkit/core/fs_utils.hpp>
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace miqu {

namespace fs = std::filesystem;

static SettingsConfig s_settings_config;

SettingsConfig& SettingsConfig::get() {
    return s_settings_config;
}

void SettingsConfig::sync_defaults_from_toolkit() {
    // Appearance tokens and metrics are sourced directly from Config::get()
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

void SettingsConfig::load(const std::string& custom_path) {
    sync_defaults_from_toolkit();
    std::string target_path;
    if (!custom_path.empty() && fs::exists(custom_path)) {
        target_path = custom_path;
    } else {
        std::string user_conf = FsUtils::get_user_config_file("miqusettings");
        target_path = fs::exists(user_conf) ? user_conf : FsUtils::get_system_config_file("miqusettings");
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
        if (line.empty() || line[0] == '#') continue;

        if (line.front() == '[' && line.back() == ']') {
            current_section = line.substr(1, line.length() - 2);
            continue;
        }

        auto eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue;

        std::string key = trim_str(line.substr(0, eq_pos));
        std::string val = trim_str(line.substr(eq_pos + 1));

        if (current_section == "general" || current_section.empty()) {
            if (key == "window_width") {
                try { window_width = std::stoi(val); } catch (...) {}
            } else if (key == "window_height") {
                try { window_height = std::stoi(val); } catch (...) {}
            } else if (key == "vim_keys") {
                vim_keys = parse_bool(val);
            }
        }
    }
}

} // namespace miqu
