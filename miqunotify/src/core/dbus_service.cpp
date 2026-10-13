#include "dbus_service.hpp"
#include <iostream>
#include <cstring>

namespace miqunotify {

static const sd_bus_vtable notifications_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Notify", "susssasa{sv}i", "u", DBusService::method_notify, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("CloseNotification", "u", "", DBusService::method_close_notification, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("GetCapabilities", "", "as", DBusService::method_get_capabilities, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("GetServerInformation", "", "ssss", DBusService::method_get_server_information, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_SIGNAL("NotificationClosed", "uu", 0),
    SD_BUS_SIGNAL("ActionInvoked", "us", 0),
    SD_BUS_VTABLE_END
};

static const sd_bus_vtable control_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("ToggleCenter", "", "", DBusService::method_control, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("OpenCenter", "", "", DBusService::method_control, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("CloseCenter", "", "", DBusService::method_control, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("ClearAll", "", "", DBusService::method_control, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("ToggleDnd", "", "", DBusService::method_control, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};

DBusService::DBusService() {}

DBusService::~DBusService() {
    stop();
}

bool DBusService::start(
    OnNotifyCallback on_notify,
    OnCloseCallback on_close,
    OnControlCallback on_control
) {
    m_on_notify = std::move(on_notify);
    m_on_close = std::move(on_close);
    m_on_control = std::move(on_control);

    int r = sd_bus_open_user(&m_bus);
    if (r < 0) {
        std::cerr << "[miqunotify] Failed to connect to user session bus: " << strerror(-r) << std::endl;
        return false;
    }

    r = sd_bus_add_object_vtable(
        m_bus,
        &m_slot_notifications,
        "/org/freedesktop/Notifications",
        "org.freedesktop.Notifications",
        notifications_vtable,
        this
    );
    if (r < 0) {
        std::cerr << "[miqunotify] Failed to add notifications vtable: " << strerror(-r) << std::endl;
        return false;
    }

    r = sd_bus_add_object_vtable(
        m_bus,
        &m_slot_control,
        "/org/freedesktop/Notifications",
        "org.miqu.Notify",
        control_vtable,
        this
    );
    if (r < 0) {
        std::cerr << "[miqunotify] Failed to add control vtable: " << strerror(-r) << std::endl;
    }

    r = sd_bus_request_name(m_bus, "org.freedesktop.Notifications", SD_BUS_NAME_REPLACE_EXISTING | SD_BUS_NAME_ALLOW_REPLACEMENT);
    if (r < 0) {
        std::cerr << "[miqunotify] Failed to acquire service name 'org.freedesktop.Notifications': " << strerror(-r) << std::endl;
        return false;
    }

    m_running = true;
    m_worker = std::thread([this]() {
        run_worker();
    });

    return true;
}

void DBusService::stop() {
    if (!m_running) return;
    m_running = false;

    if (m_bus) {
        sd_bus_close(m_bus);
    }

    if (m_worker.joinable()) {
        m_worker.join();
    }

    if (m_slot_notifications) {
        sd_bus_slot_unref(m_slot_notifications);
        m_slot_notifications = nullptr;
    }
    if (m_slot_control) {
        sd_bus_slot_unref(m_slot_control);
        m_slot_control = nullptr;
    }
    if (m_bus) {
        sd_bus_unref(m_bus);
        m_bus = nullptr;
    }
}

void DBusService::run_worker() {
    while (m_running) {
        int r = sd_bus_process(m_bus, nullptr);
        if (r > 0) continue;
        if (r == 0) {
            r = sd_bus_wait(m_bus, 250000); // 250 ms timeout
            if (r < 0 && r != -EINTR) break;
        } else {
            break;
        }
    }
}

int DBusService::method_notify(sd_bus_message* m, void* userdata, sd_bus_error* ret_error) {
    auto* self = static_cast<DBusService*>(userdata);

    const char* app_name = nullptr;
    uint32_t replaces_id = 0;
    const char* app_icon = nullptr;
    const char* summary = nullptr;
    const char* body = nullptr;
    int32_t expire_timeout = -1;

    int r = sd_bus_message_read(m, "susss", &app_name, &replaces_id, &app_icon, &summary, &body);
    if (r < 0) {
        return sd_bus_error_set_errno(ret_error, -r);
    }

    Notification notif;
    notif.app_name = app_name ? app_name : "";
    notif.replaces_id = replaces_id;
    notif.app_icon = app_icon ? app_icon : "";
    notif.summary = summary ? summary : "";
    notif.body = body ? body : "";

    // Read actions (as)
    r = sd_bus_message_enter_container(m, 'a', "s");
    if (r >= 0) {
        while (true) {
            const char* act_key = nullptr;
            const char* act_label = nullptr;
            int r1 = sd_bus_message_read(m, "s", &act_key);
            if (r1 <= 0) break;
            int r2 = sd_bus_message_read(m, "s", &act_label);
            if (r2 <= 0) break;
            if (act_key && act_label) {
                notif.actions.push_back({act_key, act_label});
            }
        }
        sd_bus_message_exit_container(m);
    }

    // Read hints (a{sv})
    r = sd_bus_message_enter_container(m, 'a', "{sv}");
    if (r >= 0) {
        while (true) {
            r = sd_bus_message_enter_container(m, 'e', "sv");
            if (r <= 0) break;

            const char* key = nullptr;
            r = sd_bus_message_read(m, "s", &key);
            if (r >= 0 && key) {
                if (strcmp(key, "urgency") == 0) {
                    uint8_t urg_byte = 1;
                    int r_urg = sd_bus_message_enter_container(m, 'v', "y");
                    if (r_urg >= 0) {
                        sd_bus_message_read(m, "y", &urg_byte);
                        sd_bus_message_exit_container(m);
                        if (urg_byte == 0) notif.urgency = Urgency::Low;
                        else if (urg_byte == 2) notif.urgency = Urgency::Critical;
                        else notif.urgency = Urgency::Normal;
                    } else {
                        // Sometimes sent as int32
                        int32_t urg_int = 1;
                        r_urg = sd_bus_message_enter_container(m, 'v', "i");
                        if (r_urg >= 0) {
                            sd_bus_message_read(m, "i", &urg_int);
                            sd_bus_message_exit_container(m);
                            if (urg_int == 0) notif.urgency = Urgency::Low;
                            else if (urg_int == 2) notif.urgency = Urgency::Critical;
                            else notif.urgency = Urgency::Normal;
                        } else {
                            sd_bus_message_skip(m, "v");
                        }
                    }
                } else if (strcmp(key, "image-path") == 0 || strcmp(key, "image_path") == 0) {
                    const char* img_p = nullptr;
                    if (sd_bus_message_enter_container(m, 'v', "s") >= 0) {
                        sd_bus_message_read(m, "s", &img_p);
                        sd_bus_message_exit_container(m);
                        if (img_p && notif.app_icon.empty()) notif.app_icon = img_p;
                    } else {
                        sd_bus_message_skip(m, "v");
                    }
                } else {
                    sd_bus_message_skip(m, "v");
                }
            }
            sd_bus_message_exit_container(m);
        }
        sd_bus_message_exit_container(m);
    }

    // Read expire_timeout (i)
    r = sd_bus_message_read(m, "i", &expire_timeout);
    notif.expire_timeout_ms = expire_timeout;

    uint32_t assigned_id = 0;
    if (self && self->m_on_notify) {
        assigned_id = self->m_on_notify(std::move(notif));
    }

    return sd_bus_reply_method_return(m, "u", assigned_id);
}

int DBusService::method_close_notification(sd_bus_message* m, void* userdata, sd_bus_error* ret_error) {
    auto* self = static_cast<DBusService*>(userdata);
    uint32_t id = 0;
    int r = sd_bus_message_read(m, "u", &id);
    if (r < 0) return sd_bus_error_set_errno(ret_error, -r);

    if (self && self->m_on_close) {
        self->m_on_close(id);
    }

    return sd_bus_reply_method_return(m, "");
}

int DBusService::method_get_capabilities(sd_bus_message* m, void* userdata, sd_bus_error* ret_error) {
    (void)userdata;
    (void)ret_error;
    static const char* const caps[] = {
        "actions",
        "body",
        "body-markup",
        "icon-static",
        "persistence"
    };
    return sd_bus_reply_method_return(m, "as", 5, caps[0], caps[1], caps[2], caps[3], caps[4]);
}

int DBusService::method_get_server_information(sd_bus_message* m, void* userdata, sd_bus_error* ret_error) {
    (void)userdata;
    (void)ret_error;
    return sd_bus_reply_method_return(m, "ssss", "miqunotify", "Miqu Project", "1.0.0", "1.2");
}

int DBusService::method_control(sd_bus_message* m, void* userdata, sd_bus_error* ret_error) {
    auto* self = static_cast<DBusService*>(userdata);
    const char* member = sd_bus_message_get_member(m);
    if (self && self->m_on_control && member) {
        self->m_on_control(member);
    }
    return sd_bus_reply_method_return(m, "");
}

void DBusService::emit_notification_closed(uint32_t id, uint32_t reason) {
    if (!m_bus) return;
    sd_bus_emit_signal(
        m_bus,
        "/org/freedesktop/Notifications",
        "org.freedesktop.Notifications",
        "NotificationClosed",
        "uu",
        id,
        reason
    );
}

void DBusService::emit_action_invoked(uint32_t id, const std::string& action_key) {
    if (!m_bus) return;
    sd_bus_emit_signal(
        m_bus,
        "/org/freedesktop/Notifications",
        "org.freedesktop.Notifications",
        "ActionInvoked",
        "us",
        id,
        action_key.c_str()
    );
}

bool DBusService::send_control_command(const std::string& action) {
    sd_bus* bus = nullptr;
    int r = sd_bus_open_user(&bus);
    if (r < 0) return false;

    sd_bus_error err = SD_BUS_ERROR_NULL;
    r = sd_bus_call_method(
        bus,
        "org.freedesktop.Notifications",
        "/org/freedesktop/Notifications",
        "org.miqu.Notify",
        action.c_str(),
        &err,
        nullptr,
        ""
    );
    sd_bus_error_free(&err);
    sd_bus_unref(bus);
    return r >= 0;
}

} // namespace miqunotify
