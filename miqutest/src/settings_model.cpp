#include "settings_model.hpp"
#include "wifi_backend.hpp"
#include "display_backend.hpp"
#include "ethernet_backend.hpp"

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

        if (WifiBackend::is_wifi_available()) {
            WifiBackend::sync_item_state(wifi);
        }

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
        eth->has_switch = true;
        eth->switch_val = true;
        eth->switch_label = "Ethernet";
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

        if (EthernetBackend::is_networkmanager_available()) {
            EthernetBackend::sync_item_state(eth);
        }

        // 1.4 Airplane Mode (Leaf)
        auto airplane = SettingItem::create_leaf("net_airplane", "Airplane Mode", "", "󰀝");
        airplane->has_switch = true;
        airplane->switch_val = false;
        airplane->switch_label = "Airplane Mode";
        airplane->telemetry = {
            {"Wi-Fi Radio", "Enabled"},
            {"Bluetooth Radio", "Enabled"},
            {"Cellular Radio", "Disabled"}
        };

        // 1.5 Private DNS (Leaf)
        auto private_dns = SettingItem::create_leaf("net_private_dns", "Private DNS", "", "󰒍");
        private_dns->is_private_dns = true;
        private_dns->private_dns_cfg.selected_option = "Cloudflare (1.1.1.1)";
        private_dns->private_dns_cfg.custom_hostname = "dns.nextdns.io";
        private_dns->telemetry_title = "DNS & encryption specifications";
        private_dns->telemetry = {
            {"Protocol Standard", "DNS-over-TLS (RFC 7858 / Port 853)"},
            {"Active Resolver", "1.1.1.1#one.one.one.one"},
            {"Transport Security", "TLSv1.3 (ChaCha20-Poly1305 / X25519)"},
            {"System Resolver", "systemd-resolved.service"},
            {"DNSSEC Validation", "Enabled (Strict verification)"},
            {"Resolution Latency", "4.2 ms (Cached / Encrypted)"}
        };

        // 1.6 Firewall (Leaf)
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

        // 1.7 Per App Network Access (Leaf)
        auto per_app_net = SettingItem::create_leaf("net_per_app", "Per App Network Access", "", "󰀻");
        per_app_net->is_per_app_network = true;

        root.push_back(SettingItem::create_category(
            "cat_connectivity", "Connectivity", "󰖩",
            {wifi, bt, eth, airplane, private_dns, firewall, per_app_net}
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
        // 2.1 Monitor 1 (Primary 4K 144Hz)
        auto mon1 = SettingItem::create_leaf("mon_dp1", "Dell U2723QE", "", "󰍹");
        mon1->is_monitor = true;
        mon1->monitor_cfg.connector = "DP-1";
        mon1->monitor_cfg.model_name = "Dell UltraSharp U2723QE";
        mon1->monitor_cfg.is_primary = true;
        mon1->monitor_cfg.current_resolution = "3840 × 2160";
        mon1->monitor_cfg.available_resolutions = {"3840 × 2160", "2560 × 1440", "1920 × 1080"};
        mon1->monitor_cfg.current_refresh_rate = "144 Hz";
        mon1->monitor_cfg.available_refresh_rates = {"144 Hz", "120 Hz", "60 Hz"};
        mon1->monitor_cfg.current_scale = "125%";
        mon1->monitor_cfg.available_scales = {"100%", "125%", "150%", "200%"};
        mon1->monitor_cfg.current_orientation = "Standard (0°)";
        mon1->monitor_cfg.available_orientations = {"Standard (0°)", "90° (Portrait)", "180°", "270°"};
        mon1->telemetry = {
            {"Connector", "DP-1 (DisplayPort 1.4)"},
            {"Panel Type", "27\" IPS Black (3840 × 2160)"},
            {"Color Space", "DCI-P3 98% / sRGB 100%"},
            {"HDR Support", "VESA DisplayHDR 400 (Active)"},
            {"VRR / FreeSync", "AMD FreeSync Premium (48-144 Hz)"}
        };

        // 2.2 Monitor 2 (Secondary 4K 60Hz)
        auto mon2 = SettingItem::create_leaf("mon_hdmi1", "LG UltraFine 27", "", "󰍹");
        mon2->is_monitor = true;
        mon2->monitor_cfg.connector = "HDMI-A-1";
        mon2->monitor_cfg.model_name = "LG UltraFine 27UP850";
        mon2->monitor_cfg.is_primary = false;
        mon2->monitor_cfg.current_resolution = "3840 × 2160";
        mon2->monitor_cfg.available_resolutions = {"3840 × 2160", "2560 × 1440", "1920 × 1080"};
        mon2->monitor_cfg.current_refresh_rate = "60 Hz";
        mon2->monitor_cfg.available_refresh_rates = {"60 Hz", "50 Hz", "30 Hz"};
        mon2->monitor_cfg.current_scale = "150%";
        mon2->monitor_cfg.available_scales = {"100%", "125%", "150%", "200%"};
        mon2->monitor_cfg.current_orientation = "Standard (0°)";
        mon2->monitor_cfg.available_orientations = {"Standard (0°)", "90° (Portrait)", "180°", "270°"};
        mon2->telemetry = {
            {"Connector", "HDMI-A-1 (HDMI 2.0b)"},
            {"Panel Type", "27\" IPS (3840 × 2160)"},
            {"Color Space", "DCI-P3 95% / sRGB 99%"},
            {"HDR Support", "HDR10 (Active)"},
            {"VRR / FreeSync", "AMD FreeSync (40-60 Hz)"}
        };

        root.push_back(SettingItem::create_category(
            "cat_monitors", "Monitors", "󰍹",
            {mon1, mon2}
        ));
    }

    // ==============================================================================
    // 3. Applications (Depth 1)
    // ==============================================================================
    {
        // 3.1 Installed Apps (Category)
        auto app_firefox = SettingItem::create_leaf("app_firefox", "Firefox Web Browser", "", "󰈹");
        app_firefox->is_installed_app = true;
        app_firefox->app_info = {
            "Firefox Web Browser", "132.0.2", "Mozilla Foundation",
            "Internet & Web (Network;WebBrowser)",
            "Fast, private, and open-source web browser developed by the Mozilla Foundation.",
            "/usr/bin/firefox", "/usr/share/applications/firefox.desktop",
            "Arch Linux Package (pacman)", "248.5 MB", "112.0 MB", "136.5 MB",
            "Wayland Native, PipeWire Audio, Network Access, Storage (~/)",
            true, 4821, "Allow"
        };
        app_firefox->telemetry = {
            {"Binary Architecture", "x86_64 (Linux ELF)"},
            {"Open Source License", "Mozilla Public License (MPL 2.0)"},
            {"Toolkit & Runtime", "GTK4 / Wayland / Gecko 132"},
            {"Core Dependencies", "gtk4, nss, libpulse, ffmpeg"}
        };

        auto app_vscode = SettingItem::create_leaf("app_vscode", "Visual Studio Code", "", "󰨞");
        app_vscode->is_installed_app = true;
        app_vscode->app_info = {
            "Visual Studio Code", "1.95.3", "Microsoft Corporation",
            "Developer Tools (Development;IDE)",
            "Code editing redefined. Powerful code editor with built-in debugging, Git, and extensions.",
            "/usr/bin/code", "/usr/share/applications/code.desktop",
            "Arch Linux Package (community)", "312.0 MB", "185.0 MB", "127.0 MB",
            "Wayland, FileSystem, Git Integration, IPC",
            true, 5120, "Allow"
        };
        app_vscode->telemetry = {
            {"Binary Architecture", "x86_64 (Linux ELF)"},
            {"Open Source License", "MIT License (Code - OSS)"},
            {"Toolkit & Runtime", "Electron 32 / Chromium 128 / Node 20"},
            {"Core Dependencies", "glibc, libsecret, libxkbfile"}
        };

        auto app_kitty = SettingItem::create_leaf("app_kitty", "Kitty Terminal", "", "󰄛");
        app_kitty->is_installed_app = true;
        app_kitty->app_info = {
            "Kitty Terminal", "0.37.0", "Kovid Goyal",
            "System & Terminal (System;TerminalEmulator)",
            "Fast, feature-rich, GPU-accelerated terminal emulator leveraging OpenGL for rendering.",
            "/usr/bin/kitty", "/usr/share/applications/kitty.desktop",
            "Arch Linux Package (extra)", "45.2 MB", "32.0 MB", "13.2 MB",
            "GPU OpenGL 3.3, Wayland Protocol Extensions, PTY",
            true, 3104, "Allow"
        };
        app_kitty->telemetry = {
            {"Binary Architecture", "x86_64 (Linux ELF)"},
            {"Open Source License", "GPL-3.0-or-later"},
            {"Toolkit & Runtime", "Python / C Native / OpenGL"},
            {"Core Dependencies", "fontconfig, freetype2, harfbuzz"}
        };

        auto app_gimp = SettingItem::create_leaf("app_gimp", "GIMP Image Editor", "", "󰽉");
        app_gimp->is_installed_app = true;
        app_gimp->app_info = {
            "GIMP Image Editor", "2.10.38", "The GIMP Development Team",
            "Graphics & Photography (Graphics;2DGraphics)",
            "Full-featured raster graphics editor for photo retouching, composition, and digital painting.",
            "/usr/bin/gimp-2.10", "/usr/share/applications/gimp.desktop",
            "Arch Linux Package (extra)", "185.0 MB", "95.0 MB", "90.0 MB",
            "Display, Color Profiles (ICC), Storage Access",
            false, 0, "Block"
        };
        app_gimp->telemetry = {
            {"Binary Architecture", "x86_64 (Linux ELF)"},
            {"Open Source License", "GPL-3.0-or-later"},
            {"Toolkit & Runtime", "GTK+ 2.24 / GEGL Engine"},
            {"Core Dependencies", "babl, gegl, libmypaint, poppler"}
        };

        auto app_vlc = SettingItem::create_leaf("app_vlc", "VLC Media Player", "", "󰕼");
        app_vlc->is_installed_app = true;
        app_vlc->app_info = {
            "VLC Media Player", "3.0.21", "VideoLAN Project",
            "Audio & Video (AudioVideo;Player)",
            "Universal multimedia framework and player supporting almost all codecs, discs, and network streams.",
            "/usr/bin/vlc", "/usr/share/applications/vlc.desktop",
            "Arch Linux Package (extra)", "142.8 MB", "88.0 MB", "54.8 MB",
            "VA-API / VDPAU GPU, PipeWire Audio, Removable Media",
            false, 0, "Local Only"
        };
        app_vlc->telemetry = {
            {"Binary Architecture", "x86_64 (Linux ELF)"},
            {"Open Source License", "GPL-2.0-or-later / LGPL-2.1"},
            {"Toolkit & Runtime", "Qt5 / LibVLC Multimedia Engine"},
            {"Core Dependencies", "ffmpeg, live-media, lua, taglib"}
        };

        auto app_miqumusic = SettingItem::create_leaf("app_miqumusic", "Miqu Music", "", "󰝚");
        app_miqumusic->is_installed_app = true;
        app_miqumusic->app_info = {
            "Miqu Music", "1.2.0", "Miqu Desktop Ecosystem",
            "Music & Audio (Audio;AudioPlayer)",
            "Native Wayland MPD client with Cairo vector rendering and low-latency audio playback.",
            "/usr/bin/miqumusic", "/usr/share/applications/miqumusic.desktop",
            "Native Miqu Package", "18.4 MB", "12.0 MB", "6.4 MB",
            "MPD Unix Socket, PipeWire Audio, Wayland Layer",
            true, 2410, "Local Only"
        };
        app_miqumusic->telemetry = {
            {"Binary Architecture", "x86_64 (Linux ELF)"},
            {"Open Source License", "GPL-3.0 (Miqu Ecosystem)"},
            {"Toolkit & Runtime", "miqutoolkit / C++20 / Cairo / Wayland"},
            {"Core Dependencies", "libmpdclient, miqutoolkit, pipewire"}
        };

        auto cat_installed_apps = SettingItem::create_category(
            "cat_installed_apps", "Installed Apps", "󰏖",
            {app_firefox, app_vscode, app_kitty, app_gimp, app_vlc, app_miqumusic}
        );

        // 3.2 Default Applications (Category)
        auto def_browser = SettingItem::create_leaf("def_browser", "Web Browser", "Firefox", "󰈹");
        def_browser->is_default_app_role = true;
        def_browser->default_app_role = {
            "Web Browser", "text/html, x-scheme-handler/http, x-scheme-handler/https",
            {
                {"Firefox", "󰈹", "Mozilla Firefox Web Browser", true},
                {"Chromium", "󰊯", "Open-source browser by Google", false},
                {"Brave", "󰇩", "Privacy-focused web browser", false}
            }
        };
        def_browser->telemetry = {
            {"MIME Types", "text/html, x-scheme-handler/http, x-scheme-handler/https"},
            {"Config File", "~/.config/mimeapps.list"},
            {"Specification", "XDG MIME Applications Specification v1.4"}
        };

        auto def_mail = SettingItem::create_leaf("def_mail", "Mail Reader", "Thunderbird", "󰇮");
        def_mail->is_default_app_role = true;
        def_mail->default_app_role = {
            "Mail Reader", "x-scheme-handler/mailto, message/rfc822",
            {
                {"Thunderbird", "󰇮", "Mozilla Thunderbird Mail & News", true},
                {"Betterbird", "󰇮", "Fine-tuned Thunderbird fork", false},
                {"Webmail (Firefox)", "󰈹", "Online webmail via default browser", false}
            }
        };
        def_mail->telemetry = {
            {"MIME Types", "x-scheme-handler/mailto, message/rfc822"},
            {"Config File", "~/.config/mimeapps.list"},
            {"Specification", "XDG MIME Applications Specification v1.4"}
        };

        auto def_filemanager = SettingItem::create_leaf("def_filemanager", "File Manager", "Miqu Files", "󰉋");
        def_filemanager->is_default_app_role = true;
        def_filemanager->default_app_role = {
            "File Manager", "inode/directory",
            {
                {"Miqu Files", "󰉋", "Fast Miller-columns native file manager", true},
                {"Yazi", "󰉋", "Blazing fast terminal file manager", false},
                {"Thunar", "󰉋", "Lightweight XFCE file manager", false}
            }
        };
        def_filemanager->telemetry = {
            {"MIME Types", "inode/directory"},
            {"Config File", "~/.config/mimeapps.list"},
            {"Specification", "XDG MIME Applications Specification v1.4"}
        };

        auto def_terminal = SettingItem::create_leaf("def_terminal", "Terminal Emulator", "Kitty", "󰄛");
        def_terminal->is_default_app_role = true;
        def_terminal->default_app_role = {
            "Terminal Emulator", "x-scheme-handler/terminal",
            {
                {"Kitty", "󰄛", "GPU-accelerated terminal emulator", true},
                {"Foot", "󰄛", "Fast, lightweight Wayland terminal", false},
                {"Alacritty", "󰄛", "Simple, GPU-accelerated terminal", false}
            }
        };
        def_terminal->telemetry = {
            {"MIME Types", "x-scheme-handler/terminal"},
            {"Config File", "~/.config/mimeapps.list"},
            {"Specification", "XDG MIME Applications Specification v1.4"}
        };

        auto def_editor = SettingItem::create_leaf("def_editor", "Text Editor", "Visual Studio Code", "󰨞");
        def_editor->is_default_app_role = true;
        def_editor->default_app_role = {
            "Text Editor", "text/plain, text/markdown, text/x-c++src",
            {
                {"Visual Studio Code", "󰨞", "Extensible code editor", true},
                {"Neovim", "󰄛", "Vim-fork focused on extensibility", false},
                {"Kate", "󰅪", "KDE advanced text editor", false}
            }
        };
        def_editor->telemetry = {
            {"MIME Types", "text/plain, text/markdown, text/x-c++src"},
            {"Config File", "~/.config/mimeapps.list"},
            {"Specification", "XDG MIME Applications Specification v1.4"}
        };

        auto def_music = SettingItem::create_leaf("def_music", "Music Player", "Miqu Music", "󰝚");
        def_music->is_default_app_role = true;
        def_music->default_app_role = {
            "Music Player", "audio/mpeg, audio/flac, audio/ogg",
            {
                {"Miqu Music", "󰝚", "Native Wayland MPD audio client", true},
                {"Amberol", "󰝚", "Simple music player for local files", false},
                {"VLC Media Player", "󰕼", "Universal media player", false}
            }
        };
        def_music->telemetry = {
            {"MIME Types", "audio/mpeg, audio/flac, audio/ogg"},
            {"Config File", "~/.config/mimeapps.list"},
            {"Specification", "XDG MIME Applications Specification v1.4"}
        };

        auto def_video = SettingItem::create_leaf("def_video", "Video Player", "VLC Media Player", "󰕼");
        def_video->is_default_app_role = true;
        def_video->default_app_role = {
            "Video Player", "video/mp4, video/mkv, video/webm",
            {
                {"VLC Media Player", "󰕼", "Universal multimedia player", true},
                {"MPV", "󰕼", "High-performance video player", false},
                {"Clapper", "󰕼", "Modern GNOME media player", false}
            }
        };
        def_video->telemetry = {
            {"MIME Types", "video/mp4, video/mkv, video/webm"},
            {"Config File", "~/.config/mimeapps.list"},
            {"Specification", "XDG MIME Applications Specification v1.4"}
        };

        auto cat_default_apps = SettingItem::create_category(
            "cat_default_apps", "Default Applications", "󰆍",
            {def_browser, def_mail, def_filemanager, def_terminal, def_editor, def_music, def_video}
        );

        root.push_back(SettingItem::create_category(
            "cat_applications", "Applications", "󰀻",
            {cat_installed_apps, cat_default_apps}
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
        DisplayBackend::sync_item_state(disp_bright);

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
        disp_nightlight->nightlight_cfg.enabled = true;
        disp_nightlight->nightlight_cfg.warmth = 0.65f;
        disp_nightlight->telemetry_title = "Gamma & color specifications";
        disp_nightlight->telemetry = {
            {"Gamma Protocol", "wlr-gamma-control-v1"},
            {"Night Temperature", "3400 K (Warm Amber)"},
            {"Day Temperature", "6500 K (Standard D65)"},
            {"Sunset Schedule", "Sunset 18:15 • Sunrise 06:20"}
        };

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
        shared_battery_cfg->percentage = 88;
        shared_battery_cfg->is_charging = false;
        shared_battery_cfg->power_saver = false;
        shared_battery_cfg->power_profile = "Balanced";
        shared_battery_cfg->charge_limit_enabled = true;

        // 7.1 Battery Info (Leaf)
        auto battery_info = SettingItem::create_leaf("bat_info", "Battery Info", "", "󰁹");
        battery_info->is_battery_info = true;
        battery_info->battery_cfg = shared_battery_cfg;
        battery_info->telemetry_title = "Battery specifications";
        battery_info->telemetry = {
            {"Battery Health", "98.4% of design capacity"},
            {"Cycle Count", "142 cycles"},
            {"Full Charge Capacity", "59.0 Wh (Design: 60.0 Wh)"},
            {"Power Source", "Battery (Discharging)"},
            {"Discharge Rate", "7.8 W (Estimated)"},
            {"Cell Temperature", "31.4 °C"},
            {"Technology", "Lithium-Polymer (LiPo)"}
        };

        // 7.2 Power & Performance Mode (Leaf)
        auto power_mode = SettingItem::create_leaf("bat_power_mode", "Power & Performance Mode", "", "󰌪");
        power_mode->is_power_mode = true;
        power_mode->battery_cfg = shared_battery_cfg;
        power_mode->telemetry_title = "Power management specifications";
        power_mode->telemetry = {
            {"Current Governor", "powersave (amd-pstate-epp)"},
            {"Energy Performance Preference", "balance_power"},
            {"Platform Profile", "balanced (ACPI platform profile)"},
            {"Active Power Draw", "7.8 W (Estimated)"},
            {"Package Temp", "42.5 °C"}
        };

        root.push_back(SettingItem::create_category(
            "cat_battery", "Battery", "󰁹",
            {battery_info, power_mode}
        ));
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

        // 10.2 Input (Leaf)
        auto sys_input = SettingItem::create_leaf("sys_input", "Input", "", "󰌌");
        sys_input->has_switch = true;
        sys_input->switch_val = true;
        sys_input->switch_label = "Natural Scrolling";
        sys_input->has_secondary_switch = true;
        sys_input->secondary_switch_val = true;
        sys_input->secondary_switch_label = "Tap-to-Click on Touchpad";
        sys_input->has_slider = true;
        sys_input->slider_val = 0.60f;
        sys_input->slider_label = "Pointer Speed (60%)";
        sys_input->slider_unit = "%";
        sys_input->has_action = true;
        sys_input->action_label = "Calibrate Touchpad Gestures";
        sys_input->telemetry_title = "Input hardware specifications";
        sys_input->telemetry = {
            {"Keyboard Layout", "English (US) / ANSI standard"},
            {"Active Keyboard", "Keychron K2 Pro (QMK/VIA)"},
            {"Pointing Device", "Logitech MX Master 3S (1000 Hz)"},
            {"Touchpad Protocol", "libinput (Precision Touchpad)"},
            {"Multi-Touch Gestures", "3-finger workspace swipe, 4-finger overview"}
        };

        root.push_back(SettingItem::create_category(
            "cat_system", "System", "󰒓",
            {sys_about, sys_input}
        ));
    }

    return root;
}

} // namespace miqu
