#pragma once

#include "cairo_buffer.hpp"
#include "core/common/wlroots.hpp"
#include "overview_config.hpp"
#include <vector>
#include <string>

namespace miquland {
    class View;
}

namespace miquoverview {

inline void make_premul_color(float out[4], float r, float g, float b, float a) {
    out[0] = r * a;
    out[1] = g * a;
    out[2] = b * a;
    out[3] = a;
}

struct WindowThumbnail {
    miquland::View* view = nullptr;
    size_t ws_id = 0;
    struct wlr_box box = {0, 0, 0, 0};
    struct wlr_scene_rect* outline[4] = {nullptr, nullptr, nullptr, nullptr};
    bool is_hovered = false;
    bool is_selected = false;

    void create_outline(struct wlr_scene_tree* parent, const OverviewTheme& theme);
    void update_outline(const OverviewTheme& theme);
    void destroy_outline();
};

struct WorkspaceCard {
    size_t ws_id = 0;
    struct wlr_box card_box = {0, 0, 0, 0};
    double scale = 1.0;
    bool is_active = false;
    bool is_hovered = false;

    // Visual nodes
    struct wlr_scene_shadow* shadow = nullptr;
    struct wlr_scene_rect* bg_rect = nullptr;
    struct wlr_scene_rect* borders[4] = {nullptr, nullptr, nullptr, nullptr};

    OverviewCairoBuffer* header_buf = nullptr;
    struct wlr_scene_buffer* header_scene_buf = nullptr;

    std::vector<WindowThumbnail> window_thumbnails;

    void create_decorations(struct wlr_scene_tree* bg_tree, struct wlr_scene_tree* overlay_tree,
                            const OverviewTheme& theme, size_t orig_active_ws);
    void update_border(const OverviewTheme& theme);
    void render_header(struct wlr_scene_tree* overlay_tree, const OverviewTheme& theme,
                       size_t win_count, bool is_orig_active);
    void set_alpha(float alpha);
    void destroy_nodes();
};

struct SavedViewInfo {
    miquland::View* view = nullptr;
    int orig_x = 0;
    int orig_y = 0;
    size_t ws_id = 0;
};

} // namespace miquoverview
