#pragma once

#include <string>

namespace miqu {

struct DisplayBrightnessInfo {
    std::string device_name = "intel_backlight";
    int current_brightness = 50;
    int max_brightness = 100;
    int percent = 50;
};

class DisplayBackend {
public:
    // Brightness (via brightnessctl)
    static DisplayBrightnessInfo get_brightness_info();
    static int get_brightness_percent();
    static void set_brightness_percent(int percent);

    // Gamma / Night Light (via native miquland / miquctl gamma)
    static bool is_night_light_active();
    static int get_gamma_temperature();
    static void set_night_light_enabled(bool enabled, int temp_k = 4500);
    static void set_gamma_temperature(int temp_k);

    // Screen Lock (via miqulock)
    static void lock_screen_now();
};

} // namespace miqu
