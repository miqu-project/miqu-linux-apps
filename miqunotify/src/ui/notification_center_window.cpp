#include "notification_center_window.hpp"
#include "../config/notify_config.hpp"
#include <miqutoolkit/core/config.hpp>
#include <miqutoolkit/view/divider_view.hpp>
#include <miqutoolkit/view/image_view.hpp>
#include <chrono>
#include <algorithm>
#include <unordered_map>

namespace miqunotify {

static std::string format_relative_time(int64_t timestamp_sec) {
    auto now = std::chrono::system_clock::now();
    int64_t now_sec = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    int64_t diff = std::max<int64_t>(0, now_sec - timestamp_sec);

    if (diff < 60) return "Just now";
    if (diff < 3600) return std::to_string(diff / 60) + "m ago";
    if (diff < 86400) return std::to_string(diff / 3600) + "h ago";
    return std::to_string(diff / 86400) + "d ago";
}

NotificationCenterWindow::NotificationCenterWindow(
    OnClearAllCb on_clear_all,
    OnToggleDndCb on_toggle_dnd,
    OnDismissCb on_dismiss,
    OnActionCb on_action,
    OnOpenCb on_open
) : m_on_clear_all(std::move(on_clear_all)),
    m_on_toggle_dnd(std::move(on_toggle_dnd)),
    m_on_dismiss(std::move(on_dismiss)),
    m_on_action(std::move(on_action)),
    m_on_open(std::move(on_open)) {}

NotificationCenterWindow::~NotificationCenterWindow() {
    close();
}

void NotificationCenterWindow::toggle() {
    if (is_open()) {
        close();
    } else {
        open();
    }
}

void NotificationCenterWindow::open() {
    if (is_open()) return;
    create_window();
    if (m_on_open) {
        m_on_open();
    }
}

void NotificationCenterWindow::close() {
    if (m_window) {
        m_window->close();
        m_window.reset();
        m_root_card.reset();
        m_history_list.reset();
        m_scroll_view.reset();
        m_badge_text.reset();
        m_dnd_btn.reset();
    }
}

void NotificationCenterWindow::refresh_history(const std::vector<Notification>& history, bool dnd_active) {
    m_cached_history = history;
    m_dnd_active = dnd_active;

    if (!is_open()) return;

    auto tk = miqu::Config::get();

    // Update unread count badge
    if (m_badge_text) {
        size_t total = m_cached_history.size();
        size_t unread = std::count_if(m_cached_history.begin(), m_cached_history.end(), [](const auto& n) {
            return !n.is_read;
        });
        if (total == 0) {
            m_badge_text->set_text("0 notifications");
        } else if (unread > 0) {
            m_badge_text->set_text(std::to_string(total) + " notifications (" + std::to_string(unread) + " unread)");
        } else {
            m_badge_text->set_text(std::to_string(total) + " notifications");
        }
    }

    // Update DND button state
    if (m_dnd_btn) {
        m_dnd_btn->set_text(m_dnd_active ? "DND: ON" : "DND: OFF");
        if (m_dnd_active) {
            m_dnd_btn->set_background_color(tk->colors.primary.with_alpha(0.35f));
            m_dnd_btn->set_text_color(tk->colors.primary);
        } else {
            m_dnd_btn->set_background_color(tk->colors.surface_variant.with_alpha(0.5f));
            m_dnd_btn->set_text_color(tk->colors.on_surface_variant);
        }
    }

    // Rebuild history list
    if (m_history_list) {
        m_history_list->clear_views();

        if (m_cached_history.empty()) {
            auto empty_box = std::make_shared<miqu::LinearLayout>(miqu::Orientation::Vertical);
            empty_box->set_gravity(miqu::Gravity::Center);
            empty_box->set_padding(32);
            empty_box->set_divider_spacing(12);

            auto empty_icon = std::make_shared<miqu::ImageView>("dialog-information");
            empty_icon->set_target_size(48);
            empty_box->add_view(empty_icon, miqu::LayoutParams(48, 48));

            auto empty_title = std::make_shared<miqu::TextView>("No Notifications");
            empty_title->set_text_size(tk->metrics.h3_size > 0 ? tk->metrics.h3_size : 14);
            empty_title->set_bold(true);
            empty_title->set_text_color(tk->colors.on_surface);
            empty_box->add_view(empty_title, miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::WrapContent), static_cast<int>(miqu::LayoutDimension::WrapContent)));

            auto empty_sub = std::make_shared<miqu::TextView>("You're all caught up!");
            empty_sub->set_text_size(tk->metrics.font_size > 0 ? tk->metrics.font_size : 11);
            empty_sub->set_text_color(tk->colors.on_surface_variant);
            empty_box->add_view(empty_sub, miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::WrapContent), static_cast<int>(miqu::LayoutDimension::WrapContent)));

            m_history_list->add_view(empty_box, miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::MatchParent), static_cast<int>(miqu::LayoutDimension::MatchParent)));
        } else if (NotifyConfig::get().group_by_app) {
            std::vector<std::string> app_order;
            std::unordered_map<std::string, std::vector<Notification>> app_groups;
            for (auto it = m_cached_history.rbegin(); it != m_cached_history.rend(); ++it) {
                std::string name = !it->app_name.empty() ? it->app_name : "Other";
                if (app_groups.find(name) == app_groups.end()) {
                    app_order.push_back(name);
                }
                app_groups[name].push_back(*it);
            }
            for (const auto& app_name : app_order) {
                auto group_header = std::make_shared<miqu::TextView>(app_name + " (" + std::to_string(app_groups[app_name].size()) + ")");
                group_header->set_text_size(tk->metrics.caption_size > 0 ? tk->metrics.caption_size : 10);
                group_header->set_bold(true);
                group_header->set_text_color(tk->colors.primary);
                group_header->set_margin(4, 4, 4, 0);
                m_history_list->add_view(group_header, miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::MatchParent), static_cast<int>(miqu::LayoutDimension::WrapContent)));

                for (const auto& notif : app_groups[app_name]) {
                    m_history_list->add_view(create_history_item_view(notif), miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::MatchParent), static_cast<int>(miqu::LayoutDimension::WrapContent)));
                }
            }
        } else {
            // Newest on top
            for (auto it = m_cached_history.rbegin(); it != m_cached_history.rend(); ++it) {
                m_history_list->add_view(create_history_item_view(*it), miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::MatchParent), static_cast<int>(miqu::LayoutDimension::WrapContent)));
            }
        }
    }

    if (m_window) {
        m_window->schedule_redraw();
    }
}

