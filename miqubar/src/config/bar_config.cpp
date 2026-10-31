#include "bar_config.hpp"
#include "miqutoolkit/core/config.hpp"
#include "miqutoolkit/core/fs_utils.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <filesystem>

namespace miqubar {

namespace fs = std::filesystem;

BarConfig& BarConfig::get() {
    static BarConfig s_instance;
    return s_instance;
}

BarConfig::BarConfig() {
    sync_defaults_from_toolkit();
}

void BarConfig::sync_defaults_from_toolkit() {
    auto tk = miqu::Config::get();
    if (!tk) return;

    if (!tk->metrics.font_family.empty()) font_family = tk->metrics.font_family;
    if (tk->metrics.font_size > 0) font_size = tk->metrics.font_size;
    if (tk->metrics.corner_radius >= 0) {
        corner_radius = tk->metrics.corner_radius;
        if (!custom_bar_corner_radius) {
            bar_corner_radius = corner_radius;
        }
    }
    if (tk->metrics.border_width >= 0) border_width = tk->metrics.border_width;
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

void BarConfig::load(const std::string& custom_path) {
    custom_bar_corner_radius = false;
    miqubar_outline = false;
    reserve_space = true;
    margin_top = 0;
    margin_bottom = 0;
    margin_left = 0;
    margin_right = 0;
    padding_top = 4;
    padding_bottom = 4;
    padding_left = 6;
    padding_right = 6;
    std::string path_to_load = custom_path;
    if (path_to_load.empty()) {
        std::string user_conf = miqu::FsUtils::get_user_config_file("miqubar");
        path_to_load = fs::exists(user_conf) ? user_conf : miqu::FsUtils::get_system_config_file("miqubar");
    }

    if (!path_to_load.empty() && fs::exists(path_to_load)) {
        miqu::Config::get()->load_from_file(path_to_load);
    }

    sync_defaults_from_toolkit();

    if (path_to_load.empty() || !fs::exists(path_to_load)) {
        return;
    }

    std::ifstream file(path_to_load);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') continue;
        if (trimmed[0] == '[' && trimmed.back() == ']') continue;

        size_t eq = trimmed.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(trimmed.substr(0, eq));
        std::string val = trim(trimmed.substr(eq + 1));

        if (key == "position") position = val;
        else if (key == "height") height = std::max(24, std::stoi(val));
        else if (key == "reserve_space") reserve_space = parse_bool(val);
        else if (key == "exclusive_zone") {
            if (val == "0" || val == "-1" || val == "false") reserve_space = false;
        }
        else if (key == "bar_corner_radius") {
            if (!val.empty()) {
                try {
                    bar_corner_radius = std::max(0, std::stoi(val));
                    custom_bar_corner_radius = true;
                } catch (const std::exception& e) {
                    std::cerr << "[miqubar] Invalid bar_corner_radius value: " << val << std::endl;
                }
            }
        }
        else if (key == "miqubar_outline") miqubar_outline = parse_bool(val);
        else if (key == "margin_top") margin_top = std::max(0, std::stoi(val));
        else if (key == "margin_bottom") margin_bottom = std::max(0, std::stoi(val));
        else if (key == "margin_left") margin_left = std::max(0, std::stoi(val));
        else if (key == "margin_right") margin_right = std::max(0, std::stoi(val));
        else if (key == "margin_horizontal") {
            int mh = std::max(0, std::stoi(val));
            margin_left = mh;
            margin_right = mh;
        }
        else if (key == "margin_vertical") {
            int mv = std::max(0, std::stoi(val));
            margin_top = mv;
            margin_bottom = mv;
        }
        else if (key == "padding_top") padding_top = std::max(0, std::stoi(val));
        else if (key == "padding_bottom") padding_bottom = std::max(0, std::stoi(val));
        else if (key == "padding_left") padding_left = std::max(0, std::stoi(val));
        else if (key == "padding_right") padding_right = std::max(0, std::stoi(val));
        else if (key == "padding_horizontal") {
            int ph = std::max(0, std::stoi(val));
            padding_left = ph;
            padding_right = ph;
        }
        else if (key == "padding_vertical") {
            int pv = std::max(0, std::stoi(val));
            padding_top = pv;
            padding_bottom = pv;
        }
        else if (key == "show_start") show_start = parse_bool(val);
        else if (key == "start_icon") start_icon = val;
        else if (key == "launcher_cmd") launcher_cmd = val;
        else if (key == "icon_size") icon_size = std::stoi(val);
        else if (key == "show_workspace_btn") show_workspace_btn = parse_bool(val);
        else if (key == "workspace_display") workspace_display = val;
        else if (key == "hover_flyout") hover_flyout = parse_bool(val);
        else if (key == "scroll_switch") scroll_switch = parse_bool(val);
        else if (key == "show_search") show_search = parse_bool(val);
        else if (key == "search_style") search_style = val;
        else if (key == "search_cmd") search_cmd = val;
        else if (key == "search_width") search_width = std::max(40, std::stoi(val));
        else if (key == "search_placeholder") search_placeholder = val;
        else if (key == "show_taskbar") show_taskbar = parse_bool(val);
        else if (key == "group_apps") group_apps = parse_bool(val);
        else if (key == "icon_only") icon_only = parse_bool(val);
        else if (key == "max_title_chars") max_title_chars = std::stoi(val);
        else if (key == "item_max_width") item_max_width = std::stoi(val);
        else if (key == "show_active_underline") show_active_underline = parse_bool(val);
        else if (key == "middle_click_close") middle_click_close = parse_bool(val);
        else if (key == "show_hover_popup") show_hover_popup = parse_bool(val);
        else if (key == "hover_popup_max_width") hover_popup_max_width = std::max(120, std::stoi(val));
        else if (key == "hover_delay_ms") hover_delay_ms = std::max(50, std::stoi(val));
        else if (key == "show_cpu") show_cpu = parse_bool(val);
        else if (key == "show_memory") show_memory = parse_bool(val);
        else if (key == "show_network") show_network = parse_bool(val);
        else if (key == "show_bluetooth") show_bluetooth = parse_bool(val);
        else if (key == "show_volume") show_volume = parse_bool(val);
        else if (key == "show_brightness") show_brightness = parse_bool(val);
        else if (key == "show_battery") show_battery = parse_bool(val);
        else if (key == "show_dark_mode") show_dark_mode = parse_bool(val);
        else if (key == "show_notifications") show_notifications = parse_bool(val);
        else if (key == "show_clock") show_clock = parse_bool(val);
        else if (key == "show_peek") show_peek = parse_bool(val);
        else if (key == "show_power") show_power = parse_bool(val);
        else if (key == "clock_stacked") clock_stacked = parse_bool(val);
        else if (key == "time_format") time_format = val;
        else if (key == "date_format") date_format = val;
        else if (key == "telemetry_interval_ms") telemetry_interval_ms = std::max(500, std::stoi(val));
    }
}

} // namespace miqubar
