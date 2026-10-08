#include "desk_config.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cstdlib>

namespace miqudesk {

namespace fs = std::filesystem;

static DeskConfig s_desk_config;

DeskConfig::DeskConfig() {
    sync_defaults_from_toolkit();
}

void DeskConfig::sync_defaults_from_toolkit() {
    auto tk = miqu::Config::get();
    if (!tk) return;

    if (!tk->metrics.font_family.empty()) font_family = tk->metrics.font_family;
    if (tk->metrics.font_size > 0) font_size = tk->metrics.font_size;
    if (tk->metrics.border_width >= 0) widget_border_width = tk->metrics.border_width;
    if (tk->metrics.corner_radius >= 0) widget_corner_radius = tk->metrics.corner_radius;

    font_color = tk->colors.on_surface;
    font_color_muted = tk->colors.on_surface_variant;
    accent_color = tk->colors.primary;
    widget_background = tk->colors.surface;
    widget_border_color = tk->colors.outline;
}

DeskConfig& DeskConfig::get() {
    return s_desk_config;
}

static std::string trim_str(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n\"'");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n\"'");
    return str.substr(first, (last - first + 1));
}

static std::string expand_home(const std::string& path) {
    if (path.empty()) return path;
    if (path[0] == '~') {
        const char* home = getenv("HOME");
        if (home) {
            return std::string(home) + path.substr(1);
        }
    }
    return path;
}

static bool parse_bool(const std::string& val) {
    std::string s = val;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return (s == "true" || s == "1" || s == "yes" || s == "on");
}

static miqu::Color parse_color_value(const std::string& input, const miqu::Color& fallback) {
    std::string s = input;
    size_t first = s.find_first_not_of(" \t\r\n\"';");
    if (first == std::string::npos) return fallback;
    size_t last = s.find_last_not_of(" \t\r\n\"';");
    s = s.substr(first, last - first + 1);

    // Support alpha(color, alpha_val) e.g. alpha(#ffffff, 0.25)
    if (s.rfind("alpha(", 0) == 0 && s.back() == ')') {
        std::string inner = s.substr(6, s.size() - 7);
        auto comma = inner.find(',');
        if (comma != std::string::npos) {
            std::string c_part = inner.substr(0, comma);
            std::string a_part = inner.substr(comma + 1);
            c_part = trim_str(c_part);
            a_part = trim_str(a_part);
            try {
                float a = std::stof(a_part);
                miqu::Color base = miqu::Color::from_hex(c_part, fallback);
                return base.with_alpha(a);
            } catch (...) {}
        }
    }

    return miqu::Color::from_hex(s, fallback);
}

std::string DeskConfig::get_user_config_path() {
    std::string user_cfg_dir = miqu::FsUtils::get_user_config_dir("miqudesk");
    if (!user_cfg_dir.empty()) {
        std::string p = user_cfg_dir + "/miqudesk.conf";
        if (fs::exists(p)) {
            return p;
        }
    }
    return "";
}

std::string DeskConfig::get_system_config_path() {
    return "/usr/share/miqudesk/miqudesk.conf";
}

