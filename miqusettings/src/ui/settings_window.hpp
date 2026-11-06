#pragma once

#include "model/setting_item.hpp"
#include "ui/setting_detail_view.hpp"
#include "ui/column_card_view.hpp"
#include <miqutoolkit/core/window.hpp>
#include <miqutoolkit/core/app_engine.hpp>
#include <miqutoolkit/view/toolbar.hpp>
#include <miqutoolkit/view/list_view.hpp>
#include <miqutoolkit/view/card_view.hpp>
#include <miqutoolkit/view/linear_layout.hpp>
#include <miqutoolkit/view/search_view.hpp>
#include <miqutoolkit/view/text_view.hpp>
#include <miqutoolkit/view/image_view.hpp>
#include <miqutoolkit/view/image_button.hpp>
#include <miqutoolkit/view/button.hpp>
#include <memory>
#include <vector>
#include <string>

namespace miqu {

class SettingsWindow : public std::enable_shared_from_this<SettingsWindow> {
public:
    static std::shared_ptr<SettingsWindow> create(std::shared_ptr<AppEngine> engine);
    ~SettingsWindow() = default;

    bool init();
    void show();

    void navigate_to(const std::shared_ptr<SettingItem>& node, bool record_history = true);
    void navigate_up();
    void navigate_back();
    void navigate_forward();
    void on_left_action();
    void on_right_action();
    void open_selected();
    void enter_fullscreen_detail();
    void exit_fullscreen_detail();
    bool is_fullscreen_detail() const { return m_fullscreen_detail; }
    void toggle_search();
    void refresh();
    void sync_theme();
    void navigate_to_id(const std::string& target_id);

    void set_selected_index(int index);
    std::shared_ptr<SettingItem> get_selected_item() const;

private:
    explicit SettingsWindow(std::shared_ptr<AppEngine> engine);

    void build_ui();
    void populate_parent_column();
    void populate_current_column();
    void update_preview_and_status();
    void update_breadcrumb();
    void apply_filter();
    void ensure_children_loaded(const std::shared_ptr<SettingItem>& node);

    std::shared_ptr<AppEngine> m_engine;
    std::shared_ptr<Window> m_window;

    std::shared_ptr<SettingItem> m_root_node;
    std::shared_ptr<SettingItem> m_current_node;
    std::vector<std::shared_ptr<SettingItem>> m_filtered_items;
    std::vector<std::shared_ptr<SettingItem>> m_parent_items;

    std::vector<std::shared_ptr<SettingItem>> m_history_back;
    std::vector<std::shared_ptr<SettingItem>> m_history_forward;

    int m_selected_index = -1;
    int m_parent_selected_index = -1;
    bool m_fullscreen_detail = false;
    bool m_search_visible = false;
    std::string m_filter_query;

    // UI hierarchy
    std::shared_ptr<LinearLayout> m_root_container;
    std::shared_ptr<Toolbar> m_toolbar;
    std::shared_ptr<LinearLayout> m_path_strip;
    std::shared_ptr<TextView> m_path_text;
    std::shared_ptr<ImageButton> m_btn_search;
    std::shared_ptr<LinearLayout> m_search_container;
    std::shared_ptr<SearchView> m_search_view;

    std::shared_ptr<LinearLayout> m_columns_container;
    std::shared_ptr<ColumnCardView> m_col_parent_card;
    std::shared_ptr<ListView> m_parent_list;
    std::shared_ptr<LinearLayout> m_parent_root_hero;

    std::shared_ptr<ColumnCardView> m_col_current_card;
    std::shared_ptr<ListView> m_current_list;

    std::shared_ptr<ColumnCardView> m_col_preview_card;
    std::shared_ptr<SettingDetailView> m_detail_view;
};

} // namespace miqu
