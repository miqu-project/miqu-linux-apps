#include "widget_container.hpp"
#include "desk_config.hpp"
#include <algorithm>

namespace miqudesk {

WidgetContainer::WidgetContainer(std::shared_ptr<Widget> widget, int x, int y, int width, int height)
    : m_widget(std::move(widget)), m_x(x), m_y(y), m_width(width), m_height(height) {
    m_container = std::make_shared<miqu::ResizableContainer>(m_widget ? m_widget->get_view() : nullptr);
    if (m_widget) {
        m_container->set_resizable(m_widget->is_resizable());
    }
}

bool WidgetContainer::contains(int canvas_x, int canvas_y) const {
    return canvas_x >= m_x && canvas_x < (m_x + m_width) &&
           canvas_y >= m_y && canvas_y < (m_y + m_height);
}

void WidgetContainer::tick() {
    if (m_widget) {
        m_widget->tick();
    }
}

void WidgetContainer::on_config_reload() {
    if (m_widget) {
        m_widget->on_config_reload();
        if (m_container) {
            m_container->set_content(m_widget->get_view());
            m_container->set_resizable(m_widget->is_resizable());
        }
    }
}

void WidgetContainer::draw(cairo_t* cr) {
    if (!cr || !m_container) return;

    miqu::Rect bounds(m_x, m_y, m_width, m_height);
    m_container->set_resizable(m_widget && m_widget->is_resizable());
    m_container->set_active(m_is_dragging || m_is_resizing);
    m_container->set_hover_resize(m_is_resizing || m_hover_resize);
    m_container->draw(cr, bounds);
}

bool WidgetContainer::handle_mouse_button(int canvas_x, int canvas_y, miqu::MouseButton button, bool pressed) {
    miqu::Rect bounds(m_x, m_y, m_width, m_height);

    if (pressed) {
        if (!contains(canvas_x, canvas_y)) {
            return false;
        }

        // Only left button initiates drag or resize
        if (button != miqu::MouseButton::Left) {
            if (m_container) {
                return m_container->on_mouse_button(canvas_x, canvas_y, button, true, bounds);
            }
            return false;
        }

        m_mouse_down = true;
        m_is_dragging = false;
        m_drag_start_x = canvas_x;
        m_drag_start_y = canvas_y;
        m_orig_x = m_x;
        m_orig_y = m_y;
        m_orig_w = m_width;
        m_orig_h = m_height;

        // Check for resize grip via toolkit ResizableContainer
        if (m_widget && m_widget->is_resizable() && m_container && m_container->is_in_resize_grip(canvas_x, canvas_y, bounds)) {
            m_is_resizing = true;
        } else {
            m_is_resizing = false;
        }

        return true;
    } else {
        // Button release
        if (m_is_resizing) {
            m_is_resizing = false;
            m_mouse_down = false;
            return true;
        }

        if (m_mouse_down) {
            m_mouse_down = false;
            if (m_is_dragging) {
                m_is_dragging = false;
                return true;
            }

            // Pure click without dragging -> dispatch to container view
            if (m_container) {
                m_container->on_mouse_button(canvas_x, canvas_y, button, true, bounds);
                m_container->on_mouse_button(canvas_x, canvas_y, button, false, bounds);
            }
            return true;
        }

        if (m_container && contains(canvas_x, canvas_y)) {
            return m_container->on_mouse_button(canvas_x, canvas_y, button, false, bounds);
        }
    }
    return false;
}

bool WidgetContainer::handle_mouse_move(int canvas_x, int canvas_y) {
    int snap = DeskConfig::get().grid_snap;
    miqu::Rect bounds(m_x, m_y, m_width, m_height);

    if (m_is_resizing) {
        int new_w = std::max(140, m_orig_w + (canvas_x - m_drag_start_x));
        int new_h = std::max(70, m_orig_h + (canvas_y - m_drag_start_y));
        if (snap > 1) {
            new_w = (new_w / snap) * snap;
            new_h = (new_h / snap) * snap;
        }
        m_width = new_w;
        m_height = new_h;
        return true;
    }

    if (m_mouse_down) {
        int dx = canvas_x - m_drag_start_x;
        int dy = canvas_y - m_drag_start_y;
        if (!m_is_dragging && (std::abs(dx) > 3 || std::abs(dy) > 3)) {
            m_is_dragging = true;
        }

        if (m_is_dragging) {
            int new_x = std::max(0, m_orig_x + dx);
            int new_y = std::max(0, m_orig_y + dy);
            if (snap > 1) {
                new_x = (new_x / snap) * snap;
                new_y = (new_y / snap) * snap;
            }
            m_x = new_x;
            m_y = new_y;
            return true;
        }
    }

    // Hover state over resize corner via toolkit
    bool near_corner = m_widget && m_widget->is_resizable() && m_container &&
                       m_container->is_in_resize_grip(canvas_x, canvas_y, bounds);
    if (near_corner != m_hover_resize) {
        m_hover_resize = near_corner;
        return true;
    }

    if (m_container && contains(canvas_x, canvas_y)) {
        return m_container->on_mouse_move(canvas_x, canvas_y, bounds);
    }
    return false;
}

} // namespace miqudesk
