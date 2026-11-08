#pragma once

#include "settings_model.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <vector>
#include <memory>
#include <functional>
#include <string>

namespace miqu {

class MillerView : public LinearLayout {
public:
    MillerView();
    ~MillerView() override;

    void init();
    void sync_theme();

    bool on_key(const KeyPressEvent& event) override;

    // Navigation Controls
    bool navigate_up();
    bool navigate_down();
    bool navigate_enter();
    bool navigate_back();
    bool toggle_active();
    bool adjust_active_slider(float delta);
    bool focus_next_control();
    bool focus_prev_control();
    bool trigger_active_action();

    // State Inspection
    bool can_navigate_back() const { return !m_path.empty(); }
    std::string get_breadcrumb_path() const;
    std::string get_status_text() const;
    bool is_preview_focused() const { return m_preview_focused; }

    void set_on_state_changed(std::function<void()> cb) {
        m_on_state_changed = std::move(cb);
    }

private:
    void rebuild_columns();
    void rebuild_parent_column();
    void rebuild_current_column();
    void rebuild_preview_column();

    std::shared_ptr<SettingItem> get_current_selected_item() const;
    const std::vector<std::shared_ptr<SettingItem>>& get_current_list() const;
    void notify_state_changed();

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
    int m_preview_control_index = 0; // 0: primary switch, 1: secondary switch, 2: slider, 3: action button

    std::function<void()> m_on_state_changed;

    // UI containers (Backgroundless LinearLayout pillars)
    std::shared_ptr<LinearLayout> m_parent_layout;
    std::shared_ptr<LinearLayout> m_current_layout;
    std::shared_ptr<ScrollView> m_preview_scroll;
    std::shared_ptr<LinearLayout> m_preview_layout;

    // Reference to active widgets for direct manipulation
    std::shared_ptr<Switch> m_active_primary_switch;
    std::shared_ptr<Switch> m_active_secondary_switch;
    std::shared_ptr<Slider> m_active_slider;
    std::shared_ptr<TextView> m_active_slider_label;
    std::shared_ptr<Button> m_active_action_btn;
    std::shared_ptr<TextView> m_active_action_feedback;
    std::shared_ptr<PopupWindow> m_active_popup;
};

} // namespace miqu
