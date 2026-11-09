#include "miller_view.hpp"
#include "preview_common.hpp"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace miqu {

MillerView::MillerView() : LinearLayout(Orientation::Horizontal) {}

MillerView::~MillerView() {
    cancel_pending_sibling_click();
}

void MillerView::cancel_pending_sibling_click() {
    if (m_single_click_timer_id != 0) {
        if (auto* eng = AppEngine::instance()) {
            eng->cancel_delayed(m_single_click_timer_id);
        }
        m_single_click_timer_id = 0;
    }
}

void MillerView::execute_sibling_swap(const std::shared_ptr<SettingItem>& item, size_t index) {
    cancel_pending_sibling_click();
    m_last_sibling_clicked_index = -1;
    m_last_sibling_click_time_ms = 0;
    if (!item) return;

    if (item->is_category) {
        if (!m_path.empty()) {
            m_path.back() = item;
        } else {
            m_path.push_back(item);
        }
        if (!m_history_indices.empty()) {
            m_history_indices.back() = static_cast<int>(index);
        }
        m_current_index = 0;
        m_preview_focused = false;
        if (m_current_scroll) {
            m_current_scroll->set_scroll_y(0);
        }
        rebuild_columns();
        notify_state_changed();
    } else {
        execute_shift_up(index);
    }
}

void MillerView::execute_shift_up(size_t index) {
    cancel_pending_sibling_click();
    m_last_sibling_clicked_index = -1;
    m_last_sibling_click_time_ms = 0;

    if (!m_path.empty()) {
        m_path.pop_back();
        if (!m_history_indices.empty()) {
            m_history_indices.pop_back();
        }
    }
    m_current_index = static_cast<int>(index);
    m_preview_focused = false;
    rebuild_columns();
    notify_state_changed();
}

void MillerView::init() {
    m_root_tree = SettingsModel::build_default_tree();

    set_orientation(Orientation::Horizontal);
    set_padding(12, 4, 12, 12);
    set_divider_spacing(10);
    set_background_color(Color::transparent());

    // 1. Column 1: Parent Pillar
    m_parent_layout = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(4)
        ->padding(10, 10)
        ->backgroundColor(Color::transparent())
        ->build();

    LayoutParams parent_lp;
    parent_lp.width = 280;
    parent_lp.height = static_cast<int>(LayoutDimension::MatchParent);
    add_view(m_parent_layout, parent_lp);

    // 2. Column 2: Current (Center) Pillar
    m_current_layout = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(4)
        ->padding(10, 10)
        ->backgroundColor(Color::transparent())
        ->build();

    m_current_scroll = ScrollViewBuilder::create()
        ->contentView(m_current_layout)
        ->scrollbar(true)
        ->build();

    LayoutParams current_lp;
    current_lp.width = 300;
    current_lp.height = static_cast<int>(LayoutDimension::MatchParent);
    add_view(m_current_scroll, current_lp);

    // 3. Column 3: Preview (Right) Pillar Container
    m_preview_pillar = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(4)
        ->backgroundColor(Color::transparent())
        ->build();

    auto config = Config::get();

    // 3a. Header Ribbon for Interactive Pillar (sidebar collapse/expand icon + section title)
    m_preview_ribbon = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->gravity(Gravity::CenterVertical)
        ->padding(14, 2, 14, 2)
        ->spacing(8)
        ->backgroundColor(Color::transparent())
        ->build();

    m_preview_toggle_icon = TextViewBuilder::create()
        ->text("|‹")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.primary)
        ->build();

    auto icon_box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(8, 2)
        ->gravity(Gravity::Center)
        ->addView(m_preview_toggle_icon)
        ->build();

    m_preview_toggle_btn = CardViewBuilder::create()
        ->style(CardStyle::Filled)
        ->backgroundColor(config->colors.surface_variant.with_alpha(0.35f))
        ->elevation(0)
        ->addView(icon_box)
        ->build();
    m_preview_toggle_btn->set_on_click_listener([this]() {
        set_preview_expanded(!m_preview_expanded);
    });
    // Non-focusable: mouse/touch only so keyboard navigation is never interrupted
    m_preview_toggle_btn->set_focusable(false);

    m_preview_title = TextViewBuilder::create()
        ->text("")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface_variant)
        ->ellipsize(true)
        ->build();
    LayoutParams title_lp;
    title_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    title_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    title_lp.weight = 1.0f;
    title_lp.gravity = Gravity::CenterVertical;
    m_preview_ribbon->add_view(m_preview_title, title_lp);

    LayoutParams toggle_lp;
    toggle_lp.width = static_cast<int>(LayoutDimension::WrapContent);
    toggle_lp.height = 26;
    toggle_lp.gravity = Gravity::CenterVertical;
    m_preview_ribbon->add_view(m_preview_toggle_btn, toggle_lp);

    LayoutParams ribbon_lp;
    ribbon_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    ribbon_lp.height = 30;
    m_preview_pillar->add_view(m_preview_ribbon, ribbon_lp);

    // 3b. Scrollable Cards Container
    m_preview_layout = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(10)
        ->padding(14, 4, 14, 14)
        ->backgroundColor(Color::transparent())
        ->build();

    m_preview_scroll = ScrollViewBuilder::create()
        ->contentView(m_preview_layout)
        ->scrollbar(true)
        ->build();

    LayoutParams scroll_lp;
    scroll_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    scroll_lp.height = static_cast<int>(LayoutDimension::MatchParent);
    scroll_lp.weight = 1.0f;
    m_preview_pillar->add_view(m_preview_scroll, scroll_lp);

    LayoutParams pillar_lp;
    pillar_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    pillar_lp.height = static_cast<int>(LayoutDimension::MatchParent);
    pillar_lp.weight = 1.0f;
    add_view(m_preview_pillar, pillar_lp);

    sync_theme();
    rebuild_columns();
}

