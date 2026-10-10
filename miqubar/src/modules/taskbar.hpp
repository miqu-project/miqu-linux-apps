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

class TaskbarView : public miqu::LinearLayout {
public:
    TaskbarView();
    ~TaskbarView() override = default;

    void sync_windows();

private:
    void show_context_menu(const miqu::WindowInfo& win, miqu::View* anchor);
    std::string resolve_window_icon(const miqu::WindowInfo& win);

    std::vector<miqu::WindowInfo> m_windows;
    std::unordered_map<std::string, std::string> m_icon_path_cache;
};

} // namespace miqubar
