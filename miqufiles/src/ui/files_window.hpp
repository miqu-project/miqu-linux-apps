#pragma once

#include "model/file_item.hpp"
#include "ui/file_preview_view.hpp"
#include "ui/status_bar_view.hpp"
#include <miqutoolkit/core/window.hpp>
#include <miqutoolkit/core/app_engine.hpp>
#include <miqutoolkit/view/toolbar.hpp>
#include <miqutoolkit/view/list_view.hpp>
#include <miqutoolkit/view/card_view.hpp>
#include <miqutoolkit/view/linear_layout.hpp>
#include <miqutoolkit/view/search_view.hpp>
#include <memory>
#include <vector>
#include <filesystem>
#include <chrono>

namespace miqu {

class FilesWindow : public std::enable_shared_from_this<FilesWindow> {
public:
    static std::shared_ptr<FilesWindow> create(std::shared_ptr<AppEngine> engine, const std::string& start_path = "");
    ~FilesWindow() = default;

    bool init();
    void show();

    void navigate_to(const std::filesystem::path& path, bool record_history = true);
    void navigate_up();
    void navigate_back();
    void navigate_forward();
    void open_selected();
    void toggle_hidden();
    void toggle_search();
    void refresh();

    void set_selected_index(int index);
    const FileItem* get_selected_item() const;

private:
    FilesWindow(std::shared_ptr<AppEngine> engine, std::filesystem::path start_path);

    void build_ui();
    void populate_parent_column();
    void populate_current_column();
    void update_preview_and_status();
    void apply_filter();

    std::shared_ptr<AppEngine> m_engine;
    std::shared_ptr<Window> m_window;

    std::filesystem::path m_current_path;
    std::vector<FileItem> m_current_items;
    std::vector<FileItem> m_filtered_items;
    std::vector<FileItem> m_parent_items;

    int m_selected_index = 0;
    int m_parent_selected_index = -1;
    bool m_show_hidden = false;

    std::vector<std::filesystem::path> m_history_back;
    std::vector<std::filesystem::path> m_history_forward;

    std::string m_filter_query;
    bool m_search_visible = false;

    // Double click tracking
    std::chrono::steady_clock::time_point m_last_click_time{};
    int m_last_clicked_index = -1;

    // UI Widgets
    std::shared_ptr<Toolbar> m_toolbar;
    std::shared_ptr<SearchView> m_search_view;
    std::shared_ptr<LinearLayout> m_search_container;

    std::shared_ptr<TextView> m_parent_title;
    std::shared_ptr<TextView> m_current_title;
    std::shared_ptr<TextView> m_preview_title;

    std::shared_ptr<ListView> m_parent_list;
    std::shared_ptr<ListView> m_current_list;
    std::shared_ptr<FilePreviewView> m_preview_view;
    std::shared_ptr<StatusBarView> m_status_bar;

    std::shared_ptr<ImageButton> m_btn_hidden;
    std::shared_ptr<ImageButton> m_btn_search;
};

} // namespace miqu
