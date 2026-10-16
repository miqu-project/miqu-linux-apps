#pragma once

#include "miqutoolkit/view/linear_layout.hpp"
#include "miqutoolkit/system/window_manager.hpp"
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

namespace miqu {
class Window;
class View;
}

namespace miqubar {

class TaskbarItemButton;

class TaskbarView : public miqu::LinearLayout {
public:
    TaskbarView();
    ~TaskbarView() override;

    void sync_windows();
    void hide_context_menu();
    void hide_group_popup();

    void on_item_hover(TaskbarItemButton* btn, bool hovered);
    void toggle_group_popup(TaskbarItemButton* btn);

private:
    void show_context_menu(const std::vector<miqu::WindowInfo>& win_group, miqu::View* anchor);
    void show_group_popup(TaskbarItemButton* anchor);
    std::string resolve_window_icon(const miqu::WindowInfo& win);

    std::vector<miqu::WindowInfo> m_windows;
    std::unordered_map<std::string, std::string> m_icon_path_cache;
    std::shared_ptr<miqu::Window> m_menu_window;
    std::shared_ptr<miqu::Window> m_preview_window;

    TaskbarItemButton* m_preview_anchor = nullptr;
    uint32_t m_hover_timer_id = 0;
    uint32_t m_dismiss_timer_id = 0;
};

} // namespace miqubar
