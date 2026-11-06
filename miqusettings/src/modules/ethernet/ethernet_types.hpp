#pragma once

#include <string>
#include <vector>

namespace miqu {

struct EthernetDevice {
    std::string name;              // e.g. "enp0s31f6"
    std::string connection_name;   // e.g. "Wired connection 1"
    std::string connection_uuid;
    std::string mac_address;       // e.g. "A4:4C:C8:84:48:AB"
    std::string state_str;         // "Connected", "Cable Unplugged", "Disconnected", etc.
    bool is_available = false;     // Hardware detected
    bool has_carrier = false;      // Physical cable plugged in
    bool is_connected = false;     // Active IP connection
    bool is_enabled = true;        // Interface administratively up
    int speed_mbps = 0;            // e.g. 1000
    std::string duplex = "Unknown";// "Full", "Half", "Unknown"
    std::string ipv4_method = "auto"; // "auto" or "manual"
    std::string ipv4_address;      // e.g. "192.168.1.105"
    std::string ipv4_prefix = "24";// e.g. "24"
    std::string ipv4_gateway;      // e.g. "192.168.1.1"
    std::string dns_servers;       // e.g. "1.1.1.1, 8.8.8.8"
    std::string ipv6_address;
    int mtu = 1500;
};

} // namespace miqu
