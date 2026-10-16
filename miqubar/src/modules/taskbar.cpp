#include "taskbar.hpp"
#include "../config/bar_config.hpp"
#include "miqutoolkit/core/config.hpp"
#include "miqutoolkit/core/app_engine.hpp"
#include "miqutoolkit/view/button.hpp"
#include "miqutoolkit/view/card_view.hpp"
#include "miqutoolkit/view/image_view.hpp"
#include "miqutoolkit/view/popup_menu.hpp"
#include "miqutoolkit/view/text_view.hpp"
#include "miqutoolkit/core/window.hpp"
#include "miqutoolkit/system/app_manager.hpp"
#include "miqutoolkit/view/divider_view.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>

namespace miqubar {

class WindowRowView : public miqu::LinearLayout {
public:
    WindowRowView(int index,
                  miqu::WindowInfo win,
                  std::function<void()> on_activate,
                  std::function<void()> on_close)
        : LinearLayout(miqu::Orientation::Horizontal),
          m_index(index),
          m_win(std::move(win)),
          m_on_activate(std::move(on_activate)),
          m_on_close(std::move(on_close)) {

        set_gravity(miqu::Gravity::CenterVertical);
        set_padding(8, 4, 6, 4);
        set_divider_spacing(8);
        set_layout_params(miqu::LayoutParams(
            static_cast<int>(miqu::LayoutDimension::MatchParent),
            34
        ));

        update_style();
        setup_views();
    }

    void update_style() {
        auto config = miqu::Config::get();
        int radius = config->metrics.corner_radius > 0 ? std::min(8, config->metrics.corner_radius - 2) : 6;
        set_corner_radius(radius);

        if (m_win.is_active) {
            set_background_color(config->colors.primary.with_alpha(0.16f));
            set_stroke(1, config->colors.primary.with_alpha(0.40f));
        } else if (m_hovered) {
            set_background_color(config->colors.surface_variant.with_alpha(0.55f));
            set_stroke(0, miqu::Color::transparent());
        } else {
            set_background_color(miqu::Color::transparent());
            set_stroke(0, miqu::Color::transparent());
        }
    }

    void setup_views() {
        auto config = miqu::Config::get();
        int base_font_size = config->metrics.font_size > 0 ? config->metrics.font_size : 11;

        // 1. Number Badge (using flat toolkit Button with centered text & pill shape)
        auto badge_bg = m_win.is_active
            ? config->colors.primary.with_alpha(0.25f)
            : config->colors.surface_variant.with_alpha(0.85f);
        auto badge_fg = m_win.is_active
            ? config->colors.primary
            : config->colors.on_surface_variant;

        auto badge = miqu::ButtonBuilder::create()
            ->text(std::to_string(m_index))
            ->bold(true)
            ->textSize(base_font_size - 1)
            ->flat(true)
            ->padding(0, 0)
            ->cornerRadius(5)
            ->customColors(badge_bg, badge_fg)
            ->build();
        badge->set_layout_params(miqu::LayoutParams(22, 20));
        add_view(badge);

        // 2. Status Dot (using 6x6 toolkit Button with corner radius 3)
        miqu::Color dot_color;
        if (m_win.is_active) {
            dot_color = config->colors.primary;
        } else if (m_win.is_minimized) {
            dot_color = config->colors.outline;
        } else {
            dot_color = config->colors.on_surface_variant.with_alpha(0.40f);
        }

        auto dot = miqu::ButtonBuilder::create()
            ->text("")
            ->flat(true)
            ->padding(0, 0)
            ->cornerRadius(3)
            ->customColors(dot_color, miqu::Color::transparent())
            ->build();
        dot->set_layout_params(miqu::LayoutParams(6, 6));
        add_view(dot);

        // 3. Title TextView
        std::string title_text = !m_win.title.empty() ? m_win.title : (!m_win.app_id.empty() ? m_win.app_id : "Window");
        if (m_win.is_minimized) {
            title_text += " (minimized)";
        }
        auto title_color = m_win.is_active
            ? config->colors.on_surface
            : config->colors.on_surface.with_alpha(m_win.is_minimized ? 0.65f : 0.88f);

        auto title = miqu::TextViewBuilder::create()
            ->text(title_text)
            ->textSize(base_font_size)
            ->bold(m_win.is_active)
            ->textColor(title_color)
            ->ellipsize(true)
            ->ellipsizeMode(miqu::EllipsizeMode::End)
            ->build();
        title->set_layout_params(miqu::LayoutParams(0, static_cast<int>(miqu::LayoutDimension::WrapContent), 1.0f));
        add_view(title);

        // 4. Close Button [✕]
        auto close_btn = miqu::ButtonBuilder::create()
            ->text("")
            ->icon("window-close")
            ->iconSize(14)
            ->flat(true)
            ->padding(4, 4)
            ->customColors(miqu::Color(0, 0, 0, 0), config->colors.on_surface_variant.with_alpha(0.6f))
            ->onClick([this]() {
                if (m_on_close) m_on_close();
            })
            ->build();
        close_btn->set_layout_params(miqu::LayoutParams(24, 24));
        add_view(close_btn);
    }

