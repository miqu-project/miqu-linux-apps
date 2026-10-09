#pragma once

#include "miqutoolkit/view/view.hpp"
#include "miqutoolkit/system/window_manager.hpp"
#include <vector>
#include <memory>

namespace miqu {
class Window;
}

namespace miqubar {

class TaskbarView : public miqu::View {
public:
    TaskbarView();
    ~TaskbarView() override;

    void draw(cairo_t* cr, const miqu::Rect& bounds) override;
    miqu::Size measure_size() const override;

    bool on_mouse_move(int lx, int ly, const miqu::Rect& bounds) override;
    bool on_mouse_button(int lx, int ly, miqu::MouseButton button, bool pressed, const miqu::Rect& bounds) override;

private:
    void sync_windows();
    void show_context_menu(const miqu::WindowInfo& win, int x, int y);

    std::vector<miqu::WindowInfo> m_windows;
    int m_hovered_index = -1;
    int m_pressed_index = -1;

    std::shared_ptr<miqu::Window> m_context_menu_window;
};

} // namespace miqubar
