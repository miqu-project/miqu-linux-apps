#include "taskbar.hpp"
#include "../config/bar_config.hpp"
#include "miqutoolkit/core/window.hpp"
#include <pango/pangocairo.h>
#include <filesystem>
#include <cmath>
#include <iostream>

namespace miqubar {

namespace fs = std::filesystem;

// Context menu view for running window
class WindowContextMenuView : public miqu::View {
public:
    WindowContextMenuView(miqu::WindowInfo win, std::function<void()> on_close)
        : m_win(win), m_on_close(std::move(on_close)) {}

    void draw(cairo_t* cr, const miqu::Rect& bounds) override {
        if (!cr) return;
        const auto& cfg = BarConfig::get();

        // Menu backdrop
        double r = cfg.corner_radius;
        cairo_new_sub_path(cr);
        cairo_arc(cr, bounds.x + bounds.width - r, bounds.y + r, r, -M_PI / 2, 0);
        cairo_arc(cr, bounds.x + bounds.width - r, bounds.y + bounds.height - r, r, 0, M_PI / 2);
        cairo_arc(cr, bounds.x + r, bounds.y + bounds.height - r, r, M_PI / 2, M_PI);
        cairo_arc(cr, bounds.x + r, bounds.y + r, r, M_PI, 3 * M_PI / 2);
        cairo_close_path(cr);

        cairo_set_source_rgba(cr, 0.09, 0.11, 0.17, 0.98);
        cairo_fill_preserve(cr);
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.12);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);

        std::vector<std::string> items = {"Restore / Focus", "Minimize", "Maximize", "Close Window"};
        int item_h = 28;
        int y = bounds.y + 6;

        for (size_t i = 0; i < items.size(); ++i) {
            if (static_cast<int>(i) == m_hovered_idx) {
                cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.10);
                cairo_rectangle(cr, bounds.x + 4, y, bounds.width - 8, item_h);
                cairo_fill(cr);
            }

            PangoLayout* layout = pango_cairo_create_layout(cr);
            PangoFontDescription* desc = pango_font_description_from_string((cfg.font_family + " 10").c_str());
            pango_layout_set_font_description(layout, desc);
            pango_layout_set_text(layout, items[i].c_str(), -1);

            cairo_move_to(cr, bounds.x + 14, y + 6);
            if (i == 3) {
                cairo_set_source_rgb(cr, 1.0, 0.35, 0.45); // Close item in red
            } else {
                cairo_set_source_rgba(cr, 0.9, 0.93, 0.97, 0.9);
            }
            pango_cairo_show_layout(cr, layout);

            pango_font_description_free(desc);
            g_object_unref(layout);

            y += item_h;
        }
    }

    bool on_mouse_button(int, int ly, miqu::MouseButton button, bool pressed, const miqu::Rect& bounds) override {
        if (button != miqu::MouseButton::Left || !pressed) return false;

        int item_h = 28;
        int idx = (ly - (bounds.y + 6)) / item_h;
        if (idx == 0) m_win.activate();
        else if (idx == 1) m_win.set_minimized(true);
        else if (idx == 2) m_win.set_maximized(!m_win.is_maximized);
        else if (idx == 3) m_win.close();

        if (m_on_close) m_on_close();
        return true;
    }

    bool on_mouse_move(int, int ly, const miqu::Rect& bounds) override {
        int item_h = 28;
        int idx = (ly - (bounds.y + 6)) / item_h;
        if (idx < 0 || idx >= 4) idx = -1;
        if (idx != m_hovered_idx) {
            m_hovered_idx = idx;
            request_redraw();
        }
        return true;
    }

private:
    miqu::WindowInfo m_win;
    int m_hovered_idx = -1;
    std::function<void()> m_on_close;
};

TaskbarView::TaskbarView() {
    sync_windows();
    auto mgr = miqu::WindowManager::get();
    if (mgr) {
        mgr->on_windows_changed([this]() {
            sync_windows();
            request_redraw();
        });
    }
}

