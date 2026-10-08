#pragma once

#include "backend/security_backend.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <functional>

namespace miqusecure {

class HardwareView : public miqu::ScrollView {
public:
    HardwareView(const HardwareInfo& info, std::function<void()> on_changed);
    ~HardwareView() override = default;

    void update_info(const HardwareInfo& info);

private:
    void setup_airplane_hero();
    void setup_wireless_section();
    void setup_sensors_section();
    void update_status_indicator(bool airplane);

    HardwareInfo m_info;
    std::function<void()> m_on_changed;

    std::shared_ptr<miqu::LinearLayout> m_layout;
    std::shared_ptr<miqu::FrameLayout> m_status_dot;

    // 1. Airplane Master Killswitch
    std::shared_ptr<miqu::TextView> m_airplane_lbl;
    std::shared_ptr<miqu::TextView> m_airplane_desc;
    std::shared_ptr<miqu::Switch> m_switch_airplane;

    // 2. Wireless Radios
    std::shared_ptr<miqu::Switch> m_switch_wifi;
    std::shared_ptr<miqu::Switch> m_switch_bt;

    // 3. Audio & Video Sensors
    std::shared_ptr<miqu::Switch> m_switch_mic;
    std::shared_ptr<miqu::TextView> m_cam_status_badge;
    std::shared_ptr<miqu::TextView> m_cam_proc_lbl;
};

} // namespace miqusecure
