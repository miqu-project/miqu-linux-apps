#include "toast_view.hpp"
#include <miqutoolkit/core/config.hpp>
#include <miqutoolkit/view/divider_view.hpp>
#include <algorithm>

namespace miqunotify {

ToastView::ToastView(
    Notification notif,
    int timeout_ms,
    OnDismissCb on_dismiss,
    OnActionCb on_action
) : m_notif(std::move(notif)),
    m_on_dismiss(std::move(on_dismiss)),
    m_on_action(std::move(on_action)) {

    auto tk = miqu::Config::get();

    // Determine timeout
    if (m_notif.expire_timeout_ms > 0) {
        m_total_timeout_ms = m_notif.expire_timeout_ms;
    } else if (m_notif.expire_timeout_ms == 0) {
        m_total_timeout_ms = 0; // Never expire
    } else {
        m_total_timeout_ms = (m_notif.urgency == Urgency::Critical) ? 0 : timeout_ms;
    }
    m_remaining_ms = m_total_timeout_ms;

    // CardView style
    set_radius(tk->metrics.corner_radius > 0 ? tk->metrics.corner_radius : 12);
    set_card_background_color(tk->colors.surface);
    set_stroke(1, tk->colors.outline.with_alpha(0.25f));
    set_elevation(2);
    set_padding(14);

    // Root vertical layout
    m_content_layout = std::make_shared<miqu::LinearLayout>(miqu::Orientation::Vertical);
    m_content_layout->set_divider_spacing(8);

    // 1. Header (Icon + App Name + Close Button)
    auto header_layout = std::make_shared<miqu::LinearLayout>(miqu::Orientation::Horizontal);
    header_layout->set_gravity(miqu::Gravity::CenterVertical);
    header_layout->set_divider_spacing(8);

    // Icon
    std::string icon_src = !m_notif.app_icon.empty() ? m_notif.app_icon : "dialog-information";
    auto icon_view = std::make_shared<miqu::ImageView>(icon_src);
    icon_view->set_target_size(22);
    header_layout->add_view(icon_view, miqu::LayoutParams(22, 22));

    // App name
    std::string display_name = !m_notif.app_name.empty() ? m_notif.app_name : "Notification";
    auto app_title = std::make_shared<miqu::TextView>(display_name);
    app_title->set_text_size(tk->metrics.caption_size > 0 ? tk->metrics.caption_size : 10);
    app_title->set_text_color(tk->colors.on_surface_variant);
    app_title->set_bold(true);
    header_layout->add_view(app_title, miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::MatchParent), static_cast<int>(miqu::LayoutDimension::WrapContent)));

    // Close button (✕)
    m_close_btn = std::make_shared<miqu::Button>("✕");
    m_close_btn->set_flat(true);
    m_close_btn->set_padding(4, 2);
    m_close_btn->set_text_size(11);
    m_close_btn->set_text_color(tk->colors.on_surface_variant);
    m_close_btn->set_on_click_listener([this]() {
        if (m_on_dismiss) {
            m_on_dismiss(m_notif.id, 2); // 2 = dismissed by user
        }
    });
    header_layout->add_view(m_close_btn, miqu::LayoutParams(24, 24));

    m_content_layout->add_view(header_layout, miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::MatchParent), static_cast<int>(miqu::LayoutDimension::WrapContent)));

    // 2. Summary (Title)
    if (!m_notif.summary.empty()) {
        m_summary_view = std::make_shared<miqu::TextView>(m_notif.summary);
        m_summary_view->set_text_size(tk->metrics.h3_size > 0 ? tk->metrics.h3_size : 13);
        m_summary_view->set_text_color(tk->colors.on_surface);
        m_summary_view->set_bold(true);
        m_content_layout->add_view(m_summary_view, miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::MatchParent), static_cast<int>(miqu::LayoutDimension::WrapContent)));
    }

    // 3. Body
    if (!m_notif.body.empty()) {
        m_body_view = std::make_shared<miqu::TextView>(m_notif.body);
        m_body_view->set_text_size(tk->metrics.font_size > 0 ? tk->metrics.font_size : 11);
        m_body_view->set_text_color(tk->colors.on_surface_variant);
        m_content_layout->add_view(m_body_view, miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::MatchParent), static_cast<int>(miqu::LayoutDimension::WrapContent)));
    }

    // 4. Action Buttons (if present)
    if (!m_notif.actions.empty()) {
        auto actions_layout = std::make_shared<miqu::LinearLayout>(miqu::Orientation::Horizontal);
        actions_layout->set_divider_spacing(8);
        actions_layout->set_margin(0, 4, 0, 0);

        for (const auto& act : m_notif.actions) {
            if (act.key == "default") continue; // default action is triggered by clicking the toast itself

            auto act_btn = std::make_shared<miqu::Button>(act.label);
            act_btn->set_style(miqu::ButtonStyle::Standard);
            act_btn->set_radius(tk->metrics.corner_radius / 2);
            act_btn->set_padding(10, 4);
            act_btn->set_text_size(tk->metrics.caption_size > 0 ? tk->metrics.caption_size : 10);
            act_btn->set_background_color(tk->colors.surface_variant.with_alpha(0.6f));
            act_btn->set_text_color(tk->colors.on_surface);

            std::string action_key = act.key;
            act_btn->set_on_click_listener([this, action_key]() {
                if (m_on_action) {
                    m_on_action(m_notif.id, action_key);
                }
            });
            actions_layout->add_view(act_btn, miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::WrapContent), 28));
        }

        m_content_layout->add_view(actions_layout, miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::MatchParent), static_cast<int>(miqu::LayoutDimension::WrapContent)));
    }

    add_view(m_content_layout, miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::MatchParent), static_cast<int>(miqu::LayoutDimension::WrapContent)));
}

