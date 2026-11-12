#pragma once

#include <string>
#include <vector>
#include <memory>
#include <utility>
#include <functional>

namespace miqu {

struct TelemetryEntry {
    std::string key;
    std::string value;
};

struct WifiNetwork {
    std::string ssid;
    std::string status = "Connect"; // "Connected", "Saved", "Connect", "Connecting..."
    std::string icon = "󰤨";
    bool secured = true;

    // Detailed link fields (displayed on 3rd line and modals)
    std::string bssid;
    int signal_percent = 0;
    std::string frequency;
    std::string channel;
    std::string security;
    std::string rate;
    std::string details;
};

struct BluetoothDevice {
    std::string name;
    std::string status = "Paired"; // "Connected", "Paired", "Tap to pair", "Ready to pair"
    std::string icon = "󰂯";
    std::string battery = "";
    std::string mac = "";
    bool is_connected = false;
    bool is_paired = true;
    int battery_percentage = -1;
    std::string category = "Other";
};

struct EthernetProfile {
    std::string name = "Wired Connection 1";
    std::string status = "Connected";
    std::string ip_address = "192.168.1.105";
    std::string subnet = "255.255.255.0";
    std::string gateway = "192.168.1.1";
    std::string dns = "1.1.1.1, 8.8.8.8";
    std::string mode = "Automatic (DHCP)";
    std::string iface = "enp0s31f6";
    std::string mac_address;
    std::string ipv4_prefix = "24";
    std::string ipv6_address;
    int speed_mbps = 0;
    std::string duplex = "Unknown";
    bool has_carrier = false;
    bool is_enabled = true;
    int mtu = 1500;
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
    std::string exec_cmd = "";
    std::string package_name = "";
    bool is_flatpak = false;
    bool is_user_app = false;
};

struct DefaultAppCandidate {
    std::string name;
    std::string icon;
    std::string description;
    bool is_default = false;
    std::string desktop_id = "";
    std::string exec_cmd = "";
};

struct DefaultAppRole {
    std::string role_name;
    std::string mime_types;
    std::vector<DefaultAppCandidate> candidates;
    std::string primary_mime = "";
    bool populated = false;
};

struct PrivateDnsPreset {
    std::string name;
    std::string hostname;
    std::string description;
    std::string endpoints;
};

struct PrivateDnsConfig {
    std::string selected_option = "Cloudflare (1.1.1.1)";
    std::string custom_hostname = "dns.nextdns.io";
    std::vector<std::string> options = {
        "Off",
        "Automatic",
        "Cloudflare (1.1.1.1)",
        "Quad9 (Malware Block)",
        "AdGuard (Ad & Tracker Block)",
        "Mullvad (Zero-Log Privacy)",
        "Custom Hostname"
    };
    std::vector<PrivateDnsPreset> presets = {
        {"Off", "(Disabled)", "Standard unencrypted DNS assigned by router / local DHCP", "Local Gateway / DHCP"},
        {"Automatic", "(Automatic opportunistic DoT)", "Opportunistic TLS upgrade if upstream server supports DNS-over-TLS", "Opportunistic TLS (Port 853)"},
        {"Cloudflare (1.1.1.1)", "one.one.one.one", "Fast global anycast resolver with DNS-over-TLS (DoT)", "1.1.1.1 • 1.0.0.1 • 2606:4700:4700::1111"},
        {"Quad9 (Malware Block)", "dns.quad9.net", "Threat intelligence blocking phishing, botnets & malware", "9.9.9.9 • 149.112.112.112 • 2620:fe::fe"},
        {"AdGuard (Ad & Tracker Block)", "dns.adguard-dns.com", "Filters ads, tracking beacons, telemetry and counters", "94.140.14.14 • 94.140.15.15"},
        {"Mullvad (Zero-Log Privacy)", "dns.mullvad.net", "Strict zero-logging privacy DNS hosted in Sweden", "194.242.2.2 • 193.19.108.2"},
        {"Custom Hostname", "", "User-specified encrypted DNS-over-TLS upstream resolver endpoint", "Custom TLS endpoint (Port 853)"}
    };
    bool dot_engine_active = false;
};

struct VpnProfile {
    std::string name;
    std::string type = "WireGuard";
    std::string endpoint;
    std::string client_ip;
    std::string public_key;
    std::string country = "Unknown";
    std::string city = "";
    bool is_active = false;
};

struct VpnConfig {
    bool connected = false;
    std::string active_profile = "";
    std::vector<VpnProfile> profiles;
    std::string account_number = "";
    std::string account_status = "";
    std::string rx_bytes_str = "0 B";
    std::string tx_bytes_str = "0 B";
    std::string latest_handshake_str = "None";
    bool killswitch_enabled = false;
};

struct BatteryConfig {
    bool has_battery = true;
    int percentage = 88;
    bool is_charging = false;
    bool power_saver = false;
    std::string status = "Discharging"; // "Charging", "Discharging", "Full", "Not charging"
    std::string time_remaining_str = "";
    std::string technology = "Li-ion";
    std::string model_name = "";
    std::string manufacturer = "";
    float voltage_v = 0.0f;
    float current_a = 0.0f;
    float energy_now_wh = 0.0f;
    std::string power_profile = "Balanced"; // "Power Saver", "Balanced", "Performance"
    std::vector<std::string> available_profiles = {"Power Saver", "Balanced", "Performance"};
    bool charge_limit_enabled = true; // 80% Protection limit
    int charge_limit_pct = 80;
    std::string health_status = "Healthy";
    float health_pct = 98.4f;
    int cycle_count = 142;
    float design_capacity_wh = 60.0f;
    float full_capacity_wh = 59.0f;
    float power_draw_w = 7.8f;
    float temp_c = 31.4f;
    bool populated = false;
};

struct FirewallCustomRule {
    int id = 0;
    std::string name;
    std::string port_proto;
    std::string port;
    std::string protocol = "tcp";
    std::string action = "ALLOW";
    std::string source = "Anywhere";
    bool enabled = true;
};

struct FirewallConfig {
    bool enabled = true;
    std::string backend = "UFW";
    std::string incoming_policy = "Drop (Deny)";
    std::string outgoing_policy = "Allow";
    std::string selected_preset = "Home";

