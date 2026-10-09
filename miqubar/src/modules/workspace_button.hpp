#pragma once

#include "miqutoolkit/view/view.hpp"
#include "miqutoolkit/system/workspace_manager.hpp"
#include <memory>
#include <vector>

namespace miqu {
class Window;
}

namespace miqubar {

class WorkspaceButtonView : public miqu::View {
public:
    WorkspaceButtonView();
    ~WorkspaceButtonView() override;

    void draw(cairo_t* cr, const miqu::Rect& bounds) override;
    miqu::Size measure_size() const override;

    bool on_mouse_move(int lx, int ly, const miqu::Rect& bounds) override;
    bool on_mouse_button(int lx, int ly, miqu::MouseButton button, bool pressed, const miqu::Rect& bounds) override;
    bool on_scroll(double delta) override;

    void show_flyout();
    void hide_flyout();

private:
    void sync_workspaces();
    void cycle_workspace(int delta);

    bool m_hovered = false;
    bool m_pressed = false;
    size_t m_active_id = 1;
    std::vector<miqu::WorkspaceInfo> m_workspaces;

    std::shared_ptr<miqu::Window> m_flyout_window;
};

} // namespace miqubar
