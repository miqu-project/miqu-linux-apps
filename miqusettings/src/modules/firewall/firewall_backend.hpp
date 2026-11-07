#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace miqu {

enum class FirewallProfile {
    Home,
    Work,
    Cellular,
    Public,
    Custom
};

struct FirewallRuleItem {
    int id = 0;                  // Rule index (1-based)
    std::string action = "Allow"; // "Allow", "Deny", "Reject"
    std::string protocol = "TCP"; // "TCP", "UDP", "Any"
    std::string port = "All";
    std::string source = "Anywhere";
    std::string raw_tuple;
};

struct FirewallLogEntry {
    std::string timestamp;
    std::string source_ip;
    std::string port;
    std::string action; // "DROP", "BLOCK", "REJECT", etc.
};

struct FirewallState {
    bool active = true;
    std::string status_text = "Firewall (Powered by UFW)";
    std::string log_level = "low";
    std::string incoming_policy = "Deny (Default)";
    std::string outgoing_policy = "Allow (Default)";
    FirewallProfile active_profile = FirewallProfile::Home;

    // Quick App Toggles (strictly determined from real parsed rules)
    bool ssh_enabled = false;
    bool plex_enabled = false;
    bool samba_enabled = false;
    bool kdeconnect_enabled = false;

    // Real Custom Rules
    std::vector<FirewallRuleItem> rules;

    // Real Monitoring from journald
    int blocked_last_hour = 0;
    std::vector<float> hourly_trend;
    std::vector<FirewallLogEntry> recent_logs;
};

using ActionCallback = std::function<void(bool success, const std::string& message)>;

class FirewallBackend {
public:
    static FirewallState& get_state();
    static void reload_from_system();

    // Privileged actions via Polkit (pkexec) - asynchronous and non-blocking
    static void set_active(bool active, ActionCallback cb = nullptr);
    static void set_incoming_policy(const std::string& policy, ActionCallback cb = nullptr);
    static void set_outgoing_policy(const std::string& policy, ActionCallback cb = nullptr);
    static void set_profile(FirewallProfile profile, ActionCallback cb = nullptr);
    static void set_app_toggle(const std::string& app_name, bool enabled, ActionCallback cb = nullptr);
    static void add_rule(const FirewallRuleItem& rule, ActionCallback cb = nullptr);
    static void delete_rule(int rule_num, ActionCallback cb = nullptr);
    static void set_log_level(const std::string& level, ActionCallback cb = nullptr);

    static void execute_privileged_cmd_async(const std::string& cmd, ActionCallback cb);

private:
    static void parse_user_rules();
    static void parse_journal_logs();
};

} // namespace miqu