    bool on_mouse_move(int lx, int ly, const miqu::Rect& bounds) override {
        bool inside = bounds.contains(miqu::Point(lx, ly));
        if (inside != m_hovered) {
            m_hovered = inside;
            update_style();
            request_redraw();
        }
        return miqu::LinearLayout::on_mouse_move(lx, ly, bounds);
    }

    bool on_mouse_button(int lx, int ly, miqu::MouseButton button, bool pressed, const miqu::Rect& bounds) override {
        if (miqu::LinearLayout::on_mouse_button(lx, ly, button, pressed, bounds)) {
            return true;
        }
        if (button == miqu::MouseButton::Left && !pressed) {
            if (bounds.contains(miqu::Point(lx, ly))) {
                if (m_on_activate) m_on_activate();
                return true;
            }
        }
        return false;
    }

private:
    int m_index = 1;
    miqu::WindowInfo m_win;
    std::function<void()> m_on_activate;
    std::function<void()> m_on_close;
    bool m_hovered = false;
};

class GroupPreviewCardView : public miqu::CardView {
public:
    explicit GroupPreviewCardView(std::function<void(bool)> on_hover_state)
        : m_on_hover_state(std::move(on_hover_state)) {}

    bool on_mouse_move(int lx, int ly, const miqu::Rect& bounds) override {
        bool inside = bounds.contains(miqu::Point(lx, ly));
        if (inside != m_hovered) {
            m_hovered = inside;
            if (m_on_hover_state) {
                m_on_hover_state(m_hovered);
            }
        }
        return miqu::CardView::on_mouse_move(lx, ly, bounds);
    }

private:
    bool m_hovered = false;
    std::function<void(bool)> m_on_hover_state;
};

class TaskbarItemButton : public miqu::Button {
public:
    TaskbarItemButton(std::vector<miqu::WindowInfo> windows,
                      std::string icon_path,
                      std::function<void(const std::vector<miqu::WindowInfo>&, miqu::View*)> on_context_menu,
                      std::function<void(TaskbarItemButton*, bool)> on_hover,
                      std::function<void(TaskbarItemButton*)> on_toggle_popup)
        : m_windows(std::move(windows)),
          m_on_context_menu(std::move(on_context_menu)),
          m_on_hover(std::move(on_hover)),
          m_on_toggle_popup(std::move(on_toggle_popup)) {

        const auto& cfg = BarConfig::get();
        auto config = miqu::Config::get();
        int fs = config->metrics.font_size > 0 ? config->metrics.font_size : 11;

        if (m_windows.empty()) return;

        // Label formatting
        if (cfg.icon_only) {
            set_text("");
        } else {
            if (m_windows.size() == 1) {
                std::string title = m_windows[0].title;
                if (cfg.max_title_chars > 0 && static_cast<int>(title.length()) > cfg.max_title_chars) {
                    title = title.substr(0, cfg.max_title_chars) + "…";
                }
                set_text(title);
            } else {
                std::string app_name = m_windows[0].app_id;
                if (app_name.empty()) app_name = "App";
                set_text(app_name + " (" + std::to_string(m_windows.size()) + ")");
            }
            set_text_size(fs);
        }

        // Icon resolution
        if (!icon_path.empty()) {
            set_icon(icon_path);
        } else if (!m_windows[0].app_id.empty()) {
            set_icon(m_windows[0].app_id);
        }

        int isz = cfg.icon_size > 0 ? cfg.icon_size : 24;
        set_icon_size(isz);
        set_padding(2, 2);
        set_flat(true);

        // Tooltip handling:
        // Single window: lightweight tooltip showing full window title
        // Multi-window group: no flat tooltip bubble; hover reveals rich preview card
        if (m_windows.size() == 1) {
            std::string tip = !m_windows[0].title.empty() ? m_windows[0].title : m_windows[0].app_id;
            set_tooltip(tip);
        } else {
            set_tooltip("");
        }

        // Primary click handling
        set_on_click_listener([this]() {
            if (m_windows.size() == 1) {
                if (m_windows[0].is_active) {
                    m_windows[0].set_minimized(true);
                } else {
                    m_windows[0].activate();
                }
            } else if (m_windows.size() > 1) {
                if (m_on_toggle_popup) {
                    m_on_toggle_popup(this);
                }
            }
        });
    }

