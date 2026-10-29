#include "overview_card.hpp"
#include "overview_config.hpp"
#include "core/view.hpp"
#include <algorithm>
#include <cmath>
#include <pango/pangocairo.h>

namespace miquoverview {

static void draw_rounded_rectangle(cairo_t* cr, double x, double y, double w, double h, double r) {
    if (r <= 0.0) {
        cairo_rectangle(cr, x, y, w, h);
        return;
    }
    r = std::min(r, std::min(w / 2.0, h / 2.0));
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2.0, 0.0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0.0, M_PI / 2.0);
    cairo_arc(cr, x + r, y + h - r, r, M_PI / 2.0, M_PI);
    cairo_arc(cr, x + r, y + r, r, M_PI, 3.0 * M_PI / 2.0);
    cairo_close_path(cr);
}

void WindowThumbnail::update_highlight(struct wlr_scene_tree* parent, const OverviewTheme& theme) {
    if (!parent) return;
    if (!is_hovered && !is_selected) {
        if (highlight_scene_buf) {
            wlr_scene_node_set_enabled(&highlight_scene_buf->node, false);
        }
        return;
    }

    int bw = std::max(1, box.width);
    int bh = std::max(1, box.height);

    if (!highlight_buf || highlight_buf->get_width() != bw || highlight_buf->get_height() != bh) {
        if (highlight_scene_buf) {
            wlr_scene_node_destroy(&highlight_scene_buf->node);
            highlight_scene_buf = nullptr;
        }
        if (highlight_buf) {
            highlight_buf->drop();
            highlight_buf = nullptr;
        }
        highlight_buf = OverviewCairoBuffer::create(bw, bh);
    }
    if (!highlight_buf) return;

    highlight_buf->clear();
    cairo_t* cr = highlight_buf->get_cr();

    double th = 2.0;
    double half_th = th / 2.0;
    double draw_w = std::max(1.0, (double)bw - th);
    double draw_h = std::max(1.0, (double)bh - th);
    double draw_r = std::max(0.0, (double)corner_radius - half_th);

    draw_rounded_rectangle(cr, half_th, half_th, draw_w, draw_h, draw_r);
    cairo_set_source_rgba(cr, theme.primary.r, theme.primary.g, theme.primary.b, theme.primary.a);
    cairo_set_line_width(cr, th);
    cairo_stroke(cr);

    if (!highlight_scene_buf) {
        highlight_scene_buf = wlr_scene_buffer_create(parent, highlight_buf->get_wlr_buffer());
    } else {
        wlr_scene_buffer_set_buffer(highlight_scene_buf, highlight_buf->get_wlr_buffer());
        wlr_scene_node_set_enabled(&highlight_scene_buf->node, true);
    }
    wlr_scene_node_set_position(&highlight_scene_buf->node, box.x, box.y);
}

void WindowThumbnail::destroy_highlight() {
    if (highlight_scene_buf) {
        wlr_scene_node_destroy(&highlight_scene_buf->node);
        highlight_scene_buf = nullptr;
    }
    if (highlight_buf) {
        highlight_buf->drop();
        highlight_buf = nullptr;
    }
}

