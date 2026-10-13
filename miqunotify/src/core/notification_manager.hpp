#pragma once

#include "model/notification.hpp"
#include "dbus_service.hpp"
#include "../ui/toast_window.hpp"
#include "../ui/notification_center_window.hpp"
#include <vector>
#include <memory>
#include <string>

namespace miqunotify {

class NotificationManager {
public:
    NotificationManager();
    ~NotificationManager();

    bool init();

    uint32_t on_notify(Notification notif);
    void on_close_notification(uint32_t id);
    void on_control(const std::string& action);

    void clear_all();
    void toggle_dnd();
    void set_dnd(bool enabled);

    void toggle_center();

private:
    void update_state_file();
    std::string get_state_file_path();

    uint32_t m_next_id = 1;
    bool m_dnd_active = false;
    std::vector<Notification> m_history;

    std::unique_ptr<DBusService> m_dbus;
    ToastWindow m_toast_window;
    NotificationCenterWindow m_center_window;
};

} // namespace miqunotify
