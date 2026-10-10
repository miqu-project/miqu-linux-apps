#include "taskbar.hpp"
#include "../config/bar_config.hpp"
#include "miqutoolkit/core/config.hpp"
#include "miqutoolkit/view/button.hpp"
#include "miqutoolkit/view/image_view.hpp"
#include "miqutoolkit/view/popup_menu.hpp"
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

        set_padding(6, 2);

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

void TaskbarView::sync_windows() {
    clear_views();
    auto mgr = miqu::WindowManager::get();
    if (mgr) {
        m_windows = mgr->get_windows();
    }

    const auto& cfg = BarConfig::get();
    int item_h = cfg.height - 8;
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
    auto menu = std::make_shared<miqu::PopupMenu>();

    menu->add_item("Activate", "window-maximize", [w = win]() mutable {
        w.activate();
    });

    std::string min_title = win.is_minimized ? "Restore" : "Minimize";
    menu->add_item(min_title, "window-minimize", [w = win]() mutable {
        w.set_minimized(!w.is_minimized);
    });

    std::string max_title = win.is_maximized ? "Unmaximize" : "Maximize";
    menu->add_item(max_title, "view-fullscreen", [w = win]() mutable {
        w.set_maximized(!w.is_maximized);
    });

    menu->add_separator();

    menu->add_destructive_item("Close", "window-close", [w = win]() mutable {
        w.close();
    });

    const auto& cfg = BarConfig::get();
    miqu::PopupGravity grav = (cfg.position == "top") ? miqu::PopupGravity::BottomStart : miqu::PopupGravity::TopStart;
    menu->show_as_dropdown(anchor, grav);
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
