#include "bar_config.hpp"
#include "miqutoolkit/core/config.hpp"
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
    if (tk->metrics.corner_radius >= 0) corner_radius = tk->metrics.corner_radius;
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
    sync_defaults_from_toolkit();

    std::string path_to_load = custom_path;
    if (path_to_load.empty()) {
        const char* xdg_config = getenv("XDG_CONFIG_HOME");
        std::string user_cfg;
        if (xdg_config && xdg_config[0] != '\0') {
            user_cfg = std::string(xdg_config) + "/miqubar/miqubar.conf";
        } else {
            const char* home = getenv("HOME");
            if (home) user_cfg = std::string(home) + "/.config/miqubar/miqubar.conf";
        }
        if (!user_cfg.empty() && fs::exists(user_cfg)) {
            path_to_load = user_cfg;
        } else if (fs::exists("/usr/share/miqubar/miqubar.conf")) {
            path_to_load = "/usr/share/miqubar/miqubar.conf";
        }
    }

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
        else if (key == "exclusive_zone") exclusive_zone = std::stoi(val);
        else if (key == "opacity") opacity = std::clamp(std::stof(val), 0.0f, 1.0f);
        else if (key == "show_start") show_start = parse_bool(val);
        else if (key == "launcher_cmd") launcher_cmd = val;
        else if (key == "icon_size") icon_size = std::stoi(val);
        else if (key == "show_workspace_btn") show_workspace_btn = parse_bool(val);
        else if (key == "workspace_display") workspace_display = val;
        else if (key == "hover_flyout") hover_flyout = parse_bool(val);
        else if (key == "scroll_switch") scroll_switch = parse_bool(val);
        else if (key == "show_taskbar") show_taskbar = parse_bool(val);
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
        else if (key == "show_volume") show_volume = parse_bool(val);
        else if (key == "show_battery") show_battery = parse_bool(val);
        else if (key == "show_clock") show_clock = parse_bool(val);
        else if (key == "show_peek") show_peek = parse_bool(val);
        else if (key == "clock_stacked") clock_stacked = parse_bool(val);
        else if (key == "time_format") time_format = val;
        else if (key == "date_format") date_format = val;
        else if (key == "telemetry_interval_ms") telemetry_interval_ms = std::max(500, std::stoi(val));
    }
}

} // namespace miqubar
