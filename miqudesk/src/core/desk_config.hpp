#pragma once

#include <miqutoolkit/miqutoolkit.hpp>
#include <string>

namespace miqudesk {

struct DeskConfig {
    // Typography
    std::string font_family = "Sans";
    int font_size = 11;
    bool font_bold = false;
    bool shortcut_bold = false;
    bool clock_bold = false;
    miqu::Color font_color;          // Synced from toolkit: colors.on_surface
    miqu::Color font_color_muted;    // Synced from toolkit: colors.on_surface_variant
    miqu::Color accent_color;        // Synced from toolkit: colors.primary

    // Widget Appearance (synced from toolkit, overridable via user config)
    miqu::Color widget_background;   // Synced from toolkit: colors.surface
    miqu::Color widget_border_color; // Synced from toolkit: colors.outline
    int widget_border_width = 1;     // Synced from toolkit: metrics.border_width
    int widget_corner_radius = 20;   // Synced from toolkit: metrics.corner_radius

    // Desktop / App Shortcuts
    int desktop_max_chars = 14;      // Max characters in .desktop names before truncating (0 to disable)
    int shortcut_width = 84;         // Overall width
    int shortcut_height = 92;        // Overall height
    int icon_size = 48;              // Target icon size
    miqu::Color shortcut_background = miqu::Color::rgba(0.0f, 0.0f, 0.0f, 0.0f);
    miqu::Color shortcut_border_color = miqu::Color::rgba(0.0f, 0.0f, 0.0f, 0.0f);
    int shortcut_corner_radius = 12;
    bool double_click_to_launch = true;
    int double_click_time_ms = 400;

    // Desktop Shortcuts Grid Layout
    int shortcuts_start_x = 480;
    int shortcuts_start_y = 80;
    int col_spacing = 96;
    int row_spacing = 104;
    int items_per_col = 5;

    // General & Canvas
    int grid_snap = 10;
    bool edit_mode = false;

    // Clock Widget
    bool clock_enabled = true;
    bool clock_24h = true;
    bool clock_show_seconds = true;
    int clock_x = 780;
    int clock_y = 460;
    int clock_width = 360;
    int clock_height = 145;

    // System Widget
    bool system_enabled = false;
    int system_x = 80;
    int system_y = 245;
    int system_width = 360;
    int system_height = 135;

    // Active path and watched directories for hot reload
    std::string active_config_path;
    std::vector<std::string> loaded_files;
    std::vector<std::string> watched_dirs;
    const std::vector<std::string>& get_watched_dirs() const { return watched_dirs; }

    // Singleton & loader
    DeskConfig();
    static DeskConfig& get();
    void sync_defaults_from_toolkit();
    void load(const std::string& custom_path = "");
    void load_file_internal(const std::string& path, int depth = 0);
    static std::string get_user_config_path();
    static std::string get_system_config_path();
};

} // namespace miqudesk
