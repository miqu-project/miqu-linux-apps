#include "ui/setting_detail_view.hpp"
#include "modules/wallpaper/wallpaper_detail_view.hpp"
#include "modules/sound/sound_detail_view.hpp"
#include "modules/wifi/wifi_detail_view.hpp"
#include "modules/bluetooth/bluetooth_detail_view.hpp"
#include "modules/ethernet/ethernet_detail_view.hpp"
#include "modules/monitors/monitor_detail_view.hpp"
#include "modules/display/brightness_detail_view.hpp"
#include "modules/windowmanager/wm_layout_detail_view.hpp"
#include "modules/windowmanager/wm_gaps_detail_view.hpp"
#include "modules/windowmanager/wm_borders_detail_view.hpp"
#include "modules/windowmanager/wm_effects_detail_view.hpp"
#include "modules/windowmanager/wm_control_detail_view.hpp"
#include "modules/keyboard/keyboard_devices_detail_view.hpp"
#include "modules/keyboard/keyboard_repeat_detail_view.hpp"
#include "modules/keyboard/pointers_detail_view.hpp"
#include "modules/firewall/firewall_detail_view.hpp"
#include "modules/applications/apps_backend.hpp"
#include "modules/applications/app_detail_view.hpp"
#include "modules/applications/default_app_detail_view.hpp"
#include <miqutoolkit/core/config.hpp>
#include <miqutoolkit/view/list_view.hpp>
#include <miqutoolkit/view/list_item_view.hpp>
#include <sys/utsname.h>

