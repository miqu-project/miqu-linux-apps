#include "ui/settings_window.hpp"
#include "config/settings_config.hpp"
#include <miqutoolkit/core/config.hpp>
#include <miqutoolkit/view/list_item_view.hpp>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <algorithm>
#include <iostream>

namespace miqu {

std::shared_ptr<SettingsWindow> SettingsWindow::create(std::shared_ptr<AppEngine> engine) {
    auto win = std::shared_ptr<SettingsWindow>(new SettingsWindow(std::move(engine)));
    if (!win->init()) {
        return nullptr;
    }
    return win;
}

SettingsWindow::SettingsWindow(std::shared_ptr<AppEngine> engine)
    : m_engine(std::move(engine)) {}

bool SettingsWindow::init() {
    m_root_node = SettingItem::create_default_tree();
    m_current_node = m_root_node;

    build_ui();
    sync_theme();
    navigate_to(m_root_node, false);

    return m_window != nullptr;
}

void SettingsWindow::show() {
    if (m_window) {
        m_window->show();
    }
}

void SettingsWindow::build_ui() {
    auto config = Config::get();
    auto& scfg = SettingsConfig::get();

    // 1. Standard Toolbar
    m_toolbar = ToolbarBuilder::create()
        ->title("Settings")
        ->titleAlignment(TitleAlignment::Center)
        ->onBack([this]() { on_left_action(); })
        ->onClose([this]() {
            if (m_window) m_window->request_close();
            else m_engine->quit();
        })
        ->build();
    m_toolbar->set_back_tooltip("Back / Parent Section (h / Backspace / Left)");

    // Search action in toolbar
    m_btn_search = m_toolbar->add_action(icons::SEARCH, [this]() {
        toggle_search();
    });

    // 2. Full-Path Breadcrumb Strip below toolbar
    m_path_strip = std::make_shared<LinearLayout>(Orientation::Horizontal);
    m_path_strip->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    m_path_strip->set_padding(16, 4, 16, 4);
    m_path_strip->set_divider_spacing(8);

    auto path_icon = std::make_shared<ImageView>("preferences-system");
    path_icon->set_target_size(14);
    path_icon->set_dim(0.35f);
    LayoutParams icon_lp;
    icon_lp.width = 14;
    icon_lp.height = 14;
    icon_lp.gravity = Gravity::CenterVertical;
    m_path_strip->add_view(path_icon, icon_lp);

    m_path_text = TextViewBuilder::create()
        ->text("Settings")
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
    m_search_view->set_hint("Filter settings (e.g. wifi, display, sound)...");
    m_search_view->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    m_search_view->set_on_query_text_listener([this](const std::string& query) {
        m_filter_query = query;
        apply_filter();
    });
    m_search_container->add_view(m_search_view);

    // 4. 3-PILLAR MILLER COLUMNS CONTAINER
    m_columns_container = std::make_shared<LinearLayout>(Orientation::Horizontal);
    m_columns_container->set_padding(12);
    m_columns_container->set_divider_spacing(12);
    LayoutParams cols_lp;
    cols_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    cols_lp.height = 0;
    cols_lp.weight = 1.0f;
    m_columns_container->set_layout_params(cols_lp);

    // LEFT COLUMN: Parent / Hero Card (weight 1.2)
    m_col_parent_card = std::make_shared<ColumnCardView>();
    m_col_parent_card->set_card_background_color(config->colors.surface_variant);
    m_col_parent_card->set_radius(config->metrics.corner_radius);
    m_col_parent_card->set_stroke(config->metrics.border_width, config->colors.outline_variant);
    m_col_parent_card->set_elevation(0);
    m_col_parent_card->set_padding(10);
    LayoutParams col_parent_lp;
    col_parent_lp.width = 0;
    col_parent_lp.height = static_cast<int>(LayoutDimension::MatchParent);
    col_parent_lp.weight = 1.2f;
    m_col_parent_card->set_layout_params(col_parent_lp);

    m_parent_list = std::make_shared<ListView>();
    m_parent_list->set_item_height(50);
    m_parent_list->set_spacing(6);
    m_parent_list->set_item_corner_radius(std::max(4, config->metrics.corner_radius - 4));
    m_parent_list->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    m_parent_list->set_on_item_click_listener([this](size_t idx, std::shared_ptr<View>) {
        auto now = std::chrono::steady_clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_parent_click_time).count();
        if (m_last_parent_clicked_index == static_cast<int>(idx) && diff < 400) {
            if (idx < m_parent_items.size()) {
                navigate_to(m_parent_items[idx]);
            }
        } else {
            m_last_parent_click_time = now;
            m_last_parent_clicked_index = static_cast<int>(idx);
        }
    });
    m_col_parent_card->add_view(m_parent_list);

