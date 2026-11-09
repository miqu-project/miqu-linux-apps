#include "miller_view.hpp"
#include "preview_common.hpp"
#include <miqutoolkit/view/popup_window.hpp>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace miqu {
void MillerView::render_wifi_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    // 1. Wi-Fi Power Toggle Card (Just like mute button of speaker)
    auto wifi_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(16, 14)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto wifi_icon = TextViewBuilder::create()
        ->text("󰤨")
        ->textSize(config->metrics.font_size + 4)
        ->textColor(item->switch_val ? config->colors.primary : config->colors.on_surface_variant)
        ->build();
    LayoutParams wicon_lp;
    wicon_lp.width = 30;
    wicon_lp.gravity = Gravity::CenterVertical;
    wifi_card->add_view(wifi_icon, wicon_lp);

    auto wifi_lbl_box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(2)
        ->build();

    auto wifi_title = TextViewBuilder::create()
        ->text("Wi-Fi")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    wifi_lbl_box->add_view(wifi_title);

    std::string wifi_hint_str = item->switch_val ? "Connected to Miqu-Fiber-5G" : "Wi-Fi is turned off";
    auto wifi_hint = TextViewBuilder::create()
        ->text(wifi_hint_str)
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    wifi_lbl_box->add_view(wifi_hint);

    LayoutParams wlbl_lp;
    wlbl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    wlbl_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    wlbl_lp.weight = 1.0f;
    wlbl_lp.gravity = Gravity::CenterVertical;
    wifi_card->add_view(wifi_lbl_box, wlbl_lp);

    auto primary_sw = SwitchBuilder::create()
        ->checked(item->switch_val)
        ->onCheckedChanged([this, item](bool checked) {
            item->switch_val = checked;
            rebuild_current_column();
            deferred_rebuild_preview();
            notify_state_changed();
        })
        ->build();

    LayoutParams sw_lp;
    sw_lp.width = 44;
    sw_lp.height = 24;
    sw_lp.gravity = Gravity::CenterVertical;
    wifi_card->add_view(primary_sw, sw_lp);
    add_preview_card(wifi_card);

    // 2. Networks Section (Visible when Wi-Fi is enabled)
    if (item->switch_val) {
        auto net_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(8)
            ->padding(16, 14)
            ->backgroundColor(config->colors.surface)
            ->stroke(1, config->colors.outline.with_alpha(0.3f))
            ->cornerRadius(get_card_radius(config))
            ->build();

        // Header row with "Networks" title and "Scan" button
        auto net_header_row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->gravity(Gravity::CenterVertical)
            ->build();

        auto net_title = TextViewBuilder::create()
            ->text("Networks")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        LayoutParams nt_lp;
        nt_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        nt_lp.weight = 1.0f;
        nt_lp.gravity = Gravity::CenterVertical;
        net_header_row->add_view(net_title, nt_lp);

        auto on_scan = [this, item]() {
            item->last_action_message = "Scan completed • 5 networks found";
            rebuild_preview_column();
            notify_state_changed();
        };

        auto scan_btn = ButtonBuilder::create()
            ->text("Scan")
            ->flat(true)
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.primary)
            ->padding(10, 4)
            ->cornerRadius(get_btn_radius(config))
            ->onClick(on_scan)
            ->build();

        LayoutParams scan_lp;
        scan_lp.width = static_cast<int>(LayoutDimension::WrapContent);
        scan_lp.height = 28;
        scan_lp.gravity = Gravity::CenterVertical;
        net_header_row->add_view(scan_btn, scan_lp);
        net_card->add_view(net_header_row);

        if (!item->last_action_message.empty()) {
            auto scan_status = TextViewBuilder::create()
                ->text(item->last_action_message)
                ->caption(true)
                ->textColor(config->colors.primary)
                ->build();
            net_card->add_view(scan_status);
        }

        // Networks List
        for (size_t ni = 0; ni < item->wifi_networks.size(); ++ni) {
            auto& net = item->wifi_networks[ni];
            bool is_conn = (net.status == "Connected");

            auto row_content = LinearLayoutBuilder::create()
                ->orientation(Orientation::Horizontal)
                ->padding(10, 8)
                ->spacing(12)
                ->build();

            // Signal icon
            auto sig_view = TextViewBuilder::create()
                ->text(net.icon)
                ->textSize(config->metrics.font_size)
                ->textColor(is_conn ? config->colors.primary : config->colors.on_surface_variant)
                ->build();
            LayoutParams sig_lp;
            sig_lp.width = 24;
            sig_lp.gravity = Gravity::CenterVertical;
            row_content->add_view(sig_view, sig_lp);

            // SSID and Status subtitle
            auto text_box = LinearLayoutBuilder::create()
                ->orientation(Orientation::Vertical)
                ->spacing(2)
                ->build();

            auto ssid_text = TextViewBuilder::create()
                ->text(net.ssid)
                ->textSize(config->metrics.font_size)
                ->textColor(is_conn ? config->colors.primary : config->colors.on_surface)
                ->ellipsize(true)
                ->build();
            text_box->add_view(ssid_text);

            Color sub_color = is_conn ? config->colors.primary :
                              (net.status == "Saved" ? config->colors.on_surface_variant : config->colors.on_surface_variant.with_alpha(0.7f));
            auto status_text = TextViewBuilder::create()
                ->text(net.status)
                ->caption(true)
                ->textColor(sub_color)
                ->build();
            text_box->add_view(status_text);

            LayoutParams tb_lp;
            tb_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            tb_lp.weight = 1.0f;
            tb_lp.gravity = Gravity::CenterVertical;
            row_content->add_view(text_box, tb_lp);

            // Trailing indicator: checkmark for connected, lock icon for secured
            if (is_conn) {
                auto check_view = TextViewBuilder::create()
                    ->text("✓")
                    ->textSize(config->metrics.font_size)
                    ->textColor(config->colors.primary)
                    ->build();
                LayoutParams chk_lp;
                chk_lp.width = static_cast<int>(LayoutDimension::WrapContent);
                chk_lp.gravity = Gravity::CenterVertical;
                row_content->add_view(check_view, chk_lp);
            } else if (net.secured) {
                auto lock_view = TextViewBuilder::create()
                    ->text("󰌾")
                    ->textSize(config->metrics.font_size)
                    ->textColor(config->colors.on_surface_variant.with_alpha(0.6f))
                    ->build();
                LayoutParams lk_lp;
                lk_lp.width = static_cast<int>(LayoutDimension::WrapContent);
                lk_lp.gravity = Gravity::CenterVertical;
                row_content->add_view(lock_view, lk_lp);
            }

            Color row_bg = is_conn ? config->colors.primary_container.with_alpha(0.35f) : Color::transparent();

            auto net_row_card = CardViewBuilder::create()
                ->style(CardStyle::Filled)
                ->backgroundColor(row_bg)
                ->cornerRadius(get_inner_radius(config))
                ->elevation(0)
                ->addView(row_content)
                ->build();

            auto on_connect = [this, item, ni]() {
                if (ni >= item->wifi_networks.size()) return;
                for (size_t k = 0; k < item->wifi_networks.size(); ++k) {
                    if (k == ni) {
                        item->wifi_networks[k].status = "Connected";
                    } else if (item->wifi_networks[k].status == "Connected") {
                        item->wifi_networks[k].status = "Saved";
                    }
                }
                deferred_rebuild_preview();
                notify_state_changed();
            };

            // Clicking network: switches connection to this network
            net_row_card->set_on_click_listener(on_connect);

            net_card->add_view(net_row_card);
        }

        add_preview_card(net_card);
    }

    // 3. Interface Specifications
    if (!item->telemetry.empty()) {
        add_preview_card(create_telemetry_card(item->telemetry, config, "Wireless interface specifications"));
    }

    return;
}

