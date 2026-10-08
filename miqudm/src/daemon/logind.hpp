#pragma once

#include <string>
#include <systemd/sd-bus.h>

namespace miqudm {

class LogindManager {
public:
    static LogindManager& get();

    bool init();
    void shutdown();

    bool power_off(bool interactive = false);
    bool reboot(bool interactive = false);
    bool suspend(bool interactive = false);
    bool hibernate(bool interactive = false);

    bool switch_to_vt(unsigned int vt_number);

private:
    LogindManager() = default;
    ~LogindManager();

    bool call_manager_method(const std::string& method, bool interactive);

    sd_bus* m_bus = nullptr;
};

} // namespace miqudm
