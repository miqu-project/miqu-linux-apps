#include "hardware_view.hpp"
#include "ui_components.hpp"
#include <thread>

using namespace miqu;

namespace miqusecure {

static const Color COLOR_ACTIVE_GREEN(0.188f, 0.820f, 0.345f, 1.0f); // #30d158
static const Color COLOR_WARN_AMBER(0.980f, 0.700f, 0.200f, 1.0f);   // #fab387

HardwareView::HardwareView(const HardwareInfo& info, std::function<void()> on_changed)
    : m_info(info), m_on_changed(std::move(on_changed)) {
    set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));

    m_layout = std::make_shared<LinearLayout>(Orientation::Vertical);
    m_layout->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    m_layout->set_padding(24, 16);

    setup_airplane_hero();
    setup_wireless_section();
    setup_sensors_section();

    set_content_view(m_layout);
}

// =============================================================================
// 1. MASTER AIRPLANE MODE HERO INSET CARD
// =============================================================================
void HardwareView::setup_airplane_hero() {
    auto card = CardViewBuilder::create()
        ->style(CardStyle::Outlined)
        ->padding(18, 14)
        ->build();
    card->set_margin(0, 4, 0, 10);

    auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));

    // 8px indicator dot
    m_status_dot = std::make_shared<FrameLayout>();
    m_status_dot->set_layout_params(LayoutParams(8, 8, Gravity::CenterVertical));
    m_status_dot->set_corner_radius(4);
    m_status_dot->set_margin(0, 0, 14, 0);
    update_status_indicator(m_info.airplane_mode);
    row->add_view(m_status_dot);

    // Text column
    auto text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    text_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

    m_airplane_lbl = TextViewBuilder::create()
        ->text(m_info.airplane_mode ? "Airplane Mode is Active" : "Hardware Radios Active")
        ->h3()
        ->bold(true)
        ->build();
    m_airplane_lbl->set_margin(0, 0, 0, 3);

    std::string air_desc = m_info.airplane_mode ?
        "All wireless transmitters (Wi-Fi, Bluetooth, and cellular) are powered down." :
        "Wireless transmitters and hardware sensors operating normally.";

    m_airplane_desc = TextViewBuilder::create()
        ->text(air_desc)
        ->caption()
        ->muted()
        ->multiline(true)
        ->ellipsize(false)
        ->build();

    text_col->add_view(m_airplane_lbl);
    text_col->add_view(m_airplane_desc);
    row->add_view(text_col);

    // Master airplane switch
    m_switch_airplane = SwitchBuilder::create()
        ->checked(m_info.airplane_mode)
        ->build();
    m_switch_airplane->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));

    std::weak_ptr<Switch> weak_air = m_switch_airplane;
    m_switch_airplane->set_on_checked_changed_listener([this, weak_air](bool checked) {
        std::thread([this, checked, weak_air]() {
            bool ok = SecurityBackend::set_airplane_mode(checked);
            if (auto engine = AppEngine::instance()) {
                engine->post([this, checked, ok, weak_air]() {
                    if (ok) {
                        if (m_on_changed) m_on_changed();
                    } else {
                        if (auto s = weak_air.lock()) {
                            s->set_checked(!checked);
                        }
                    }
                });
            }
        }).detach();
    });
    row->add_view(m_switch_airplane);
    card->add_view(row);
    m_layout->add_view(card);
}

