#pragma once

#include "model/setting_item.hpp"
#include <miqutoolkit/view/frame_layout.hpp>
#include <miqutoolkit/view/scroll_view.hpp>
#include <miqutoolkit/view/card_view.hpp>
#include <miqutoolkit/view/linear_layout.hpp>
#include <miqutoolkit/view/text_view.hpp>
#include <miqutoolkit/view/image_view.hpp>
#include <miqutoolkit/view/button.hpp>
#include <miqutoolkit/view/switch.hpp>
#include <miqutoolkit/view/slider.hpp>
#include <memory>
#include <functional>
#include <string>

namespace miqu {

// SettingDetailView renders the active settings page/fragment full-width.
// It directly displays the domain module corresponding to the selected sidebar page.
class SettingDetailView : public FrameLayout {
public:
    SettingDetailView();
    ~SettingDetailView() override = default;

    void show_page(const std::string& page_id);
    void clear();

    std::string get_current_page_id() const { return m_current_page_id; }

    bool is_subpage_open() const { return m_subpage_open; }
    void navigate_back_to_main_page();

    void set_on_subpage_changed(std::function<void(bool in_subpage)> cb) {
        m_on_subpage_changed = std::move(cb);
    }

private:
    void show_empty_state();
    void show_wallpaper_page();
    void show_monitors_page();
    void show_brightness_page();
    void show_sound_page();
    void show_wifi_page();
    void show_bluetooth_page();
    void show_ethernet_page();
    void show_wm_layout_page();
    void show_wm_gaps_page();
    void show_wm_borders_page();
    void show_wm_effects_page();
    void show_wm_control_page();
    void show_keyboard_devices_page();
    void show_keyboard_repeat_page();
    void show_pointers_page();
    void show_power_page();
    void show_apps_list_page();
    void show_app_detail(const std::string& app_id);
    void show_default_apps_page();
    void show_default_role_detail(const std::string& role_id);
    void show_firewall_page();
    void show_about_page();

    std::string m_current_page_id;
    bool m_subpage_open = false;
    std::shared_ptr<View> m_current_content;
    std::function<void(bool)> m_on_subpage_changed;
};

} // namespace miqu
