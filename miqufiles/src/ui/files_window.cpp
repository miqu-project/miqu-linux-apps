#include "ui/files_window.hpp"
#include "ui/column_card_view.hpp"
#include "config/files_config.hpp"
#include <miqutoolkit/core/fs_utils.hpp>
#include <miqutoolkit/core/config.hpp>
#include <miqutoolkit/view/list_item_view.hpp>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <unistd.h>
#include <sys/wait.h>
#include <iostream>

namespace miqu {

namespace fs = std::filesystem;

static std::string format_display_path(const fs::path& p) {
    const char* home = std::getenv("HOME");
    std::string path_str = p.string();
    if (home && *home) {
        std::string home_str(home);
        if (path_str == home_str) {
            return "~";
        }
        if (path_str.rfind(home_str + "/", 0) == 0) {
            return "~" + path_str.substr(home_str.size());
        }
    }
    return path_str;
}

std::shared_ptr<FilesWindow> FilesWindow::create(std::shared_ptr<AppEngine> engine, const std::string& start_path) {
    std::string initial_path = start_path;
    if (initial_path.empty()) {
        initial_path = FilesConfig::get().default_path;
    }
    std::string expanded = FsUtils::expand_user_path(initial_path);
    std::error_code ec;
    fs::path target_p = fs::canonical(expanded, ec);
    if (ec || !fs::is_directory(target_p)) {
        target_p = fs::canonical(std::getenv("HOME") ? std::getenv("HOME") : "/", ec);
    }

    auto win = std::shared_ptr<FilesWindow>(new FilesWindow(std::move(engine), target_p));
    if (!win->init()) {
        return nullptr;
    }
    return win;
}

FilesWindow::FilesWindow(std::shared_ptr<AppEngine> engine, fs::path start_path)
    : m_engine(std::move(engine)),
      m_current_path(std::move(start_path)),
      m_show_hidden(FilesConfig::get().show_hidden) {}

bool FilesWindow::init() {
    build_ui();
    navigate_to(m_current_path, false);
    return m_window != nullptr;
}

void FilesWindow::show() {
    if (m_window) {
        m_window->show();
    }
}

void FilesWindow::build_ui() {
    auto config = Config::get();
    auto& fcfg = FilesConfig::get();

    // 1. Standard Toolbar
    m_toolbar = ToolbarBuilder::create()
        ->title("Files")
        ->subtitle(format_display_path(m_current_path))
        ->onBack([this]() { navigate_up(); })
        ->onRefresh([this]() { refresh(); })
        ->onClose([this]() {
            if (m_window) m_window->request_close();
            else m_engine->quit();
        })
        ->build();
    m_toolbar->set_back_tooltip("Up to parent folder (h / Backspace)");

    // Home action
    auto btn_home = m_toolbar->add_action(icons::HOME, [this]() {
        const char* h = std::getenv("HOME");
        if (h) navigate_to(h);
    });
    btn_home->set_tooltip("Home Directory (~)");

    // Hidden files toggle action
    m_btn_hidden = m_toolbar->add_action(m_show_hidden ? "•*" : ".*", [this]() {
        toggle_hidden();
    });
    m_btn_hidden->set_tooltip("Toggle Hidden Files (.)");

    // Search action
    m_btn_search = m_toolbar->add_action(icons::SEARCH, [this]() {
        toggle_search();
    });
    m_btn_search->set_tooltip("Filter / Search Files (/)");

    // 2. Search Container (collapsible)
    m_search_container = std::make_shared<LinearLayout>(Orientation::Horizontal);
    m_search_container->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    m_search_container->set_padding(8, 0, 8, 6);
    m_search_container->set_visibility(Visibility::Gone);

    m_search_view = std::make_shared<SearchView>();
    m_search_view->set_hint("Filter items in current folder...");
    m_search_view->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        36
    ));
    m_search_view->set_on_query_text_listener([this](const std::string& query) {
        m_filter_query = query;
        apply_filter();
    });
    m_search_container->add_view(m_search_view);

    // 3. Three Columns Architecture (Miller Columns)
    auto columns_container = std::make_shared<LinearLayout>(Orientation::Horizontal);
    columns_container->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        0,
        1.0f
    ));
    columns_container->set_padding(8, 4, 8, 4);
    columns_container->set_divider_spacing(8);

    // --- LEFT COLUMN: Parent Directory (weight 1.0) ---
    auto col_parent_card = std::make_shared<ColumnCardView>();
    col_parent_card->set_card_background_color(config->colors.surface_variant);
    col_parent_card->set_radius(config->metrics.corner_radius);
    col_parent_card->set_stroke(1, config->colors.outline_variant);
    col_parent_card->set_elevation(0);
    col_parent_card->set_padding(6);
    LayoutParams col_parent_lp;
    col_parent_lp.width = 0;
    col_parent_lp.height = static_cast<int>(LayoutDimension::MatchParent);
    col_parent_lp.weight = 1.0f;
    col_parent_card->set_layout_params(col_parent_lp);

    auto col_parent_layout = std::make_shared<LinearLayout>(Orientation::Vertical);
    col_parent_layout->set_divider_spacing(4);

    m_parent_title = TextViewBuilder::create()
        ->text("Parent")
        ->bold(true)
        ->textSize(10)
        ->textColor(config->colors.outline)
        ->build();
    col_parent_layout->add_view(m_parent_title);

    m_parent_list = std::make_shared<ListView>();
    m_parent_list->set_item_height(32);
    m_parent_list->set_spacing(2);
    m_parent_list->set_item_corner_radius(4);
    m_parent_list->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    m_parent_list->set_on_item_click_listener([this](size_t idx, std::shared_ptr<View>) {
        if (idx < m_parent_items.size() && m_parent_items[idx].is_dir) {
            navigate_to(m_parent_items[idx].path);
        }
    });
    col_parent_layout->add_view(m_parent_list);
    col_parent_card->add_view(col_parent_layout);

    // --- CENTER COLUMN: Active Directory (weight 1.6) ---
    auto col_current_card = std::make_shared<ColumnCardView>();
    col_current_card->set_card_background_color(config->colors.surface);
    col_current_card->set_radius(config->metrics.corner_radius);
    col_current_card->set_stroke(1, config->colors.outline);
    col_current_card->set_elevation(0);
    col_current_card->set_padding(6);
    LayoutParams col_current_lp;
    col_current_lp.width = 0;
    col_current_lp.height = static_cast<int>(LayoutDimension::MatchParent);
    col_current_lp.weight = 1.6f;
    col_current_card->set_layout_params(col_current_lp);

    auto col_current_layout = std::make_shared<LinearLayout>(Orientation::Vertical);
    col_current_layout->set_divider_spacing(4);

    m_current_title = TextViewBuilder::create()
        ->text("Current")
        ->bold(true)
        ->textSize(11)
        ->textColor(config->colors.primary)
        ->build();
    col_current_layout->add_view(m_current_title);

    m_current_list = std::make_shared<ListView>();
    m_current_list->set_item_height(34);
    m_current_list->set_spacing(2);
    m_current_list->set_item_corner_radius(6);
    m_current_list->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    m_current_list->set_on_item_click_listener([this](size_t idx, std::shared_ptr<View>) {
        auto now = std::chrono::steady_clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_click_time).count();
        if (m_last_clicked_index == static_cast<int>(idx) && diff < 400) {
            // Double click: open
            open_selected();
        } else {
            // Single click: select and preview
            m_last_click_time = now;
            m_last_clicked_index = static_cast<int>(idx);
            set_selected_index(static_cast<int>(idx));
        }
    });
    col_current_layout->add_view(m_current_list);
    col_current_card->add_view(col_current_layout);

    // --- RIGHT COLUMN: Preview & Child View (weight 2.0) ---
    auto col_preview_card = std::make_shared<ColumnCardView>();
    col_preview_card->set_card_background_color(config->colors.surface_variant);
    col_preview_card->set_radius(config->metrics.corner_radius);
    col_preview_card->set_stroke(1, config->colors.outline_variant);
    col_preview_card->set_elevation(0);
    col_preview_card->set_padding(6);
    LayoutParams col_preview_lp;
    col_preview_lp.width = 0;
    col_preview_lp.height = static_cast<int>(LayoutDimension::MatchParent);
    col_preview_lp.weight = 2.0f;
    col_preview_card->set_layout_params(col_preview_lp);

    auto col_preview_layout = std::make_shared<LinearLayout>(Orientation::Vertical);
    col_preview_layout->set_divider_spacing(4);

    m_preview_title = TextViewBuilder::create()
        ->text("Preview")
        ->bold(true)
        ->textSize(10)
        ->textColor(config->colors.outline)
        ->build();
    col_preview_layout->add_view(m_preview_title);

    m_preview_view = std::make_shared<FilePreviewView>();
    m_preview_view->set_on_open_listener([this](const fs::path& p) {
        pid_t pid = fork();
        if (pid == 0) {
            setsid();
            execlp("xdg-open", "xdg-open", p.c_str(), nullptr);
            _exit(1);
        }
    });
    col_preview_layout->add_view(m_preview_view);
    col_preview_card->add_view(col_preview_layout);

    // Add columns
    columns_container->add_view(col_parent_card);
    columns_container->add_view(col_current_card);
    columns_container->add_view(col_preview_card);

    // 4. Status Bar
    m_status_bar = std::make_shared<StatusBarView>();

    // 5. Root Layout
    auto root_container = std::make_shared<LinearLayout>(Orientation::Vertical);
    root_container->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    root_container->set_background_color(config->colors.background);

    root_container->add_view(m_toolbar);
    root_container->add_view(m_search_container);
    root_container->add_view(columns_container);
    root_container->add_view(m_status_bar);

    // 6. Toplevel Window Creation
    m_window = WindowBuilder::create()
        ->title("Files - miqufiles")
        ->appId("miqufiles")
        ->role(WindowRole::Toplevel)
        ->preferredSize(fcfg.window_width, fcfg.window_height)
        ->closeOnEscape(false)
        ->contentView(root_container)
        ->onKey([this](const KeyPressEvent& ev) {
            if (!ev.pressed) return;

            // Handle key events for search view when visible
            if (m_search_visible) {
                if (ev.keysym == XKB_KEY_Escape) {
                    toggle_search();
                    return;
                }
                if (ev.keysym == XKB_KEY_Return || ev.keysym == XKB_KEY_KP_Enter) {
                    open_selected();
                    return;
                }
            }

            // Global navigation keys
            switch (ev.keysym) {
                case XKB_KEY_Left:
                case XKB_KEY_h:
                case XKB_KEY_BackSpace:
                    if (!m_search_visible) {
                        navigate_up();
                        return;
                    }
                    break;

                case XKB_KEY_Right:
                case XKB_KEY_l:
                    if (!m_search_visible) {
                        open_selected();
                        return;
                    }
                    break;

                case XKB_KEY_Return:
                case XKB_KEY_KP_Enter:
                    open_selected();
                    return;

                case XKB_KEY_Up:
                case XKB_KEY_k:
                    if (!m_search_visible || ev.keysym == XKB_KEY_Up) {
                        set_selected_index(m_selected_index - 1);
                        return;
                    }
                    break;

                case XKB_KEY_Down:
                case XKB_KEY_j:
                    if (!m_search_visible || ev.keysym == XKB_KEY_Down) {
                        set_selected_index(m_selected_index + 1);
                        return;
                    }
                    break;

                case XKB_KEY_Home:
                case XKB_KEY_g:
                    if (!m_search_visible) {
                        set_selected_index(0);
                        return;
                    }
                    break;

                case XKB_KEY_End:
                case XKB_KEY_G:
                    if (!m_search_visible) {
                        set_selected_index(static_cast<int>(m_filtered_items.size()) - 1);
                        return;
                    }
                    break;

                case XKB_KEY_Page_Up:
                    set_selected_index(m_selected_index - 10);
                    return;

                case XKB_KEY_Page_Down:
                    set_selected_index(m_selected_index + 10);
                    return;

                case XKB_KEY_period:
                    if (!m_search_visible) {
                        toggle_hidden();
                        return;
                    }
                    break;

                case XKB_KEY_slash:
                    if (!m_search_visible) {
                        toggle_search();
                        return;
                    }
                    break;

                case XKB_KEY_asciitilde:
                    if (!m_search_visible) {
                        const char* h = std::getenv("HOME");
                        if (h) navigate_to(h);
                        return;
                    }
                    break;

                case XKB_KEY_F5:
                    refresh();
                    return;

                case XKB_KEY_q:
                    if (!m_search_visible) {
                        if (m_window) m_window->request_close();
                        return;
                    }
                    break;

                case XKB_KEY_Escape:
                    if (m_search_visible) {
                        toggle_search();
                    } else if (!m_filter_query.empty()) {
                        m_filter_query.clear();
                        if (m_search_view) m_search_view->set_query("");
                        apply_filter();
                    } else {
                        if (m_window) m_window->request_close();
                    }
                    return;

                default:
                    // Check Ctrl shortcuts
                    if (ev.modifiers & static_cast<uint32_t>(KeyboardModifier::Control)) {
                        if (ev.keysym == XKB_KEY_h || ev.keysym == XKB_KEY_H) {
                            toggle_hidden();
                            return;
                        }
                        if (ev.keysym == XKB_KEY_r || ev.keysym == XKB_KEY_R) {
                            refresh();
                            return;
                        }
                        if (ev.keysym == XKB_KEY_f || ev.keysym == XKB_KEY_F) {
                            toggle_search();
                            return;
                        }
                    }
                    break;
            }
        })
        ->build();
}

