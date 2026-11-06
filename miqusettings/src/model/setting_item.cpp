#include "model/setting_item.hpp"
#include "modules/ethernet/ethernet_backend.hpp"
#include <sstream>
#include <iomanip>

namespace miqu {

std::string SettingItem::get_status_text() const {
    switch (type) {
        case SettingType::Toggle:
            return bool_val ? "Enabled" : "Disabled";
        case SettingType::SliderControl: {
            int pct = static_cast<int>(float_val * 100.0f + 0.5f);
            return std::to_string(pct) + "%";
        }
        case SettingType::Selection:
            if (selected_option_idx >= 0 && selected_option_idx < static_cast<int>(options.size())) {
                return options[selected_option_idx];
            }
            return "";
        case SettingType::Category:
            return std::to_string(children.size()) + " items";
        case SettingType::Info:
        default:
            return "";
    }
}

static void link_parent_tree(const std::shared_ptr<SettingItem>& node) {
    for (auto& child : node->children) {
        child->parent = node;
        link_parent_tree(child);
    }
}

std::shared_ptr<SettingItem> SettingItem::create_default_tree() {
    auto root = std::make_shared<SettingItem>();
    root->id = "root";
    root->title = "Settings";
    root->icon_name = "preferences-system";
    root->type = SettingType::Category;

    // 1. Wi-Fi & Network
    {
        auto net = std::make_shared<SettingItem>();
        net->id = "network";
        net->title = "Wireless & Network";
        net->subtitle = "Wi-Fi, Ethernet, Bluetooth, VPN";
        net->icon_name = "network-wireless";
        net->type = SettingType::Category;

        // Wi-Fi subcategory
        auto wifi = std::make_shared<SettingItem>();
        wifi->id = "wifi";
        wifi->title = "Wi-Fi";
        wifi->subtitle = "Connected to MiquNet_5G";
        wifi->icon_name = "network-wireless";
        wifi->type = SettingType::Category;

        auto wifi_toggle = std::make_shared<SettingItem>();
        wifi_toggle->id = "wifi_enable";
        wifi_toggle->title = "Wi-Fi Switch";
        wifi_toggle->subtitle = "Enable or disable wireless radio";
        wifi_toggle->icon_name = "network-wireless";
        wifi_toggle->type = SettingType::Toggle;
        wifi_toggle->bool_val = true;
        wifi->children.push_back(wifi_toggle);

        auto wifi_current = std::make_shared<SettingItem>();
        wifi_current->id = "wifi_current";
        wifi_current->title = "MiquNet_5G";
        wifi_current->subtitle = "Connected, 866 Mbps (5 GHz, WPA3-Personal)";
        wifi_current->icon_name = "network-wireless-signal-excellent";
        wifi_current->type = SettingType::Info;
        wifi->children.push_back(wifi_current);

        auto wifi_networks = std::make_shared<SettingItem>();
        wifi_networks->id = "wifi_networks";
        wifi_networks->title = "Available Networks";
        wifi_networks->subtitle = "3 detected wireless networks";
        wifi_networks->icon_name = "network-wireless-acquiring";
        wifi_networks->type = SettingType::Category;

        auto net1 = std::make_shared<SettingItem>();
        net1->id = "net_office";
        net1->title = "Office_Station";
        net1->subtitle = "Saved, Secured (WPA2-Enterprise)";
        net1->icon_name = "network-wireless-signal-good";
        net1->type = SettingType::Info;
        wifi_networks->children.push_back(net1);

        auto net2 = std::make_shared<SettingItem>();
        net2->id = "net_guest";
        net2->title = "Guest_Lounge";
        net2->subtitle = "Open Network (Captive Portal)";
        net2->icon_name = "network-wireless-signal-ok";
        net2->type = SettingType::Info;
        wifi_networks->children.push_back(net2);

        auto net3 = std::make_shared<SettingItem>();
        net3->id = "net_iot";
        net3->title = "Home_IoT_2.4G";
        net3->subtitle = "Secured (WPA2)";
        net3->icon_name = "network-wireless-signal-weak";
        net3->type = SettingType::Info;
        wifi_networks->children.push_back(net3);

        wifi->children.push_back(wifi_networks);
        net->children.push_back(wifi);

        // Ethernet
        auto eth = std::make_shared<SettingItem>();
        eth->id = "ethernet";
        eth->title = "Wired (Ethernet)";
        auto eth_dev = EthernetBackend::get_device();
        eth->subtitle = eth_dev.name + " · " + eth_dev.state_str;
        eth->icon_name = eth_dev.is_connected ? "network-wired" : "network-wired-disconnected";
        eth->type = SettingType::Category;

        auto eth_ip = std::make_shared<SettingItem>();
        eth_ip->id = "eth_ip";
        eth_ip->title = "IPv4 Address";
        eth_ip->subtitle = "192.168.1.108 / 24";
        eth_ip->icon_name = "network-wired";
        eth_ip->type = SettingType::Info;
        eth->children.push_back(eth_ip);

        auto eth_gw = std::make_shared<SettingItem>();
        eth_gw->id = "eth_gw";
        eth_gw->title = "Default Gateway";
        eth_gw->subtitle = "192.168.1.1";
        eth_gw->icon_name = "network-wired";
        eth_gw->type = SettingType::Info;
        eth->children.push_back(eth_gw);

        auto eth_dns = std::make_shared<SettingItem>();
        eth_dns->id = "eth_dns";
        eth_dns->title = "DNS Servers";
        eth_dns->subtitle = "1.1.1.1, 8.8.8.8";
        eth_dns->icon_name = "network-wired";
        eth_dns->type = SettingType::Info;
        eth->children.push_back(eth_dns);

        net->children.push_back(eth);

        // Bluetooth
        auto bt = std::make_shared<SettingItem>();
        bt->id = "bluetooth";
        bt->title = "Bluetooth";
        bt->subtitle = "Audio, Mouse, Keyboard, Stylus & File Transfer";
        bt->icon_name = "bluetooth";
        bt->type = SettingType::Category;

        auto bt_toggle = std::make_shared<SettingItem>();
        bt_toggle->id = "bt_enable";
        bt_toggle->title = "Bluetooth Switch";
        bt_toggle->subtitle = "Discoverable as 'miqu-desktop'";
        bt_toggle->icon_name = "bluetooth";
        bt_toggle->type = SettingType::Toggle;
        bt_toggle->bool_val = true;
        bt->children.push_back(bt_toggle);

        auto bt_dev1 = std::make_shared<SettingItem>();
        bt_dev1->id = "bt_dev1";
        bt_dev1->title = "Sony WH-1000XM4";
        bt_dev1->subtitle = "Connected (Audio Headset, 80% Battery)";
        bt_dev1->icon_name = "audio-headphones";
        bt_dev1->type = SettingType::Info;
        bt->children.push_back(bt_dev1);

        auto bt_dev2 = std::make_shared<SettingItem>();
        bt_dev2->id = "bt_dev2";
        bt_dev2->title = "Logitech MX Master 3S";
        bt_dev2->subtitle = "Connected (Mouse, 90% Battery)";
        bt_dev2->icon_name = "input-mouse";
        bt_dev2->type = SettingType::Info;
        bt->children.push_back(bt_dev2);

        net->children.push_back(bt);

        root->children.push_back(net);
    }

    // 2. Displays & Monitor
    {
        auto disp = std::make_shared<SettingItem>();
        disp->id = "display";
        disp->title = "Displays & Monitor";
        disp->subtitle = "Resolution, Refresh Rate, Scaling, Brightness";
        disp->icon_name = "video-display";
        disp->type = SettingType::Category;

        auto disp_res = std::make_shared<SettingItem>();
        disp_res->id = "disp_res";
        disp_res->title = "Display Resolution";
        disp_res->subtitle = "Screen pixel dimensions";
        disp_res->icon_name = "video-display";
        disp_res->type = SettingType::Selection;
        disp_res->options = {"1920x1080 (16:9)", "2560x1440 (16:9)", "3840x2160 (4K UHD)", "1680x1050 (16:10)"};
        disp_res->selected_option_idx = 0;
        disp->children.push_back(disp_res);

        auto disp_hz = std::make_shared<SettingItem>();
        disp_hz->id = "disp_hz";
        disp_hz->title = "Refresh Rate";
        disp_hz->subtitle = "Vertical refresh frequency";
        disp_hz->icon_name = "preferences-system-windows";
        disp_hz->type = SettingType::Selection;
        disp_hz->options = {"60 Hz", "75 Hz", "120 Hz", "144 Hz (VRR Adaptive)", "165 Hz"};
        disp_hz->selected_option_idx = 3;
        disp->children.push_back(disp_hz);

        auto disp_scale = std::make_shared<SettingItem>();
        disp_scale->id = "disp_scale";
        disp_scale->title = "Display Scaling";
        disp_scale->subtitle = "UI element scale factor";
        disp_scale->icon_name = "zoom-original";
        disp_scale->type = SettingType::Selection;
        disp_scale->options = {"100% (Native 1x)", "125% (1.25x)", "150% (1.5x)", "200% (2x HiDPI)"};
        disp_scale->selected_option_idx = 0;
        disp->children.push_back(disp_scale);

        auto disp_bright = std::make_shared<SettingItem>();
        disp_bright->id = "disp_bright";
        disp_bright->title = "Display Brightness";
        disp_bright->subtitle = "Backlight intensity level";
        disp_bright->icon_name = "display-brightness";
        disp_bright->type = SettingType::SliderControl;
        disp_bright->float_val = 0.85f;
        disp->children.push_back(disp_bright);

        auto disp_night = std::make_shared<SettingItem>();
        disp_night->id = "disp_night";
        disp_night->title = "Night Light (Blue Filter)";
        disp_night->subtitle = "Warm color temperature for reduced eye strain";
        disp_night->icon_name = "weather-clear-night";
        disp_night->type = SettingType::Toggle;
        disp_night->bool_val = true;
        disp->children.push_back(disp_night);

        root->children.push_back(disp);
    }

    // 3. Sound & Audio
    {
        auto sound = std::make_shared<SettingItem>();
        sound->id = "sound";
        sound->title = "Sound & Audio";
        sound->subtitle = "Volume, Output Device, Microphone";
        sound->icon_name = "audio-speakers";
        sound->type = SettingType::Category;

        auto vol_out = std::make_shared<SettingItem>();
        vol_out->id = "vol_out";
        vol_out->title = "Master Output Volume";
        vol_out->subtitle = "Speaker and headphone playback gain";
        vol_out->icon_name = "audio-volume-high";
        vol_out->type = SettingType::SliderControl;
        vol_out->float_val = 0.70f;
        sound->children.push_back(vol_out);

        auto dev_out = std::make_shared<SettingItem>();
        dev_out->id = "dev_out";
        dev_out->title = "Output Device";
        dev_out->subtitle = "Active playback sink";
        dev_out->icon_name = "audio-speakers";
        dev_out->type = SettingType::Selection;
        dev_out->options = {"Realtek ALC1220 (Analog Stereo)", "HDMI / DP Digital Output", "Sony WH-1000XM4 (Bluetooth LDAC)"};
        dev_out->selected_option_idx = 0;
        sound->children.push_back(dev_out);

        auto vol_in = std::make_shared<SettingItem>();
        vol_in->id = "vol_in";
        vol_in->title = "Microphone Volume";
        vol_in->subtitle = "Voice capture recording level";
        vol_in->icon_name = "audio-input-microphone";
        vol_in->type = SettingType::SliderControl;
        vol_in->float_val = 0.80f;
        sound->children.push_back(vol_in);

        auto mute_all = std::make_shared<SettingItem>();
        mute_all->id = "mute_all";
        mute_all->title = "Mute All Sounds";
        mute_all->subtitle = "Silence audio output completely";
        mute_all->icon_name = "audio-volume-muted";
        mute_all->type = SettingType::Toggle;
        mute_all->bool_val = false;
        sound->children.push_back(mute_all);

        root->children.push_back(sound);
    }

    // 4. Appearance & Desktop
    {
        auto app = std::make_shared<SettingItem>();
        app->id = "appearance";
        app->title = "Appearance & Theming";
        app->subtitle = "Dark Theme, Accent Color, Corners, Blur";
        app->icon_name = "preferences-desktop-theme";
        app->type = SettingType::Category;

        auto dark_theme = std::make_shared<SettingItem>();
        dark_theme->id = "dark_theme";
        dark_theme->title = "Dark Theme";
        dark_theme->subtitle = "Apply dark UI token styling across all applications";
        dark_theme->icon_name = "preferences-desktop-theme";
        dark_theme->type = SettingType::Toggle;
        dark_theme->bool_val = true;
        app->children.push_back(dark_theme);

        auto accent = std::make_shared<SettingItem>();
        accent->id = "accent_color";
        accent->title = "Accent Palette";
        accent->subtitle = "Primary accent color for buttons and highlights";
        accent->icon_name = "applications-graphics";
        accent->type = SettingType::Selection;
        accent->options = {"Miqu Violet (Default)", "Ocean Blue", "Emerald Teal", "Sunset Amber", "Ruby Red"};
        accent->selected_option_idx = 0;
        app->children.push_back(accent);

        auto radius = std::make_shared<SettingItem>();
        radius->id = "corner_radius";
        radius->title = "Window Corner Radius";
        radius->subtitle = "Corner curvature for cards, windows, and flyouts";
        radius->icon_name = "preferences-desktop-display";
        radius->type = SettingType::Selection;
        radius->options = {"0 px (Sharp)", "8 px (Compact)", "12 px (Standard)", "16 px (Smooth)"};
        radius->selected_option_idx = 2;
        app->children.push_back(radius);

        auto blur = std::make_shared<SettingItem>();
        blur->id = "blur_effect";
        blur->title = "Window & Flyout Blur";
        blur->subtitle = "Compositor dual-kawase background blur";
        blur->icon_name = "color-management";
        blur->type = SettingType::Toggle;
        blur->bool_val = true;
        app->children.push_back(blur);

        root->children.push_back(app);
    }

    // 5. Power & Battery
    {
        auto pwr = std::make_shared<SettingItem>();
        pwr->id = "power";
        pwr->title = "Power & Battery";
        pwr->subtitle = "Performance Profile, Sleep, Screen Off";
        pwr->icon_name = "battery";
        pwr->type = SettingType::Category;

        auto profile = std::make_shared<SettingItem>();
        profile->id = "pwr_profile";
        profile->title = "Performance Profile";
        profile->subtitle = "CPU frequency governor and power balance";
        profile->icon_name = "power-profile-balanced";
        profile->type = SettingType::Selection;
        profile->options = {"Power Saver (Extend battery)", "Balanced (Adaptive)", "Performance (Max CPU boost)"};
        profile->selected_option_idx = 1;
        pwr->children.push_back(profile);

        auto screen_off = std::make_shared<SettingItem>();
        screen_off->id = "pwr_screen_off";
        screen_off->title = "Screen Off Timeout";
        screen_off->subtitle = "Turn display off after idle period";
        screen_off->icon_name = "preferences-system-power";
        screen_off->type = SettingType::Selection;
        screen_off->options = {"5 minutes", "10 minutes", "15 minutes", "30 minutes", "Never"};
        screen_off->selected_option_idx = 1;
        pwr->children.push_back(screen_off);

        auto suspend = std::make_shared<SettingItem>();
        suspend->id = "pwr_suspend";
        suspend->title = "Automatic Suspend";
        suspend->subtitle = "Enter low-power sleep state on extended idle";
        suspend->icon_name = "system-suspend";
        suspend->type = SettingType::Toggle;
        suspend->bool_val = true;
        pwr->children.push_back(suspend);

        root->children.push_back(pwr);
    }

    // 6. Applications & Defaults
    {
        auto apps = std::make_shared<SettingItem>();
        apps->id = "apps";
        apps->title = "Applications & Defaults";
        apps->subtitle = "Default Browser, Terminal, File Manager, Player";
        apps->icon_name = "system-software-install";
        apps->type = SettingType::Category;

        auto browser = std::make_shared<SettingItem>();
        browser->id = "def_browser";
        browser->title = "Default Web Browser";
        browser->subtitle = "Preferred application for opening web links";
        browser->icon_name = "web-browser";
        browser->type = SettingType::Selection;
        browser->options = {"Firefox", "Chromium", "Brave", "Zen Browser"};
        browser->selected_option_idx = 0;
        apps->children.push_back(browser);

        auto filem = std::make_shared<SettingItem>();
        filem->id = "def_files";
        filem->title = "Default File Manager";
        filem->subtitle = "Standard directory browser";
        filem->icon_name = "system-file-manager";
        filem->type = SettingType::Selection;
        filem->options = {"Miqu Files (miqufiles)", "Thunar", "Nautilus"};
        filem->selected_option_idx = 0;
        apps->children.push_back(filem);

        auto term = std::make_shared<SettingItem>();
        term->id = "def_term";
        term->title = "Default Terminal Emulator";
        term->subtitle = "Command-line terminal window";
        term->icon_name = "utilities-terminal";
        term->type = SettingType::Selection;
        term->options = {"Kitty", "Foot", "Alacritty", "WezTerm"};
        term->selected_option_idx = 0;
        apps->children.push_back(term);

        auto music = std::make_shared<SettingItem>();
        music->id = "def_music";
        music->title = "Default Music Player";
        music->subtitle = "Audio playback application";
        music->icon_name = "audio-player";
        music->type = SettingType::Selection;
        music->options = {"Miqu Music (miqumusic)", "MPD Client", "VLC Media Player"};
        music->selected_option_idx = 0;
        apps->children.push_back(music);

        root->children.push_back(apps);
    }

    // 7. Security & Privacy
    {
        auto sec = std::make_shared<SettingItem>();
        sec->id = "security";
        sec->title = "Security & Privacy";
        sec->subtitle = "Firewall, Tor Routing, Screen Lock, Encryption";
        sec->icon_name = "security-high";
        sec->type = SettingType::Category;

        auto fw = std::make_shared<SettingItem>();
        fw->id = "sec_firewall";
        fw->title = "Application Firewall (OpenSnitch)";
        fw->subtitle = "Monitor and prompt for outbound application sockets";
        fw->icon_name = "security-high";
        fw->type = SettingType::Toggle;
        fw->bool_val = true;
        sec->children.push_back(fw);

        auto tor = std::make_shared<SettingItem>();
        tor->id = "sec_tor";
        tor->title = "Tor Anonymity Routing";
        tor->subtitle = "Route designated traffic through Tor onion network";
        tor->icon_name = "network-wired";
        tor->type = SettingType::Toggle;
        tor->bool_val = false;
        sec->children.push_back(tor);

        auto lock = std::make_shared<SettingItem>();
        lock->id = "sec_lock";
        lock->title = "Session Lock Timeout";
        lock->subtitle = "Lock screen with miqulock after inactivity";
        lock->icon_name = "system-lock-screen";
        lock->type = SettingType::Selection;
        lock->options = {"Immediate", "5 minutes", "10 minutes", "15 minutes", "Never"};
        lock->selected_option_idx = 1;
        sec->children.push_back(lock);

        auto luks = std::make_shared<SettingItem>();
        luks->id = "sec_luks";
        luks->title = "LUKS Disk Encryption";
        luks->subtitle = "NVMe Storage Partition AES-XTS-512 Encrypted";
        luks->icon_name = "drive-harddisk-encrypted";
        luks->type = SettingType::Info;
        sec->children.push_back(luks);

        root->children.push_back(sec);
    }

    // 8. About System
    {
        auto about = std::make_shared<SettingItem>();
        about->id = "about";
        about->title = "System & About";
        about->subtitle = "OS, Compositor, Kernel, Hardware Specifications";
        about->icon_name = "help-about";
        about->type = SettingType::Category;

        auto os = std::make_shared<SettingItem>();
        os->id = "about_os";
        os->title = "Operating System";
        os->subtitle = "Arch Linux (Rolling Release, x86_64)";
        os->icon_name = "distributor-logo-archlinux";
        os->type = SettingType::Info;
        about->children.push_back(os);

        auto de = std::make_shared<SettingItem>();
        de->id = "about_de";
        de->title = "Desktop Environment";
        de->subtitle = "Miqu Desktop Environment 1.0";
        de->icon_name = "preferences-system";
        de->type = SettingType::Info;
        about->children.push_back(de);

        auto wm = std::make_shared<SettingItem>();
        wm->id = "about_wm";
        wm->title = "Window Manager & Compositor";
        wm->subtitle = "miquland (Wayland / wlroots 0.18 Native)";
        wm->icon_name = "preferences-system-windows";
        wm->type = SettingType::Info;
        about->children.push_back(wm);

        auto tk = std::make_shared<SettingItem>();
        tk->id = "about_toolkit";
        tk->title = "UI Framework";
        tk->subtitle = "miqutoolkit 1.0 (Cairo / Pango C++20)";
        tk->icon_name = "applications-development";
        tk->type = SettingType::Info;
        about->children.push_back(tk);

        auto kernel = std::make_shared<SettingItem>();
        kernel->id = "about_kernel";
        kernel->title = "Linux Kernel";
        kernel->subtitle = "Linux 6.12.1-arch1-1 (Standard SMP)";
        kernel->icon_name = "computer";
        kernel->type = SettingType::Info;
        about->children.push_back(kernel);

        auto cpu = std::make_shared<SettingItem>();
        cpu->id = "about_cpu";
        cpu->title = "Processor & Memory";
        cpu->subtitle = "x86_64 Multi-Core Processor, 16 GB RAM";
        cpu->icon_name = "cpu";
        cpu->type = SettingType::Info;
        about->children.push_back(cpu);

        root->children.push_back(about);
    }

    link_parent_tree(root);
    return root;
}

} // namespace miqu
