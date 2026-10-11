#include "taskbar.hpp"
#include "../config/bar_config.hpp"
#include "miqutoolkit/core/config.hpp"
#include "miqutoolkit/view/button.hpp"
#include "miqutoolkit/view/card_view.hpp"
#include "miqutoolkit/view/image_view.hpp"
#include "miqutoolkit/view/popup_menu.hpp"
#include "miqutoolkit/core/window.hpp"
#include "miqutoolkit/system/app_manager.hpp"
#include <algorithm>
#include <iostream>

namespace miqubar {

class TaskbarItemButton : public miqu::Button {
public:
    TaskbarItemButton(miqu::WindowInfo win,
                      std::string icon_path,
                      std::function<void(const miqu::WindowInfo&, miqu::View*)> on_context_menu)
        : m_win(std::move(win)),
          m_on_context_menu(std::move(on_context_menu)) {
        
        const auto& cfg = BarConfig::get();
        auto config = miqu::Config::get();
        int fs = config->metrics.font_size > 0 ? config->metrics.font_size : 11;
        if (cfg.icon_only) {
            set_text("");
        } else {
            set_text(m_win.title);
            set_text_size(fs);
        }

        if (!icon_path.empty()) {
            set_icon(icon_path);
        } else if (!m_win.app_id.empty()) {
            set_icon(m_win.app_id);
        }

        int isz = cfg.icon_size > 0 ? cfg.icon_size : 24;
        set_icon_size(isz);
        set_padding(2, 2);

        if (m_win.is_active) {
            set_selected(true);
        } else {
            set_flat(true);
        }

        set_on_click_listener([this]() {
            if (m_win.is_active) {
                m_win.set_minimized(true);
            } else {
                m_win.activate();
            }
        });
    }

    bool on_mouse_button(int lx, int ly, miqu::MouseButton button, bool pressed, const miqu::Rect& bounds) override {
        if (button == miqu::MouseButton::Right && pressed) {
            if (m_on_context_menu) {
                m_on_context_menu(m_win, this);
            }
            return true;
        }
        if (button == miqu::MouseButton::Middle && pressed) {
            m_win.close();
            return true;
        }
        return miqu::Button::on_mouse_button(lx, ly, button, pressed, bounds);
    }

private:
    miqu::WindowInfo m_win;
    std::function<void(const miqu::WindowInfo&, miqu::View*)> m_on_context_menu;
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
}

void TaskbarView::hide_context_menu() {
    if (m_menu_window) {
        auto win = m_menu_window;
        m_menu_window = nullptr;
        win->close();
    }
}

void TaskbarView::sync_windows() {
    clear_views();
    auto mgr = miqu::WindowManager::get();
    if (mgr) {
        m_windows = mgr->get_windows();
    }

    const auto& cfg = BarConfig::get();
    int item_h = cfg.height - 4;
    int item_w = cfg.icon_only ? item_h : cfg.item_max_width;

    for (const auto& win : m_windows) {
        std::string icon = resolve_window_icon(win);
        auto btn = std::make_shared<TaskbarItemButton>(
            win,
            icon,
            [this](const miqu::WindowInfo& w, miqu::View* anchor) {
                show_context_menu(w, anchor);
            }
        );
        btn->set_layout_params(miqu::LayoutParams(item_w, item_h));
        add_view(btn);
    }
    request_redraw();
}

void TaskbarView::show_context_menu(const miqu::WindowInfo& win, miqu::View* anchor) {
    if (!anchor) return;
    hide_context_menu();

    auto config = miqu::Config::get();
    const auto& cfg = BarConfig::get();

    auto card = std::make_shared<miqu::CardView>();
    card->set_style(miqu::CardStyle::Elevated);
    card->set_radius(config->metrics.corner_radius > 0 ? config->metrics.corner_radius : 10);
    card->set_elevation(6);
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

    card->add_view(layout);

    int menu_w = 150;
    int menu_h = 4 * 32 + 10;

    miqu::Rect anchor_bounds = anchor->get_bounds();
    bool is_top = (cfg.position == "top");
    miqu::Gravity gravity = is_top ? (miqu::Gravity::Top | miqu::Gravity::Left)
                                   : (miqu::Gravity::Bottom | miqu::Gravity::Left);
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
        ->contentSize(menu_w, menu_h)
        ->contentGravity(gravity)
        ->contentMargin(margin)
        ->dimBackdrop(false)
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