std::shared_ptr<miqu::View> NotificationCenterWindow::create_history_item_view(const Notification& notif) {
    auto tk = miqu::Config::get();

    auto card = std::make_shared<miqu::CardView>();
    card->set_radius(tk->metrics.corner_radius > 0 ? tk->metrics.corner_radius : 10);
    if (!notif.is_read) {
        card->set_card_background_color(tk->colors.surface_variant.with_alpha(0.8f));
        card->set_stroke(1, tk->colors.primary.with_alpha(0.4f));
    } else {
        card->set_card_background_color(tk->colors.surface_variant.with_alpha(0.45f));
        card->set_stroke(1, tk->colors.outline.with_alpha(0.15f));
    }
    card->set_padding(12);

    auto box = std::make_shared<miqu::LinearLayout>(miqu::Orientation::Vertical);
    box->set_divider_spacing(6);

    // Header (Icon + Name + Time + Dismiss)
    auto header = std::make_shared<miqu::LinearLayout>(miqu::Orientation::Horizontal);
    header->set_gravity(miqu::Gravity::CenterVertical);
    header->set_divider_spacing(8);

    std::string icon_src = !notif.app_icon.empty() ? notif.app_icon : "dialog-information";
    auto icon_view = std::make_shared<miqu::ImageView>(icon_src);
    icon_view->set_target_size(20);
    header->add_view(icon_view, miqu::LayoutParams(20, 20));

    std::string title_str = !notif.app_name.empty() ? notif.app_name : "Notification";
    if (!notif.is_read) {
        title_str += "  ●";
    }
    auto app_title = std::make_shared<miqu::TextView>(title_str);
    app_title->set_text_size(tk->metrics.caption_size > 0 ? tk->metrics.caption_size : 10);
    app_title->set_bold(true);
    app_title->set_text_color(!notif.is_read ? tk->colors.primary : tk->colors.on_surface);
    header->add_view(app_title, miqu::LayoutParams(0, static_cast<int>(miqu::LayoutDimension::WrapContent), 1.0f));

    auto time_text = std::make_shared<miqu::TextView>(format_relative_time(notif.timestamp_sec));
    time_text->set_text_size(tk->metrics.caption_size > 0 ? tk->metrics.caption_size : 9);
    time_text->set_text_color(tk->colors.on_surface_variant);
    header->add_view(time_text, miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::WrapContent), static_cast<int>(miqu::LayoutDimension::WrapContent)));

    auto dismiss_btn = std::make_shared<miqu::Button>("✕");
    dismiss_btn->set_flat(true);
    dismiss_btn->set_padding(4, 2);
    dismiss_btn->set_text_size(10);
    dismiss_btn->set_text_color(tk->colors.on_surface_variant);
    uint32_t notif_id = notif.id;
    dismiss_btn->set_on_click_listener([this, notif_id]() {
        if (m_on_dismiss) m_on_dismiss(notif_id);
    });
    header->add_view(dismiss_btn, miqu::LayoutParams(20, 20));

    box->add_view(header, miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::MatchParent), static_cast<int>(miqu::LayoutDimension::WrapContent)));

    // Summary
    if (!notif.summary.empty()) {
        auto sum_view = std::make_shared<miqu::TextView>(notif.summary);
        sum_view->set_text_size(tk->metrics.h3_size > 0 ? tk->metrics.h3_size : 12);
        sum_view->set_bold(true);
        sum_view->set_text_color(tk->colors.on_surface);
        box->add_view(sum_view, miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::MatchParent), static_cast<int>(miqu::LayoutDimension::WrapContent)));
    }

    // Body
    if (!notif.body.empty()) {
        auto body_view = std::make_shared<miqu::TextView>(notif.body);
        body_view->set_text_size(tk->metrics.font_size > 0 ? tk->metrics.font_size : 11);
        body_view->set_text_color(tk->colors.on_surface_variant);
        box->add_view(body_view, miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::MatchParent), static_cast<int>(miqu::LayoutDimension::WrapContent)));
    }

    // Action buttons
    if (!notif.actions.empty()) {
        auto actions_row = std::make_shared<miqu::LinearLayout>(miqu::Orientation::Horizontal);
        actions_row->set_divider_spacing(6);
        actions_row->set_margin(0, 4, 0, 0);

        for (const auto& act : notif.actions) {
            if (act.key == "default") continue;

            auto btn = std::make_shared<miqu::Button>(act.label);
            btn->set_style(miqu::ButtonStyle::Standard);
            btn->set_radius(tk->metrics.corner_radius / 2);
            btn->set_padding(8, 2);
            btn->set_text_size(tk->metrics.caption_size > 0 ? tk->metrics.caption_size : 10);
            btn->set_background_color(tk->colors.surface_variant.with_alpha(0.8f));
            btn->set_text_color(tk->colors.on_surface);

            std::string key = act.key;
            btn->set_on_click_listener([this, notif_id, key]() {
                if (m_on_action) m_on_action(notif_id, key);
            });
            actions_row->add_view(btn, miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::WrapContent), 24));
        }
        box->add_view(actions_row, miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::MatchParent), static_cast<int>(miqu::LayoutDimension::WrapContent)));
    }

    card->add_view(box, miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::MatchParent), static_cast<int>(miqu::LayoutDimension::WrapContent)));
    return card;
}

