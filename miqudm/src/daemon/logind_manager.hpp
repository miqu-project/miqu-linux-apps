#pragma once

#include "types.hpp"
#include <systemd/sd-bus.h>
#include <string>

namespace miqudm {

class LogindManager {
public:
    static LogindManager& get();

    bool init();
    void shutdown();

    bool reboot();
    bool power_off();
    bool suspend();
    bool execute_power_action(PowerAction action);
    bool switch_to_vt(int vt_number);

    LogindManager() = default;
    ~LogindManager();

private:
    bool call_manager_method(const char* method, bool interactive = true);

    sd_bus* m_bus = nullptr;
    bool m_initialized = false;
};

} // namespace miqudm
