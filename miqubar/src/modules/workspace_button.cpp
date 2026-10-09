#include "workspace_button.hpp"
#include "../config/bar_config.hpp"
#include "miqutoolkit/core/window.hpp"
#include <pango/pangocairo.h>
#include <cmath>
#include <iostream>

namespace miqubar {

// Interactive floating flyout view for workspaces
class WorkspaceFlyoutView : public miqu::View {
public:
    WorkspaceFlyoutView(std::vector<miqu::WorkspaceInfo> workspaces,
                        size_t active_id,
                        std::function<void(size_t)> on_select)
        : m_workspaces(std::move(workspaces)),
          m_active_id(active_id),
          m_on_select(std::move(on_select)) {}

    void draw(cairo_t* cr, const miqu::Rect& bounds) override {
        if (!cr) return;
        const auto& cfg = BarConfig::get();

        // Flyout backdrop
        double r = cfg.corner_radius + 2;
        cairo_new_sub_path(cr);
        cairo_arc(cr, bounds.x + bounds.width - r, bounds.y + r, r, -M_PI / 2, 0);
        cairo_arc(cr, bounds.x + bounds.width - r, bounds.y + bounds.height - r, r, 0, M_PI / 2);
        cairo_arc(cr, bounds.x + r, bounds.y + bounds.height - r, r, M_PI / 2, M_PI);
        cairo_arc(cr, bounds.x + r, bounds.y + r, r, M_PI, 3 * M_PI / 2);
        cairo_close_path(cr);

        cairo_set_source_rgba(cr, 0.08, 0.10, 0.15, 0.98);
        cairo_fill_preserve(cr);
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.12);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);

        // Draw each workspace pill
        int pill_w = 34;
        int pill_h = 34;
        int gap = 6;
        int start_x = bounds.x + 8;
        int start_y = bounds.y + 8;

        for (size_t i = 0; i < m_workspaces.size(); ++i) {
            const auto& ws = m_workspaces[i];
            int px = start_x + static_cast<int>(i) * (pill_w + gap);
            int py = start_y;
            bool is_active = (ws.id == m_active_id);
            bool is_hovered = (static_cast<int>(i) == m_hovered_index);

            // Pill background
            cairo_new_sub_path(cr);
            double pr = 6.0;
            cairo_arc(cr, px + pill_w - pr, py + pr, pr, -M_PI / 2, 0);
            cairo_arc(cr, px + pill_w - pr, py + pill_h - pr, pr, 0, M_PI / 2);
            cairo_arc(cr, px + pr, py + pill_h - pr, pr, M_PI / 2, M_PI);
            cairo_arc(cr, px + pr, py + pr, pr, M_PI, 3 * M_PI / 2);
            cairo_close_path(cr);

            if (is_active) {
                cairo_set_source_rgba(cr, 0.65, 0.33, 0.97, 0.35); // #a855f7
                cairo_fill_preserve(cr);
                cairo_set_source_rgb(cr, 0.65, 0.33, 0.97);
                cairo_set_line_width(cr, 1.5);
                cairo_stroke(cr);
            } else if (is_hovered) {
                cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.14);
                cairo_fill(cr);
            } else {
                cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.05);
                cairo_fill(cr);
            }

            // Draw workspace number
            std::string num = std::to_string(ws.id);
            PangoLayout* layout = pango_cairo_create_layout(cr);
            PangoFontDescription* desc = pango_font_description_from_string((cfg.font_family + " Bold 11").c_str());
            pango_layout_set_font_description(layout, desc);
            pango_layout_set_text(layout, num.c_str(), -1);

            int lw, lh;
            pango_layout_get_pixel_size(layout, &lw, &lh);
            cairo_move_to(cr, px + (pill_w - lw) / 2.0, py + (pill_h - lh) / 2.0);
            if (is_active) {
                cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
            } else {
                cairo_set_source_rgba(cr, 0.85, 0.90, 0.95, 0.8);
            }
            pango_cairo_show_layout(cr, layout);

            pango_font_description_free(desc);
            g_object_unref(layout);
        }
    }

    bool on_mouse_button(int lx, int ly, miqu::MouseButton button, bool pressed, const miqu::Rect& bounds) override {
        if (button != miqu::MouseButton::Left || !pressed) return false;

        int pill_w = 34;
        int gap = 6;
        int start_x = bounds.x + 8;
        int rel_x = lx - start_x;
        if (rel_x >= 0 && ly >= bounds.y + 8 && ly <= bounds.y + 42) {
            int idx = rel_x / (pill_w + gap);
            if (idx >= 0 && static_cast<size_t>(idx) < m_workspaces.size()) {
                if (m_on_select) {
                    m_on_select(m_workspaces[idx].id);
                }
                return true;
            }
        }
        return false;
    }

    bool on_mouse_move(int lx, int, const miqu::Rect& bounds) override {
        int pill_w = 34;
        int gap = 6;
        int start_x = bounds.x + 8;
        int rel_x = lx - start_x;
        int idx = (rel_x >= 0) ? (rel_x / (pill_w + gap)) : -1;
        if (idx != m_hovered_index) {
            m_hovered_index = idx;
            request_redraw();
        }
        return true;
    }

