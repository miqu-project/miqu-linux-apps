#pragma once

#include <string>
#include <vector>

namespace miqu {

struct WifiNetwork {
    std::string ssid;
    std::string bssid;
    int signal_percent = 0;
    std::string frequency;   // "2.4 GHz", "5 GHz", etc.
    std::string security;    // "WPA2", "WPA3", "WPA2/WPA3", "Open", etc.
    std::string rate;        // "1170 Mbit/s"
    bool is_connected = false;
    bool is_saved = false;
    bool is_secured = true;
};

struct WifiAdapterInfo {
    std::string iface;       // e.g. "wlp2s0"
    std::string mac_address; // e.g. "E4:70:B8:52:2F:BF"
    bool available = false;
    bool radio_enabled = true;
    bool is_connected = false;
    std::string connected_ssid;
    std::string ipv4_address;
    std::string ipv4_gateway;
    std::string dns_servers;
    int signal_percent = 0;
    std::string frequency;
    std::string bitrate;
    std::string security;
};

} // namespace miqu