void MillerView::render_bluetooth_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    // 1. Bluetooth Power Toggle Card
    auto bt_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(16, 14)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto bt_icon = TextViewBuilder::create()
        ->text("󰂯")
        ->textSize(config->metrics.font_size + 4)
        ->textColor(item->switch_val ? config->colors.primary : config->colors.on_surface_variant)
        ->build();
    LayoutParams bicon_lp;
    bicon_lp.width = 30;
    bicon_lp.gravity = Gravity::CenterVertical;
    bt_card->add_view(bt_icon, bicon_lp);

    auto bt_lbl_box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(2)
        ->build();

    auto bt_title = TextViewBuilder::create()
        ->text("Bluetooth")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    bt_lbl_box->add_view(bt_title);

    std::string connected_device_name;
    for (const auto& dev : item->bt_devices) {
        if (dev.status == "Connected") {
            connected_device_name = dev.name;
            break;
        }
    }

    std::string bt_hint_str;
    if (!item->switch_val) {
        bt_hint_str = "Bluetooth is turned off";
    } else if (!connected_device_name.empty()) {
        bt_hint_str = "Connected to " + connected_device_name;
    } else {
        bt_hint_str = "Discoverable as Miqu-Desktop";
    }

    auto bt_hint = TextViewBuilder::create()
        ->text(bt_hint_str)
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    bt_lbl_box->add_view(bt_hint);

    LayoutParams blbl_lp;
    blbl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    blbl_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    blbl_lp.weight = 1.0f;
    blbl_lp.gravity = Gravity::CenterVertical;
    bt_card->add_view(bt_lbl_box, blbl_lp);

    auto primary_sw = SwitchBuilder::create()
        ->checked(item->switch_val)
        ->onCheckedChanged([this, item](bool checked) {
            item->switch_val = checked;
            rebuild_current_column();
            deferred_rebuild_preview();
            notify_state_changed();
        })
        ->build();

    LayoutParams sw_lp;
    sw_lp.width = 44;
    sw_lp.height = 24;
    sw_lp.gravity = Gravity::CenterVertical;
    bt_card->add_view(primary_sw, sw_lp);
    add_preview_card(bt_card);

    // 2. Devices Section (Visible when Bluetooth is enabled)
    if (item->switch_val) {
        auto dev_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(8)
            ->padding(16, 14)
            ->backgroundColor(config->colors.surface)
            ->stroke(1, config->colors.outline.with_alpha(0.3f))
            ->cornerRadius(get_card_radius(config))
            ->build();

        // Header row with "Devices" title and "Scan" button
        auto dev_header_row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->gravity(Gravity::CenterVertical)
            ->build();

        auto dev_title = TextViewBuilder::create()
            ->text("Devices")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        LayoutParams dt_lp;
        dt_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        dt_lp.weight = 1.0f;
        dt_lp.gravity = Gravity::CenterVertical;
        dev_header_row->add_view(dev_title, dt_lp);

        auto on_scan = [this, item]() {
            bool found_new = false;
            bool already_has_bose = false;
            for (const auto& d : item->bt_devices) {
                if (d.name == "Bose QuietComfort 45") {
                    already_has_bose = true;
                    break;
                }
            }
            if (!already_has_bose) {
                item->bt_devices.push_back({"Bose QuietComfort 45", "Tap to pair", "󰋋", ""});
                item->last_action_message = "Scan completed • Found Bose QuietComfort 45";
                found_new = true;
            } else {
                bool already_has_mx = false;
                for (const auto& d : item->bt_devices) {
                    if (d.name == "MX Anywhere 3S") {
                        already_has_mx = true;
                        break;
                    }
                }
                if (!already_has_mx) {
                    item->bt_devices.push_back({"MX Anywhere 3S", "Tap to pair", "󰍽", ""});
                    item->last_action_message = "Scan completed • Found MX Anywhere 3S";
                    found_new = true;
                }
            }
            if (!found_new) {
                item->last_action_message = "Scan completed • All nearby devices discovered";
            }
            rebuild_preview_column();
            notify_state_changed();
        };

        auto scan_btn = ButtonBuilder::create()
            ->text("Scan")
            ->flat(true)
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.primary)
            ->padding(10, 4)
            ->cornerRadius(get_btn_radius(config))
            ->onClick(on_scan)
            ->build();

        LayoutParams scan_lp;
        scan_lp.width = static_cast<int>(LayoutDimension::WrapContent);
        scan_lp.height = 28;
        scan_lp.gravity = Gravity::CenterVertical;
        dev_header_row->add_view(scan_btn, scan_lp);
        dev_card->add_view(dev_header_row);

        if (!item->last_action_message.empty()) {
            auto scan_status = TextViewBuilder::create()
                ->text(item->last_action_message)
                ->caption(true)
                ->textColor(config->colors.primary)
                ->build();
            dev_card->add_view(scan_status);
        }

        // Devices List
        for (size_t di = 0; di < item->bt_devices.size(); ++di) {
            auto& dev = item->bt_devices[di];
            bool is_conn = (dev.status == "Connected");

            auto row_content = LinearLayoutBuilder::create()
                ->orientation(Orientation::Horizontal)
                ->padding(10, 8)
                ->spacing(12)
                ->build();

            // Device icon
            auto icon_view = TextViewBuilder::create()
                ->text(dev.icon.empty() ? "󰂯" : dev.icon)
                ->textSize(config->metrics.font_size)
                ->textColor(is_conn ? config->colors.primary : config->colors.on_surface_variant)
                ->build();
            LayoutParams icon_lp;
            icon_lp.width = 24;
            icon_lp.gravity = Gravity::CenterVertical;
            row_content->add_view(icon_view, icon_lp);

            // Name and Status subtitle
            auto text_box = LinearLayoutBuilder::create()
                ->orientation(Orientation::Vertical)
                ->spacing(2)
                ->build();

            auto name_text = TextViewBuilder::create()
                ->text(dev.name)
                ->textSize(config->metrics.font_size)
                ->textColor(is_conn ? config->colors.primary : config->colors.on_surface)
                ->ellipsize(true)
                ->build();
            text_box->add_view(name_text);

            std::string status_sub = dev.status;
            if (is_conn && !dev.battery.empty()) {
                status_sub = "Connected • " + dev.battery;
            }

            Color sub_color = is_conn ? config->colors.primary :
                              (dev.status == "Tap to pair" ? config->colors.primary :
                              (dev.status == "Paired" ? config->colors.on_surface_variant : config->colors.on_surface_variant.with_alpha(0.7f)));
            auto status_text = TextViewBuilder::create()
                ->text(status_sub)
                ->caption(true)
                ->textColor(sub_color)
                ->build();
            text_box->add_view(status_text);

            LayoutParams tb_lp;
            tb_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            tb_lp.weight = 1.0f;
            tb_lp.gravity = Gravity::CenterVertical;
            row_content->add_view(text_box, tb_lp);

            // Trailing indicator: checkmark for connected
            if (is_conn) {
                auto check_view = TextViewBuilder::create()
                    ->text("✓")
                    ->textSize(config->metrics.font_size)
                    ->textColor(config->colors.primary)
                    ->build();
                LayoutParams chk_lp;
                chk_lp.width = static_cast<int>(LayoutDimension::WrapContent);
                chk_lp.gravity = Gravity::CenterVertical;
                row_content->add_view(check_view, chk_lp);
            }

            Color row_bg = is_conn ? config->colors.primary_container.with_alpha(0.35f) : Color::transparent();

            auto dev_row_card = CardViewBuilder::create()
                ->style(CardStyle::Filled)
                ->backgroundColor(row_bg)
                ->cornerRadius(get_inner_radius(config))
                ->elevation(0)
                ->addView(row_content)
                ->build();

            auto on_dev_connect = [this, item, di]() {
                if (di >= item->bt_devices.size()) return;
                for (size_t k = 0; k < item->bt_devices.size(); ++k) {
                    if (k == di) {
                        item->bt_devices[k].status = "Connected";
                        if (item->bt_devices[k].battery.empty()) {
                            item->bt_devices[k].battery = "85%";
                        }
                    } else if (item->bt_devices[k].status == "Connected") {
                        item->bt_devices[k].status = "Paired";
                    }
                }
                deferred_rebuild_preview();
                notify_state_changed();
            };

            // Clicking device: connects it (or pairs & connects if Tap to pair)
            dev_row_card->set_on_click_listener(on_dev_connect);

            dev_card->add_view(dev_row_card);
        }

        add_preview_card(dev_card);
    }

    // 3. Controller Specifications
    if (!item->telemetry.empty()) {
        add_preview_card(create_telemetry_card(item->telemetry, config, "Bluetooth controller specifications"));
    }

    return;
}

