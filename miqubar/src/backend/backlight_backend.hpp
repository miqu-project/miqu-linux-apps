#pragma once

#include <string>

namespace miqubar {

struct BacklightInfo {
    int percent = 50;
    std::string device_path;
};

class BacklightBackend {
public:
    static BacklightInfo get_info();
    static void set_brightness(int percent);
};

} // namespace miqubar