    // Root Hero: Big App Logo & Title in 1st pillar when at root
    m_parent_root_hero = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams hero_lp;
    hero_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    hero_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    hero_lp.gravity = Gravity::Center;
    m_parent_root_hero->set_layout_params(hero_lp);
    m_parent_root_hero->set_divider_spacing(12);
    m_parent_root_hero->set_visibility(Visibility::Gone);

    auto app_icon = std::make_shared<ImageView>("preferences-system");
    app_icon->set_target_size(64);
    app_icon->set_dim(0.15f);
    LayoutParams hero_icon_lp;
    hero_icon_lp.width = 64;
    hero_icon_lp.height = 64;
    hero_icon_lp.gravity = Gravity::CenterHorizontal;
    m_parent_root_hero->add_view(app_icon, hero_icon_lp);

    auto h1_title = TextViewBuilder::create()
        ->text("Settings")
        ->bold(true)
        ->textSize(config->metrics.h1_size > 0 ? config->metrics.h1_size + 8 : 28)
        ->center(true)
        ->textColor(config->colors.primary)
        ->build();
    LayoutParams h1_lp;
    h1_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    h1_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    h1_lp.gravity = Gravity::CenterHorizontal;
    h1_title->set_layout_params(h1_lp);
    m_parent_root_hero->add_view(h1_title);

    auto hero_subtitle = TextViewBuilder::create()
        ->text("System Configuration")
        ->textSize(config->metrics.caption_size)
        ->center(true)
        ->textColor(config->colors.outline)
        ->build();
    LayoutParams sub_lp;
    sub_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    sub_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    sub_lp.gravity = Gravity::CenterHorizontal;
    hero_subtitle->set_layout_params(sub_lp);
    m_parent_root_hero->add_view(hero_subtitle);

    m_col_parent_card->add_view(m_parent_root_hero);
    m_columns_container->add_view(m_col_parent_card);

    // CENTER COLUMN: Active Section List (weight 1.6)
    m_col_current_card = std::make_shared<ColumnCardView>();
    m_col_current_card->set_card_background_color(config->colors.surface);
    m_col_current_card->set_radius(config->metrics.corner_radius);
    m_col_current_card->set_stroke(config->metrics.border_width, config->colors.outline_variant);
    m_col_current_card->set_elevation(0);
    m_col_current_card->set_padding(10);
    LayoutParams col_current_lp;
    col_current_lp.width = 0;
    col_current_lp.height = static_cast<int>(LayoutDimension::MatchParent);
    col_current_lp.weight = 1.6f;
    m_col_current_card->set_layout_params(col_current_lp);

