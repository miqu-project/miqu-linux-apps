#pragma once

#include "widget.hpp"
#include <miqutoolkit/miqutoolkit.hpp>

namespace miqudesk {

class WidgetContainer {
public:
    WidgetContainer(std::shared_ptr<Widget> widget, int x, int y, int width, int height);

    void draw(cairo_t* cr);
    void tick();
    void on_config_reload();

    bool handle_mouse_button(int canvas_x, int canvas_y, miqu::MouseButton button, bool pressed);
    bool handle_mouse_move(int canvas_x, int canvas_y);

    int get_x() const { return m_x; }
    int get_y() const { return m_y; }
    int get_width() const { return m_width; }
    int get_height() const { return m_height; }

    void set_position(int x, int y) { m_x = x; m_y = y; }
    void set_size(int w, int h) { m_width = w; m_height = h; }

    bool contains(int canvas_x, int canvas_y) const;
    bool is_dragging() const { return m_is_dragging; }
    bool is_resizing() const { return m_is_resizing; }

    std::shared_ptr<Widget> get_widget() const { return m_widget; }
    std::shared_ptr<miqu::ResizableContainer> get_container_view() const { return m_container; }

private:
    std::shared_ptr<Widget> m_widget;
    std::shared_ptr<miqu::ResizableContainer> m_container;

    int m_x = 0;
    int m_y = 0;
    int m_width = 300;
    int m_height = 150;

    bool m_mouse_down = false;
    bool m_is_dragging = false;
    bool m_is_resizing = false;
    bool m_hover_resize = false;

    int m_drag_start_x = 0;
    int m_drag_start_y = 0;
    int m_orig_x = 0;
    int m_orig_y = 0;
    int m_orig_w = 0;
    int m_orig_h = 0;
};

} // namespace miqudesk
