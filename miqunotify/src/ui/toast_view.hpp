#pragma once

#include "model/notification.hpp"
#include <miqutoolkit/view/card_view.hpp>
#include <miqutoolkit/view/linear_layout.hpp>
#include <miqutoolkit/view/text_view.hpp>
#include <miqutoolkit/view/image_view.hpp>
#include <miqutoolkit/view/button.hpp>
#include <functional>
#include <memory>

namespace miqunotify {

class ToastView : public miqu::CardView {
public:
    using OnDismissCb = std::function<void(uint32_t id, uint32_t reason)>;
    using OnActionCb = std::function<void(uint32_t id, const std::string& key)>;

    ToastView(
        Notification notif,
        int timeout_ms,
        OnDismissCb on_dismiss,
        OnActionCb on_action
    );

    uint32_t get_id() const { return m_notif.id; }
    void tick(int delta_ms);
    bool is_expired() const { return m_remaining_ms <= 0 && m_total_timeout_ms > 0; }

    bool on_mouse_move(int lx, int ly, const miqu::Rect& bounds) override;
    bool on_mouse_button(int lx, int ly, miqu::MouseButton button, bool pressed, const miqu::Rect& bounds) override;
    void draw(cairo_t* cr, const miqu::Rect& bounds) override;

private:
    Notification m_notif;
    int m_total_timeout_ms = 5000;
    int m_remaining_ms = 5000;
    bool m_hovered = false;

    OnDismissCb m_on_dismiss;
    OnActionCb m_on_action;

    std::shared_ptr<miqu::LinearLayout> m_content_layout;
    std::shared_ptr<miqu::TextView> m_summary_view;
    std::shared_ptr<miqu::TextView> m_body_view;
    std::shared_ptr<miqu::Button> m_close_btn;
};

} // namespace miqunotify
