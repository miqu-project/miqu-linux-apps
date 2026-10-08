#include "overview_state.hpp"
#include "core/server.hpp"
#include "core/workspace.hpp"
#include "core/view.hpp"
#include "core/output.hpp"

#include <xkbcommon/xkbcommon-keysyms.h>
#include <linux/input-event-codes.h>
#include <pango/pangocairo.h>

#include <cmath>
#include <algorithm>

namespace miquoverview {

void OverviewState::init(miquland::PluginAPI* api) {
    m_api = api;
}

void OverviewState::toggle() {
    if (m_is_open) {
        close();
    } else {
        open();
    }
}

void OverviewState::handle_view_destroyed(miquland::View* view) {
    if (!m_is_open || !view) return;

    // 1. Purge from saved views so close() never accesses deallocated memory
    for (auto it = m_saved_views.begin(); it != m_saved_views.end(); ) {
        if (it->view == view) {
            it = m_saved_views.erase(it);
        } else {
            ++it;
        }
    }

    // 2. Remove from card thumbnails and refresh window count header
    for (auto& card : m_cards) {
        card.remove_thumbnail_for_view(view);
        render_card_header(card);
    }
}

void OverviewState::open() {
    if (m_is_open || !m_api) return;

    auto* wm = m_api->get_workspace_manager();
    auto* om = m_api->get_output_manager();
    if (!wm || !om) return;

    m_orig_active_ws = wm->get_active_workspace_id();
    m_selected_ws = m_orig_active_ws;

    struct wlr_box screen = om->get_primary_usable_geometry();
    if (screen.width <= 0 || screen.height <= 0) {
        screen = om->get_primary_geometry();
    }
    if (screen.width <= 0 || screen.height <= 0) {
        screen.width = 1920;
        screen.height = 1080;
        screen.x = 0;
        screen.y = 0;
    }

    double screen_aspect = static_cast<double>(screen.width) / static_cast<double>(screen.height);

    // Number of workspaces to show
    size_t max_ws = 4;
    for (const auto& [id, ws] : wm->get_workspaces_map()) {
        if (!ws->is_empty() || id == m_orig_active_ws) {
            max_ws = std::max(max_ws, id);
        }
    }
    if (max_ws <= 4) max_ws = 4;
    else if (max_ws <= 6) max_ws = 6;
    else if (max_ws <= 8) max_ws = 8;
    else max_ws = 9;

    int cols = (max_ws <= 4) ? 2 : (max_ws <= 6 ? 3 : 4);
    int rows = (max_ws + cols - 1) / cols;

    int pad_x = 40;
    int pad_y = 64;
    int gap_x = 24;
    int gap_y = 36;

    int avail_w = screen.width - 2 * pad_x - (cols - 1) * gap_x;
    int avail_h = screen.height - 2 * pad_y - (rows - 1) * gap_y;
    int cell_w = std::max(100, avail_w / cols);
    int cell_h = std::max(80, avail_h / rows);

    int card_w = cell_w;
    int card_h = static_cast<int>(std::round(card_w / screen_aspect));
    if (card_h > cell_h) {
        card_h = cell_h;
        card_w = static_cast<int>(std::round(card_h * screen_aspect));
    }

    double scale = static_cast<double>(card_w) / static_cast<double>(screen.width);

    // Scene trees
    m_bg_tree = wlr_scene_tree_create(m_api->get_workspaces_tree());
    wlr_scene_node_lower_to_bottom(&m_bg_tree->node);
    m_overlay_tree = wlr_scene_tree_create(m_api->get_layer_overlay_tree());

    // Dimmed background
    float dim_color[4];
    make_premul_color(dim_color, 0.05f, 0.06f, 0.08f, 0.85f);
    m_backdrop_rect = wlr_scene_rect_create(m_bg_tree, screen.width, screen.height, dim_color);
    wlr_scene_node_set_position(&m_backdrop_rect->node, screen.x, screen.y);

    create_top_banner(screen);

    m_cards.clear();
    m_saved_views.clear();

    for (size_t i = 0; i < max_ws; ++i) {
        size_t ws_id = i + 1;
        int r = i / cols;
        int c = i % cols;

        int cell_left = screen.x + pad_x + c * (cell_w + gap_x);
        int cell_top  = screen.y + pad_y + r * (cell_h + gap_y);
        int card_x = cell_left + (cell_w - card_w) / 2;
        int card_y = cell_top + (cell_h - card_h) / 2;

        WorkspaceCard card;
        card.ws_id = ws_id;
        card.card_box = {card_x, card_y, card_w, card_h};
        card.scale = scale;
        card.is_active = (ws_id == m_orig_active_ws);

        // 1. Workspace canvas
        float canvas_bg[4];
        make_premul_color(canvas_bg, 0.08f, 0.09f, 0.12f, 1.0f);
        card.bg_rect = wlr_scene_rect_create(m_bg_tree, card_w, card_h, canvas_bg);
        wlr_scene_node_set_position(&card.bg_rect->node, card_x, card_y);

        // 2. Borders
        int border_th = card.is_active ? 3 : 1;
        float bcolor[4];
        make_premul_color(bcolor, 0.20f, 0.24f, 0.32f, 0.70f);
        if (card.is_active) make_premul_color(bcolor, 0.25f, 0.65f, 0.95f, 0.95f);

        card.border_top = wlr_scene_rect_create(m_overlay_tree, card_w, border_th, bcolor);
        wlr_scene_node_set_position(&card.border_top->node, card_x, card_y);

        card.border_bottom = wlr_scene_rect_create(m_overlay_tree, card_w, border_th, bcolor);
        wlr_scene_node_set_position(&card.border_bottom->node, card_x, card_y + card_h - border_th);

        card.border_left = wlr_scene_rect_create(m_overlay_tree, border_th, card_h, bcolor);
        wlr_scene_node_set_position(&card.border_left->node, card_x, card_y);

        card.border_right = wlr_scene_rect_create(m_overlay_tree, border_th, card_h, bcolor);
        wlr_scene_node_set_position(&card.border_right->node, card_x + card_w - border_th, card_y);

        // 3. Header badge
        render_card_header(card);

        // 4. Position and scale windows
        miquland::Workspace* ws = wm->get_or_create_workspace(ws_id);
        if (ws && ws->get_scene_tree()) {
            wlr_scene_node_set_position(&ws->get_scene_tree()->node, card_x, card_y);
            wlr_scene_node_set_enabled(&ws->get_scene_tree()->node, true);

            std::vector<miquland::View*> views = ws->get_tiled_views();
            for (miquland::View* fv : ws->get_floating_views()) {
                views.push_back(fv);
            }

            for (miquland::View* v : views) {
                if (!v || !v->get_scene_tree()) continue;

                SavedViewInfo svi;
                svi.view = v;
                svi.orig_x = v->get_x();
                svi.orig_y = v->get_y();
                svi.ws_id = ws_id;
                m_saved_views.push_back(svi);

                int rel_x = v->get_x() - screen.x;
                int rel_y = v->get_y() - screen.y;

                int vx = static_cast<int>(std::round(rel_x * scale));
                int vy = static_cast<int>(std::round(rel_y * scale));
                int vw = static_cast<int>(std::round(v->get_width() * scale));
                int vh = static_cast<int>(std::round(v->get_height() * scale));

                wlr_scene_node_set_position(&v->get_scene_tree()->node, vx, vy);
                v->set_overview_scaled(true, scale);

                WindowThumbnail wt;
                wt.view = v;
                wt.box = {card_x + vx, card_y + vy, vw, vh};

                float win_border[4];
                make_premul_color(win_border, 0.35f, 0.45f, 0.60f, 0.85f);

                wt.outline[0] = wlr_scene_rect_create(m_overlay_tree, vw, 1, win_border);
                wlr_scene_node_set_position(&wt.outline[0]->node, wt.box.x, wt.box.y);

                wt.outline[1] = wlr_scene_rect_create(m_overlay_tree, vw, 1, win_border);
                wlr_scene_node_set_position(&wt.outline[1]->node, wt.box.x, wt.box.y + vh - 1);

                wt.outline[2] = wlr_scene_rect_create(m_overlay_tree, 1, vh, win_border);
                wlr_scene_node_set_position(&wt.outline[2]->node, wt.box.x, wt.box.y);

                wt.outline[3] = wlr_scene_rect_create(m_overlay_tree, 1, vh, win_border);
                wlr_scene_node_set_position(&wt.outline[3]->node, wt.box.x + vw - 1, wt.box.y);

                card.window_thumbnails.push_back(wt);
            }
        }

        m_cards.push_back(std::move(card));
    }

    m_is_open = true;
}

void OverviewState::close(size_t target_ws_id, miquland::View* focus_view) {
    if (!m_is_open || !m_api) return;

    auto* wm = m_api->get_workspace_manager();

    // 1. Restore views geometry and exit overview scale mode safely
    for (const auto& svi : m_saved_views) {
        if (!svi.view || !svi.view->get_scene_tree()) continue;

        svi.view->set_overview_scaled(false);
        wlr_scene_node_set_position(&svi.view->get_scene_tree()->node, svi.orig_x, svi.orig_y);
    }
    m_saved_views.clear();

    size_t final_ws = (target_ws_id > 0) ? target_ws_id : m_orig_active_ws;

    // 2. Restore workspaces visibility and position
    if (wm) {
        for (const auto& card : m_cards) {
            miquland::Workspace* ws = wm->get_workspace(card.ws_id);
            if (ws && ws->get_scene_tree()) {
                wlr_scene_node_set_position(&ws->get_scene_tree()->node, 0, 0);
                wlr_scene_node_set_enabled(&ws->get_scene_tree()->node, ws->get_id() == final_ws);
            }
        }
    }

    // 3. Clean up overlay nodes
    for (auto& card : m_cards) {
        card.destroy_nodes();
    }
    m_cards.clear();

    if (m_top_banner_scene_buf) {
        wlr_scene_node_destroy(&m_top_banner_scene_buf->node);
        m_top_banner_scene_buf = nullptr;
    }
    if (m_top_banner_buf) {
        m_top_banner_buf->drop();
        m_top_banner_buf = nullptr;
    }

    if (m_bg_tree) {
        wlr_scene_node_destroy(&m_bg_tree->node);
        m_bg_tree = nullptr;
    }
    if (m_overlay_tree) {
        wlr_scene_node_destroy(&m_overlay_tree->node);
        m_overlay_tree = nullptr;
    }

    m_is_open = false;

    // 4. Switch workspace and focus
    if (wm) {
        wm->switch_to_workspace(final_ws, focus_view);
        if (focus_view) {
            focus_view->focus();
        }
        wm->recalculate_layout();
    }
}

void OverviewState::create_top_banner(const struct wlr_box& screen) {
    int bw = 420;
    int bh = 32;
    int bx = screen.x + (screen.width - bw) / 2;
    int by = screen.y + 16;

    m_top_banner_buf = OverviewCairoBuffer::create(bw, bh);
    if (!m_top_banner_buf) return;

    cairo_t* cr = m_top_banner_buf->get_cr();

    // Rounded background pill
    cairo_new_sub_path(cr);
    cairo_arc(cr, bw - bh / 2, bh / 2, bh / 2, -M_PI / 2, M_PI / 2);
    cairo_arc(cr, bh / 2, bh / 2, bh / 2, M_PI / 2, 3 * M_PI / 2);
    cairo_close_path(cr);
    cairo_set_source_rgba(cr, 0.12, 0.14, 0.18, 0.90);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 0.28, 0.35, 0.48, 0.80);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    // Text
    PangoLayout* layout = pango_cairo_create_layout(cr);
    pango_layout_set_text(layout, "Miqu Overview   •   Click card or press 1-9 / Esc", -1);
    PangoFontDescription* desc = pango_font_description_from_string("Sans Bold 10");
    pango_layout_set_font_description(layout, desc);
    pango_font_description_free(desc);

