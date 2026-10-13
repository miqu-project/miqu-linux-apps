#include "notify_config.hpp"
#include <miqutoolkit/core/config.hpp>
#include <miqutoolkit/core/fs_utils.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

namespace miqunotify {

namespace fs = std::filesystem;

NotifyConfig& NotifyConfig::get() {
    static NotifyConfig s_instance;
    return s_instance;
}

NotifyConfig::NotifyConfig() {
    sync_defaults_from_toolkit();
}

void NotifyConfig::sync_defaults_from_toolkit() {
    // All appearances are automatically handled via miqu::Config::get()
}

static std::string trim(const std::string& str) {
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

void NotifyConfig::load(const std::string& custom_path) {
    sync_defaults_from_toolkit();

    std::string path_to_load = custom_path;
    if (path_to_load.empty()) {
        std::string cfg_dir = miqu::FsUtils::get_user_config_dir("miqunotify");
        if (!cfg_dir.empty()) {
            std::string user_cfg = cfg_dir + "/miqunotify.conf";
            if (fs::exists(user_cfg)) {
                path_to_load = user_cfg;
            }
        }
    }

    if (path_to_load.empty() || !fs::exists(path_to_load)) {
        return;
    }

    // Let toolkit config overlay colors and metrics
    miqu::Config::get()->load_from_file(path_to_load);

    std::ifstream file(path_to_load);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        if (line.front() == '[' && line.back() == ']') continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));

        size_t comment = val.find('#');
        if (comment != std::string::npos) {
            val = trim(val.substr(0, comment));
        }

        if (key == "position") {
            std::string lower_pos = val;
            std::transform(lower_pos.begin(), lower_pos.end(), lower_pos.begin(), ::tolower);
            if (lower_pos == "top-right" || lower_pos == "top-left" ||
                lower_pos == "bottom-right" || lower_pos == "bottom-left") {
                position = lower_pos;
            }
        } else if (key == "timeout_ms") {
            try { timeout_ms = std::max(500, std::stoi(val)); } catch (...) {}
        } else if (key == "max_toasts") {
            try { max_toasts = std::clamp(std::stoi(val), 1, 20); } catch (...) {}
        } else if (key == "toast_width") {
            try { toast_width = std::clamp(std::stoi(val), 200, 800); } catch (...) {}
        } else if (key == "toast_spacing") {
            try { toast_spacing = std::clamp(std::stoi(val), 0, 50); } catch (...) {}
        } else if (key == "margin_x") {
            try { margin_x = std::clamp(std::stoi(val), 0, 100); } catch (...) {}
        } else if (key == "margin_y") {
            try { margin_y = std::clamp(std::stoi(val), 0, 100); } catch (...) {}
        } else if (key == "dnd_enabled") {
            dnd_enabled = parse_bool(val);
        } else if (key == "history_limit") {
            try { history_limit = std::clamp(std::stoi(val), 10, 500); } catch (...) {}
        } else if (key == "center_width") {
            try { center_width = std::clamp(std::stoi(val), 250, 800); } catch (...) {}
        }
    }
}

} // namespace miqunotify
