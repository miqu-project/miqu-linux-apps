#include "logind.hpp"
#include <iostream>

namespace miqudm {

static constexpr const char* LOGIND_DEST = "org.freedesktop.login1";
static constexpr const char* LOGIND_PATH = "/org/freedesktop/login1";
static constexpr const char* LOGIND_INTERFACE = "org.freedesktop.login1.Manager";

LogindManager& LogindManager::get() {
    static LogindManager instance;
    return instance;
}

LogindManager::~LogindManager() {
    shutdown();
}

bool LogindManager::init() {
    if (m_bus) return true;

    int ret = sd_bus_default_system(&m_bus);
    if (ret < 0) {
        std::cerr << "[miqudm-logind] Failed to connect to system bus: " << strerror(-ret) << std::endl;
        m_bus = nullptr;
        return false;
    }
    return true;
}

void LogindManager::shutdown() {
    if (m_bus) {
        sd_bus_unref(m_bus);
        m_bus = nullptr;
    }
}

bool LogindManager::call_manager_method(const std::string& method, bool interactive) {
    if (!init()) return false;

    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message* reply = nullptr;

    int ret = sd_bus_call_method(
        m_bus,
        LOGIND_DEST,
        LOGIND_PATH,
        LOGIND_INTERFACE,
        method.c_str(),
        &error,
        &reply,
        "b",
        interactive ? 1 : 0
    );

    if (ret < 0) {
        std::cerr << "[miqudm-logind] " << method << " failed: " << (error.message ? error.message : strerror(-ret)) << std::endl;
        sd_bus_error_free(&error);
        return false;
    }

    sd_bus_message_unref(reply);
    return true;
}

bool LogindManager::power_off(bool interactive) {
    return call_manager_method("PowerOff", interactive);
}

bool LogindManager::reboot(bool interactive) {
    return call_manager_method("Reboot", interactive);
}

bool LogindManager::suspend(bool interactive) {
    return call_manager_method("Suspend", interactive);
}

bool LogindManager::hibernate(bool interactive) {
    return call_manager_method("Hibernate", interactive);
}

bool LogindManager::switch_to_vt(unsigned int vt_number) {
    if (!init()) return false;

    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message* reply = nullptr;

    // SwitchTo(u) on seat0: /org/freedesktop/login1/seat/seat0
    int ret = sd_bus_call_method(
        m_bus,
        LOGIND_DEST,
        "/org/freedesktop/login1/seat/seat0",
        "org.freedesktop.login1.Seat",
        "SwitchTo",
        &error,
        &reply,
        "u",
        vt_number
    );

    if (ret < 0) {
        sd_bus_error_free(&error);
        return false;
    }

    sd_bus_message_unref(reply);
    return true;
}

} // namespace miqudm
