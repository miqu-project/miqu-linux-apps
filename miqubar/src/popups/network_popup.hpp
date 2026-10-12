#pragma once

#include "miqutoolkit/view/card_view.hpp"
#include "miqutoolkit/view/linear_layout.hpp"
#include "miqutoolkit/view/button.hpp"
#include "miqutoolkit/view/switch.hpp"
#include "miqutoolkit/view/text_view.hpp"
#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace miqubar {

struct WifiNetworkEntry {
    std::string ssid;
    int signal = 0; // 0 - 100
    bool is_active = false;
    bool is_secured = false;
};

class NetworkPopupView : public miqu::CardView {
public:
    explicit NetworkPopupView(std::function<void()> on_close);
    ~NetworkPopupView() override = default;

    miqu::Size measure_size() const override;

private:
    void setup_ui();
    void scan_networks();
    void set_wifi_enabled(bool enabled);

    bool m_wifi_enabled = true;
    std::string m_active_connection;
    std::vector<WifiNetworkEntry> m_networks;

    std::shared_ptr<miqu::TextView> m_status_label;
    std::shared_ptr<miqu::Switch> m_switch;
    std::shared_ptr<miqu::LinearLayout> m_network_list;

    std::function<void()> m_on_close;
};

} // namespace miqubar
