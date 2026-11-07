#pragma once

#include "modules/firewall/firewall_backend.hpp"
#include <miqutoolkit/view/frame_layout.hpp>
#include <miqutoolkit/view/scroll_view.hpp>
#include <miqutoolkit/view/linear_layout.hpp>
#include <miqutoolkit/view/card_view.hpp>
#include <miqutoolkit/view/text_view.hpp>
#include <miqutoolkit/view/image_view.hpp>
#include <miqutoolkit/view/button.hpp>
#include <miqutoolkit/view/switch.hpp>
#include <miqutoolkit/view/spinner.hpp>
#include <memory>
#include <string>
#include <functional>

namespace miqu {

class FirewallDetailView : public FrameLayout {
public:
    FirewallDetailView();
    ~FirewallDetailView() override = default;

    void set_is_fullscreen(bool fullscreen);
    bool is_fullscreen() const { return m_is_fullscreen; }

    void set_on_request_fullscreen(std::function<void()> cb) {
        m_on_request_fullscreen = std::move(cb);
    }

private:
    void setup_ui();

    // UI builder helpers
    std::shared_ptr<View> create_status_banner();
    std::shared_ptr<View> create_hero_preview_card();
    std::shared_ptr<View> create_master_switch_card();
    std::shared_ptr<View> create_traffic_policy_card();
    std::shared_ptr<View> create_profile_presets_section();
    std::shared_ptr<View> create_advanced_rules_card();
    std::shared_ptr<View> create_logs_monitor_card();
    std::shared_ptr<View> create_add_rule_inline_card();

    bool m_is_fullscreen = false;
    bool m_show_add_rule = false;
    bool m_is_busy = false;
    std::string m_status_message;

    std::shared_ptr<ScrollView> m_scroll;
    std::shared_ptr<LinearLayout> m_content_layout;
    std::function<void()> m_on_request_fullscreen;
};

} // namespace miqu
