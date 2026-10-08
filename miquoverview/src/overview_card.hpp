#pragma once

#include "cairo_buffer.hpp"
#include "core/common/wlroots.hpp"
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
    struct wlr_box box = {0, 0, 0, 0};
    struct wlr_scene_rect* outline[4] = {nullptr, nullptr, nullptr, nullptr};

    void destroy_outline();
};

struct WorkspaceCard {
    size_t ws_id = 0;
    struct wlr_box card_box = {0, 0, 0, 0};
    double scale = 1.0;
    bool is_active = false;
    bool is_hovered = false;

    // Visual nodes in overlay scene tree
    struct wlr_scene_rect* bg_rect = nullptr;
    struct wlr_scene_rect* border_top = nullptr;
    struct wlr_scene_rect* border_bottom = nullptr;
    struct wlr_scene_rect* border_left = nullptr;
    struct wlr_scene_rect* border_right = nullptr;

    OverviewCairoBuffer* header_buf = nullptr;
    struct wlr_scene_buffer* header_scene_buf = nullptr;

    std::vector<WindowThumbnail> window_thumbnails;

    void update_border();
    void render_header(const std::string& label, bool active);
    void remove_thumbnail_for_view(miquland::View* view);
    void destroy_nodes();
};

} // namespace miquoverview