void NotificationCenterWindow::create_window() {
    const auto& cfg = NotifyConfig::get();
    auto tk = miqu::Config::get();

    m_root_card = std::make_shared<miqu::CardView>();
    m_root_card->set_style(miqu::CardStyle::Outlined);
    m_root_card->set_radius(tk->metrics.corner_radius);
    m_root_card->set_elevation(0);

    auto root_layout = std::make_shared<miqu::LinearLayout>(miqu::Orientation::Vertical);
    root_layout->set_padding(16, 16, 16, 16);
    root_layout->set_divider_spacing(12);

    // Top Header
    auto top_header = std::make_shared<miqu::LinearLayout>(miqu::Orientation::Horizontal);
    top_header->set_gravity(miqu::Gravity::CenterVertical);
    top_header->set_divider_spacing(8);

    auto title = std::make_shared<miqu::TextView>("Notifications");
    title->set_text_size(tk->metrics.h2_size > 0 ? tk->metrics.h2_size : 16);
    title->set_bold(true);
    title->set_text_color(tk->colors.on_surface);
    top_header->add_view(title, miqu::LayoutParams(0, static_cast<int>(miqu::LayoutDimension::WrapContent), 1.0f));

    // DND Toggle Button
    m_dnd_btn = std::make_shared<miqu::Button>(m_dnd_active ? "DND: ON" : "DND: OFF");
    m_dnd_btn->set_style(miqu::ButtonStyle::Standard);
    m_dnd_btn->set_radius(tk->metrics.corner_radius / 2);
    m_dnd_btn->set_padding(8, 4);
    m_dnd_btn->set_text_size(tk->metrics.caption_size > 0 ? tk->metrics.caption_size : 10);
    m_dnd_btn->set_background_color(m_dnd_active ? tk->colors.primary.with_alpha(0.35f) : tk->colors.surface_variant.with_alpha(0.5f));
    m_dnd_btn->set_text_color(m_dnd_active ? tk->colors.primary : tk->colors.on_surface_variant);
    m_dnd_btn->set_on_click_listener([this]() {
        if (m_on_toggle_dnd) {
            m_on_toggle_dnd(!m_dnd_active);
        }
    });
    top_header->add_view(m_dnd_btn, miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::WrapContent), 28));

    // Clear All Button
    auto clear_btn = std::make_shared<miqu::Button>("Clear All");
    clear_btn->set_style(miqu::ButtonStyle::Standard);
    clear_btn->set_radius(tk->metrics.corner_radius / 2);
    clear_btn->set_padding(8, 4);
    clear_btn->set_text_size(tk->metrics.caption_size > 0 ? tk->metrics.caption_size : 10);
    clear_btn->set_background_color(tk->colors.surface_variant.with_alpha(0.5f));
    clear_btn->set_text_color(tk->colors.on_surface_variant);
    clear_btn->set_on_click_listener([this]() {
        if (m_on_clear_all) m_on_clear_all();
    });
    top_header->add_view(clear_btn, miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::WrapContent), 28));

    // Close Button
    auto close_btn = std::make_shared<miqu::Button>("✕");
    close_btn->set_flat(true);
    close_btn->set_padding(4, 2);
    close_btn->set_text_size(12);
    close_btn->set_text_color(tk->colors.on_surface_variant);
    close_btn->set_on_click_listener([this]() {
        close();
    });
    top_header->add_view(close_btn, miqu::LayoutParams(24, 24));

    root_layout->add_view(top_header, miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::MatchParent), static_cast<int>(miqu::LayoutDimension::WrapContent)));

    // Subtitle badge
    m_badge_text = std::make_shared<miqu::TextView>(m_cached_history.empty() ? "0 notifications" : std::to_string(m_cached_history.size()) + " unread");
    m_badge_text->set_text_size(tk->metrics.caption_size > 0 ? tk->metrics.caption_size : 10);
    m_badge_text->set_text_color(tk->colors.on_surface_variant);
    root_layout->add_view(m_badge_text, miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::MatchParent), static_cast<int>(miqu::LayoutDimension::WrapContent)));

    // Divider
    auto div = std::make_shared<miqu::DividerView>();
    div->set_color(tk->colors.outline.with_alpha(0.2f));
    root_layout->add_view(div, miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::MatchParent), 1));

    // Scrollable History List
    m_history_list = std::make_shared<miqu::LinearLayout>(miqu::Orientation::Vertical);
    m_history_list->set_divider_spacing(8);

    m_scroll_view = std::make_shared<miqu::ScrollView>(m_history_list);
    root_layout->add_view(m_scroll_view, miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::MatchParent), 0, 1.0f));

    m_root_card->add_view(root_layout, miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::MatchParent), static_cast<int>(miqu::LayoutDimension::MatchParent)));

    // Anchors: Top (1) | Bottom (2) | Right (8) = 11
    uint32_t anchors = 1 | 2 | 8;

    m_window = miqu::WindowBuilder::create()
        ->role(miqu::WindowRole::LayerTop)
        ->appId("miqunotify")
        ->layerNamespace("miqunotify-center")
        ->anchors(anchors)
        ->exclusiveZone(0)
        ->transparent(true)
        ->keyboardInteractive(true)
        ->closeOnClickOutside(true)
        ->closeOnEscape(true)
        ->preferredSize(cfg.center_width, 0)
        ->contentView(m_root_card)
        ->onClose([this]() {
            m_window.reset();
        })
        ->build();

    if (m_window) {
        m_window->show();
        refresh_history(m_cached_history, m_dnd_active);
    }
}

} // namespace miqunotify
