#pragma once

#include <string>
#include <vector>

namespace miqu {

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
    bool enabled = true;
    bool is_primary = true;
    int pos_x = 0;
    int pos_y = 0;
    int current_width = 1920;
    int current_height = 1080;
    float current_refresh = 60.0f;
    float scale = 1.0f;
    std::string transform = "normal"; // "normal", "90", "180", "270"
    std::vector<MonitorMode> modes;
};

class MonitorBackend {
public:
    static std::vector<MonitorInfo> get_monitors();
    static MonitorInfo get_monitor(const std::string& name);
    static bool apply_settings(const std::string& name, int width, int height, float refresh, float scale, const std::string& transform);
};

} // namespace miqu