namespace miqu {

SettingDetailView::SettingDetailView() {
    set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    show_wallpaper_page();
}

void SettingDetailView::clear() {
    clear_views();
    m_current_content.reset();
}

void SettingDetailView::show_page(const std::string& page_id) {
    m_current_page_id = page_id;
    m_subpage_open = false;
    clear_views();

    if (m_on_subpage_changed) {
        m_on_subpage_changed(false);
    }

    if (page_id.rfind("app_", 0) == 0) {
        show_app_detail(page_id.substr(4));
        return;
    }
    if (page_id.rfind("defrole_", 0) == 0) {
        show_default_role_detail(page_id.substr(8));
        return;
    }

    if (page_id == "wallpaper") {
        show_wallpaper_page();
    } else if (page_id == "monitors") {
        show_monitors_page();
    } else if (page_id == "disp_bright" || page_id == "display") {
        show_brightness_page();
    } else if (page_id == "sound") {
        show_sound_page();
    } else if (page_id == "wifi") {
        show_wifi_page();
    } else if (page_id == "bluetooth") {
        show_bluetooth_page();
    } else if (page_id == "ethernet") {
        show_ethernet_page();
    } else if (page_id == "wm_layout") {
        show_wm_layout_page();
    } else if (page_id == "wm_gaps") {
        show_wm_gaps_page();
    } else if (page_id == "wm_borders") {
        show_wm_borders_page();
    } else if (page_id == "wm_effects") {
        show_wm_effects_page();
    } else if (page_id == "wm_control") {
        show_wm_control_page();
    } else if (page_id == "input_keyboards" || page_id == "input_devices") {
        show_keyboard_devices_page();
    } else if (page_id == "input_repeat") {
        show_keyboard_repeat_page();
    } else if (page_id == "input_pointers") {
        show_pointers_page();
    } else if (page_id == "power") {
        show_power_page();
    } else if (page_id == "apps_list" || page_id == "apps") {
        show_apps_list_page();
    } else if (page_id == "default_apps") {
        show_default_apps_page();
    } else if (page_id == "firewall" || page_id == "security") {
        show_firewall_page();
    } else if (page_id == "about") {
        show_about_page();
    } else {
        show_wallpaper_page();
    }
}

void SettingDetailView::navigate_back_to_main_page() {
    if (m_subpage_open) {
        show_page(m_current_page_id);
    }
}

void SettingDetailView::show_wallpaper_page() {
    auto v = std::make_shared<WallpaperDetailView>();
    m_current_content = v;
    add_view(v);
}

void SettingDetailView::show_monitors_page() {
    auto v = std::make_shared<MonitorDetailView>();
    m_current_content = v;
    add_view(v);
}

void SettingDetailView::show_brightness_page() {
    auto v = std::make_shared<BrightnessDetailView>();
    m_current_content = v;
    add_view(v);
}

void SettingDetailView::show_sound_page() {
    auto v = std::make_shared<SoundDetailView>();
    m_current_content = v;
    add_view(v);
}

void SettingDetailView::show_wifi_page() {
    auto v = std::make_shared<WifiDetailView>();
    m_current_content = v;
    add_view(v);
}

void SettingDetailView::show_bluetooth_page() {
    auto v = std::make_shared<BluetoothDetailView>();
    m_current_content = v;
    add_view(v);
}

void SettingDetailView::show_ethernet_page() {
    auto v = std::make_shared<EthernetDetailView>();
    m_current_content = v;
    add_view(v);
}

void SettingDetailView::show_wm_layout_page() {
    auto v = std::make_shared<WmLayoutDetailView>();
    m_current_content = v;
    add_view(v);
}

void SettingDetailView::show_wm_gaps_page() {
    auto v = std::make_shared<WmGapsDetailView>();
    m_current_content = v;
    add_view(v);
}

void SettingDetailView::show_wm_borders_page() {
    auto v = std::make_shared<WmBordersDetailView>();
    m_current_content = v;
    add_view(v);
}

void SettingDetailView::show_wm_effects_page() {
    auto v = std::make_shared<WmEffectsDetailView>();
    m_current_content = v;
    add_view(v);
}

void SettingDetailView::show_wm_control_page() {
    auto v = std::make_shared<WmControlDetailView>();
    m_current_content = v;
    add_view(v);
}

void SettingDetailView::show_keyboard_devices_page() {
    auto v = std::make_shared<KeyboardDevicesDetailView>();
    m_current_content = v;
    add_view(v);
}

void SettingDetailView::show_keyboard_repeat_page() {
    auto v = std::make_shared<KeyboardRepeatDetailView>();
    m_current_content = v;
    add_view(v);
}

void SettingDetailView::show_pointers_page() {
    auto v = std::make_shared<PointersDetailView>();
    m_current_content = v;
    add_view(v);
}

void SettingDetailView::show_firewall_page() {
    auto v = std::make_shared<FirewallDetailView>();
    m_current_content = v;
    add_view(v);
}

void SettingDetailView::show_power_page() {
    auto config = Config::get();
    auto scroll = std::make_shared<ScrollView>();
    scroll->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));

    auto root = std::make_shared<LinearLayout>(Orientation::Vertical);
    root->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    root->set_divider_spacing(16);
    root->set_padding(24);

    // Hero Header
    auto hero_card = std::make_shared<CardView>();
    hero_card->set_card_background_color(config->colors.surface);
    hero_card->set_radius(config->metrics.corner_radius);
    hero_card->set_padding(20);
    hero_card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto hero_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    hero_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    hero_row->set_divider_spacing(16);

    auto icon = std::make_shared<ImageView>("battery");
    icon->set_target_size(56);
    LayoutParams icon_lp(56, 56);
    icon_lp.gravity = Gravity::CenterVertical;
    hero_row->add_view(icon, icon_lp);

    auto text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams text_lp;
    text_lp.width = 0;
    text_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    text_lp.weight = 1.0f;
    text_col->set_layout_params(text_lp);

    auto title = TextViewBuilder::create()
        ->text("Power & Battery")
        ->bold(true)
        ->textSize(22)
        ->textColor(config->colors.on_surface)
        ->build();
    text_col->add_view(title);

    auto sub = TextViewBuilder::create()
        ->text("Manage energy consumption, performance states and display sleep timeouts")
        ->textSize(13)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    text_col->add_view(sub);
    hero_row->add_view(text_col);
    hero_card->add_view(hero_row);
    root->add_view(hero_card);

    // Controls Card
    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_radius(config->metrics.corner_radius);
    card->set_padding(20);
    card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
    col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    col->set_divider_spacing(18);

    auto profile_title = TextViewBuilder::create()
        ->text("Energy Performance Profile")
        ->bold(true)
        ->textSize(15)
        ->textColor(config->colors.on_surface)
        ->build();
    col->add_view(profile_title);

    std::vector<std::string> profiles = {
        "Power Saver (Maximize battery life)",
        "Balanced (Adaptive dynamic governor)",
        "Performance (Uncapped frequency boost)"
    };
    for (size_t i = 0; i < profiles.size(); ++i) {
        bool selected = (i == 1);
        auto btn = ButtonBuilder::create()
            ->text((selected ? "●  " : "○  ") + profiles[i])
            ->textSize(14)
            ->textColor(selected ? config->colors.primary : config->colors.on_surface)
            ->backgroundColor(selected ? config->colors.surface_variant : config->colors.surface)
            ->cornerRadius(8)
            ->build();
        btn->set_layout_params(LayoutParams(static_cast<int>(LayoutDimension::MatchParent), 44));
        col->add_view(btn);
    }

    card->add_view(col);
    root->add_view(card);

    scroll->set_content_view(root);
    m_current_content = scroll;
    add_view(scroll);
}

