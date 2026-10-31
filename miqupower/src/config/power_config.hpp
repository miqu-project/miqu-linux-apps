#pragma once

#include <string>
#include <cstdint>

namespace miqu {

struct PowerConfig {
    int width = 720;
    int height = 460;

    bool dim_backdrop = true;
    bool close_on_click_outside = false;
    bool close_on_escape = true;

    bool warn_running_apps = true;
    int max_running_apps = 6;
    uint32_t graceful_timeout_ms = 2500;
    bool lock_before_suspend = true;

    bool enable_shutdown = true;
    bool enable_reboot = true;
    bool enable_suspend = true;
    bool enable_lock = true;
    bool enable_logout = true;
    bool enable_hibernate = false;

    std::string lock_cmd;
    std::string logout_cmd;
    std::string shutdown_cmd;
    std::string reboot_cmd;
    std::string suspend_cmd;
    std::string hibernate_cmd;

    static PowerConfig& get();
    void load(const std::string& custom_path = "");
};

} // namespace miqu
