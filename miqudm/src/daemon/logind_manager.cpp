#include "logind_manager.hpp"
#include <iostream>

namespace miqudm {

static LogindManager s_logind;

LogindManager& LogindManager::get() {
    return s_logind;
}

LogindManager::~LogindManager() {
    shutdown();
}

bool LogindManager::init() {
    if (m_initialized && m_bus) return true;

    int r = sd_bus_open_system(&m_bus);
    if (r < 0) {
        std::cerr << "[miqudm-logind] Failed to connect to system bus: " << strerror(-r) << std::endl;
        m_bus = nullptr;
        m_initialized = false;
        return false;
    }

    m_initialized = true;
    return true;
}

void LogindManager::shutdown() {
    if (m_bus) {
        sd_bus_flush_close_unref(m_bus);
        m_bus = nullptr;
    }
    m_initialized = false;
}

bool LogindManager::call_manager_method(const char* method, bool interactive) {
    if (!m_initialized && !init()) return false;

    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message* reply = nullptr;

    int r = sd_bus_call_method(
        m_bus,
        "org.freedesktop.login1",
        "/org/freedesktop/login1",
        "org.freedesktop.login1.Manager",
        method,
        &error,
        &reply,
        "b",
        interactive ? 1 : 0
    );

    if (r < 0) {
        std::cerr << "[miqudm-logind] Method " << method << " failed: " << error.message << std::endl;
        sd_bus_error_free(&error);
        return false;
    }

    sd_bus_message_unref(reply);
    return true;
}

bool LogindManager::reboot() {
    return call_manager_method("Reboot");
}

bool LogindManager::power_off() {
    return call_manager_method("PowerOff");
}

bool LogindManager::suspend() {
    return call_manager_method("Suspend");
}

bool LogindManager::execute_power_action(PowerAction action) {
    switch (action) {
        case PowerAction::PowerOff: return power_off();
        case PowerAction::Reboot:   return reboot();
        case PowerAction::Suspend:  return suspend();
        case PowerAction::Hibernate: return call_manager_method("Hibernate");
    }
    return false;
}

bool LogindManager::switch_to_vt(int vt_number) {
    if (!m_initialized && !init()) return false;
    if (vt_number <= 0) return true;

    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message* reply = nullptr;

    int r = sd_bus_call_method(
        m_bus,
        "org.freedesktop.login1",
        "/org/freedesktop/login1/seat/seat0",
        "org.freedesktop.login1.Seat",
        "SwitchTo",
        &error,
        &reply,
        "u",
        static_cast<uint32_t>(vt_number)
    );

    if (r < 0) {
        std::cerr << "[miqudm-logind] SwitchTo VT " << vt_number << " failed: " << error.message << std::endl;
        sd_bus_error_free(&error);
        return false;
    }

    sd_bus_message_unref(reply);
    return true;
}

} // namespace miqudm
