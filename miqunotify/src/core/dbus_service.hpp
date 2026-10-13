#pragma once

#include "model/notification.hpp"
#include <systemd/sd-bus.h>
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <memory>

namespace miqunotify {

class DBusService {
public:
    using OnNotifyCallback = std::function<uint32_t(Notification)>;
    using OnCloseCallback = std::function<void(uint32_t)>;
    using OnControlCallback = std::function<void(const std::string& action)>;
    using OnGetHistoryCallback = std::function<std::string()>;

    DBusService();
    ~DBusService();

    bool start(
        OnNotifyCallback on_notify,
        OnCloseCallback on_close,
        OnControlCallback on_control,
        OnGetHistoryCallback on_get_history = nullptr
    );
    void stop();

    void emit_notification_closed(uint32_t id, uint32_t reason);
    void emit_action_invoked(uint32_t id, const std::string& action_key);

    static bool send_control_command(const std::string& action);
    static bool fetch_history_json(std::string& out_json);

    // D-Bus method callbacks (used by sd-bus vtables)
    static int method_notify(sd_bus_message* m, void* userdata, sd_bus_error* ret_error);
    static int method_close_notification(sd_bus_message* m, void* userdata, sd_bus_error* ret_error);
    static int method_get_capabilities(sd_bus_message* m, void* userdata, sd_bus_error* ret_error);
    static int method_get_server_information(sd_bus_message* m, void* userdata, sd_bus_error* ret_error);
    static int method_control(sd_bus_message* m, void* userdata, sd_bus_error* ret_error);
    static int method_get_history(sd_bus_message* m, void* userdata, sd_bus_error* ret_error);

private:
    void run_worker();

    sd_bus* m_bus = nullptr;
    sd_bus_slot* m_slot_notifications = nullptr;
    sd_bus_slot* m_slot_control = nullptr;

    std::thread m_worker;
    std::atomic<bool> m_running{false};

    OnNotifyCallback m_on_notify;
    OnCloseCallback m_on_close;
    OnControlCallback m_on_control;
    OnGetHistoryCallback m_on_get_history;
};

} // namespace miqunotify
