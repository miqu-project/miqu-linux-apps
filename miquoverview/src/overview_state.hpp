#pragma once

#include "overview_card.hpp"
#include "cairo_buffer.hpp"
#include "overview_config.hpp"
#include "core/common/wlroots.hpp"
#include <vector>
#include <string>

namespace miquland {
    class PluginAPI;
    class View;
}

namespace miquoverview {

class OverviewState {
public:
    static OverviewState& get() {
        static OverviewState s_instance;
        return s_instance;
    }

    void init(miquland::PluginAPI* api);
    bool is_active() const { return m_is_open; }

    void toggle();
    void open();
    void close(size_t target_ws_id = 0, miquland::View* focus_view = nullptr);
    void refresh_appearance();

    // Lifecycle defense
    void handle_view_destroyed(miquland::View* view);

    // Input hooks
    bool handle_pointer_button(double lx, double ly, uint32_t button, bool pressed);
    bool handle_pointer_motion(double lx, double ly);
    bool handle_pointer_axis(double lx, double ly, double delta_x, double delta_y);
    bool handle_key(uint32_t keysym, uint32_t modifiers, bool pressed);

private:
    OverviewState() = default;
    ~OverviewState() = default;

    void render_top_banner();
    void select_relative(int delta_x, int delta_y);
    void apply_animation_step(float progress, bool is_opening);
    void finish_close(size_t target_ws_id, miquland::View* focus_view);

    miquland::PluginAPI* m_api = nullptr;
    bool m_is_open = false;
    bool m_is_animating = false;
    bool m_is_closing = false;
    uint64_t m_anim_id = 0;
    bool m_restored_blur_passes = false;

    size_t m_orig_active_ws = 1;
    size_t m_selected_ws = 1;
    int m_cols = 2;

    struct wlr_box m_screen = {0, 0, 1920, 1080}; // Usable screen geometry

    // Scene trees
    struct wlr_scene_tree* m_bg_tree = nullptr;
    struct wlr_scene_tree* m_overlay_tree = nullptr;

    // GPU frosted glass blur and full-screen dimmed backdrop
    struct wlr_scene_blur* m_blur_node = nullptr;
    struct wlr_scene_rect* m_backdrop_rect = nullptr;

    // Top hint banner
    OverviewCairoBuffer* m_top_banner_buf = nullptr;
    struct wlr_scene_buffer* m_top_banner_scene_buf = nullptr;

    std::vector<WorkspaceCard> m_cards;
    std::vector<SavedViewInfo> m_saved_views;
};

} // namespace miquoverview
