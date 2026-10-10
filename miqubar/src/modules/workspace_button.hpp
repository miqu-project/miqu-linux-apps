#pragma once

#include "miqutoolkit/view/button.hpp"
#include "miqutoolkit/system/workspace_manager.hpp"
#include <memory>
#include <vector>

namespace miqu {
class Window;
}

namespace miqubar {

class WorkspaceButtonView : public miqu::Button {
public:
    WorkspaceButtonView();
    ~WorkspaceButtonView() override;

    bool on_scroll(double delta) override;

    void show_flyout();
    void hide_flyout();
    void sync_workspaces();

private:
    void cycle_workspace(int delta);

    size_t m_active_id = 1;
    std::vector<miqu::WorkspaceInfo> m_workspaces;
    std::shared_ptr<miqu::Window> m_flyout_window;
};

} // namespace miqubar
