#pragma once

#include <miqutoolkit/miqutoolkit.hpp>
#include <string>
#include <memory>

namespace miqudesk {

class Widget {
public:
    virtual ~Widget() = default;

    virtual std::string get_id() const = 0;
    virtual std::string get_title() const = 0;
    virtual std::shared_ptr<miqu::View> get_view() = 0;
    virtual void tick() {}
    virtual void on_config_reload() {}

    virtual bool is_resizable() const { return true; }

    virtual bool on_mouse_button(int rel_x, int rel_y, miqu::MouseButton button, bool pressed) { return false; }
    virtual bool on_mouse_move(int rel_x, int rel_y) { return false; }
};

} // namespace miqudesk
