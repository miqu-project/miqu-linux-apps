#pragma once

#include <string>

namespace miqu {

enum class BluetoothDeviceCategory {
    Audio,
    Input,
    Other
};

struct BluetoothDeviceItem {
    std::string id;
    std::string mac;
    std::string name;
    std::string icon_name;
    BluetoothDeviceCategory category = BluetoothDeviceCategory::Other;
    bool is_connected = false;
    bool is_paired = true;
    int battery_percentage = -1; // -1 if unknown
    std::string connection_info;
};

struct BluetoothAdapterInfo {
    bool available = false;
    bool powered = false;
    bool discoverable = false;
    bool discovering = false;
    bool soft_blocked = false;
    std::string name = "miqu-desktop";
    std::string mac;
};

} // namespace miqu
