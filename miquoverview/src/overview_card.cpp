#include "overview_card.hpp"
#include "core/view.hpp"
#include <pango/pangocairo.h>
#include <algorithm>

namespace miquoverview {

static void get_border_color(float out[4], bool is_active, bool is_hovered) {
    if (is_hovered) {
        make_premul_color(out, 0.45f, 0.85f, 1.0f, 1.0f); // Bright cyan on hover
    } else if (is_active) {
        make_premul_color(out, 0.25f, 0.65f, 0.95f, 0.95f); // Deep active blue
    } else {
        make_premul_color(out, 0.20f, 0.24f, 0.32f, 0.70f); // Subtle outline for inactive
    }
}

void WindowThumbnail::destroy_outline() {
    for (int j = 0; j < 4; ++j) {
        if (outline[j]) {
            wlr_scene_node_destroy(&outline[j]->node);
            outline[j] = nullptr;
        }
    }
}

void WorkspaceCard::destroy_nodes() {
    for (auto& wt : window_thumbnails) {
        wt.destroy_outline();
    }
    window_thumbnails.clear();

    if (border_top) { wlr_scene_node_destroy(&border_top->node); border_top = nullptr; }
    if (border_bottom) { wlr_scene_node_destroy(&border_bottom->node); border_bottom = nullptr; }
    if (border_left) { wlr_scene_node_destroy(&border_left->node); border_left = nullptr; }
    if (border_right) { wlr_scene_node_destroy(&border_right->node); border_right = nullptr; }
    if (bg_rect) { wlr_scene_node_destroy(&bg_rect->node); bg_rect = nullptr; }

    if (header_scene_buf) {
        wlr_scene_node_destroy(&header_scene_buf->node);
        header_scene_buf = nullptr;
    }
    if (header_buf) {
        header_buf->drop();
        header_buf = nullptr;
    }
}

void WorkspaceCard::remove_thumbnail_for_view(miquland::View* v) {
    for (auto it = window_thumbnails.begin(); it != window_thumbnails.end(); ) {
        if (it->view == v) {
            it->destroy_outline();
            it = window_thumbnails.erase(it);
        } else {
            ++it;
        }
    }
}

void WorkspaceCard::update_border() {
    bool selected = (is_active || is_hovered);
    int border_th = selected ? 3 : 1;
    float color[4];
    get_border_color(color, is_active, is_hovered);

    if (border_top) {
        wlr_scene_rect_set_size(border_top, card_box.width, border_th);
        wlr_scene_rect_set_color(border_top, color);
    }
    if (border_bottom) {
        wlr_scene_rect_set_size(border_bottom, card_box.width, border_th);
        wlr_scene_rect_set_color(border_bottom, color);
        wlr_scene_node_set_position(&border_bottom->node, card_box.x, card_box.y + card_box.height - border_th);
    }
    if (border_left) {
        wlr_scene_rect_set_size(border_left, border_th, card_box.height);
        wlr_scene_rect_set_color(border_left, color);
    }
    if (border_right) {
        wlr_scene_rect_set_size(border_right, border_th, card_box.height);
        wlr_scene_rect_set_color(border_right, color);
        wlr_scene_node_set_position(&border_right->node, card_box.x + card_box.width - border_th, card_box.y);
    }
}

void WorkspaceCard::render_header(const std::string& label, bool active) {
    if (!header_buf) return;
    cairo_t* cr = header_buf->get_cr();
    header_buf->clear();

    PangoLayout* layout = pango_cairo_create_layout(cr);
    pango_layout_set_text(layout, label.c_str(), -1);
    PangoFontDescription* desc = pango_font_description_from_string("Sans Bold 10");
    pango_layout_set_font_description(layout, desc);
    pango_font_description_free(desc);

    if (active) {
        cairo_set_source_rgba(cr, 0.35, 0.75, 1.0, 1.0);
    } else {
        cairo_set_source_rgba(cr, 0.85, 0.88, 0.94, 0.95);
    }
    cairo_move_to(cr, 2, 4);
    pango_cairo_show_layout(cr, layout);

    g_object_unref(layout);
}

} // namespace miquoverview