void FilesWindow::navigate_to(const fs::path& p, bool record_history) {
    std::error_code ec;
    fs::path canonical_p = fs::canonical(p, ec);
    if (ec || !fs::is_directory(canonical_p, ec)) {
        return;
    }

    if (record_history && canonical_p != m_current_path) {
        m_history_back.push_back(m_current_path);
        m_history_forward.clear();
    }

    m_current_path = canonical_p;

    // Update toolbar display
    m_toolbar->set_title(m_current_path.filename().empty() ? "/" : m_current_path.filename().string());
    m_toolbar->set_subtitle(format_display_path(m_current_path));
    m_toolbar->set_back_visible(m_current_path != m_current_path.root_path());

    // Update parent column
    populate_parent_column();

    // Scan active directory
    m_current_items = FileItem::scan_directory(m_current_path, m_show_hidden);
    apply_filter();
}

void FilesWindow::navigate_up() {
    if (m_current_path != m_current_path.root_path()) {
        navigate_to(m_current_path.parent_path());
    }
}

void FilesWindow::navigate_back() {
    if (!m_history_back.empty()) {
        auto prev = m_history_back.back();
        m_history_back.pop_back();
        m_history_forward.push_back(m_current_path);
        navigate_to(prev, false);
    }
}

void FilesWindow::navigate_forward() {
    if (!m_history_forward.empty()) {
        auto next = m_history_forward.back();
        m_history_forward.pop_back();
        m_history_back.push_back(m_current_path);
        navigate_to(next, false);
    }
}

