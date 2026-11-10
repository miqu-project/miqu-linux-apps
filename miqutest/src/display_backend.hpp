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

    // Gamma / Night Light (via native miquland / miquctl gamma)
    static bool is_available();
    static bool is_night_light_active();
    static int get_gamma_temperature();
    static void set_night_light_enabled(bool enabled, int temp_k = 4500);
    static void set_gamma_temperature(int temp_k);
    static void sync_nightlight_state(const std::shared_ptr<SettingItem>& item);

    // Warmth / Kelvin conversion helpers
    static int kelvin_from_warmth(float warmth);
    static float warmth_from_kelvin(int kelvin);
};

} // namespace miqu
