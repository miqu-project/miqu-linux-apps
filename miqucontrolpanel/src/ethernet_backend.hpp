#pragma once

#include "settings_model.hpp"
#include <vector>
#include <string>
#include <memory>

namespace miqu {

struct EthernetDevice {
    std::string name;              // e.g. "enp0s31f6"
    std::string connection_name;   // e.g. "Wired connection 1"
    std::string connection_uuid;
    std::string mac_address;       // e.g. "A4:4C:C8:84:48:AB"
    std::string state_str;         // "Connected", "Cable Unplugged", "Disconnected"
    bool is_available = false;     // Hardware detected
    bool has_carrier = false;      // Physical cable plugged in
    bool is_connected = false;     // Active IP connection
    bool is_enabled = true;        // Interface administratively up
    int speed_mbps = 0;            // e.g. 1000
    std::string duplex = "Unknown";// "Full Duplex", "Half Duplex", "Unknown"
    std::string ipv4_method = "auto"; // "auto" or "manual"
    std::string ipv4_address;      // e.g. "192.168.1.105"
    std::string ipv4_prefix = "24";// e.g. "24"
    std::string ipv4_gateway;      // e.g. "192.168.1.1"
    std::string dns_servers;       // e.g. "1.1.1.1, 8.8.8.8"
    std::string ipv6_address;
    int mtu = 1500;
};

class EthernetBackend {
public:
    static bool is_networkmanager_available();
    static std::vector<EthernetDevice> get_all_devices();
    static EthernetDevice get_device(const std::string& iface = "");
    static bool set_device_enabled(const std::string& iface, bool enabled);
    static bool configure_ipv4_auto(const std::string& con_name);
    static bool configure_ipv4_manual(
        const std::string& con_name,
        const std::string& ip_with_prefix,
        const std::string& gateway,
        const std::string& dns
    );
    static void copy_to_clipboard(const std::string& text);
    static std::string prefix_to_netmask(int prefix);
    static int netmask_to_prefix(const std::string& netmask);
    static void sync_item_state(const std::shared_ptr<SettingItem>& item);
};

} // namespace miqu
