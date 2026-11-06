#pragma once

#include <string>
#include <vector>

namespace miqu {

struct RadioDeviceStatus {
    int id = 0;
    std::string type;     // "wlan", "bluetooth", "wwan"
    std::string name;     // e.g. "dell-wifi", "phy0", "hci0"
    bool soft_blocked = false;
    bool hard_blocked = false;
};

struct AirplaneStatus {
    bool enabled = false;
    bool wifi_enabled = true;
    bool bluetooth_enabled = true;
    bool wwan_enabled = false;
    bool has_wwan = false;
    int total_radios = 0;
    int blocked_radios = 0;
    std::vector<RadioDeviceStatus> devices;
};

} // namespace miqu
