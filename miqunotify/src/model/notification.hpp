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
    bool transient = false;

    Notification() {
        auto now = std::chrono::system_clock::now();
        timestamp_sec = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    }

    std::string to_json() const {
        auto escape = [](const std::string& s) -> std::string {
            std::string out;
            out.reserve(s.size() + 8);
            for (char c : s) {
                switch (c) {
                    case '"': out += "\\\""; break;
                    case '\\': out += "\\\\"; break;
                    case '\b': out += "\\b"; break;
                    case '\f': out += "\\f"; break;
                    case '\n': out += "\\n"; break;
                    case '\r': out += "\\r"; break;
                    case '\t': out += "\\t"; break;
                    default:
                        if (static_cast<unsigned char>(c) < 0x20) {
                            char buf[8];
                            snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                            out += buf;
                        } else {
                            out += c;
                        }
                        break;
                }
            }
            return out;
        };

        std::string json = "{";
        json += "\"id\":" + std::to_string(id) + ",";
        json += "\"app_name\":\"" + escape(app_name) + "\",";
        json += "\"app_icon\":\"" + escape(app_icon) + "\",";
        json += "\"summary\":\"" + escape(summary) + "\",";
        json += "\"body\":\"" + escape(body) + "\",";
        std::string urg_str = (urgency == Urgency::Critical) ? "critical" : (urgency == Urgency::Low ? "low" : "normal");
        json += "\"urgency\":\"" + urg_str + "\",";
        json += "\"timestamp_sec\":" + std::to_string(timestamp_sec) + ",";
        json += "\"is_read\":" + std::string(is_read ? "true" : "false") + ",";
        json += "\"transient\":" + std::string(transient ? "true" : "false") + ",";
        json += "\"actions\":[";
        for (size_t i = 0; i < actions.size(); ++i) {
            if (i > 0) json += ",";
            json += "{\"key\":\"" + escape(actions[i].key) + "\",\"label\":\"" + escape(actions[i].label) + "\"}";
        }
        json += "]}";
        return json;
    }
};

} // namespace miqunotify