    const std::vector<miqu::WindowInfo>& get_windows() const { return m_windows; }

    bool is_group() const { return m_windows.size() > 1; }

    bool has_active_window() const {
        return std::any_of(m_windows.begin(), m_windows.end(), [](const auto& w) {
            return w.is_active;
        });
    }

    void draw(cairo_t* cr, const miqu::Rect& bounds) override {
        if (!is_visible() || !cr || bounds.width <= 0 || bounds.height <= 0) return;

        auto config = miqu::Config::get();
        const auto& cfg = BarConfig::get();
        int radius = config->metrics.corner_radius > 0 ? config->metrics.corner_radius : 8;

        bool any_active = has_active_window();

        // If any window in this button is active, paint a subtle translucent background tile
        if (any_active) {
            miqu::CardView::draw_rounded_rect(cr, bounds.x, bounds.y, bounds.width, bounds.height, radius);
            auto active_bg = config->colors.surface_variant.with_alpha(0.35f);
            cairo_set_source_rgba(cr, active_bg.r, active_bg.g, active_bg.b, active_bg.a);
            cairo_fill(cr);
        }

        // Draw standard flat button (icon, text, hover state)
        miqu::Button::draw(cr, bounds);

        // Draw bottom indicators
        if (cfg.show_active_underline && !m_windows.empty()) {
            bool is_top = (cfg.position == "top");
            int pill_h = 3;
            int ind_y = is_top ? (bounds.y + 2) : (bounds.y + bounds.height - pill_h - 2);

            if (m_windows.size() == 1) {
                // Standalone window: pill for active, dot for inactive
                if (any_active) {
                    int pill_w = cfg.icon_only ? 18 : std::min(36, bounds.width - 24);
                    int pill_x = bounds.x + (bounds.width - pill_w) / 2;
                    miqu::CardView::draw_rounded_rect(cr, pill_x, ind_y, pill_w, pill_h, 1.5);
                    cairo_set_source_rgba(cr, config->colors.primary.r, config->colors.primary.g, config->colors.primary.b, config->colors.primary.a);
                    cairo_fill(cr);
                } else {
                    int dot_w = 6;
                    int dot_x = bounds.x + (bounds.width - dot_w) / 2;
                    miqu::CardView::draw_rounded_rect(cr, dot_x, ind_y, dot_w, pill_h, 1.5);
                    auto ind_col = config->colors.on_surface_variant.with_alpha(0.55f);
                    cairo_set_source_rgba(cr, ind_col.r, ind_col.g, ind_col.b, ind_col.a);
                    cairo_fill(cr);
                }
            } else {
                // Multi-window group indicator:
                if (any_active) {
                    // Active accent pill + companion instance dot
                    int pill_w = 16;
                    int dot_w = 4;
                    int spacing = 3;
                    int total_w = pill_w + spacing + dot_w;
                    int start_x = bounds.x + (bounds.width - total_w) / 2;

                    // Active pill
                    miqu::CardView::draw_rounded_rect(cr, start_x, ind_y, pill_w, pill_h, 1.5);
                    cairo_set_source_rgba(cr, config->colors.primary.r, config->colors.primary.g, config->colors.primary.b, config->colors.primary.a);
                    cairo_fill(cr);

                    // Instance dot
                    miqu::CardView::draw_rounded_rect(cr, start_x + pill_w + spacing, ind_y, dot_w, pill_h, 1.5);
                    cairo_set_source_rgba(cr, config->colors.primary.r, config->colors.primary.g, config->colors.primary.b, 0.70f);
                    cairo_fill(cr);
                } else {
                    // Two dots side-by-side indicating multiple inactive instances
                    int dot_w = 5;
                    int spacing = 3;
                    int total_w = dot_w * 2 + spacing;
                    int start_x = bounds.x + (bounds.width - total_w) / 2;
                    auto ind_col = config->colors.on_surface_variant.with_alpha(0.55f);
                    cairo_set_source_rgba(cr, ind_col.r, ind_col.g, ind_col.b, ind_col.a);

                    for (int i = 0; i < 2; ++i) {
                        miqu::CardView::draw_rounded_rect(cr, start_x + i * (dot_w + spacing), ind_y, dot_w, pill_h, 1.5);
                        cairo_fill(cr);
                    }
                }
            }
        }
    }