TaskbarView::~TaskbarView() {
    if (m_context_menu_window) {
        m_context_menu_window->close();
        m_context_menu_window = nullptr;
    }
}

void TaskbarView::sync_windows() {
    auto mgr = miqu::WindowManager::get();
    if (mgr) {
        m_windows = mgr->get_windows();
    }
}

miqu::Size TaskbarView::measure_size() const {
    const auto& cfg = BarConfig::get();
    int total_w = static_cast<int>(m_windows.size()) * cfg.item_max_width;
    return {total_w, cfg.height};
}

void TaskbarView::draw(cairo_t* cr, const miqu::Rect& bounds) {
    if (!cr || m_windows.empty()) return;

    const auto& cfg = BarConfig::get();
    int tab_w = std::min(cfg.item_max_width, static_cast<int>(bounds.width / std::max(1UL, m_windows.size())));
    int tab_h = bounds.height - 8;
    int cur_x = bounds.x + 4;
    int cur_y = bounds.y + 4;

    for (size_t i = 0; i < m_windows.size(); ++i) {
        const auto& win = m_windows[i];
        bool is_active = win.is_active;
        bool is_hovered = (static_cast<int>(i) == m_hovered_index);
        bool is_pressed = (static_cast<int>(i) == m_pressed_index);

        double r = cfg.corner_radius;

        // Tab background
        cairo_new_sub_path(cr);
        cairo_arc(cr, cur_x + tab_w - r, cur_y + r, r, -M_PI / 2, 0);
        cairo_arc(cr, cur_x + tab_w - r, cur_y + tab_h - r, r, 0, M_PI / 2);
        cairo_arc(cr, cur_x + r, cur_y + tab_h - r, r, M_PI / 2, M_PI);
        cairo_arc(cr, cur_x + r, cur_y + r, r, M_PI, 3 * M_PI / 2);
        cairo_close_path(cr);

        if (is_active) {
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.12);
            cairo_fill_preserve(cr);
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.16);
            cairo_set_line_width(cr, 1.0);
            cairo_stroke(cr);
        } else if (is_pressed) {
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.14);
            cairo_fill(cr);
        } else if (is_hovered) {
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.08);
            cairo_fill(cr);
        }

        // Active indicator line at bottom (Windows style)
        if (is_active && cfg.show_active_underline) {
            double bar_margin = 12.0;
            cairo_set_source_rgb(cr, 0.0, 0.90, 1.0); // Bright cyan #00e5ff
            cairo_set_line_width(cr, 2.5);
            cairo_move_to(cr, cur_x + bar_margin, cur_y + tab_h - 1.0);
            cairo_line_to(cr, cur_x + tab_w - bar_margin, cur_y + tab_h - 1.0);
            cairo_stroke(cr);
        } else if (!is_active && !win.is_minimized) {
            // Inactive open window has subtle small dot
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.4);
            cairo_arc(cr, cur_x + tab_w / 2.0, cur_y + tab_h - 2.0, 1.5, 0, 2 * M_PI);
            cairo_fill(cr);
        }

        // Draw App Icon
        double icon_cx = cur_x + 14;
        double icon_cy = cur_y + tab_h / 2.0;
        cairo_arc(cr, icon_cx, icon_cy, 6.0, 0, 2 * M_PI);
        if (is_active) {
            cairo_set_source_rgb(cr, 0.0, 0.90, 1.0);
        } else {
            cairo_set_source_rgba(cr, 0.65, 0.33, 0.97, 0.8);
        }
        cairo_fill(cr);

        // Draw Title text (if not icon_only)
        if (!cfg.icon_only) {
            std::string title = win.title.empty() ? win.app_id : win.title;
            if (static_cast<int>(title.size()) > cfg.max_title_chars) {
                title = title.substr(0, cfg.max_title_chars) + "...";
            }

            PangoLayout* layout = pango_cairo_create_layout(cr);
            PangoFontDescription* desc = pango_font_description_from_string((cfg.font_family + " 10").c_str());
            pango_layout_set_font_description(layout, desc);
            pango_layout_set_text(layout, title.c_str(), -1);

            int lw, lh;
            pango_layout_get_pixel_size(layout, &lw, &lh);
            cairo_move_to(cr, cur_x + 26, cur_y + (tab_h - lh) / 2.0);
            if (is_active) {
                cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
            } else {
                cairo_set_source_rgba(cr, 0.85, 0.90, 0.96, 0.85);
            }
            pango_cairo_show_layout(cr, layout);

            pango_font_description_free(desc);
            g_object_unref(layout);
        }

        cur_x += tab_w + 3;
    }
}

