#include "notification_manager.hpp"
#include "../config/notify_config.hpp"
#include <miqutoolkit/core/app_engine.hpp>
#include <miqutoolkit/core/fs_utils.hpp>
#include <fstream>
#include <iostream>
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
        },
        [this]() {
            mark_all_read();
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

    size_t unread = std::count_if(m_history.begin(), m_history.end(), [](const auto& n) {
        return !n.is_read;
    });
    file << unread << " " << (m_dnd_active ? 1 : 0) << "\n";
}

void NotificationManager::log_debug(const std::string& msg) const {
    if (NotifyConfig::get().debug) {
        std::cout << "[miqunotify] " << msg << std::endl;
    }
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
        },
        [this]() {
            return get_history_json();
        }
    );

    log_debug("Daemon initialized. Listening on org.freedesktop.Notifications.");
    update_state_file();
    return ok;
}

uint32_t NotificationManager::on_notify(Notification notif) {
    const auto& cfg = NotifyConfig::get();

    log_debug("[NOTIFY] id=" + std::to_string(notif.id) +
              " app='" + notif.app_name + "'" +
              " summary='" + notif.summary + "'" +
              " urgency=" + std::to_string(static_cast<int>(notif.urgency)) +
              " transient=" + (notif.transient ? "true" : "false"));

    // Check if replacing existing notification
    if (notif.replaces_id > 0) {
        m_toast_window.remove_toast(notif.replaces_id);
        auto it = std::find_if(m_history.begin(), m_history.end(), [&](const auto& n) {
            return n.id == notif.replaces_id;
        });
        if (it != m_history.end()) {
            if (notif.transient) {
                m_history.erase(it);
            } else {
                *it = notif;
            }
        } else if (!notif.transient) {
            m_history.push_back(notif);
        }
    } else if (!notif.transient) {
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
                log_debug("[CLOSE] id=" + std::to_string(id) + " reason=" + std::to_string(reason));
                m_toast_window.remove_toast(id);
                if (m_dbus) m_dbus->emit_notification_closed(id, reason);
            },
            [this](uint32_t id, const std::string& key) {
                log_debug("[ACTION] id=" + std::to_string(id) + " key='" + key + "'");
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
    log_debug("[CLOSE] id=" + std::to_string(id) + " (by close call)");
    m_toast_window.remove_toast(id);
    if (m_dbus) {
        m_dbus->emit_notification_closed(id, 3); // 3 = closed by call
    }
}

void NotificationManager::on_control(const std::string& action) {
    log_debug("[CONTROL] command='" + action + "'");
    if (action == "ToggleCenter") {
        toggle_center();
    } else if (action == "OpenCenter") {
        open_center();
    } else if (action == "CloseCenter") {
        m_center_window.close();
    } else if (action == "ClearAll") {
        clear_all();
    } else if (action == "ToggleDnd") {
        toggle_dnd();
    } else if (action == "PopHistory") {
        pop_history();
    }
}

void NotificationManager::open_center() {
    m_center_window.open();
    mark_all_read();
    m_center_window.refresh_history(m_history, m_dnd_active);
}

void NotificationManager::toggle_center() {
    m_center_window.toggle();
    if (m_center_window.is_open()) {
        mark_all_read();
        m_center_window.refresh_history(m_history, m_dnd_active);
    }
}

void NotificationManager::mark_all_read() {
    bool changed = false;
    for (auto& n : m_history) {
        if (!n.is_read) {
            n.is_read = true;
            changed = true;
        }
    }
    if (changed) {
        update_state_file();
    }
}

void NotificationManager::pop_history() {
    if (m_history.empty()) return;
    const auto& notif = m_history.back();
    const auto& cfg = NotifyConfig::get();
    log_debug("[POP] Re-showing notification id=" + std::to_string(notif.id));

    auto toast = std::make_shared<ToastView>(
        notif,
        cfg.timeout_ms,
        [this](uint32_t id, uint32_t reason) {
            log_debug("[CLOSE] id=" + std::to_string(id) + " reason=" + std::to_string(reason));
            m_toast_window.remove_toast(id);
            if (m_dbus) m_dbus->emit_notification_closed(id, reason);
        },
        [this](uint32_t id, const std::string& key) {
            log_debug("[ACTION] id=" + std::to_string(id) + " key='" + key + "'");
            m_toast_window.remove_toast(id);
            if (m_dbus) {
                m_dbus->emit_action_invoked(id, key);
                m_dbus->emit_notification_closed(id, 2);
            }
        }
    );
    m_toast_window.add_toast(toast);
}

std::string NotificationManager::get_history_json() const {
    std::string json = "[";
    for (size_t i = 0; i < m_history.size(); ++i) {
        if (i > 0) json += ",";
        json += m_history[i].to_json();
    }
    json += "]";
    return json;
}

void NotificationManager::clear_all() {
    log_debug("[CLEAR] Clearing all notifications");
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
    log_debug(std::string("[DND] ") + (m_dnd_active ? "Enabled" : "Disabled"));
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
