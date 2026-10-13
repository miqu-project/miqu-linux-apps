#include "notification_manager.hpp"
#include "../config/notify_config.hpp"
#include <miqutoolkit/core/app_engine.hpp>
#include <miqutoolkit/core/fs_utils.hpp>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <unistd.h>

namespace miqunotify {

namespace fs = std::filesystem;

NotificationManager::NotificationManager()
    : m_center_window(
        [this]() { clear_all(); },
        [this](bool en) { set_dnd(en); },
        [this](uint32_t id) {
            auto it = std::find_if(m_history.begin(), m_history.end(), [id](const auto& n) {
                return n.id == id;
            });
            if (it != m_history.end()) {
                m_history.erase(it);
                update_state_file();
                m_center_window.refresh_history(m_history, m_dnd_active);
            }
        },
        [this](uint32_t id, const std::string& key) {
            if (m_dbus) {
                m_dbus->emit_action_invoked(id, key);
                m_dbus->emit_notification_closed(id, 2);
            }
            auto it = std::find_if(m_history.begin(), m_history.end(), [id](const auto& n) {
                return n.id == id;
            });
            if (it != m_history.end()) {
                m_history.erase(it);
                update_state_file();
                m_center_window.refresh_history(m_history, m_dnd_active);
            }
        }
    ) {
    m_dnd_active = NotifyConfig::get().dnd_enabled;
}

NotificationManager::~NotificationManager() {
    std::string path = get_state_file_path();
    if (!path.empty()) {
        std::error_code ec;
        fs::remove(path, ec);
    }
}

std::string NotificationManager::get_state_file_path() {
    const char* runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (runtime_dir && *runtime_dir) {
        return std::string(runtime_dir) + "/miqunotify_state";
    }
    const char* home = getenv("HOME");
    if (home) {
        std::string dir = std::string(home) + "/.cache/miqunotify";
        std::error_code ec;
        fs::create_directories(dir, ec);
        return dir + "/state";
    }
    return "";
}

void NotificationManager::update_state_file() {
    std::string path = get_state_file_path();
    if (path.empty()) return;

    std::ofstream file(path);
    if (!file.is_open()) return;

    size_t unread = m_history.size();
    file << unread << " " << (m_dnd_active ? 1 : 0) << "\n";
}

bool NotificationManager::init() {
    m_dbus = std::make_unique<DBusService>();

    bool ok = m_dbus->start(
        [this](Notification notif) -> uint32_t {
            uint32_t assigned_id = (notif.replaces_id > 0) ? notif.replaces_id : m_next_id++;
            notif.id = assigned_id;

            auto engine = miqu::AppEngine::instance();
            if (engine) {
                engine->post([this, notif]() {
                    on_notify(notif);
                });
            }
            return assigned_id;
        },
        [this](uint32_t id) {
            auto engine = miqu::AppEngine::instance();
            if (engine) {
                engine->post([this, id]() {
                    on_close_notification(id);
                });
            }
        },
        [this](const std::string& action) {
            auto engine = miqu::AppEngine::instance();
            if (engine) {
                engine->post([this, action]() {
                    on_control(action);
                });
            }
        }
    );

    update_state_file();
    return ok;
}

uint32_t NotificationManager::on_notify(Notification notif) {
    const auto& cfg = NotifyConfig::get();

    // Check if replacing existing notification
    if (notif.replaces_id > 0) {
        m_toast_window.remove_toast(notif.replaces_id);
        auto it = std::find_if(m_history.begin(), m_history.end(), [&](const auto& n) {
            return n.id == notif.replaces_id;
        });
        if (it != m_history.end()) {
            *it = notif;
        } else {
            m_history.push_back(notif);
        }
    } else {
        m_history.push_back(notif);
    }

    // Limit history size
    while (m_history.size() > static_cast<size_t>(cfg.history_limit)) {
        m_history.erase(m_history.begin());
    }

    // If DND is inactive, show floating toast
    if (!m_dnd_active) {
        auto toast = std::make_shared<ToastView>(
            notif,
            cfg.timeout_ms,
            [this](uint32_t id, uint32_t reason) {
                m_toast_window.remove_toast(id);
                if (m_dbus) m_dbus->emit_notification_closed(id, reason);
            },
            [this](uint32_t id, const std::string& key) {
                m_toast_window.remove_toast(id);
                if (m_dbus) {
                    m_dbus->emit_action_invoked(id, key);
                    m_dbus->emit_notification_closed(id, 2);
                }
            }
        );
        m_toast_window.add_toast(toast);
    }

    update_state_file();

    if (m_center_window.is_open()) {
        m_center_window.refresh_history(m_history, m_dnd_active);
    }

    return notif.id;
}

void NotificationManager::on_close_notification(uint32_t id) {
    m_toast_window.remove_toast(id);
    if (m_dbus) {
        m_dbus->emit_notification_closed(id, 3); // 3 = closed by call
    }
}

void NotificationManager::on_control(const std::string& action) {
    if (action == "ToggleCenter") {
        toggle_center();
    } else if (action == "OpenCenter") {
        m_center_window.open();
        m_center_window.refresh_history(m_history, m_dnd_active);
    } else if (action == "CloseCenter") {
        m_center_window.close();
    } else if (action == "ClearAll") {
        clear_all();
    } else if (action == "ToggleDnd") {
        toggle_dnd();
    }
}

void NotificationManager::toggle_center() {
    m_center_window.toggle();
    if (m_center_window.is_open()) {
        m_center_window.refresh_history(m_history, m_dnd_active);
    }
}

void NotificationManager::clear_all() {
    m_toast_window.close();
    m_history.clear();
    update_state_file();
    if (m_center_window.is_open()) {
        m_center_window.refresh_history(m_history, m_dnd_active);
    }
}

void NotificationManager::toggle_dnd() {
    set_dnd(!m_dnd_active);
}

void NotificationManager::set_dnd(bool enabled) {
    m_dnd_active = enabled;
    if (m_dnd_active) {
        // Close any currently visible toast banners
        m_toast_window.close();
    }
    update_state_file();
    if (m_center_window.is_open()) {
        m_center_window.refresh_history(m_history, m_dnd_active);
    }
}

} // namespace miqunotify