    bool on_mouse_move(int lx, int ly, const miqu::Rect& bounds) override {
        bool h = bounds.contains(miqu::Point(lx, ly));
        if (h != m_is_hovered) {
            m_is_hovered = h;
            if (m_on_hover) {
                m_on_hover(this, m_is_hovered);
            }
        }
        return miqu::Button::on_mouse_move(lx, ly, bounds);
    }

    bool on_mouse_button(int lx, int ly, miqu::MouseButton button, bool pressed, const miqu::Rect& bounds) override {
        if (button == miqu::MouseButton::Right && pressed) {
            if (m_on_context_menu) {
                m_on_context_menu(m_windows, this);
            }
            return true;
        }
        if (button == miqu::MouseButton::Middle && pressed) {
            const auto& cfg = BarConfig::get();
            if (cfg.middle_click_close && !m_windows.empty()) {
                auto it = std::find_if(m_windows.begin(), m_windows.end(), [](const auto& w) { return w.is_active; });
                if (it != m_windows.end()) {
                    it->close();
                } else {
                    m_windows.front().close();
                }
            }
            return true;
        }
        return miqu::Button::on_mouse_button(lx, ly, button, pressed, bounds);
    }

private:
    std::vector<miqu::WindowInfo> m_windows;
    std::function<void(const std::vector<miqu::WindowInfo>&, miqu::View*)> m_on_context_menu;
    std::function<void(TaskbarItemButton*, bool)> m_on_hover;
    std::function<void(TaskbarItemButton*)> m_on_toggle_popup;
    bool m_is_hovered = false;
};

TaskbarView::TaskbarView() : LinearLayout(miqu::Orientation::Horizontal) {
    set_divider_spacing(4);
    sync_windows();

    auto mgr = miqu::WindowManager::get();
    if (mgr) {
        mgr->on_windows_changed([this]() {
            sync_windows();
        });
    }
}

TaskbarView::~TaskbarView() {
    hide_context_menu();
    hide_group_popup();
}

void TaskbarView::hide_context_menu() {
    if (m_menu_window) {
        auto win = m_menu_window;
        m_menu_window = nullptr;
        win->close();
    }
}

void TaskbarView::hide_group_popup() {
    auto* engine = miqu::AppEngine::instance();
    if (engine) {
        if (m_hover_timer_id != 0) {
            engine->cancel_delayed(m_hover_timer_id);
            m_hover_timer_id = 0;
        }
        if (m_dismiss_timer_id != 0) {
            engine->cancel_delayed(m_dismiss_timer_id);
            m_dismiss_timer_id = 0;
        }
    }
    m_preview_anchor = nullptr;
    if (m_preview_window) {
        auto win = m_preview_window;
        m_preview_window = nullptr;
        win->close();
    }
}

void TaskbarView::on_item_hover(TaskbarItemButton* btn, bool hovered) {
    if (!btn) return;
    auto* engine = miqu::AppEngine::instance();
    if (!engine) return;

    const auto& cfg = BarConfig::get();
    if (!cfg.show_hover_popup) return;

    if (hovered) {
        if (m_dismiss_timer_id != 0) {
            engine->cancel_delayed(m_dismiss_timer_id);
            m_dismiss_timer_id = 0;
        }

        // Standalone item: TooltipManager automatically handles the window title tooltip.
        // Dismiss any existing group popup if the cursor entered a standalone item.
        if (btn->get_windows().size() <= 1) {
            if (m_preview_window) {
                hide_group_popup();
            }
            return;
        }

        if (m_preview_window && m_preview_anchor == btn) {
            return;
        }

        m_preview_anchor = btn;
        if (m_hover_timer_id != 0) {
            engine->cancel_delayed(m_hover_timer_id);
        }

        int delay = m_preview_window ? 80 : cfg.hover_delay_ms;
        m_hover_timer_id = engine->post_delayed(delay, [this, btn]() {
            m_hover_timer_id = 0;
            show_group_popup(btn);
        });
    } else {
        if (m_hover_timer_id != 0) {
            engine->cancel_delayed(m_hover_timer_id);
            m_hover_timer_id = 0;
        }

        if (m_preview_window && m_preview_anchor == btn) {
            if (m_dismiss_timer_id != 0) {
                engine->cancel_delayed(m_dismiss_timer_id);
            }
            // 220ms grace period so pointer can smoothly transition across the gap into the popup
            m_dismiss_timer_id = engine->post_delayed(220, [this]() {
                m_dismiss_timer_id = 0;
                hide_group_popup();
            });
        }
    }
}

void TaskbarView::toggle_group_popup(TaskbarItemButton* btn) {
    if (!btn) return;
    if (m_preview_window && m_preview_anchor == btn) {
        hide_group_popup();
    } else {
        show_group_popup(btn);
    }
}

void TaskbarView::sync_windows() {
    clear_views();
    auto mgr = miqu::WindowManager::get();
    if (mgr) {
        m_windows = mgr->get_windows();
    }

    const auto& cfg = BarConfig::get();
    int item_h = std::max(28, cfg.height - 8);
    int item_w = cfg.icon_only ? item_h : cfg.item_max_width;

    // Window grouping
    std::vector<std::vector<miqu::WindowInfo>> grouped_list;
    if (cfg.group_apps) {
        std::unordered_map<std::string, size_t> group_index;
        for (const auto& win : m_windows) {
            std::string key = !win.app_id.empty() ? win.app_id : win.title;
            if (key.empty()) key = "unknown";

            auto it = group_index.find(key);
            if (it != group_index.end()) {
                grouped_list[it->second].push_back(win);
            } else {
                group_index[key] = grouped_list.size();
                grouped_list.push_back({win});
            }
        }
    } else {
        for (const auto& win : m_windows) {
            grouped_list.push_back({win});
        }
    }

    for (const auto& grp : grouped_list) {
        if (grp.empty()) continue;
        std::string icon = resolve_window_icon(grp.front());
        auto btn = std::make_shared<TaskbarItemButton>(
            grp,
            icon,
            [this](const std::vector<miqu::WindowInfo>& w_grp, miqu::View* anchor) {
                show_context_menu(w_grp, anchor);
            },
            [this](TaskbarItemButton* b, bool hovered) {
                on_item_hover(b, hovered);
            },
            [this](TaskbarItemButton* b) {
                toggle_group_popup(b);
            }
        );
        btn->set_layout_params(miqu::LayoutParams(item_w, item_h));
        add_view(btn);
    }

    request_redraw();
}

void TaskbarView::show_group_popup(TaskbarItemButton* anchor) {
    if (!anchor) return;
    const auto& windows = anchor->get_windows();
    if (windows.size() <= 1) return;

    hide_group_popup();
    m_preview_anchor = anchor;

    auto config = miqu::Config::get();
    const auto& cfg = BarConfig::get();

    auto card = std::make_shared<GroupPreviewCardView>([this](bool hovered) {
        auto* engine = miqu::AppEngine::instance();
        if (!engine) return;
        if (hovered) {
            if (m_dismiss_timer_id != 0) {
                engine->cancel_delayed(m_dismiss_timer_id);
                m_dismiss_timer_id = 0;
            }
        } else {
            if (m_dismiss_timer_id != 0) {
                engine->cancel_delayed(m_dismiss_timer_id);
            }
            m_dismiss_timer_id = engine->post_delayed(220, [this]() {
                m_dismiss_timer_id = 0;
                hide_group_popup();
            });
        }
    });

    card->set_style(miqu::CardStyle::Elevated);
    int radius = config->metrics.corner_radius > 0 ? config->metrics.corner_radius + 2 : 12;
    card->set_radius(radius);
    card->set_elevation(0);
    card->set_padding(8, 8);

    auto root_layout = std::make_shared<miqu::LinearLayout>(miqu::Orientation::Vertical);
    root_layout->set_divider_spacing(3);
    root_layout->set_layout_params(miqu::LayoutParams(
        static_cast<int>(miqu::LayoutDimension::MatchParent),
        static_cast<int>(miqu::LayoutDimension::MatchParent)
    ));

    int base_font_size = config->metrics.font_size > 0 ? config->metrics.font_size : 11;

    // 1. Header Row (App Icon + App Name + Count Badge + Close All Action)
    auto header = std::make_shared<miqu::LinearLayout>(miqu::Orientation::Horizontal);
    header->set_gravity(miqu::Gravity::CenterVertical);
    header->set_padding(4, 2, 4, 4);
    header->set_layout_params(miqu::LayoutParams(
        static_cast<int>(miqu::LayoutDimension::MatchParent),
        28
    ));
    header->set_divider_spacing(8);

    std::string icon_path = resolve_window_icon(windows.front());
    if (!icon_path.empty()) {
        auto icon_view = std::make_shared<miqu::ImageView>(icon_path);
        icon_view->set_target_size(20);
        icon_view->set_layout_params(miqu::LayoutParams(20, 20));
        header->add_view(icon_view);
    }

    std::string app_display = !windows.front().app_id.empty() ? windows.front().app_id : "Application";
    if (!app_display.empty() && app_display[0] >= 'a' && app_display[0] <= 'z') {
        app_display[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(app_display[0])));
    }