private:
    std::vector<miqu::WorkspaceInfo> m_workspaces;
    size_t m_active_id = 1;
    int m_hovered_index = -1;
    std::function<void(size_t)> m_on_select;
};

WorkspaceButtonView::WorkspaceButtonView() {
    sync_workspaces();
    auto mgr = miqu::WorkspaceManager::get();
    if (mgr) {
        mgr->on_workspaces_changed([this]() {
            sync_workspaces();
            request_redraw();
        });
    }
}

WorkspaceButtonView::~WorkspaceButtonView() {
    hide_flyout();
}

void WorkspaceButtonView::sync_workspaces() {
    auto mgr = miqu::WorkspaceManager::get();
    if (mgr) {
        m_workspaces = mgr->get_workspaces();
        const auto* act = mgr->get_active_workspace();
        if (act) {
            m_active_id = act->id;
        }
    }
    if (m_workspaces.empty()) {
        miqu::WorkspaceInfo fallback;
        fallback.id = 1;
        fallback.name = "1";
        fallback.is_active = true;
        m_workspaces.push_back(fallback);
    }
}

miqu::Size WorkspaceButtonView::measure_size() const {
    const auto& cfg = BarConfig::get();
    // Compact button with room for icon + badge
    return {cfg.height + 8, cfg.height};
}

void WorkspaceButtonView::draw(cairo_t* cr, const miqu::Rect& bounds) {
    if (!cr) return;

    const auto& cfg = BarConfig::get();
    int pad = 4;
    double x = bounds.x + pad;
    double y = bounds.y + pad;
    double w = bounds.width - (pad * 2);
    double h = bounds.height - (pad * 2);
    double r = cfg.corner_radius;

    // Button Background
    if (m_pressed || (m_flyout_window != nullptr)) {
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.16);
    } else if (m_hovered) {
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.08);
    } else {
        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
    }

    if (m_hovered || m_pressed || m_flyout_window) {
        cairo_new_sub_path(cr);
        cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2, 0);
        cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI / 2);
        cairo_arc(cr, x + r, y + h - r, r, M_PI / 2, M_PI);
        cairo_arc(cr, x + r, y + r, r, M_PI, 3 * M_PI / 2);
        cairo_close_path(cr);
        cairo_fill(cr);
    }

    // Draw Workspace Icon (2 stacked rectangles)
    double icon_x = bounds.x + 8;
    double icon_y = bounds.y + (bounds.height - 18) / 2.0;

    cairo_set_line_width(cr, 1.5);
    cairo_set_source_rgba(cr, 0.85, 0.90, 0.96, 0.85);

    // Front workspace screen
    cairo_rectangle(cr, icon_x, icon_y + 3, 14, 11);
    cairo_stroke(cr);
    // Back layered screen
    cairo_move_to(cr, icon_x + 3, icon_y + 3);
    cairo_line_to(cr, icon_x + 3, icon_y);
    cairo_line_to(cr, icon_x + 17, icon_y);
    cairo_line_to(cr, icon_x + 17, icon_y + 8);
    cairo_line_to(cr, icon_x + 14, icon_y + 8);
    cairo_stroke(cr);

    // Draw Active Workspace Badge pill on the right
    double badge_x = icon_x + 20;
    double badge_y = bounds.y + (bounds.height - 18) / 2.0;
    double badge_w = 18;
    double badge_h = 18;
    double br = 5;

    cairo_new_sub_path(cr);
    cairo_arc(cr, badge_x + badge_w - br, badge_y + br, br, -M_PI / 2, 0);
    cairo_arc(cr, badge_x + badge_w - br, badge_y + badge_h - br, br, 0, M_PI / 2);
    cairo_arc(cr, badge_x + br, badge_y + badge_h - br, br, M_PI / 2, M_PI);
    cairo_arc(cr, badge_x + br, badge_y + br, br, M_PI, 3 * M_PI / 2);
    cairo_close_path(cr);

    // Badge background in violet/purple accent
    cairo_set_source_rgba(cr, 0.65, 0.33, 0.97, 0.85); // #a855f7
    cairo_fill(cr);

    // Badge text: active workspace number
    std::string badge_num = std::to_string(m_active_id);
    PangoLayout* layout = pango_cairo_create_layout(cr);
    PangoFontDescription* desc = pango_font_description_from_string((cfg.font_family + " Bold 10").c_str());
    pango_layout_set_font_description(layout, desc);
    pango_layout_set_text(layout, badge_num.c_str(), -1);

    int lw, lh;
    pango_layout_get_pixel_size(layout, &lw, &lh);
    cairo_move_to(cr, badge_x + (badge_w - lw) / 2.0, badge_y + (badge_h - lh) / 2.0);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    pango_cairo_show_layout(cr, layout);

    pango_font_description_free(desc);
    g_object_unref(layout);
}

