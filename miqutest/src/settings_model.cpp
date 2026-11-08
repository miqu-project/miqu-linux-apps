#include "settings_model.hpp"

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
            {"Miqu-Fiber-5G", "Connected", "󰤨", true},
            {"Home-Mesh-Office", "Saved", "󰤥", true},
            {"Miqu-Lab-Secure", "Saved", "󰤢", true},
            {"Cafe-Express-WiFi", "Connect", "󰤨", false},
            {"Airport-Free-5G", "Connect", "󰤢", false}
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
            "Automatic (DHCP)"
        };
        eth->telemetry = {
            {"Interface", "enp4s0 (Realtek RTL8125 2.5GbE)"},
            {"MAC Address", "e4:54:e8:1b:9a:2c"},
            {"Link Speed", "2500 Mbps (Full Duplex)"},
            {"MTU", "1500 bytes"},
            {"Carrier", "Active (CAT6)"}
        };

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

        root.push_back(SettingItem::create_category(
            "cat_connectivity", "Connectivity", "󰖩",
            {wifi, bt, eth, airplane, private_dns}
        ));
    }

    // ==============================================================================
    // 2. Monitors & Displays (Depth 1)
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
    // 4. Personalization & UI (Depth 1)
    // ==============================================================================
    {
        // 2.1 Appearance Subcategory (Depth 2)
        auto theme_scheme = SettingItem::create_leaf("theme_scheme", "Color Theme", "Material 3 & dynamic Matugen schemes", "󰔎");
        theme_scheme->has_switch = true;
        theme_scheme->switch_val = true;
        theme_scheme->switch_label = "Dark Appearance";
        theme_scheme->has_secondary_switch = true;
        theme_scheme->secondary_switch_val = false;
        theme_scheme->secondary_switch_label = "High Contrast Mode";
        theme_scheme->has_slider = true;
        theme_scheme->slider_val = 0.35f;
        theme_scheme->slider_label = "Theme Palette Warmth";
        theme_scheme->slider_unit = "%";
        theme_scheme->has_action = true;
        theme_scheme->action_label = "Regenerate Palette (Matugen)";
        theme_scheme->telemetry = {
            {"Active Scheme", "Miqu Dark Material 3"},
            {"Primary Accent", "#3584e4 (Blue Sapphire)"},
            {"Surface Tint", "#1e1e2e (Catppuccin Mocha)"}
        };

        auto theme_borders = SettingItem::create_leaf("theme_borders", "Gradients & Borders", "Multi-stop borders & titlebar shading", "󰃟");
        theme_borders->has_switch = true;
        theme_borders->switch_val = true;
        theme_borders->switch_label = "Window Border Shading";
        theme_borders->has_secondary_switch = true;
        theme_borders->secondary_switch_val = true;
        theme_borders->secondary_switch_label = "Rainbow Gradient Color Stops";
        theme_borders->has_slider = true;
        theme_borders->slider_val = 0.40f;
        theme_borders->slider_label = "Border Width (2px)";
        theme_borders->slider_unit = "%";
        theme_borders->has_action = true;
        theme_borders->action_label = "Reset Default Borders";
        theme_borders->telemetry = {
            {"Corner Radius", "16 px"},
            {"Gradient Stops", "3 color stops"},
            {"Angle", "45.0 degrees"}
        };

        auto appear_cat = SettingItem::create_category(
            "appear_cat", "Appearance & Colors", "󰔎",
            {theme_scheme, theme_borders}
        );

        // 2.2 Compositor Effects Subcategory (Depth 2)
        auto fx_blur = SettingItem::create_leaf("fx_blur", "Kawase Blur & Shadows", "Hardware GPU blur passes & dropshadows", "󰍹");
        fx_blur->has_switch = true;
        fx_blur->switch_val = true;
        fx_blur->switch_label = "Dual-Kawase Background Blur";
        fx_blur->has_secondary_switch = true;
        fx_blur->secondary_switch_val = true;
        fx_blur->secondary_switch_label = "Ambient Drop Shadows";
        fx_blur->has_slider = true;
        fx_blur->slider_val = 0.70f;
        fx_blur->slider_label = "Blur Radius (8 passes)";
        fx_blur->slider_unit = "%";
        fx_blur->has_action = true;
        fx_blur->action_label = "Benchmark GPU Frame Time";
        fx_blur->telemetry = {
            {"Compositor Engine", "miquland (wlroots)"},
            {"VSync Refresh", "144 Hz (FreeSync Active)"},
            {"Frame Latency", "6.94 ms (Stable)"}
        };

        auto fx_anim = SettingItem::create_leaf("fx_anim", "Window Animations", "Easing curves & spring physics", "󰑮");
        fx_anim->has_switch = true;
        fx_anim->switch_val = true;
        fx_anim->switch_label = "Smooth Window Animations";
        fx_anim->has_secondary_switch = true;
        fx_anim->secondary_switch_val = true;
        fx_anim->secondary_switch_label = "Spring Physics Simulation";
        fx_anim->has_slider = true;
        fx_anim->slider_val = 0.80f;
        fx_anim->slider_label = "Animation Velocity (1.0x)";
        fx_anim->slider_unit = "%";
        fx_anim->has_action = true;
        fx_anim->action_label = "Preview Workspace Transition";
        fx_anim->telemetry = {
            {"Easing Curve", "cubic-bezier(0.16, 1, 0.3, 1)"},
            {"Spring Damping", "0.85"},
            {"Spring Stiffness", "240"}
        };

        auto fx_cat = SettingItem::create_category(
            "fx_cat", "Compositor Effects", "󰍹",
            {fx_blur, fx_anim}
        );

        // 2.3 Night Light & Displays Subcategory (Depth 2)
        auto nl_temp = SettingItem::create_leaf("nl_temp", "Color Temperature", "Blue light reduction & night comfort", "󰃟");
        nl_temp->has_switch = true;
        nl_temp->switch_val = true;
        nl_temp->switch_label = "Night Light Filter";
        nl_temp->has_secondary_switch = true;
        nl_temp->secondary_switch_val = true;
        nl_temp->secondary_switch_label = "Automatic Sunset to Sunrise";
        nl_temp->has_slider = true;
        nl_temp->slider_val = 0.65f;
        nl_temp->slider_label = "Color Warmth (3400 K)";
        nl_temp->slider_unit = "%";
        nl_temp->has_action = true;
        nl_temp->action_label = "Preview Warmth Calibration";
        nl_temp->telemetry = {
            {"Current Temp", "3400 K"},
            {"Coordinates", "23.8° N, 90.4° E"},
            {"Status", "Active (Night)"}
        };

        auto disp_scale = SettingItem::create_leaf("disp_scale", "Display Scaling & DPI", "HiDPI fractional scaling & font sizing", "󰹑");
        disp_scale->has_switch = true;
        disp_scale->switch_val = true;
        disp_scale->switch_label = "HiDPI Fractional Scaling";
        disp_scale->has_secondary_switch = true;
        disp_scale->secondary_switch_val = true;
        disp_scale->secondary_switch_label = "Sync Font DPI with Display Scale";
        disp_scale->has_slider = true;
        disp_scale->slider_val = 0.50f;
        disp_scale->slider_label = "Scale Factor (125%)";
        disp_scale->slider_unit = "%";
        disp_scale->has_action = true;
        disp_scale->action_label = "Apply Output Resolution";
        disp_scale->telemetry = {
            {"Native Resolution", "2560 x 1440 (16:9)"},
            {"Applied Scale", "1.25x"},
            {"Active Output", "DP-1 (DisplayPort)"}
        };

        auto disp_cat = SettingItem::create_category(
            "disp_cat", "Night Light & Displays", "󰃟",
            {nl_temp, disp_scale}
        );

        root.push_back(SettingItem::create_category(
            "cat_personalization", "Personalization & UI", "󰔎",
            {appear_cat, fx_cat, disp_cat}
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
    // 4. Privacy, Security & Firewall (Depth 1)
    // ==============================================================================
    {
        // 4.1 Firewall Subcategory (Depth 2)
        auto fw_in = SettingItem::create_leaf("fw_in", "Inbound Packet Filter", "Stateful packet inspection & stealth mode", "󰒃");
        fw_in->has_switch = true;
        fw_in->switch_val = true;
        fw_in->switch_label = "Stateful Packet Inspection";
        fw_in->has_secondary_switch = true;
        fw_in->secondary_switch_val = true;
        fw_in->secondary_switch_label = "Stealth Mode (Drop ICMP Echo)";
        fw_in->has_slider = true;
        fw_in->slider_val = 0.30f;
        fw_in->slider_label = "Log Retention Duration (30d)";
        fw_in->slider_unit = "d";
        fw_in->has_action = true;
        fw_in->action_label = "Flush Deny Tables";
        fw_in->telemetry = {
            {"Backend Engine", "nftables v1.0.9"},
            {"Active Rulesets", "48 rules"},
            {"Blocked Probes Today", "1,842 requests"}
        };

        auto fw_out = SettingItem::create_leaf("fw_out", "Outbound Application Rules", "OpenSnitch per-app socket monitor", "󰈿");
        fw_out->has_switch = true;
        fw_out->switch_val = true;
        fw_out->switch_label = "Prompt for New Outbound Sockets";
        fw_out->has_secondary_switch = true;
        fw_out->secondary_switch_val = true;
        fw_out->secondary_switch_label = "Block All Telemetry & Tracking Domains";
        fw_out->has_slider = true;
        fw_out->slider_val = 0.25f;
        fw_out->slider_label = "Default Decision Timeout (15s)";
        fw_out->slider_unit = "s";
        fw_out->has_action = true;
        fw_out->action_label = "Export Ruleset (JSON)";
        fw_out->telemetry = {
            {"Monitored Apps", "28 running applications"},
            {"Allowed Today", "14,200 connections"},
            {"Blocked Today", "312 requests"}
        };

        auto sec_fw_cat = SettingItem::create_category(
            "sec_fw_cat", "App Firewall", "󰒃",
            {fw_in, fw_out}
        );

        // 4.2 Session Lock Subcategory (Depth 2)
        auto lock_timeouts = SettingItem::create_leaf("lock_timeouts", "Auto-Lock Timeouts", "Inactivity timers & power event hooks", "󰌿");
        lock_timeouts->has_switch = true;
        lock_timeouts->switch_val = true;
        lock_timeouts->switch_label = "Auto-Lock on Inactivity";
        lock_timeouts->has_secondary_switch = true;
        lock_timeouts->secondary_switch_val = true;
        lock_timeouts->secondary_switch_label = "Lock Screen on Laptop Lid Close";
        lock_timeouts->has_slider = true;
        lock_timeouts->slider_val = 0.25f;
        lock_timeouts->slider_label = "Inactivity Timeout (5 min)";
        lock_timeouts->slider_unit = "m";
        lock_timeouts->has_action = true;
        lock_timeouts->action_label = "Lock Screen Immediately";
        lock_timeouts->telemetry = {
            {"Security Protocol", "ext-session-lock-v1"},
            {"Lock Daemon", "miqulock"},
            {"Current State", "Unlocked"}
        };

        auto lock_bio = SettingItem::create_leaf("lock_bio", "Biometrics & PAM 2FA", "Fingerprint auth & hardware security keys", "󰌾");
        lock_bio->has_switch = true;
        lock_bio->switch_val = true;
        lock_bio->switch_label = "Fingerprint Reader (fprintd)";
        lock_bio->has_secondary_switch = true;
        lock_bio->secondary_switch_val = false;
        lock_bio->secondary_switch_label = "Require YubiKey 2FA for Privilege Escalation";
        lock_bio->has_slider = true;
        lock_bio->slider_val = 0.30f;
        lock_bio->slider_label = "Max Failed Attempts Before Lockout (3)";
        lock_bio->slider_unit = "%";
        lock_bio->has_action = true;
        lock_bio->action_label = "Enroll New Fingerprint";
        lock_bio->telemetry = {
            {"Biometric Hardware", "Goodix Fingerprint SPI"},
            {"Enrolled Finger", "Right Index Finger"},
            {"PAM Status", "Verified active"}
        };

        auto sec_lock_cat = SettingItem::create_category(
            "sec_lock_cat", "Session Lock & Biometrics", "󰌿",
            {lock_timeouts, lock_bio}
        );

        root.push_back(SettingItem::create_category(
            "cat_security", "Privacy, Security & Firewall", "󰒃",
            {sec_fw_cat, sec_lock_cat}
        ));
    }

    // ==============================================================================
    // 5. System & Power (Depth 1)
    // ==============================================================================
    {
        // 5.1 Power Subcategory (Depth 2)
        auto power_profiles = SettingItem::create_leaf("power_profiles", "Energy Profiles", "CPU governors & battery savers", "󰂄");
        power_profiles->has_switch = true;
        power_profiles->switch_val = false;
        power_profiles->switch_label = "High Performance Turbo Boost";
        power_profiles->has_secondary_switch = true;
        power_profiles->secondary_switch_val = true;
        power_profiles->secondary_switch_label = "Auto-Energy Saver on Low Battery";
        power_profiles->has_slider = true;
        power_profiles->slider_val = 0.20f;
        power_profiles->slider_label = "Display Sleep Timeout (4 min)";
        power_profiles->slider_unit = "m";
        power_profiles->has_action = true;
        power_profiles->action_label = "Refresh Battery Telemetry";
        power_profiles->telemetry = {
            {"Current Governor", "powersave (amd-pstate-epp)"},
            {"Battery Charge", "92% (Discharging, ~5h 20m)"},
            {"Package Temp", "42.5 °C"}
        };

        auto power_charge = SettingItem::create_leaf("power_charge", "Charging Longevity", "Battery threshold & cycle health", "󰂑");
        power_charge->has_switch = true;
        power_charge->switch_val = true;
        power_charge->switch_label = "Conservation Mode (80% Max Charge)";
        power_charge->has_secondary_switch = true;
        power_charge->secondary_switch_val = true;
        power_charge->secondary_switch_label = "Fast Charging via USB-PD";
        power_charge->has_slider = true;
        power_charge->slider_val = 0.80f;
        power_charge->slider_label = "Threshold Limit (80%)";
        power_charge->slider_unit = "%";
        power_charge->has_action = true;
        power_charge->action_label = "Recalibrate Battery Gauge";
        power_charge->telemetry = {
            {"Cycle Count", "142 cycles"},
            {"Battery Health", "98.4% of design capacity"},
            {"Design Capacity", "60.0 Wh"}
        };

        auto sys_power_cat = SettingItem::create_category(
            "sys_power_cat", "Power & Battery", "󰂄",
            {power_profiles, power_charge}
        );

        // 5.2 Diagnostics Subcategory (Depth 2)
        auto diag_about = SettingItem::create_leaf("diag_about", "About System", "OS versions, kernel & architecture", "󰋽");
        diag_about->has_switch = true;
        diag_about->switch_val = false;
        diag_about->switch_label = "Developer Diagnostic Mode";
        diag_about->has_secondary_switch = true;
        diag_about->secondary_switch_val = false;
        diag_about->secondary_switch_label = "Send Anonymous Performance Metrics";
        diag_about->has_slider = true;
        diag_about->slider_val = 1.0f;
        diag_about->slider_label = "Display DPI Scaling (100%)";
        diag_about->slider_unit = "%";
        diag_about->has_action = true;
        diag_about->action_label = "Check for Ecosystem Updates";
        diag_about->telemetry = {
            {"Distribution", "Arch Linux (Rolling Release)"},
            {"Linux Kernel", "6.11.7-arch1-1"},
            {"Desktop Compositor", "miquland (C++20)"},
            {"Toolkit Version", "miqutoolkit 1.0"}
        };

        auto sys_diag_cat = SettingItem::create_category(
            "sys_diag_cat", "System & Diagnostics", "󰋽",
            {diag_about}
        );

        root.push_back(SettingItem::create_category(
            "cat_system", "System & Power", "󰂄",
            {sys_power_cat, sys_diag_cat}
        ));
    }

    return root;
}

} // namespace miqu
