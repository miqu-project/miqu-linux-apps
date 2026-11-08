#pragma once

#include "modules/ethernet/ethernet_types.hpp"
#include <miqutoolkit/view/frame_layout.hpp>
#include <miqutoolkit/view/scroll_view.hpp>
#include <miqutoolkit/view/linear_layout.hpp>
#include <miqutoolkit/view/card_view.hpp>
#include <miqutoolkit/view/text_view.hpp>
#include <miqutoolkit/view/image_view.hpp>
#include <miqutoolkit/view/button.hpp>
#include <miqutoolkit/view/switch.hpp>
#include <memory>
#include <string>
#include <functional>

namespace miqu {

class EthernetDetailView : public FrameLayout {
public:
    EthernetDetailView();
    ~EthernetDetailView() override;

    void set_is_fullscreen(bool fullscreen);
    bool is_fullscreen() const { return m_is_fullscreen; }

    void set_on_request_fullscreen(std::function<void()> cb) {
        m_on_request_fullscreen = std::move(cb);
    }

    void refresh_state();

private:
    void setup_ui();
    void load_data();

    std::shared_ptr<View> create_hero_card();
    std::shared_ptr<View> create_action_row();
    std::shared_ptr<View> create_telemetry_section();
    std::shared_ptr<View> create_telemetry_row(
        const std::string& icon_name,
        const std::string& title,
        const std::string& value,
        const std::string& subvalue = "",
        const std::string& copy_text = ""
    );

    void show_ip_config_modal();
    void toggle_power(bool power);

    bool m_is_fullscreen = false;
    EthernetDevice m_device;

    std::shared_ptr<ScrollView> m_scroll;
    std::shared_ptr<LinearLayout> m_content_layout;
    std::shared_ptr<View> m_active_modal;
    std::function<void()> m_on_request_fullscreen;
};

} // namespace miqu
