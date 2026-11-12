#include "settings_model.hpp"
#include "wifi_backend.hpp"
#include "bluetooth_backend.hpp"
#include "display_backend.hpp"
#include "ethernet_backend.hpp"
#include "firewall_backend.hpp"
#include "airplane_backend.hpp"
#include "dns_backend.hpp"
#include "monitor_backend.hpp"
#include "apps_backend.hpp"
#include "xdg_backend.hpp"
#include "battery_backend.hpp"

namespace miqu {

std::shared_ptr<SettingItem> SettingItem::create_category(
    std::string id, std::string title, std::string icon,
    std::vector<std::shared_ptr<SettingItem>> children
) {
    auto item = std::make_shared<SettingItem>();
    item->id = std::move(id);
    item->title = std::move(title);
    item->subtitle = "";
    item->icon = std::move(icon);
    item->is_category = true;
    item->children = std::move(children);
    return item;
}

std::shared_ptr<SettingItem> SettingItem::create_leaf(
    std::string id, std::string title, std::string subtitle, std::string icon
) {
    auto item = std::make_shared<SettingItem>();
    item->id = std::move(id);
    item->title = std::move(title);
    item->subtitle = std::move(subtitle);
    item->icon = std::move(icon);
    item->is_category = false;
    return item;
}

std::vector<std::shared_ptr<SettingItem>> SettingsModel::build_default_tree() {
    std::vector<std::shared_ptr<SettingItem>> root;

    // ==============================================================================
    // 1. Connectivity Category (Depth 1)
    // ==============================================================================
    {
        // 1.1 Wi-Fi (Direct Leaf node with interactive UI)
        auto wifi = SettingItem::create_leaf("net_wifi", "Wi-Fi", "", "󰖩");
        wifi->has_switch = true;
        wifi->switch_val = true;
        wifi->switch_label = "Wi-Fi";
        wifi->wifi_networks = {
            {"Miqu-Fiber-5G", "Connected", "󰤨", true, "E4:70:B8:52:2F:BF", 92, "5 GHz", "36", "WPA3", "1200 Mbit/s", "WPA3 • 5 GHz (Ch 36) • 92% • 1200 Mbit/s • E4:70:B8:52:2F:BF"},
            {"Home-Mesh-Office", "Saved", "󰤥", true, "A0:B1:C2:D3:E4:F5", 78, "5 GHz", "44", "WPA2", "866 Mbit/s", "WPA2 • 5 GHz (Ch 44) • 78% • 866 Mbit/s • A0:B1:C2:D3:E4:F5"},
            {"Miqu-Lab-Secure", "Saved", "󰤢", true, "12:34:56:78:9A:BC", 60, "5 GHz", "149", "WPA2 Enterprise", "540 Mbit/s", "WPA2 Enterprise • 5 GHz (Ch 149) • 60% • 540 Mbit/s • 12:34:56:78:9A:BC"},
            {"Cafe-Express-WiFi", "Connect", "󰤨", false, "FE:DC:BA:98:76:54", 85, "2.4 GHz", "6", "Open", "144 Mbit/s", "Open • 2.4 GHz (Ch 6) • 85% • 144 Mbit/s • FE:DC:BA:98:76:54"},
            {"Airport-Free-5G", "Connect", "󰤢", false, "00:11:22:33:44:55", 52, "5 GHz", "157", "Open", "300 Mbit/s", "Open • 5 GHz (Ch 157) • 52% • 300 Mbit/s • 00:11:22:33:44:55"}
        };
        wifi->telemetry = {
            {"Interface", "wlan0 (Intel Wi-Fi 6 AX200)"},
            {"MAC Address", "a4:bb:6d:42:ee:18"},
            {"IPv4 Address", "192.168.1.104"},
            {"Link Speed", "866 Mbps (5 GHz, 80 MHz)"},
            {"Signal Strength", "-46 dBm (Excellent)"}
        };

        // 1.2 Bluetooth (Leaf)
        auto bt = SettingItem::create_leaf("net_bluetooth", "Bluetooth", "", "󰂯");
        bt->has_switch = true;
        bt->switch_val = true;
        bt->switch_label = "Bluetooth";
        bt->bt_devices = {
            {"Sony WH-1000XM4", "Connected", "󰋋", "80%"},
            {"Logitech MX Master 3S", "Paired", "󰍽", ""},
            {"Keychron K2 Pro", "Paired", "󰌌", ""},
            {"DualSense Wireless Controller", "Tap to pair", "󰊴", ""}
        };
        bt->telemetry = {
            {"Controller", "hci0 (Intel AX200 Bluetooth 5.2)"},
            {"BD_ADDR", "DC:41:A9:8E:22:1B"},
            {"Active Codec", "Sony LDAC (96 kHz / 24-bit)"},
            {"Profiles", "A2DP Sink, AVRCP, HFP/HSP"}
        };

        // 1.3 Ethernet (Leaf)
        auto eth = SettingItem::create_leaf("net_ethernet", "Ethernet", "", "󰈀");
        eth->eth_profile = {
            "Wired Connection 1",
            "Connected",
            "192.168.1.105",
            "255.255.255.0",
            "192.168.1.1",
            "1.1.1.1, 8.8.8.8",
            "Automatic (DHCP)",
            "enp0s31f6",
            "e4:54:e8:1b:9a:2c",
            "24",
            "",
            1000,
            "Full Duplex",
            true,
            true,
            1500
        };
        eth->telemetry = {
            {"Interface", "enp0s31f6"},
            {"MAC Address", "e4:54:e8:1b:9a:2c"},
            {"Carrier Status", "Physical Link Active"},
            {"Link Speed", "1000 Mbps (Full Duplex)"},
            {"MTU", "1500 bytes"}
        };

        // 1.4 VPN (Leaf)
        auto vpn = SettingItem::create_leaf("net_vpn", "VPN", "", "󰖂");
        vpn->is_vpn = true;
        vpn->has_switch = true;
        vpn->switch_label = "VPN Tunnel";
        vpn->telemetry_title = "VPN tunnel specifications";

        // 1.5 Airplane Mode (Leaf)
        auto airplane = SettingItem::create_leaf("net_airplane", "Airplane Mode", "", "󰀝");
        airplane->has_switch = true;
        airplane->switch_label = "Airplane Mode";

        // 1.6 Private DNS (Leaf)
        auto private_dns = SettingItem::create_leaf("net_private_dns", "Private DNS", "", "󰒍");
        private_dns->is_private_dns = true;
        private_dns->telemetry_title = "DNS & encryption specifications";

        // 1.7 Firewall (Leaf)
        auto firewall = SettingItem::create_leaf("net_firewall", "Firewall", "", "󰒃");
        firewall->is_firewall = true;
        firewall->firewall_cfg.enabled = true;
        firewall->firewall_cfg.incoming_policy = "Drop (Deny)";
        firewall->firewall_cfg.outgoing_policy = "Allow";
        firewall->firewall_cfg.selected_preset = "Home";
        firewall->firewall_cfg.allow_kdeconnect = true;
        firewall->firewall_cfg.allow_ssh = false;
        firewall->firewall_cfg.allow_samba = false;
        firewall->firewall_cfg.allow_cups = true;
        firewall->firewall_cfg.monitor_enabled = true;

        // 1.8 Per App Network Access (Leaf)
        auto per_app_net = SettingItem::create_leaf("net_per_app", "Per App Network Access", "", "󰀻");
        per_app_net->is_per_app_network = true;

        root.push_back(SettingItem::create_category(
            "cat_connectivity", "Connectivity", "󰖩",
            {wifi, bt, eth, vpn, airplane, private_dns, firewall, per_app_net}
        ));
    }

    // ==============================================================================
    // 2. Window Management (Depth 1)
    // ==============================================================================
    {
        auto shared_tiling = std::make_shared<TilingConfig>();

        // 2.1 Layout Mode
        auto tile_mode = SettingItem::create_leaf("tile_mode", "Layout Mode", "", "󰕰");
        tile_mode->is_tiling_config = true;
        tile_mode->tiling_cfg = shared_tiling;

        // 2.2 Windows
        auto tile_windows = SettingItem::create_leaf("tile_windows", "Windows", "", "󰃟");
        tile_windows->is_tiling_config = true;
        tile_windows->tiling_cfg = shared_tiling;

        // 2.3 Dwindle (BSP) Tuning
        auto tile_split = SettingItem::create_leaf("tile_split", "Dwindle (BSP) Tuning", "", "󰤻");
        tile_split->is_tiling_config = true;
        tile_split->tiling_cfg = shared_tiling;

        // 2.4 Focus Behaviour
        auto tile_focus = SettingItem::create_leaf("tile_focus", "Focus Behaviour", "", "󰍽");
        tile_focus->is_tiling_config = true;
        tile_focus->tiling_cfg = shared_tiling;

        root.push_back(SettingItem::create_category(
            "cat_tiling", "Window Management", "󰕰",
            {tile_mode, tile_windows, tile_split, tile_focus}
        ));
    }

    // ==============================================================================
    // 3. Keybindings (Depth 1)
    // ==============================================================================
    {
        // 3.1 System & Session
        auto kb_system = SettingItem::create_leaf("kb_system", "System & Session", "", "󰐥");
        kb_system->is_keybinding_group = true;
        kb_system->keybindings = {
            {"Power Menu", "Open system shutdown, restart and suspend dialog", {"Super", "Shift", "Q"}},
            {"Lock Screen", "Lock session immediately via Wayland lock protocol", {"Super", "Alt", "L"}},
            {"Reload Compositor", "Reload configuration and dynamic theme files without restart", {"Super", "Shift", "R"}},
            {"Toggle Overview", "Interactive live workspace expose and window switcher", {"Super", "Tab"}},
            {"Notification Center", "Toggle slide-out notification drawer and history", {"Super", "N"}}
        };

        // 3.2 Core Applications
        auto kb_apps = SettingItem::create_leaf("kb_apps", "Applications", "", "󰀻");
        kb_apps->is_keybinding_group = true;
        kb_apps->keybindings = {
            {"Terminal", "Spawn default terminal emulator (Kitty)", {"Super", "T"}},
            {"Application Launcher", "Search and launch desktop applications", {"Super", "Space"}},
            {"Universal Search", "Search commands, web, and system actions", {"Super", "S"}},
            {"Web Browser", "Launch default web browser (Firefox)", {"Super", "F"}},
            {"File Manager", "Open terminal file manager (Yazi)", {"Super", "E"}},
            {"Component Sandbox", "Interactive toolkit test sandbox (Miqu Test)", {"Super", "P"}}
        };

        // 3.3 Window Management
        auto kb_windows = SettingItem::create_leaf("kb_windows", "Window Management", "", "󰕰");
        kb_windows->is_keybinding_group = true;
        kb_windows->keybindings = {
            {"Close Window", "Gracefully close the currently active focused window", {"Super", "Q"}},
            {"Toggle Floating", "Toggle window between floating and tiled layout modes", {"Super", "V"}},
            {"Toggle Fullscreen", "Expand window to fill display area", {"Super", "M"}},
            {"Toggle Layout", "Cycle through available tiling layouts (Dwindle, Stack, etc.)", {"Super", "L"}},
            {"Swap with Master", "Promote active window to dominant master position", {"Super", "Return"}},
            {"Toggle Split", "Toggle horizontal or vertical binary space partitioning", {"Super", "Alt", "Space"}}
        };

        // 3.4 Navigation & Workspaces
        auto kb_nav = SettingItem::create_leaf("kb_nav", "Navigation & Workspaces", "", "󰍹");
        kb_nav->is_keybinding_group = true;
        kb_nav->keybindings = {
            {"Next Window", "Shift keyboard focus to next window in current workspace", {"Super", "J"}},
            {"Previous Window", "Shift keyboard focus to previous window", {"Super", "K"}},
            {"Next Workspace", "Switch to adjacent next workspace", {"Super", "Shift", "Right"}},
            {"Previous Workspace", "Switch to adjacent previous workspace", {"Super", "Shift", "Left"}},
            {"Switch to Workspace 1..9", "Jump directly to numbered workspace", {"Super", "Shift", "1..9"}},
            {"Move Window to Workspace 1..9", "Send focused window to numbered workspace", {"Super", "Alt", "1..9"}}
        };

        root.push_back(SettingItem::create_category(
            "cat_keybindings", "Keybindings", "󰌌",
            {kb_system, kb_apps, kb_windows, kb_nav}
        ));
    }

    // ==============================================================================
    // 3. Monitors & Displays (Depth 1)
    // ==============================================================================
    {
        auto cat_monitors = SettingItem::create_category("cat_monitors", "Monitors", "󰍹");
        cat_monitors->children_provider = []() {
            std::vector<std::shared_ptr<SettingItem>> mon_items;
            auto connected_monitors = MonitorBackend::get_monitors();
            for (const auto& mon_info : connected_monitors) {
                std::string id = "mon_" + mon_info.name;
                std::string title = !mon_info.description.empty() ? mon_info.description : mon_info.name;
                auto mon_item = SettingItem::create_leaf(id, title, "", "󰍹");
                MonitorBackend::sync_monitor_item(mon_item, mon_info);
                mon_items.push_back(mon_item);
            }

            if (mon_items.empty()) {
                auto def_item = SettingItem::create_leaf("mon_edp1", "eDP-1", "", "󰍹");
                MonitorInfo def_info;
                MonitorBackend::sync_monitor_item(def_item, def_info);
                mon_items.push_back(def_item);
            }
            return mon_items;
        };

        root.push_back(cat_monitors);
    }

    // ==============================================================================
    // 3. Applications (Depth 1)
    // ==============================================================================
    {
        // 3.1 Desktop Applications (Category)
        auto cat_desktop_apps = SettingItem::create_category(
            "cat_desktop_apps", "Desktop Applications", "󰀻"
        );
        cat_desktop_apps->children_provider = []() {
            std::vector<std::shared_ptr<SettingItem>> desktop_items = AppsBackend::get_desktop_app_items();
            if (desktop_items.empty()) {
                auto app_firefox = SettingItem::create_leaf("app_firefox", "Firefox Web Browser", "", "󰈹");
                app_firefox->is_installed_app = true;
                app_firefox->app_info.app_name = "Firefox Web Browser";
                app_firefox->app_info.desktop_file = "/usr/share/applications/firefox.desktop";
                app_firefox->app_info.binary_path = "/usr/bin/firefox";
                desktop_items.push_back(app_firefox);
            }
            return desktop_items;
        };

        // 3.2 System Packages (Category)
        auto cat_system_packages = SettingItem::create_category(
            "cat_system_packages", "System Packages", "󰒓"
        );
        cat_system_packages->children_provider = []() {
            return AppsBackend::get_system_package_items();
        };

        // 3.2 Default Applications (Category)
        auto def_browser = SettingItem::create_leaf("def_browser", "Web Browser", XdgBackend::get_default_app_name("text/html", "Firefox"), "󰈹");
        def_browser->is_default_app_role = true;
        def_browser->default_app_role = {
            "Web Browser", "text/html, x-scheme-handler/http, x-scheme-handler/https",
            {}, "text/html", false
        };
        def_browser->telemetry = {
            {"MIME Types", "text/html, x-scheme-handler/http, x-scheme-handler/https"},
            {"Config File", "~/.config/mimeapps.list"},
            {"Specification", "XDG MIME Applications Specification v1.4"}
        };

        auto def_filemanager = SettingItem::create_leaf("def_filemanager", "File Manager", XdgBackend::get_default_app_name("inode/directory", "Miqu Files"), "󰉋");
        def_filemanager->is_default_app_role = true;
        def_filemanager->default_app_role = {
            "File Manager", "inode/directory",
            {}, "inode/directory", false
        };
        def_filemanager->telemetry = {
            {"MIME Types", "inode/directory"},
            {"Config File", "~/.config/mimeapps.list"},
            {"Specification", "XDG MIME Applications Specification v1.4"}
        };

        auto def_pdf = SettingItem::create_leaf("def_pdf", "PDF Reader", XdgBackend::get_default_app_name("application/pdf", "Document Viewer"), "󰈦");
        def_pdf->is_default_app_role = true;
        def_pdf->default_app_role = {
            "PDF Reader", "application/pdf",
            {}, "application/pdf", false
        };
        def_pdf->telemetry = {
            {"MIME Types", "application/pdf"},
            {"Config File", "~/.config/mimeapps.list"},
            {"Specification", "XDG MIME Applications Specification v1.4"}
        };

        auto def_image = SettingItem::create_leaf("def_image", "Image Viewer", XdgBackend::get_default_app_name("image/png", "Gallery"), "󰋩");
        def_image->is_default_app_role = true;
        def_image->default_app_role = {
            "Image Viewer", "image/png, image/jpeg, image/webp, image/gif, image/svg+xml",
            {}, "image/png", false
        };
        def_image->telemetry = {
            {"MIME Types", "image/png, image/jpeg, image/webp, image/gif, image/svg+xml"},
            {"Config File", "~/.config/mimeapps.list"},
            {"Specification", "XDG MIME Applications Specification v1.4"}
        };

        auto def_editor = SettingItem::create_leaf("def_editor", "Text Editor", XdgBackend::get_default_app_name("text/plain", "Geany"), "󰨞");
        def_editor->is_default_app_role = true;
        def_editor->default_app_role = {
            "Text Editor", "text/plain, text/markdown",
            {}, "text/plain", false
        };
        def_editor->telemetry = {
            {"MIME Types", "text/plain, text/markdown"},
            {"Config File", "~/.config/mimeapps.list"},
            {"Specification", "XDG MIME Applications Specification v1.4"}
        };

        auto def_video = SettingItem::create_leaf("def_video", "Video Player", XdgBackend::get_default_app_name("video/mp4", "Celluloid"), "󰕼");
        def_video->is_default_app_role = true;
        def_video->default_app_role = {
            "Video Player", "video/mp4, video/mkv, video/webm, video/x-matroska",
            {}, "video/mp4", false
        };
        def_video->telemetry = {
            {"MIME Types", "video/mp4, video/mkv, video/webm, video/x-matroska"},
            {"Config File", "~/.config/mimeapps.list"},
            {"Specification", "XDG MIME Applications Specification v1.4"}
        };

        auto def_music = SettingItem::create_leaf("def_music", "Music Player", XdgBackend::get_default_app_name("audio/mpeg", "Miqu Music"), "󰝚");
        def_music->is_default_app_role = true;
        def_music->default_app_role = {
            "Music Player", "audio/mpeg, audio/flac, audio/ogg, audio/wav",
            {}, "audio/mpeg", false
        };
        def_music->telemetry = {
            {"MIME Types", "audio/mpeg, audio/flac, audio/ogg, audio/wav"},
            {"Config File", "~/.config/mimeapps.list"},
            {"Specification", "XDG MIME Applications Specification v1.4"}
        };

        auto def_mail = SettingItem::create_leaf("def_mail", "Mail Reader", XdgBackend::get_default_app_name("x-scheme-handler/mailto", "Thunderbird"), "󰇮");
        def_mail->is_default_app_role = true;
        def_mail->default_app_role = {
            "Mail Reader", "x-scheme-handler/mailto, message/rfc822",
            {}, "x-scheme-handler/mailto", false
        };
        def_mail->telemetry = {
            {"MIME Types", "x-scheme-handler/mailto, message/rfc822"},
            {"Config File", "~/.config/mimeapps.list"},
            {"Specification", "XDG MIME Applications Specification v1.4"}
        };

        auto def_terminal = SettingItem::create_leaf("def_terminal", "Terminal Emulator", "Kitty", "󰄛");
        def_terminal->is_default_app_role = true;
        def_terminal->default_app_role = {
            "Terminal Emulator", "x-scheme-handler/terminal",
            {}, "x-scheme-handler/terminal", false
        };
        def_terminal->telemetry = {
            {"MIME Types", "x-scheme-handler/terminal"},
            {"Config File", "~/.config/mimeapps.list"},
            {"Specification", "XDG MIME Applications Specification v1.4"}
        };

        auto cat_default_apps = SettingItem::create_category(
            "cat_default_apps", "Default Applications", "󰆍",
            {def_browser, def_filemanager, def_pdf, def_image, def_editor, def_video, def_music, def_mail, def_terminal}
        );

        root.push_back(SettingItem::create_category(
            "cat_applications", "Applications", "󰀻",
            {cat_desktop_apps, cat_system_packages, cat_default_apps}
        ));
    }

    // ==============================================================================
    // 5. Display (Depth 1)
    // ==============================================================================
    {
        // 5.1 Brightness
        auto disp_bright = SettingItem::create_leaf("disp_bright", "Brightness", "", "󰃠");
        disp_bright->is_display_brightness = true;
        disp_bright->brightness_val = 0.85f;
        disp_bright->telemetry_title = "Backlight & panel specifications";
        disp_bright->telemetry = {
            {"Backlight Interface", "intel_backlight (/sys/class/backlight)"},
            {"Current Level", "217 / 255 steps (85%)"},
            {"Minimum Brightness", "1% (Prevents full blackout)"},
            {"PWM Frequency", "20000 Hz (Flicker-Free)"}
        };

        // 5.2 Screen Timeout & Lock (Unified Inactivity & Security)
        auto disp_timeout_lock = SettingItem::create_leaf("disp_timeout_lock", "Screen Timeout & Lock", "", "󰌾");
        disp_timeout_lock->is_timeout_and_lock = true;
        disp_timeout_lock->timeout_lock_cfg.lock_timeout_minutes = 1;
        disp_timeout_lock->timeout_lock_cfg.display_off_minutes = 5;
        disp_timeout_lock->timeout_lock_cfg.sleep_minutes = 15;
        disp_timeout_lock->timeout_lock_cfg.hibernate_delay_minutes = 120;
        disp_timeout_lock->timeout_lock_cfg.sleep_mode = "Hybrid Sleep (Recommended)";
        disp_timeout_lock->timeout_lock_cfg.lid_action = "Sleep";
        disp_timeout_lock->timeout_lock_cfg.power_button_action = "Show power menu (miqupower)";
        disp_timeout_lock->timeout_lock_cfg.critical_battery_action = "Hibernate";
        disp_timeout_lock->timeout_lock_cfg.dim_screen = true;
        disp_timeout_lock->timeout_lock_cfg.prevent_sleep_media = true;
        disp_timeout_lock->timeout_lock_cfg.energy_saver_battery = true;
        disp_timeout_lock->timeout_lock_cfg.clamshell_mode = true;
        disp_timeout_lock->timeout_lock_cfg.lock_before_sleep = true;
        disp_timeout_lock->timeout_lock_cfg.lock_on_display_off = true;
        disp_timeout_lock->timeout_lock_cfg.require_password_immediately = true;
        disp_timeout_lock->timeout_lock_cfg.grace_period_seconds = 0;
        disp_timeout_lock->timeout_lock_cfg.lockscreen_show_power_actions = true;
        disp_timeout_lock->timeout_lock_cfg.lockscreen_24h_clock = true;
        disp_timeout_lock->telemetry_title = "Inactivity & security specifications";
        disp_timeout_lock->telemetry = {
            {"Inactivity Manager", "miquidle (ext-idle-notify-v1)"},
            {"Screen Locker", "miqulock (ext-session-lock-v1)"},
            {"Display DPMS", "wlr-output-power-management-v1"},
            {"Sleep / Suspend Daemon", "systemd-logind (suspend-then-hibernate)"},
            {"Lid & Power Keys", "/etc/systemd/logind.conf"},
            {"Battery & Power Events", "UPower & systemd-udev"},
            {"Clamshell Docking", "Monitored via seat0 & DRM connectors"},
            {"Authentication Provider", "PAM (system-auth)"}
        };

        // 5.3 Night Light
        auto disp_nightlight = SettingItem::create_leaf("disp_nightlight", "Night Light", "", "󰃟");
        disp_nightlight->is_display_nightlight = true;
        disp_nightlight->telemetry_title = "Gamma & color specifications";

        root.push_back(SettingItem::create_category(
            "cat_display", "Display", "󰍹",
            {disp_bright, disp_timeout_lock, disp_nightlight}
        ));
    }

    // ==============================================================================
    // 6. Wallpaper & Style (Depth 1)
    // ==============================================================================
    {
        auto ws_wallpaper = SettingItem::create_leaf("ws_wallpaper", "Wallpaper", "", "󰸉");
        ws_wallpaper->is_wallpaper = true;

        auto ws_colors = SettingItem::create_leaf("ws_colors", "Color Theme & Palette", "", "󰔎");
        ws_colors->has_switch = true;
        ws_colors->switch_val = true;
        ws_colors->switch_label = "Dark Appearance";
        ws_colors->has_secondary_switch = true;
        ws_colors->secondary_switch_val = true;
        ws_colors->secondary_switch_label = "Extract Dynamic Matugen Palette";
        ws_colors->has_slider = true;
        ws_colors->slider_val = 0.45f;
        ws_colors->slider_label = "Palette Warmth Tint (45%)";
        ws_colors->slider_unit = "%";
        ws_colors->has_action = true;
        ws_colors->action_label = "Regenerate Palette (Matugen)";
        ws_colors->telemetry_title = "Color palette specifications";
        ws_colors->telemetry = {
            {"Active Theme", "Miqu Dark Material 3"},
            {"Primary Accent", "#3584e4 (Blue Sapphire)"},
            {"Surface Tint", "#1e1e2e (Catppuccin Mocha)"},
            {"Live Reload", "miqutheme (Dynamic inotify sync)"}
        };

        root.push_back(SettingItem::create_category(
            "cat_wallpaper_style", "Wallpaper & Style", "󰸉",
            {ws_wallpaper, ws_colors}
        ));
    }

    // ==============================================================================
    // 3. Sound (Depth 1)
    // ==============================================================================
    {
        // 3.1 Speaker (Leaf)
        auto speaker = SettingItem::create_leaf("sound_speaker", "Speaker", "", "󰕾");
        speaker->has_slider = true;
        speaker->slider_val = 0.70f;
        speaker->slider_label = "Volume";
        speaker->slider_unit = "%";
        speaker->has_switch = true;
        speaker->switch_val = false;
        speaker->switch_label = "Mute";
        speaker->has_secondary_switch = false;
        speaker->has_action = false;
        speaker->telemetry = {
            {"Device", "ALC1220 High Definition Audio"},
            {"Audio Server", "PipeWire 1.0 (PulseAudio/ALSA)"},
            {"Sample Rate", "48.0 kHz / 32-bit Float Stereo"},
            {"Active Port", "Line Out (Green Rear)"}
        };

        // 3.2 Mic (Leaf)
        auto mic = SettingItem::create_leaf("sound_mic", "Mic", "", "󰍬");
        mic->has_slider = true;
        mic->slider_val = 0.82f;
        mic->slider_label = "Gain";
        mic->slider_unit = "%";
        mic->has_switch = true;
        mic->switch_val = false;
        mic->switch_label = "Mute";
        mic->has_secondary_switch = false;
        mic->has_action = false;
        mic->telemetry = {
            {"Device", "Blue Yeti Pro USB"},
            {"Audio Server", "PipeWire 1.0 (PulseAudio)"},
            {"Format", "24-bit / 48.0 kHz Cardioid"},
            {"Noise Floor", "-54 dBFS"}
        };

        root.push_back(SettingItem::create_category(
            "cat_audio", "Sound", "󰕾",
            {speaker, mic}
        ));
    }

    // ==============================================================================
    // 9. Keyboard & Mouse (Depth 1)
    // ==============================================================================
    {
        // 9.1 Keyboards & Layout (Leaf)
        auto in_kb = SettingItem::create_leaf("input_keyboards", "Keyboards & Layout", "", "󰌌");
        in_kb->is_input_keyboards = true;
        in_kb->telemetry_title = "Hardware keyboard specifications";

        // 9.2 Typing & Repeat Speed (Leaf)
        auto in_rep = SettingItem::create_leaf("input_repeat", "Typing & Repeat Speed", "", "󰁯");
        in_rep->is_input_repeat = true;
        in_rep->telemetry_title = "Typing & repeat specifications";

        // 9.3 Pointers & Touchpad (Leaf)
        auto in_ptr = SettingItem::create_leaf("input_pointers", "Pointers & Touchpad", "", "󰍽");
        in_ptr->is_input_pointers = true;
        in_ptr->telemetry_title = "Pointing device specifications";

        // 9.4 Touchscreen & Gestures (Leaf)
        auto in_touch = SettingItem::create_leaf("input_touch", "Touchscreen & Gestures", "", "󰟸");
        in_touch->is_input_touch = true;
        in_touch->telemetry_title = "Digitizer & gesture specifications";

        root.push_back(SettingItem::create_category(
            "cat_input", "Keyboard & Mouse", "󰌌",
            {in_kb, in_rep, in_ptr, in_touch}
        ));
    }

    // ==============================================================================
    // 8. Storage (Depth 1)
    // ==============================================================================
    {
        auto stor_root = SettingItem::create_leaf("stor_root", "Root Filesystem (/)", "", "󰋊");
        stor_root->has_slider = true;
        stor_root->slider_val = 0.42f;
        stor_root->slider_label = "Storage Used (42% • 420.5 GB of 1.0 TB)";
        stor_root->slider_unit = "%";
        stor_root->slider_read_only = true;
        stor_root->has_switch = true;
        stor_root->switch_val = true;
        stor_root->switch_label = "Periodic SSD TRIM (fstrim.timer)";
        stor_root->has_secondary_switch = true;
        stor_root->secondary_switch_val = true;
        stor_root->secondary_switch_label = "ZSTD Transparent Compression";
        stor_root->has_action = true;
        stor_root->action_label = "Run Drive Health Diagnostic (SMART)";
        stor_root->telemetry_title = "Block device & filesystem specifications";
        stor_root->telemetry = {
            {"Filesystem Type", "Btrfs (rw, noatime, compress=zstd:3)"},
            {"Drive Model", "Samsung SSD 990 PRO 1TB NVMe"},
            {"Total Capacity", "1,000.2 GB (931.5 GiB)"},
            {"Used Space", "420.5 GB (42%)"},
            {"Available Space", "579.7 GB (58%)"},
            {"SMART Health Status", "PASSED (100% remaining lifetime)"}
        };

        auto stor_home = SettingItem::create_leaf("stor_home", "User Data (~/)", "", "󰉋");
        stor_home->has_slider = true;
        stor_home->slider_val = 0.28f;
        stor_home->slider_label = "User Directory Space (280.4 GB)";
        stor_home->slider_unit = "%";
        stor_home->slider_read_only = true;
        stor_home->has_switch = true;
        stor_home->switch_val = true;
        stor_home->switch_label = "Automatic Cache Cleaner";
        stor_home->has_secondary_switch = false;
        stor_home->has_action = true;
        stor_home->action_label = "Clean Package & App Cache";
        stor_home->telemetry_title = "Directory usage specifications";
        stor_home->telemetry = {
            {"Mount Point", "/home/anisur"},
            {"Documents & Code", "54.2 GB"},
            {"Pictures & Videos", "112.8 GB"},
            {"Downloads", "24.6 GB"},
            {"Application Cache (~/.cache)", "18.4 GB"},
            {"Snapshots / Backups", "70.4 GB"}
        };

        root.push_back(SettingItem::create_category(
            "cat_storage", "Storage", "󰋊",
            {stor_root, stor_home}
        ));
    }



    // ==============================================================================
    // 7. Battery (Depth 1)
    // ==============================================================================
    {
        auto shared_battery_cfg = std::make_shared<BatteryConfig>();

        // 7.1 Battery Info (Leaf) - loaded lazily on visibility
        auto battery_info = SettingItem::create_leaf("bat_info", "Battery Info", "", "󰁹");
        battery_info->is_battery_info = true;
        battery_info->battery_cfg = shared_battery_cfg;
        battery_info->telemetry_title = "Battery specifications";

        // 7.2 Power & Performance Mode (Leaf) - loaded lazily on visibility
        auto power_mode = SettingItem::create_leaf("bat_power_mode", "Power & Performance Mode", "Balanced", "󰌪");
        power_mode->is_power_mode = true;
        power_mode->battery_cfg = shared_battery_cfg;
        power_mode->telemetry_title = "Power management specifications";

        auto cat_battery = SettingItem::create_category(
            "cat_battery", "Battery", "󰁹",
            {battery_info, power_mode}
        );

        root.push_back(cat_battery);
    }

    // ==============================================================================
    // 10. System (Depth 1)
    // ==============================================================================
    {
        // 10.1 About (Leaf)
        auto sys_about = SettingItem::create_leaf("sys_about", "About", "", "󰋽");
        sys_about->has_action = true;
        sys_about->action_label = "Check for Ecosystem Updates";
        sys_about->telemetry_title = "System specifications";
        sys_about->telemetry = {
            {"Distribution", "Arch Linux (Rolling Release)"},
            {"Linux Kernel", "6.11.7-arch1-1"},
            {"Desktop Compositor", "miquland (C++20)"},
            {"Toolkit Version", "miqutoolkit 1.0"},
            {"Hardware Architecture", "x86_64 (Zen 4 / AMD Ryzen)"},
            {"Memory (RAM)", "32.0 GB LPDDR5-6400"},
            {"Firmware / UEFI", "UEFI v2.8 (Secure Boot: Active)"}
        };

        root.push_back(SettingItem::create_category(
            "cat_system", "System", "󰒓",
            {sys_about}
        ));
    }

    return root;
}

} // namespace miqu