bool WorkspaceButtonView::on_mouse_move(int lx, int ly, const miqu::Rect& bounds) {
    bool hov = bounds.contains(lx, ly);
    if (hov != m_hovered) {
        m_hovered = hov;
        request_redraw();
        if (m_hovered && BarConfig::get().hover_flyout && !m_flyout_window) {
            show_flyout();
        }
    }
    return hov;
}

bool WorkspaceButtonView::on_mouse_button(int, int, miqu::MouseButton button, bool pressed, const miqu::Rect&) {
    if (button != miqu::MouseButton::Left) return false;

    if (pressed) {
        m_pressed = true;
        request_redraw();
        return true;
    } else {
        if (m_pressed) {
            m_pressed = false;
            if (m_flyout_window) {
                hide_flyout();
            } else {
                show_flyout();
            }
            request_redraw();
            return true;
        }
    }
    return false;
}

bool WorkspaceButtonView::on_scroll(double delta) {
    if (!BarConfig::get().scroll_switch) return false;
    if (std::abs(delta) > 0.05) {
        cycle_workspace(delta > 0 ? -1 : 1);
        return true;
    }
    return false;
}

void WorkspaceButtonView::cycle_workspace(int delta) {
    if (m_workspaces.empty()) return;

    size_t cur_idx = 0;
    for (size_t i = 0; i < m_workspaces.size(); ++i) {
        if (m_workspaces[i].id == m_active_id) {
            cur_idx = i;
            break;
        }
    }

    int next_idx = static_cast<int>(cur_idx) + delta;
    if (next_idx < 0) next_idx = static_cast<int>(m_workspaces.size()) - 1;
    if (static_cast<size_t>(next_idx) >= m_workspaces.size()) next_idx = 0;

    auto mgr = miqu::WorkspaceManager::get();
    if (mgr) {
        mgr->activate_workspace(m_workspaces[next_idx].id);
    }
    m_active_id = m_workspaces[next_idx].id;
    request_redraw();
}

void WorkspaceButtonView::show_flyout() {
    sync_workspaces();
    int count = static_cast<int>(m_workspaces.size());
    int flyout_w = std::max(120, 16 + count * 40);
    int flyout_h = 50;

    auto flyout_content = std::make_shared<WorkspaceFlyoutView>(
        m_workspaces,
        m_active_id,
        [this](size_t id) {
            auto mgr = miqu::WorkspaceManager::get();
            if (mgr) mgr->activate_workspace(id);
            m_active_id = id;
            hide_flyout();
            request_redraw();
        }
    );

    uint32_t anchors = 2 | 4; // Bottom (2) | Left (4)
    if (BarConfig::get().position == "top") {
        anchors = 1 | 4; // Top (1) | Left (4)
    }

    m_flyout_window = miqu::WindowBuilder::create()
        ->role(miqu::WindowRole::LayerOverlay)
        ->layerNamespace("miqubar-workspaces")
        ->contentSize(flyout_w, flyout_h)
        ->anchors(anchors)
        ->exclusiveZone(-1)
        ->closeOnClickOutside(true)
        ->closeOnEscape(true)
        ->contentView(flyout_content)
        ->onClose([this]() {
            m_flyout_window = nullptr;
            request_redraw();
        })
        ->build();

    if (m_flyout_window) {
        m_flyout_window->show();
    }
}

void WorkspaceButtonView::hide_flyout() {
    if (m_flyout_window) {
        m_flyout_window->close();
        m_flyout_window = nullptr;
    }
}

} // namespace miqubar