void MillerView::sync_theme() {
    auto config = Config::get();
    if (config && m_preview_toggle_btn) {
        m_preview_toggle_btn->set_card_background_color(config->colors.surface_variant.with_alpha(0.35f));
    }
    update_preview_ribbon();
    update_pillar_styles();
    request_redraw();
}

void MillerView::update_pillar_styles() {
    auto config = Config::get();
    if (!config) return;

    int radius = get_card_radius(config);
    Color active_border = config->colors.primary.with_alpha(0.55f);

    // 1. Column 1: Parent Pillar (Always transparent, zero border)
    if (m_parent_layout) {
        m_parent_layout->set_corner_radius(radius);
        m_parent_layout->set_stroke(0, Color::transparent());
        m_parent_layout->set_background_color(Color::transparent());
    }

    // 2. Column 2: Center Pillar (Subtle border ONLY when actively focused, transparent background)
    if (m_current_scroll) {
        m_current_scroll->set_corner_radius(radius);
        m_current_scroll->set_stroke(m_preview_focused ? 0 : 1, m_preview_focused ? Color::transparent() : active_border);
        m_current_scroll->set_background_color(Color::transparent());
    }

    // 3. Column 3: Right (Preview) Pillar Container (Subtle border ONLY when actively focused in 3-pillar mode)
    if (m_preview_pillar) {
        m_preview_pillar->set_corner_radius(radius);
        bool show_border = m_preview_focused && !m_preview_expanded;
        m_preview_pillar->set_stroke(show_border ? 1 : 0, show_border ? active_border : Color::transparent());
        m_preview_pillar->set_background_color(Color::transparent());
    }
}

void MillerView::update_preview_ribbon() {
    if (!m_preview_ribbon) return;

    auto config = Config::get();
    auto item = get_current_selected_item();

    // Only show the ribbon for leaf items (which have dedicated interactive pages)
    if (!item || item->is_category) {
        m_preview_ribbon->set_visibility(Visibility::Gone);
        return;
    }

    m_preview_ribbon->set_visibility(Visibility::Visible);

    if (m_preview_title) {
        std::string title_str = item ? item->title : "";
        m_preview_title->set_text(title_str);
        if (config) {
            m_preview_title->set_text_color(config->colors.on_surface_variant);
        }
    }

    if (m_preview_toggle_icon) {
        m_preview_toggle_icon->set_text(m_preview_expanded ? "|›" : "|‹");
        if (config) {
            m_preview_toggle_icon->set_text_color(config->colors.primary);
        }
    }
}