void MillerView::render_ethernet_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    bool is_conn = (item->eth_profile.status == "Connected");

    // 1. Active Wired Connection Profile & IP Parameters (Visible when connected)
    if (is_conn) {
        auto profile_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(10)
            ->padding(16, 14)
            ->backgroundColor(config->colors.surface)
            ->stroke(1, config->colors.outline.with_alpha(0.3f))
            ->cornerRadius(get_card_radius(config))
            ->build();

        // Header row with "Wired Connection" title and "Renew DHCP" button
        auto header_row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->gravity(Gravity::CenterVertical)
            ->build();

        auto header_title = TextViewBuilder::create()
            ->text("Wired Connection")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        LayoutParams ht_lp;
        ht_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        ht_lp.weight = 1.0f;
        ht_lp.gravity = Gravity::CenterVertical;
        header_row->add_view(header_title, ht_lp);

        auto on_renew = [this, item]() {
            item->last_action_message = "DHCP lease renewed • IPv4 192.168.1.105";
            rebuild_preview_column();
            notify_state_changed();
        };

        auto renew_btn = ButtonBuilder::create()
            ->text("Renew DHCP")
            ->flat(true)
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.primary)
            ->padding(10, 4)
            ->cornerRadius(get_btn_radius(config))
            ->onClick(on_renew)
            ->build();

        LayoutParams renew_lp;
        renew_lp.width = static_cast<int>(LayoutDimension::WrapContent);
        renew_lp.height = 28;
        renew_lp.gravity = Gravity::CenterVertical;
        header_row->add_view(renew_btn, renew_lp);
        profile_card->add_view(header_row);

        if (!item->last_action_message.empty()) {
            auto renew_status = TextViewBuilder::create()
                ->text(item->last_action_message)
                ->caption(true)
                ->textColor(config->colors.primary)
                ->build();
            profile_card->add_view(renew_status);
        }

        // Active Connection Highlight Card
        auto conn_row_content = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->padding(10, 8)
            ->spacing(12)
            ->build();

        auto conn_icon = TextViewBuilder::create()
            ->text("󰈀")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.primary)
            ->build();
        LayoutParams cicon_lp;
        cicon_lp.width = 24;
        cicon_lp.gravity = Gravity::CenterVertical;
        conn_row_content->add_view(conn_icon, cicon_lp);

        auto text_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();

        auto profile_name = TextViewBuilder::create()
            ->text(item->eth_profile.name)
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.primary)
            ->build();
        text_box->add_view(profile_name);

        auto profile_sub = TextViewBuilder::create()
            ->text(item->eth_profile.mode + " • " + item->eth_profile.status)
            ->caption(true)
            ->textColor(config->colors.primary)
            ->build();
        text_box->add_view(profile_sub);

        LayoutParams tb_lp;
        tb_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        tb_lp.weight = 1.0f;
        tb_lp.gravity = Gravity::CenterVertical;
        conn_row_content->add_view(text_box, tb_lp);

        auto check_view = TextViewBuilder::create()
            ->text("✓")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.primary)
            ->build();
        LayoutParams chk_lp;
        chk_lp.width = static_cast<int>(LayoutDimension::WrapContent);
        chk_lp.gravity = Gravity::CenterVertical;
        conn_row_content->add_view(check_view, chk_lp);

        auto conn_row_card = CardViewBuilder::create()
            ->style(CardStyle::Filled)
            ->backgroundColor(config->colors.primary_container.with_alpha(0.35f))
            ->cornerRadius(get_inner_radius(config))
            ->elevation(0)
            ->addView(conn_row_content)
            ->build();
        profile_card->add_view(conn_row_card);

        // IP Parameters Details
        std::vector<std::pair<std::string, std::string>> ip_params = {
            {"IP Address", item->eth_profile.ip_address},
            {"Subnet Mask", item->eth_profile.subnet},
            {"Default Gateway", item->eth_profile.gateway},
            {"DNS Servers", item->eth_profile.dns}
        };

        for (const auto& [k, v] : ip_params) {
            auto row = LinearLayoutBuilder::create()
                ->orientation(Orientation::Horizontal)
                ->padding(2, 2)
                ->build();

            auto k_view = TextViewBuilder::create()
                ->text(k)
                ->caption(true)
                ->textColor(config->colors.on_surface_variant)
                ->build();
            LayoutParams k_lp;
            k_lp.width = 140;
            row->add_view(k_view, k_lp);

            auto v_view = TextViewBuilder::create()
                ->text(v)
                ->caption(true)
                ->textColor(config->colors.on_surface)
                ->build();
            LayoutParams v_lp;
            v_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            v_lp.weight = 1.0f;
            row->add_view(v_view, v_lp);

            profile_card->add_view(row);
        }

        add_preview_card(profile_card);
    }

    // 3. Hardware & Link Telemetry
    if (!item->telemetry.empty()) {
        add_preview_card(create_telemetry_card(item->telemetry, config, "Hardware & link specifications"));
    }

    return;
}

