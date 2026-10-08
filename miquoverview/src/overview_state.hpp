#pragma once

#include "overview_card.hpp"
#include "cairo_buffer.hpp"
#include "miquland/plugin.hpp"
#include <vector>
#include <memory>

namespace miquland {
    class View;
    class Server;
    class WorkspaceManager;
    class OutputManager;
}

namespace miquoverview {

struct SavedViewInfo {
    miquland::View* view = nullptr;
    int orig_x = 0;
    int orig_y = 0;
    size_t ws_id = 0;
};

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

    // Lifecycle defense: dynamically clean up state if a tracked window is destroyed mid-overview
    void handle_view_destroyed(miquland::View* view);

    // Input hooks
    bool handle_pointer_button(double lx, double ly, uint32_t button, bool pressed);
    bool handle_pointer_motion(double lx, double ly);
    bool handle_key(uint32_t keysym, uint32_t modifiers, bool pressed);

private:
    OverviewState() = default;
    ~OverviewState() = default;

    void create_top_banner(const struct wlr_box& screen);
    void render_card_header(WorkspaceCard& card);
    void select_relative(int delta_x, int delta_y);

    miquland::PluginAPI* m_api = nullptr;
    bool m_is_open = false;
    size_t m_orig_active_ws = 1;
    size_t m_selected_ws = 1;

    struct wlr_scene_tree* m_bg_tree = nullptr;
    struct wlr_scene_tree* m_overlay_tree = nullptr;
    struct wlr_scene_rect* m_backdrop_rect = nullptr;

    OverviewCairoBuffer* m_top_banner_buf = nullptr;
    struct wlr_scene_buffer* m_top_banner_scene_buf = nullptr;

    std::vector<WorkspaceCard> m_cards;
    std::vector<SavedViewInfo> m_saved_views;
};

} // namespace miquoverview