bool MillerView::on_key(const KeyPressEvent& event) {
    (void)event;
    return false;
}

void MillerView::set_preview_focused(bool focused) {
    if (m_preview_focused == focused) return;
    m_preview_focused = focused;
    if (!m_preview_focused) {
        if (m_window) {
            m_window->clear_focus();
        }
    }
    rebuild_current_column();
    update_pillar_styles();
    notify_state_changed();
}

void MillerView::set_preview_expanded(bool expanded) {
    if (m_preview_expanded == expanded) return;
    m_preview_expanded = expanded;

    if (m_parent_layout) {
        m_parent_layout->set_visibility(m_preview_expanded ? Visibility::Gone : Visibility::Visible);
    }
    if (m_current_scroll) {
        m_current_scroll->set_visibility(m_preview_expanded ? Visibility::Gone : Visibility::Visible);
    }

    if (m_preview_expanded) {
        set_preview_focused(true);
    } else {
        set_preview_focused(false);
    }

    update_preview_ribbon();
    update_pillar_styles();
    request_redraw();
    notify_state_changed();
}

bool MillerView::on_mouse_move(int lx, int ly, const Rect& bounds) {
    m_last_mouse_x = lx;
    m_last_mouse_y = ly;
    return LinearLayout::on_mouse_move(lx, ly, bounds);
}

bool MillerView::on_mouse_button(int lx, int ly, MouseButton button, bool pressed, const Rect& bounds) {
    if (!is_visible() || !is_enabled()) return false;

    m_last_mouse_x = lx;
    m_last_mouse_y = ly;

    if (button == MouseButton::Left && pressed) {
        // 1. Click within Right Pillar (Preview Column)
        if (m_preview_pillar && m_preview_pillar->get_bounds().contains(Point(lx, ly))) {
            if (!m_preview_focused) {
                set_preview_focused(true);
            }

            bool handled = LinearLayout::on_mouse_button(lx, ly, button, pressed, bounds);
            if (!handled) {
                // Empty space in the right pillar was clicked: maintain current scroll position
                handled = true;
            }
            return handled;
        }

        // 2. Click within Middle Pillar (Current List Column)
        if (m_current_scroll && m_current_scroll->get_bounds().contains(Point(lx, ly))) {
            // Dispatch click to existing rows first! Never call rebuild_current_column()
            // before dispatching, otherwise row views are cleared and their bounds become invalid.
            bool handled = LinearLayout::on_mouse_button(lx, ly, button, pressed, bounds);
            if (!handled) {
                // Empty space in the middle pillar was clicked:
                // Transfer active border back to middle pillar and undim selection
                if (m_preview_focused) {
                    set_preview_focused(false);
                }
                handled = true;
            } else if (m_preview_focused) {
                // A row was clicked while preview was focused:
                // Ensure preview focus is cleanly disarmed
                m_preview_focused = false;
                if (m_window) {
                    m_window->clear_focus();
                }
                update_pillar_styles();
                notify_state_changed();
            }
            return handled;
        }

        // 3. Click within Parent Pillar (Left Column)
        if (m_parent_layout && m_parent_layout->get_bounds().contains(Point(lx, ly))) {
            bool handled = LinearLayout::on_mouse_button(lx, ly, button, pressed, bounds);
            if (!handled) {
                if (m_preview_focused) {
                    set_preview_focused(false);
                }
                handled = true;
            } else if (m_preview_focused) {
                m_preview_focused = false;
                if (m_window) {
                    m_window->clear_focus();
                }
                update_pillar_styles();
                notify_state_changed();
            }
            return handled;
        }
    }

    return LinearLayout::on_mouse_button(lx, ly, button, pressed, bounds);
}

