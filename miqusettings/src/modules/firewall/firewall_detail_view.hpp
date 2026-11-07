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
namespace miqu {

class FirewallDetailView : public FrameLayout {
public:
    FirewallDetailView();
    ~FirewallDetailView() override;

private:
    void setup_ui();

    // Streamlined UI builder helpers
    std::shared_ptr<View> create_status_banner();
    std::shared_ptr<View> create_core_settings_card();
    std::shared_ptr<View> create_quick_allow_card();
    std::shared_ptr<View> create_custom_rules_card();
    std::shared_ptr<View> create_logging_card();
    void show_add_rule_modal();

    bool m_is_busy = false;
    std::string m_status_message;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);

    std::shared_ptr<ScrollView> m_scroll;
    std::shared_ptr<LinearLayout> m_content_layout;
};

} // namespace miqu
