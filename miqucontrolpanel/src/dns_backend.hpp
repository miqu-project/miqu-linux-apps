#pragma once

#include "settings_model.hpp"
#include <string>
#include <vector>
#include <memory>

namespace miqu {

struct DnsState {
    bool encrypted = false;
    std::string mode = "Off";
    std::string active_provider = "Local Gateway / DHCP";
    std::string active_connection;
    std::string active_device;
    std::vector<std::string> nameservers;
    std::string custom_hostname;
    std::string dot_status = "default";
    std::string dnssec_status = "default";
    std::string system_resolver = "NetworkManager (Direct)";
    bool systemd_resolved_active = false;
    bool dnscrypt_active = false;
    float last_latency_ms = -1.0f;
    std::string last_test_details;
};

class DnsBackend {
public:
    static bool is_available();
    static DnsState get_state();
    static void sync_item_state(const std::shared_ptr<SettingItem>& item);
    static bool apply_preset(const std::string& preset_name, const std::string& custom_host = "");
    static bool test_resolution(const std::string& domain = "cloudflare.com",
                                float* out_latency_ms = nullptr,
                                std::string* out_details = nullptr);

    // State persistence
    static void load_saved_state(std::string& out_mode, std::string& out_custom_host);
    static void save_state(const std::string& mode, const std::string& custom_host);

    // Hardware and network inspection helpers
    static std::string get_active_connection(std::string* out_device = nullptr);
    static std::vector<std::string> read_resolv_conf_nameservers(const std::string& iface = "");

    // System DoT Engine Control (requires pkexec privilege escalation)
    static bool is_dot_engine_active();
    static bool enable_dot_engine(bool enable);
};

} // namespace miqu