void MillerView::render_airplane_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    // 1. Airplane Mode Primary Switch Card
    auto airplane_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(16, 14)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto plane_icon = TextViewBuilder::create()
        ->text("󰀝")
        ->textSize(config->metrics.font_size + 4)
        ->textColor(item->switch_val ? config->colors.primary : config->colors.on_surface_variant)
        ->build();
    LayoutParams picon_lp;
    picon_lp.width = 30;
    picon_lp.gravity = Gravity::CenterVertical;
    airplane_card->add_view(plane_icon, picon_lp);

    auto plane_lbl_box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(2)
        ->build();

    auto plane_title = TextViewBuilder::create()
        ->text("Airplane Mode")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    plane_lbl_box->add_view(plane_title);

    std::string plane_sub = item->switch_val ? "All wireless communications disabled" : "Wireless communications enabled";
    auto plane_hint = TextViewBuilder::create()
        ->text(plane_sub)
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    plane_lbl_box->add_view(plane_hint);

    LayoutParams plbl_lp;
    plbl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    plbl_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    plbl_lp.weight = 1.0f;
    plbl_lp.gravity = Gravity::CenterVertical;
    airplane_card->add_view(plane_lbl_box, plbl_lp);

    auto primary_sw = SwitchBuilder::create()
        ->checked(item->switch_val)
        ->onCheckedChanged([this, item](bool checked) {
            item->switch_val = checked;
            rebuild_current_column();
            deferred_rebuild_preview();
            notify_state_changed();
        })
        ->build();

    LayoutParams sw_lp;
    sw_lp.width = 44;
    sw_lp.height = 24;
    sw_lp.gravity = Gravity::CenterVertical;
    airplane_card->add_view(primary_sw, sw_lp);
    add_preview_card(airplane_card);

    // 2. Bottom Info Card: What things are going to be disabled
    auto info_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(10)
        ->padding(16, 14)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto info_title = TextViewBuilder::create()
        ->text("Affected Wireless Radios")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    info_card->add_view(info_title);

    struct RadioImpact {
        std::string name;
        std::string icon;
        std::string desc;
    };

    std::vector<RadioImpact> radios = {
        {"Wi-Fi Radio", "󰤨", "Disables 802.11 Wi-Fi networks, local connections, and scanning"},
        {"Bluetooth", "󰂯", "Disconnects wireless audio, peripherals, keyboards, and discovery"},
        {"Cellular & Mobile", "󰄡", "Suspends WWAN mobile data modems and SIM network connectivity"},
        {"GPS & Location", "󰍎", "Powers down satellite positioning and location-sensing hardware"}
    };

    for (const auto& radio : radios) {
        auto row_content = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->padding(10, 8)
            ->spacing(12)
            ->build();

        // Radio icon
        auto r_icon = TextViewBuilder::create()
            ->text(radio.icon)
            ->textSize(config->metrics.font_size)
            ->textColor(item->switch_val ? config->colors.on_surface_variant.with_alpha(0.6f) : config->colors.primary)
            ->build();
        LayoutParams ricon_lp;
        ricon_lp.width = 24;
        ricon_lp.gravity = Gravity::CenterVertical;
        row_content->add_view(r_icon, ricon_lp);

        auto text_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();

        auto r_name = TextViewBuilder::create()
            ->text(radio.name)
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        text_box->add_view(r_name);

        auto r_desc = TextViewBuilder::create()
            ->text(radio.desc)
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        text_box->add_view(r_desc);

        LayoutParams tb_lp;
        tb_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        tb_lp.weight = 1.0f;
        tb_lp.gravity = Gravity::CenterVertical;
        row_content->add_view(text_box, tb_lp);

        // Status Badge
        std::string status_str = item->switch_val ? "Disabled" : "Active";
        Color badge_bg = item->switch_val ? config->colors.surface_variant : config->colors.primary_container.with_alpha(0.4f);
        Color badge_fg = item->switch_val ? config->colors.on_surface_variant : config->colors.primary;

        auto status_view = TextViewBuilder::create()
            ->text(status_str)
            ->caption(true)
            ->textColor(badge_fg)
            ->padding(8, 4)
            ->build();

        auto pill = CardViewBuilder::create()
            ->style(CardStyle::Filled)
            ->backgroundColor(badge_bg)
            ->cornerRadius(get_btn_radius(config))
            ->elevation(0)
            ->addView(status_view)
            ->build();

        LayoutParams pill_lp;
        pill_lp.width = static_cast<int>(LayoutDimension::WrapContent);
        pill_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        pill_lp.gravity = Gravity::CenterVertical;
        row_content->add_view(pill, pill_lp);

        Color row_bg = item->switch_val ? config->colors.surface_variant.with_alpha(0.2f) : Color::transparent();
        auto row_card = CardViewBuilder::create()
            ->style(CardStyle::Filled)
            ->backgroundColor(row_bg)
            ->cornerRadius(get_inner_radius(config))
            ->elevation(0)
            ->addView(row_content)
            ->build();

        info_card->add_view(row_card);
    }

    add_preview_card(info_card);
    return;
}

