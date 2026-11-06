#pragma once

#include "modules/wifi/wifi_types.hpp"
#include <vector>
#include <string>

namespace miqu {

class WifiBackend {
public:
    static bool is_wifi_available();
    static WifiAdapterInfo get_adapter_info();
    static bool set_radio_enabled(bool enabled);
    static std::vector<WifiNetwork> scan_nearby_networks();
    static std::vector<std::string> get_saved_network_names();
    static bool connect_network(const std::string& ssid, const std::string& password = "");
    static bool disconnect_current(const std::string& iface = "");
    static bool forget_network(const std::string& ssid);
    static void rescan_async();
    static std::string resolve_signal_icon(int signal);
};

} // namespace miqu
