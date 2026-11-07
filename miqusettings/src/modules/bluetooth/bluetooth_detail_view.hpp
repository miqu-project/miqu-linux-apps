#pragma once

#include "modules/bluetooth/bluetooth_types.hpp"
#include <miqutoolkit/view/frame_layout.hpp>
#include <miqutoolkit/view/scroll_view.hpp>
#include <miqutoolkit/view/linear_layout.hpp>
#include <miqutoolkit/view/card_view.hpp>
#include <miqutoolkit/view/text_view.hpp>
#include <miqutoolkit/view/image_view.hpp>
#include <miqutoolkit/view/button.hpp>
#include <miqutoolkit/view/switch.hpp>
#include <memory>
#include <vector>
#include <string>
namespace miqu {

class BluetoothAgent;
class ModalOverlay;

class BluetoothDetailView : public FrameLayout {
public:
    BluetoothDetailView();
    ~BluetoothDetailView() override;


    void refresh_state();

private:
    void setup_ui();
    void load_devices();
    void start_polling();
    void stop_polling();
    void init_agent();

    std::shared_ptr<View> create_hero_card();
    std::shared_ptr<View> create_paired_section();
    std::shared_ptr<View> create_device_row(BluetoothDeviceItem& dev);
    std::shared_ptr<View> create_available_section();
    std::shared_ptr<View> create_available_row(BluetoothDeviceItem& dev);
    std::shared_ptr<View> create_file_transfer_section();
    std::shared_ptr<View> create_disabled_state();

    void toggle_power(bool power);
    void toggle_connection(const std::string& mac);
    void on_pair_device(const std::string& mac);
    void on_forget_device(const std::string& mac);
    void on_send_file();
    void on_receive_file();
    void toggle_scan();

    bool m_powered = true;
    bool m_scanning = false;
int m_scan_ticks = 0;
    uint64_t m_poll_timer_id = 0;

    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
    std::unique_ptr<BluetoothAgent> m_agent;
    std::shared_ptr<ModalOverlay> m_active_pairing_modal;

    BluetoothAdapterInfo m_adapter_info;
    std::vector<BluetoothDeviceItem> m_paired_devices;
    std::vector<BluetoothDeviceItem> m_available_devices;

    std::shared_ptr<ScrollView> m_scroll;
    std::shared_ptr<LinearLayout> m_content_layout;
};

} // namespace miqu
