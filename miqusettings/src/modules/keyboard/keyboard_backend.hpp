#pragma once

#include <string>
#include <vector>

namespace miqu {

struct KeyboardDeviceInfo {
    std::string name;
    std::string type = "keyboard";
    std::string layout = "us";
    int repeat_rate = 25;
    int repeat_delay = 600;
};

struct PointerDeviceInfo {
    std::string name;
    std::string type = "pointer";
};

struct TouchDeviceInfo {
    std::string name;
    std::string type = "touch";
};

struct InputDevicesInfo {
    std::vector<KeyboardDeviceInfo> keyboards;
    std::vector<PointerDeviceInfo> pointers;
    std::vector<TouchDeviceInfo> touch_devices;
};

class KeyboardBackend {
public:
    static InputDevicesInfo get_devices_info();
    static void set_repeat_info(int rate, int delay);
    static void set_keyboard_layout(const std::string& layout);
    static std::string run_miquctl(const std::string& args);
};

} // namespace miqu
