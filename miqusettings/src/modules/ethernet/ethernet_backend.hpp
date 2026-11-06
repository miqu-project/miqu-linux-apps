#pragma once

#include "modules/ethernet/ethernet_types.hpp"
#include <vector>
#include <string>

namespace miqu {

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
};

} // namespace miqu
