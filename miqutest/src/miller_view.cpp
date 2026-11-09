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

    // 3. Column 3: Preview (Right) Pillar
    m_preview_layout = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(10)
        ->padding(14, 14)
        ->backgroundColor(Color::transparent())
        ->build();

    m_preview_scroll = ScrollViewBuilder::create()
        ->contentView(m_preview_layout)
        ->scrollbar(true)
        ->build();

    LayoutParams preview_lp;
    preview_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    preview_lp.height = static_cast<int>(LayoutDimension::MatchParent);
    preview_lp.weight = 1.0f;
    add_view(m_preview_scroll, preview_lp);

    sync_theme();
    rebuild_columns();
}

void MillerView::sync_theme() {
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

    // 3. Column 3: Right (Preview) Pillar (Subtle border ONLY when actively focused, transparent background)
    if (m_preview_scroll) {
        m_preview_scroll->set_corner_radius(radius);
        m_preview_scroll->set_stroke(m_preview_focused ? 1 : 0, m_preview_focused ? active_border : Color::transparent());
        m_preview_scroll->set_background_color(Color::transparent());
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
        if (m_preview_scroll && m_preview_scroll->get_bounds().contains(Point(lx, ly))) {
            if (!m_preview_focused) {
                set_preview_focused(true);
            }

            bool handled = LinearLayout::on_mouse_button(lx, ly, button, pressed, bounds);
            if (!handled) {
                // Empty space in the right pillar was clicked:
                // Focus first interactive control, or if none, focus the scroll container
                if (m_preview_layout) {
                    std::vector<View*> focusables;
                    m_preview_layout->collect_focusable_views(focusables);
                    if (!focusables.empty()) {
                        focusables.front()->request_focus();
                    } else if (m_window) {
                        m_preview_scroll->set_focusable(true);
                        m_preview_scroll->request_focus();
                    }
                }
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

void MillerView::rebuild_parent_column() {
    m_parent_layout->clear_views();
    auto config = Config::get();

    if (m_path.empty() && m_search_query.empty()) {
        m_parent_layout->set_gravity(Gravity::Center);

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

        m_parent_layout->add_view(hero_box);
        return;
    }

    m_parent_layout->set_gravity(Gravity::None);

    // Depth > 0 or Search Mode: Show categories for spatial context
    const std::vector<std::shared_ptr<SettingItem>>* parent_siblings = nullptr;
    if (!m_search_query.empty() || m_path.size() == 1) {
        parent_siblings = &m_root_tree;
    } else {
        parent_siblings = &(m_path[m_path.size() - 2]->children);
    }

    auto active_parent = (!m_search_query.empty() || m_path.empty()) ? nullptr : m_path.back();
    for (size_t i = 0; i < parent_siblings->size(); ++i) {
        const auto& item = (*parent_siblings)[i];
        bool is_active = (item == active_parent);

        auto item_row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->padding(10, 8)
            ->spacing(10)
            ->build();

        // Left Icon
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

        // Text container (Title and optional contextual Subtitle)
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

        // Hybrid Interaction: Single-click shifts up one level (Option A)
        // Double-click directly swaps active branch at current depth (Option B)
        row_card->set_on_click_listener([this, item, i]() {
            auto now = std::chrono::steady_clock::now();
            uint64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

            if (m_last_sibling_clicked_index == static_cast<int>(i) &&
                (now_ms - m_last_sibling_click_time_ms) <= 280) {
                // Double Click: cancel pending single-click and swap sibling branch
                cancel_pending_sibling_click();
                execute_sibling_swap(item, i);
            } else {
                // First Click: schedule single-click shift up after debounce
                cancel_pending_sibling_click();
                m_last_sibling_clicked_index = static_cast<int>(i);
                m_last_sibling_click_time_ms = now_ms;

                if (auto* eng = AppEngine::instance()) {
                    m_single_click_timer_id = eng->post_delayed(220, [this, i]() {
                        m_single_click_timer_id = 0;
                        m_last_sibling_clicked_index = -1;
                        m_last_sibling_click_time_ms = 0;
                        execute_shift_up(i);
                    });
                } else {
                    execute_shift_up(i);
                }
            }
        });

        m_parent_layout->add_view(row_card);
    }
}

void MillerView::rebuild_current_column() {
    m_current_layout->clear_views();
    auto config = Config::get();

    const auto& list = get_current_list();
    if (list.empty()) {
        if (!m_search_query.empty()) {
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

            m_current_layout->add_view(empty_box);
        }
        return;
    }

    m_current_index = std::clamp(m_current_index, 0, static_cast<int>(list.size()) - 1);

    // List items
    for (size_t i = 0; i < list.size(); ++i) {
        const auto& item = list[i];
        bool is_selected = (static_cast<int>(i) == m_current_index);

        auto item_row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->padding(10, 8)
            ->spacing(10)
            ->build();

        // Left Icon
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

        // Text container (Title and optional contextual Subtitle)
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

        // Wrap in CardView for selection styling
        Color bg = Color::transparent();
        int stroke_w = 0;
        Color stroke_col = Color::transparent();

        if (is_selected) {
            if (m_preview_focused) {
                // Inactive / dormant selection while focus is in preview controls
                bg = config->colors.surface_variant.with_alpha(0.50f);
                stroke_w = 1;
                stroke_col = config->colors.outline.with_alpha(0.35f);
            } else {
                // Active vibrant selection
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

        if (is_selected) {
            m_selected_row_card = row_card;
        }

        // Click handler
        row_card->set_on_click_listener([this, i, item]() {
            if (static_cast<int>(i) != m_current_index) {
                m_current_index = static_cast<int>(i);
                m_preview_focused = false;
                if (m_window) {
                    m_window->clear_focus();
                }
                rebuild_columns();
                notify_state_changed();
            } else if (item->is_category) {
                navigate_enter();
            } else {
                if (m_preview_focused) {
                    set_preview_focused(false);
                }
            }
        });

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
    m_preview_layout->clear_views();
    m_active_slider = nullptr;
    m_active_slider_label = nullptr;
    m_active_action_feedback = nullptr;

    auto config = Config::get();
    auto item = get_current_selected_item();

    if (!item) {
        render_empty_preview(config);
        return;
    }

    if (item->is_category) {
        render_category_children_preview(item, config);
        return;
    }

    // --- DEAD END PREVIEW: INTERACTIVE SETTINGS DETAIL PANEL ---
    m_preview_layout->set_padding(Padding(14, 14));
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
}

bool MillerView::navigate_up() {
    cancel_pending_sibling_click();
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
        if (m_current_scroll) {
            m_current_scroll->set_scroll_y(0);
        }
        rebuild_columns();
        notify_state_changed();
        return true;
    } else {
        // Entering leaf setting: transfer keyboard focus to the first focusable widget in the preview column
        set_preview_focused(true);
        if (m_preview_layout) {
            std::vector<View*> focusables;
            m_preview_layout->collect_focusable_views(focusables);
            if (!focusables.empty()) {
                focusables.front()->request_focus();
            } else if (m_window) {
                m_preview_scroll->set_focusable(true);
                m_preview_scroll->request_focus();
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
        rebuild_columns();
        notify_state_changed();
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
        rebuild_columns();
        notify_state_changed();
        return true;
    }

    return false;
}

bool MillerView::toggle_active() {
    auto item = get_current_selected_item();
    if (item && !item->is_category && item->has_switch) {
        item->switch_val = !item->switch_val;
        rebuild_columns();
        notify_state_changed();
        return true;
    }
    return false;
}

bool MillerView::adjust_active_slider(float delta) {
    auto item = get_current_selected_item();
    if (item && !item->is_category && item->has_slider && !item->slider_read_only) {
        item->slider_val = std::clamp(item->slider_val + delta, 0.0f, 1.0f);
        if (m_active_slider) {
            m_active_slider->set_value(item->slider_val);
        }
        rebuild_current_column();
        rebuild_preview_column();
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
