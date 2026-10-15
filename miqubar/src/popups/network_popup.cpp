#include "network_popup.hpp"
#include "../config/bar_config.hpp"
#include "miqutoolkit/core/config.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <sstream>
#include <iostream>

using namespace miqu;

namespace miqubar {

NetworkPopupView::NetworkPopupView(std::function<void()> on_close)
    : m_on_close(std::move(on_close)) {
    scan_networks();
    setup_ui();
}

void NetworkPopupView::scan_networks() {
    m_networks.clear();
    m_active_connection.clear();
    m_wifi_enabled = true;

    // Check Wi-Fi hardware/radio status
    FILE* rf_fp = popen("nmcli radio wifi 2>/dev/null", "r");
    if (rf_fp) {
        char buf[32];
        if (fgets(buf, sizeof(buf), rf_fp)) {
            std::string s(buf);
            if (s.find("disabled") != std::string::npos) {
                m_wifi_enabled = false;
            }
        }
        pclose(rf_fp);
    }

    if (!m_wifi_enabled) return;

    // List Wi-Fi networks via nmcli without forcing a slow hardware rescan
    FILE* fp = popen("nmcli -t -f IN-USE,SSID,SIGNAL,SECURITY dev wifi list --rescan no 2>/dev/null", "r");
    if (fp) {
        char buf[256];
        while (fgets(buf, sizeof(buf), fp)) {
            std::string line(buf);
            if (!line.empty() && line.back() == '\n') line.pop_back();

            size_t p1 = line.find(':');
            if (p1 == std::string::npos) continue;
            bool active = (line.substr(0, p1) == "*");

            size_t p2 = line.find(':', p1 + 1);
            if (p2 == std::string::npos) continue;
            std::string ssid = line.substr(p1 + 1, p2 - p1 - 1);
            if (ssid.empty()) continue;

            size_t p3 = line.find(':', p2 + 1);
            std::string sig_str = (p3 != std::string::npos) ? line.substr(p2 + 1, p3 - p2 - 1) : line.substr(p2 + 1);
            std::string sec_str = (p3 != std::string::npos) ? line.substr(p3 + 1) : "";

            int signal = 50;
            try {
                signal = std::stoi(sig_str);
            } catch (...) {}

            auto it = std::find_if(m_networks.begin(), m_networks.end(), [&](const WifiNetworkEntry& e) {
                return e.ssid == ssid;
            });
            if (it != m_networks.end()) {
                if (active) {
                    it->is_active = true;
                    it->signal = signal;
                } else if (!it->is_active && signal > it->signal) {
                    it->signal = signal;
                }
            } else {
                WifiNetworkEntry entry;
                entry.ssid = ssid;
                entry.signal = signal;
                entry.is_active = active;
                entry.is_secured = (!sec_str.empty() && sec_str != "--");
                if (active) {
                    m_active_connection = ssid;
                    m_networks.insert(m_networks.begin(), entry);
                } else {
                    m_networks.push_back(entry);
                }
            }
        }
        pclose(fp);
    }
}

void NetworkPopupView::setup_ui() {
    auto config = Config::get();

    set_style(CardStyle::Outlined);
    set_radius(config->metrics.corner_radius > 0 ? config->metrics.corner_radius + 4 : 12);
    // Since popup elevation is handled by miquland.conf layer rule and CardView is tightly packed to edge, we don't need any elevation of CardView.
    set_elevation(0);

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
        ->text("Wi-Fi")
        ->bold(true)
        ->textSize(h2_font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    title->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));
    header->add_view(title);

    m_switch = SwitchBuilder::create()
        ->checked(m_wifi_enabled)
        ->onCheckedChanged([this](bool checked) {
            set_wifi_enabled(checked);
        })
        ->build();
    m_switch->set_layout_params(LayoutParams(46, 26));
    header->add_view(m_switch);
    root->add_view(header);

    // Status subtitle
    std::string status_text = !m_active_connection.empty()
        ? ("Connected to " + m_active_connection)
        : (m_wifi_enabled ? "Available Networks" : "Wi-Fi is Off");
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

    // Available networks list
    if (m_wifi_enabled) {
        m_network_list = std::make_shared<LinearLayout>(Orientation::Vertical);
        m_network_list->set_divider_spacing(6);
        m_network_list->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));

        int count = 0;
        for (const auto& net : m_networks) {
            if (count++ >= 6) break;

            std::string icon = "network-wireless";
            auto btn = ButtonBuilder::create()
                ->text(net.ssid)
                ->icon(icon)
                ->textSize(base_font_size + 1)
                ->build();
            btn->set_layout_params(LayoutParams(
                static_cast<int>(LayoutDimension::MatchParent),
                38
            ));
            btn->set_content_gravity(Gravity::Left | Gravity::CenterVertical);
            btn->set_padding(12, 6);
            if (net.is_active) {
                btn->set_selected(true);
            } else {
                btn->set_flat(true);
            }
            btn->set_on_click_listener([this, s = net.ssid]() {
                std::string cmd = "nmcli dev wifi connect \"" + s + "\" 2>/dev/null &";
                std::system(cmd.c_str());
                if (m_on_close) m_on_close();
            });
            m_network_list->add_view(btn);
        }
        root->add_view(m_network_list);
    }

    add_view(root);
}

void NetworkPopupView::set_wifi_enabled(bool enabled) {
    m_wifi_enabled = enabled;
    std::string cmd = m_wifi_enabled ? "nmcli radio wifi on 2>/dev/null" : "nmcli radio wifi off 2>/dev/null";
    std::system(cmd.c_str());

    scan_networks();
    clear_views();
    setup_ui();
    request_redraw();
}

miqu::Size NetworkPopupView::measure_size() const {
    if (!m_wifi_enabled) {
        return {340, 130};
    }
    int rows = std::min(6, static_cast<int>(m_networks.size()));
    int h = 100 + std::max(1, rows) * 44;
    return {340, std::max(180, h)};
}

} // namespace miqubar
