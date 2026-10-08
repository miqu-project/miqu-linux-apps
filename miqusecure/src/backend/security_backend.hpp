#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace miqusecure {

enum class NetworkMode {
    Home,
    MobileHotspot,
    PublicWifi,
    Lockdown,
    Custom
};

struct FirewallRule {
    int id = 0;
    std::string port_or_service;
    std::string protocol = "tcp";
    std::string action = "ALLOW";
    std::string direction = "IN";
    std::string source = "Anywhere";
    std::string raw_line;
};

struct FirewallInfo {
    bool active = false;
    std::string status_text = "Inactive";
    std::string default_incoming = "DROP";
    std::string default_outgoing = "ACCEPT";
    std::string active_preset = "Custom";
    bool strict_mode = false;
    bool metered = false;
    bool lockdown_active = false;
    NetworkMode current_mode = NetworkMode::Home;
    std::string active_network_name;
    bool ssh_allowed = false;
    bool web_dev_allowed = false;
    bool syncthing_allowed = false;
    bool samba_allowed = false;
    std::vector<FirewallRule> rules;
    std::vector<std::string> recent_blocks;
};

struct DnsInfo {
    bool encrypted = false;
    std::string mode = "Standard (Plaintext)";
    std::string active_provider = "ISP Default";
    std::string active_connection_name;
    std::vector<std::string> nameservers;
    bool dnscrypt_installed = false;
    bool dnscrypt_active = false;
};

struct FlatpakAppInfo {
    std::string id;
    std::string name;
};

struct LsmInfo {
    std::vector<std::string> active_lsms;
    bool landlock_active = false;
    bool yama_active = false;
    bool apparmor_active = false;
    int enforcing_profiles = 0;
    int complain_profiles = 0;

    bool flatpak_installed = false;
    std::vector<FlatpakAppInfo> flatpak_apps;
};

struct CleanableFile {
    std::string filename;
    std::string full_path;
    std::string size_str;
    std::string type;
};

struct CleanerInfo {
    bool mat2_installed = false;
    std::string version = "";
    std::string thumbnails_size = "0 B";
    std::string browser_cache_size = "0 B";
    std::string trash_size = "0 B";
    std::string bash_history_size = "0 B";
    std::vector<CleanableFile> recent_files;
};

using MetadataInfo = CleanerInfo;

struct HardwareInfo {
    bool wifi_enabled = true;
    bool bluetooth_enabled = true;
    bool airplane_mode = false;
    bool mic_muted = false;
    bool camera_detected = false;
    bool camera_in_use = false;
    std::string camera_device_name;
    std::string camera_active_proc;
};

struct NetworkConnection {
    std::string id;              // proto:local:peer
    std::string protocol;        // "TCP", "UDP"
    std::string state;           // "ESTABLISHED", "LISTEN", "CLOSE_WAIT", etc.
    std::string local_addr;
    int local_port = 0;
    std::string remote_addr;
    int remote_port = 0;
    std::string remote_host;     // Resolved domain / reverse DNS
    std::string service_name;    // "HTTPS", "HTTP", "DNS", "DoT", "SSH", etc.
    std::string process_name;    // e.g. "firefox", "language_server", "mpd"
    int pid = 0;
    bool is_outbound = true;
    bool is_loopback = false;
    bool is_encrypted = false;
    bool is_dns = false;
    std::string timestamp;       // e.g. "20:28:15"
};

struct TrafficReport {
    std::vector<NetworkConnection> active_connections;
    std::vector<NetworkConnection> recent_events;
    int total_outbound = 0;
    int total_inbound = 0;
    int total_encrypted = 0;
    int total_plaintext = 0;
    int total_apps = 0;
    float rx_rate_kb = 0.0f;
    float tx_rate_kb = 0.0f;
};

struct TestbedPackage {
    std::string name;
    std::string version;
};

struct TestboxItem {
    std::string name;
    std::string image;
    std::string status;
    std::vector<TestbedPackage> installed_packages;
};

struct DistroboxInfo {
    bool distrobox_installed = false;
    bool podman_installed = false;
    std::vector<TestboxItem> boxes;
};

class SecurityBackend {
public:
    // Specific readers
    static FirewallInfo read_firewall();
    static DnsInfo read_dns();
    static HardwareInfo read_hardware();
    static LsmInfo read_lsm();
    static CleanerInfo read_cleaner();
    static MetadataInfo read_metadata() { return read_cleaner(); }
    static TrafficReport read_traffic();
    static DistroboxInfo read_testbed();

    // Testbed actions
    static bool create_testbed(const std::string& name, const std::string& image = "archlinux");
    static bool launch_testbed_terminal(const std::string& name);
    static bool destroy_testbed(const std::string& name);
    static bool install_testbed_package(const std::string& box_name, const std::string& package_name, std::string& out_error);
    static bool remove_testbed_package(const std::string& box_name, const std::string& package_name);
    static bool launch_testbed_app(const std::string& box_name, const std::string& app_name);
    static std::vector<TestbedPackage> list_testbed_packages(const std::string& box_name);

    // Process & Connection actions
    static bool terminate_process(int pid, bool force = false);

    // Hardware and radio actions
    static bool set_wifi_enabled(bool enabled);
    static bool set_bluetooth_enabled(bool enabled);
    static bool set_airplane_mode(bool enabled);
    static bool set_mic_muted(bool muted);

    // Privileged firewall actions via pkexec
    static bool set_firewall_enabled(bool enabled);
    static bool set_strict_mode(bool strict);
    static bool set_network_mode(NetworkMode mode);
    static bool toggle_service(const std::string& port, bool enable);
    static bool apply_firewall_preset(const std::string& preset);
    static bool add_firewall_rule(const std::string& port, const std::string& proto, const std::string& action);
    static bool delete_firewall_rule(const FirewallRule& rule);
    static bool delete_firewall_rule(int rule_index);

    // DNS configuration actions via pkexec nmcli
    static bool apply_dns(const std::string& primary_ip, const std::string& secondary_ip);
    static bool restore_default_dns();

    // Metadata & BleachBit Cache Cleaning
    static bool clean_metadata(const std::string& filepath, std::string& output_log);
    static bool clean_thumbnails(std::string& output_log);
    static bool clean_browser_caches(std::string& output_log);
    static bool clean_trash_and_temp(std::string& output_log);
    static bool clean_shell_history(std::string& output_log);
    static bool clean_all_caches(std::string& output_log);
    static std::vector<CleanableFile> scan_recent_cleanable_files();

private:
    static std::string get_active_connection();
};

} // namespace miqusecure
