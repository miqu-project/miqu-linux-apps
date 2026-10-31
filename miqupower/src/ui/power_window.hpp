#pragma once

#include "core/system_power.hpp"
#include "ui/action_card.hpp"
#include <miqutoolkit/core/window.hpp>
#include <miqutoolkit/view/card_view.hpp>
#include <miqutoolkit/view/navigation_view.hpp>
#include <miqutoolkit/view/scroll_view.hpp>
#include <miqutoolkit/view/linear_layout.hpp>
#include <miqutoolkit/view/text_view.hpp>
#include <miqutoolkit/view/progress_bar.hpp>

#include <vector>
#include <memory>
#include <optional>

namespace miqu {

class PowerWindow : public std::enable_shared_from_this<PowerWindow> {
public:
    PowerWindow();
    ~PowerWindow();

    void show(std::optional<PowerAction> direct_action = std::nullopt);
    void dismiss();

private:
    std::shared_ptr<View> create_main_page();
    std::shared_ptr<View> create_warning_page(PowerAction action, const std::vector<AppInfo>& apps);
    std::shared_ptr<View> create_progress_page(PowerAction action);

    void handle_action_selected(PowerAction action);
    void execute_confirmed_action(PowerAction action);

    void select_card_index(int index);
    void navigate_selection(int direction);
    void handle_key_event(const KeyPressEvent& event);

    std::shared_ptr<Window> m_window;
    std::shared_ptr<CardView> m_root_card;
    std::shared_ptr<NavigationView> m_nav_view;

    std::vector<std::shared_ptr<ActionCard>> m_action_cards;
    int m_selected_index = 0;
    PowerAction m_pending_action = PowerAction::Shutdown;

    // Progress page live text views
    std::shared_ptr<TextView> m_progress_title;
    std::shared_ptr<TextView> m_progress_sub;
};

} // namespace miqu
