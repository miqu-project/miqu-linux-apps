#pragma once

#include "settings_model.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <vector>
#include <memory>
#include <functional>
#include <string>

namespace miqu {

class BluetoothAgent;
struct SoundState;
struct SoundDevice;

class MillerView : public LinearLayout {
public:
    MillerView();
    ~MillerView() override;

    void init();
    void sync_theme();

    bool on_key(const KeyPressEvent& event) override;
    bool on_mouse_move(int lx, int ly, const Rect& bounds) override;
    bool on_mouse_button(int lx, int ly, MouseButton button, bool pressed, const Rect& bounds) override;
    bool on_scroll(double delta) override;

    void set_preview_focused(bool focused);
    void set_preview_expanded(bool expanded);
    bool is_preview_expanded() const { return m_preview_expanded; }

    // Navigation Controls
    bool navigate_up();
    bool navigate_down();
    bool navigate_enter();
    bool navigate_back();
    bool trigger_active_action();

    // State Inspection
    bool can_navigate_back() const { return !m_search_query.empty() || m_preview_expanded || m_preview_focused || !m_path.empty(); }
    void set_search_query(const std::string& query);
    const std::string& get_search_query() const { return m_search_query; }
    bool is_searching() const { return !m_search_query.empty(); }
    std::string get_breadcrumb_path() const;
    bool is_preview_focused() const { return m_preview_focused; }

    void set_on_state_changed(std::function<void()> cb) {
        m_on_state_changed = std::move(cb);
    }

private:
    void rebuild_columns();
    void update_pillar_styles();
    void update_preview_ribbon();
    void rebuild_parent_column();
    void rebuild_current_column();
    void rebuild_preview_column();
    void deferred_rebuild_preview(int delay_ms = 15);
    void add_preview_card(std::shared_ptr<View> card);

    // Column Element Builders (Modular Single-Responsibility Helpers)
    std::shared_ptr<View> create_parent_hero_box(const std::shared_ptr<Config>& config);
    std::shared_ptr<View> create_empty_search_box(const std::shared_ptr<Config>& config);
    std::shared_ptr<View> create_list_row_item(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config);

    // Reusable Form Builders
    std::shared_ptr<LinearLayout> create_form_card(const std::shared_ptr<Config>& config);
    void add_form_divider(const std::shared_ptr<LinearLayout>& card, const std::shared_ptr<Config>& config);
    void add_form_toggle(const std::shared_ptr<LinearLayout>& card, const std::shared_ptr<Config>& config,
                        const std::string& title, const std::string& hint, bool checked,
                        std::function<void(bool)> on_toggle);
    void add_form_slider(const std::shared_ptr<LinearLayout>& card, const std::shared_ptr<Config>& config,
                        const std::string& title, const std::string& hint, float initial_val,
                        const std::string& initial_label, std::function<std::string(float)> on_change);
    void add_form_spinner(const std::shared_ptr<LinearLayout>& card, const std::shared_ptr<Config>& config,
                         const std::string& title, const std::string& hint,
                         const std::vector<std::string>& items, int cur_idx,
                         std::function<void(int, const std::string&)> on_select);

    // Modular Domain Preview Renderers (preview_renderers.cpp)
    void render_empty_preview(const std::shared_ptr<Config>& config);
    void render_category_children_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config);
    void render_sound_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config);
    std::shared_ptr<View> create_sound_volume_card(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config, const SoundState& state, bool is_speaker);
    std::shared_ptr<View> create_sound_presets_card(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config, int current_pct, bool is_speaker);
    std::shared_ptr<View> create_sound_mute_card(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config, bool is_muted, bool is_speaker);
    std::shared_ptr<View> create_sound_devices_card(const std::shared_ptr<Config>& config, const std::vector<SoundDevice>& devices, bool is_speaker);
    void render_wifi_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config);
    void render_bluetooth_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config);
    void render_ethernet_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config);
    void render_vpn_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config);
    void render_airplane_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config);
    void render_dns_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config);
    void render_firewall_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config);
    void render_app_network_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config);
    void render_brightness_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config);
    void render_tiling_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config);
    void render_wallpaper_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config);
    void render_keybindings_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config);
    void render_timeout_lock_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config);
    void render_night_light_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config);
    void render_battery_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config);
    void render_power_mode_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config);
    void render_monitors_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config);
    void render_installed_app_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config);
    void render_default_app_role_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config);
    void render_keyboards_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config);
    void render_repeat_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config);
    void render_pointers_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config);
    void render_touch_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config);
    void render_generic_fallback_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config);

    std::shared_ptr<SettingItem> get_current_selected_item() const;
    const std::vector<std::shared_ptr<SettingItem>>& get_current_list() const;
    void notify_state_changed();

    void handle_parent_item_click(size_t idx);
    void execute_sibling_swap(const std::shared_ptr<SettingItem>& item, size_t index);
    void execute_shift_up(size_t index);
    void cancel_pending_sibling_click();

    uint64_t m_last_sibling_click_time_ms = 0;
    int m_last_sibling_clicked_index = -1;
    uint64_t m_single_click_timer_id = 0;

    std::vector<std::shared_ptr<SettingItem>> m_root_tree;

    // Navigation stack
    // Path contains the ancestor chain: empty when at root
    std::vector<std::shared_ptr<SettingItem>> m_path;
    // Selection history: index for each depth level
    std::vector<int> m_history_indices;
    int m_current_index = 0;

    // Focus state: whether user has entered into the preview column controls
    bool m_preview_focused = false;
    bool m_preview_expanded = false;
    int m_last_mouse_x = -1;
    int m_last_mouse_y = -1;

    std::function<void()> m_on_state_changed;

    // UI containers (Backgroundless LinearLayout pillars with ListView / ScrollView)
    std::shared_ptr<LinearLayout> m_parent_pillar;
    std::shared_ptr<ListView> m_parent_list;
    std::shared_ptr<View> m_parent_hero;

    std::shared_ptr<LinearLayout> m_current_pillar;
    std::shared_ptr<ListView> m_current_list;
    std::shared_ptr<View> m_empty_search_box;

    std::shared_ptr<LinearLayout> m_preview_pillar;
    std::shared_ptr<LinearLayout> m_preview_ribbon;
    std::shared_ptr<CardView> m_preview_toggle_btn;
    std::shared_ptr<TextView> m_preview_toggle_icon;
    std::shared_ptr<TextView> m_preview_title;

    std::shared_ptr<ListView> m_category_list;
    std::shared_ptr<ScrollView> m_preview_scroll;
    std::shared_ptr<LinearLayout> m_preview_layout;

    // Reference to active widgets for direct manipulation
    std::shared_ptr<Slider> m_active_slider;
    std::shared_ptr<TextView> m_active_slider_label;
    std::shared_ptr<TextView> m_active_action_feedback;
    std::string m_search_query;
    std::vector<std::shared_ptr<SettingItem>> m_search_results;
    std::shared_ptr<PopupWindow> m_active_popup;
    std::shared_ptr<PopupWindow> m_active_pairing_popup;

    void init_bluetooth_agent();
    void show_numeric_comparison_dialog(const std::string& dev_name, const std::string& mac, uint32_t passkey, std::function<void(bool)> cb);
    void show_passkey_display_dialog(const std::string& dev_name, const std::string& mac, uint32_t passkey);
    void show_authorization_dialog(const std::string& dev_name, const std::string& mac, std::function<void(bool)> cb);
    void show_pin_request_dialog(const std::string& dev_name, const std::string& mac, std::function<void(const std::string&)> cb);

    std::unique_ptr<BluetoothAgent> m_bt_agent;
};

} // namespace miqu
