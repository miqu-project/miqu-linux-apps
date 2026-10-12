#include "bluetooth_popup.hpp"
#include "../config/bar_config.hpp"
#include "miqutoolkit/core/config.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <sstream>
#include <iostream>

using namespace miqu;

namespace miqubar {

BluetoothPopupView::BluetoothPopupView(std::function<void()> on_close)
    : m_on_close(std::move(on_close)) {
    scan_devices();
    setup_ui();
}

void BluetoothPopupView::scan_devices() {
    m_devices.clear();
    m_connected_device_name.clear();
    m_powered = false;

    FILE* p_fp = popen("bluetoothctl show 2>/dev/null", "r");
    if (p_fp) {
        char buf[256];
        while (fgets(buf, sizeof(buf), p_fp)) {
            std::string line(buf);
            if (line.find("Powered: yes") != std::string::npos) {
                m_powered = true;
                break;
            }
        }
        pclose(p_fp);
    }

    if (!m_powered) return;

    std::string connected_mac;
    FILE* c_fp = popen("bluetoothctl devices Connected 2>/dev/null", "r");
    if (c_fp) {
        char buf[256];
        while (fgets(buf, sizeof(buf), c_fp)) {
            std::string line(buf);
            if (!line.empty() && line.back() == '\n') line.pop_back();

            size_t p = line.find("Device ");
            if (p != std::string::npos) {
                size_t p2 = line.find(' ', p + 7);
                if (p2 != std::string::npos) {
                    connected_mac = line.substr(p + 7, p2 - (p + 7));
                    m_connected_device_name = line.substr(p2 + 1);
                    BluetoothDeviceEntry e;
                    e.mac = connected_mac;
                    e.name = m_connected_device_name;
                    e.is_connected = true;
                    m_devices.push_back(e);
                }
            }
        }
        pclose(c_fp);
    }

    FILE* dev_fp = popen("bluetoothctl devices Paired 2>/dev/null || bluetoothctl devices 2>/dev/null", "r");
    if (dev_fp) {
        char buf[256];
        while (fgets(buf, sizeof(buf), dev_fp)) {
            std::string line(buf);
            if (!line.empty() && line.back() == '\n') line.pop_back();

            size_t p = line.find("Device ");
            if (p != std::string::npos) {
                size_t p2 = line.find(' ', p + 7);
                if (p2 != std::string::npos) {
                    std::string mac = line.substr(p + 7, p2 - (p + 7));
                    std::string name = line.substr(p2 + 1);
                    if (mac != connected_mac) {
                        BluetoothDeviceEntry e;
                        e.mac = mac;
                        e.name = name;
                        e.is_connected = false;
                        m_devices.push_back(e);
                    }
                }
            }
        }
        pclose(dev_fp);
    }
}

void BluetoothPopupView::setup_ui() {
    auto config = Config::get();

    set_style(CardStyle::Outlined);
    set_radius(config->metrics.corner_radius > 0 ? config->metrics.corner_radius + 4 : 12);
    set_elevation(8);

    auto root = std::make_shared<LinearLayout>(Orientation::Vertical);
    root->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    root->set_padding(20, 20, 20, 20);

    int base_font_size = config->metrics.font_size > 0 ? config->metrics.font_size : 11;
    int h2_font_size = config->metrics.h2_size > 0 ? config->metrics.h2_size : 15;

    // Header row: Title + Miqu Switch toggle
    auto header = std::make_shared<LinearLayout>(Orientation::Horizontal);
    header->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    header->set_gravity(Gravity::CenterVertical);
    header->set_margin(0, 0, 0, 12);

    auto title = TextViewBuilder::create()
        ->text("Bluetooth")
        ->bold(true)
        ->textSize(h2_font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    title->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));
    header->add_view(title);

    m_switch = SwitchBuilder::create()
        ->checked(m_powered)
        ->onCheckedChanged([this](bool checked) {
            set_bluetooth_powered(checked);
        })
        ->build();
    m_switch->set_layout_params(LayoutParams(46, 26));
    header->add_view(m_switch);
    root->add_view(header);

    // Status subtitle
    std::string status_text = !m_connected_device_name.empty()
        ? ("Connected to " + m_connected_device_name)
        : (m_powered ? "Paired Devices" : "Bluetooth is Off");
    m_status_label = TextViewBuilder::create()
        ->text(status_text)
        ->caption(true)
        ->muted(true)
        ->build();
    m_status_label->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    m_status_label->set_margin(0, 0, 0, 14);
    root->add_view(m_status_label);

    // Devices list
    if (m_powered) {
        m_device_list = std::make_shared<LinearLayout>(Orientation::Vertical);
        m_device_list->set_divider_spacing(6);
        m_device_list->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));

        int count = 0;
        for (const auto& dev : m_devices) {
            if (count++ >= 6) break;

            std::string icon = dev.is_connected ? "bluetooth-active" : "bluetooth";
            auto btn = ButtonBuilder::create()
                ->text(dev.name)
                ->icon(icon)
                ->textSize(base_font_size + 1)
                ->build();
            btn->set_layout_params(LayoutParams(
                static_cast<int>(LayoutDimension::MatchParent),
                38
            ));
            btn->set_content_gravity(Gravity::Left | Gravity::CenterVertical);
            btn->set_padding(12, 6);
            if (dev.is_connected) {
                btn->set_selected(true);
            } else {
                btn->set_flat(true);
            }
            btn->set_on_click_listener([this, mac = dev.mac, conn = dev.is_connected]() {
                std::string action = conn ? "disconnect" : "connect";
                std::string cmd = "bluetoothctl " + action + " " + mac + " 2>/dev/null &";
                std::system(cmd.c_str());
                if (m_on_close) m_on_close();
            });
            m_device_list->add_view(btn);
        }
        root->add_view(m_device_list);
    }

    add_view(root);
}

void BluetoothPopupView::set_bluetooth_powered(bool powered) {
    m_powered = powered;
    std::string cmd = m_powered ? "bluetoothctl power on 2>/dev/null" : "bluetoothctl power off 2>/dev/null";
    std::system(cmd.c_str());

    scan_devices();
    clear_views();
    setup_ui();
    request_redraw();
}

miqu::Size BluetoothPopupView::measure_size() const {
    if (!m_powered) {
        return {340, 130};
    }
    int rows = std::min(6, static_cast<int>(m_devices.size()));
    int h = 100 + std::max(1, rows) * 44;
    return {340, std::max(180, h)};
}

} // namespace miqubar
