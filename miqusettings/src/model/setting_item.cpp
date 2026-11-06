#include "model/setting_item.hpp"

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

    // 1. Applications & Defaults
    {
        auto apps = std::make_shared<SettingItem>();
        apps->id = "apps";
        apps->title = "Applications & Defaults";
        apps->icon_name = "system-software-install";
        apps->type = SettingType::Category;

        auto apps_list = std::make_shared<SettingItem>();
        apps_list->id = "apps_list";
        apps_list->title = "Apps";
        apps_list->icon_name = "applications-all";
        apps_list->type = SettingType::Category;
        apps->children.push_back(apps_list);

        auto def_apps = std::make_shared<SettingItem>();
        def_apps->id = "default_apps";
        def_apps->title = "Default Apps";
        def_apps->icon_name = "preferences-desktop-default-applications";
        def_apps->type = SettingType::Category;
        apps->children.push_back(def_apps);

        root->children.push_back(apps);
    }

    // 2. Wireless & Network
    {
        auto net = std::make_shared<SettingItem>();
        net->id = "network";
        net->title = "Wireless & Network";
        net->icon_name = "network-wireless";
        net->type = SettingType::Category;

        auto airplane = std::make_shared<SettingItem>();
        airplane->id = "airplane_mode";
        airplane->title = "Airplane Mode";
        airplane->icon_name = "airplane-mode";
        airplane->type = SettingType::Toggle;
        airplane->bool_val = false;
        net->children.push_back(airplane);

        auto wifi = std::make_shared<SettingItem>();
        wifi->id = "wifi";
        wifi->title = "Wi-Fi";
        wifi->icon_name = "network-wireless";
        wifi->type = SettingType::Info;
        net->children.push_back(wifi);

        auto eth = std::make_shared<SettingItem>();
        eth->id = "ethernet";
        eth->title = "Wired (Ethernet)";
        eth->icon_name = "network-wired";
        eth->type = SettingType::Info;
        net->children.push_back(eth);

        auto bt = std::make_shared<SettingItem>();
        bt->id = "bluetooth";
        bt->title = "Bluetooth";
        bt->icon_name = "bluetooth";
        bt->type = SettingType::Info;
        net->children.push_back(bt);

        root->children.push_back(net);
    }

    // 3. Monitors (Zero startup system queries; loaded on demand)
    {
        auto mon = std::make_shared<SettingItem>();
        mon->id = "monitors";
        mon->title = "Monitors";
        mon->icon_name = "video-display";
        mon->type = SettingType::Category;
        root->children.push_back(mon);
    }

    // 4. Display
    {
        auto disp = std::make_shared<SettingItem>();
        disp->id = "display";
        disp->title = "Display";
        disp->icon_name = "display-brightness";
        disp->type = SettingType::Category;

        auto disp_bright = std::make_shared<SettingItem>();
        disp_bright->id = "disp_bright";
        disp_bright->title = "Brightness";
        disp_bright->icon_name = "display-brightness";
        disp_bright->type = SettingType::Info;
        disp->children.push_back(disp_bright);

        auto disp_gamma = std::make_shared<SettingItem>();
        disp_gamma->id = "disp_gamma";
        disp_gamma->title = "Gamma";
        disp_gamma->icon_name = "weather-clear-night";
        disp_gamma->type = SettingType::Info;
        disp->children.push_back(disp_gamma);

        auto disp_lock = std::make_shared<SettingItem>();
        disp_lock->id = "disp_lock";
        disp_lock->title = "Screen Lock";
        disp_lock->icon_name = "system-lock-screen";
        disp_lock->type = SettingType::Toggle;
        disp_lock->bool_val = true;
        disp->children.push_back(disp_lock);

        auto disp_timeout = std::make_shared<SettingItem>();
        disp_timeout->id = "disp_timeout";
        disp_timeout->title = "Time Out";
        disp_timeout->icon_name = "preferences-desktop-screensaver";
        disp_timeout->type = SettingType::Selection;
        disp_timeout->options = {"5 Minutes", "10 Minutes (Recommended)", "15 Minutes", "Never"};
        disp_timeout->selected_option_idx = 1;
        disp->children.push_back(disp_timeout);

        root->children.push_back(disp);
    }

    // 5. Sound & Audio
    {
        auto sound = std::make_shared<SettingItem>();
        sound->id = "sound";
        sound->title = "Sound & Audio";
        sound->icon_name = "audio-speakers";
        sound->type = SettingType::Category;

        auto vol_out = std::make_shared<SettingItem>();
        vol_out->id = "vol_out";
        vol_out->title = "Master Output Volume";
        vol_out->icon_name = "audio-volume-high";
        vol_out->type = SettingType::SliderControl;
        vol_out->float_val = 0.70f;
        sound->children.push_back(vol_out);

        auto dev_out = std::make_shared<SettingItem>();
        dev_out->id = "dev_out";
        dev_out->title = "Output Device";
        dev_out->icon_name = "audio-speakers";
        dev_out->type = SettingType::Selection;
        dev_out->options = {"Realtek ALC1220 (Analog Stereo)", "HDMI / DP Digital Output", "Sony WH-1000XM4 (Bluetooth LDAC)"};
        dev_out->selected_option_idx = 0;
        sound->children.push_back(dev_out);

        auto vol_in = std::make_shared<SettingItem>();
        vol_in->id = "vol_in";
        vol_in->title = "Microphone Volume";
        vol_in->icon_name = "audio-input-microphone";
        vol_in->type = SettingType::SliderControl;
        vol_in->float_val = 0.80f;
        sound->children.push_back(vol_in);

        auto mute_all = std::make_shared<SettingItem>();
        mute_all->id = "mute_all";
        mute_all->title = "Mute All Sounds";
        mute_all->icon_name = "audio-volume-muted";
        mute_all->type = SettingType::Toggle;
        mute_all->bool_val = false;
        sound->children.push_back(mute_all);

        root->children.push_back(sound);
    }

    // 6. Appearance & Desktop
    {
        auto app = std::make_shared<SettingItem>();
        app->id = "appearance";
        app->title = "Appearance & Theming";
        app->icon_name = "preferences-desktop-theme";
        app->type = SettingType::Category;

        auto dark_theme = std::make_shared<SettingItem>();
        dark_theme->id = "dark_theme";
        dark_theme->title = "Dark Theme";
        dark_theme->icon_name = "preferences-desktop-theme";
        dark_theme->type = SettingType::Toggle;
        dark_theme->bool_val = true;
        app->children.push_back(dark_theme);

        auto accent = std::make_shared<SettingItem>();
        accent->id = "accent_color";
        accent->title = "Accent Palette";
        accent->icon_name = "applications-graphics";
        accent->type = SettingType::Selection;
        accent->options = {"Miqu Violet (Default)", "Ocean Blue", "Emerald Teal", "Sunset Amber", "Ruby Red"};
        accent->selected_option_idx = 0;
        app->children.push_back(accent);

        auto radius = std::make_shared<SettingItem>();
        radius->id = "corner_radius";
        radius->title = "Window Corner Radius";
        radius->icon_name = "preferences-desktop-display";
        radius->type = SettingType::Selection;
        radius->options = {"0 px (Sharp)", "8 px (Compact)", "12 px (Standard)", "16 px (Smooth)"};
        radius->selected_option_idx = 2;
        app->children.push_back(radius);

        auto blur = std::make_shared<SettingItem>();
        blur->id = "blur_effect";
        blur->title = "Window & Flyout Blur";
        blur->icon_name = "color-management";
        blur->type = SettingType::Toggle;
        blur->bool_val = true;
        app->children.push_back(blur);

        root->children.push_back(app);
    }

    // 7. Window Management & Tiling
    {
        auto wm = std::make_shared<SettingItem>();
        wm->id = "wm_tiling";
        wm->title = "Window Management";
        wm->icon_name = "preferences-system-windows";
        wm->type = SettingType::Category;

        auto wm_layout = std::make_shared<SettingItem>();
        wm_layout->id = "wm_layout";
        wm_layout->title = "Tiling Layout";
        wm_layout->icon_name = "preferences-system-windows";
        wm_layout->type = SettingType::Info;
        wm->children.push_back(wm_layout);

        auto wm_gaps = std::make_shared<SettingItem>();
        wm_gaps->id = "wm_gaps";
        wm_gaps->title = "Window Gaps";
        wm_gaps->icon_name = "preferences-desktop-display";
        wm_gaps->type = SettingType::Info;
        wm->children.push_back(wm_gaps);

        auto wm_borders = std::make_shared<SettingItem>();
        wm_borders->id = "wm_borders";
        wm_borders->title = "Borders & Corners";
        wm_borders->icon_name = "preferences-desktop-theme";
        wm_borders->type = SettingType::Info;
        wm->children.push_back(wm_borders);

        auto wm_effects = std::make_shared<SettingItem>();
        wm_effects->id = "wm_effects";
        wm_effects->title = "Opacity & Effects";
        wm_effects->icon_name = "color-management";
        wm_effects->type = SettingType::Info;
        wm->children.push_back(wm_effects);

        auto wm_control = std::make_shared<SettingItem>();
        wm_control->id = "wm_control";
        wm_control->title = "Compositor Control";
        wm_control->icon_name = "system-reboot";
        wm_control->type = SettingType::Info;
        wm->children.push_back(wm_control);

        root->children.push_back(wm);
    }

    // 8. Keyboard & Mouse
    {
        auto input = std::make_shared<SettingItem>();
        input->id = "input_devices";
        input->title = "Keyboard & Mouse";
        input->icon_name = "input-keyboard";
        input->type = SettingType::Category;

        auto in_kb = std::make_shared<SettingItem>();
        in_kb->id = "input_keyboards";
        in_kb->title = "Keyboards & Layout";
        in_kb->icon_name = "input-keyboard";
        in_kb->type = SettingType::Info;
        input->children.push_back(in_kb);

        auto in_rep = std::make_shared<SettingItem>();
        in_rep->id = "input_repeat";
        in_rep->title = "Typing & Repeat Speed";
        in_rep->icon_name = "input-keyboard";
        in_rep->type = SettingType::Info;
        input->children.push_back(in_rep);

        auto in_ptr = std::make_shared<SettingItem>();
        in_ptr->id = "input_pointers";
        in_ptr->title = "Pointers & Touch";
        in_ptr->icon_name = "input-mouse";
        in_ptr->type = SettingType::Info;
        input->children.push_back(in_ptr);

        root->children.push_back(input);
    }

    // 9. Power & Battery
    {
        auto pwr = std::make_shared<SettingItem>();
        pwr->id = "power";
        pwr->title = "Power & Battery";
        pwr->icon_name = "battery";
        pwr->type = SettingType::Category;

        auto profile = std::make_shared<SettingItem>();
        profile->id = "pwr_profile";
        profile->title = "Performance Profile";
        profile->icon_name = "power-profile-balanced";
        profile->type = SettingType::Selection;
        profile->options = {"Power Saver (Extend battery)", "Balanced (Adaptive)", "Performance (Max CPU boost)"};
        profile->selected_option_idx = 1;
        pwr->children.push_back(profile);

        auto screen_off = std::make_shared<SettingItem>();
        screen_off->id = "pwr_screen_off";
        screen_off->title = "Screen Off Timeout";
        screen_off->icon_name = "preferences-system-power";
        screen_off->type = SettingType::Selection;
        screen_off->options = {"5 minutes", "10 minutes", "15 minutes", "30 minutes", "Never"};
        screen_off->selected_option_idx = 1;
        pwr->children.push_back(screen_off);

        auto suspend = std::make_shared<SettingItem>();
        suspend->id = "pwr_suspend";
        suspend->title = "Automatic Suspend";
        suspend->icon_name = "system-suspend";
        suspend->type = SettingType::Toggle;
        suspend->bool_val = true;
        pwr->children.push_back(suspend);

        root->children.push_back(pwr);
    }

    // 10. Security & Privacy
    {
        auto sec = std::make_shared<SettingItem>();
        sec->id = "security";
        sec->title = "Security & Privacy";
        sec->icon_name = "security-high";
        sec->type = SettingType::Category;

        auto fw = std::make_shared<SettingItem>();
        fw->id = "sec_firewall";
        fw->title = "Application Firewall (OpenSnitch)";
        fw->icon_name = "security-high";
        fw->type = SettingType::Toggle;
        fw->bool_val = true;
        sec->children.push_back(fw);

        auto tor = std::make_shared<SettingItem>();
        tor->id = "sec_tor";
        tor->title = "Tor Anonymity Routing";
        tor->icon_name = "network-wired";
        tor->type = SettingType::Toggle;
        tor->bool_val = false;
        sec->children.push_back(tor);

        auto lock = std::make_shared<SettingItem>();
        lock->id = "sec_lock";
        lock->title = "Session Lock Timeout";
        lock->icon_name = "system-lock-screen";
        lock->type = SettingType::Selection;
        lock->options = {"Immediate", "5 minutes", "10 minutes", "15 minutes", "Never"};
        lock->selected_option_idx = 1;
        sec->children.push_back(lock);

        auto luks = std::make_shared<SettingItem>();
        luks->id = "sec_luks";
        luks->title = "LUKS Disk Encryption";
        luks->icon_name = "drive-harddisk-encrypted";
        luks->type = SettingType::Info;
        sec->children.push_back(luks);

        root->children.push_back(sec);
    }

    // 11. System & About
    {
        auto about = std::make_shared<SettingItem>();
        about->id = "about";
        about->title = "System & About";
        about->icon_name = "help-about";
        about->type = SettingType::Category;

        auto os = std::make_shared<SettingItem>();
        os->id = "about_os";
        os->title = "Operating System";
        os->icon_name = "distributor-logo-archlinux";
        os->type = SettingType::Info;
        about->children.push_back(os);

        auto de = std::make_shared<SettingItem>();
        de->id = "about_de";
        de->title = "Desktop Environment";
        de->icon_name = "preferences-system";
        de->type = SettingType::Info;
        about->children.push_back(de);

        auto wm = std::make_shared<SettingItem>();
        wm->id = "about_wm";
        wm->title = "Window Manager & Compositor";
        wm->icon_name = "preferences-system-windows";
        wm->type = SettingType::Info;
        about->children.push_back(wm);

        auto tk = std::make_shared<SettingItem>();
        tk->id = "about_toolkit";
        tk->title = "UI Framework";
        tk->icon_name = "applications-development";
        tk->type = SettingType::Info;
        about->children.push_back(tk);

        auto kernel = std::make_shared<SettingItem>();
        kernel->id = "about_kernel";
        kernel->title = "Linux Kernel";
        kernel->icon_name = "computer";
        kernel->type = SettingType::Info;
        about->children.push_back(kernel);

        auto cpu = std::make_shared<SettingItem>();
        cpu->id = "about_cpu";
        cpu->title = "Processor & Memory";
        cpu->icon_name = "cpu";
        cpu->type = SettingType::Info;
        about->children.push_back(cpu);

        root->children.push_back(about);
    }

    link_parent_tree(root);
    return root;
}

} // namespace miqu