    int tw = 0, th = 0;
    pango_layout_get_pixel_size(layout, &tw, &th);

    cairo_set_source_rgba(cr, 0.88, 0.92, 0.98, 0.95);
    cairo_move_to(cr, (bw - tw) / 2, (bh - th) / 2);
    pango_cairo_show_layout(cr, layout);
    g_object_unref(layout);

    m_top_banner_scene_buf = wlr_scene_buffer_create(m_overlay_tree, m_top_banner_buf->get_wlr_buffer());
    wlr_scene_node_set_position(&m_top_banner_scene_buf->node, bx, by);
}

void OverviewState::render_card_header(WorkspaceCard& card) {
    int bw = card.card_box.width;
    int bh = 24;
    int bx = card.card_box.x;
    int by = card.card_box.y - bh - 6;

    if (!card.header_buf) {
        card.header_buf = OverviewCairoBuffer::create(bw, bh);
    }
    if (!card.header_buf) return;

    cairo_t* cr = card.header_buf->get_cr();
    card.header_buf->clear();

    std::string title = "Workspace " + std::to_string(card.ws_id);
    if (card.ws_id == m_orig_active_ws) {
        title += "  [Active]";
    }

    auto* wm = m_api->get_workspace_manager();
    miquland::Workspace* ws = wm ? wm->get_workspace(card.ws_id) : nullptr;
    size_t count = ws ? ws->total_view_count() : 0;
    std::string count_str = (count == 0) ? "Empty" : (std::to_string(count) + (count == 1 ? " window" : " windows"));

    PangoLayout* layout = pango_cairo_create_layout(cr);
    pango_layout_set_text(layout, title.c_str(), -1);
    PangoFontDescription* desc = pango_font_description_from_string("Sans Bold 10");
    pango_layout_set_font_description(layout, desc);
    pango_font_description_free(desc);

    if (card.ws_id == m_orig_active_ws) {
        cairo_set_source_rgba(cr, 0.35, 0.75, 1.0, 1.0);
    } else {
        cairo_set_source_rgba(cr, 0.85, 0.88, 0.94, 0.95);
    }
    cairo_move_to(cr, 2, 4);
    pango_cairo_show_layout(cr, layout);

    pango_layout_set_text(layout, count_str.c_str(), -1);
    desc = pango_font_description_from_string("Sans 9");
    pango_layout_set_font_description(layout, desc);
    pango_font_description_free(desc);

    int cw = 0, ch = 0;
    pango_layout_get_pixel_size(layout, &cw, &ch);

    cairo_set_source_rgba(cr, 0.55, 0.60, 0.70, 0.85);
    cairo_move_to(cr, bw - cw - 2, 5);
    pango_cairo_show_layout(cr, layout);

    g_object_unref(layout);

    if (!card.header_scene_buf) {
        card.header_scene_buf = wlr_scene_buffer_create(m_overlay_tree, card.header_buf->get_wlr_buffer());
        wlr_scene_node_set_position(&card.header_scene_buf->node, bx, by);
    }
}