void FilesWindow::refresh() {
    navigate_to(m_current_path, false);
}

void FilesWindow::toggle_hidden() {
    m_show_hidden = !m_show_hidden;
    if (m_btn_hidden) {
        m_btn_hidden->set_icon(m_show_hidden ? "•*" : ".*");
    }
    refresh();
}

void FilesWindow::toggle_search() {
    m_search_visible = !m_search_visible;
    if (m_search_container) {
        m_search_container->set_visibility(m_search_visible ? Visibility::Visible : Visibility::Gone);
    }
    if (m_search_visible && m_search_view) {
        m_search_view->set_focused(true);
    } else {
        m_filter_query.clear();
        if (m_search_view) m_search_view->set_query("");
        apply_filter();
    }
}

void FilesWindow::populate_parent_column() {
    fs::path parent_p = m_current_path.parent_path();
    if (parent_p.empty() || m_current_path == m_current_path.root_path()) {
        m_parent_items.clear();
        m_parent_list->clear_items();
        m_parent_title->set_text("Root (No Parent)");
        return;
    }

    std::string parent_name = parent_p.filename().empty() ? "/" : parent_p.filename().string();
    m_parent_title->set_text(parent_name);

    m_parent_items = FileItem::scan_directory(parent_p, m_show_hidden);
    m_parent_list->clear_items();

    int found_selected = -1;
    for (size_t i = 0; i < m_parent_items.size(); ++i) {
        const auto& item = m_parent_items[i];
        auto view = ListItemViewBuilder::create()
            ->title(item.name)
            ->trailingText(item.size_formatted)
            ->iconSource(item.icon_name)
            ->iconSize(18)
            ->build();
        m_parent_list->add_item(view);

        if (item.path == m_current_path) {
            found_selected = static_cast<int>(i);
        }
    }

    m_parent_selected_index = found_selected;
    m_parent_list->set_selected_index(found_selected);
}

