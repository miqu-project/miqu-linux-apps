#pragma once

#include "settings_model.hpp"
#include <string>
#include <vector>
#include <memory>

namespace miqu {

struct WifiAdapterInfo {
    std::string iface;
    std::string mac_address;
    bool available = false;
    bool radio_enabled = true;
    bool is_connected = false;
    std::string connected_ssid;
    std::string ipv4_address;
    std::string ipv4_gateway;
    std::string dns_servers;
    int signal_percent = 0;
    std::string frequency;
    std::string channel;
    std::string bitrate;
    std::string security;
};

class WifiBackend {
public:
    static bool is_wifi_available();
    static WifiAdapterInfo get_adapter_info();
    static bool set_radio_enabled(bool enabled);
    static std::vector<WifiNetwork> scan_networks();
    static std::vector<std::string> get_saved_network_names();
    static bool connect_network(const std::string& ssid, const std::string& password = "");
    static bool disconnect_current(const std::string& iface = "");
    static bool forget_network(const std::string& ssid);
    static void rescan_async();
    static void sync_item_state(const std::shared_ptr<SettingItem>& item);
    static std::string resolve_signal_icon(int signal);
};

} // namespace miqu