    m_current_list = std::make_shared<ListView>();
    m_current_list->set_item_height(54);
    m_current_list->set_spacing(6);
    m_current_list->set_item_corner_radius(std::max(4, config->metrics.corner_radius - 4));
    m_current_list->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));

    m_current_list->set_on_item_click_listener([this](size_t idx, std::shared_ptr<View>) {
        auto now = std::chrono::steady_clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_click_time).count();
        if (m_last_clicked_index == static_cast<int>(idx) && diff < 400) {
            if (idx < m_filtered_items.size() && m_detail_view &&
                m_detail_view->get_current_id() == m_filtered_items[idx]->id) {
                open_selected();
            } else {
                set_selected_index(static_cast<int>(idx));
            }
        } else {
            m_last_click_time = now;
            m_last_clicked_index = static_cast<int>(idx);
            select_item_without_preview(static_cast<int>(idx));
        }
    });

    m_col_current_card->add_view(m_current_list);
    m_columns_container->add_view(m_col_current_card);

    // RIGHT COLUMN: Detail / Preview Card (weight 2.0)
    m_col_preview_card = std::make_shared<ColumnCardView>();
    m_col_preview_card->set_card_background_color(config->colors.surface_variant);
    m_col_preview_card->set_radius(config->metrics.corner_radius);
    m_col_preview_card->set_stroke(config->metrics.border_width, config->colors.outline_variant);
    m_col_preview_card->set_elevation(0);
    m_col_preview_card->set_padding(12);
    LayoutParams col_preview_lp;
    col_preview_lp.width = 0;
    col_preview_lp.height = static_cast<int>(LayoutDimension::MatchParent);
    col_preview_lp.weight = 2.0f;
    m_col_preview_card->set_layout_params(col_preview_lp);

    m_detail_view = std::make_shared<SettingDetailView>();
    m_detail_view->set_on_navigate_into([this](const std::shared_ptr<SettingItem>& child) {
        if (child->type == SettingType::Category) {
            navigate_to(child);
        } else {
            enter_fullscreen_detail();
        }
    });
    m_detail_view->set_on_request_fullscreen([this]() {
        enter_fullscreen_detail();
    });
    m_detail_view->set_on_setting_changed([this](const std::shared_ptr<SettingItem>&) {
        populate_current_column();
    });

    m_col_preview_card->add_view(m_detail_view);
    m_columns_container->add_view(m_col_preview_card);

    // 5. Root Container Assembly
    m_root_container = std::make_shared<LinearLayout>(Orientation::Vertical);
    m_root_container->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    m_root_container->set_background_color(config->colors.background);

    m_root_container->add_view(m_toolbar);
    m_root_container->add_view(m_path_strip);
    m_root_container->add_view(m_search_container);
    m_root_container->add_view(m_columns_container);

    // 6. Window Creation via WindowBuilder
    m_window = WindowBuilder::create()
        ->title("Settings - miqusettings")
        ->appId("miqusettings")
        ->role(WindowRole::Toplevel)
        ->preferredSize(scfg.window_width, scfg.window_height)
        ->closeOnEscape(false)
        ->contentView(m_root_container)
        ->onKey([this](const KeyPressEvent& ev) {
            if (!ev.pressed) return;

            // Handle search view input keys when visible
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

            switch (ev.keysym) {
                case XKB_KEY_Escape:
                    if (m_fullscreen_detail) {
                        exit_fullscreen_detail();
                    } else if (m_search_visible) {
                        toggle_search();
                    } else if (!m_filter_query.empty()) {
                        m_filter_query.clear();
                        if (m_search_view) m_search_view->set_query("");
                        apply_filter();
                    } else {
                        if (m_window) m_window->request_close();
                    }
                    return;

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
                        if (!m_fullscreen_detail) {
                            set_selected_index(m_selected_index - 1);
                        }
                        return;
                    }
                    break;

                case XKB_KEY_Down:
                case XKB_KEY_j:
                    if (!m_search_visible || ev.keysym == XKB_KEY_Down) {
                        if (!m_fullscreen_detail) {
                            set_selected_index(m_selected_index + 1);
                        }
                        return;
                    }
                    break;

                case XKB_KEY_slash:
                    if (!m_search_visible && !m_fullscreen_detail) {
                        toggle_search();
                        return;
                    }
                    break;

                default:
                    break;
            }
        })
        ->build();
}

void SettingsWindow::sync_theme() {
    auto config = Config::get();
    if (!config) return;

    if (m_col_parent_card) {
        m_col_parent_card->set_card_background_color(config->colors.surface_variant);
        m_col_parent_card->set_stroke(config->metrics.border_width, config->colors.outline_variant);
        m_col_parent_card->set_radius(config->metrics.corner_radius);
    }
    if (m_col_current_card) {
        m_col_current_card->set_card_background_color(config->colors.surface);
        m_col_current_card->set_stroke(config->metrics.border_width, config->colors.outline_variant);
        m_col_current_card->set_radius(config->metrics.corner_radius);
    }
    if (m_col_preview_card) {
        m_col_preview_card->set_card_background_color(config->colors.surface_variant);
        m_col_preview_card->set_stroke(config->metrics.border_width, config->colors.outline_variant);
        m_col_preview_card->set_radius(config->metrics.corner_radius);
    }
    if (m_parent_list) {
        m_parent_list->set_item_corner_radius(std::max(4, config->metrics.corner_radius - 4));
    }
    if (m_current_list) {
        m_current_list->set_item_corner_radius(std::max(4, config->metrics.corner_radius - 4));
    }
    if (m_path_text) {
        m_path_text->set_text_color(config->colors.on_surface_variant);
    }

    populate_parent_column();
    populate_current_column();
    update_preview_and_status();

    if (m_window) {
        m_window->refresh_theme();
        m_window->schedule_redraw();
    }
}

