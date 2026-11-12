#pragma once

#include <string>
#include <vector>
#include <memory>

namespace miqu {

struct KeyboardDevice {
    std::string name;
    std::string type = "keyboard";
    std::string layout = "English (US)";
    int repeat_rate = 25;
    int repeat_delay = 600;
    bool is_primary = false;
};

struct PointerDevice {
    std::string name;
    std::string type = "pointer";
    std::string display_type = "Pointing Device";
    bool is_touchpad = false;
    bool is_trackpoint = false;
};

struct TouchDevice {
    std::string name;
    std::string type = "touch";
    std::string mapped_output = "eDP-1";
};

struct InputConfigState {
    // Hardware introspection (from miquctl -j devices)
    std::vector<KeyboardDevice> keyboards;
    std::vector<PointerDevice> pointers;
    std::vector<TouchDevice> touch_devices;

    // Active Keyboard Settings (baseline from miquland.conf, dynamic in-memory)
    std::string active_layout = "us";
    int repeat_rate = 25;
    int repeat_delay = 600;

    // Active Touchpad Settings
    bool tap_to_click = true;
    bool natural_scroll = false;
    bool disable_while_typing = true;
    float pointer_speed = 0.5f; // 0.0 to 1.0 (50% default)
    std::string accel_profile = "adaptive";

    // Gestures
    bool workspace_swipe = true;
    float gesture_multiplier = 1.0f;
};

class InputBackend {
public:
    static InputConfigState get_state();

    // In-memory runtime setters (reboot persistence deferred)
    static void set_keyboard_layout(const std::string& layout);
    static void set_repeat_info(int rate, int delay);
    static void set_touchpad_options(bool tap_to_click, bool natural_scroll, bool disable_while_typing);
    static void set_pointer_speed(float speed);
    static void set_accel_profile(const std::string& profile);
    static void set_workspace_swipe(bool enabled, float multiplier);
};

} // namespace miqu