void FilesWindow::apply_filter() {
    m_filtered_items.clear();

    if (m_filter_query.empty()) {
        m_filtered_items = m_current_items;
    } else {
        std::string query_lower = m_filter_query;
        std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(), ::tolower);

        for (const auto& item : m_current_items) {
            std::string name_lower = item.name;
            std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
            if (name_lower.find(query_lower) != std::string::npos) {
                m_filtered_items.push_back(item);
            }
        }
    }

    populate_current_column();
}

void FilesWindow::populate_current_column() {
    std::string cur_name = m_current_path.filename().empty() ? "/" : m_current_path.filename().string();
    std::string title_str = cur_name + " (" + std::to_string(m_filtered_items.size()) + ")";
    m_current_title->set_text(title_str);

    m_current_list->clear_items();

    for (const auto& item : m_filtered_items) {
        auto view = ListItemViewBuilder::create()
            ->title(item.name)
            ->trailingText(item.size_formatted)
            ->iconSource(item.icon_name)
            ->iconSize(20)
            ->build();
        m_current_list->add_item(view);
    }

    set_selected_index(m_filtered_items.empty() ? -1 : 0);
}

void FilesWindow::set_selected_index(int index) {
    if (m_filtered_items.empty()) {
        m_selected_index = -1;
        m_current_list->set_selected_index(-1);
    } else {
        m_selected_index = std::clamp(index, 0, static_cast<int>(m_filtered_items.size()) - 1);
        m_current_list->set_selected_index(m_selected_index);
    }

    update_preview_and_status();
}

const FileItem* FilesWindow::get_selected_item() const {
    if (m_selected_index >= 0 && m_selected_index < static_cast<int>(m_filtered_items.size())) {
        return &m_filtered_items[m_selected_index];
    }
    return nullptr;
}

void FilesWindow::update_preview_and_status() {
    const FileItem* item = get_selected_item();

    if (item) {
        m_preview_title->set_text(item->name);
        m_preview_view->update_preview(item, m_show_hidden);
    } else {
        m_preview_title->set_text("Preview");
        m_preview_view->update_preview(nullptr, m_show_hidden);
    }

    m_status_bar->update_stats(m_filtered_items, item, m_current_path);
}

void FilesWindow::open_selected() {
    const FileItem* item = get_selected_item();
    if (!item) return;

    if (item->is_dir) {
        navigate_to(item->path);
    } else {
        pid_t pid = fork();
        if (pid == 0) {
            setsid();
            execlp("xdg-open", "xdg-open", item->path.c_str(), nullptr);
            _exit(1);
        }
    }
}

} // namespace miqu
