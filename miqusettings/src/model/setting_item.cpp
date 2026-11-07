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

    auto add_page = [&](const std::string& id,
                        const std::string& title,
                        const std::string& subtitle,
                        const std::string& icon,
                        SettingType type = SettingType::Info) -> std::shared_ptr<SettingItem> {
        auto item = std::make_shared<SettingItem>();
        item->id = id;
        item->title = title;
        item->subtitle = subtitle;
        item->icon_name = icon;
        item->type = type;
        root->children.push_back(item);
        return item;
    };

    // 1. Personalization & Appearance
    add_page("wallpaper", "Wallpaper", "Desktop background & display fit", "preferences-desktop-wallpaper");
    add_page("wm_borders", "Borders & Corners", "Window border width & corner radius", "preferences-desktop-theme");
    add_page("wm_effects", "Opacity & Effects", "Window transparency, blur & animations", "color-management");

    // 2. Hardware & Displays
    add_page("monitors", "Display & Monitors", "Resolution, refresh rate & layout", "video-display");
    add_page("disp_bright", "Brightness", "Screen backlight & night light temperature", "display-brightness");
    add_page("sound", "Sound & Audio", "Volume, outputs & PipeWire devices", "audio-volume-high");
    add_page("power", "Power & Battery", "Performance profiles & screen timeout", "battery");

    // 3. Connectivity & Networks
    add_page("wifi", "Wi-Fi", "Wireless networks & connection", "network-wireless");
    add_page("bluetooth", "Bluetooth", "Device pairing & adapter status", "bluetooth");
    add_page("ethernet", "Wired (Ethernet)", "Wired network adapter & IP status", "network-wired");

    // 4. Window Manager
    add_page("wm_layout", "Tiling Layout", "Window manager tiling algorithms", "preferences-system-windows");
    add_page("wm_gaps", "Window Gaps", "Inner and outer window spacing", "preferences-desktop-display");
    add_page("wm_control", "Compositor Control", "Reload or exit miquland window manager", "system-reboot");

    // 5. Input Devices
    add_page("input_keyboards", "Keyboards & Layout", "Keyboard devices & active layout", "input-keyboard");
    add_page("input_repeat", "Typing & Repeat Speed", "Key repeat delay & repetition speed", "input-keyboard");
    add_page("input_pointers", "Mouse & Touchpad", "Pointer speed & natural scrolling", "input-mouse");

    // 6. Applications & Security
    add_page("apps_list", "Installed Applications", "Manage desktop applications", "system-software-install");
    add_page("default_apps", "Default Applications", "Default web browser, editor & mail client", "preferences-desktop-default-applications");
    add_page("firewall", "Security & Firewall", "UFW firewall rules, policies & status", "security-high");
    add_page("about", "About System", "OS, desktop environment & system telemetry", "help-about");

    link_parent_tree(root);
    return root;
}

} // namespace miqu