void OverviewState::select_relative(int delta_x, int delta_y) {
    if (m_cards.empty()) return;
    int cur_idx = 0;
    for (size_t i = 0; i < m_cards.size(); ++i) {
        if (m_cards[i].ws_id == m_selected_ws) {
            cur_idx = static_cast<int>(i);
            break;
        }
    }

    int next_idx = cur_idx + delta_x + delta_y * 2;
    if (next_idx >= 0 && next_idx < static_cast<int>(m_cards.size())) {
        m_selected_ws = m_cards[next_idx].ws_id;
        for (auto& card : m_cards) {
            card.is_active = (card.ws_id == m_selected_ws);
            card.update_border();
        }
    }
}

bool OverviewState::handle_pointer_button(double lx, double ly, uint32_t button, bool pressed) {
    if (!m_is_open) return false;

    if (button == BTN_LEFT && pressed) {
        for (const auto& card : m_cards) {
            if (wlr_box_contains_point(&card.card_box, lx, ly)) {
                miquland::View* clicked_view = nullptr;
                for (auto it = card.window_thumbnails.rbegin(); it != card.window_thumbnails.rend(); ++it) {
                    if (wlr_box_contains_point(&it->box, lx, ly)) {
                        clicked_view = it->view;
                        break;
                    }
                }
                close(card.ws_id, clicked_view);
                return true;
            }
        }
        close(m_orig_active_ws);
        return true;
    }
    return true;
}

