#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <chrono>

namespace miqunotify {

enum class Urgency {
    Low = 0,
    Normal = 1,
    Critical = 2
};

struct NotificationAction {
    std::string key;
    std::string label;
};

struct Notification {
    uint32_t id = 0;
    std::string app_name;
    uint32_t replaces_id = 0;
    std::string app_icon;
    std::string summary;
    std::string body;
    std::vector<NotificationAction> actions;
    Urgency urgency = Urgency::Normal;
    int32_t expire_timeout_ms = -1; // -1 = server default, 0 = never, >0 = ms
    int64_t timestamp_sec = 0;
    bool is_read = false;

    Notification() {
        auto now = std::chrono::system_clock::now();
        timestamp_sec = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    }
};

} // namespace miqunotify
