#include "power_popup.hpp"
#include "../config/bar_config.hpp"
#include "miqutoolkit/core/config.hpp"
#include "miqutoolkit/system/app_manager.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>

using namespace miqu;

namespace miqubar {

PowerPopupView::PowerPopupView(std::function<void()> on_close)
    : m_on_close(std::move(on_close)) {
    setup_ui();
}

void PowerPopupView::setup_ui() {
    auto config = Config::get();

    set_style(CardStyle::Outlined);
    set_radius(config->metrics.corner_radius > 0 ? config->metrics.corner_radius + 4 : 12);
    set_elevation(0);

    auto root = std::make_shared<LinearLayout>(Orientation::Vertical);
    root->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    root->set_padding(18, 16, 18, 16);

    int base_font_size = config->metrics.font_size > 0 ? config->metrics.font_size : 11;
    int h2_font_size = config->metrics.h2_size > 0 ? config->metrics.h2_size : 15;

    // 1. Header row: Title + Expand Button
    auto header = std::make_shared<LinearLayout>(Orientation::Horizontal);
    header->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    header->set_gravity(Gravity::CenterVertical);
    header->set_margin(0, 0, 0, 12);

    auto title = TextViewBuilder::create()
        ->text("Session & Power")
        ->bold(true)
        ->textSize(h2_font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    title->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));
    header->add_view(title);

    auto expand_btn = ButtonBuilder::create()
        ->icon("view-fullscreen-symbolic")
        ->iconSize(16)
        ->build();
    expand_btn->set_flat(true);
    expand_btn->set_padding(4);
    expand_btn->set_layout_params(LayoutParams(30, 30));
    expand_btn->set_tooltip("Open full power dialog (miqupower)");
    expand_btn->set_on_click_listener([this]() {
        open_full_menu();
    });
    header->add_view(expand_btn);

    root->add_view(header);

    // 2. Action list
    auto action_list = std::make_shared<LinearLayout>(Orientation::Vertical);
    action_list->set_divider_spacing(4);
    action_list->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    struct ActionItem {
        std::string label;
        std::string icon;
        std::string action;
    };

    const std::vector<ActionItem> actions = {
        {"Shut Down", "system-shutdown-symbolic", "shutdown"},
        {"Restart", "system-reboot-symbolic", "reboot"},
        {"Suspend", "system-suspend-symbolic", "suspend"},
        {"Lock Screen", "system-lock-screen-symbolic", "lock"},
        {"Log Out", "system-log-out-symbolic", "logout"}
    };

    for (const auto& item : actions) {
        auto btn = ButtonBuilder::create()
            ->text(item.label)
            ->icon(item.icon)
            ->iconSize(18)
            ->textSize(base_font_size + 1)
            ->build();
        btn->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            38
        ));
        btn->set_content_gravity(Gravity::Left | Gravity::CenterVertical);
        btn->set_padding(12, 6);
        btn->set_flat(true);
        btn->set_on_click_listener([this, act = item.action]() {
            trigger_action(act);
        });
        action_list->add_view(btn);
    }

    root->add_view(action_list);
    add_view(root);
}

void PowerPopupView::trigger_action(const std::string& action_name) {
    if (m_on_close) m_on_close();

    if (action_name == "lock") {
        miqu::AppManager::launch_command("miqulock &");
    } else if (action_name == "suspend") {
        miqu::AppManager::launch_command("systemctl suspend &");
    } else if (action_name == "shutdown") {
        miqu::AppManager::launch_command("miqupower --shutdown");
    } else if (action_name == "reboot") {
        miqu::AppManager::launch_command("miqupower --reboot");
    } else if (action_name == "logout") {
        miqu::AppManager::launch_command("miqupower --logout");
    }
}

void PowerPopupView::open_full_menu() {
    if (m_on_close) m_on_close();
    miqu::AppManager::launch_command("miqupower");
}

miqu::Size PowerPopupView::measure_size() const {
    return {260, 275};
}

} // namespace miqubar
