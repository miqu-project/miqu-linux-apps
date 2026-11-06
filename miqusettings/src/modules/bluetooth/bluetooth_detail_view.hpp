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
#include <functional>

namespace miqu {

class BluetoothAgent;
class ModalOverlay;

class BluetoothDetailView : public FrameLayout {
public:
    BluetoothDetailView();
    ~BluetoothDetailView() override;

    void set_is_fullscreen(bool fullscreen);
    bool is_fullscreen() const { return m_is_fullscreen; }

    void set_on_request_fullscreen(std::function<void()> cb) {
        m_on_request_fullscreen = std::move(cb);
    }

    void refresh_state();

private:
    void setup_ui();
    void load_devices();
    void start_polling();
    void stop_polling();
    void init_agent();

    std::shared_ptr<View> create_hero_card();
    std::shared_ptr<View> create_action_row();
    std::shared_ptr<View> create_device_section(const std::string& title, const std::vector<BluetoothDeviceItem>& devs, bool is_connected_group);
    std::shared_ptr<View> create_device_row(const BluetoothDeviceItem& dev, bool is_connected_group);
    std::shared_ptr<View> create_available_section();
    std::shared_ptr<View> create_available_row(const BluetoothDeviceItem& dev);
    std::shared_ptr<View> create_file_transfer_card();
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
    bool m_is_fullscreen = false;
    uint64_t m_poll_timer_id = 0;

    std::unique_ptr<BluetoothAgent> m_agent;
    std::shared_ptr<ModalOverlay> m_active_pairing_modal;

    BluetoothAdapterInfo m_adapter_info;
    std::vector<BluetoothDeviceItem> m_paired_devices;
    std::vector<BluetoothDeviceItem> m_available_devices;

    std::shared_ptr<ScrollView> m_scroll;
    std::shared_ptr<LinearLayout> m_content_layout;
    std::function<void()> m_on_request_fullscreen;
};

} // namespace miqu