static std::shared_ptr<SettingItem> find_item_recursive(const std::shared_ptr<SettingItem>& node, const std::string& id) {
    if (!node) return nullptr;
    if (node->id == id) return node;
    for (const auto& child : node->children) {
        auto res = find_item_recursive(child, id);
        if (res) return res;
    }
    return nullptr;
}

void SettingsWindow::navigate_to_id(const std::string& target_id) {
    auto item = find_item_recursive(m_root_node, target_id);
    if (!item) return;

    auto parent = item->parent.lock();
    if (parent) {
        navigate_to(parent, false);
        for (size_t i = 0; i < m_filtered_items.size(); ++i) {
            if (m_filtered_items[i]->id == target_id) {
                set_selected_index(static_cast<int>(i));
                break;
            }
        }
    } else {
        navigate_to(item, false);
    }

    if (target_id == "bluetooth") {
        enter_fullscreen_detail();
    }
}

void SettingsWindow::navigate_to(const std::shared_ptr<SettingItem>& node, bool record_history) {
    if (!node) return;

    if (m_fullscreen_detail) {
        exit_fullscreen_detail();
    }

    if (record_history && node != m_current_node) {
        m_history_back.push_back(m_current_node);
        m_history_forward.clear();
    }

    m_current_node = node;

    // Update path breadcrumb
    std::string breadcrumb = m_current_node->title;
    auto p = m_current_node->parent.lock();
    while (p) {
        breadcrumb = p->title + "  ›  " + breadcrumb;
        p = p->parent.lock();
    }
    if (m_path_text) {
        m_path_text->set_text(breadcrumb);
    }

    m_toolbar->set_back_visible(m_current_node != m_root_node);

    populate_parent_column();
    apply_filter();
}

void SettingsWindow::navigate_up() {
    if (m_fullscreen_detail) {
        exit_fullscreen_detail();
        return;
    }
    auto p = m_current_node->parent.lock();
    if (p) {
        navigate_to(p, true);
    }
}

void SettingsWindow::navigate_back() {
    if (!m_history_back.empty()) {
        auto prev = m_history_back.back();
        m_history_back.pop_back();
        m_history_forward.push_back(m_current_node);
        navigate_to(prev, false);
    }
}

void SettingsWindow::navigate_forward() {
    if (!m_history_forward.empty()) {
        auto next = m_history_forward.back();
        m_history_forward.pop_back();
        m_history_back.push_back(m_current_node);
        navigate_to(next, false);
    }
}

void SettingsWindow::on_left_action() {
    if (m_fullscreen_detail) {
        exit_fullscreen_detail();
    } else {
        navigate_up();
    }
}

void SettingsWindow::on_right_action() {
    open_selected();
}

void SettingsWindow::open_selected() {
    auto item = get_selected_item();
    if (!item) return;

    if (item->id == "bluetooth") {
        enter_fullscreen_detail();
        return;
    }

    if (item->type == SettingType::Category) {
        navigate_to(item);
    } else {
        enter_fullscreen_detail();
    }
}

void SettingsWindow::enter_fullscreen_detail() {
    auto item = get_selected_item();
    if (!item) return;

    m_fullscreen_detail = true;
    m_col_parent_card->set_visibility(Visibility::Gone);
    m_col_current_card->set_visibility(Visibility::Gone);

    LayoutParams full_lp;
    full_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    full_lp.height = static_cast<int>(LayoutDimension::MatchParent);
    full_lp.weight = 1.0f;
    m_col_preview_card->set_layout_params(full_lp);

    m_toolbar->set_back_visible(true);
    m_detail_view->update_item(item, true);

    if (m_window) {
        m_window->schedule_redraw();
    }
}

