#pragma once

#include "settings_model.hpp"
#include <string>
#include <vector>
#include <memory>

namespace miqu {

struct FirewallTelemetry {
    int blocked_today = 0;
    int active_connections = 0;
    std::string monitored_interfaces = "None";
    std::string last_blocked_probe = "None";
    std::vector<std::string> recent_blocks;
};

class FirewallBackend {
public:
    static bool is_available();
    static bool is_enabled();
    static std::string get_incoming_policy();
    static std::string get_outgoing_policy();
    static std::string get_active_preset();
    static bool is_service_allowed(const std::string& service_key);
    static std::vector<FirewallCustomRule> get_custom_rules();
    static bool is_logging_enabled();
    static FirewallTelemetry get_telemetry();

    // Synchronize SettingItem firewall configuration with live system state
    static void sync_item_state(const std::shared_ptr<SettingItem>& item);

    // Privileged actions via Polkit (pkexec)
    static bool set_enabled(bool enable);
    static bool set_default_policy(bool incoming, const std::string& policy_val);
    static bool toggle_service(const std::string& service_key, bool allow);
    static bool add_custom_rule(const std::string& name, const std::string& port_proto, const std::string& action = "ALLOW");
    static bool delete_custom_rule(const FirewallCustomRule& rule);
    static bool set_logging(bool enable);
    static bool apply_preset(const std::string& preset_name);

    // Telemetry and interface helper
    static std::string get_active_connection();
};

} // namespace miqu
