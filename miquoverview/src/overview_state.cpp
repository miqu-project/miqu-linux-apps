#include "overview_state.hpp"
#include "cairo_buffer.hpp"
#include "overview_config.hpp"
#include "miquland/plugin.hpp"
#include "core/server.hpp"
#include "core/workspace.hpp"
#include "core/output.hpp"
#include "core/view.hpp"
#include "core/animation/easing.hpp"

#include <cmath>
#include <algorithm>
#include <iostream>
#include <linux/input-event-codes.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <pango/pangocairo.h>

namespace miquoverview {

void OverviewState::init(miquland::PluginAPI* api) {
    m_api = api;
    OverviewConfig::get().load();
    if (m_api && m_api->get_server()) {
        OverviewConfig::get().setup_watcher(m_api->get_server()->get_event_loop());
    }
    OverviewConfig::get().set_reload_callback([this]() {
        refresh_appearance();
    });
}

void OverviewState::toggle() {
    if (m_is_open) {
        close();
    } else {
        open();
    }
}

void OverviewState::render_top_banner() {
    if (!m_overlay_tree) return;

    const auto& theme = OverviewConfig::get().theme();
    int bw = 460;
    int bh = 32;
    int bx = m_screen.x + (m_screen.width - bw) / 2;
    int by = m_screen.y + std::max(8, OverviewConfig::get().edge_margin() / 3);

    if (!m_top_banner_buf || m_top_banner_buf->get_width() != bw || m_top_banner_buf->get_height() != bh) {
        if (m_top_banner_scene_buf) {
            wlr_scene_node_destroy(&m_top_banner_scene_buf->node);
            m_top_banner_scene_buf = nullptr;
        }
        if (m_top_banner_buf) {
            m_top_banner_buf->drop();
            m_top_banner_buf = nullptr;
        }
        m_top_banner_buf = OverviewCairoBuffer::create(bw, bh);
    }
    if (!m_top_banner_buf) return;

    m_top_banner_buf->clear();
    cairo_t* cr = m_top_banner_buf->get_cr();

    // Capsule pill container
    cairo_new_sub_path(cr);
    cairo_arc(cr, bw - bh / 2.0, bh / 2.0, bh / 2.0 - 1.0, -M_PI / 2, M_PI / 2);
    cairo_arc(cr, bh / 2.0, bh / 2.0, bh / 2.0 - 1.0, M_PI / 2, 3 * M_PI / 2);
    cairo_close_path(cr);

    cairo_set_source_rgba(cr, theme.surface.r, theme.surface.g, theme.surface.b, 0.92);
    cairo_fill(cr);

    // Text
    PangoLayout* layout = pango_cairo_create_layout(cr);
    pango_layout_set_text(layout, "Miqu Overview   •   Click window or press 1-9 / Esc", -1);
    PangoFontDescription* desc = pango_font_description_from_string(OverviewConfig::get().font().c_str());
    pango_layout_set_font_description(layout, desc);
    pango_font_description_free(desc);

    int tw = 0, th = 0;
    pango_layout_get_pixel_size(layout, &tw, &th);

    cairo_set_source_rgba(cr, theme.on_surface.r, theme.on_surface.g, theme.on_surface.b, 0.95);
    cairo_move_to(cr, (bw - tw) / 2.0, (bh - th) / 2.0);
    pango_cairo_show_layout(cr, layout);
    g_object_unref(layout);

    if (!m_top_banner_scene_buf) {
        m_top_banner_scene_buf = wlr_scene_buffer_create(m_overlay_tree, m_top_banner_buf->get_wlr_buffer());
    } else {
        wlr_scene_buffer_set_buffer(m_top_banner_scene_buf, m_top_banner_buf->get_wlr_buffer());
    }
    wlr_scene_node_set_position(&m_top_banner_scene_buf->node, bx, by);
}

void OverviewState::open() {
    if (m_is_open || !m_api) return;

    auto* wm = m_api->get_workspace_manager();
    auto* om = m_api->get_output_manager();
    if (!wm || !om) return;

    m_orig_active_ws = wm->get_active_workspace_id();
    m_selected_ws = m_orig_active_ws;

    m_accum_swipe_dx = 0.0;
    m_accum_swipe_dy = 0.0;
    m_swipe_action_triggered = false;

    if (m_api) {
        m_api->clear_pointer_focus();
        m_api->set_cursor_icon("default");
    }

    // 1. Usable screen area (respects exclusive reservations by miqubar, etc.)
    m_screen = om->get_primary_usable_geometry();
    if (m_screen.width <= 0 || m_screen.height <= 0) {
        m_screen = om->get_primary_geometry();
    }
    if (m_screen.width <= 0 || m_screen.height <= 0) {
        m_screen = {0, 0, 1920, 1080};
    }

    // 2. Collect ONLY active workspaces (no empty non-existent placeholders)
    std::vector<size_t> active_workspaces;
    for (const auto& [id, ws] : wm->get_workspaces_map()) {
        if (!ws->is_empty() || id == m_orig_active_ws) {
            active_workspaces.push_back(id);
        }
    }
    if (active_workspaces.empty()) {
        active_workspaces.push_back(m_orig_active_ws);
    }
    std::sort(active_workspaces.begin(), active_workspaces.end());
    size_t count = active_workspaces.size();

    // 3. Grid configuration
    int cols = 1;
    if (count <= 1) {
        cols = 1;
    } else if (count <= 4) {
        cols = 2;
    } else if (count <= 6) {
        cols = 3;
    } else {
        cols = 4;
    }
    int rows = static_cast<int>((count + cols - 1) / cols);
    m_cols = cols;

    double screen_aspect = static_cast<double>(m_screen.width) / static_cast<double>(m_screen.height);
    int margin = OverviewConfig::get().edge_margin();

    int banner_h = 32;
    int banner_gap = 16;
    int header_h = 24;
    int header_gap = 6;
    int top_reserved = banner_h + banner_gap;

    int gap_x = (cols > 2) ? 20 : 28;
    int gap_y = (rows > 2) ? 24 : 36;

    int avail_w = m_screen.width - 2 * margin;
    int avail_h = m_screen.height - 2 * margin - top_reserved;
    if (avail_w < 200) avail_w = 200;
    if (avail_h < 150) avail_h = 150;

    int cell_w = std::max(100, (avail_w - (cols - 1) * gap_x) / cols);
    int cell_h = std::max(80, (avail_h - (rows - 1) * (gap_y + header_h + header_gap)) / rows);

    int card_w = cell_w;
    int card_h = static_cast<int>(std::round(card_w / screen_aspect));
    if (card_h > cell_h) {
        card_h = cell_h;
        card_w = static_cast<int>(std::round(card_h * screen_aspect));
    }

    double scale = static_cast<double>(card_w) / static_cast<double>(m_screen.width);

    int total_grid_h = rows * card_h + (rows - 1) * (gap_y + header_h + header_gap);
    int grid_start_y = m_screen.y + margin + top_reserved + (avail_h - total_grid_h) / 2;

    const auto& theme = OverviewConfig::get().theme();

    // Disable compositor-wide popups during overview so dropdowns/tooltips don't float unscaled
    if (m_api->get_server() && m_api->get_server()->get_popup_tree()) {
        wlr_scene_node_set_enabled(&m_api->get_server()->get_popup_tree()->node, false);
    }

    // 4. Scene trees
    m_bg_tree = wlr_scene_tree_create(m_api->get_workspaces_tree());
    wlr_scene_node_lower_to_bottom(&m_bg_tree->node);
    m_overlay_tree = wlr_scene_tree_create(m_api->get_layer_overlay_tree());

    // 5. GPU frosted glass backdrop blur & dimmed backdrop
    if (OverviewConfig::get().blur()) {
        struct wlr_scene* scene = m_api->get_scene();
        if (scene && scene->WLR_PRIVATE.blur_data.num_passes == 0) {
            wlr_scene_set_blur_data(scene, 3, 5, 0.0f, 1.0f, 1.0f, 1.0f);
            m_restored_blur_passes = true;
        }

        m_blur_node = wlr_scene_blur_create(m_bg_tree, m_screen.width, m_screen.height);
        if (m_blur_node) {
            wlr_scene_node_set_position(&m_blur_node->node, m_screen.x, m_screen.y);
            wlr_scene_node_lower_to_bottom(&m_blur_node->node);
            wlr_scene_blur_set_alpha(m_blur_node, OverviewConfig::get().animation() ? 0.0f : OverviewConfig::get().blur_alpha());
            wlr_scene_blur_set_strength(m_blur_node, OverviewConfig::get().animation() ? 0.0f : 1.0f);
        }
    }

    const auto& bg = (theme.backdrop.a > 0.0f) ? theme.backdrop : theme.background;
    float dim_color[4];
    float init_alpha = OverviewConfig::get().animation() ? 0.0f : bg.a;
    make_premul_color(dim_color, bg.r, bg.g, bg.b, init_alpha);
    m_backdrop_rect = wlr_scene_rect_create(m_bg_tree, m_screen.width, m_screen.height, dim_color);
    wlr_scene_node_set_position(&m_backdrop_rect->node, m_screen.x, m_screen.y);

    render_top_banner();

    m_cards.clear();
    m_saved_views.clear();

    // 6. Layout grid row-by-row with centered incomplete rows
    for (int r = 0; r < rows; ++r) {
        size_t start_idx = r * cols;
        size_t end_idx = std::min(start_idx + cols, count);
        size_t items_in_row = end_idx - start_idx;

        int row_w = items_in_row * card_w + (items_in_row - 1) * gap_x;
        int row_start_x = m_screen.x + margin + (avail_w - row_w) / 2;
        int row_y = grid_start_y + r * (card_h + gap_y + header_h + header_gap) + header_h + header_gap;

        for (size_t c = 0; c < items_in_row; ++c) {
            size_t idx = start_idx + c;
            size_t ws_id = active_workspaces[idx];
            int card_x = row_start_x + c * (card_w + gap_x);
            int card_y = row_y;

            WorkspaceCard card;
            card.ws_id = ws_id;
            card.card_box = {card_x, card_y, card_w, card_h};
            card.scale = scale;
            card.is_active = (ws_id == m_orig_active_ws);

            card.create_decorations(m_bg_tree, m_overlay_tree, theme, m_orig_active_ws);

            miquland::Workspace* ws = wm->get_workspace(ws_id);
            size_t win_count = ws ? ws->total_view_count() : 0;
            card.render_header(m_overlay_tree, theme, win_count, ws_id == m_orig_active_ws);

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

                    int rel_x = v->get_x() - m_screen.x;
                    int rel_y = v->get_y() - m_screen.y;

                    int vx = static_cast<int>(std::round(rel_x * scale));
                    int vy = static_cast<int>(std::round(rel_y * scale));
                    int vw = static_cast<int>(std::round(v->get_width() * scale));
                    int vh = static_cast<int>(std::round(v->get_height() * scale));

                    wlr_scene_node_set_position(&v->get_scene_tree()->node, vx, vy);
                    v->set_overview_scaled(true, scale);

                    WindowThumbnail wt;
                    wt.view = v;
                    wt.ws_id = ws_id;
                    wt.box = {card_x + vx, card_y + vy, vw, vh};
                    int base_r = v->is_fullscreen() ? 0 : OverviewConfig::get().corner_radius();
                    wt.corner_radius = static_cast<int>(std::round(static_cast<double>(base_r) * scale));
                    card.window_thumbnails.push_back(std::move(wt));
                }
            }

            m_cards.push_back(std::move(card));
        }
    }

    m_is_open = true;

    if (OverviewConfig::get().animation()) {
        m_is_animating = true;
        m_is_closing = false;
        apply_animation_step(0.0f, true);
        double dur = static_cast<double>(OverviewConfig::get().duration());
        m_anim_id = m_api->schedule_animation(
            dur,
            miquland::Easing::ease_out_cubic,
            [this](float progress) {
                apply_animation_step(progress, true);
            },
            [this]() {
                apply_animation_step(1.0f, true);
                m_is_animating = false;
                m_anim_id = 0;
            }
        );
    } else {
        apply_animation_step(1.0f, true);
    }
}

