#pragma once

#include "miqutoolkit/core/color.hpp"
#include <string>

namespace miqubar {

class BarConfig {
public:
    static BarConfig& get();

    void load(const std::string& custom_path = "");
    void sync_defaults_from_toolkit();

    // General
    std::string position = "bottom";
    int height = 42;
    int exclusive_zone = 42;
    float opacity = 0.96f;

    // Start
    bool show_start = true;
    std::string launcher_cmd = "miqulauncher";
    int icon_size = 22;

    // Workspaces
    bool show_workspace_btn = true;
    std::string workspace_display = "badge"; // "badge" or "strip"
    bool hover_flyout = true;
    bool scroll_switch = true;

    // Taskbar
    bool show_taskbar = true;
    bool icon_only = true;
    int max_title_chars = 24;
    int item_max_width = 200;
    bool show_active_underline = true;
    bool middle_click_close = true;
    bool show_hover_popup = true;
    int hover_popup_max_width = 320;
    int hover_delay_ms = 300;

    // Modules
    bool show_cpu = true;
    bool show_memory = true;
    bool show_volume = true;
    bool show_battery = true;
    bool show_clock = true;
    bool show_peek = true;
    bool clock_stacked = true;
    std::string time_format = "%H:%M";
    std::string date_format = "%d/%m/%Y";
    int telemetry_interval_ms = 2000;

    // Appearance tokens (synced from miqutoolkit)
    std::string font_family = "Sans";
    int font_size = 11;
    int corner_radius = 8;
    int border_width = 1;

    miqu::Color color_bg{19, 23, 34, 245};
    miqu::Color color_surface{26, 32, 48, 255};
    miqu::Color color_surface_hover{35, 44, 66, 255};
    miqu::Color color_text{237, 242, 247, 255};
    miqu::Color color_text_muted{140, 150, 168, 255};
    miqu::Color color_accent{255, 87, 120, 255};
    miqu::Color color_border{255, 255, 255, 20};

private:
    BarConfig();
};

} // namespace miqubar