    auto title_lbl = miqu::TextViewBuilder::create()
        ->text(app_display)
        ->bold(true)
        ->textSize(base_font_size + 1)
        ->textColor(config->colors.on_surface)
        ->build();
    title_lbl->set_layout_params(miqu::LayoutParams(
        static_cast<int>(miqu::LayoutDimension::WrapContent),
        static_cast<int>(miqu::LayoutDimension::WrapContent)
    ));
    header->add_view(title_lbl);

    std::string count_str = std::to_string(windows.size()) + " Windows";
    auto count_btn = miqu::ButtonBuilder::create()
        ->text(count_str)
        ->textSize(base_font_size - 2)
        ->flat(true)
        ->padding(6, 2)
        ->customColors(
            config->colors.primary.with_alpha(0.15f),
            config->colors.primary
        )
        ->build();
    count_btn->set_layout_params(miqu::LayoutParams(
        static_cast<int>(miqu::LayoutDimension::WrapContent),
        22
    ));
    header->add_view(count_btn);

    auto spacer = std::make_shared<miqu::FrameLayout>();
    spacer->set_layout_params(miqu::LayoutParams(0, 0, 1.0f));
    header->add_view(spacer);

    auto close_all_btn = miqu::ButtonBuilder::create()
        ->text("Close All")
        ->textSize(base_font_size - 2)
        ->flat(true)
        ->padding(6, 2)
        ->customColors(
            miqu::Color(0, 0, 0, 0),
            config->colors.on_surface_variant.with_alpha(0.7f)
        )
        ->onClick([this, wins = windows]() mutable {
            hide_group_popup();
            for (auto& w : wins) {
                w.close();
            }
        })
        ->build();
    close_all_btn->set_layout_params(miqu::LayoutParams(
        static_cast<int>(miqu::LayoutDimension::WrapContent),
        22
    ));
    header->add_view(close_all_btn);

