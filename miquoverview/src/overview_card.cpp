#include "overview_card.hpp"
#include "overview_config.hpp"
#include "core/view.hpp"
#include <algorithm>
#include <pango/pangocairo.h>

namespace miquoverview {

void WindowThumbnail::create_outline(struct wlr_scene_tree* parent, const OverviewTheme& theme) {
    if (!parent) return;
    destroy_outline();

    float color[4];
    make_premul_color(color, theme.outline.r, theme.outline.g, theme.outline.b, 0.75f);

    int w = std::max(1, box.width);
    int h = std::max(1, box.height);

    // 0: top, 1: bottom, 2: left, 3: right
    outline[0] = wlr_scene_rect_create(parent, w, 1, color);
    outline[1] = wlr_scene_rect_create(parent, w, 1, color);
    outline[2] = wlr_scene_rect_create(parent, 1, h, color);
    outline[3] = wlr_scene_rect_create(parent, 1, h, color);

    wlr_scene_node_set_position(&outline[0]->node, box.x, box.y);
    wlr_scene_node_set_position(&outline[1]->node, box.x, box.y + h - 1);
    wlr_scene_node_set_position(&outline[2]->node, box.x, box.y);
    wlr_scene_node_set_position(&outline[3]->node, box.x + w - 1, box.y);
}

void WindowThumbnail::update_outline(const OverviewTheme& theme) {
    float color[4];
    int th = (is_hovered || is_selected) ? 2 : 1;
    if (is_hovered || is_selected) {
        make_premul_color(color, theme.primary.r, theme.primary.g, theme.primary.b, theme.primary.a);
    } else {
        make_premul_color(color, theme.outline.r, theme.outline.g, theme.outline.b, 0.70f);
    }

    int w = std::max(1, box.width);
    int h = std::max(1, box.height);

    if (outline[0]) {
        wlr_scene_rect_set_color(outline[0], color);
        wlr_scene_rect_set_size(outline[0], w, th);
        wlr_scene_node_set_position(&outline[0]->node, box.x, box.y);
    }
    if (outline[1]) {
        wlr_scene_rect_set_color(outline[1], color);
        wlr_scene_rect_set_size(outline[1], w, th);
        wlr_scene_node_set_position(&outline[1]->node, box.x, box.y + h - th);
    }
    if (outline[2]) {
        wlr_scene_rect_set_color(outline[2], color);
        wlr_scene_rect_set_size(outline[2], th, h);
        wlr_scene_node_set_position(&outline[2]->node, box.x, box.y);
    }
    if (outline[3]) {
        wlr_scene_rect_set_color(outline[3], color);
        wlr_scene_rect_set_size(outline[3], th, h);
        wlr_scene_node_set_position(&outline[3]->node, box.x + w - th, box.y);
    }
}

void WindowThumbnail::destroy_outline() {
    for (int i = 0; i < 4; ++i) {
        if (outline[i]) {
            wlr_scene_node_destroy(&outline[i]->node);
            outline[i] = nullptr;
        }
    }
}

void WorkspaceCard::create_decorations(struct wlr_scene_tree* bg_tree, struct wlr_scene_tree* overlay_tree,
                                       const OverviewTheme& theme, size_t orig_active_ws) {
    destroy_nodes();

    // 1. Workspace card drop shadow
    if (OverviewConfig::get().shadow()) {
        float s_color[4];
        const auto& sc = OverviewConfig::get().shadow_color();
        make_premul_color(s_color, sc.r, sc.g, sc.b, sc.a);

        float sigma = OverviewConfig::get().shadow_blur();
        int radius = OverviewConfig::get().shadow_radius();
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

    // 2. Workspace background canvas
    float bg_color[4];
    make_premul_color(bg_color, theme.surface.r, theme.surface.g, theme.surface.b, theme.surface.a);
    bg_rect = wlr_scene_rect_create(bg_tree, card_box.width, card_box.height, bg_color);
    wlr_scene_node_set_position(&bg_rect->node, card_box.x, card_box.y);

    // 3. Workspace card border
    int th = (ws_id == orig_active_ws) ? 2 : 1;
    float b_color[4];
    if (ws_id == orig_active_ws) {
        make_premul_color(b_color, theme.primary.r, theme.primary.g, theme.primary.b, theme.primary.a);
    } else {
        make_premul_color(b_color, theme.outline.r, theme.outline.g, theme.outline.b, theme.outline.a);
    }

    borders[0] = wlr_scene_rect_create(overlay_tree, card_box.width, th, b_color);
    borders[1] = wlr_scene_rect_create(overlay_tree, card_box.width, th, b_color);
    borders[2] = wlr_scene_rect_create(overlay_tree, th, card_box.height, b_color);
    borders[3] = wlr_scene_rect_create(overlay_tree, th, card_box.height, b_color);

    wlr_scene_node_set_position(&borders[0]->node, card_box.x, card_box.y);
    wlr_scene_node_set_position(&borders[1]->node, card_box.x, card_box.y + card_box.height - th);
    wlr_scene_node_set_position(&borders[2]->node, card_box.x, card_box.y);
    wlr_scene_node_set_position(&borders[3]->node, card_box.x + card_box.width - th, card_box.y);
}

void WorkspaceCard::update_border(const OverviewTheme& theme) {
    float color[4];
    int th = (is_active || is_hovered) ? 2 : 1;
    if (is_active) {
        make_premul_color(color, theme.primary.r, theme.primary.g, theme.primary.b, theme.primary.a);
    } else if (is_hovered) {
        make_premul_color(color, theme.outline_variant.r, theme.outline_variant.g, theme.outline_variant.b, 0.95f);
    } else {
        make_premul_color(color, theme.outline.r, theme.outline.g, theme.outline.b, theme.outline.a);
    }

    if (borders[0]) {
        wlr_scene_rect_set_color(borders[0], color);
        wlr_scene_rect_set_size(borders[0], card_box.width, th);
        wlr_scene_node_set_position(&borders[0]->node, card_box.x, card_box.y);
    }
    if (borders[1]) {
        wlr_scene_rect_set_color(borders[1], color);
        wlr_scene_rect_set_size(borders[1], card_box.width, th);
        wlr_scene_node_set_position(&borders[1]->node, card_box.x, card_box.y + card_box.height - th);
    }
    if (borders[2]) {
        wlr_scene_rect_set_color(borders[2], color);
        wlr_scene_rect_set_size(borders[2], th, card_box.height);
        wlr_scene_node_set_position(&borders[2]->node, card_box.x, card_box.y);
    }
    if (borders[3]) {
        wlr_scene_rect_set_color(borders[3], color);
        wlr_scene_rect_set_size(borders[3], th, card_box.height);
        wlr_scene_node_set_position(&borders[3]->node, card_box.x + card_box.width - th, card_box.y);
    }
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
    float b_color[4];
    if (is_active) {
        make_premul_color(b_color, theme.primary.r, theme.primary.g, theme.primary.b, theme.primary.a * alpha);
    } else if (is_hovered) {
        make_premul_color(b_color, theme.outline_variant.r, theme.outline_variant.g, theme.outline_variant.b, 0.95f * alpha);
    } else {
        make_premul_color(b_color, theme.outline.r, theme.outline.g, theme.outline.b, theme.outline.a * alpha);
    }
    for (int i = 0; i < 4; ++i) {
        if (borders[i]) {
            wlr_scene_rect_set_color(borders[i], b_color);
        }
    }
    if (header_scene_buf) {
        wlr_scene_buffer_set_opacity(header_scene_buf, alpha);
    }
    for (auto& wt : window_thumbnails) {
        float o_color[4];
        if (wt.is_hovered || wt.is_selected) {
            make_premul_color(o_color, theme.primary.r, theme.primary.g, theme.primary.b, theme.primary.a * alpha);
        } else {
            make_premul_color(o_color, theme.outline.r, theme.outline.g, theme.outline.b, 0.70f * alpha);
        }
        for (int i = 0; i < 4; ++i) {
            if (wt.outline[i]) {
                wlr_scene_rect_set_color(wt.outline[i], o_color);
            }
        }
    }
}

void WorkspaceCard::destroy_nodes() {
    if (shadow) {
        wlr_scene_node_destroy(&shadow->node);
        shadow = nullptr;
    }
    for (int i = 0; i < 4; ++i) {
        if (borders[i]) {
            wlr_scene_node_destroy(&borders[i]->node);
            borders[i] = nullptr;
        }
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
        wt.destroy_outline();
    }
    window_thumbnails.clear();
}

} // namespace miquoverview