void SettingDetailView::show_apps_list_page() {
    auto config = Config::get();
    auto root = std::make_shared<LinearLayout>(Orientation::Vertical);
    root->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    root->set_padding(24, 16, 24, 16);
    root->set_divider_spacing(12);

    auto title = TextViewBuilder::create()
        ->text("Installed Applications")
        ->bold(true)
        ->textSize(22)
        ->textColor(config->colors.on_surface)
        ->build();
    root->add_view(title);

    auto sub = TextViewBuilder::create()
        ->text("Select an application to view package details, path and desktop integration")
        ->textSize(13)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    root->add_view(sub);

    auto list = std::make_shared<ListView>();
    list->set_item_height(54);
    list->set_spacing(6);
    list->set_item_corner_radius(8);
    LayoutParams list_lp;
    list_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    list_lp.height = 0;
    list_lp.weight = 1.0f;
    list->set_layout_params(list_lp);

    auto apps = AppsBackend::get_installed_apps();
    for (const auto& app : apps) {
        auto item = ListItemViewBuilder::create()
            ->title(app.name)
            ->subtitle(app.desktop_path.empty() ? app.id : app.desktop_path)
            ->iconSource(app.icon_name.empty() ? "application-x-executable" : app.icon_name)
            ->iconSize(26)
            ->build();
        list->add_item(item);
    }

    list->set_on_item_click_listener([this, apps](size_t idx, std::shared_ptr<View>) {
        if (idx < apps.size()) {
            show_app_detail(apps[idx].id);
        }
    });

    root->add_view(list);
    m_current_content = root;
    add_view(root);
}

void SettingDetailView::show_app_detail(const std::string& app_id) {
    clear_views();
    m_subpage_open = true;
    if (m_on_subpage_changed) {
        m_on_subpage_changed(true);
    }

    auto config = Config::get();
    auto root = std::make_shared<LinearLayout>(Orientation::Vertical);
    root->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));

    // Back bar
    auto back_bar = std::make_shared<LinearLayout>(Orientation::Horizontal);
    back_bar->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    back_bar->set_padding(16, 8, 16, 4);

    auto btn_back = ButtonBuilder::create()
        ->text("← Back to Applications")
        ->textSize(13)
        ->textColor(config->colors.primary)
        ->backgroundColor(config->colors.surface_variant)
        ->cornerRadius(6)
        ->padding(10, 6)
        ->onClick([this]() {
            show_apps_list_page();
        })
        ->build();
    back_bar->add_view(btn_back);
    root->add_view(back_bar);

    auto app_view = std::make_shared<AppDetailView>();
    app_view->set_app_id(app_id);
    LayoutParams app_lp;
    app_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    app_lp.height = 0;
    app_lp.weight = 1.0f;
    app_view->set_layout_params(app_lp);

    root->add_view(app_view);
    m_current_content = root;
    add_view(root);
}

void SettingDetailView::show_default_apps_page() {
    auto config = Config::get();
    auto root = std::make_shared<LinearLayout>(Orientation::Vertical);
    root->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    root->set_padding(24, 16, 24, 16);
    root->set_divider_spacing(12);

    auto title = TextViewBuilder::create()
        ->text("Default Applications")
        ->bold(true)
        ->textSize(22)
        ->textColor(config->colors.on_surface)
        ->build();
    root->add_view(title);

    auto sub = TextViewBuilder::create()
        ->text("Configure default handlers for MIME types, protocols and desktop roles")
        ->textSize(13)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    root->add_view(sub);

    auto list = std::make_shared<ListView>();
    list->set_item_height(58);
    list->set_spacing(6);
    list->set_item_corner_radius(8);
    LayoutParams list_lp;
    list_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    list_lp.height = 0;
    list_lp.weight = 1.0f;
    list->set_layout_params(list_lp);

    auto roles = AppsBackend::get_default_roles();
    for (const auto& role : roles) {
        auto item = ListItemViewBuilder::create()
            ->title(role.title)
            ->subtitle(role.current_default_name.empty() ? "Default handler configured" : role.current_default_name)
            ->iconSource(role.icon_name.empty() ? "preferences-desktop-default-applications" : role.icon_name)
            ->iconSize(26)
            ->build();
        list->add_item(item);
    }

    list->set_on_item_click_listener([this, roles](size_t idx, std::shared_ptr<View>) {
        if (idx < roles.size()) {
            show_default_role_detail(roles[idx].role_id);
        }
    });

    root->add_view(list);
    m_current_content = root;
    add_view(root);
}

