#pragma once

#include "model/notification.hpp"
#include <miqutoolkit/core/window.hpp>
#include <miqutoolkit/view/card_view.hpp>
#include <miqutoolkit/view/linear_layout.hpp>
#include <miqutoolkit/view/scroll_view.hpp>
#include <miqutoolkit/view/text_view.hpp>
#include <miqutoolkit/view/button.hpp>
#include <vector>
#include <memory>
#include <functional>

namespace miqunotify {

class NotificationCenterWindow {
public:
    using OnClearAllCb = std::function<void()>;
    using OnToggleDndCb = std::function<void(bool enabled)>;
    using OnDismissCb = std::function<void(uint32_t id)>;
    using OnActionCb = std::function<void(uint32_t id, const std::string& key)>;
    using OnOpenCb = std::function<void()>;

    NotificationCenterWindow(
        OnClearAllCb on_clear_all,
        OnToggleDndCb on_toggle_dnd,
        OnDismissCb on_dismiss,
        OnActionCb on_action,
        OnOpenCb on_open = nullptr
    );
    ~NotificationCenterWindow();

    void toggle();
    void open();
    void close();
    bool is_open() const { return m_window != nullptr; }

    void refresh_history(const std::vector<Notification>& history, bool dnd_active);

private:
    void create_window();
    std::shared_ptr<miqu::View> create_history_item_view(const Notification& notif);

    OnClearAllCb m_on_clear_all;
    OnToggleDndCb m_on_toggle_dnd;
    OnDismissCb m_on_dismiss;
    OnActionCb m_on_action;
    OnOpenCb m_on_open;

    std::shared_ptr<miqu::Window> m_window;
    std::shared_ptr<miqu::CardView> m_root_card;
    std::shared_ptr<miqu::TextView> m_badge_text;
    std::shared_ptr<miqu::Button> m_dnd_btn;
    std::shared_ptr<miqu::LinearLayout> m_history_list;
    std::shared_ptr<miqu::ScrollView> m_scroll_view;

    std::vector<Notification> m_cached_history;
    bool m_dnd_active = false;
};

} // namespace miqunotify