void MillerView::render_dns_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    // 1. Configuration Card (Single Spinner on new line, Hostname EditText on new line)
    auto cfg_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(10)
        ->padding(16, 14)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto cfg_title = TextViewBuilder::create()
        ->text("DNS-over-TLS configuration")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    cfg_card->add_view(cfg_title);

    // Label for Spinner
    auto sp_lbl = TextViewBuilder::create()
        ->text("Mode / Provider")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    cfg_card->add_view(sp_lbl);

    // Forward declare details views to update in-place without tearing down spinner
    const auto& presets = item->private_dns_cfg.presets;
    const PrivateDnsPreset* active_preset = &presets[0];
    for (const auto& p : presets) {
        if (p.name == item->private_dns_cfg.selected_option) {
            active_preset = &p;
            break;
        }
    }

    bool is_custom = (item->private_dns_cfg.selected_option == "Custom Hostname");
    std::string initial_ep_host = is_custom ? item->private_dns_cfg.custom_hostname : active_preset->hostname;

    auto host_val = TextViewBuilder::create()
        ->text(initial_ep_host.empty() ? "(Not set)" : initial_ep_host)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.primary)
        ->build();

    auto desc_val = TextViewBuilder::create()
        ->text(active_preset->description)
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();

    auto ip_val = TextViewBuilder::create()
        ->text(active_preset->endpoints)
        ->caption(true)
        ->textColor(config->colors.on_surface)
        ->build();

    std::string initial_edit_text = is_custom ? item->private_dns_cfg.custom_hostname : active_preset->hostname;
    auto custom_edit = EditTextBuilder::create()
        ->text(initial_edit_text)
        ->hint(is_custom ? "Enter custom DoT hostname (e.g. dns.nextdns.io)" : "")
        ->editable(is_custom)
        ->padding(14, 8)
        ->build();

    auto sync_dns_ui = [item, host_val, desc_val, ip_val]() {
        bool custom = (item->private_dns_cfg.selected_option == "Custom Hostname");
        for (const auto& p : item->private_dns_cfg.presets) {
            if (p.name == item->private_dns_cfg.selected_option) {
                std::string h = custom ? item->private_dns_cfg.custom_hostname : p.hostname;
                host_val->set_text(h.empty() ? "(Not set)" : h);
                desc_val->set_text(p.description);
                ip_val->set_text(p.endpoints);
                host_val->request_redraw();
                desc_val->request_redraw();
                ip_val->request_redraw();
                break;
            }
        }
    };

    // Find selected index for Spinner
    int sel_idx = 0;
    for (size_t i = 0; i < item->private_dns_cfg.options.size(); ++i) {
        if (item->private_dns_cfg.options[i] == item->private_dns_cfg.selected_option) {
            sel_idx = static_cast<int>(i);
            break;
        }
    }

    // Single Spinner on a NEW line
    auto dns_spinner = SpinnerBuilder::create()
        ->items(item->private_dns_cfg.options)
        ->selectedIndex(sel_idx)
        ->fontSize(config->metrics.font_size)
        ->padding(12, 6)
        ->cornerRadius(get_inner_radius(config))
        ->onItemSelected([item, custom_edit, sync_dns_ui](int idx, const std::string& val) {
            (void)idx;
            item->private_dns_cfg.selected_option = val;
            bool custom = (val == "Custom Hostname");
            custom_edit->set_editable(custom);
            if (custom) {
                custom_edit->set_text(item->private_dns_cfg.custom_hostname);
                custom_edit->set_hint("Enter custom DoT hostname (e.g. dns.nextdns.io)");
            } else {
                for (const auto& p : item->private_dns_cfg.presets) {
                    if (p.name == val) {
                        custom_edit->set_text(p.hostname);
                        custom_edit->set_hint("");
                        break;
                    }
                }
            }
            sync_dns_ui();
        })
        ->build();

    LayoutParams sp_lp(static_cast<int>(LayoutDimension::WrapContent), 36);
    cfg_card->add_view(dns_spinner, sp_lp);

    // Hostname Label
    auto host_lbl = TextViewBuilder::create()
        ->text("Provider Hostname")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    cfg_card->add_view(host_lbl);

    // Conditional EditText on a NEW line
    custom_edit->set_on_text_changed_listener([item, sync_dns_ui](std::shared_ptr<EditText>, const std::string& new_text) {
        item->private_dns_cfg.custom_hostname = new_text;
        sync_dns_ui();
    });

    LayoutParams edit_lp;
    edit_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    edit_lp.height = 38;
    cfg_card->add_view(custom_edit, edit_lp);

    add_preview_card(cfg_card);

    // 3. Active Endpoint & Verification Card
    auto ep_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(10)
        ->padding(16, 14)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto ep_header = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->gravity(Gravity::CenterVertical)
        ->build();

    auto ep_title = TextViewBuilder::create()
        ->text("Active endpoint & features")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    LayoutParams ept_lp;
    ept_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    ept_lp.weight = 1.0f;
    ept_lp.gravity = Gravity::CenterVertical;
    ep_header->add_view(ep_title, ept_lp);

    auto test_btn = ButtonBuilder::create()
        ->text("Test Resolution")
        ->flat(true)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.primary)
        ->padding(10, 4)
        ->cornerRadius(get_btn_radius(config))
        ->onClick([this, item]() {
            item->last_action_message = "DNS handshake verified • Latency: 3.8 ms";
            rebuild_preview_column();
            notify_state_changed();
        })
        ->build();
    LayoutParams tb_lp;
    tb_lp.width = static_cast<int>(LayoutDimension::WrapContent);
    tb_lp.height = 28;
    tb_lp.gravity = Gravity::CenterVertical;
    ep_header->add_view(test_btn, tb_lp);
    ep_card->add_view(ep_header);

    if (!item->last_action_message.empty()) {
        auto test_feedback = TextViewBuilder::create()
            ->text(item->last_action_message)
            ->caption(true)
            ->textColor(config->colors.primary)
            ->build();
        ep_card->add_view(test_feedback);
    }

    // Hostname row
    auto host_box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(2)
        ->build();
    auto host_caption = TextViewBuilder::create()
        ->text("TLS Hostname")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    host_box->add_view(host_caption);
    host_box->add_view(host_val);
    ep_card->add_view(host_box);

    // Description box
    auto desc_box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(2)
        ->build();
    auto desc_caption = TextViewBuilder::create()
        ->text("Protection Scope")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    desc_box->add_view(desc_caption);
    desc_box->add_view(desc_val);
    ep_card->add_view(desc_box);

    // Endpoints box
    auto ip_box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(2)
        ->build();
    auto ip_caption = TextViewBuilder::create()
        ->text("Upstream Anycast Resolvers")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    ip_box->add_view(ip_caption);
    ip_box->add_view(ip_val);
    ep_card->add_view(ip_box);

    add_preview_card(ep_card);

    // 4. DNS & Encryption Specifications Telemetry
    if (!item->telemetry.empty()) {
        add_preview_card(create_telemetry_card(item->telemetry, config, "DNS & encryption specifications"));
    }

    return;
}

