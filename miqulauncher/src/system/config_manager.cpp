#include "config_manager.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <algorithm>

namespace miqu {

namespace fs = std::filesystem;

static std::string trim_str(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n\"'");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n\"'");
    return str.substr(first, (last - first + 1));
}

std::string ConfigManager::get_user_config_path() {
    return FsUtils::get_user_config_file("miqulauncher");
}

void ConfigManager::load(LauncherConfig& config) {
    std::string target_path;
    if (!config.config_path.empty() && fs::exists(config.config_path)) {
        target_path = config.config_path;
    } else {
        std::string user_path = get_user_config_path();
        target_path = fs::exists(user_path) ? user_path : FsUtils::get_system_config_file("miqulauncher");
    }

    if (target_path.empty() || !fs::exists(target_path)) {
        return;
    }

    // 1. Let Config load all colors and metrics from this config file
    Config::get()->load_from_file(target_path);

    // 2. Parse launcher-specific layout properties
    std::ifstream file(target_path);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        line = trim_str(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;

        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue;

        std::string key = trim_str(line.substr(0, eq_pos));
        std::string val = trim_str(line.substr(eq_pos + 1));

        size_t c_pos = val.find('#');
        if (c_pos != std::string::npos) {
            val = trim_str(val.substr(0, c_pos));
        }

        if (key == "width") {
            try { config.width = std::max(200, std::stoi(val)); } catch (...) {}
        } else if (key == "height") {
            try { config.height = std::max(150, std::stoi(val)); } catch (...) {}
        } else if (key == "cell_size") {
            try { config.cell_size = std::max(30, std::stoi(val)); } catch (...) {}
        } else if (key == "spacing") {
            try { config.spacing = std::max(0, std::stoi(val)); } catch (...) {}
        } else if (key == "edge_padding" || key == "padding") {
            try { config.edge_padding = std::clamp(std::stoi(val), 0, 80); } catch (...) {}
        } else if (key == "show_subtitles") {
            std::string lower_val = val;
            std::transform(lower_val.begin(), lower_val.end(), lower_val.begin(), ::tolower);
            config.show_subtitles = (lower_val == "true" || lower_val == "1" || lower_val == "yes" || lower_val == "on");
        } else if (key == "selection_border") {
            std::string lower_val = val;
            std::transform(lower_val.begin(), lower_val.end(), lower_val.begin(), ::tolower);
            config.selection_border = (lower_val == "true" || lower_val == "1" || lower_val == "yes" || lower_val == "on");
        } else if (key == "search_focus_border") {
            std::string lower_val = val;
            std::transform(lower_val.begin(), lower_val.end(), lower_val.begin(), ::tolower);
            config.search_focus_border = (lower_val == "true" || lower_val == "1" || lower_val == "yes" || lower_val == "on");
        } else if (key == "search_width_percent") {
            try { config.search_width_percent = std::clamp(std::stoi(val), 20, 100); } catch (...) {}
        } else if (key == "search_height") {
            try { config.search_height = std::clamp(std::stoi(val), 28, 64); } catch (...) {}
        } else if (key == "search_elevation") {
            try { config.search_elevation = std::clamp(std::stoi(val), 0, 5); } catch (...) {}
        } else if (key == "view_mode") {
            std::string lower_val = val;
            std::transform(lower_val.begin(), lower_val.end(), lower_val.begin(), ::tolower);
            if (lower_val == "list" || lower_val == "grid") {
                config.view_mode = lower_val;
            }
        } else if (key == "search_view_mode") {
            std::string lower_val = val;
            std::transform(lower_val.begin(), lower_val.end(), lower_val.begin(), ::tolower);
            if (lower_val == "list" || lower_val == "grid" || lower_val == "inherit") {
                config.search_view_mode = lower_val;
            }
        } else if (key == "icon_theme") {
            config.icon_theme = val;
            Config::get()->metrics.icon_theme = val;
            Config::get()->notify_changed();
        }
    }
}

} // namespace miqu
