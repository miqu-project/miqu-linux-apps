#pragma once

#include <string>

namespace miqunotify {

struct NotifyConfig {
    // Section 1: Application Options
    std::string position = "top-right"; // top-right, top-left, bottom-right, bottom-left
    int timeout_ms = 5000;
    int max_toasts = 5;
    int toast_width = 380;
    int toast_spacing = 10;
    int margin_x = 16;
    int margin_y = 16;
    bool dnd_enabled = false;
    int history_limit = 100;
    int center_width = 400;
    bool debug = false;
    bool group_by_app = true;

    static NotifyConfig& get();
    void sync_defaults_from_toolkit();
    void load(const std::string& custom_path = "");

private:
    NotifyConfig();
};

} // namespace miqunotify