bool MillerView::on_scroll(double delta) {
    if (!is_visible() || !is_enabled()) return false;

    // 1. Right Pillar (Preview Column)
    if (m_preview_scroll && m_preview_scroll->get_bounds().contains(Point(m_last_mouse_x, m_last_mouse_y))) {
        if (!m_preview_focused) {
            set_preview_focused(true);
        }
        m_preview_scroll->on_scroll(delta);
        return true;
    }

    // 2. Middle Pillar (Current List Column)
    if (m_current_scroll && m_current_scroll->get_bounds().contains(Point(m_last_mouse_x, m_last_mouse_y))) {
        if (m_preview_focused) {
            set_preview_focused(false);
        }
        m_current_scroll->on_scroll(delta);
        return true;
    }

    // 3. Left Pillar (Parent List Column)
    if (m_parent_layout && m_parent_layout->get_bounds().contains(Point(m_last_mouse_x, m_last_mouse_y))) {
        if (m_preview_focused) {
            set_preview_focused(false);
        }
        return false;
    }

    return LinearLayout::on_scroll(delta);
}

const std::vector<std::shared_ptr<SettingItem>>& MillerView::get_current_list() const {
    if (!m_search_query.empty()) {
        return m_search_results;
    }
    if (m_path.empty()) {
        return m_root_tree;
    }
    return m_path.back()->children;
}

std::shared_ptr<SettingItem> MillerView::get_current_selected_item() const {
    const auto& list = get_current_list();
    if (list.empty()) return nullptr;
    int idx = std::clamp(m_current_index, 0, static_cast<int>(list.size()) - 1);
    return list[idx];
}

void MillerView::notify_state_changed() {
    update_pillar_styles();
    if (m_on_state_changed) {
        m_on_state_changed();
    }
    request_redraw();
}

void MillerView::rebuild_columns() {
    update_pillar_styles();
    rebuild_parent_column();
    rebuild_current_column();
    rebuild_preview_column();
    request_redraw();
}

std::shared_ptr<View> MillerView::create_parent_hero_box(const std::shared_ptr<Config>& config) {
    auto hero_box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->gravity(Gravity::Center)
        ->padding(16, 24)
        ->spacing(10)
        ->build();

    auto hero_icon = TextViewBuilder::create()
        ->text("⚙")
        ->textSize(static_cast<int>(config->metrics.font_size * 2.8f))
        ->textColor(config->colors.primary.with_alpha(0.6f))
        ->center(true)
        ->build();
    hero_box->add_view(hero_icon);

    auto hero_title = TextViewBuilder::create()
        ->text("Settings")
        ->textSize(static_cast<int>(config->metrics.font_size * 1.3f))
        ->bold(true)
        ->textColor(config->colors.on_surface)
        ->center(true)
        ->build();
    hero_box->add_view(hero_title);

    auto hero_hint = TextViewBuilder::create()
        ->text("↑/↓ Navigate\nEnter Select\nEsc Back")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.6f))
        ->center(true)
        ->build();
    hero_box->add_view(hero_hint);

    return hero_box;
}

std::shared_ptr<View> MillerView::create_empty_search_box(const std::shared_ptr<Config>& config) {
    auto empty_box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->gravity(Gravity::Center)
        ->padding(16, 32)
        ->spacing(8)
        ->build();

    auto empty_icon = TextViewBuilder::create()
        ->text("🔍")
        ->textSize(static_cast<int>(config->metrics.font_size * 2))
        ->textColor(config->colors.on_surface_variant.with_alpha(0.5f))
        ->build();
    empty_box->add_view(empty_icon);

    auto empty_text = TextViewBuilder::create()
        ->text("No settings found")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    empty_box->add_view(empty_text);

    auto empty_hint = TextViewBuilder::create()
        ->text("No matches for \"" + m_search_query + "\"")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.6f))
        ->build();
    empty_box->add_view(empty_hint);

    return empty_box;
}