    // Common predefined service switches
    bool allow_kdeconnect = true;
    bool allow_ssh = false;
    bool allow_samba = false;
    bool allow_cups = true;

    // User-added custom rules
    std::vector<FirewallCustomRule> custom_rules;

    // Monitor card
    bool monitor_enabled = true;
    int blocked_today = 0;
    int active_connections = 0;
    std::string monitored_interfaces = "None";
    std::string last_blocked_probe = "None";
    std::vector<std::string> recent_blocks;
};

struct ScreenTimeoutLockConfig {
    // 1. Inactivity Timers (miquidle listeners)
    int lock_timeout_minutes = 1;        // Inactivity before locking (1, 2, 5, 10, 15, 30 min, 0 = Never)
    int display_off_minutes = 5;         // Inactivity before display off (1, 2, 5, 10, 15, 30, 60 min, 0 = Never)
    int sleep_minutes = 15;              // Inactivity before sleep (5, 10, 15, 30, 60, 120 min, 0 = Never)
    int hibernate_delay_minutes = 120;   // Inactivity before auto-hibernate to disk (30, 60, 120, 240 min, 0 = Never)

    // 2. Arch Linux & systemd-logind Power Targets
    std::string sleep_mode = "Hybrid Sleep (Recommended)"; // Hybrid Sleep, Sleep (RAM), Hibernate (Disk)
    std::string lid_action = "Sleep";                     // Sleep, Lock only, Hibernate, Turn off display only, Ignore (Do nothing)
    std::string power_button_action = "Show power menu (miqupower)"; // Show power menu, Sleep, Lock, Hibernate, Power off
    std::string critical_battery_action = "Hibernate";    // Hibernate, Sleep, Power off

    // 3. Laptop & Display Power Saving
    bool dim_screen = true;              // Dim screen before display turns off (miquidle pre-timeout)
    bool prevent_sleep_media = true;     // Keep display awake during media playback (Wayland idle-inhibit)
    bool energy_saver_battery = true;    // Conserve power by shortening timeouts on battery
    bool clamshell_mode = true;          // Stay awake when closed if external display connected

    // 4. Security & miqulock Options
    bool lock_before_sleep = true;       // Lock screen before system sleeps (miquidle before_sleep_cmd)
    bool lock_on_display_off = true;     // Lock session when display turns off
    bool require_password_immediately = true; // Demand password on wake
    int grace_period_seconds = 0;        // Grace period window (0s = Instant, 15s, 30s, 1m, 2m, 5m)
    bool lockscreen_show_power_actions = true; // miqulock show_power_actions (sleep/reboot/shutdown buttons)
    bool lockscreen_24h_clock = true;    // miqulock 24-hour time format (%H:%M)
};

struct NightLightConfig {
    bool enabled = false;
    float warmth = 0.53f;
};

struct TilingConfig {
    // 1. Layout Mode
    std::string layout_mode = "Dwindle (Dynamic BSP)";
    std::vector<std::string> available_layout_modes = {
        "Dwindle (Dynamic BSP)",
        "Master / Stack (Right Stack)",
        "Bottom Stack (bstack)",
        "Centered Master (Flanked Stacks)",
        "Fibonacci Spiral",
        "Equal Columns",
        "Grid Matrix (N × M)",
        "Monocle (Single Focus)"
    };