    root_layout->add_view(header);

    // Subtle divider line using miqutoolkit::DividerView
    auto divider = miqu::DividerViewBuilder::create()
        ->thickness(1)
        ->color(config->colors.outline_variant.with_alpha(0.35f))
        ->margin(0, 2, 0, 4)
        ->build();
    divider->set_layout_params(miqu::LayoutParams(
        static_cast<int>(miqu::LayoutDimension::MatchParent),
        1
    ));
    root_layout->add_view(divider);

    // 2. Table rows (WindowRowView)
    int row_idx = 1;
    for (const auto& win : windows) {
        auto row = std::make_shared<WindowRowView>(
            row_idx++,
            win,
            [this, w = win]() mutable {
                hide_group_popup();
                w.activate();
            },
            [this, w = win]() mutable {
                w.close();
                hide_group_popup();
            }
        );
        root_layout->add_view(row);
    }

    card->add_view(root_layout);

    int popup_w = std::clamp(cfg.hover_popup_max_width > 0 ? cfg.hover_popup_max_width : 340, 280, 420);
    int popup_h = 36 + 6 + static_cast<int>(windows.size()) * 38 + 16;

    miqu::Rect anchor_bounds = anchor->get_bounds();
    bool is_top = (cfg.position == "top");
    uint32_t anchors = (is_top ? 1 : 2) | 4;
    miqu::Margin margin;
    int win_w = anchor->get_window() ? anchor->get_window()->get_width() : 1920;
    int target_x = anchor_bounds.x + (anchor_bounds.width - popup_w) / 2;
    if (target_x < 8) target_x = 8;
    if (target_x + popup_w > win_w - 8) target_x = win_w - popup_w - 8;
    margin.left = target_x;

