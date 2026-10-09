#pragma once

#include "miqutoolkit/view/view.hpp"
#include <memory>
#include <functional>

namespace miqubar {

class QuickSettingsPopupView : public miqu::View {
public:
    QuickSettingsPopupView(std::function<void()> on_close);
    ~QuickSettingsPopupView() override = default;

    void draw(cairo_t* cr, const miqu::Rect& bounds) override;
    miqu::Size measure_size() const override;

    bool on_mouse_enter(int lx, int ly);
    bool on_mouse_leave();
    bool on_mouse_move(int lx, int ly, const miqu::Rect& bounds) override;
    bool on_mouse_button(int lx, int ly, miqu::MouseButton button, bool pressed, const miqu::Rect& bounds) override;

private:
    void update_audio_info();
    void set_volume(int pct);

    int m_volume = 70;
    bool m_muted = false;
    bool m_wifi_on = true;
    bool m_bluetooth_on = true;
    bool m_night_light_on = false;

    bool m_dragging_slider = false;
    int m_hovered_tile = -1;
    std::function<void()> m_on_close;
};

} // namespace miqubar