std::shared_ptr<View> MillerView::create_parent_row_card(const std::shared_ptr<SettingItem>& item,
                                                         bool is_active,
                                                         size_t index,
                                                         const std::shared_ptr<Config>& config) {
    auto item_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(10, 8)
        ->spacing(10)
        ->build();

    auto icon_view = TextViewBuilder::create()
        ->text(item->icon.empty() ? "⚙" : item->icon)
        ->textSize(config->metrics.font_size)
        ->textColor(is_active ? config->colors.primary : config->colors.on_surface_variant)
        ->build();
    LayoutParams icon_lp;
    icon_lp.width = 24;
    icon_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    icon_lp.gravity = Gravity::CenterVertical;
    item_row->add_view(icon_view, icon_lp);

    auto text_box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(1)
        ->build();

    auto title_view = TextViewBuilder::create()
        ->text(item->title)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->ellipsize(true)
        ->build();
    text_box->add_view(title_view);

    if (!item->subtitle.empty()) {
        auto sub_view = TextViewBuilder::create()
            ->text(item->subtitle)
            ->caption(true)
            ->textColor(is_active ? config->colors.primary : config->colors.on_surface_variant.with_alpha(0.7f))
            ->ellipsize(true)
            ->build();
        text_box->add_view(sub_view);
    }

    LayoutParams text_lp;
    text_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    text_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    text_lp.weight = 1.0f;
    text_lp.gravity = Gravity::CenterVertical;
    item_row->add_view(text_box, text_lp);

    Color bg = is_active ? config->colors.primary_container.with_alpha(0.35f) : Color::transparent();
    int stroke_w = is_active ? 1 : 0;
    Color stroke_col = is_active ? config->colors.primary.with_alpha(0.5f) : Color::transparent();

    auto row_card = CardViewBuilder::create()
        ->style(is_active ? CardStyle::Outlined : CardStyle::Filled)
        ->backgroundColor(bg)
        ->stroke(stroke_w, stroke_col)
        ->cornerRadius(get_inner_radius(config))
        ->elevation(0)
        ->addView(item_row)
        ->build();

    row_card->set_on_click_listener([this, item, index]() {
        auto now = std::chrono::steady_clock::now();
        uint64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

        if (m_last_sibling_clicked_index == static_cast<int>(index) &&
            (now_ms - m_last_sibling_click_time_ms) <= 280) {
            cancel_pending_sibling_click();
            execute_sibling_swap(item, index);
        } else {
            cancel_pending_sibling_click();
            m_last_sibling_clicked_index = static_cast<int>(index);
            m_last_sibling_click_time_ms = now_ms;

            if (auto* eng = AppEngine::instance()) {
                m_single_click_timer_id = eng->post_delayed(220, [this, index]() {
                    m_single_click_timer_id = 0;
                    m_last_sibling_clicked_index = -1;
                    m_last_sibling_click_time_ms = 0;
                    execute_shift_up(index);
                });
            } else {
                execute_shift_up(index);
            }
        }
    });

    return row_card;
}

std::shared_ptr<View> MillerView::create_current_row_card(const std::shared_ptr<SettingItem>& item,
                                                         bool is_selected,
                                                         size_t index,
                                                         const std::shared_ptr<Config>& config) {
    auto item_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(10, 8)
        ->spacing(10)
        ->build();

    Color icon_color = config->colors.on_surface_variant;
    if (is_selected) {
        icon_color = m_preview_focused ? config->colors.on_surface_variant : config->colors.primary;
    }

    auto icon_view = TextViewBuilder::create()
        ->text(item->icon.empty() ? "⚙" : item->icon)
        ->textSize(config->metrics.font_size)
        ->textColor(icon_color)
        ->build();
    LayoutParams icon_lp;
    icon_lp.width = 24;
    icon_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    icon_lp.gravity = Gravity::CenterVertical;
    item_row->add_view(icon_view, icon_lp);

    auto text_box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(1)
        ->build();

    auto title_view = TextViewBuilder::create()
        ->text(item->title)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->ellipsize(true)
        ->build();
    text_box->add_view(title_view);

    if (!item->subtitle.empty()) {
        Color sub_color = config->colors.on_surface_variant.with_alpha(0.7f);
        if (is_selected) {
            sub_color = m_preview_focused ? config->colors.on_surface_variant.with_alpha(0.7f) : config->colors.primary;
        }

        auto sub_view = TextViewBuilder::create()
            ->text(item->subtitle)
            ->caption(true)
            ->textColor(sub_color)
            ->ellipsize(true)
            ->build();
        text_box->add_view(sub_view);
    }

    LayoutParams text_lp;
    text_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    text_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    text_lp.weight = 1.0f;
    text_lp.gravity = Gravity::CenterVertical;
    item_row->add_view(text_box, text_lp);

    Color bg = Color::transparent();
    int stroke_w = 0;
    Color stroke_col = Color::transparent();

    if (is_selected) {
        if (m_preview_focused) {
            bg = config->colors.surface_variant.with_alpha(0.50f);
            stroke_w = 1;
            stroke_col = config->colors.outline.with_alpha(0.35f);
        } else {
            bg = config->colors.primary_container.with_alpha(0.65f);
            stroke_w = 1;
            stroke_col = config->colors.primary.with_alpha(0.70f);
        }
    }

    auto row_card = CardViewBuilder::create()
        ->style(is_selected ? CardStyle::Outlined : CardStyle::Filled)
        ->backgroundColor(bg)
        ->stroke(stroke_w, stroke_col)
        ->cornerRadius(get_inner_radius(config))
        ->elevation(0)
        ->addView(item_row)
        ->build();

    row_card->set_on_click_listener([this, index, item]() {
        if (static_cast<int>(index) != m_current_index) {
            m_current_index = static_cast<int>(index);
            m_preview_focused = false;
            if (m_preview_expanded) {
                set_preview_expanded(false);
            }
            if (m_window) {
                m_window->clear_focus();
            }
            rebuild_columns();
            notify_state_changed();
        } else {
            navigate_enter();
        }
    });

    return row_card;
}