void ToastView::tick(int delta_ms) {
    if (m_total_timeout_ms <= 0) return; // Persistent
    if (m_hovered) return; // Pause timer on hover

    m_remaining_ms -= delta_ms;
    if (m_remaining_ms <= 0) {
        m_remaining_ms = 0;
        if (m_on_dismiss) {
            m_on_dismiss(m_notif.id, 1); // 1 = expired
        }
    }
    request_redraw();
}

bool ToastView::on_mouse_move(int lx, int ly, const miqu::Rect& bounds) {
    bool inside = bounds.contains(lx, ly);
    if (m_hovered != inside) {
        m_hovered = inside;
        request_redraw();
    }
    return CardView::on_mouse_move(lx, ly, bounds);
}

bool ToastView::on_mouse_button(int lx, int ly, miqu::MouseButton button, bool pressed, const miqu::Rect& bounds) {
    if (bounds.contains(lx, ly)) {
        if (button == miqu::MouseButton::Right && pressed) {
            // Right-click dismisses
            if (m_on_dismiss) m_on_dismiss(m_notif.id, 2);
            return true;
        }
        if (button == miqu::MouseButton::Left && pressed) {
            // Check if user clicked children (e.g. close button or action buttons)
            if (CardView::on_mouse_button(lx, ly, button, pressed, bounds)) {
                return true;
            }
            // Otherwise, invoke default action or dismiss
            bool had_default = false;
            for (const auto& act : m_notif.actions) {
                if (act.key == "default") {
                    if (m_on_action) m_on_action(m_notif.id, "default");
                    had_default = true;
                    break;
                }
            }
            if (!had_default && m_on_dismiss) {
                m_on_dismiss(m_notif.id, 2);
            }
            return true;
        }
    }
    return CardView::on_mouse_button(lx, ly, button, pressed, bounds);
}

void ToastView::draw(cairo_t* cr, const miqu::Rect& bounds) {
    CardView::draw(cr, bounds);

    // Draw bottom progress bar if timed notification
    if (m_total_timeout_ms > 0 && m_remaining_ms > 0) {
        double progress = static_cast<double>(m_remaining_ms) / static_cast<double>(m_total_timeout_ms);
        double bar_w = (bounds.width - 24.0) * progress;
        double bar_h = 2.0;
        double bar_x = bounds.x + 12.0;
        double bar_y = bounds.y + bounds.height - 4.0;

        auto tk = miqu::Config::get();
        miqu::Color bar_col = (m_notif.urgency == Urgency::Critical) ? miqu::Color(0.9f, 0.2f, 0.2f, 0.8f) : tk->colors.primary.with_alpha(0.6f);

        cairo_save(cr);
        cairo_set_source_rgba(cr, bar_col.r, bar_col.g, bar_col.b, bar_col.a);
        cairo_rectangle(cr, bar_x, bar_y, bar_w, bar_h);
        cairo_fill(cr);
        cairo_restore(cr);
    }
}

} // namespace miqunotify
