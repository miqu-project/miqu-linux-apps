#pragma once

#include <string>
#include <vector>
#include <memory>
#include <utility>

namespace miqu {

class SettingItem;

struct MonitorMode {
    int width = 1920;
    int height = 1080;
    float refresh_rate = 60.0f;
    bool is_preferred = false;
    bool is_current = false;
};

struct MonitorInfo {
    std::string name = "eDP-1";
    std::string description = "Built-in Display";
    std::string make;
    std::string model;
    int physical_width_mm = 0;
    int physical_height_mm = 0;
    bool enabled = true;
    bool is_primary = true;
    int pos_x = 0;
    int pos_y = 0;
    int current_width = 1920;
    int current_height = 1080;
    float current_refresh = 60.0f;
    float scale = 1.0f;
    std::string transform = "normal"; // "normal", "90", "180", "270"
    bool adaptive_sync = false;
    std::vector<MonitorMode> modes;
};

class MonitorBackend {
public:
    static std::vector<MonitorInfo> get_monitors();
    static MonitorInfo get_monitor(const std::string& name);

    // Dynamic runtime output adjustments via wlr-randr (never writes to miquland.conf)
    static bool set_mode(const std::string& name, int width, int height, float refresh = 0.0f);
    static bool set_scale(const std::string& name, float scale);
    static bool set_transform(const std::string& name, const std::string& transform);
    static bool set_primary(const std::string& name);
    static bool apply_settings(const std::string& name, int width, int height, float refresh, float scale, const std::string& transform);

    // Resolution, refresh, scale and orientation converters
    static std::pair<int, int> parse_resolution(const std::string& res_str);
    static float parse_refresh_rate(const std::string& hz_str);
    static float parse_scale(const std::string& scale_str);
    static std::string transform_to_ui(const std::string& transform);
    static std::string ui_to_transform(const std::string& ui_str);
    static std::vector<std::string> get_available_refresh_rates(const std::string& name, int width, int height);

    // Model synchronization
    static void sync_monitor_item(const std::shared_ptr<SettingItem>& item, const MonitorInfo& info);
};

} // namespace miqu
