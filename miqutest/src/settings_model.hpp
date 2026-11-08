#pragma once

#include <string>
#include <vector>
#include <memory>
#include <utility>

namespace miqu {

struct TelemetryEntry {
    std::string key;
    std::string value;
};

struct WifiNetwork {
    std::string ssid;
    std::string status; // "Connected", "Saved", "Connect"
    std::string icon = "󰤨";
    bool secured = true;
};

struct BluetoothDevice {
    std::string name;
    std::string status; // "Connected", "Paired", "Pair"
    std::string icon = "󰂯";
    std::string battery = "";
};

struct EthernetProfile {
    std::string name = "Wired Connection 1";
    std::string status = "Connected";
    std::string ip_address = "192.168.1.105";
    std::string subnet = "255.255.255.0";
    std::string gateway = "192.168.1.1";
    std::string dns = "1.1.1.1, 8.8.8.8";
    std::string mode = "Automatic (DHCP)";
};

struct MonitorConfig {
    std::string connector = "DP-1";
    std::string model_name = "Dell UltraSharp U2723QE";
    bool is_primary = true;
    std::string current_resolution = "3840 × 2160";
    std::vector<std::string> available_resolutions = {"3840 × 2160", "2560 × 1440", "1920 × 1080"};
    std::string current_refresh_rate = "144 Hz";
    std::vector<std::string> available_refresh_rates = {"144 Hz", "120 Hz", "60 Hz"};
    std::string current_scale = "125%";
    std::vector<std::string> available_scales = {"100%", "125%", "150%", "200%"};
    std::string current_orientation = "Standard (0°)";
    std::vector<std::string> available_orientations = {"Standard (0°)", "90° (Portrait)", "180°", "270° (Portrait)"};
};

struct InstalledAppInfo {
    std::string app_name;
    std::string version;
    std::string publisher;
    std::string category;
    std::string description;
    std::string binary_path;
    std::string desktop_file;
    std::string package_type;
    std::string size_total;
    std::string size_binary;
    std::string size_data;
    std::string permissions;
    bool is_running = false;
    int pid = 0;
    std::string network_policy = "Allow"; // "Allow", "Local Only", "Block"
};

struct DefaultAppCandidate {
    std::string name;
    std::string icon;
    std::string description;
    bool is_default = false;
};

struct DefaultAppRole {
    std::string role_name;
    std::string mime_types;
    std::vector<DefaultAppCandidate> candidates;
};

class SettingItem {
public:
    std::string id;
    std::string title;
    std::string subtitle;
    std::string icon;
    bool is_category = false;
    std::vector<std::shared_ptr<SettingItem>> children;

    // Dead-end settings controls
    bool has_switch = false;
    bool switch_val = false;
    std::string switch_label = "Enabled";

    bool has_secondary_switch = false;
    bool secondary_switch_val = false;
    std::string secondary_switch_label = "Auto-Connect";

    bool has_slider = false;
    float slider_val = 0.5f;
    std::string slider_label = "Level";
    std::string slider_unit = "%";

    bool has_action = false;
    std::string action_label = "Execute";
    std::string last_action_message = "";

    std::vector<TelemetryEntry> telemetry;
    std::string telemetry_title;
    std::vector<WifiNetwork> wifi_networks;
    std::vector<BluetoothDevice> bt_devices;
    EthernetProfile eth_profile;
    bool is_monitor = false;
    MonitorConfig monitor_cfg;

    bool is_installed_app = false;
    InstalledAppInfo app_info;

    bool is_default_app_role = false;
    DefaultAppRole default_app_role;

    static std::shared_ptr<SettingItem> create_category(
        std::string id, std::string title, std::string icon,
        std::vector<std::shared_ptr<SettingItem>> children = {}
    );

    static std::shared_ptr<SettingItem> create_leaf(
        std::string id, std::string title, std::string subtitle, std::string icon
    );
};

class SettingsModel {
public:
    static std::vector<std::shared_ptr<SettingItem>> build_default_tree();
};

} // namespace miqu
