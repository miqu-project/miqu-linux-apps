#pragma once

#include "settings_model.hpp"
#include <string>
#include <memory>

namespace miqu {

struct DisplayBrightnessInfo {
    std::string device_name = "intel_backlight";
    int current_brightness = 50;
    int max_brightness = 100;
    int percent = 50;
    bool available = false;
};

class DisplayBackend {
public:
    static DisplayBrightnessInfo get_brightness_info();
    static int get_brightness_percent();
    static void set_brightness_percent(int percent);
    static void sync_item_state(const std::shared_ptr<SettingItem>& item);
};

} // namespace miqu
