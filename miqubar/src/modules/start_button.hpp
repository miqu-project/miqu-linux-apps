#pragma once

#include "miqutoolkit/view/view.hpp"
#include <string>

namespace miqubar {

class StartButtonView : public miqu::View {
public:
    StartButtonView();
    ~StartButtonView() override = default;

    void draw(cairo_t* cr, const miqu::Rect& bounds) override;
    miqu::Size measure_size() const override;

    bool on_mouse_move(int lx, int ly, const miqu::Rect& bounds) override;
    bool on_mouse_button(int lx, int ly, miqu::MouseButton button, bool pressed, const miqu::Rect& bounds) override;

private:
    void launch_menu();

    bool m_hovered = false;
    bool m_pressed = false;
};

} // namespace miqubar
