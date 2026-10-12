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
    int height = 48;
    int exclusive_zone = 48;
    float opacity = 0.96f;

    // Start & Taskbar
    bool show_start = true;
    std::string launcher_cmd = "miqulauncher";
    std::string start_icon = "miquland";
    int icon_size = 28;

    // Workspaces
    bool show_workspace_btn = true;
    std::string workspace_display = "icon"; // "icon", "dots", or "badge"
    bool hover_flyout = true;
    bool scroll_switch = true;

    // Search
    bool show_search = true;
    std::string search_style = "pill"; // "pill" or "icon"
    std::string search_cmd = "miqulauncher";
    int search_width = 160;
    std::string search_placeholder = "Search";

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
    bool show_cpu = false;
    bool show_memory = false;
    bool show_network = true;
    bool show_bluetooth = true;
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


private:
    BarConfig();
};

} // namespace miqubar