// =============================================================================
// 2. WIRELESS TRANSMITTERS INSET CARD
// =============================================================================
void HardwareView::setup_wireless_section() {
    auto sec_hdr = ui::make_section_header("WIRELESS TRANSMITTERS");
    m_layout->add_view(sec_hdr);

    auto card = CardViewBuilder::create()
        ->style(CardStyle::Outlined)
        ->padding(18, 6)
        ->build();
    card->set_margin(0, 0, 0, 10);

    auto box = std::make_shared<LinearLayout>(Orientation::Vertical);
    box->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    // Wi-Fi Row
    auto wifi_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    wifi_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    wifi_row->set_padding(0, 10);

    auto wifi_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    wifi_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

    auto wifi_title = TextViewBuilder::create()->text("Wi-Fi Wireless Transmitter")->bold(true)->build();
    auto wifi_desc = TextViewBuilder::create()
        ->text("Powers down network antenna and disconnects from active wireless networks")
        ->caption()
        ->muted()
        ->multiline(true)
        ->ellipsize(false)
        ->build();
    wifi_desc->set_margin(0, 2, 0, 0);

    wifi_col->add_view(wifi_title);
    wifi_col->add_view(wifi_desc);
    wifi_row->add_view(wifi_col);

    m_switch_wifi = SwitchBuilder::create()
        ->checked(m_info.wifi_enabled)
        ->build();
    m_switch_wifi->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));

    std::weak_ptr<Switch> weak_wifi = m_switch_wifi;
    m_switch_wifi->set_on_checked_changed_listener([this, weak_wifi](bool checked) {
        std::thread([this, checked, weak_wifi]() {
            bool ok = SecurityBackend::set_wifi_enabled(checked);
            if (auto engine = AppEngine::instance()) {
                engine->post([this, checked, ok, weak_wifi]() {
                    if (ok) {
                        if (m_on_changed) m_on_changed();
                    } else {
                        if (auto s = weak_wifi.lock()) {
                            s->set_checked(!checked);
                        }
                    }
                });
            }
        }).detach();
    });
    wifi_row->add_view(m_switch_wifi);
    box->add_view(wifi_row);

    box->add_view(DividerViewBuilder::create()->build());

    // Bluetooth Row
    auto bt_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    bt_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    bt_row->set_padding(0, 10);

    auto bt_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    bt_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

    auto bt_title = TextViewBuilder::create()->text("Bluetooth Radio")->bold(true)->build();
    auto bt_desc = TextViewBuilder::create()
        ->text("Disables wireless adapter to prevent beacon tracking and unauthorized pairing")
        ->caption()
        ->muted()
        ->multiline(true)
        ->ellipsize(false)
        ->build();
    bt_desc->set_margin(0, 2, 0, 0);

    bt_col->add_view(bt_title);
    bt_col->add_view(bt_desc);
    bt_row->add_view(bt_col);

    m_switch_bt = SwitchBuilder::create()
        ->checked(m_info.bluetooth_enabled)
        ->build();
    m_switch_bt->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));

    std::weak_ptr<Switch> weak_bt = m_switch_bt;
    m_switch_bt->set_on_checked_changed_listener([this, weak_bt](bool checked) {
        std::thread([this, checked, weak_bt]() {
            bool ok = SecurityBackend::set_bluetooth_enabled(checked);
            if (auto engine = AppEngine::instance()) {
                engine->post([this, checked, ok, weak_bt]() {
                    if (ok) {
                        if (m_on_changed) m_on_changed();
                    } else {
                        if (auto s = weak_bt.lock()) {
                            s->set_checked(!checked);
                        }
                    }
                });
            }
        }).detach();
    });
    bt_row->add_view(m_switch_bt);
    box->add_view(bt_row);

    card->add_view(box);
    m_layout->add_view(card);
}