void DeskConfig::load_file_internal(const std::string& raw_path, int depth) {
    if (depth > 10) return;

    std::string expanded = expand_home(raw_path);
    if (!fs::exists(expanded)) return;

    try {
        std::string canonical = fs::canonical(expanded).string();
        if (std::find(loaded_files.begin(), loaded_files.end(), canonical) == loaded_files.end()) {
            loaded_files.push_back(canonical);
        }
    } catch (...) {
        if (std::find(loaded_files.begin(), loaded_files.end(), expanded) == loaded_files.end()) {
            loaded_files.push_back(expanded);
        }
    }

    fs::path parent_dir = fs::path(expanded).parent_path();
    std::string parent_str = parent_dir.string();
    if (!parent_str.empty() && std::find(watched_dirs.begin(), watched_dirs.end(), parent_str) == watched_dirs.end()) {
        watched_dirs.push_back(parent_str);
    }

    std::ifstream file(expanded);
    if (!file.is_open()) return;

    std::string line;
    std::string current_section;

    while (std::getline(file, line)) {
        line = trim_str(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;

        // Check for space-separated include / source / @import (e.g. "include foo.conf")
        std::string inc_file;
        if (line.rfind("include ", 0) == 0) {
            inc_file = trim_str(line.substr(8));
        } else if (line.rfind("source ", 0) == 0) {
            inc_file = trim_str(line.substr(7));
        } else if (line.rfind("@import ", 0) == 0) {
            inc_file = trim_str(line.substr(8));
            if (!inc_file.empty() && inc_file.back() == ';') inc_file.pop_back();
            inc_file = trim_str(inc_file);
        }

        // Section header [section]
        if (line.front() == '[' && line.back() == ']') {
            current_section = trim_str(line.substr(1, line.size() - 2));
            std::transform(current_section.begin(), current_section.end(), current_section.begin(), ::tolower);
            continue;
        }

        std::string key, val;
        auto eq = line.find('=');
        if (eq != std::string::npos) {
            key = trim_str(line.substr(0, eq));
            val = trim_str(line.substr(eq + 1));
            std::string lower_key = key;
            std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(), ::tolower);

            if (lower_key == "include" || lower_key == "source" || lower_key == "@import") {
                inc_file = val;
            }
        }

        // Handle include / source file inclusion
        if (!inc_file.empty()) {
            size_t c_pos = inc_file.find('#');
            if (c_pos != std::string::npos) {
                inc_file = trim_str(inc_file.substr(0, c_pos));
            }
            std::string resolved = expand_home(inc_file);
            if (!fs::path(resolved).is_absolute()) {
                resolved = (parent_dir / inc_file).string();
            }
            if (fs::exists(resolved)) {
                std::cout << "[miqudesk] Including config from: " << resolved << std::endl;
                load_file_internal(resolved, depth + 1);
            }
            continue;
        }

        if (eq == std::string::npos) continue;

        // Strip comments in value (preceded by whitespace)
        size_t c_pos = std::string::npos;
        for (size_t i = 1; i < val.size(); ++i) {
            if (val[i] == '#' && (val[i - 1] == ' ' || val[i - 1] == '\t')) {
                c_pos = i;
                break;
            }
        }
        if (c_pos != std::string::npos) {
            val = trim_str(val.substr(0, c_pos));
        }

        std::string lower_key = key;
        std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(), ::tolower);

        // Section: [general] or typography/appearance
        if (lower_key == "font_family" || lower_key == "font") {
            font_family = val;
        } else if (lower_key == "font_size") {
            try { font_size = std::max(6, std::stoi(val)); } catch (...) {}
        } else if (lower_key == "font_bold" || lower_key == "bold") {
            font_bold = parse_bool(val);
        } else if (lower_key == "font_color" || lower_key == "text_color" || lower_key == "color" || lower_key == "text" || lower_key == "on_surface" || lower_key == "fg") {
            font_color = parse_color_value(val, font_color);
        } else if (lower_key == "font_color_muted" || lower_key == "text_muted" || lower_key == "muted_color" || lower_key == "on_surface_variant" || lower_key == "muted") {
            font_color_muted = parse_color_value(val, font_color_muted);
        } else if (lower_key == "accent_color" || lower_key == "primary" || lower_key == "accent" || lower_key == "color_primary") {
            accent_color = parse_color_value(val, accent_color);
        } else if (lower_key == "widget_background" || lower_key == "widget_bg_color" || lower_key == "background" || lower_key == "surface" || lower_key == "bg") {
            widget_background = parse_color_value(val, widget_background);
        } else if (lower_key == "widget_border_color" || lower_key == "border_color" || lower_key == "outline" || lower_key == "border") {
            widget_border_color = parse_color_value(val, widget_border_color);
        } else if (lower_key == "widget_border_width" || lower_key == "border_width") {
            try { widget_border_width = std::max(0, std::stoi(val)); } catch (...) {}
        } else if (lower_key == "widget_corner_radius" || lower_key == "corner_radius" || lower_key == "rounding") {
            try { widget_corner_radius = std::max(0, std::stoi(val)); } catch (...) {}
        } else if (lower_key == "grid_snap") {
            try { grid_snap = std::max(1, std::stoi(val)); } catch (...) {}
        } else if (lower_key == "edit_mode") {
            edit_mode = parse_bool(val);
        }

        // Desktop / Shortcuts settings
        else if (lower_key == "shortcut_bold") {
            shortcut_bold = parse_bool(val);
        } else if (lower_key == "desktop_max_chars" || lower_key == "max_characters" || lower_key == "max_chars") {
            try { desktop_max_chars = std::max(0, std::stoi(val)); } catch (...) {}
        } else if (lower_key == "shortcut_width") {
            try { shortcut_width = std::max(40, std::stoi(val)); } catch (...) {}
        } else if (lower_key == "shortcut_height") {
            try { shortcut_height = std::max(40, std::stoi(val)); } catch (...) {}
        } else if (lower_key == "overall_size" || lower_key == "shortcut_size") {
            try {
                int s = std::max(40, std::stoi(val));
                shortcut_width = s;
                shortcut_height = s + 8;
            } catch (...) {}
        } else if (lower_key == "icon_size") {
            try { icon_size = std::max(16, std::stoi(val)); } catch (...) {}
        } else if (lower_key == "shortcut_background" || lower_key == "shortcut_bg_color") {
            shortcut_background = parse_color_value(val, shortcut_background);
        } else if (lower_key == "shortcut_border_color") {
            shortcut_border_color = parse_color_value(val, shortcut_border_color);
        } else if (lower_key == "shortcut_corner_radius" || lower_key == "shortcut_border_radius") {
            try { shortcut_corner_radius = std::max(0, std::stoi(val)); } catch (...) {}
        } else if (lower_key == "double_click_to_launch" || lower_key == "double_click") {
            double_click_to_launch = parse_bool(val);
        } else if (lower_key == "double_click_time_ms" || lower_key == "double_click_interval") {
            try { double_click_time_ms = std::max(50, std::stoi(val)); } catch (...) {}
        } else if (lower_key == "shortcuts_start_x" || (current_section == "desktop" && lower_key == "start_x")) {
            try { shortcuts_start_x = std::stoi(val); } catch (...) {}
        } else if (lower_key == "shortcuts_start_y" || (current_section == "desktop" && lower_key == "start_y")) {
            try { shortcuts_start_y = std::stoi(val); } catch (...) {}
        } else if (lower_key == "col_spacing") {
            try { col_spacing = std::max(10, std::stoi(val)); } catch (...) {}
        } else if (lower_key == "row_spacing") {
            try { row_spacing = std::max(10, std::stoi(val)); } catch (...) {}
        } else if (lower_key == "items_per_col") {
            try { items_per_col = std::max(1, std::stoi(val)); } catch (...) {}
        }

        // Clock Widget settings
        else if (current_section == "clock" || lower_key.rfind("clock_", 0) == 0) {
            std::string subkey = (current_section == "clock") ? lower_key : lower_key.substr(6);
            if (subkey == "bold") clock_bold = parse_bool(val);
            else if (subkey == "enabled") clock_enabled = parse_bool(val);
            else if (subkey == "format_24h" || subkey == "24h") clock_24h = parse_bool(val);
            else if (subkey == "show_seconds") clock_show_seconds = parse_bool(val);
            else if (subkey == "x") { try { clock_x = std::stoi(val); } catch (...) {} }
            else if (subkey == "y") { try { clock_y = std::stoi(val); } catch (...) {} }
            else if (subkey == "width") { try { clock_width = std::max(100, std::stoi(val)); } catch (...) {} }
            else if (subkey == "height") { try { clock_height = std::max(50, std::stoi(val)); } catch (...) {} }
        }

        // System Widget settings
        else if (current_section == "system" || lower_key.rfind("system_", 0) == 0) {
            std::string subkey = (current_section == "system") ? lower_key : lower_key.substr(7);
            if (subkey == "enabled") system_enabled = parse_bool(val);
            else if (subkey == "x") { try { system_x = std::stoi(val); } catch (...) {} }
            else if (subkey == "y") { try { system_y = std::stoi(val); } catch (...) {} }
            else if (subkey == "width") { try { system_width = std::max(100, std::stoi(val)); } catch (...) {} }
            else if (subkey == "height") { try { system_height = std::max(50, std::stoi(val)); } catch (...) {} }
        }
    }
}