void MillerView::render_firewall_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    // 1. Top Card: Simple Bluetooth-like On/Off Switch with Subtitle "Powered by UFW"
    auto fw_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(16, 14)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto fw_icon = TextViewBuilder::create()
        ->text("󰒃")
        ->textSize(config->metrics.font_size + 4)
        ->textColor(item->firewall_cfg.enabled ? config->colors.primary : config->colors.on_surface_variant)
        ->build();
    LayoutParams fwicon_lp;
    fwicon_lp.width = 30;
    fwicon_lp.gravity = Gravity::CenterVertical;
    fw_card->add_view(fw_icon, fwicon_lp);

    auto fw_lbl_box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(2)
        ->build();

    auto fw_title = TextViewBuilder::create()
        ->text("Firewall")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    fw_lbl_box->add_view(fw_title);

    auto fw_hint = TextViewBuilder::create()
        ->text("Powered by UFW")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    fw_lbl_box->add_view(fw_hint);

    LayoutParams fwlbl_lp;
    fwlbl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    fwlbl_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    fwlbl_lp.weight = 1.0f;
    fwlbl_lp.gravity = Gravity::CenterVertical;
    fw_card->add_view(fw_lbl_box, fwlbl_lp);

    auto primary_sw = SwitchBuilder::create()
        ->checked(item->firewall_cfg.enabled)
        ->onCheckedChanged([this, item](bool checked) {
            item->firewall_cfg.enabled = checked;
            rebuild_current_column();
            deferred_rebuild_preview();
            notify_state_changed();
        })
        ->build();

    LayoutParams sw_lp;
    sw_lp.width = 44;
    sw_lp.height = 24;
    sw_lp.gravity = Gravity::CenterVertical;
    fw_card->add_view(primary_sw, sw_lp);

    add_preview_card(fw_card);

    if (item->firewall_cfg.enabled) {
        // 2. Policy Card: Incoming Spinner, Outgoing Spinner
        auto policy_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(10)
            ->padding(16, 14)
            ->backgroundColor(config->colors.surface)
            ->stroke(1, config->colors.outline.with_alpha(0.3f))
            ->cornerRadius(get_card_radius(config))
            ->build();

        auto policy_title = TextViewBuilder::create()
            ->text("Default Policies")
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        policy_card->add_view(policy_title);

        // Incoming traffic
        auto in_lbl = TextViewBuilder::create()
            ->text("Incoming traffic")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        policy_card->add_view(in_lbl);

        std::vector<std::string> in_policies = {"Drop (Deny)", "Reject", "Allow"};
        int in_sel = 0;
        for (size_t i = 0; i < in_policies.size(); ++i) {
            if (in_policies[i] == item->firewall_cfg.incoming_policy) {
                in_sel = static_cast<int>(i);
                break;
            }
        }

        auto in_spinner = SpinnerBuilder::create()
            ->items(in_policies)
            ->selectedIndex(in_sel)
            ->fontSize(config->metrics.font_size)
            ->padding(12, 6)
            ->cornerRadius(get_inner_radius(config))
            ->onItemSelected([item, in_policies](int idx, const std::string& val) {
                (void)idx;
                item->firewall_cfg.incoming_policy = val;
            })
            ->build();

        LayoutParams in_sp_lp(static_cast<int>(LayoutDimension::WrapContent), 36);
        policy_card->add_view(in_spinner, in_sp_lp);

        // Outgoing traffic
        auto out_lbl = TextViewBuilder::create()
            ->text("Outgoing traffic")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        policy_card->add_view(out_lbl);

        std::vector<std::string> out_policies = {"Allow", "Drop (Deny)", "Reject"};
        int out_sel = 0;
        for (size_t i = 0; i < out_policies.size(); ++i) {
            if (out_policies[i] == item->firewall_cfg.outgoing_policy) {
                out_sel = static_cast<int>(i);
                break;
            }
        }

        auto out_spinner = SpinnerBuilder::create()
            ->items(out_policies)
            ->selectedIndex(out_sel)
            ->fontSize(config->metrics.font_size)
            ->padding(12, 6)
            ->cornerRadius(get_inner_radius(config))
            ->onItemSelected([item, out_policies](int idx, const std::string& val) {
                (void)idx;
                item->firewall_cfg.outgoing_policy = val;
            })
            ->build();

        LayoutParams out_sp_lp(static_cast<int>(LayoutDimension::WrapContent), 36);
        policy_card->add_view(out_spinner, out_sp_lp);

        add_preview_card(policy_card);

        // 3. Preset Card: Home, Work, Cellular, Public
        auto preset_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(10)
            ->padding(16, 14)
            ->backgroundColor(config->colors.surface)
            ->stroke(1, config->colors.outline.with_alpha(0.3f))
            ->cornerRadius(get_card_radius(config))
            ->build();

        auto preset_title = TextViewBuilder::create()
            ->text("Security Preset")
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        preset_card->add_view(preset_title);

        auto preset_lbl = TextViewBuilder::create()
            ->text("Profile / Location")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        preset_card->add_view(preset_lbl);

        auto get_preset_desc = [](const std::string& p) -> std::string {
            if (p == "Work") return "Restricts inbound traffic to corporate VPN and office intranet; enables managed remote access.";
            if (p == "Cellular") return "Minimizes background traffic and blocks non-essential inbound sockets to preserve metered data.";
            if (p == "Public") return "Strict stealth isolation. Silently drops all unsolicited probes and disables local discovery.";
            return "Permits local network device discovery, KDE Connect, and home media streaming while blocking external WAN probes.";
        };

        auto preset_desc = TextViewBuilder::create()
            ->text(get_preset_desc(item->firewall_cfg.selected_preset))
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();

        std::vector<std::string> presets = {"Home", "Work", "Cellular", "Public"};
        int preset_sel = 0;
        for (size_t i = 0; i < presets.size(); ++i) {
            if (presets[i] == item->firewall_cfg.selected_preset) {
                preset_sel = static_cast<int>(i);
                break;
            }
        }

        auto preset_spinner = SpinnerBuilder::create()
            ->items(presets)
            ->selectedIndex(preset_sel)
            ->fontSize(config->metrics.font_size)
            ->padding(12, 6)
            ->cornerRadius(get_inner_radius(config))
            ->onItemSelected([item, preset_desc, get_preset_desc](int idx, const std::string& val) {
                (void)idx;
                item->firewall_cfg.selected_preset = val;
                preset_desc->set_text(get_preset_desc(val));
                preset_desc->request_redraw();
            })
            ->build();

        LayoutParams preset_sp_lp(static_cast<int>(LayoutDimension::WrapContent), 36);
        preset_card->add_view(preset_spinner, preset_sp_lp);
        preset_card->add_view(preset_desc);

        add_preview_card(preset_card);

        // 4. Custom Rules Card: switches for common services + Add Rule popup button on header
        auto rules_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(12)
            ->padding(16, 14)
            ->backgroundColor(config->colors.surface)
            ->stroke(1, config->colors.outline.with_alpha(0.3f))
            ->cornerRadius(get_card_radius(config))
            ->build();

        // Card Header Row: Title on Left, Add Rule Button on Right
        auto rules_hdr = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->gravity(Gravity::CenterVertical)
            ->build();

        auto rules_title = TextViewBuilder::create()
            ->text("Custom Rules")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        LayoutParams rtitle_lp;
        rtitle_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        rtitle_lp.weight = 1.0f;
        rtitle_lp.gravity = Gravity::CenterVertical;
        rules_hdr->add_view(rules_title, rtitle_lp);

        auto add_btn = ButtonBuilder::create()
            ->text("+ Add Rule")
            ->style(ButtonStyle::Outlined)
            ->padding(12, 4)
            ->cornerRadius(get_btn_radius(config))
            ->build();

        // When Add Rule button is clicked, show Popup
        auto on_open_add_rule = [this, item, config, add_btn]() {
            if (m_active_popup) {
                m_active_popup->dismiss();
                m_active_popup = nullptr;
            }

            auto popup_col = LinearLayoutBuilder::create()
                ->orientation(Orientation::Vertical)
                ->padding(16, 14)
                ->spacing(10)
                ->backgroundColor(config->colors.surface)
                ->stroke(1, config->colors.outline.with_alpha(0.4f))
                ->cornerRadius(get_card_radius(config))
                ->build();

            auto pop_title = TextViewBuilder::create()
                ->text("Add Firewall Rule")
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.on_surface)
                ->build();
            popup_col->add_view(pop_title);

            auto name_lbl = TextViewBuilder::create()
                ->text("Rule Name / Service")
                ->caption(true)
                ->textColor(config->colors.on_surface_variant.with_alpha(0.8f))
                ->build();
            popup_col->add_view(name_lbl);

            auto name_edit = EditTextBuilder::create()
                ->text("")
                ->hint("e.g. Plex, Syncthing, Deluge")
                ->editable(true)
                ->padding(12, 6)
                ->build();
            LayoutParams n_lp;
            n_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            n_lp.height = 36;
            popup_col->add_view(name_edit, n_lp);

            auto port_lbl = TextViewBuilder::create()
                ->text("Port / Protocol")
                ->caption(true)
                ->textColor(config->colors.on_surface_variant.with_alpha(0.8f))
                ->build();
            popup_col->add_view(port_lbl);

            auto port_edit = EditTextBuilder::create()
                ->text("")
                ->hint("e.g. 8080/tcp, 22000/udp")
                ->editable(true)
                ->padding(12, 6)
                ->build();
            LayoutParams p_lp;
            p_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            p_lp.height = 36;
            popup_col->add_view(port_edit, p_lp);

            // Action buttons
            auto btn_box = LinearLayoutBuilder::create()
                ->orientation(Orientation::Horizontal)
                ->spacing(8)
                ->gravity(Gravity::End)
                ->build();

            auto cancel_btn = ButtonBuilder::create()
                ->text("Cancel")
                ->style(ButtonStyle::Outlined)
                ->padding(12, 4)
                ->cornerRadius(get_btn_radius(config))
                ->onClick([this]() {
                    if (m_active_popup) {
                        m_active_popup->dismiss();
                        m_active_popup = nullptr;
                    }
                })
                ->build();
            btn_box->add_view(cancel_btn);

            auto submit_btn = ButtonBuilder::create()
                ->text("Add")
                ->primary(true)
                ->padding(14, 4)
                ->cornerRadius(get_btn_radius(config))
                ->onClick([this, item, name_edit, port_edit]() {
                    std::string r_name = name_edit->get_text();
                    std::string r_port = port_edit->get_text();
                    if (r_name.empty()) r_name = "Custom Port";
                    if (r_port.empty()) r_port = "8080/tcp";
                    item->firewall_cfg.custom_rules.push_back({r_name, r_port, true});
                    if (m_active_popup) {
                        m_active_popup->dismiss();
                        m_active_popup = nullptr;
                    }
                    deferred_rebuild_preview();
                    notify_state_changed();
                })
                ->build();
            btn_box->add_view(submit_btn);

            popup_col->add_view(btn_box);

            m_active_popup = PopupWindowBuilder::create()
                ->content(popup_col)
                ->width(280)
                ->elevation(6)
                ->cornerRadius(get_card_radius(config))
                ->dismissOnOutsideClick(true)
                ->dismissOnEscape(true)
                ->build();

            m_active_popup->show_as_dropdown(add_btn, PopupGravity::BottomEnd);
        };

        add_btn->set_on_click_listener(on_open_add_rule);

        LayoutParams add_btn_lp;
        add_btn_lp.width = static_cast<int>(LayoutDimension::WrapContent);
        add_btn_lp.height = 28;
        add_btn_lp.gravity = Gravity::CenterVertical;
        rules_hdr->add_view(add_btn, add_btn_lp);

        rules_card->add_view(rules_hdr);

        // Vertically stacked switches helper
        auto add_rule_row = [&](const std::string& name, const std::string& desc, bool is_checked, std::function<void(bool)> on_toggle) {
            auto row = LinearLayoutBuilder::create()
                ->orientation(Orientation::Horizontal)
                ->spacing(12)
                ->padding(8, 4)
                ->backgroundColor(Color::transparent())
                ->cornerRadius(get_inner_radius(config))
                ->gravity(Gravity::CenterVertical)
                ->build();

            auto box = LinearLayoutBuilder::create()
                ->orientation(Orientation::Vertical)
                ->spacing(2)
                ->build();

            auto lbl = TextViewBuilder::create()
                ->text(name)
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.on_surface)
                ->build();
            box->add_view(lbl);

            auto sub = TextViewBuilder::create()
                ->text(desc)
                ->caption(true)
                ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
                ->build();
            box->add_view(sub);

            LayoutParams box_lp;
            box_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            box_lp.weight = 1.0f;
            box_lp.gravity = Gravity::CenterVertical;
            row->add_view(box, box_lp);

            auto sw = SwitchBuilder::create()
                ->checked(is_checked)
                ->onCheckedChanged([this, on_toggle](bool c) {
                    if (on_toggle) on_toggle(c);
                    notify_state_changed();
                })
                ->build();
            LayoutParams sw_lp;
            sw_lp.width = 44;
            sw_lp.height = 24;
            sw_lp.gravity = Gravity::CenterVertical;
            row->add_view(sw, sw_lp);

            rules_card->add_view(row);
        };

        // Predefined common rules
        add_rule_row("KDE Connect", "Allow local device pairing & clipboard sync (UDP/TCP 1714–1764)",
                     item->firewall_cfg.allow_kdeconnect, [item](bool c) { item->firewall_cfg.allow_kdeconnect = c; });

        add_rule_row("SSH Server", "Allow secure shell remote access (TCP 22)",
                     item->firewall_cfg.allow_ssh, [item](bool c) { item->firewall_cfg.allow_ssh = c; });

        add_rule_row("Samba File Sharing", "Allow local network folder sharing (TCP 445 / UDP 137–138)",
                     item->firewall_cfg.allow_samba, [item](bool c) { item->firewall_cfg.allow_samba = c; });

        add_rule_row("Network Printing (CUPS)", "Allow local IPP printer discovery (UDP/TCP 631)",
                     item->firewall_cfg.allow_cups, [item](bool c) { item->firewall_cfg.allow_cups = c; });

        // User-added custom rules
        for (size_t i = 0; i < item->firewall_cfg.custom_rules.size(); ++i) {
            auto& crule = item->firewall_cfg.custom_rules[i];
            add_rule_row(crule.name, "Custom port rule (" + crule.port_proto + ")",
                         crule.enabled, [&crule](bool c) { crule.enabled = c; });
        }

        add_preview_card(rules_card);

        // 5. Monitor Card: Can be disabled with switch
        auto monitor_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(10)
            ->padding(16, 14)
            ->backgroundColor(config->colors.surface)
            ->stroke(1, config->colors.outline.with_alpha(0.3f))
            ->cornerRadius(get_card_radius(config))
            ->build();

        auto mon_hdr = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->spacing(12)
            ->gravity(Gravity::CenterVertical)
            ->build();

        auto mon_icon = TextViewBuilder::create()
            ->text("󰒍")
            ->textSize(config->metrics.font_size + 2)
            ->textColor(item->firewall_cfg.monitor_enabled ? config->colors.primary : config->colors.on_surface_variant)
            ->build();
        LayoutParams mico_lp;
        mico_lp.width = 24;
        mico_lp.gravity = Gravity::CenterVertical;
        mon_hdr->add_view(mon_icon, mico_lp);

        auto mon_lbl_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();

        auto mon_title = TextViewBuilder::create()
            ->text("Monitor")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        mon_lbl_box->add_view(mon_title);

        auto mon_sub = TextViewBuilder::create()
            ->text(item->firewall_cfg.monitor_enabled ? "Active • Real-time socket & probe auditing" : "Disabled • Logging and packet inspection inactive")
            ->caption(true)
            ->textColor(item->firewall_cfg.monitor_enabled ? config->colors.primary : config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        mon_lbl_box->add_view(mon_sub);

        LayoutParams mlbl_lp;
        mlbl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        mlbl_lp.weight = 1.0f;
        mlbl_lp.gravity = Gravity::CenterVertical;
        mon_hdr->add_view(mon_lbl_box, mlbl_lp);

        auto mon_sw = SwitchBuilder::create()
            ->checked(item->firewall_cfg.monitor_enabled)
            ->onCheckedChanged([this, item](bool checked) {
                item->firewall_cfg.monitor_enabled = checked;
                deferred_rebuild_preview();
                notify_state_changed();
            })
            ->build();
        LayoutParams mon_sw_lp;
        mon_sw_lp.width = 44;
        mon_sw_lp.height = 24;
        mon_sw_lp.gravity = Gravity::CenterVertical;
        mon_hdr->add_view(mon_sw, mon_sw_lp);

        monitor_card->add_view(mon_hdr);

        if (item->firewall_cfg.monitor_enabled) {
            std::vector<TelemetryEntry> mon_telemetry = {
                {"Backend Engine", "UFW (iptables / netfilter)"},
                {"Blocked Probes Today", std::to_string(item->firewall_cfg.blocked_today) + " unauthorized requests"},
                {"Active Connections", std::to_string(item->firewall_cfg.active_connections) + " running sockets"},
                {"Monitored Interfaces", item->firewall_cfg.monitored_interfaces},
                {"Last Blocked Event", item->firewall_cfg.last_blocked_probe}
            };

            for (const auto& entry : mon_telemetry) {
                auto mrow = LinearLayoutBuilder::create()
                    ->orientation(Orientation::Horizontal)
                    ->padding(2, 2)
                    ->build();

                auto mk = TextViewBuilder::create()
                    ->text(entry.key)
                    ->caption(true)
                    ->textColor(config->colors.on_surface_variant)
                    ->build();
                LayoutParams mk_lp;
                mk_lp.width = 150;
                mrow->add_view(mk, mk_lp);

                auto mv = TextViewBuilder::create()
                    ->text(entry.value)
                    ->caption(true)
                    ->textColor(config->colors.on_surface)
                    ->build();
                LayoutParams mv_lp;
                mv_lp.width = static_cast<int>(LayoutDimension::MatchParent);
                mv_lp.weight = 1.0f;
                mrow->add_view(mv, mv_lp);

                monitor_card->add_view(mrow);
            }
        }

        add_preview_card(monitor_card);
    }

    return;
}

void MillerView::render_app_network_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    auto card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(16, 14)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto icon = TextViewBuilder::create()
        ->text("󰋽")
        ->textSize(config->metrics.font_size + 2)
        ->textColor(config->colors.primary)
        ->build();
    LayoutParams icon_lp;
    icon_lp.width = 24;
    icon_lp.gravity = Gravity::CenterVertical;
    card->add_view(icon, icon_lp);

    auto hint = TextViewBuilder::create()
        ->text("Please go to Applications->Installed App->App Info->Network Access.")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface_variant)
        ->build();

    LayoutParams hint_lp;
    hint_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    hint_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    hint_lp.weight = 1.0f;
    hint_lp.gravity = Gravity::CenterVertical;
    card->add_view(hint, hint_lp);

    add_preview_card(card);
    return;
}


} // namespace miqu
