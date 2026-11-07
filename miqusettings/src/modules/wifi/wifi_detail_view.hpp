#pragma once

#include "modules/wifi/wifi_types.hpp"
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
#include <vector>
namespace miqu {

class WifiDetailView : public FrameLayout {
public:
    WifiDetailView();
    ~WifiDetailView() override;


    void refresh_state();

private:
    void setup_ui();
    void load_data();

    std::shared_ptr<View> create_hero_card();
    std::shared_ptr<View> create_connected_section();
    std::shared_ptr<View> create_available_section();
    std::shared_ptr<View> create_network_row(const WifiNetwork& net);
    std::shared_ptr<View> create_disabled_state();

    void show_password_modal(const WifiNetwork& net);
    void toggle_radio(bool enabled);
    void on_connect_clicked(const WifiNetwork& net);
    void on_disconnect_clicked();
    void on_forget_clicked(const std::string& ssid);
    void trigger_scan();

    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
    bool m_scanning = false;

    WifiAdapterInfo m_adapter;
    std::vector<WifiNetwork> m_networks;

    std::shared_ptr<ScrollView> m_scroll;
    std::shared_ptr<LinearLayout> m_content_layout;
    std::shared_ptr<View> m_active_modal;
};

} // namespace miqu
