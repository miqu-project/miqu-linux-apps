#pragma once

#include "settings_model.hpp"
#include <string>
#include <vector>
#include <memory>

namespace miqu {

struct VpnState {
    bool connected = false;
    std::string active_profile_name;
    std::string active_device;
    std::string endpoint = "Not connected";
    std::string client_ip = "None";
    std::string rx_bytes_str = "0 B";
    std::string tx_bytes_str = "0 B";
    std::string latest_handshake_str = "None";
    std::string account_number;
    std::string account_status = "Not linked";
    bool killswitch_active = false;
    std::vector<VpnProfile> profiles;
};

class VpnBackend {
public:
    static bool is_available();
    static VpnState get_state();
    static void sync_item_state(const std::shared_ptr<SettingItem>& item);

    // Profile actions
    static bool connect_profile(const std::string& profile_name);
    static bool disconnect();
    static bool import_profile(const std::string& file_path, std::string* out_name = nullptr, std::string* out_err = nullptr);
    static bool delete_profile(const std::string& profile_name);

    // Mullvad integration
    static bool check_mullvad_account(const std::string& account_id, std::string* out_status = nullptr);

    // Killswitch
    static bool set_killswitch(bool enable);

    // Persistence
    static void load_saved_state(std::string& out_active_profile, std::string& out_account_id, bool& out_killswitch);
    static void save_state(const std::string& active_profile, const std::string& account_id, bool killswitch);

    // Scanning
    static std::vector<VpnProfile> scan_system_profiles();
};

} // namespace miqu
