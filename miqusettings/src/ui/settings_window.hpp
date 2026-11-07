#pragma once

#include "model/setting_item.hpp"
#include "ui/setting_detail_view.hpp"
#include <miqutoolkit/core/window.hpp>
#include <miqutoolkit/core/app_engine.hpp>
#include <miqutoolkit/view/toolbar.hpp>
#include <miqutoolkit/view/list_view.hpp>
#include <miqutoolkit/view/card_view.hpp>
#include <miqutoolkit/view/linear_layout.hpp>
#include <memory>
#include <vector>
#include <string>

namespace miqu {

// Conventional Settings Window:
//   Left: Fixed sidebar list of settings pages
//   Right: Active domain page (fragment) directly displayed full-width
class SettingsWindow : public std::enable_shared_from_this<SettingsWindow> {
public:
    static std::shared_ptr<SettingsWindow> create(std::shared_ptr<AppEngine> engine);
    ~SettingsWindow() = default;

    bool init();
    void show();
    void sync_theme();
    void navigate_to_id(const std::string& target_id);

private:
    explicit SettingsWindow(std::shared_ptr<AppEngine> engine);

    void build_ui();
    void select_page(int index);

    std::shared_ptr<AppEngine>   m_engine;
    std::shared_ptr<Window>      m_window;
    std::shared_ptr<SettingItem> m_root_node;

    int m_selected_index = 0;

    std::shared_ptr<LinearLayout>      m_root_container;
    std::shared_ptr<Toolbar>           m_toolbar;
    std::shared_ptr<LinearLayout>      m_body;
    std::shared_ptr<CardView>          m_sidebar_card;
    std::shared_ptr<ListView>          m_sidebar_list;
    std::shared_ptr<SettingDetailView> m_detail_view;
};

} // namespace miqu