void SettingsWindow::exit_fullscreen_detail() {
    m_fullscreen_detail = false;

    m_col_parent_card->set_visibility(Visibility::Visible);
    m_col_current_card->set_visibility(Visibility::Visible);

    LayoutParams col_preview_lp;
    col_preview_lp.width = 0;
    col_preview_lp.height = static_cast<int>(LayoutDimension::MatchParent);
    col_preview_lp.weight = 2.0f;
    m_col_preview_card->set_layout_params(col_preview_lp);

    m_toolbar->set_back_visible(m_current_node != m_root_node);
    m_detail_view->update_item(get_selected_item(), false);

    if (m_window) {
        m_window->schedule_redraw();
    }
}

void SettingsWindow::toggle_search() {
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

void SettingsWindow::refresh() {
    populate_current_column();
    update_preview_and_status();
}

void SettingsWindow::populate_parent_column() {
    auto parent_node = m_current_node->parent.lock();
    if (!parent_node) {
        m_parent_list->set_visibility(Visibility::Gone);
        m_parent_root_hero->set_visibility(Visibility::Visible);
        m_parent_items.clear();
        m_parent_selected_index = -1;
        return;
    }

    m_parent_root_hero->set_visibility(Visibility::Gone);
    m_parent_list->set_visibility(Visibility::Visible);
    m_parent_list->clear_items();

    m_parent_items = parent_node->children;
    int found_selected = -1;

    for (size_t i = 0; i < m_parent_items.size(); ++i) {
        const auto& item = m_parent_items[i];
        auto view = ListItemViewBuilder::create()
            ->title(item->title)
            ->iconSource(item->icon_name.empty() ? "preferences-system" : item->icon_name)
            ->iconSize(22)
            ->build();
        m_parent_list->add_item(view);

        if (item->id == m_current_node->id) {
            found_selected = static_cast<int>(i);
        }
    }

    m_parent_selected_index = found_selected;
    m_parent_list->set_selected_index(found_selected);
}

void SettingsWindow::apply_filter() {
    m_filtered_items.clear();

    if (m_filter_query.empty()) {
        m_filtered_items = m_current_node->children;
    } else {
        std::string query_lower = m_filter_query;
        std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(), ::tolower);

        for (const auto& item : m_current_node->children) {
            std::string title_lower = item->title;
            std::transform(title_lower.begin(), title_lower.end(), title_lower.begin(), ::tolower);
            std::string sub_lower = item->subtitle;
            std::transform(sub_lower.begin(), sub_lower.end(), sub_lower.begin(), ::tolower);

            if (title_lower.find(query_lower) != std::string::npos ||
                sub_lower.find(query_lower) != std::string::npos) {
                m_filtered_items.push_back(item);
            }
        }
    }

    populate_current_column();
}

void SettingsWindow::populate_current_column() {
    m_current_list->clear_items();

    for (const auto& item : m_filtered_items) {
        auto view = ListItemViewBuilder::create()
            ->title(item->title)
            ->subtitle(item->subtitle)
            ->trailingText(item->get_status_text())
            ->iconSource(item->icon_name.empty() ? "preferences-system" : item->icon_name)
            ->iconSize(24)
            ->build();
        m_current_list->add_item(view);
    }

    int select_idx = m_filtered_items.empty() ? -1 : 0;
    if (m_selected_index >= 0 && m_selected_index < static_cast<int>(m_filtered_items.size())) {
        select_idx = m_selected_index;
    }

    set_selected_index(select_idx);
}

void SettingsWindow::set_selected_index(int index) {
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

void SettingsWindow::select_item_without_preview(int index) {
    if (m_filtered_items.empty()) {
        m_selected_index = -1;
        m_current_list->set_selected_index(-1);
    } else {
        m_selected_index = std::clamp(index, 0, static_cast<int>(m_filtered_items.size()) - 1);
        m_current_list->set_selected_index(m_selected_index);
    }

    if (m_window) {
        m_window->schedule_redraw();
    }
}

std::shared_ptr<SettingItem> SettingsWindow::get_selected_item() const {
    if (m_selected_index >= 0 && m_selected_index < static_cast<int>(m_filtered_items.size())) {
        return m_filtered_items[m_selected_index];
    }
    return nullptr;
}

void SettingsWindow::update_preview_and_status() {
    auto item = get_selected_item();
    m_detail_view->update_item(item, m_fullscreen_detail);
}

} // namespace miqu
