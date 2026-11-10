#pragma once

#include "settings_model.hpp"
#include <string>
#include <vector>
#include <memory>

namespace miqu {

struct RadioDeviceInfo {
    int id = -1;
    std::string name;       // e.g. "phy0", "dell-wifi", "hci0"
    std::string type;       // "wlan", "bluetooth", "wwan", "gps", "nfc"
    bool soft_blocked = false;
    bool hard_blocked = false;
};

struct AirplaneState {
    bool is_airplane_mode = false;
    bool global_networking_enabled = true;
    bool wifi_blocked = false;
    bool bluetooth_blocked = false;
    bool wwan_blocked = false;
    bool any_hard_blocked = false;
    std::vector<RadioDeviceInfo> radios;
};

class AirplaneBackend {
public:
    static bool is_available();
    static AirplaneState get_state();
    static bool set_airplane_mode(bool enable);
    static void sync_item_state(const std::shared_ptr<SettingItem>& item);

    // Direct hardware / kernel control
    static bool set_rfkill_all(bool block);
    static bool set_networking_enabled(bool enable);
    static bool set_bluetooth_powered(bool powered);
    static std::vector<RadioDeviceInfo> scan_rfkill_devices();
};

} // namespace miqu