void OverviewState::apply_animation_step(float progress, bool is_opening) {
    float t = is_opening ? progress : (1.0f - progress);
    t = std::clamp(t, 0.0f, 1.0f);

    const auto& theme = OverviewConfig::get().theme();
    const auto& bg = (theme.backdrop.a > 0.0f) ? theme.backdrop : theme.background;

    if (m_backdrop_rect) {
        float dim_color[4];
        make_premul_color(dim_color, bg.r, bg.g, bg.b, bg.a * t);
        wlr_scene_rect_set_color(m_backdrop_rect, dim_color);
    }

    if (m_blur_node) {
        wlr_scene_blur_set_alpha(m_blur_node, OverviewConfig::get().blur_alpha() * t);
        wlr_scene_blur_set_strength(m_blur_node, t);
    }

    if (m_top_banner_scene_buf) {
        wlr_scene_buffer_set_opacity(m_top_banner_scene_buf, t);
        int final_y = m_screen.y + std::max(8, OverviewConfig::get().edge_margin() / 3);
        int cur_y = final_y - static_cast<int>(std::round((1.0f - t) * 16.0f));
        int cur_x = m_screen.x + (m_screen.width - 460) / 2;
        wlr_scene_node_set_position(&m_top_banner_scene_buf->node, cur_x, cur_y);
    }

    for (auto& card : m_cards) {
        card.set_alpha(t);
    }

    for (const auto& svi : m_saved_views) {
        if (!svi.view || !svi.view->get_scene_tree()) continue;
        if (svi.ws_id != m_orig_active_ws) {
            float view_op = t;
            wlr_scene_node_for_each_buffer(&svi.view->get_scene_tree()->node, [](struct wlr_scene_buffer* buf, int sx, int sy, void* udata) {
                float op = *static_cast<float*>(udata);
                wlr_scene_buffer_set_opacity(buf, op);
            }, &view_op);
        }
    }
}