void MillerView::rebuild_parent_column() {
    m_parent_layout->clear_views();
    auto config = Config::get();

    if (m_path.empty() && m_search_query.empty()) {
        m_parent_layout->set_gravity(Gravity::Center);
        m_parent_layout->add_view(create_parent_hero_box(config));
        return;
    }

    m_parent_layout->set_gravity(Gravity::None);

    const auto* parent_siblings = (!m_search_query.empty() || m_path.size() == 1)
        ? &m_root_tree
        : &(m_path[m_path.size() - 2]->children);

    auto active_parent = (!m_search_query.empty() || m_path.empty()) ? nullptr : m_path.back();
    for (size_t i = 0; i < parent_siblings->size(); ++i) {
        const auto& item = (*parent_siblings)[i];
        bool is_active = (item == active_parent);
        m_parent_layout->add_view(create_parent_row_card(item, is_active, i, config));
    }
}

void MillerView::rebuild_current_column() {
    m_current_layout->clear_views();
    auto config = Config::get();

    const auto& list = get_current_list();
    if (list.empty()) {
        if (!m_search_query.empty()) {
            m_current_layout->add_view(create_empty_search_box(config));
        }
        return;
    }

    m_current_index = std::clamp(m_current_index, 0, static_cast<int>(list.size()) - 1);

    for (size_t i = 0; i < list.size(); ++i) {
        const auto& item = list[i];
        bool is_selected = (static_cast<int>(i) == m_current_index);
        auto row_card = create_current_row_card(item, is_selected, i, config);
        if (is_selected) {
            m_selected_row_card = row_card;
        }
        m_current_layout->add_view(row_card);
    }

    if (m_current_scroll && m_selected_row_card) {
        m_current_scroll->scroll_to_view(m_selected_row_card.get());
    }
}

void MillerView::add_preview_card(std::shared_ptr<View> card) {
    if (!card) return;
    LayoutParams lp = card->get_layout_params();
    lp.width = static_cast<int>(LayoutDimension::MatchParent);
    if (lp.height <= 0 && lp.height != static_cast<int>(LayoutDimension::MatchParent)) {
        lp.height = static_cast<int>(LayoutDimension::WrapContent);
    }
    m_preview_layout->add_view(std::move(card), lp);
}