// =============================================================================
// 3. AUDIO & VISUAL SENSORS INSET CARD
// =============================================================================
void HardwareView::setup_sensors_section() {
    auto sec_hdr = ui::make_section_header("AUDIO & VISUAL PRIVACY SENSORS");
    m_layout->add_view(sec_hdr);

    auto card = CardViewBuilder::create()
        ->style(CardStyle::Outlined)
        ->padding(18, 6)
        ->build();
    card->set_margin(0, 0, 0, 24);

    auto box = std::make_shared<LinearLayout>(Orientation::Vertical);
    box->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    // Microphone Mute Row
    auto mic_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    mic_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    mic_row->set_padding(0, 10);

    auto mic_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    mic_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

    auto mic_title = TextViewBuilder::create()->text("Microphone Hardware Mute")->bold(true)->build();
    auto mic_desc = TextViewBuilder::create()
        ->text("Hardware audio mute preventing background applications from capturing sound")
        ->caption()
        ->muted()
        ->multiline(true)
        ->ellipsize(false)
        ->build();
    mic_desc->set_margin(0, 2, 0, 0);

    mic_col->add_view(mic_title);
    mic_col->add_view(mic_desc);
    mic_row->add_view(mic_col);

    m_switch_mic = SwitchBuilder::create()
        ->checked(m_info.mic_muted)
        ->build();
    m_switch_mic->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));

    std::weak_ptr<Switch> weak_mic = m_switch_mic;
    m_switch_mic->set_on_checked_changed_listener([this, weak_mic](bool checked) {
        std::thread([this, checked, weak_mic]() {
            bool ok = SecurityBackend::set_mic_muted(checked);
            if (auto engine = AppEngine::instance()) {
                engine->post([this, checked, ok, weak_mic]() {
                    if (ok) {
                        if (m_on_changed) m_on_changed();
                    } else {
                        if (auto s = weak_mic.lock()) {
                            s->set_checked(!checked);
                        }
                    }
                });
            }
        }).detach();
    });
    mic_row->add_view(m_switch_mic);
    box->add_view(mic_row);

    box->add_view(DividerViewBuilder::create()->build());

    // Webcam Live Sensor Row
    auto cam_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    cam_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    cam_row->set_padding(0, 10);

    auto cam_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    cam_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

    auto cam_title = TextViewBuilder::create()->text("Webcam Video Sensor Guard")->bold(true)->build();

    std::string cam_str = m_info.camera_in_use ?
        ("Video in use by " + m_info.camera_active_proc) :
        (m_info.camera_detected ? "Camera standby. No applications streaming video." : "No camera hardware detected.");

    m_cam_proc_lbl = TextViewBuilder::create()->text(cam_str)->caption()->muted()->multiline(true)->ellipsize(false)->build();
    m_cam_proc_lbl->set_margin(0, 2, 0, 0);

    cam_col->add_view(cam_title);
    cam_col->add_view(m_cam_proc_lbl);
    cam_row->add_view(cam_col);

    std::string cam_status_txt = m_info.camera_in_use ? "IN USE" : (m_info.camera_detected ? "IDLE" : "OFF");
    auto cam_pill = ui::make_status_pill(cam_status_txt, m_cam_status_badge);
    cam_row->add_view(cam_pill);

    box->add_view(cam_row);
    card->add_view(box);
    m_layout->add_view(card);
}

void HardwareView::update_status_indicator(bool airplane) {
    if (!m_status_dot) return;
    m_status_dot->set_background_color(airplane ? COLOR_WARN_AMBER : COLOR_ACTIVE_GREEN);
    m_status_dot->request_redraw();
}

void HardwareView::update_info(const HardwareInfo& info) {
    m_info = info;

    update_status_indicator(m_info.airplane_mode);

    if (m_airplane_lbl) {
        m_airplane_lbl->set_text(m_info.airplane_mode ? "Airplane Mode is Active" : "Hardware Radios Active");
    }

    if (m_airplane_desc) {
        std::string air_desc = m_info.airplane_mode ?
            "All wireless transmitters (Wi-Fi, Bluetooth, and cellular) are powered down." :
            "Wireless transmitters and hardware sensors operating normally.";
        m_airplane_desc->set_text(air_desc);
    }

    if (m_switch_airplane) {
        m_switch_airplane->set_checked(m_info.airplane_mode);
    }

    if (m_switch_wifi) {
        m_switch_wifi->set_checked(m_info.wifi_enabled);
    }

    if (m_switch_bt) {
        m_switch_bt->set_checked(m_info.bluetooth_enabled);
    }

    if (m_switch_mic) {
        m_switch_mic->set_checked(m_info.mic_muted);
    }

    if (m_cam_proc_lbl) {
        std::string cam_str = m_info.camera_in_use ?
            ("Video in use by " + m_info.camera_active_proc) :
            (m_info.camera_detected ? "Camera standby. No applications streaming video." : "No camera hardware detected.");
        m_cam_proc_lbl->set_text(cam_str);
    }

    if (m_cam_status_badge) {
        m_cam_status_badge->set_text(m_info.camera_in_use ? "IN USE" : (m_info.camera_detected ? "IDLE" : "OFF"));
    }
}

} // namespace miqusecure