bool OverviewState::handle_pointer_motion(double lx, double ly) {
    if (!m_is_open) return false;

    for (auto& card : m_cards) {
        bool hovered = wlr_box_contains_point(&card.card_box, lx, ly);
        if (hovered != card.is_hovered) {
            card.is_hovered = hovered;
            card.update_border();
        }
    }
    return true;
}

bool OverviewState::handle_key(uint32_t keysym, uint32_t modifiers, bool pressed) {
    if (!m_is_open) return false;
    if (!pressed) return true;

    if (keysym == XKB_KEY_Escape) {
        close(m_orig_active_ws);
        return true;
    }

    if (keysym >= XKB_KEY_1 && keysym <= XKB_KEY_9) {
        size_t target_ws = keysym - XKB_KEY_1 + 1;
        close(target_ws);
        return true;
    }

    if (keysym == XKB_KEY_Return || keysym == XKB_KEY_KP_Enter || keysym == XKB_KEY_space) {
        close(m_selected_ws);
        return true;
    }

    if (keysym == XKB_KEY_Left || keysym == XKB_KEY_h) {
        select_relative(-1, 0);
        return true;
    }
    if (keysym == XKB_KEY_Right || keysym == XKB_KEY_l) {
        select_relative(1, 0);
        return true;
    }
    if (keysym == XKB_KEY_Up || keysym == XKB_KEY_k) {
        select_relative(0, -1);
        return true;
    }
    if (keysym == XKB_KEY_Down || keysym == XKB_KEY_j) {
        select_relative(0, 1);
        return true;
    }

    return true;
}

} // namespace miquoverview