void WorkspaceCard::create_decorations(struct wlr_scene_tree* bg_tree, struct wlr_scene_tree* overlay_tree,
                                       const OverviewTheme& theme, size_t orig_active_ws) {
    destroy_nodes();

    int r = OverviewConfig::get().corner_radius();

    // 1. Workspace card drop shadow
    if (OverviewConfig::get().shadow()) {
        float s_color[4];
        const auto& sc = OverviewConfig::get().shadow_color();
        make_premul_color(s_color, sc.r, sc.g, sc.b, sc.a);

        float sigma = OverviewConfig::get().shadow_blur();
        int radius = (r > 0) ? r : OverviewConfig::get().shadow_radius();
        int pad = static_cast<int>(std::ceil(std::max(4.0f, sigma * 2.5f)));

        shadow = wlr_scene_shadow_create(bg_tree, card_box.width + 2 * pad, card_box.height + 2 * pad,
                                         radius, sigma, s_color);
        if (shadow) {
            wlr_scene_node_set_position(&shadow->node, card_box.x - pad, card_box.y - pad);
            struct clipped_region cr = clipped_region_get_default();
            cr.area.x = pad;
            cr.area.y = pad;
            cr.area.width = card_box.width;
            cr.area.height = card_box.height;
            cr.corners = corner_radii_all(radius);
            wlr_scene_shadow_set_clipped_region(shadow, cr);
        }
    }

    // 2. Workspace background canvas with subtle corner rounding
    float bg_color[4];
    make_premul_color(bg_color, theme.surface.r, theme.surface.g, theme.surface.b, theme.surface.a);
    bg_rect = wlr_scene_rect_create(bg_tree, card_box.width, card_box.height, bg_color);
    wlr_scene_node_set_position(&bg_rect->node, card_box.x, card_box.y);
    if (r > 0 && bg_rect) {
        wlr_scene_rect_set_corner_radius(bg_rect, r);
    }

    // 3. Workspace card border (only shown when active or hovered)
    is_active = (ws_id == orig_active_ws);
    update_border(overlay_tree, theme);
}

void WorkspaceCard::update_border(struct wlr_scene_tree* overlay_tree, const OverviewTheme& theme) {
    if (!overlay_tree) return;
    bool show_border = (is_active || is_hovered);
    if (!show_border) {
        if (border_scene_buf) {
            wlr_scene_node_set_enabled(&border_scene_buf->node, false);
        }
        return;
    }

    int bw = std::max(1, card_box.width);
    int bh = std::max(1, card_box.height);

    if (!border_buf || border_buf->get_width() != bw || border_buf->get_height() != bh) {
        if (border_scene_buf) {
            wlr_scene_node_destroy(&border_scene_buf->node);
            border_scene_buf = nullptr;
        }
        if (border_buf) {
            border_buf->drop();
            border_buf = nullptr;
        }
        border_buf = OverviewCairoBuffer::create(bw, bh);
    }
    if (!border_buf) return;

    border_buf->clear();
    cairo_t* cr = border_buf->get_cr();

    double th = 2.0;
    double half_th = th / 2.0;
    int r = OverviewConfig::get().corner_radius();
    double draw_r = std::max(0.0, (double)r - half_th);

    draw_rounded_rectangle(cr, half_th, half_th, bw - th, bh - th, draw_r);

    if (is_active) {
        cairo_set_source_rgba(cr, theme.primary.r, theme.primary.g, theme.primary.b, theme.primary.a);
    } else {
        cairo_set_source_rgba(cr, theme.outline_variant.r, theme.outline_variant.g, theme.outline_variant.b, 0.95);
    }
    cairo_set_line_width(cr, th);
    cairo_stroke(cr);

    if (!border_scene_buf) {
        border_scene_buf = wlr_scene_buffer_create(overlay_tree, border_buf->get_wlr_buffer());
    } else {
        wlr_scene_buffer_set_buffer(border_scene_buf, border_buf->get_wlr_buffer());
        wlr_scene_node_set_enabled(&border_scene_buf->node, true);
    }
    wlr_scene_node_set_position(&border_scene_buf->node, card_box.x, card_box.y);
}