bool TaskbarView::on_mouse_move(int lx, int, const miqu::Rect& bounds) {
    if (m_windows.empty()) return false;
    const auto& cfg = BarConfig::get();
    int tab_w = std::min(cfg.item_max_width, static_cast<int>(bounds.width / std::max(1UL, m_windows.size())));
    int rel_x = lx - (bounds.x + 4);
    int idx = (rel_x >= 0) ? (rel_x / (tab_w + 3)) : -1;
    if (idx < 0 || static_cast<size_t>(idx) >= m_windows.size()) idx = -1;

    if (idx != m_hovered_index) {
        m_hovered_index = idx;
        request_redraw();
    }
    return (m_hovered_index >= 0);
}

bool TaskbarView::on_mouse_button(int lx, int, miqu::MouseButton button, bool pressed, const miqu::Rect& bounds) {
    if (m_windows.empty()) return false;

    const auto& cfg = BarConfig::get();
    int tab_w = std::min(cfg.item_max_width, static_cast<int>(bounds.width / std::max(1UL, m_windows.size())));
    int rel_x = lx - (bounds.x + 4);
    if (rel_x < 0) return false;

    int idx = rel_x / (tab_w + 3);
    if (idx < 0 || static_cast<size_t>(idx) >= m_windows.size()) return false;

    auto& win = m_windows[idx];

    if (button == miqu::MouseButton::Left) {
        if (pressed) {
            m_pressed_index = idx;
            request_redraw();
            return true;
        } else {
            if (m_pressed_index == idx) {
                m_pressed_index = -1;
                if (win.is_active) {
                    win.set_minimized(true);
                } else {
                    win.activate();
                }
                sync_windows();
                request_redraw();
                return true;
            }
        }
    } else if (button == miqu::MouseButton::Middle && pressed) {
        if (cfg.middle_click_close) {
            win.close();
            sync_windows();
            request_redraw();
            return true;
        }
    } else if (button == miqu::MouseButton::Right && pressed) {
        show_context_menu(win, lx, bounds.y);
        return true;
    }

    return false;
}

void TaskbarView::show_context_menu(const miqu::WindowInfo& win, int x, int) {
    if (m_context_menu_window) {
        m_context_menu_window->close();
        m_context_menu_window = nullptr;
    }

    auto menu_content = std::make_shared<WindowContextMenuView>(win, [this]() {
        if (m_context_menu_window) {
            m_context_menu_window->close();
            m_context_menu_window = nullptr;
            request_redraw();
        }
    });

    uint32_t anchors = 2 | 4; // Bottom | Left
    if (BarConfig::get().position == "top") {
        anchors = 1 | 4; // Top | Left
    }

    m_context_menu_window = miqu::WindowBuilder::create()
        ->role(miqu::WindowRole::LayerOverlay)
        ->layerNamespace("miqubar-menu")
        ->contentSize(160, 124)
        ->anchors(anchors)
        ->exclusiveZone(-1)
        ->closeOnClickOutside(true)
        ->closeOnEscape(true)
        ->contentView(menu_content)
        ->onClose([this]() {
            m_context_menu_window = nullptr;
            request_redraw();
        })
        ->build();

    if (m_context_menu_window) {
        m_context_menu_window->show();
    }
}

} // namespace miqubar