void DeskConfig::load(const std::string& custom_path) {
    if (!custom_path.empty()) {
        active_config_path = custom_path;
    }

    // Always reset tracking state
    loaded_files.clear();
    watched_dirs.clear();

    // Always watch user config dir and Desktop dir if they exist
    std::string user_cfg_dir = expand_home("~/.config/miqudesk");
    if (fs::exists(user_cfg_dir)) watched_dirs.push_back(user_cfg_dir);
    std::string desktop_dir = expand_home("~/Desktop");
    if (fs::exists(desktop_dir)) watched_dirs.push_back(desktop_dir);

    // 1. Synchronize with toolkit theme (always, even without a config file)
    sync_defaults_from_toolkit();

    // 2. Resolve config file path
    std::string target_path;
    if (!active_config_path.empty() && fs::exists(active_config_path)) {
        target_path = active_config_path;
    } else {
        std::string user_path = get_user_config_path();
        if (!user_path.empty() && fs::exists(user_path)) {
            target_path = user_path;
        }
    }

    // 3. If a config file exists, load it as an overlay on top of toolkit defaults
    if (!target_path.empty()) {
        active_config_path = target_path;
        std::cout << "[miqudesk] Loading config from: " << target_path << std::endl;

        // Feed into toolkit config system so setup_config_watcher() tracks it
        miqu::Config::get()->load_from_file(target_path);

        // Parse app-specific options (grid, shortcuts, clock, system widgets, etc.)
        load_file_internal(target_path, 0);
    } else {
        std::cout << "[miqudesk] No config file found. Using toolkit defaults." << std::endl;
    }
}

} // namespace miqudesk
