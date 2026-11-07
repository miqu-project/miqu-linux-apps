#pragma once

#include <string>
#include <cstdint>

namespace miqu {

struct SettingsConfig {
    int window_width = 1150;
    int window_height = 740;

    static SettingsConfig& get();
    void sync_defaults_from_toolkit();
    void load(const std::string& custom_path = "");
};

} // namespace miqu
