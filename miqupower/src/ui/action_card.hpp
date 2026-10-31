#pragma once

#include "core/system_power.hpp"
#include <miqutoolkit/view/card_view.hpp>
#include <miqutoolkit/view/text_view.hpp>
#include <miqutoolkit/view/image_view.hpp>
#include <miqutoolkit/core/config.hpp>

#include <memory>
#include <functional>

namespace miqu {

class ActionCard {
public:
    ActionCard(
        PowerAction action,
        const std::string& title,
        const std::string& icon_name,
        char hotkey,
        const std::string& hotkey_label,
        std::function<void(PowerAction)> on_click
    );

    std::shared_ptr<CardView> get_view() const { return m_card; }
    PowerAction get_action() const { return m_action; }
    char get_hotkey() const { return m_hotkey; }

    void set_selected(bool selected);
    bool is_selected() const { return m_selected; }

    void update_colors();

private:
    PowerAction m_action;
    char m_hotkey;
    bool m_selected = false;
    std::function<void(PowerAction)> m_on_click;

    std::shared_ptr<CardView> m_card;
    std::shared_ptr<ImageView> m_icon_view;
    std::shared_ptr<TextView> m_title_view;
    std::shared_ptr<TextView> m_hotkey_view;
    std::shared_ptr<CardView> m_badge_card;
};

} // namespace miqu