    // 2. Windows Geometry & Borders
    int inner_gap = 8;        // 0 to 32 px
    int outer_gap = 12;       // 0 to 48 px
    bool smart_gaps = false;  // collapse gaps when single window
    int border_width = 1;     // 0 to 8 px
    int corner_radius = 18;   // 0 to 32 px

    // Border colors
    std::string active_border_preset = "Yellow-Red Gradient (45°)";
    std::vector<std::string> active_border_presets = {
        "Yellow-Red Gradient (45°)",
        "Cyan-Blue Gradient (45°)",
        "Purple-Blue Gradient (135°)",
        "Neon Emerald-Cyan (90°)",
        "Monochrome Bright (#FFFFFF)",
        "Primary Accent"
    };
    std::string active_border_value = "#ffff00 #ff0000 45deg";

    std::string inactive_border_preset = "Dark Slate Gradient (45°)";
    std::vector<std::string> inactive_border_presets = {
        "Dark Slate Gradient (45°)",
        "Graphite Charcoal (#2D3748)",
        "Muted Amber-Red (45°)",
        "Subtle Outline (20% Opacity)",
        "Transparent (Zero Border)"
    };
    std::string inactive_border_value = "#334455 #222d38 45deg";

    bool border_shading = false;
    float border_shading_strength = 0.35f; // 0.0 to 1.0
    bool resize_on_border = true;
    int border_grab_area = 8; // px

    // 3. Split Behaviour
    float default_split_ratio = 0.50f; // 0.1 to 0.9
    bool split_focus = true;           // Hyprland style: split focused window
    bool smart_split = true;           // Cursor-aware split orientation
    bool preserve_split = true;        // Preserve split ratio/orientation
    float split_width_multiplier = 1.2f; // Aspect ratio multiplier (0.8 to 2.5)

    // 4. Focus Behaviour
    bool focus_follows_mouse = true;   // Hover to focus (sloppy focus)
    bool click_to_focus = true;        // Window raise and focus on mouse click
    bool warp_cursor_on_focus = true;  // Warp cursor to focused window center
    bool focus_on_workspace_switch = true;
    bool auto_float_fixed_size = true; // Auto-float non-resizable dialogs/popups
};

struct WallpaperConfig {
    std::string current_wallpaper = "aurora_minimal.png";
    std::string wallpaper_path = "~/Pictures/Wallpapers/aurora_minimal.png";
    std::string fit_mode = "Cover";
    std::vector<std::string> fit_modes = {"Cover", "Fit", "Stretch", "Center", "Tile"};
    std::string display_aspect = "16:9";
};

struct KeybindingEntry {
    std::string action;
    std::string description;
    std::vector<std::string> keys;
};

class SettingItem {
public:
    std::string id;
    std::string title;
    // Role-specific value or default app candidate name (e.g. "Firefox").
    // Note: Navigation pillars (Parent, Current, Category Overview) do NOT render
    // subtitles to enforce clean navigation hierarchy and ensure zero eager querying.
    std::string subtitle;
    std::string icon;
    bool is_category = false;
    std::vector<std::shared_ptr<SettingItem>> children;
    std::function<std::vector<std::shared_ptr<SettingItem>>()> children_provider = nullptr;

    void ensure_children() {
        if (children_provider) {
            auto provider = std::move(children_provider);
            children_provider = nullptr;
            children = provider();
        }
    }

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
    bool slider_read_only = false;

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

    bool is_private_dns = false;
    PrivateDnsConfig private_dns_cfg;

    bool is_vpn = false;
    VpnConfig vpn_cfg;

    bool is_battery_info = false;
    bool is_power_mode = false;
    std::shared_ptr<BatteryConfig> battery_cfg = std::make_shared<BatteryConfig>();

    bool is_firewall = false;
    FirewallConfig firewall_cfg;

    bool is_per_app_network = false;

    bool is_display_brightness = false;
    float brightness_val = 0.85f;

    bool is_timeout_and_lock = false;
    ScreenTimeoutLockConfig timeout_lock_cfg;

    bool is_display_nightlight = false;
    NightLightConfig nightlight_cfg;

    bool is_tiling_config = false;
    std::shared_ptr<TilingConfig> tiling_cfg = std::make_shared<TilingConfig>();

    bool is_wallpaper = false;
    std::shared_ptr<WallpaperConfig> wallpaper_cfg = std::make_shared<WallpaperConfig>();

    bool is_keybinding_group = false;
    std::vector<KeybindingEntry> keybindings;

    bool is_input_keyboards = false;
    bool is_input_repeat = false;
    bool is_input_pointers = false;
    bool is_input_touch = false;

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
