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
    sync_theme();
    navigate_to(m_current_path, false);
    return m_window != nullptr;
}

void FilesWindow::show() {
    if (m_window) {
        m_window->show();
    }
}

void FilesWindow::sync_theme() {
    auto config = Config::get();
    if (!config) return;

    if (m_col_parent_card) {
        m_col_parent_card->set_card_background_color(config->colors.surface_variant);
        m_col_parent_card->set_stroke(1, config->colors.outline_variant);
        m_col_parent_card->set_radius(config->metrics.corner_radius);
    }
    if (m_col_current_card) {
        m_col_current_card->set_card_background_color(config->colors.surface);
        m_col_current_card->set_stroke(1, config->colors.outline_variant);
        m_col_current_card->set_radius(config->metrics.corner_radius);
    }
    if (m_col_preview_card) {
        m_col_preview_card->set_card_background_color(config->colors.surface_variant);
        m_col_preview_card->set_stroke(1, config->colors.outline_variant);
        m_col_preview_card->set_radius(config->metrics.corner_radius);
    }
    if (m_path_text) {
        m_path_text->set_text_color(config->colors.on_surface_variant);
    }
    if (m_h1_title) {
        m_h1_title->set_text_color(config->colors.primary);
    }
    if (m_hero_subtitle) {
        m_hero_subtitle->set_text_color(config->colors.outline);
    }

    populate_parent_column();
    populate_current_column();

    if (m_preview_view) {
        m_preview_view->update_preview(get_selected_item(), m_show_hidden);
    }
    if (m_status_bar) {
        m_status_bar->request_redraw();
    }
    if (m_window) {
        m_window->refresh_theme();
        m_window->schedule_redraw();
    }
}