void SettingDetailView::show_default_role_detail(const std::string& role_id) {
    clear_views();
    m_subpage_open = true;
    if (m_on_subpage_changed) {
        m_on_subpage_changed(true);
    }

    auto config = Config::get();
    auto root = std::make_shared<LinearLayout>(Orientation::Vertical);
    root->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));

    auto back_bar = std::make_shared<LinearLayout>(Orientation::Horizontal);
    back_bar->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    back_bar->set_padding(16, 8, 16, 4);

    auto btn_back = ButtonBuilder::create()
        ->text("← Back to Default Applications")
        ->textSize(13)
        ->textColor(config->colors.primary)
        ->backgroundColor(config->colors.surface_variant)
        ->cornerRadius(6)
        ->padding(10, 6)
        ->onClick([this]() {
            show_default_apps_page();
        })
        ->build();
    back_bar->add_view(btn_back);
    root->add_view(back_bar);

    auto role_view = std::make_shared<DefaultAppDetailView>();
    role_view->set_role_id(role_id);
    LayoutParams role_lp;
    role_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    role_lp.height = 0;
    role_lp.weight = 1.0f;
    role_view->set_layout_params(role_lp);

    root->add_view(role_view);
    m_current_content = root;
    add_view(root);
}

void SettingDetailView::show_about_page() {
    auto config = Config::get();
    auto scroll = std::make_shared<ScrollView>();
    scroll->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));

    auto root = std::make_shared<LinearLayout>(Orientation::Vertical);
    root->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    root->set_divider_spacing(16);
    root->set_padding(24);

    // Hero Header
    auto hero_card = std::make_shared<CardView>();
    hero_card->set_card_background_color(config->colors.surface);
    hero_card->set_radius(config->metrics.corner_radius);
    hero_card->set_padding(20);
    hero_card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto hero_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    hero_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    hero_row->set_divider_spacing(18);

    auto icon = std::make_shared<ImageView>("preferences-system");
    icon->set_target_size(56);
    LayoutParams icon_lp(56, 56);
    icon_lp.gravity = Gravity::CenterVertical;
    hero_row->add_view(icon, icon_lp);

    auto text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams text_lp;
    text_lp.width = 0;
    text_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    text_lp.weight = 1.0f;
    text_col->set_layout_params(text_lp);

    auto title = TextViewBuilder::create()
        ->text("About Miqu Desktop")
        ->bold(true)
        ->textSize(22)
        ->textColor(config->colors.on_surface)
        ->build();
    text_col->add_view(title);

    auto sub = TextViewBuilder::create()
        ->text("A modern, modular tiling Wayland desktop environment")
        ->textSize(13)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    text_col->add_view(sub);
    hero_row->add_view(text_col);
    hero_card->add_view(hero_row);
    root->add_view(hero_card);

    // System Telemetry Card
    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_radius(config->metrics.corner_radius);
    card->set_padding(20);
    card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
    col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    col->set_divider_spacing(12);

    struct utsname un;
    std::string kernel_str = (uname(&un) == 0) ? std::string(un.release) : "Linux";

    std::vector<std::pair<std::string, std::string>> specs = {
        {"Operating System", "Arch Linux (Rolling Release)"},
        {"Desktop Environment", "Miqu DE"},
        {"Window Manager", "miquland (Wayland / wlroots)"},
        {"UI Toolkit", "miqutoolkit (C++20 / Cairo / Pango)"},
        {"Linux Kernel", kernel_str},
        {"Audio Engine", "PipeWire / WirePlumber"}
    };

    for (const auto& [label, val] : specs) {
        auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
        row->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));

        auto lbl_view = TextViewBuilder::create()
            ->text(label)
            ->textSize(13)
            ->textColor(config->colors.on_surface_variant)
            ->build();
        LayoutParams lp_l;
        lp_l.width = 200;
        lp_l.height = static_cast<int>(LayoutDimension::WrapContent);
        lbl_view->set_layout_params(lp_l);
        row->add_view(lbl_view);

        auto val_view = TextViewBuilder::create()
            ->text(val)
            ->bold(true)
            ->textSize(13)
            ->textColor(config->colors.on_surface)
            ->build();
        LayoutParams lp_r;
        lp_r.width = 0;
        lp_r.height = static_cast<int>(LayoutDimension::WrapContent);
        lp_r.weight = 1.0f;
        val_view->set_layout_params(lp_r);
        row->add_view(val_view);

        col->add_view(row);
    }

    card->add_view(col);
    root->add_view(card);

    scroll->set_content_view(root);
    m_current_content = scroll;
    add_view(scroll);
}

} // namespace miqu
