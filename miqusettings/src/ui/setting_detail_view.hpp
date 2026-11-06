#pragma once

#include "model/setting_item.hpp"
#include <miqutoolkit/view/frame_layout.hpp>
#include <miqutoolkit/view/list_view.hpp>
#include <miqutoolkit/view/scroll_view.hpp>
#include <miqutoolkit/view/card_view.hpp>
#include <miqutoolkit/view/text_view.hpp>
#include <miqutoolkit/view/image_view.hpp>
#include <miqutoolkit/view/switch.hpp>
#include <miqutoolkit/view/slider.hpp>
#include <miqutoolkit/view/button.hpp>
#include <memory>
#include <functional>
namespace miqu {

class SettingDetailView : public FrameLayout {
public:
    SettingDetailView();
    ~SettingDetailView() override = default;

    void update_item(const std::shared_ptr<SettingItem>& item, bool is_fullscreen = false);
    void clear();

    std::shared_ptr<SettingItem> get_current_item() const { return m_current_item; }
    std::string get_current_id() const { return m_current_item ? m_current_item->id : ""; }

    void set_on_navigate_into(std::function<void(const std::shared_ptr<SettingItem>&)> cb) {
        m_on_navigate_into = std::move(cb);
    }

    void set_on_setting_changed(std::function<void(const std::shared_ptr<SettingItem>&)> cb) {
        m_on_setting_changed = std::move(cb);
    }

    void set_on_request_fullscreen(std::function<void()> cb) {
        m_on_request_fullscreen = std::move(cb);
    }

private:
    void show_empty_state();
    void show_category_preview(const std::shared_ptr<SettingItem>& item);
    void show_leaf_detail(const std::shared_ptr<SettingItem>& item, bool is_fullscreen);
    void show_app_detail(const std::shared_ptr<SettingItem>& item, bool is_fullscreen);
    void show_default_role_detail(const std::shared_ptr<SettingItem>& item, bool is_fullscreen);
    void show_bluetooth_detail(bool is_fullscreen);
    void show_ethernet_detail(bool is_fullscreen);
    void show_wifi_detail(bool is_fullscreen);
    void show_airplane_detail(bool is_fullscreen);
    void show_monitors_detail(const std::shared_ptr<SettingItem>& item, bool is_fullscreen);
    void show_brightness_detail(bool is_fullscreen);
    void show_gamma_detail(bool is_fullscreen);
    void show_screen_lock_detail(bool is_fullscreen);
    void show_timeout_detail(bool is_fullscreen);
    void show_wm_layout_detail(bool is_fullscreen);
    void show_wm_gaps_detail(bool is_fullscreen);
    void show_wm_borders_detail(bool is_fullscreen);
    void show_wm_effects_detail(bool is_fullscreen);
    void show_wm_control_detail(bool is_fullscreen);
    void show_keyboard_devices_detail(bool is_fullscreen);
    void show_keyboard_repeat_detail(bool is_fullscreen);
    void show_pointers_detail(bool is_fullscreen);

    std::shared_ptr<SettingItem> m_current_item;
    std::shared_ptr<View> m_current_content;

    std::function<void(const std::shared_ptr<SettingItem>&)> m_on_navigate_into;
    std::function<void(const std::shared_ptr<SettingItem>&)> m_on_setting_changed;
    std::function<void()> m_on_request_fullscreen;
};

} // namespace miqu