    if (is_top) {
        margin.top = cfg.height + 6;
    } else {
        margin.bottom = cfg.height + 6;
    }

    m_preview_window = miqu::WindowBuilder::create()
        ->role(miqu::WindowRole::LayerOverlay)
        ->layerNamespace("miqubar-taskbar-preview")
        ->anchors(anchors)
        ->preferredSize(popup_w, popup_h)
        ->contentSize(popup_w, popup_h)
        ->contentMargin(margin)
        ->transparent(true)
        ->exclusiveZone(-1)
        ->closeOnClickOutside(true)
        ->closeOnEscape(true)
        ->contentView(card)
        ->onClose([this]() {
            m_preview_window = nullptr;
            m_preview_anchor = nullptr;
            request_redraw();
        })
        ->build();

    if (m_preview_window) m_preview_window->show();
}

void TaskbarView::show_context_menu(const std::vector<miqu::WindowInfo>& win_group, miqu::View* anchor) {
    if (!anchor || win_group.empty()) return;
    hide_context_menu();
    hide_group_popup();

    auto config = miqu::Config::get();
    const auto& cfg = BarConfig::get();

    auto card = std::make_shared<miqu::CardView>();
    card->set_style(miqu::CardStyle::Elevated);
    card->set_radius(config->metrics.corner_radius > 0 ? config->metrics.corner_radius : 10);
    card->set_elevation(0);
    card->set_padding(4, 4);

    auto layout = std::make_shared<miqu::LinearLayout>(miqu::Orientation::Vertical);
    layout->set_divider_spacing(2);
    layout->set_layout_params(miqu::LayoutParams(
        static_cast<int>(miqu::LayoutDimension::MatchParent),
        static_cast<int>(miqu::LayoutDimension::MatchParent)
    ));

    int base_font_size = config->metrics.font_size > 0 ? config->metrics.font_size : 11;

    auto make_item = [&](const std::string& text, const std::string& icon, std::function<void()> on_click, bool destructive = false) {
        auto builder = miqu::ButtonBuilder::create()
            ->text(text)
            ->icon(icon)
            ->iconSize(16)
            ->flat(true)
            ->textSize(base_font_size)
            ->padding(8, 4)
            ->onClick([this, cb = std::move(on_click)]() {
                hide_context_menu();
                if (cb) cb();
            });
        if (destructive) {
            builder->customColors(miqu::Color(0, 0, 0, 0), miqu::Color(0.92f, 0.26f, 0.26f, 1.0f));
        }
        auto btn = builder->build();
        btn->set_layout_params(miqu::LayoutParams(
            static_cast<int>(miqu::LayoutDimension::MatchParent),
            30
        ));
        return btn;
    };

    if (win_group.size() == 1) {
        const auto& win = win_group.front();
        layout->add_view(make_item("Activate", "window-maximize", [w = win]() mutable {
            w.activate();
        }));

        std::string min_title = win.is_minimized ? "Restore" : "Minimize";
        layout->add_view(make_item(min_title, "window-minimize", [w = win]() mutable {
            w.set_minimized(!w.is_minimized);
        }));

        std::string max_title = win.is_maximized ? "Unmaximize" : "Maximize";
        layout->add_view(make_item(max_title, "view-fullscreen", [w = win]() mutable {
            w.set_maximized(!w.is_maximized);
        }));

        layout->add_view(make_item("Close", "window-close", [w = win]() mutable {
            w.close();
        }, true));
    } else {
        // Multi-window group actions
        layout->add_view(make_item("Activate Most Recent", "window-maximize", [wins = win_group]() mutable {
            if (!wins.empty()) wins.front().activate();
        }));

        layout->add_view(make_item("Minimize All", "window-minimize", [wins = win_group]() mutable {
            for (auto& w : wins) {
                w.set_minimized(true);
            }
        }));

        std::string close_all_title = "Close All (" + std::to_string(win_group.size()) + ")";
        layout->add_view(make_item(close_all_title, "window-close", [wins = win_group]() mutable {
            for (auto& w : wins) {
                w.close();
            }
        }, true));
    }

    card->add_view(layout);

    int menu_w = 160;
    int items_count = (win_group.size() == 1) ? 4 : 3;
    int menu_h = items_count * 32 + 10;

    miqu::Rect anchor_bounds = anchor->get_bounds();
    bool is_top = (cfg.position == "top");
    uint32_t anchors = (is_top ? 1 : 2) | 4;
    miqu::Margin margin;
    int win_w = anchor->get_window() ? anchor->get_window()->get_width() : 1920;
    int left_pos = anchor_bounds.x;
    if (left_pos + menu_w > win_w - 8) {
        left_pos = std::max(8, win_w - menu_w - 8);
    }
    margin.left = std::max(8, left_pos);

    if (is_top) {
        margin.top = cfg.height + 4;
    } else {
        margin.bottom = cfg.height + 4;
    }

    m_menu_window = miqu::WindowBuilder::create()
        ->role(miqu::WindowRole::LayerOverlay)
        ->layerNamespace("miqubar-taskbar-menu")
        ->anchors(anchors)
        ->preferredSize(menu_w, menu_h)
        ->contentSize(menu_w, menu_h)
        ->contentMargin(margin)
        ->keyboardInteractive(true)
        ->transparent(true)
        ->exclusiveZone(-1)
        ->closeOnClickOutside(true)
        ->closeOnEscape(true)
        ->contentView(card)
        ->onClose([this]() {
            m_menu_window = nullptr;
            request_redraw();
        })
        ->build();

    if (m_menu_window) m_menu_window->show();
}

std::string TaskbarView::resolve_window_icon(const miqu::WindowInfo& win) {
    if (win.app_id.empty()) return "";

    auto it = m_icon_path_cache.find(win.app_id);
    if (it != m_icon_path_cache.end()) {
        return it->second;
    }

    std::string path = miqu::ImageView::resolve_icon_path(win.app_id);

    if (path.empty()) {
        auto* app_mgr = miqu::AppManager::get();
        if (app_mgr) {
            const auto& apps = app_mgr->get_installed_apps();
            for (const auto& app : apps) {
                if (app.id == win.app_id || app.id == (win.app_id + ".desktop") ||
                    app.name == win.app_id) {
                    if (!app.icon_path.empty()) {
                        path = app.icon_path;
                        break;
                    } else if (!app.icon_name.empty()) {
                        path = miqu::ImageView::resolve_icon_path(app.icon_name);
                        if (!path.empty()) break;
                    }
                }
            }
        }
    }

    m_icon_path_cache[win.app_id] = path;
    return path;
}

} // namespace miqubar