void OverviewState::close(size_t target_ws_id, miquland::View* focus_view) {
    if (!m_is_open || !m_api) return;

    if (m_is_closing) return;

    if (m_is_animating && m_anim_id != 0) {
        m_api->cancel_animation(m_anim_id);
        m_anim_id = 0;
    }

    if (OverviewConfig::get().animation()) {
        m_is_animating = true;
        m_is_closing = true;
        double dur = static_cast<double>(OverviewConfig::get().duration());
        m_anim_id = m_api->schedule_animation(
            dur,
            miquland::Easing::ease_out_cubic,
            [this](float progress) {
                apply_animation_step(progress, false);
            },
            [this, target_ws_id, focus_view]() {
                m_is_animating = false;
                m_is_closing = false;
                m_anim_id = 0;
                finish_close(target_ws_id, focus_view);
            }
        );
    } else {
        finish_close(target_ws_id, focus_view);
    }
}

void OverviewState::finish_close(size_t target_ws_id, miquland::View* focus_view) {
    auto* wm = m_api->get_workspace_manager();

    // 1. Restore views geometry and exit overview scale mode safely
    for (const auto& svi : m_saved_views) {
        if (!svi.view || !svi.view->get_scene_tree()) continue;

        svi.view->set_overview_scaled(false);
        wlr_scene_node_set_position(&svi.view->get_scene_tree()->node, svi.orig_x, svi.orig_y);

        float full_op = 1.0f;
        wlr_scene_node_for_each_buffer(&svi.view->get_scene_tree()->node, [](struct wlr_scene_buffer* buf, int sx, int sy, void* udata) {
            float op = *static_cast<float*>(udata);
            wlr_scene_buffer_set_opacity(buf, op);
        }, &full_op);
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

    if (m_backdrop_rect) {
        wlr_scene_node_destroy(&m_backdrop_rect->node);
        m_backdrop_rect = nullptr;
    }

    if (m_blur_node) {
        wlr_scene_node_destroy(&m_blur_node->node);
        m_blur_node = nullptr;
    }

    if (m_restored_blur_passes) {
        if (m_api->get_scene()) {
            wlr_scene_set_blur_data(m_api->get_scene(), 0, 0, 0.0f, 1.0f, 1.0f, 1.0f);
        }
        m_restored_blur_passes = false;
    }

    if (m_bg_tree) {
        wlr_scene_node_destroy(&m_bg_tree->node);
        m_bg_tree = nullptr;
    }
    if (m_overlay_tree) {
        wlr_scene_node_destroy(&m_overlay_tree->node);
        m_overlay_tree = nullptr;
    }

    if (m_api->get_server() && m_api->get_server()->get_popup_tree()) {
        wlr_scene_node_set_enabled(&m_api->get_server()->get_popup_tree()->node, true);
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

    if (m_api) {
        m_api->set_cursor_icon("default");
        if (focus_view) {
            const auto& geom = focus_view->get_geometry();
            double cx = geom.x + std::max(1, geom.width) / 2.0;
            double cy = geom.y + std::max(1, geom.height) / 2.0;
            m_api->warp_cursor(cx, cy);
        } else {
            m_api->recheck_cursor_focus();
        }
    }
}

void OverviewState::handle_view_destroyed(miquland::View* view) {
    if (!m_is_open || !view) return;

    m_saved_views.erase(
        std::remove_if(m_saved_views.begin(), m_saved_views.end(),
                       [view](const SavedViewInfo& svi) { return svi.view == view; }),
        m_saved_views.end());

    auto* wm = m_api ? m_api->get_workspace_manager() : nullptr;
    const auto& theme = OverviewConfig::get().theme();

    for (auto& card : m_cards) {
        for (auto it = card.window_thumbnails.begin(); it != card.window_thumbnails.end(); ) {
            if (it->view == view) {
                it->destroy_highlight();
                it = card.window_thumbnails.erase(it);
            } else {
                ++it;
            }
        }
        miquland::Workspace* ws = wm ? wm->get_workspace(card.ws_id) : nullptr;
        size_t win_count = ws ? ws->total_view_count() : 0;
        card.render_header(m_overlay_tree, theme, win_count, card.ws_id == m_orig_active_ws);
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

    int step_y = (m_cols > 0) ? m_cols : 2;
    int next_idx = cur_idx + delta_x + delta_y * step_y;
    if (next_idx < 0) next_idx = 0;
    if (next_idx >= static_cast<int>(m_cards.size())) next_idx = static_cast<int>(m_cards.size()) - 1;

    m_selected_ws = m_cards[next_idx].ws_id;
    const auto& theme = OverviewConfig::get().theme();
    for (auto& card : m_cards) {
        card.is_active = (card.ws_id == m_selected_ws);
        card.update_border(m_overlay_tree, theme);
    }
}

bool OverviewState::handle_pointer_button(double lx, double ly, uint32_t button, bool pressed) {
    if (!m_is_open) return false;
    if (m_is_closing) return true;

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
    if (m_is_closing) return true;

    if (m_api) {
        m_api->set_cursor_icon("default");
    }

    const auto& theme = OverviewConfig::get().theme();
    for (auto& card : m_cards) {
        bool hovered = wlr_box_contains_point(&card.card_box, lx, ly);
        if (hovered != card.is_hovered) {
            card.is_hovered = hovered;
            card.update_border(m_overlay_tree, theme);
        }
        for (auto& wt : card.window_thumbnails) {
            bool win_h = hovered && wlr_box_contains_point(&wt.box, lx, ly);
            if (win_h != wt.is_hovered) {
                wt.is_hovered = win_h;
                wt.update_highlight(m_overlay_tree, theme);
            }
        }
    }
    return true;
}

bool OverviewState::handle_pointer_axis(double lx, double ly, double delta_x, double delta_y) {
    if (!m_is_open) return false;
    if (m_is_closing) return true;

    if (delta_y > 0 || delta_x > 0) {
        select_relative(1, 0);
    } else if (delta_y < 0 || delta_x < 0) {
        select_relative(-1, 0);
    }
    return true;
}

bool OverviewState::handle_key(uint32_t keysym, uint32_t modifiers, bool pressed) {
    if (!m_is_open) return false;
    if (m_is_closing) return true;
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

bool OverviewState::handle_touchpad_swipe(const miquland::SwipeEvent& event) {
    if (!m_is_open) return false;
    if (m_is_closing) return true;

    if (event.state == miquland::SwipeState::Begin) {
        m_accum_swipe_dx = 0.0;
        m_accum_swipe_dy = 0.0;
        m_swipe_action_triggered = false;
        return true;
    }

    if (event.state == miquland::SwipeState::Update) {
        m_accum_swipe_dx += event.dx;
        m_accum_swipe_dy += event.dy;

        if (m_swipe_action_triggered) {
            return true;
        }

        constexpr double SWIPE_HORIZ_THRESHOLD = 18.0;
        constexpr double SWIPE_VERT_THRESHOLD = 24.0;

        // Horizontal 3-finger swipe: navigate between workspace cards
        if (std::abs(m_accum_swipe_dx) >= SWIPE_HORIZ_THRESHOLD &&
            std::abs(m_accum_swipe_dx) > std::abs(m_accum_swipe_dy) * 1.2) {
            if (m_accum_swipe_dx > 0) {
                select_relative(1, 0);
            } else {
                select_relative(-1, 0);
            }
            m_swipe_action_triggered = true;
            return true;
        }

        // Vertical 3-finger swipe: dismiss overview to selected workspace
        if (std::abs(m_accum_swipe_dy) >= SWIPE_VERT_THRESHOLD &&
            std::abs(m_accum_swipe_dy) > std::abs(m_accum_swipe_dx) * 1.2) {
            close(m_selected_ws);
            m_swipe_action_triggered = true;
            return true;
        }

        return true;
    }

    if (event.state == miquland::SwipeState::End) {
        m_accum_swipe_dx = 0.0;
        m_accum_swipe_dy = 0.0;
        m_swipe_action_triggered = false;
        return true;
    }

    return true;
}

bool OverviewState::handle_touch_gesture(const miquland::TouchGestureEvent& event) {
    if (!m_is_open) return false;
    if (m_is_closing) return true;

    if (event.state == miquland::SwipeState::Begin) {
        m_swipe_action_triggered = false;
        return true;
    }

    if (event.state == miquland::SwipeState::Update) {
        if (m_swipe_action_triggered) {
            return true;
        }

        constexpr double TOUCH_HORIZ_THRESHOLD = 30.0;
        constexpr double TOUCH_VERT_THRESHOLD = 40.0;

        if (std::abs(event.avg_dx) >= TOUCH_HORIZ_THRESHOLD &&
            std::abs(event.avg_dx) > std::abs(event.avg_dy) * 1.2) {
            if (event.avg_dx > 0) {
                select_relative(1, 0);
            } else {
                select_relative(-1, 0);
            }
            m_swipe_action_triggered = true;
            return true;
        }

        if (std::abs(event.avg_dy) >= TOUCH_VERT_THRESHOLD &&
            std::abs(event.avg_dy) > std::abs(event.avg_dx) * 1.2) {
            close(m_selected_ws);
            m_swipe_action_triggered = true;
            return true;
        }

        return true;
    }

    if (event.state == miquland::SwipeState::End) {
        m_swipe_action_triggered = false;
        return true;
    }

    return true;
}

void OverviewState::refresh_appearance() {
    if (!m_is_open) return;

    const auto& theme = OverviewConfig::get().theme();
    const auto& bg = (theme.backdrop.a > 0.0f) ? theme.backdrop : theme.background;

    if (m_backdrop_rect) {
        float dim_color[4];
        make_premul_color(dim_color, bg.r, bg.g, bg.b, bg.a);
        wlr_scene_rect_set_color(m_backdrop_rect, dim_color);
    }

    if (m_blur_node) {
        wlr_scene_node_set_enabled(&m_blur_node->node, OverviewConfig::get().blur());
        wlr_scene_blur_set_alpha(m_blur_node, OverviewConfig::get().blur_alpha());
        wlr_scene_blur_set_strength(m_blur_node, 1.0f);
    }

    render_top_banner();

    auto* wm = m_api ? m_api->get_workspace_manager() : nullptr;
    for (auto& card : m_cards) {
        card.update_border(m_overlay_tree, theme);
        card.set_alpha(1.0f);
        miquland::Workspace* ws = wm ? wm->get_workspace(card.ws_id) : nullptr;
        size_t win_count = ws ? ws->total_view_count() : 0;
        card.render_header(m_overlay_tree, theme, win_count, card.ws_id == m_orig_active_ws);
        for (auto& wt : card.window_thumbnails) {
            wt.update_highlight(m_overlay_tree, theme);
        }
    }

    if (m_api) {
        m_api->schedule_frame();
    }
}

} // namespace miquoverview