// ==============================================================================
void MillerView::deferred_rebuild_preview(int delay_ms) {
    if (auto* eng = AppEngine::instance()) {
        eng->post_delayed(delay_ms, [this]() {
            rebuild_preview_column();
            notify_state_changed();
        });
    } else {
        rebuild_preview_column();
        notify_state_changed();
    }
}
void MillerView::rebuild_preview_column() {
    int focused_index = -1;
    if (m_window && m_preview_layout) {
        View* current_focus = m_window->get_focused_view();
        if (current_focus) {
            std::vector<View*> old_focusables;
            m_preview_layout->collect_focusable_views(old_focusables);
            for (size_t i = 0; i < old_focusables.size(); ++i) {
                if (old_focusables[i] == current_focus) {
                    focused_index = static_cast<int>(i);
                    break;
                }
            }
        }
    }

    m_preview_layout->clear_views();
    m_active_slider = nullptr;
    m_active_slider_label = nullptr;
    m_active_action_feedback = nullptr;

    auto config = Config::get();
    auto item = get_current_selected_item();

    update_preview_ribbon();

    if (!item) {
        render_empty_preview(config);
        return;
    }

    if (item->is_category) {
        render_category_children_preview(item, config);
        return;
    }

    // --- DEAD END PREVIEW: INTERACTIVE SETTINGS DETAIL PANEL ---
    m_preview_layout->set_padding(Padding(14, 4, 14, 14));
    m_preview_layout->set_divider_spacing(10);

    if (item->id == "sound_speaker" || item->id == "sound_mic") {
        render_sound_preview(item, config);
    } else if (item->id == "net_wifi") {
        render_wifi_preview(item, config);
    } else if (item->id == "net_bluetooth") {
        render_bluetooth_preview(item, config);
    } else if (item->id == "net_ethernet") {
        render_ethernet_preview(item, config);
    } else if (item->id == "net_airplane") {
        render_airplane_preview(item, config);
    } else if (item->is_private_dns) {
        render_dns_preview(item, config);
    } else if (item->is_firewall) {
        render_firewall_preview(item, config);
    } else if (item->is_per_app_network) {
        render_app_network_preview(item, config);
    } else if (item->is_display_brightness) {
        render_brightness_preview(item, config);
    } else if (item->is_tiling_config) {
        render_tiling_preview(item, config);
    } else if (item->is_wallpaper || item->id == "ws_wallpaper") {
        render_wallpaper_preview(item, config);
    } else if (item->is_keybinding_group) {
        render_keybindings_preview(item, config);
    } else if (item->is_timeout_and_lock) {
        render_timeout_lock_preview(item, config);
    } else if (item->is_display_nightlight) {
        render_night_light_preview(item, config);
    } else if (item->is_battery_info) {
        render_battery_preview(item, config);
    } else if (item->is_power_mode) {
        render_power_mode_preview(item, config);
    } else if (item->is_monitor) {
        render_monitors_preview(item, config);
    } else if (item->is_installed_app) {
        render_installed_app_preview(item, config);
    } else if (item->is_default_app_role) {
        render_default_app_role_preview(item, config);
    } else {
        render_generic_fallback_preview(item, config);
    }

    // Seamlessly preserve focus on the active control across live updates
    if (m_window && m_preview_layout) {
        if (focused_index >= 0) {
            std::vector<View*> new_focusables;
            m_preview_layout->collect_focusable_views(new_focusables);
            if (focused_index < static_cast<int>(new_focusables.size())) {
                m_window->set_focused_view(new_focusables[focused_index]);
            } else if (!new_focusables.empty()) {
                m_window->set_focused_view(new_focusables.front());
            }
        } else if (m_preview_focused && m_window->get_focused_view() == nullptr) {
            std::vector<View*> new_focusables;
            m_preview_layout->collect_focusable_views(new_focusables);
            if (!new_focusables.empty()) {
                m_window->set_focused_view(new_focusables.front());
            }
        }
    }
}

bool MillerView::navigate_up() {
    cancel_pending_sibling_click();
    if (m_preview_expanded) {
        set_preview_expanded(false);
    }
    if (m_preview_focused) {
        set_preview_focused(false);
    }
    const auto& list = get_current_list();
    if (list.empty()) return false;

    if (m_current_index > 0) {
        m_current_index--;
        rebuild_columns();
        notify_state_changed();
        return true;
    }
    return false;
}

bool MillerView::navigate_down() {
    cancel_pending_sibling_click();
    if (m_preview_expanded) {
        set_preview_expanded(false);
    }
    if (m_preview_focused) {
        set_preview_focused(false);
    }
    const auto& list = get_current_list();
    if (list.empty()) return false;

    if (m_current_index < static_cast<int>(list.size()) - 1) {
        m_current_index++;
        rebuild_columns();
        notify_state_changed();
        return true;
    }
    return false;
}