void WorkspaceCard::render_header(struct wlr_scene_tree* overlay_tree, const OverviewTheme& theme,
                                  size_t win_count, bool is_orig_active) {
    int bw = card_box.width;
    int bh = 24;
    int bx = card_box.x;
    int by = card_box.y - bh - 4;

    if (!header_buf || header_buf->get_width() != bw || header_buf->get_height() != bh) {
        if (header_scene_buf) {
            wlr_scene_node_destroy(&header_scene_buf->node);
            header_scene_buf = nullptr;
        }
        if (header_buf) {
            header_buf->drop();
            header_buf = nullptr;
        }
        header_buf = OverviewCairoBuffer::create(bw, bh);
    }
    if (!header_buf) return;

    header_buf->clear();
    cairo_t* cr = header_buf->get_cr();

    std::string title = "Workspace " + std::to_string(ws_id);
    if (is_orig_active) {
        title += "  [Active]";
    }

    std::string count_str = (win_count == 0) ? "Empty" : (std::to_string(win_count) + (win_count == 1 ? " window" : " windows"));

    PangoLayout* layout = pango_cairo_create_layout(cr);
    PangoFontDescription* desc = pango_font_description_from_string(OverviewConfig::get().font().c_str());
    pango_layout_set_font_description(layout, desc);
    pango_font_description_free(desc);

    pango_layout_set_text(layout, title.c_str(), -1);
    if (is_orig_active) {
        cairo_set_source_rgba(cr, theme.primary.r, theme.primary.g, theme.primary.b, 1.0);
    } else {
        cairo_set_source_rgba(cr, theme.on_surface.r, theme.on_surface.g, theme.on_surface.b, 0.95);
    }
    cairo_move_to(cr, 2, 4);
    pango_cairo_show_layout(cr, layout);

    pango_layout_set_text(layout, count_str.c_str(), -1);
    int cw = 0, ch = 0;
    pango_layout_get_pixel_size(layout, &cw, &ch);

    cairo_set_source_rgba(cr, theme.on_surface_variant.r, theme.on_surface_variant.g, theme.on_surface_variant.b, 0.80);
    cairo_move_to(cr, bw - cw - 2, 4);
    pango_cairo_show_layout(cr, layout);

    g_object_unref(layout);

    if (!header_scene_buf) {
        header_scene_buf = wlr_scene_buffer_create(overlay_tree, header_buf->get_wlr_buffer());
    } else {
        wlr_scene_buffer_set_buffer(header_scene_buf, header_buf->get_wlr_buffer());
    }
    wlr_scene_node_set_position(&header_scene_buf->node, bx, by);
}

void WorkspaceCard::set_alpha(float alpha) {
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    const auto& theme = OverviewConfig::get().theme();

    if (shadow) {
        float s_color[4];
        const auto& sc = OverviewConfig::get().shadow_color();
        make_premul_color(s_color, sc.r, sc.g, sc.b, sc.a * alpha);
        wlr_scene_shadow_set_color(shadow, s_color);
    }
    if (bg_rect) {
        float bg_color[4];
        make_premul_color(bg_color, theme.surface.r, theme.surface.g, theme.surface.b, theme.surface.a * alpha);
        wlr_scene_rect_set_color(bg_rect, bg_color);
    }
    if (border_scene_buf) {
        wlr_scene_buffer_set_opacity(border_scene_buf, alpha);
    }
    if (header_scene_buf) {
        wlr_scene_buffer_set_opacity(header_scene_buf, alpha);
    }
    for (auto& wt : window_thumbnails) {
        if (wt.highlight_scene_buf) {
            wlr_scene_buffer_set_opacity(wt.highlight_scene_buf, alpha);
        }
    }
}

void WorkspaceCard::destroy_nodes() {
    if (shadow) {
        wlr_scene_node_destroy(&shadow->node);
        shadow = nullptr;
    }
    if (border_scene_buf) {
        wlr_scene_node_destroy(&border_scene_buf->node);
        border_scene_buf = nullptr;
    }
    if (border_buf) {
        border_buf->drop();
        border_buf = nullptr;
    }
    if (bg_rect) {
        wlr_scene_node_destroy(&bg_rect->node);
        bg_rect = nullptr;
    }
    if (header_scene_buf) {
        wlr_scene_node_destroy(&header_scene_buf->node);
        header_scene_buf = nullptr;
    }
    if (header_buf) {
        header_buf->drop();
        header_buf = nullptr;
    }
    for (auto& wt : window_thumbnails) {
        wt.destroy_highlight();
    }
    window_thumbnails.clear();
}

} // namespace miquoverview