void FilesWindow::build_ui() {
    auto config = Config::get();
    auto& fcfg = FilesConfig::get();

    // 1. Standard Toolbar
    m_toolbar = ToolbarBuilder::create()
        ->title("Files")
        ->titleAlignment(TitleAlignment::Center)
        ->onBack([this]() { on_left_action(); })
        ->onRefresh([this]() { refresh(); })
        ->onClose([this]() {
            if (m_window) m_window->request_close();
            else m_engine->quit();
        })
        ->build();
    m_toolbar->set_back_tooltip("Up to parent folder (h / Backspace / Left)");

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

    // 2. Dedicated Full-Path Text Strip just below the toolbar
    m_path_strip = std::make_shared<LinearLayout>(Orientation::Horizontal);
    m_path_strip->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    m_path_strip->set_padding(16, 4, 16, 4);
    m_path_strip->set_divider_spacing(8);

    auto path_icon = std::make_shared<ImageView>("folder");
    path_icon->set_target_size(14);
    path_icon->set_dim(0.35f);
    LayoutParams icon_lp;
    icon_lp.width = 14;
    icon_lp.height = 14;
    icon_lp.gravity = Gravity::CenterVertical;
    m_path_strip->add_view(path_icon, icon_lp);

    m_path_text = TextViewBuilder::create()
        ->text(m_current_path.string())
        ->textSize(11)
        ->textColor(config->colors.on_surface_variant)
        ->wrap(false)
        ->build();
    LayoutParams text_lp;
    text_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    text_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    text_lp.gravity = Gravity::CenterVertical;
    m_path_text->set_layout_params(text_lp);
    m_path_strip->add_view(m_path_text);

    // 3. Search Container (collapsible)
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
    columns_container->set_padding(14, 6, 14, 8);
    columns_container->set_divider_spacing(12);

    // --- LEFT COLUMN: Parent Directory (weight 1.0) ---
    m_col_parent_card = std::make_shared<ColumnCardView>();
    m_col_parent_card->set_card_background_color(config->colors.surface_variant);
    m_col_parent_card->set_radius(config->metrics.corner_radius);
    m_col_parent_card->set_stroke(1, config->colors.outline_variant);
    m_col_parent_card->set_elevation(0);
    m_col_parent_card->set_padding(8);
    LayoutParams col_parent_lp;
    col_parent_lp.width = 0;
    col_parent_lp.height = static_cast<int>(LayoutDimension::MatchParent);
    col_parent_lp.weight = 1.0f;
    m_col_parent_card->set_layout_params(col_parent_lp);

    m_parent_list = std::make_shared<ListView>();
    m_parent_list->set_item_height(38);
    m_parent_list->set_spacing(4);
    m_parent_list->set_item_corner_radius(6);
    m_parent_list->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    m_parent_list->set_on_item_click_listener([this](size_t idx, std::shared_ptr<View>) {
        if (idx < m_parent_items.size() && m_parent_items[idx].is_dir) {
            navigate_to(m_parent_items[idx].path);
        }
    });
    m_col_parent_card->add_view(m_parent_list);

    // Root Hero: Big H1 App Name centered in 1st pillar when at root (no parent)
    m_parent_root_hero = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams hero_lp;
    hero_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    hero_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    hero_lp.gravity = Gravity::Center;
    m_parent_root_hero->set_layout_params(hero_lp);
    m_parent_root_hero->set_divider_spacing(10);
    m_parent_root_hero->set_visibility(Visibility::Gone);

    auto app_icon = std::make_shared<ImageView>("system-file-manager");
    app_icon->set_target_size(56);
    app_icon->set_dim(0.15f);
    LayoutParams hero_icon_lp;
    hero_icon_lp.width = 56;
    hero_icon_lp.height = 56;
    hero_icon_lp.gravity = Gravity::CenterHorizontal;
    m_parent_root_hero->add_view(app_icon, hero_icon_lp);

    m_h1_title = TextViewBuilder::create()
        ->text("Files")
        ->bold(true)
        ->textSize(26)
        ->center(true)
        ->textColor(config->colors.primary)
        ->build();
    LayoutParams h1_lp;
    h1_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    h1_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    h1_lp.gravity = Gravity::CenterHorizontal;
    m_h1_title->set_layout_params(h1_lp);
    m_parent_root_hero->add_view(m_h1_title);

    m_hero_subtitle = TextViewBuilder::create()
        ->text("File Manager")
        ->textSize(11)
        ->center(true)
        ->textColor(config->colors.outline)
        ->build();
    LayoutParams sub_lp;
    sub_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    sub_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    sub_lp.gravity = Gravity::CenterHorizontal;
    m_hero_subtitle->set_layout_params(sub_lp);
    m_parent_root_hero->add_view(m_hero_subtitle);

    m_col_parent_card->add_view(m_parent_root_hero);

    // --- CENTER COLUMN: Active Directory (weight 1.6) ---
    m_col_current_card = std::make_shared<ColumnCardView>();
    m_col_current_card->set_card_background_color(config->colors.surface);
    m_col_current_card->set_radius(config->metrics.corner_radius);
    m_col_current_card->set_stroke(1, config->colors.outline_variant);
    m_col_current_card->set_elevation(0);
    m_col_current_card->set_padding(8);
    LayoutParams col_current_lp;
    col_current_lp.width = 0;
    col_current_lp.height = static_cast<int>(LayoutDimension::MatchParent);
    col_current_lp.weight = 1.6f;
    m_col_current_card->set_layout_params(col_current_lp);

    m_current_list = std::make_shared<ListView>();
    m_current_list->set_item_height(40);
    m_current_list->set_spacing(4);
    m_current_list->set_item_corner_radius(6);
    m_current_list->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    m_current_list->set_on_item_click_listener([this](size_t idx, std::shared_ptr<View>) {
        auto now = std::chrono::steady_clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_click_time).count();
        if (m_last_clicked_index == static_cast<int>(idx) && diff < 400) {
            open_selected();
        } else {
            m_last_click_time = now;
            m_last_clicked_index = static_cast<int>(idx);
            set_selected_index(static_cast<int>(idx));
        }
    });
    m_col_current_card->add_view(m_current_list);

    // --- RIGHT COLUMN: Preview & Child View (weight 2.0) [NO SELECTION] ---
    m_col_preview_card = std::make_shared<ColumnCardView>();
    m_col_preview_card->set_card_background_color(config->colors.surface_variant);
    m_col_preview_card->set_radius(config->metrics.corner_radius);
    m_col_preview_card->set_stroke(1, config->colors.outline_variant);
    m_col_preview_card->set_elevation(0);
    m_col_preview_card->set_padding(8);
    LayoutParams col_preview_lp;
    col_preview_lp.width = 0;
    col_preview_lp.height = static_cast<int>(LayoutDimension::MatchParent);
    col_preview_lp.weight = 2.0f;
    m_col_preview_card->set_layout_params(col_preview_lp);

    m_preview_view = std::make_shared<FilePreviewView>();
    m_preview_view->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    m_preview_view->set_on_item_click_listener([this](const FileItem& item) {
        if (item.is_dir) {
            navigate_to(item.path.parent_path(), true, item.name);
        } else {
            navigate_to(item.path.parent_path(), true, item.name);
            enter_fullscreen_preview();
        }
    });
    m_preview_view->set_on_preview_click_listener([this](const FileItem& item) {
        if (!item.is_dir) {
            enter_fullscreen_preview();
        }
    });
    m_col_preview_card->add_view(m_preview_view);

    // Add columns
    columns_container->add_view(m_col_parent_card);
    columns_container->add_view(m_col_current_card);
    columns_container->add_view(m_col_preview_card);

    // 4. Status Bar
    m_status_bar = std::make_shared<StatusBarView>();

    // 5. Root Layout
    m_root_container = std::make_shared<LinearLayout>(Orientation::Vertical);
    m_root_container->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));

    m_root_container->add_view(m_toolbar);
    m_root_container->add_view(m_path_strip);
    m_root_container->add_view(m_search_container);
    m_root_container->add_view(columns_container);
    m_root_container->add_view(m_status_bar);

    // 6. Toplevel Window Creation
    m_window = WindowBuilder::create()
        ->title("Files - miqufiles")
        ->appId("miqufiles")
        ->role(WindowRole::Toplevel)
        ->preferredSize(fcfg.window_width, fcfg.window_height)
        ->closeOnEscape(false)
        ->contentView(m_root_container)
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
                        on_left_action();
                        return;
                    }
                    break;

                case XKB_KEY_Right:
                case XKB_KEY_l:
                    if (!m_search_visible) {
                        on_right_action();
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
                        if (m_fullscreen_preview) {
                            m_preview_view->scroll_preview(-2.0);
                        } else {
                            set_selected_index(m_selected_index - 1);
                        }
                        return;
                    }
                    break;

                case XKB_KEY_Down:
                case XKB_KEY_j:
                    if (!m_search_visible || ev.keysym == XKB_KEY_Down) {
                        if (m_fullscreen_preview) {
                            m_preview_view->scroll_preview(2.0);
                        } else {
                            set_selected_index(m_selected_index + 1);
                        }
                        return;
                    }
                    break;

                case XKB_KEY_Home:
                case XKB_KEY_g:
                    if (!m_search_visible) {
                        if (!m_fullscreen_preview) {
                            set_selected_index(0);
                        }
                        return;
                    }
                    break;

                case XKB_KEY_End:
                case XKB_KEY_G:
                    if (!m_search_visible) {
                        if (!m_fullscreen_preview) {
                            set_selected_index(static_cast<int>(m_filtered_items.size()) - 1);
                        }
                        return;
                    }
                    break;

                case XKB_KEY_Page_Up:
                    if (m_fullscreen_preview) {
                        m_preview_view->scroll_preview(-10.0);
                    } else {
                        set_selected_index(m_selected_index - 10);
                    }
                    return;

                case XKB_KEY_Page_Down:
                    if (m_fullscreen_preview) {
                        m_preview_view->scroll_preview(10.0);
                    } else {
                        set_selected_index(m_selected_index + 10);
                    }
                    return;

                case XKB_KEY_period:
                    if (!m_search_visible && !m_fullscreen_preview) {
                        toggle_hidden();
                        return;
                    }
                    break;

                case XKB_KEY_slash:
                    if (!m_search_visible && !m_fullscreen_preview) {
                        toggle_search();
                        return;
                    }
                    break;

                case XKB_KEY_asciitilde:
                    if (!m_search_visible) {
                        if (m_fullscreen_preview) {
                            exit_fullscreen_preview();
                        }
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
                        if (m_fullscreen_preview) {
                            exit_fullscreen_preview();
                        } else if (m_window) {
                            m_window->request_close();
                        }
                        return;
                    }
                    break;

                case XKB_KEY_Escape:
                    if (m_search_visible) {
                        toggle_search();
                    } else if (m_fullscreen_preview) {
                        exit_fullscreen_preview();
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

void FilesWindow::navigate_to(const fs::path& p, bool record_history, const std::string& target_select_name) {
    if (m_fullscreen_preview) {
        exit_fullscreen_preview();
    }

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

    // Update path strip display & back button (toolbar title remains stable "Files")
    if (m_path_text) {
        m_path_text->set_text(m_current_path.string());
    }
    m_toolbar->set_back_visible(m_current_path != m_current_path.root_path());

    // Update parent column
    populate_parent_column();

    // Scan active directory
    m_current_items = FileItem::scan_directory(m_current_path, m_show_hidden);
    apply_filter(target_select_name);
}

void FilesWindow::navigate_up() {
    if (m_fullscreen_preview) {
        exit_fullscreen_preview();
        return;
    }
    if (m_current_path != m_current_path.root_path()) {
        std::string prev_name = m_current_path.filename().string();
        navigate_to(m_current_path.parent_path(), true, prev_name);
    }
}

void FilesWindow::on_left_action() {
    if (m_fullscreen_preview) {
        exit_fullscreen_preview();
        return;
    }
    navigate_up();
}

void FilesWindow::on_right_action() {
    if (m_fullscreen_preview) {
        return;
    }
    const FileItem* item = get_selected_item();
    if (!item) return;

    if (item->is_dir) {
        navigate_to(item->path);
    } else {
        enter_fullscreen_preview();
    }
}

void FilesWindow::enter_fullscreen_preview() {
    const FileItem* item = get_selected_item();
    if (!item || item->is_dir || m_fullscreen_preview) return;

    m_fullscreen_preview = true;
    if (m_col_parent_card) m_col_parent_card->set_visibility(Visibility::Gone);
    if (m_col_current_card) m_col_current_card->set_visibility(Visibility::Gone);
    if (m_col_preview_card) m_col_preview_card->set_visibility(Visibility::Visible);

    if (m_path_text) {
        m_path_text->set_text(item->path.string());
    }
    if (m_window) {
        m_window->schedule_redraw();
    }
}

void FilesWindow::exit_fullscreen_preview() {
    if (!m_fullscreen_preview) return;

    m_fullscreen_preview = false;
    if (m_col_parent_card) m_col_parent_card->set_visibility(Visibility::Visible);
    if (m_col_current_card) m_col_current_card->set_visibility(Visibility::Visible);
    if (m_col_preview_card) m_col_preview_card->set_visibility(Visibility::Visible);

    if (m_path_text) {
        m_path_text->set_text(m_current_path.string());
    }
    if (m_window) {
        m_window->schedule_redraw();
    }
}

void FilesWindow::open_external(const fs::path& p) {
    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        execlp("xdg-open", "xdg-open", p.c_str(), nullptr);
        _exit(1);
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
        m_parent_list->set_visibility(Visibility::Gone);
        if (m_parent_root_hero) m_parent_root_hero->set_visibility(Visibility::Visible);
        return;
    }

    m_parent_list->set_visibility(Visibility::Visible);
    if (m_parent_root_hero) m_parent_root_hero->set_visibility(Visibility::Gone);

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

void FilesWindow::apply_filter(const std::string& target_select_name) {
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

    populate_current_column(target_select_name);
}

void FilesWindow::populate_current_column(const std::string& target_select_name) {
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

    int select_idx = 0;
    if (!target_select_name.empty()) {
        for (size_t i = 0; i < m_filtered_items.size(); ++i) {
            if (m_filtered_items[i].name == target_select_name) {
                select_idx = static_cast<int>(i);
                break;
            }
        }
    }

    set_selected_index(m_filtered_items.empty() ? -1 : select_idx);
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
    if (m_window) {
        m_window->schedule_redraw();
    }
}

const FileItem* FilesWindow::get_selected_item() const {
    if (m_selected_index >= 0 && m_selected_index < static_cast<int>(m_filtered_items.size())) {
        return &m_filtered_items[m_selected_index];
    }
    return nullptr;
}

void FilesWindow::update_preview_and_status() {
    const FileItem* item = get_selected_item();

    m_preview_view->update_preview(item, m_show_hidden);
    m_status_bar->update_stats(m_filtered_items, item, m_current_path);
}

void FilesWindow::open_selected() {
    const FileItem* item = get_selected_item();
    if (!item) return;

    if (item->is_dir) {
        navigate_to(item->path);
    } else {
        open_external(item->path);
    }
}

} // namespace miqu
