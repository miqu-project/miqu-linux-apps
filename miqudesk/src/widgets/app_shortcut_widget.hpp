#pragma once

#include "core/widget.hpp"
#include "core/desktop_scanner.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <chrono>

namespace miqudesk {

class AppShortcutWidget : public Widget {
public:
    explicit AppShortcutWidget(DesktopShortcut shortcut);

    std::string get_id() const override { return "shortcut_" + m_shortcut.id; }
    std::string get_title() const override { return m_shortcut.name; }
    std::shared_ptr<miqu::View> get_view() override { return m_card; }
    bool is_resizable() const override { return false; }
    void on_config_reload() override;

    void launch();

private:
    DesktopShortcut m_shortcut;
    std::shared_ptr<miqu::CardView> m_card;
    std::shared_ptr<miqu::ImageView> m_icon_view;
    std::shared_ptr<miqu::TextView> m_title_view;
    std::chrono::steady_clock::time_point m_last_click_time{};
};

} // namespace miqudesk
