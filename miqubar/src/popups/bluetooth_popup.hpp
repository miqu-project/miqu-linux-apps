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

struct BluetoothDeviceEntry {
    std::string mac;
    std::string name;
    bool is_connected = false;
};

class BluetoothPopupView : public miqu::CardView {
public:
    explicit BluetoothPopupView(std::function<void()> on_close);
    ~BluetoothPopupView() override = default;

    miqu::Size measure_size() const override;

private:
    void setup_ui();
    void scan_devices();
    void set_bluetooth_powered(bool powered);

    bool m_powered = false;
    std::string m_connected_device_name;
    std::vector<BluetoothDeviceEntry> m_devices;

    std::shared_ptr<miqu::TextView> m_status_label;
    std::shared_ptr<miqu::Switch> m_switch;
    std::shared_ptr<miqu::LinearLayout> m_device_list;

    std::function<void()> m_on_close;
};

} // namespace miqubar