bool MillerView::navigate_enter() {
    cancel_pending_sibling_click();
    auto item = get_current_selected_item();
    if (!item) return false;

    if (item->is_category) {
        m_path.push_back(item);
        m_history_indices.push_back(m_current_index);
        m_current_index = 0;
        m_preview_focused = false;
        if (m_preview_expanded) {
            set_preview_expanded(false);
        }
        if (m_current_scroll) {
            m_current_scroll->set_scroll_y(0);
        }
        rebuild_columns();
        notify_state_changed();
        return true;
    } else {
        // Entering leaf setting: expand preview to full width!
        set_preview_expanded(true);
        if (m_preview_layout) {
            std::vector<View*> focusables;
            m_preview_layout->collect_focusable_views(focusables);
            if (!focusables.empty() && m_window) {
                m_window->set_focused_view(focusables.front());
            }
        }
        return true;
    }
}

void MillerView::set_search_query(const std::string& query) {
    m_search_query = query;
    m_search_results.clear();
    m_current_index = 0;
    m_preview_focused = false;
    if (m_preview_expanded) {
        set_preview_expanded(false);
    }
    if (m_current_scroll) {
        m_current_scroll->set_scroll_y(0);
    }

    if (!m_search_query.empty()) {
        std::string lower_query = m_search_query;
        std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        std::function<void(const std::vector<std::shared_ptr<SettingItem>>&)> search_recursive =
            [&](const std::vector<std::shared_ptr<SettingItem>>& items) {
                for (const auto& item : items) {
                    std::string t = item->title;
                    std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c) { return std::tolower(c); });
                    std::string sub = item->subtitle;
                    std::transform(sub.begin(), sub.end(), sub.begin(), [](unsigned char c) { return std::tolower(c); });

                    if (t.find(lower_query) != std::string::npos ||
                        sub.find(lower_query) != std::string::npos ||
                        item->id.find(lower_query) != std::string::npos) {
                        m_search_results.push_back(item);
                    }

                    if (!item->children.empty()) {
                        search_recursive(item->children);
                    }
                }
            };

        search_recursive(m_root_tree);
    }

    rebuild_columns();
    notify_state_changed();
}

bool MillerView::navigate_back() {
    cancel_pending_sibling_click();
    if (m_window) {
        m_window->clear_focus();
    }
    if (!m_search_query.empty()) {
        m_search_query.clear();
        m_search_results.clear();
        m_current_index = 0;
        m_preview_focused = false;
        if (m_preview_expanded) {
            set_preview_expanded(false);
        }
        rebuild_columns();
        notify_state_changed();
        return true;
    }

    if (m_preview_expanded) {
        set_preview_expanded(false);
        return true;
    }

    if (m_preview_focused) {
        set_preview_focused(false);
        return true;
    }

    if (!m_path.empty()) {
        m_path.pop_back();
        if (!m_history_indices.empty()) {
            m_current_index = m_history_indices.back();
            m_history_indices.pop_back();
        } else {
            m_current_index = 0;
        }
        m_preview_focused = false;
        m_preview_expanded = false;
        rebuild_columns();
        notify_state_changed();
        return true;
    }

    return false;
}


bool MillerView::trigger_active_action() {
    auto item = get_current_selected_item();
    if (item && item->has_action) {
        item->last_action_message = "Done at " + std::to_string(std::time(nullptr));
        if (m_active_action_feedback) {
            m_active_action_feedback->set_text(item->last_action_message);
        }
        rebuild_preview_column();
        notify_state_changed();
        return true;
    }
    return false;
}

std::string MillerView::get_breadcrumb_path() const {
    if (!m_search_query.empty()) {
        std::string path = "Settings › Search: \"" + m_search_query + "\"";
        auto current = get_current_selected_item();
        if (current) {
            path += " › " + current->title;
        }
        return path;
    }
    std::string path = "Settings";
    for (const auto& item : m_path) {
        path += " › " + item->title;
    }
    auto current = get_current_selected_item();
    if (current) {
        path += " › " + current->title;
    }
    return path;
}

} // namespace miqu
