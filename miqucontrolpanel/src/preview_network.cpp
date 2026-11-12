#include "miller_view.hpp"
#include "preview_common.hpp"
#include "wifi_backend.hpp"
#include "bluetooth_backend.hpp"
#include "bluetooth_agent.hpp"
#include "ethernet_backend.hpp"
#include "firewall_backend.hpp"
#include "airplane_backend.hpp"
#include "dns_backend.hpp"
#include "vpn_backend.hpp"
#include <miqutoolkit/view/popup_window.hpp>
#include <miqutoolkit/view/edit_text.hpp>
#include <miqutoolkit/core/app_engine.hpp>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <thread>

namespace fs = std::filesystem;

namespace miqu {
void MillerView::render_wifi_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    // Sync with live backend if available and not actively in a background connect/scan
    if (WifiBackend::is_wifi_available()) {
        if (item->last_action_message.find("Connecting") == std::string::npos &&
            item->last_action_message.find("Scanning") == std::string::npos) {
            WifiBackend::sync_item_state(item);
        }
    }

    // Determine connected SSID
    std::string connected_ssid;
    if (item->switch_val) {
        for (const auto& n : item->wifi_networks) {
            if (n.status == "Connected") {
                connected_ssid = n.ssid;
                break;
            }
        }
    }

    // 1. Wi-Fi Power Toggle Card (Just like mute button of speaker)
    auto wifi_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(16, 14)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    std::string wifi_top_icon = !item->switch_val ? "󰤮" : (!connected_ssid.empty() ? "󰤨" : "󰤟");
    Color wifi_icon_color = (!item->switch_val || connected_ssid.empty())
        ? config->colors.on_surface_variant
        : config->colors.primary;

    auto wifi_icon = TextViewBuilder::create()
        ->text(wifi_top_icon)
        ->textSize(config->metrics.font_size + 4)
        ->textColor(wifi_icon_color)
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

    std::string wifi_hint_str;
    if (!item->switch_val) {
        wifi_hint_str = "Wi-Fi is turned off";
    } else if (!connected_ssid.empty()) {
        wifi_hint_str = "Connected to " + connected_ssid;
    } else {
        wifi_hint_str = "Not connected to any network";
    }

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
            item->last_action_message = checked ? "Turning Wi-Fi on..." : "Turning Wi-Fi off...";
            std::thread([this, item, checked]() {
                WifiBackend::set_radio_enabled(checked);
                std::this_thread::sleep_for(std::chrono::milliseconds(800));
                if (auto* eng = AppEngine::instance()) {
                    eng->post_delayed(0, [this, item]() {
                        WifiBackend::sync_item_state(item);
                        item->last_action_message = "";
                        rebuild_current_column();
                        deferred_rebuild_preview();
                        notify_state_changed();
                    });
                }
            }).detach();
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
            item->last_action_message = "Scanning for networks...";
            rebuild_preview_column();
            std::thread([this, item]() {
                WifiBackend::rescan_async();
                std::this_thread::sleep_for(std::chrono::milliseconds(1800));
                if (auto* eng = AppEngine::instance()) {
                    eng->post_delayed(0, [this, item]() {
                        WifiBackend::sync_item_state(item);
                        item->last_action_message = "Scan completed • " + std::to_string(item->wifi_networks.size()) + " networks found";
                        deferred_rebuild_preview();
                        notify_state_changed();
                    });
                }
            }).detach();
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
            const auto& net = item->wifi_networks[ni];
            bool is_conn = (net.status == "Connected");

            auto row_content = LinearLayoutBuilder::create()
                ->orientation(Orientation::Horizontal)
                ->padding(12, 10)
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

            // 3-Line Network Information Box:
            // Line 1: Title (SSID)
            // Line 2: Status (Connected / Saved / Connect / Connecting...)
            // Line 3: Details (Security • Frequency/Channel • Signal • Bitrate • BSSID)
            auto text_box = LinearLayoutBuilder::create()
                ->orientation(Orientation::Vertical)
                ->spacing(2)
                ->build();

            // Line 1: Title
            auto ssid_text = TextViewBuilder::create()
                ->text(net.ssid)
                ->textSize(config->metrics.font_size)
                ->textColor(is_conn ? config->colors.primary : config->colors.on_surface)
                ->bold(is_conn)
                ->ellipsize(true)
                ->build();
            text_box->add_view(ssid_text);

            // Line 2: Status
            Color sub_color = is_conn ? config->colors.primary :
                              (net.status == "Saved" ? config->colors.on_surface_variant : config->colors.on_surface_variant.with_alpha(0.7f));
            auto status_text = TextViewBuilder::create()
                ->text(net.status)
                ->caption(true)
                ->textColor(sub_color)
                ->build();
            text_box->add_view(status_text);

            // Line 3: Details
            std::string details_str = net.details;
            if (details_str.empty()) {
                std::vector<std::string> parts;
                if (!net.security.empty()) parts.push_back(net.security);
                std::string freq_chan = net.frequency;
                if (!net.channel.empty()) freq_chan += " (Ch " + net.channel + ")";
                if (!freq_chan.empty()) parts.push_back(freq_chan);
                if (net.signal_percent > 0) parts.push_back(std::to_string(net.signal_percent) + "%");
                if (!net.rate.empty()) parts.push_back(net.rate);
                if (!net.bssid.empty()) parts.push_back(net.bssid);

                for (size_t pi = 0; pi < parts.size(); ++pi) {
                    if (pi > 0) details_str += " • ";
                    details_str += parts[pi];
                }
            }
            if (!details_str.empty()) {
                auto details_text = TextViewBuilder::create()
                    ->text(details_str)
                    ->caption(true)
                    ->textColor(config->colors.on_surface_variant.with_alpha(0.65f))
                    ->ellipsize(true)
                    ->build();
                text_box->add_view(details_text);
            }

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
                    ->bold(true)
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
            int stroke_w = is_conn ? 1 : 0;
            Color stroke_col = is_conn ? config->colors.primary.with_alpha(0.5f) : Color::transparent();

            auto net_row_card = HoverCardViewBuilder::create()
                ->selected(is_conn, row_bg, stroke_w, stroke_col)
                ->cornerRadius(get_inner_radius(config))
                ->elevation(0)
                ->addView(row_content)
                ->build();

            // Connect / Manage Network Action
            auto on_connect = [this, item, net, net_row_card, config]() {
                if (m_active_popup) {
                    m_active_popup->dismiss();
                    m_active_popup = nullptr;
                }

                // If currently connected: show Disconnect / Forget options popup
                if (net.status == "Connected") {
                    auto popup_col = LinearLayoutBuilder::create()
                        ->orientation(Orientation::Vertical)
                        ->spacing(10)
                        ->padding(14, 12)
                        ->backgroundColor(config->colors.surface)
                        ->stroke(1, config->colors.outline.with_alpha(0.3f))
                        ->cornerRadius(get_card_radius(config))
                        ->build();

                    auto title = TextViewBuilder::create()
                        ->text(net.ssid)
                        ->textSize(config->metrics.font_size)
                        ->bold(true)
                        ->textColor(config->colors.on_surface)
                        ->build();
                    popup_col->add_view(title);

                    auto sub = TextViewBuilder::create()
                        ->text("Active Wi-Fi Connection")
                        ->caption(true)
                        ->textColor(config->colors.primary)
                        ->build();
                    popup_col->add_view(sub);

                    auto btn_row = LinearLayoutBuilder::create()
                        ->orientation(Orientation::Horizontal)
                        ->spacing(8)
                        ->gravity(Gravity::End)
                        ->build();

                    auto btn_forget = ButtonBuilder::create()
                        ->text("Forget")
                        ->style(ButtonStyle::Outlined)
                        ->padding(10, 4)
                        ->cornerRadius(get_btn_radius(config))
                        ->onClick([this, item, net]() {
                            if (m_active_popup) {
                                m_active_popup->dismiss();
                                m_active_popup = nullptr;
                            }
                            item->last_action_message = "Forgetting " + net.ssid + "...";
                            rebuild_preview_column();
                            std::thread([this, item, net]() {
                                WifiBackend::forget_network(net.ssid);
                                if (auto* eng = AppEngine::instance()) {
                                    eng->post_delayed(500, [this, item, net]() {
                                        WifiBackend::sync_item_state(item);
                                        item->last_action_message = "Removed network " + net.ssid;
                                        deferred_rebuild_preview();
                                        notify_state_changed();
                                    });
                                }
                            }).detach();
                        })
                        ->build();
                    btn_row->add_view(btn_forget);

                    auto btn_disc = ButtonBuilder::create()
                        ->text("Disconnect")
                        ->style(ButtonStyle::Standard)
                        ->textColor(config->colors.primary)
                        ->padding(10, 4)
                        ->cornerRadius(get_btn_radius(config))
                        ->onClick([this, item]() {
                            if (m_active_popup) {
                                m_active_popup->dismiss();
                                m_active_popup = nullptr;
                            }
                            item->last_action_message = "Disconnecting...";
                            rebuild_preview_column();
                            std::thread([this, item]() {
                                WifiBackend::disconnect_current();
                                if (auto* eng = AppEngine::instance()) {
                                    eng->post_delayed(800, [this, item]() {
                                        WifiBackend::sync_item_state(item);
                                        item->last_action_message = "Disconnected";
                                        deferred_rebuild_preview();
                                        notify_state_changed();
                                    });
                                }
                            }).detach();
                        })
                        ->build();
                    btn_row->add_view(btn_disc);

                    popup_col->add_view(btn_row);

                    m_active_popup = PopupWindowBuilder::create()
                        ->content(popup_col)
                        ->width(280)
                        ->elevation(6)
                        ->cornerRadius(get_card_radius(config))
                        ->dismissOnOutsideClick(true)
                        ->dismissOnEscape(true)
                        ->build();
                    m_active_popup->show_as_dropdown(net_row_card, PopupGravity::BottomCenter);
                    return;
                }

                // If saved or open: connect directly without prompting
                if (net.status == "Saved" || !net.secured) {
                    item->last_action_message = "Connecting to " + net.ssid + "...";
                    for (auto& n : item->wifi_networks) {
                        if (n.ssid == net.ssid) n.status = "Connecting...";
                    }
                    rebuild_preview_column();

                    std::thread([this, item, net]() {
                        bool ok = WifiBackend::connect_network(net.ssid);
                        if (auto* eng = AppEngine::instance()) {
                            eng->post_delayed(1500, [this, item, net, ok]() {
                                WifiBackend::sync_item_state(item);
                                item->last_action_message = ok ? ("Connected to " + net.ssid) : ("Failed to connect to " + net.ssid);
                                deferred_rebuild_preview();
                                notify_state_changed();
                            });
                        }
                    }).detach();
                    return;
                }

                // Otherwise, secured & not saved: prompt password in popup modal
                auto popup_col = LinearLayoutBuilder::create()
                    ->orientation(Orientation::Vertical)
                    ->spacing(12)
                    ->padding(16, 14)
                    ->backgroundColor(config->colors.surface)
                    ->stroke(1, config->colors.outline.with_alpha(0.3f))
                    ->cornerRadius(get_card_radius(config))
                    ->build();

                auto title = TextViewBuilder::create()
                    ->text("Connect to " + net.ssid)
                    ->textSize(config->metrics.font_size)
                    ->bold(true)
                    ->textColor(config->colors.on_surface)
                    ->build();
                popup_col->add_view(title);

                if (!net.details.empty()) {
                    auto det = TextViewBuilder::create()
                        ->text(net.details)
                        ->caption(true)
                        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
                        ->ellipsize(true)
                        ->build();
                    popup_col->add_view(det);
                }

                auto pwd_edit = EditTextBuilder::create()
                    ->text("")
                    ->hint("Enter Wi-Fi password")
                    ->passwordMode(true)
                    ->editable(true)
                    ->padding(12, 6)
                    ->build();
                LayoutParams p_lp;
                p_lp.width = static_cast<int>(LayoutDimension::MatchParent);
                p_lp.height = 36;
                popup_col->add_view(pwd_edit, p_lp);

                // Show password row
                auto show_row = LinearLayoutBuilder::create()
                    ->orientation(Orientation::Horizontal)
                    ->spacing(8)
                    ->gravity(Gravity::CenterVertical)
                    ->build();

                auto sw_reveal = SwitchBuilder::create()
                    ->checked(false)
                    ->onCheckedChanged([pwd_edit](bool checked) {
                        pwd_edit->set_password_mode(!checked);
                        pwd_edit->request_redraw();
                    })
                    ->build();
                LayoutParams swr_lp;
                swr_lp.width = 44;
                swr_lp.height = 24;
                show_row->add_view(sw_reveal, swr_lp);

                auto show_lbl = TextViewBuilder::create()
                    ->text("Show password")
                    ->caption(true)
                    ->textColor(config->colors.on_surface_variant)
                    ->build();
                show_row->add_view(show_lbl);
                popup_col->add_view(show_row);

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

                auto conn_btn = ButtonBuilder::create()
                    ->text("Connect")
                    ->primary(true)
                    ->padding(14, 4)
                    ->cornerRadius(get_btn_radius(config))
                    ->onClick([this, item, net, pwd_edit]() {
                        std::string pwd = pwd_edit->get_text();
                        if (m_active_popup) {
                            m_active_popup->dismiss();
                            m_active_popup = nullptr;
                        }
                        item->last_action_message = "Connecting to " + net.ssid + "...";
                        for (auto& n : item->wifi_networks) {
                            if (n.ssid == net.ssid) n.status = "Connecting...";
                        }
                        rebuild_preview_column();

                        std::thread([this, item, net, pwd]() {
                            bool ok = WifiBackend::connect_network(net.ssid, pwd);
                            if (auto* eng = AppEngine::instance()) {
                                eng->post_delayed(2000, [this, item, net, ok]() {
                                    WifiBackend::sync_item_state(item);
                                    item->last_action_message = ok ? ("Connected to " + net.ssid) : ("Failed to connect to " + net.ssid);
                                    deferred_rebuild_preview();
                                    notify_state_changed();
                                });
                            }
                        }).detach();
                    })
                    ->build();
                btn_box->add_view(conn_btn);

                popup_col->add_view(btn_box);

                m_active_popup = PopupWindowBuilder::create()
                    ->content(popup_col)
                    ->width(320)
                    ->elevation(6)
                    ->cornerRadius(get_card_radius(config))
                    ->dismissOnOutsideClick(true)
                    ->dismissOnEscape(true)
                    ->build();
                m_active_popup->show_as_dropdown(net_row_card, PopupGravity::BottomCenter);
            };

            net_row_card->set_on_click_listener(on_connect);
            net_card->add_view(net_row_card);
        }

        add_preview_card(net_card);
    }

    // 3. Interface Specifications (Live telemetry from backend)
    if (!item->telemetry.empty()) {
        add_preview_card(create_telemetry_card(item->telemetry, config, "Wireless interface specifications"));
    }

    return;
}

void MillerView::init_bluetooth_agent() {
    if (!BluetoothBackend::is_bluetooth_installed() || !BluetoothBackend::is_bluetooth_available()) {
        return;
    }

    if (!m_bt_agent) {
        m_bt_agent = std::make_unique<BluetoothAgent>();
    }

    // 1. Numeric Comparison (SSP) pairing callback - standard for modern phones, tablets, PCs
    m_bt_agent->set_on_request_confirmation([this](const std::string& dev_name, const std::string& mac, uint32_t passkey, BluetoothAgent::ConfirmCallback cb) {
        show_numeric_comparison_dialog(dev_name, mac, passkey, cb);
    });

    // 2. Passkey Display callback - for typing passkey on wireless keyboards
    m_bt_agent->set_on_display_passkey([this](const std::string& dev_name, const std::string& mac, uint32_t passkey) {
        show_passkey_display_dialog(dev_name, mac, passkey);
    });

    // 3. Authorization callback - for incoming connection requests
    m_bt_agent->set_on_request_authorization([this](const std::string& dev_name, const std::string& mac, BluetoothAgent::AuthorizationCallback cb) {
        show_authorization_dialog(dev_name, mac, cb);
    });

    // 4. PIN code callback - for legacy devices or PIN verification (e.g. 0000 for headsets)
    m_bt_agent->set_on_request_pin([this](const std::string& dev_name, const std::string& mac, BluetoothAgent::PinCallback cb) {
        show_pin_request_dialog(dev_name, mac, cb);
    });

    // 5. Cancel callback - when pairing request expires or is cancelled on remote device
    m_bt_agent->set_on_cancel([this]() {
        if (m_active_pairing_popup) {
            m_active_pairing_popup->dismiss();
            m_active_pairing_popup = nullptr;
        }
        deferred_rebuild_preview();
    });

    m_bt_agent->start();
}

void MillerView::show_numeric_comparison_dialog(
    const std::string& dev_name,
    const std::string& mac,
    uint32_t passkey,
    std::function<void(bool)> cb
) {
    if (m_active_pairing_popup) {
        m_active_pairing_popup->dismiss();
        m_active_pairing_popup = nullptr;
    }

    auto config = Config::get();

    // Format 6-digit passkey with leading zeros (e.g. "384 291")
    std::ostringstream ss;
    ss << std::setw(6) << std::setfill('0') << passkey;
    std::string raw_code = ss.str();
    std::string formatted_code = raw_code.substr(0, 3) + " " + raw_code.substr(3, 3);

    auto card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(20, 18)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.35f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    // Header row with Bluetooth icon and device info
    auto hdr = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(10)
        ->gravity(Gravity::CenterVertical)
        ->build();

    auto icon = TextViewBuilder::create()
        ->text("󰂯")
        ->textSize(config->metrics.font_size + 4)
        ->textColor(config->colors.primary)
        ->build();
    hdr->add_view(icon);

    auto title_col = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(2)
        ->build();

    auto title = TextViewBuilder::create()
        ->text("Bluetooth Pairing Request")
        ->bold(true)
        ->textSize(config->metrics.font_size + 1)
        ->textColor(config->colors.on_surface)
        ->build();
    title_col->add_view(title);

    auto subtitle = TextViewBuilder::create()
        ->text(dev_name + " (" + mac + ")")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    title_col->add_view(subtitle);

    LayoutParams tcol_lp;
    tcol_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    tcol_lp.weight = 1.0f;
    hdr->add_view(title_col, tcol_lp);
    card->add_view(hdr);

    // Passkey display box
    auto pass_box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(14, 12)
        ->spacing(4)
        ->gravity(Gravity::CenterHorizontal)
        ->backgroundColor(config->colors.surface_variant.with_alpha(0.5f))
        ->cornerRadius(get_inner_radius(config))
        ->build();

    auto pass_lbl = TextViewBuilder::create()
        ->text("CONFIRM PAIRING PASSKEY")
        ->caption(true)
        ->bold(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.8f))
        ->build();
    pass_box->add_view(pass_lbl);

    auto pass_code = TextViewBuilder::create()
        ->text(formatted_code)
        ->bold(true)
        ->textSize(config->metrics.font_size + 12)
        ->textColor(config->colors.primary)
        ->build();
    pass_box->add_view(pass_code);
    card->add_view(pass_box);

    auto desc = TextViewBuilder::create()
        ->text("Does the 6-digit code on " + dev_name + " match the passkey above? Confirm on both devices to authorize connection.")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    card->add_view(desc);

    // Button row
    auto btn_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(8)
        ->gravity(Gravity::End)
        ->build();

    auto cancel_btn = ButtonBuilder::create()
        ->text("Reject")
        ->style(ButtonStyle::Outlined)
        ->padding(12, 6)
        ->cornerRadius(get_btn_radius(config))
        ->onClick([this, cb, dev_name]() {
            if (m_active_pairing_popup) {
                m_active_pairing_popup->dismiss();
                m_active_pairing_popup = nullptr;
            }
            cb(false);
            auto cur = get_current_selected_item();
            if (cur) {
                cur->last_action_message = "Pairing rejected with " + dev_name;
                deferred_rebuild_preview();
                notify_state_changed();
            }
        })
        ->build();
    btn_row->add_view(cancel_btn);

    auto pair_btn = ButtonBuilder::create()
        ->text("Confirm & Pair")
        ->primary(true)
        ->padding(14, 6)
        ->cornerRadius(get_btn_radius(config))
        ->onClick([this, cb, dev_name]() {
            if (m_active_pairing_popup) {
                m_active_pairing_popup->dismiss();
                m_active_pairing_popup = nullptr;
            }
            cb(true);
            auto cur = get_current_selected_item();
            if (cur) {
                cur->last_action_message = "Pairing confirmed with " + dev_name + " • Bonding...";
                deferred_rebuild_preview();
                notify_state_changed();
            }
        })
        ->build();
    btn_row->add_view(pair_btn);
    card->add_view(btn_row);

    m_active_pairing_popup = PopupWindowBuilder::create()
        ->content(card)
        ->width(360)
        ->elevation(12)
        ->cornerRadius(get_card_radius(config))
        ->dismissOnOutsideClick(false)
        ->dismissOnEscape(true)
        ->build();

    auto anchor = m_preview_ribbon ? m_preview_ribbon : (m_preview_pillar ? m_preview_pillar : shared_from_this());
    m_active_pairing_popup->show_as_dropdown(anchor, PopupGravity::BottomCenter);
    request_redraw();
    if (auto* win = get_window()) {
        win->schedule_redraw();
    }
}

void MillerView::show_passkey_display_dialog(const std::string& dev_name, const std::string& mac, uint32_t passkey) {
    if (m_active_pairing_popup) {
        m_active_pairing_popup->dismiss();
        m_active_pairing_popup = nullptr;
    }
    auto config = Config::get();
    std::ostringstream ss;
    ss << std::setw(6) << std::setfill('0') << passkey;
    std::string raw_code = ss.str();
    std::string formatted_code = raw_code.substr(0, 3) + " " + raw_code.substr(3, 3);

    auto card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(20, 18)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.35f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto title = TextViewBuilder::create()
        ->text("Type Passkey on Keyboard")
        ->bold(true)
        ->textSize(config->metrics.font_size + 1)
        ->textColor(config->colors.on_surface)
        ->build();
    card->add_view(title);

    auto pass_code = TextViewBuilder::create()
        ->text(formatted_code)
        ->bold(true)
        ->textSize(config->metrics.font_size + 12)
        ->textColor(config->colors.primary)
        ->build();
    card->add_view(pass_code);

    auto desc = TextViewBuilder::create()
        ->text("Type this 6-digit code on " + dev_name + " (" + mac + ") and press Return / Enter.")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    card->add_view(desc);

    auto close_btn = ButtonBuilder::create()
        ->text("Dismiss")
        ->style(ButtonStyle::Outlined)
        ->padding(12, 6)
        ->cornerRadius(get_btn_radius(config))
        ->onClick([this]() {
            if (m_active_pairing_popup) {
                m_active_pairing_popup->dismiss();
                m_active_pairing_popup = nullptr;
            }
        })
        ->build();
    card->add_view(close_btn);

    m_active_pairing_popup = PopupWindowBuilder::create()
        ->content(card)
        ->width(340)
        ->elevation(12)
        ->cornerRadius(get_card_radius(config))
        ->dismissOnOutsideClick(true)
        ->dismissOnEscape(true)
        ->build();

    auto anchor = m_preview_ribbon ? m_preview_ribbon : (m_preview_pillar ? m_preview_pillar : shared_from_this());
    m_active_pairing_popup->show_as_dropdown(anchor, PopupGravity::BottomCenter);
    request_redraw();
    if (auto* win = get_window()) {
        win->schedule_redraw();
    }
}

void MillerView::show_authorization_dialog(const std::string& dev_name, const std::string& mac, std::function<void(bool)> cb) {
    if (m_active_pairing_popup) {
        m_active_pairing_popup->dismiss();
        m_active_pairing_popup = nullptr;
    }
    auto config = Config::get();

    auto card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(18, 16)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.35f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto title = TextViewBuilder::create()
        ->text("Bluetooth Connection Request")
        ->bold(true)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    card->add_view(title);

    auto desc = TextViewBuilder::create()
        ->text("Device " + dev_name + " (" + mac + ") is requesting to connect. Authorize this connection?")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    card->add_view(desc);

    auto btn_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(8)
        ->gravity(Gravity::End)
        ->build();

    auto btn_deny = ButtonBuilder::create()
        ->text("Deny")
        ->style(ButtonStyle::Outlined)
        ->padding(10, 4)
        ->cornerRadius(get_btn_radius(config))
        ->onClick([this, cb]() {
            if (m_active_pairing_popup) {
                m_active_pairing_popup->dismiss();
                m_active_pairing_popup = nullptr;
            }
            cb(false);
        })
        ->build();
    btn_row->add_view(btn_deny);

    auto btn_allow = ButtonBuilder::create()
        ->text("Allow")
        ->primary(true)
        ->padding(12, 4)
        ->cornerRadius(get_btn_radius(config))
        ->onClick([this, cb]() {
            if (m_active_pairing_popup) {
                m_active_pairing_popup->dismiss();
                m_active_pairing_popup = nullptr;
            }
            cb(true);
        })
        ->build();
    btn_row->add_view(btn_allow);
    card->add_view(btn_row);

    m_active_pairing_popup = PopupWindowBuilder::create()
        ->content(card)
        ->width(320)
        ->elevation(12)
        ->cornerRadius(get_card_radius(config))
        ->dismissOnOutsideClick(false)
        ->dismissOnEscape(true)
        ->build();

    auto anchor = m_preview_ribbon ? m_preview_ribbon : (m_preview_pillar ? m_preview_pillar : shared_from_this());
    m_active_pairing_popup->show_as_dropdown(anchor, PopupGravity::BottomCenter);
    request_redraw();
    if (auto* win = get_window()) {
        win->schedule_redraw();
    }
}

void MillerView::show_pin_request_dialog(
    const std::string& dev_name,
    const std::string& mac,
    std::function<void(const std::string&)> cb
) {
    if (m_active_pairing_popup) {
        m_active_pairing_popup->dismiss();
        m_active_pairing_popup = nullptr;
    }
    auto config = Config::get();

    auto card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(18, 16)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.35f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto title = TextViewBuilder::create()
        ->text("Bluetooth PIN Request")
        ->bold(true)
        ->textSize(config->metrics.font_size + 1)
        ->textColor(config->colors.on_surface)
        ->build();
    card->add_view(title);

    auto subtitle = TextViewBuilder::create()
        ->text(dev_name + " (" + mac + ") requires a PIN code to pair. Standard audio accessories typically use 0000.")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    card->add_view(subtitle);

    auto btn_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(8)
        ->gravity(Gravity::End)
        ->build();

    auto btn_reject = ButtonBuilder::create()
        ->text("Reject")
        ->style(ButtonStyle::Outlined)
        ->padding(12, 6)
        ->cornerRadius(get_btn_radius(config))
        ->onClick([this, cb, dev_name]() {
            if (m_active_pairing_popup) {
                m_active_pairing_popup->dismiss();
                m_active_pairing_popup = nullptr;
            }
            cb("");
            auto cur = get_current_selected_item();
            if (cur) {
                cur->last_action_message = "PIN entry rejected for " + dev_name;
                deferred_rebuild_preview();
                notify_state_changed();
            }
        })
        ->build();
    btn_row->add_view(btn_reject);

    auto btn_default_pin = ButtonBuilder::create()
        ->text("Pair (0000)")
        ->primary(true)
        ->padding(14, 6)
        ->cornerRadius(get_btn_radius(config))
        ->onClick([this, cb, dev_name]() {
            if (m_active_pairing_popup) {
                m_active_pairing_popup->dismiss();
                m_active_pairing_popup = nullptr;
            }
            cb("0000");
            auto cur = get_current_selected_item();
            if (cur) {
                cur->last_action_message = "Pairing with " + dev_name + " • Sending PIN 0000...";
                deferred_rebuild_preview();
                notify_state_changed();
            }
        })
        ->build();
    btn_row->add_view(btn_default_pin);
    card->add_view(btn_row);

    m_active_pairing_popup = PopupWindowBuilder::create()
        ->content(card)
        ->width(340)
        ->elevation(12)
        ->cornerRadius(get_card_radius(config))
        ->dismissOnOutsideClick(false)
        ->dismissOnEscape(true)
        ->build();

    auto anchor = m_preview_ribbon ? m_preview_ribbon : (m_preview_pillar ? m_preview_pillar : shared_from_this());
    m_active_pairing_popup->show_as_dropdown(anchor, PopupGravity::BottomCenter);
    request_redraw();
    if (auto* win = get_window()) {
        win->schedule_redraw();
    }
}

void MillerView::render_bluetooth_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    init_bluetooth_agent();
    if (BluetoothBackend::is_bluetooth_installed() && BluetoothBackend::is_bluetooth_available()) {
        if (item->last_action_message.find("Connecting") == std::string::npos &&
            item->last_action_message.find("Scanning") == std::string::npos &&
            item->last_action_message.find("Pairing") == std::string::npos &&
            item->last_action_message.find("Removing") == std::string::npos &&
            item->last_action_message.find("Disconnecting") == std::string::npos &&
            item->last_action_message.find("Turning") == std::string::npos) {
            BluetoothBackend::sync_item_state(item);
        }
    }

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
        BluetoothAdapterInfo adapter = BluetoothBackend::get_adapter_info();
        if (!adapter.name.empty()) {
            bt_hint_str = "Discoverable as " + adapter.name;
        } else {
            bt_hint_str = "Discoverable as Miqu-Desktop";
        }
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
            item->last_action_message = checked ? "Turning Bluetooth on..." : "Turning Bluetooth off...";
            std::thread([this, item, checked]() {
                if (BluetoothBackend::is_bluetooth_installed() && BluetoothBackend::is_bluetooth_available()) {
                    BluetoothBackend::set_powered(checked);
                    std::this_thread::sleep_for(std::chrono::milliseconds(600));
                    if (auto* eng = AppEngine::instance()) {
                        eng->post_delayed(0, [this, item]() {
                            BluetoothBackend::sync_item_state(item);
                            item->last_action_message = "";
                            rebuild_current_column();
                            deferred_rebuild_preview();
                            notify_state_changed();
                        });
                    }
                } else {
                    if (auto* eng = AppEngine::instance()) {
                        eng->post_delayed(300, [this, item]() {
                            item->last_action_message = "";
                            rebuild_current_column();
                            deferred_rebuild_preview();
                            notify_state_changed();
                        });
                    }
                }
            }).detach();
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
            item->last_action_message = "Scanning for nearby Bluetooth devices...";
            rebuild_preview_column();
            notify_state_changed();

            std::thread([this, item]() {
                if (BluetoothBackend::is_bluetooth_installed() && BluetoothBackend::is_bluetooth_available()) {
                    BluetoothBackend::start_scan();
                    std::this_thread::sleep_for(std::chrono::milliseconds(3500));
                    BluetoothBackend::stop_scan();
                    if (auto* eng = AppEngine::instance()) {
                        eng->post_delayed(0, [this, item]() {
                            BluetoothBackend::sync_item_state(item);
                            item->last_action_message = "Scan completed";
                            rebuild_preview_column();
                            notify_state_changed();
                        });
                    }
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                    if (auto* eng = AppEngine::instance()) {
                        eng->post_delayed(0, [this, item]() {
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
                        });
                    }
                }
            }).detach();
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
            int stroke_w = is_conn ? 1 : 0;
            Color stroke_col = is_conn ? config->colors.primary.with_alpha(0.5f) : Color::transparent();

            auto dev_row_card = HoverCardViewBuilder::create()
                ->selected(is_conn, row_bg, stroke_w, stroke_col)
                ->cornerRadius(get_inner_radius(config))
                ->elevation(0)
                ->addView(row_content)
                ->build();

            auto on_dev_connect = [this, item, di, dev_row_card]() {
                if (di >= item->bt_devices.size()) return;
                auto dev = item->bt_devices[di];

                if (BluetoothBackend::is_bluetooth_installed() && BluetoothBackend::is_bluetooth_available() && !dev.mac.empty()) {
                    auto config = Config::get();
                    auto popup_col = LinearLayoutBuilder::create()
                        ->orientation(Orientation::Vertical)
                        ->padding(16, 14)
                        ->spacing(10)
                        ->backgroundColor(config->colors.surface)
                        ->stroke(1, config->colors.outline.with_alpha(0.3f))
                        ->cornerRadius(get_card_radius(config))
                        ->build();

                    auto title = TextViewBuilder::create()
                        ->text(dev.name)
                        ->bold(true)
                        ->textSize(config->metrics.font_size)
                        ->textColor(config->colors.on_surface)
                        ->build();
                    popup_col->add_view(title);

                    std::string sub_info = dev.mac;
                    if (!dev.battery.empty()) {
                        sub_info += " • Battery " + dev.battery;
                    }
                    sub_info += " • " + dev.status;

                    auto sub = TextViewBuilder::create()
                        ->text(sub_info)
                        ->caption(true)
                        ->textColor(config->colors.on_surface_variant)
                        ->build();
                    popup_col->add_view(sub);

                    auto btn_row = LinearLayoutBuilder::create()
                        ->orientation(Orientation::Horizontal)
                        ->spacing(8)
                        ->gravity(Gravity::End)
                        ->build();

                    if (dev.is_paired) {
                        auto btn_forget = ButtonBuilder::create()
                            ->text("Forget")
                            ->style(ButtonStyle::Outlined)
                            ->padding(10, 4)
                            ->cornerRadius(get_btn_radius(config))
                            ->onClick([this, item, dev]() {
                                if (m_active_popup) {
                                    m_active_popup->dismiss();
                                    m_active_popup = nullptr;
                                }
                                item->last_action_message = "Removing " + dev.name + "...";
                                rebuild_preview_column();
                                std::thread([this, item, dev]() {
                                    BluetoothBackend::forget_device(dev.mac);
                                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                                    if (auto* eng = AppEngine::instance()) {
                                        eng->post_delayed(0, [this, item, dev]() {
                                            BluetoothBackend::sync_item_state(item);
                                            item->last_action_message = "Removed device " + dev.name;
                                            deferred_rebuild_preview();
                                            notify_state_changed();
                                        });
                                    }
                                }).detach();
                            })
                            ->build();
                        btn_row->add_view(btn_forget);
                    }

                    if (dev.status == "Connected") {
                        auto btn_disc = ButtonBuilder::create()
                            ->text("Disconnect")
                            ->style(ButtonStyle::Standard)
                            ->textColor(config->colors.primary)
                            ->padding(10, 4)
                            ->cornerRadius(get_btn_radius(config))
                            ->onClick([this, item, dev]() {
                                if (m_active_popup) {
                                    m_active_popup->dismiss();
                                    m_active_popup = nullptr;
                                }
                                item->last_action_message = "Disconnecting " + dev.name + "...";
                                rebuild_preview_column();
                                std::thread([this, item, dev]() {
                                    BluetoothBackend::disconnect_device(dev.mac);
                                    std::this_thread::sleep_for(std::chrono::milliseconds(800));
                                    if (auto* eng = AppEngine::instance()) {
                                        eng->post_delayed(0, [this, item]() {
                                            BluetoothBackend::sync_item_state(item);
                                            item->last_action_message = "Disconnected";
                                            deferred_rebuild_preview();
                                            notify_state_changed();
                                        });
                                    }
                                }).detach();
                            })
                            ->build();
                        btn_row->add_view(btn_disc);
                    } else if (dev.is_paired) {
                        auto btn_conn = ButtonBuilder::create()
                            ->text("Connect")
                            ->primary(true)
                            ->padding(10, 4)
                            ->cornerRadius(get_btn_radius(config))
                            ->onClick([this, item, dev]() {
                                if (m_active_popup) {
                                    m_active_popup->dismiss();
                                    m_active_popup = nullptr;
                                }
                                item->last_action_message = "Connecting to " + dev.name + "...";
                                rebuild_preview_column();
                                std::thread([this, item, dev]() {
                                    BluetoothBackend::connect_device(dev.mac);
                                    bool is_conn = false;
                                    for (int step = 0; step < 6; ++step) {
                                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                                        if (BluetoothBackend::is_device_connected(dev.mac)) {
                                            is_conn = true;
                                            break;
                                        }
                                    }
                                    if (auto* eng = AppEngine::instance()) {
                                        eng->post_delayed(0, [this, item, dev, is_conn]() {
                                            BluetoothBackend::sync_item_state(item);
                                            if (is_conn) {
                                                item->last_action_message = "Connected to " + dev.name;
                                            } else {
                                                item->last_action_message = "Connection failed to " + dev.name + " • Ensure device is powered on";
                                            }
                                            deferred_rebuild_preview();
                                            notify_state_changed();
                                        });
                                    }
                                }).detach();
                            })
                            ->build();
                        btn_row->add_view(btn_conn);
                    } else {
                        // "Tap to pair"
                        auto btn_pair = ButtonBuilder::create()
                            ->text("Pair & Connect")
                            ->primary(true)
                            ->padding(10, 4)
                            ->cornerRadius(get_btn_radius(config))
                            ->onClick([this, item, dev]() {
                                if (m_active_popup) {
                                    m_active_popup->dismiss();
                                    m_active_popup = nullptr;
                                }
                                BluetoothAgent::cache_device_name(dev.mac, dev.name);
                                item->last_action_message = "Pairing with " + dev.name + " • Confirm PIN on both devices...";
                                rebuild_preview_column();
                                std::thread([this, item, dev]() {
                                    BluetoothBackend::pair_and_connect(dev.mac);
                                    bool conn = false;
                                    for (int step = 0; step < 12; ++step) {
                                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                                        if (BluetoothBackend::is_device_connected(dev.mac)) {
                                            conn = true;
                                            break;
                                        }
                                    }
                                    if (auto* eng = AppEngine::instance()) {
                                        eng->post_delayed(0, [this, item, dev, conn]() {
                                            BluetoothBackend::sync_item_state(item);
                                            if (conn) {
                                                item->last_action_message = "Successfully paired and connected to " + dev.name;
                                            } else {
                                                bool is_p = false;
                                                for (const auto& d : item->bt_devices) {
                                                    if (d.mac == dev.mac && d.is_paired) {
                                                        is_p = true;
                                                        break;
                                                    }
                                                }
                                                if (is_p) {
                                                    item->last_action_message = "Paired with " + dev.name;
                                                } else if (!item->last_action_message.empty() && item->last_action_message.find("rejected") != std::string::npos) {
                                                    // Retain user rejection message
                                                } else {
                                                    item->last_action_message = "Pairing incomplete with " + dev.name;
                                                }
                                            }
                                            deferred_rebuild_preview();
                                            notify_state_changed();
                                        });
                                    }
                                }).detach();
                            })
                            ->build();
                        btn_row->add_view(btn_pair);
                    }

                    popup_col->add_view(btn_row);

                    m_active_popup = PopupWindowBuilder::create()
                        ->content(popup_col)
                        ->width(280)
                        ->elevation(6)
                        ->cornerRadius(get_card_radius(config))
                        ->dismissOnOutsideClick(true)
                        ->dismissOnEscape(true)
                        ->build();
                    m_active_popup->show_as_dropdown(dev_row_card, PopupGravity::BottomCenter);
                    return;
                }

                // Mock fallback
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
    if (EthernetBackend::is_networkmanager_available()) {
        if (item->last_action_message.find("Enabling") == std::string::npos &&
            item->last_action_message.find("Disabling") == std::string::npos &&
            item->last_action_message.find("Renewing") == std::string::npos &&
            item->last_action_message.find("Applying") == std::string::npos &&
            item->last_action_message.find("Connecting") == std::string::npos) {
            EthernetBackend::sync_item_state(item);
        }
    }

    bool is_conn = (item->eth_profile.status == "Connected");
    bool has_carrier = item->eth_profile.has_carrier;
    // 1. Primary Ethernet Status Card
    auto eth_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(16, 14)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    std::string eth_icon = is_conn ? "󰈀" : (has_carrier ? "󰈀" : "󰈂");
    auto eth_icon_view = TextViewBuilder::create()
        ->text(eth_icon)
        ->textSize(config->metrics.font_size + 4)
        ->textColor(is_conn ? config->colors.primary : config->colors.on_surface_variant)
        ->build();
    LayoutParams eicon_lp;
    eicon_lp.width = 30;
    eicon_lp.gravity = Gravity::CenterVertical;
    eth_card->add_view(eth_icon_view, eicon_lp);

    auto eth_lbl_box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(2)
        ->build();

    auto eth_title = TextViewBuilder::create()
        ->text("Ethernet")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    eth_lbl_box->add_view(eth_title);

    std::string eth_hint_str;
    if (is_conn) {
        eth_hint_str = item->eth_profile.iface + " • Connected (" + item->eth_profile.ip_address + ")";
        if (item->eth_profile.speed_mbps > 0) {
            eth_hint_str += " • " + std::to_string(item->eth_profile.speed_mbps) + " Mbps";
        }
    } else if (has_carrier) {
        eth_hint_str = item->eth_profile.iface + " • Physical Link Active • Disconnected";
    } else {
        eth_hint_str = item->eth_profile.iface + " • Cable Unplugged";
    }

    auto eth_hint = TextViewBuilder::create()
        ->text(eth_hint_str)
        ->caption(true)
        ->textColor(is_conn ? config->colors.primary : config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    eth_lbl_box->add_view(eth_hint);

    LayoutParams elbl_lp;
    elbl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    elbl_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    elbl_lp.weight = 1.0f;
    elbl_lp.gravity = Gravity::CenterVertical;
    eth_card->add_view(eth_lbl_box, elbl_lp);

    // Status Badge
    std::string badge_text = is_conn ? "✓ Connected" : (has_carrier ? "Link Active" : "Unplugged");
    auto badge_tv = TextViewBuilder::create()
        ->text(badge_text)
        ->caption(true)
        ->textColor(is_conn ? config->colors.primary : config->colors.on_surface_variant)
        ->build();

    auto badge_card = CardViewBuilder::create()
        ->style(CardStyle::Filled)
        ->backgroundColor(is_conn ? config->colors.primary_container.with_alpha(0.35f) : config->colors.surface_variant.with_alpha(0.5f))
        ->cornerRadius(get_inner_radius(config))
        ->elevation(0)
        ->padding(8, 4)
        ->addView(badge_tv)
        ->build();
    LayoutParams badge_lp;
    badge_lp.width = static_cast<int>(LayoutDimension::WrapContent);
    badge_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    badge_lp.gravity = Gravity::CenterVertical;
    eth_card->add_view(badge_card, badge_lp);

    add_preview_card(eth_card);

    // 2. Wired Connection & Configuration Card
    auto profile_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(10)

            ->padding(16, 14)
            ->backgroundColor(config->colors.surface)
            ->stroke(1, config->colors.outline.with_alpha(0.3f))
            ->cornerRadius(get_card_radius(config))
            ->build();

        // Header row with "Wired Connection" title and action buttons
        auto header_row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->gravity(Gravity::CenterVertical)
            ->spacing(8)
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

        // Copy IP button (if assigned)
        if (is_conn && !item->eth_profile.ip_address.empty() && item->eth_profile.ip_address != "None") {
            auto copy_btn = ButtonBuilder::create()
                ->text("Copy IP")
                ->style(ButtonStyle::Standard)
                ->textColor(config->colors.on_surface_variant)
                ->padding(10, 4)
                ->cornerRadius(get_btn_radius(config))
                ->onClick([this, item]() {
                    EthernetBackend::copy_to_clipboard(item->eth_profile.ip_address);
                    item->last_action_message = "Copied " + item->eth_profile.ip_address + " to clipboard";
                    rebuild_preview_column();
                })
                ->build();
            LayoutParams cpy_lp;
            cpy_lp.width = static_cast<int>(LayoutDimension::WrapContent);
            cpy_lp.height = 28;
            cpy_lp.gravity = Gravity::CenterVertical;
            header_row->add_view(copy_btn, cpy_lp);
        }

        // Renew DHCP button (when carrier active and in DHCP mode)
        if (has_carrier && item->eth_profile.mode.find("DHCP") != std::string::npos) {
            auto on_renew = [this, item]() {
                item->last_action_message = "Renewing DHCP lease...";
                rebuild_preview_column();
                std::thread([this, item]() {
                    bool ok = EthernetBackend::configure_ipv4_auto(item->eth_profile.name);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
                    if (auto* eng = AppEngine::instance()) {
                        eng->post_delayed(0, [this, item, ok]() {
                            EthernetBackend::sync_item_state(item);
                            if (ok && item->eth_profile.ip_address != "None") {
                                item->last_action_message = "DHCP lease renewed • IPv4 " + item->eth_profile.ip_address;
                            } else {
                                item->last_action_message = "DHCP lease renewed";
                            }
                            deferred_rebuild_preview();
                            notify_state_changed();
                        });
                    }
                }).detach();
            };

            auto renew_btn = ButtonBuilder::create()
                ->text("Renew DHCP")
                ->style(ButtonStyle::Standard)
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
        }

        // Configure button (opens popup modal for IPv4 settings)
        auto config_btn = ButtonBuilder::create()
            ->text("⚙ Configure")
            ->style(ButtonStyle::Standard)
            ->textColor(config->colors.primary)
            ->padding(10, 4)
            ->cornerRadius(get_btn_radius(config))
            ->build();

        config_btn->set_on_click_listener([this, item, config, config_btn]() {
            if (m_active_popup) {
                m_active_popup->dismiss();
                m_active_popup = nullptr;
            }

            auto popup_col = LinearLayoutBuilder::create()
                ->orientation(Orientation::Vertical)
                ->padding(16, 14)
                ->spacing(12)
                ->backgroundColor(config->colors.surface)
                ->build();

            auto p_title = TextViewBuilder::create()
                ->text("Configure IPv4 Network")
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.on_surface)
                ->bold(true)
                ->build();
            popup_col->add_view(p_title);

            auto p_desc = TextViewBuilder::create()
                ->text("Choose whether to obtain your IP address automatically via DHCP or specify static parameters.")
                ->caption(true)
                ->textColor(config->colors.on_surface_variant)
                ->build();
            popup_col->add_view(p_desc);

            // Mode Selector Row
            auto mode_row = LinearLayoutBuilder::create()
                ->orientation(Orientation::Horizontal)
                ->spacing(8)
                ->build();

            bool is_manual_initial = (item->eth_profile.mode.find("Manual") != std::string::npos ||
                                      item->eth_profile.mode.find("Static") != std::string::npos);
            auto is_manual_ref = std::make_shared<bool>(is_manual_initial);

            auto btn_dhcp = ButtonBuilder::create()
                ->text("Automatic (DHCP)")
                ->textSize(config->metrics.caption_size)
                ->style(ButtonStyle::Standard)
                ->primary(!*is_manual_ref)
                ->backgroundColor(!*is_manual_ref ? config->colors.primary : config->colors.surface_variant)
                ->textColor(!*is_manual_ref ? config->colors.on_primary : config->colors.on_surface)
                ->cornerRadius(get_btn_radius(config))
                ->padding(12, 6)
                ->build();
            LayoutParams dhcp_lp;
            dhcp_lp.width = 0;
            dhcp_lp.weight = 1.0f;
            dhcp_lp.height = 32;
            mode_row->add_view(btn_dhcp, dhcp_lp);

            auto btn_static = ButtonBuilder::create()
                ->text("Manual (Static IP)")
                ->textSize(config->metrics.caption_size)
                ->style(ButtonStyle::Standard)
                ->primary(*is_manual_ref)
                ->backgroundColor(*is_manual_ref ? config->colors.primary : config->colors.surface_variant)
                ->textColor(*is_manual_ref ? config->colors.on_primary : config->colors.on_surface)
                ->cornerRadius(get_btn_radius(config))
                ->padding(12, 6)
                ->build();
            LayoutParams static_lp;
            static_lp.width = 0;
            static_lp.weight = 1.0f;
            static_lp.height = 32;
            mode_row->add_view(btn_static, static_lp);

            popup_col->add_view(mode_row);

            // Manual fields container
            auto manual_container = LinearLayoutBuilder::create()
                ->orientation(Orientation::Vertical)
                ->spacing(8)
                ->build();
            manual_container->set_visibility(*is_manual_ref ? Visibility::Visible : Visibility::Gone);

            auto ip_lbl = TextViewBuilder::create()
                ->text("IP Address & Subnet (e.g. 192.168.1.100/24)")
                ->caption(true)
                ->textColor(config->colors.on_surface_variant)
                ->build();
            manual_container->add_view(ip_lbl);

            std::string def_ip = (item->eth_profile.ip_address != "None" && !item->eth_profile.ip_address.empty())
                ? (item->eth_profile.ip_address + "/" + (item->eth_profile.ipv4_prefix.empty() ? "24" : item->eth_profile.ipv4_prefix))
                : "";
            auto ip_edit = EditTextBuilder::create()
                ->text(def_ip)
                ->hint("192.168.1.100/24")
                ->editable(true)
                ->padding(10, 6)
                ->build();
            LayoutParams ipe_lp;
            ipe_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            ipe_lp.height = 34;
            manual_container->add_view(ip_edit, ipe_lp);

            auto gw_lbl = TextViewBuilder::create()
                ->text("Default Gateway (Router IP)")
                ->caption(true)
                ->textColor(config->colors.on_surface_variant)
                ->build();
            manual_container->add_view(gw_lbl);

            std::string def_gw = (item->eth_profile.gateway != "None") ? item->eth_profile.gateway : "";
            auto gw_edit = EditTextBuilder::create()
                ->text(def_gw)
                ->hint("192.168.1.1")
                ->editable(true)
                ->padding(10, 6)
                ->build();
            LayoutParams gwe_lp;
            gwe_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            gwe_lp.height = 34;
            manual_container->add_view(gw_edit, gwe_lp);

            auto dns_lbl = TextViewBuilder::create()
                ->text("DNS Servers (comma separated)")
                ->caption(true)
                ->textColor(config->colors.on_surface_variant)
                ->build();
            manual_container->add_view(dns_lbl);

            std::string def_dns = (item->eth_profile.dns != "None") ? item->eth_profile.dns : "";
            auto dns_edit = EditTextBuilder::create()
                ->text(def_dns)
                ->hint("1.1.1.1, 8.8.8.8")
                ->editable(true)
                ->padding(10, 6)
                ->build();
            LayoutParams dnse_lp;
            dnse_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            dnse_lp.height = 34;
            manual_container->add_view(dns_edit, dnse_lp);

            popup_col->add_view(manual_container);

            btn_dhcp->set_on_click_listener([btn_dhcp, btn_static, manual_container, config, is_manual_ref]() {
                *is_manual_ref = false;
                btn_dhcp->set_primary(true);
                btn_dhcp->set_background_color(config->colors.primary);
                btn_dhcp->set_text_color(config->colors.on_primary);
                btn_static->set_primary(false);
                btn_static->set_background_color(config->colors.surface_variant);
                btn_static->set_text_color(config->colors.on_surface);
                manual_container->set_visibility(Visibility::Gone);
            });

            btn_static->set_on_click_listener([btn_dhcp, btn_static, manual_container, config, is_manual_ref]() {
                *is_manual_ref = true;
                btn_static->set_primary(true);
                btn_static->set_background_color(config->colors.primary);
                btn_static->set_text_color(config->colors.on_primary);
                btn_dhcp->set_primary(false);
                btn_dhcp->set_background_color(config->colors.surface_variant);
                btn_dhcp->set_text_color(config->colors.on_surface);
                manual_container->set_visibility(Visibility::Visible);
            });

            // Dialog buttons row: Cancel / Save
            auto btn_row = LinearLayoutBuilder::create()
                ->orientation(Orientation::Horizontal)
                ->spacing(8)
                ->gravity(Gravity::Right)
                ->build();

            auto cancel_btn = ButtonBuilder::create()
                ->text("Cancel")
                ->style(ButtonStyle::Standard)
                ->textColor(config->colors.on_surface_variant)
                ->padding(12, 6)
                ->cornerRadius(get_btn_radius(config))
                ->onClick([this]() {
                    if (m_active_popup) {
                        m_active_popup->dismiss();
                        m_active_popup = nullptr;
                    }
                })
                ->build();
            btn_row->add_view(cancel_btn);

            auto save_btn = ButtonBuilder::create()
                ->text("Save & Apply")
                ->style(ButtonStyle::Standard)
                ->primary(true)
                ->textColor(config->colors.on_primary)
                ->backgroundColor(config->colors.primary)
                ->padding(12, 6)
                ->cornerRadius(get_btn_radius(config))
                ->onClick([this, item, is_manual_ref, ip_edit, gw_edit, dns_edit]() {
                    bool manual = *is_manual_ref;
                    std::string ip = ip_edit ? ip_edit->get_text() : "";
                    std::string gw = gw_edit ? gw_edit->get_text() : "";
                    std::string dns = dns_edit ? dns_edit->get_text() : "";

                    if (m_active_popup) {
                        m_active_popup->dismiss();
                        m_active_popup = nullptr;
                    }

                    item->last_action_message = manual ? "Applying static IPv4 configuration..." : "Applying Automatic (DHCP)...";
                    rebuild_preview_column();

                    std::thread([this, item, manual, ip, gw, dns]() {
                        bool ok = false;
                        if (!manual) {
                            ok = EthernetBackend::configure_ipv4_auto(item->eth_profile.name);
                        } else {
                            ok = EthernetBackend::configure_ipv4_manual(item->eth_profile.name, ip, gw, dns);
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(1200));
                        if (auto* eng = AppEngine::instance()) {
                            eng->post_delayed(0, [this, item, ok]() {
                                EthernetBackend::sync_item_state(item);
                                item->last_action_message = ok ? "Network configuration applied" : "Failed to apply network configuration";
                                deferred_rebuild_preview();
                                notify_state_changed();
                            });
                        }
                    }).detach();
                })
                ->build();
            btn_row->add_view(save_btn);

            popup_col->add_view(btn_row);

            m_active_popup = PopupWindowBuilder::create()
                ->content(popup_col)
                ->width(360)
                ->elevation(6)
                ->cornerRadius(get_card_radius(config))
                ->dismissOnOutsideClick(true)
                ->dismissOnEscape(true)
                ->build();
            m_active_popup->show_as_dropdown(config_btn, PopupGravity::BottomEnd);
        });

        LayoutParams cfg_lp;
        cfg_lp.width = static_cast<int>(LayoutDimension::WrapContent);
        cfg_lp.height = 28;
        cfg_lp.gravity = Gravity::CenterVertical;
        header_row->add_view(config_btn, cfg_lp);

        profile_card->add_view(header_row);

        // Feedback message
        if (!item->last_action_message.empty()) {
            auto action_status = TextViewBuilder::create()
                ->text(item->last_action_message)
                ->caption(true)
                ->textColor(config->colors.primary)
                ->build();
            profile_card->add_view(action_status);
        }

        // Active Connection Highlight or Link Status
        if (is_conn) {
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

            // IP Parameters Details Table
            std::vector<std::pair<std::string, std::string>> ip_params = {
                {"IP Address", item->eth_profile.ip_address + (item->eth_profile.ipv4_prefix.empty() ? "" : " (/" + item->eth_profile.ipv4_prefix + ")")},
                {"Subnet Mask", item->eth_profile.subnet},
                {"Default Gateway", item->eth_profile.gateway},
                {"DNS Servers", item->eth_profile.dns}
            };
            if (!item->eth_profile.mac_address.empty()) {
                ip_params.push_back({"MAC Address", item->eth_profile.mac_address});
            }

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
        } else if (has_carrier) {
            // Physical carrier active, but connection profile disconnected
            auto link_box = LinearLayoutBuilder::create()
                ->orientation(Orientation::Horizontal)
                ->padding(10, 8)
                ->spacing(12)
                ->gravity(Gravity::CenterVertical)
                ->build();

            auto conn_icon = TextViewBuilder::create()
                ->text("󰈀")
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.primary)
                ->build();
            LayoutParams cicon_lp;
            cicon_lp.width = 24;
            cicon_lp.gravity = Gravity::CenterVertical;
            link_box->add_view(conn_icon, cicon_lp);

            auto text_box = LinearLayoutBuilder::create()
                ->orientation(Orientation::Vertical)
                ->spacing(2)
                ->build();

            auto profile_name = TextViewBuilder::create()
                ->text(item->eth_profile.name)
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.on_surface)
                ->build();
            text_box->add_view(profile_name);

            auto profile_sub = TextViewBuilder::create()
                ->text("Physical Link Active • Disconnected")
                ->caption(true)
                ->textColor(config->colors.on_surface_variant)
                ->build();
            text_box->add_view(profile_sub);

            LayoutParams tb_lp;
            tb_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            tb_lp.weight = 1.0f;
            tb_lp.gravity = Gravity::CenterVertical;
            link_box->add_view(text_box, tb_lp);

            auto connect_btn = ButtonBuilder::create()
                ->text("Connect")
                ->style(ButtonStyle::Standard)
                ->primary(true)
                ->textColor(config->colors.on_primary)
                ->backgroundColor(config->colors.primary)
                ->padding(12, 4)
                ->cornerRadius(get_btn_radius(config))
                ->onClick([this, item]() {
                    item->last_action_message = "Connecting " + item->eth_profile.name + "...";
                    rebuild_preview_column();
                    std::thread([this, item]() {
                        std::string cmd = "nmcli connection up \"" + item->eth_profile.name + "\" 2>/dev/null";
                        std::system(cmd.c_str());
                        std::this_thread::sleep_for(std::chrono::milliseconds(1200));
                        if (auto* eng = AppEngine::instance()) {
                            eng->post_delayed(0, [this, item]() {
                                EthernetBackend::sync_item_state(item);
                                item->last_action_message = (item->eth_profile.status == "Connected") ? "Connected" : "";
                                deferred_rebuild_preview();
                                notify_state_changed();
                            });
                        }
                    }).detach();
                })
                ->build();
            LayoutParams conn_lp;
            conn_lp.width = static_cast<int>(LayoutDimension::WrapContent);
            conn_lp.height = 28;
            link_box->add_view(connect_btn, conn_lp);

            auto link_card = CardViewBuilder::create()
                ->style(CardStyle::Filled)
                ->backgroundColor(config->colors.surface_variant.with_alpha(0.5f))
                ->cornerRadius(get_inner_radius(config))
                ->elevation(0)
                ->addView(link_box)
                ->build();
            profile_card->add_view(link_card);
        } else {
            // Cable unplugged
            auto unplugged_box = LinearLayoutBuilder::create()
                ->orientation(Orientation::Horizontal)
                ->padding(12, 10)
                ->spacing(12)
                ->gravity(Gravity::CenterVertical)
                ->build();

            auto u_icon = TextViewBuilder::create()
                ->text("󰈂")
                ->textSize(config->metrics.font_size + 2)
                ->textColor(config->colors.on_surface_variant)
                ->build();
            LayoutParams uicon_lp;
            uicon_lp.width = 24;
            uicon_lp.gravity = Gravity::CenterVertical;
            unplugged_box->add_view(u_icon, uicon_lp);

            auto u_text_box = LinearLayoutBuilder::create()
                ->orientation(Orientation::Vertical)
                ->spacing(2)
                ->build();

            auto u_title = TextViewBuilder::create()
                ->text("Cable Unplugged")
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.on_surface)
                ->build();
            u_text_box->add_view(u_title);

            auto u_desc = TextViewBuilder::create()
                ->text("No physical Ethernet link detected on interface " + item->eth_profile.iface + ". Connect an RJ-45 cable to activate.")
                ->caption(true)
                ->textColor(config->colors.on_surface_variant.with_alpha(0.8f))
                ->build();
            u_text_box->add_view(u_desc);

            LayoutParams utb_lp;
            utb_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            utb_lp.weight = 1.0f;
            utb_lp.gravity = Gravity::CenterVertical;
            unplugged_box->add_view(u_text_box, utb_lp);

            auto u_card = CardViewBuilder::create()
                ->style(CardStyle::Filled)
                ->backgroundColor(config->colors.surface_variant.with_alpha(0.35f))
                ->cornerRadius(get_inner_radius(config))
                ->elevation(0)
                ->addView(unplugged_box)
                ->build();
            profile_card->add_view(u_card);
        }

        add_preview_card(profile_card);

    // 3. Hardware & Link Telemetry (Live telemetry from backend)
    if (!item->telemetry.empty()) {
        add_preview_card(create_telemetry_card(item->telemetry, config, "Hardware & link specifications"));
    }

    return;
}


void MillerView::render_airplane_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    // 1. Live synchronization with AirplaneBackend
    AirplaneBackend::sync_item_state(item);
    auto state = AirplaneBackend::get_state();

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
        ->textColor(state.is_airplane_mode ? config->colors.primary : config->colors.on_surface_variant)
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

    std::string plane_sub = state.is_airplane_mode
        ? "Ultimate air-gapped isolation • Outer network connectivity severed"
        : "Standard operational mode • Wireless & wired communications active";
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
        ->checked(state.is_airplane_mode)
        ->onCheckedChanged([this, item](bool checked) {
            AirplaneBackend::set_airplane_mode(checked);
            AirplaneBackend::sync_item_state(item);
            rebuild_current_column();
            deferred_rebuild_preview(50);
            notify_state_changed();
        })
        ->build();

    LayoutParams sw_lp;
    sw_lp.width = 44;
    sw_lp.height = 24;
    sw_lp.gravity = Gravity::CenterVertical;
    airplane_card->add_view(primary_sw, sw_lp);
    add_preview_card(airplane_card);

    // 2. Subsystems Protection & Isolation Grid Card
    auto info_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(8)
        ->padding(16, 14)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto info_title = TextViewBuilder::create()
        ->text("Communications Subsystems & Isolation")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    info_card->add_view(info_title);

    struct SubsystemImpact {
        std::string name;
        std::string icon;
        std::string desc;
        bool is_blocked;
    };

    std::vector<SubsystemImpact> subsystems = {
        {"Wi-Fi Radio (802.11)", "󰤨", "Shuts down wireless transmitters, background probe requests, and AP associations", state.wifi_blocked || state.is_airplane_mode},
        {"Bluetooth Controller", "󰂯", "Powers off Bluetooth hardware, halts BLE advertising, peripherals, and discovery", state.bluetooth_blocked || state.is_airplane_mode},
        {"Wired Ethernet & LAN", "󰈀", "Drops physical network links, releases DHCP leases, and flushes default routes", !state.global_networking_enabled || state.is_airplane_mode},
        {"Cellular & WWAN", "󰄡", "Powers down mobile broadband modems, SIM card interfaces, and packet data", state.wwan_blocked || state.is_airplane_mode},
        {"Global Network Stack", "󰒃", "Halts NetworkManager daemon sockets, active VPN tunnels, and wireguard bridges", !state.global_networking_enabled || state.is_airplane_mode}
    };

    for (const auto& sub : subsystems) {
        auto row_content = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->padding(10, 8)
            ->spacing(12)
            ->gravity(Gravity::CenterVertical)
            ->build();

        auto r_icon = TextViewBuilder::create()
            ->text(sub.icon)
            ->textSize(config->metrics.font_size)
            ->textColor(sub.is_blocked ? config->colors.on_surface_variant.with_alpha(0.6f) : config->colors.primary)
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
            ->text(sub.name)
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        text_box->add_view(r_name);

        auto r_desc = TextViewBuilder::create()
            ->text(sub.desc)
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        text_box->add_view(r_desc);

        LayoutParams tb_lp;
        tb_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        tb_lp.weight = 1.0f;
        tb_lp.gravity = Gravity::CenterVertical;
        row_content->add_view(text_box, tb_lp);

        std::string status_str = sub.is_blocked ? "Isolated" : "Active";
        Color badge_bg = sub.is_blocked ? config->colors.surface_variant.with_alpha(0.6f) : config->colors.primary_container.with_alpha(0.45f);
        Color badge_fg = sub.is_blocked ? config->colors.on_surface_variant : config->colors.primary;

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

        Color row_bg = sub.is_blocked ? config->colors.surface_variant.with_alpha(0.18f) : Color::transparent();
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

    // 3. Hardware RFKill Devices Card (Physical & Kernel Devices from /sys/class/rfkill)
    if (!state.radios.empty()) {
        auto rf_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(6)
            ->padding(16, 14)
            ->backgroundColor(config->colors.surface)
            ->stroke(1, config->colors.outline.with_alpha(0.3f))
            ->cornerRadius(get_card_radius(config))
            ->build();

        auto rf_title = TextViewBuilder::create()
            ->text("Kernel RFKill Hardware Switches")
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        rf_card->add_view(rf_title);

        for (const auto& dev : state.radios) {
            auto dev_row = LinearLayoutBuilder::create()
                ->orientation(Orientation::Horizontal)
                ->padding(10, 6)
                ->spacing(10)
                ->gravity(Gravity::CenterVertical)
                ->build();

            std::string dev_label = dev.name + " (" + dev.type + ")";
            auto dev_name_tv = TextViewBuilder::create()
                ->text(dev_label)
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.on_surface)
                ->build();
            dev_row->add_view(dev_name_tv, LayoutParams(static_cast<int>(LayoutDimension::MatchParent), static_cast<int>(LayoutDimension::WrapContent), 1.0f));

            std::string rf_status = dev.hard_blocked ? "Hard Blocked" : (dev.soft_blocked ? "Soft Blocked" : "Unblocked");
            Color rf_fg = (dev.hard_blocked || dev.soft_blocked) ? config->colors.on_surface_variant : config->colors.primary;
            Color rf_bg = (dev.hard_blocked || dev.soft_blocked) ? config->colors.surface_variant.with_alpha(0.6f) : config->colors.primary_container.with_alpha(0.45f);

            auto dev_badge = TextViewBuilder::create()
                ->text(rf_status)
                ->caption(true)
                ->textColor(rf_fg)
                ->padding(8, 3)
                ->build();

            auto dev_pill = CardViewBuilder::create()
                ->style(CardStyle::Filled)
                ->backgroundColor(rf_bg)
                ->cornerRadius(get_chip_radius(config))
                ->elevation(0)
                ->addView(dev_badge)
                ->build();
            dev_row->add_view(dev_pill, LayoutParams(static_cast<int>(LayoutDimension::WrapContent), static_cast<int>(LayoutDimension::WrapContent), Gravity::CenterVertical));

            rf_card->add_view(dev_row);
        }
        add_preview_card(rf_card);
    }

    // 4. Telemetry Card
    if (!item->telemetry.empty()) {
        add_preview_card(create_telemetry_card(item->telemetry, config, "Hardware & subsystem specifications"));
    }
}

void MillerView::render_vpn_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    // 0. Live synchronization with backend if available and not actively transitioning
    if (VpnBackend::is_available()) {
        if (item->last_action_message.find("Connecting") == std::string::npos &&
            item->last_action_message.find("Disconnecting") == std::string::npos &&
            item->last_action_message.find("Importing") == std::string::npos) {
            VpnBackend::sync_item_state(item);
        }
    }

    // 1. Top Card: Master VPN Tunnel Switch
    auto master_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(16, 14)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto vpn_icon = TextViewBuilder::create()
        ->text("󰖂")
        ->textSize(config->metrics.font_size + 4)
        ->textColor(item->vpn_cfg.connected ? config->colors.primary : config->colors.on_surface_variant)
        ->build();
    LayoutParams vicon_lp;
    vicon_lp.width = 30;
    vicon_lp.gravity = Gravity::CenterVertical;
    master_card->add_view(vpn_icon, vicon_lp);

    auto lbl_box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(2)
        ->build();

    auto vpn_title = TextViewBuilder::create()
        ->text("Virtual Private Network (WireGuard)")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    lbl_box->add_view(vpn_title);

    std::string vpn_hint = item->vpn_cfg.connected
        ? ("Connected (" + item->vpn_cfg.active_profile + ") • Kernel WireGuard tunnel active")
        : "Disconnected • Outer traffic routing directly via local ISP";
    auto vpn_sub = TextViewBuilder::create()
        ->text(vpn_hint)
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    lbl_box->add_view(vpn_sub);

    LayoutParams vlbl_lp;
    vlbl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    vlbl_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    vlbl_lp.weight = 1.0f;
    vlbl_lp.gravity = Gravity::CenterVertical;
    master_card->add_view(lbl_box, vlbl_lp);

    auto master_sw = SwitchBuilder::create()
        ->checked(item->vpn_cfg.connected)
        ->onCheckedChanged([this, item](bool checked) {
            item->switch_val = checked;
            item->last_action_message = checked
                ? "Connecting to WireGuard tunnel..."
                : "Disconnecting VPN tunnel...";
            rebuild_preview_column();
            rebuild_current_column();
            notify_state_changed();

            std::thread([this, item, checked]() {
                bool ok = false;
                if (!checked) {
                    ok = VpnBackend::disconnect();
                } else {
                    std::string prof = item->vpn_cfg.active_profile;
                    if (prof.empty() && !item->vpn_cfg.profiles.empty()) {
                        prof = item->vpn_cfg.profiles[0].name;
                        item->vpn_cfg.active_profile = prof;
                    }
                    ok = VpnBackend::connect_profile(prof);
                }

                if (auto* eng = AppEngine::instance()) {
                    eng->post_delayed(350, [this, item, ok, checked]() {
                        VpnBackend::sync_item_state(item);
                        item->last_action_message = ok
                            ? (checked ? "WireGuard tunnel connected" : "VPN tunnel disconnected")
                            : "Failed to update VPN connection";
                        rebuild_preview_column();
                        rebuild_current_column();
                        notify_state_changed();
                    });
                }
            }).detach();
        })
        ->build();

    LayoutParams vsw_lp;
    vsw_lp.width = 44;
    vsw_lp.height = 24;
    vsw_lp.gravity = Gravity::CenterVertical;
    master_card->add_view(master_sw, vsw_lp);
    add_preview_card(master_card);

    // 2. Profile Selection & Server Card
    auto prof_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(10)
        ->padding(16, 14)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto prof_card_title = TextViewBuilder::create()
        ->text("VPN profiles & servers")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    prof_card->add_view(prof_card_title);

    std::vector<std::string> profile_names;
    int selected_prof_idx = 0;
    for (size_t i = 0; i < item->vpn_cfg.profiles.size(); ++i) {
        profile_names.push_back(item->vpn_cfg.profiles[i].name);
        if (item->vpn_cfg.profiles[i].name == item->vpn_cfg.active_profile) {
            selected_prof_idx = static_cast<int>(i);
        }
    }
    if (profile_names.empty()) {
        profile_names.push_back("(No profiles imported)");
    }

    const VpnProfile* active_prof_ptr = item->vpn_cfg.profiles.empty() ? nullptr : &item->vpn_cfg.profiles[selected_prof_idx];

    auto ep_val = TextViewBuilder::create()
        ->text(active_prof_ptr ? active_prof_ptr->endpoint : "None")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();

    auto ip_val = TextViewBuilder::create()
        ->text(active_prof_ptr ? active_prof_ptr->client_ip : "None")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();

    auto prof_spinner = SpinnerBuilder::create()
        ->items(profile_names)
        ->selectedIndex(selected_prof_idx)
        ->fontSize(config->metrics.font_size)
        ->padding(12, 6)
        ->cornerRadius(get_inner_radius(config))
        ->onItemSelected([item, ep_val, ip_val](int idx, const std::string& val) {
            if (idx >= 0 && idx < static_cast<int>(item->vpn_cfg.profiles.size())) {
                item->vpn_cfg.active_profile = val;
                const auto& p = item->vpn_cfg.profiles[idx];
                ep_val->set_text(p.endpoint);
                ip_val->set_text(p.client_ip);
                ep_val->request_redraw();
                ip_val->request_redraw();
            }
        })
        ->build();
    LayoutParams psp_lp(static_cast<int>(LayoutDimension::WrapContent), 36);
    prof_card->add_view(prof_spinner, psp_lp);

    // Endpoint details box
    auto detail_box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(3)
        ->padding(10, 8)
        ->backgroundColor(config->colors.surface_variant.with_alpha(0.2f))
        ->cornerRadius(get_inner_radius(config))
        ->build();

    auto ep_caption = TextViewBuilder::create()
        ->text("Server Endpoint & Client Address")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    detail_box->add_view(ep_caption);
    detail_box->add_view(ep_val);
    detail_box->add_view(ip_val);
    prof_card->add_view(detail_box);

    // Profile action buttons row
    auto act_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(8)
        ->gravity(Gravity::Right | Gravity::CenterVertical)
        ->build();

    auto conn_btn = ButtonBuilder::create()
        ->text(item->vpn_cfg.connected ? "Disconnect" : "Connect Profile")
        ->style(ButtonStyle::Standard)
        ->primary(!item->vpn_cfg.connected)
        ->textColor(item->vpn_cfg.connected ? config->colors.on_surface : config->colors.on_primary)
        ->backgroundColor(item->vpn_cfg.connected ? config->colors.surface_variant.with_alpha(0.6f) : config->colors.primary)
        ->padding(14, 6)
        ->cornerRadius(get_btn_radius(config))
        ->onClick([this, item]() {
            bool conn = !item->vpn_cfg.connected;
            item->last_action_message = conn ? "Connecting to profile..." : "Disconnecting...";
            rebuild_preview_column();
            std::thread([this, item, conn]() {
                bool ok = conn ? VpnBackend::connect_profile(item->vpn_cfg.active_profile) : VpnBackend::disconnect();
                if (auto* eng = AppEngine::instance()) {
                    eng->post_delayed(350, [this, item, ok, conn]() {
                        VpnBackend::sync_item_state(item);
                        item->last_action_message = ok
                            ? (conn ? "Connected successfully" : "Disconnected successfully")
                            : "Failed to update VPN connection";
                        rebuild_preview_column();
                        rebuild_current_column();
                        notify_state_changed();
                    });
                }
            }).detach();
        })
        ->build();
    LayoutParams cb_lp(static_cast<int>(LayoutDimension::WrapContent), 32);
    act_row->add_view(conn_btn, cb_lp);

    auto del_btn = ButtonBuilder::create()
        ->text("Delete")
        ->flat(true)
        ->textColor(config->colors.on_surface_variant)
        ->padding(10, 6)
        ->cornerRadius(get_btn_radius(config))
        ->onClick([this, item]() {
            std::string prof = item->vpn_cfg.active_profile;
            if (prof.empty()) return;
            VpnBackend::delete_profile(prof);
            VpnBackend::sync_item_state(item);
            item->last_action_message = "Deleted profile: " + prof;
            rebuild_preview_column();
            rebuild_current_column();
            notify_state_changed();
        })
        ->build();
    LayoutParams db_lp(static_cast<int>(LayoutDimension::WrapContent), 32);
    act_row->add_view(del_btn, db_lp);

    prof_card->add_view(act_row);
    add_preview_card(prof_card);

    // 3. Import / Account Link Card
    auto import_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(10)
        ->padding(16, 14)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto import_title = TextViewBuilder::create()
        ->text("Add WireGuard profile or link account")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    import_card->add_view(import_title);

    auto file_edit = EditTextBuilder::create()
        ->text("")
        ->hint("Path to .conf file (e.g. ~/Downloads/mullvad-se.conf)")
        ->padding(12, 8)
        ->build();
    LayoutParams fe_lp;
    fe_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    fe_lp.height = 36;
    import_card->add_view(file_edit, fe_lp);

    auto import_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->gravity(Gravity::Right | Gravity::CenterVertical)
        ->build();

    auto import_btn = ButtonBuilder::create()
        ->text("Import Configuration")
        ->style(ButtonStyle::Standard)
        ->textColor(config->colors.primary)
        ->backgroundColor(config->colors.primary.with_alpha(0.12f))
        ->padding(14, 6)
        ->cornerRadius(get_btn_radius(config))
        ->onClick([this, item, file_edit]() {
            std::string path = file_edit ? file_edit->get_text() : "";
            path = StringUtils::trim(path);
            if (path.empty()) {
                item->last_action_message = "Please enter a valid file path";
                rebuild_preview_column();
                return;
            }
            std::string out_name, out_err;
            bool ok = VpnBackend::import_profile(path, &out_name, &out_err);
            VpnBackend::sync_item_state(item);
            if (ok) {
                item->vpn_cfg.active_profile = out_name;
                item->last_action_message = "Imported WireGuard profile: " + out_name;
            } else {
                item->last_action_message = out_err.empty() ? "Import failed" : out_err;
            }
            rebuild_preview_column();
            rebuild_current_column();
            notify_state_changed();
        })
        ->build();
    LayoutParams ib_lp(static_cast<int>(LayoutDimension::WrapContent), 32);
    import_row->add_view(import_btn, ib_lp);
    import_card->add_view(import_row);

    // Mullvad Account section
    auto m_lbl = TextViewBuilder::create()
        ->text("Mullvad Account Link")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    import_card->add_view(m_lbl);

    std::string acc_status_str = item->vpn_cfg.account_status.empty() ? "Enter 16-digit account ID to link" : item->vpn_cfg.account_status;
    auto m_sub = TextViewBuilder::create()
        ->text(acc_status_str)
        ->caption(true)
        ->textColor(config->colors.primary)
        ->build();
    import_card->add_view(m_sub);

    auto acc_edit = EditTextBuilder::create()
        ->text(item->vpn_cfg.account_number)
        ->hint("Enter 16-digit Mullvad Account ID")
        ->padding(12, 8)
        ->build();
    LayoutParams ae_lp;
    ae_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    ae_lp.height = 36;
    import_card->add_view(acc_edit, ae_lp);

    auto acc_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->gravity(Gravity::Right | Gravity::CenterVertical)
        ->build();

    auto link_btn = ButtonBuilder::create()
        ->text("Verify & Link Account")
        ->style(ButtonStyle::Standard)
        ->textColor(config->colors.on_surface)
        ->backgroundColor(config->colors.surface_variant.with_alpha(0.5f))
        ->padding(14, 6)
        ->cornerRadius(get_btn_radius(config))
        ->onClick([this, item, acc_edit]() {
            std::string id = acc_edit ? acc_edit->get_text() : "";
            std::string status;
            bool ok = VpnBackend::check_mullvad_account(id, &status);
            item->vpn_cfg.account_status = status;
            item->last_action_message = (ok ? "Account verified: " : "Error: ") + status;
            VpnBackend::sync_item_state(item);
            rebuild_preview_column();
            notify_state_changed();
        })
        ->build();
    LayoutParams lb_lp(static_cast<int>(LayoutDimension::WrapContent), 32);
    acc_row->add_view(link_btn, lb_lp);
    import_card->add_view(acc_row);

    add_preview_card(import_card);

    // Feedback message
    if (!item->last_action_message.empty()) {
        bool is_err = (item->last_action_message.find("failed") != std::string::npos ||
                       item->last_action_message.find("Failed") != std::string::npos ||
                       item->last_action_message.find("Invalid") != std::string::npos);
        auto feedback = TextViewBuilder::create()
            ->text(item->last_action_message)
            ->caption(true)
            ->textColor(is_err ? Color(0.9f, 0.3f, 0.3f, 1.0f) : config->colors.primary)
            ->build();
        add_preview_card(feedback);
    }

    // 4. Telemetry Card
    if (!item->telemetry.empty()) {
        add_preview_card(create_telemetry_card(item->telemetry, config, "VPN tunnel specifications"));
    }
}

void MillerView::render_dns_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    // 0. Live synchronization with backend if available and not actively applying or testing
    if (DnsBackend::is_available()) {
        if (item->last_action_message.find("Applying") == std::string::npos &&
            item->last_action_message.find("Testing") == std::string::npos &&
            item->last_action_message.find("authenticating") == std::string::npos) {
            DnsBackend::sync_item_state(item);
        }
    }

    // 1. Top Card: Master DNS-over-TLS System Engine Switch (Polkit Privileged)
    auto engine_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(16, 14)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto engine_icon = TextViewBuilder::create()
        ->text("󰒍")
        ->textSize(config->metrics.font_size + 4)
        ->textColor(item->private_dns_cfg.dot_engine_active ? config->colors.primary : config->colors.on_surface_variant)
        ->build();
    LayoutParams eicon_lp;
    eicon_lp.width = 30;
    eicon_lp.gravity = Gravity::CenterVertical;
    engine_card->add_view(engine_icon, eicon_lp);

    auto engine_lbl_box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(2)
        ->build();

    auto engine_title = TextViewBuilder::create()
        ->text("Private DNS (DNS-over-TLS)")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    engine_lbl_box->add_view(engine_title);

    std::string engine_hint;
    if (item->private_dns_cfg.dot_engine_active) {
        engine_hint = "Strict DoT Active (" + item->private_dns_cfg.selected_option + ") • TLS Encrypted (Port 853)";
    } else if (item->private_dns_cfg.selected_option != "Off") {
        engine_hint = "Engine Setup Required (" + item->private_dns_cfg.selected_option + ") • Click switch to activate system DoT engine";
    } else {
        engine_hint = "Disabled • Standard unencrypted DNS assigned by router / local DHCP";
    }
    auto engine_sub = TextViewBuilder::create()
        ->text(engine_hint)
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    engine_lbl_box->add_view(engine_sub);

    LayoutParams elbl_lp;
    elbl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    elbl_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    elbl_lp.weight = 1.0f;
    elbl_lp.gravity = Gravity::CenterVertical;
    engine_card->add_view(engine_lbl_box, elbl_lp);

    auto engine_sw = SwitchBuilder::create()
        ->checked(item->private_dns_cfg.dot_engine_active)
        ->onCheckedChanged([this, item](bool checked) {
            item->switch_val = checked;
            item->last_action_message = checked
                ? "Enabling Private DNS (DNS-over-TLS)..."
                : "Disabling Private DNS (reverting to DHCP / standard DNS)...";
            rebuild_preview_column();
            rebuild_current_column();
            notify_state_changed();

            std::thread([this, item, checked]() {
                bool ok = false;
                if (!checked) {
                    ok = DnsBackend::apply_preset("Off", "");
                } else {
                    if (!DnsBackend::is_dot_engine_active()) {
                        if (auto* eng = AppEngine::instance()) {
                            eng->post([this, item]() {
                                item->last_action_message = "Activating DNS-over-TLS encryption engine (authenticating)...";
                                rebuild_preview_column();
                            });
                        }
                        bool engine_ok = DnsBackend::enable_dot_engine(true);
                        if (!engine_ok) {
                            if (auto* eng = AppEngine::instance()) {
                                eng->post([this, item]() {
                                    item->switch_val = false;
                                    item->private_dns_cfg.dot_engine_active = false;
                                    item->last_action_message = "Authentication failed or cancelled";
                                    rebuild_preview_column();
                                    rebuild_current_column();
                                    notify_state_changed();
                                });
                            }
                            return;
                        }
                    }

                    std::string opt = item->private_dns_cfg.selected_option;
                    if (opt.empty() || opt == "Off") {
                        std::string saved_mode, saved_host;
                        DnsBackend::load_saved_state(saved_mode, saved_host);
                        opt = (!saved_mode.empty() && saved_mode != "Off") ? saved_mode : "Cloudflare (1.1.1.1)";
                        if (!saved_host.empty()) item->private_dns_cfg.custom_hostname = saved_host;
                        item->private_dns_cfg.selected_option = opt;
                    }
                    ok = DnsBackend::apply_preset(opt, item->private_dns_cfg.custom_hostname);
                }

                if (auto* eng = AppEngine::instance()) {
                    eng->post_delayed(300, [this, item, ok, checked]() {
                        DnsBackend::sync_item_state(item);
                        item->last_action_message = ok
                            ? (checked ? "Private DNS active (DNS-over-TLS / Port 853)" : "Private DNS disabled (Standard DNS)")
                            : "Failed to apply DNS configuration";
                        rebuild_preview_column();
                        rebuild_current_column();
                        notify_state_changed();
                    });
                }
            }).detach();
        })
        ->build();

    LayoutParams esw_lp;
    esw_lp.width = 44;
    esw_lp.height = 24;
    esw_lp.gravity = Gravity::CenterVertical;
    engine_card->add_view(engine_sw, esw_lp);
    add_preview_card(engine_card);

    // 2. Configuration Card (Single Spinner on new line, Hostname EditText on new line)
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

    // Action Row: Apply button
    auto apply_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->gravity(Gravity::Right | Gravity::CenterVertical)
        ->build();

    auto apply_btn = ButtonBuilder::create()
        ->text("Apply Configuration")
        ->style(ButtonStyle::Standard)
        ->primary(true)
        ->textColor(config->colors.on_primary)
        ->backgroundColor(config->colors.primary)
        ->padding(14, 6)
        ->cornerRadius(get_btn_radius(config))
        ->onClick([this, item, custom_edit]() {
            std::string opt = item->private_dns_cfg.selected_option;
            std::string host = custom_edit ? custom_edit->get_text() : "";
            item->private_dns_cfg.custom_hostname = host;
            item->last_action_message = "Applying " + opt + "...";
            rebuild_preview_column();
            std::thread([this, item, opt, host]() {
                if (opt != "Off" && !DnsBackend::is_dot_engine_active()) {
                    if (auto* eng = AppEngine::instance()) {
                        eng->post([this, item]() {
                            item->last_action_message = "Activating DNS-over-TLS encryption engine (authenticating)...";
                            rebuild_preview_column();
                        });
                    }
                    bool engine_ok = DnsBackend::enable_dot_engine(true);
                    if (!engine_ok) {
                        if (auto* eng = AppEngine::instance()) {
                            eng->post([this, item]() {
                                item->last_action_message = "Authentication failed or cancelled";
                                rebuild_preview_column();
                                notify_state_changed();
                            });
                        }
                        return;
                    }
                }

                bool ok = DnsBackend::apply_preset(opt, host);
                if (auto* eng = AppEngine::instance()) {
                    eng->post_delayed(500, [this, item, ok, opt]() {
                        DnsBackend::sync_item_state(item);
                        item->last_action_message = ok ? (opt + " applied successfully") : "Failed to apply DNS configuration";
                        rebuild_preview_column();
                        rebuild_current_column();
                        notify_state_changed();
                    });
                }
            }).detach();
        })
        ->build();

    LayoutParams ab_lp;
    ab_lp.width = static_cast<int>(LayoutDimension::WrapContent);
    ab_lp.height = 32;
    apply_row->add_view(apply_btn, ab_lp);
    cfg_card->add_view(apply_row);

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
            item->last_action_message = "Verifying TLS handshake on port 853...";
            rebuild_preview_column();
            std::thread([this, item]() {
                float latency_ms = -1.0f;
                std::string details;
                bool ok = DnsBackend::test_resolution("cloudflare.com", &latency_ms, &details);
                if (auto* eng = AppEngine::instance()) {
                    eng->post_delayed(100, [this, item, ok, latency_ms, details]() {
                        DnsBackend::sync_item_state(item);
                        if (ok) {
                            std::ostringstream ss;
                            ss << std::fixed << std::setprecision(1) << latency_ms;
                            item->last_action_message = "DoT Verified • Latency: " + ss.str() + " ms (" + details + ")";
                        } else {
                            item->last_action_message = "Resolution failed: " + details;
                        }
                        rebuild_preview_column();
                        notify_state_changed();
                    });
                }
            }).detach();
        })
        ->build();
    LayoutParams tb_lp;
    tb_lp.width = static_cast<int>(LayoutDimension::WrapContent);
    tb_lp.height = 28;
    tb_lp.gravity = Gravity::CenterVertical;
    ep_header->add_view(test_btn, tb_lp);
    ep_card->add_view(ep_header);

    if (!item->last_action_message.empty()) {
        bool is_error = (item->last_action_message.find("failed") != std::string::npos ||
                         item->last_action_message.find("Failed") != std::string::npos);
        auto test_feedback = TextViewBuilder::create()
            ->text(item->last_action_message)
            ->caption(true)
            ->textColor(is_error ? Color(0.9f, 0.3f, 0.3f, 1.0f) : config->colors.primary)
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
    // 0. Sync with live backend if available and not actively in a background authentication command
    if (FirewallBackend::is_available()) {
        if (item->last_action_message.find("authenticating") == std::string::npos &&
            item->last_action_message.find("Enabling") == std::string::npos &&
            item->last_action_message.find("Disabling") == std::string::npos &&
            item->last_action_message.find("Applying") == std::string::npos &&
            item->last_action_message.find("Updating") == std::string::npos &&
            item->last_action_message.find("Adding") == std::string::npos &&
            item->last_action_message.find("Deleting") == std::string::npos) {
            FirewallBackend::sync_item_state(item);
        }
    }

    // 1. Top Card: Master Firewall On/Off Switch (with Polkit authentication)
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

    std::string fw_hint_str;
    if (item->firewall_cfg.enabled) {
        fw_hint_str = "Powered by UFW • Active (" + item->firewall_cfg.incoming_policy + " in, " + item->firewall_cfg.outgoing_policy + " out)";
    } else {
        fw_hint_str = "Powered by UFW • Disabled";
    }

    auto fw_hint = TextViewBuilder::create()
        ->text(fw_hint_str)
        ->caption(true)
        ->textColor(item->firewall_cfg.enabled ? config->colors.primary : config->colors.on_surface_variant.with_alpha(0.7f))
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
            item->last_action_message = checked ? "Enabling firewall (authenticating)..." : "Disabling firewall (authenticating)...";
            rebuild_preview_column();
            std::thread([this, item, checked]() {
                bool ok = FirewallBackend::set_enabled(checked);
                if (auto* eng = AppEngine::instance()) {
                    eng->post_delayed(800, [this, item, ok]() {
                        FirewallBackend::sync_item_state(item);
                        item->last_action_message = ok ? "" : "Authentication failed or cancelled";
                        rebuild_current_column();
                        deferred_rebuild_preview();
                        notify_state_changed();
                    });
                }
            }).detach();
            rebuild_current_column();
            notify_state_changed();
        })
        ->build();

    LayoutParams sw_lp;
    sw_lp.width = 44;
    sw_lp.height = 24;
    sw_lp.gravity = Gravity::CenterVertical;
    fw_card->add_view(primary_sw, sw_lp);

    add_preview_card(fw_card);

    // Feedback message banner
    if (!item->last_action_message.empty()) {
        auto msg_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->padding(12, 8)
            ->backgroundColor(config->colors.primary_container.with_alpha(0.25f))
            ->stroke(1, config->colors.primary.with_alpha(0.4f))
            ->cornerRadius(get_inner_radius(config))
            ->build();

        auto msg_tv = TextViewBuilder::create()
            ->text(item->last_action_message)
            ->caption(true)
            ->textColor(config->colors.primary)
            ->build();
        msg_card->add_view(msg_tv);
        add_preview_card(msg_card);
    }

    if (item->firewall_cfg.enabled) {
        // 2. Policy Card: Incoming Spinner, Outgoing Spinner (with Polkit authentication)
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
            ->onItemSelected([this, item, in_policies](int idx, const std::string& val) {
                (void)idx;
                if (val != item->firewall_cfg.incoming_policy) {
                    item->last_action_message = "Updating incoming policy to " + val + " (authenticating)...";
                    rebuild_preview_column();
                    std::thread([this, item, val]() {
                        bool ok = FirewallBackend::set_default_policy(true, val);
                        if (auto* eng = AppEngine::instance()) {
                            eng->post_delayed(800, [this, item, ok]() {
                                FirewallBackend::sync_item_state(item);
                                item->last_action_message = ok ? "Incoming policy updated" : "Authentication failed or cancelled";
                                deferred_rebuild_preview();
                                notify_state_changed();
                            });
                        }
                    }).detach();
                }
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
            ->onItemSelected([this, item, out_policies](int idx, const std::string& val) {
                (void)idx;
                if (val != item->firewall_cfg.outgoing_policy) {
                    item->last_action_message = "Updating outgoing policy to " + val + " (authenticating)...";
                    rebuild_preview_column();
                    std::thread([this, item, val]() {
                        bool ok = FirewallBackend::set_default_policy(false, val);
                        if (auto* eng = AppEngine::instance()) {
                            eng->post_delayed(800, [this, item, ok]() {
                                FirewallBackend::sync_item_state(item);
                                item->last_action_message = ok ? "Outgoing policy updated" : "Authentication failed or cancelled";
                                deferred_rebuild_preview();
                                notify_state_changed();
                            });
                        }
                    }).detach();
                }
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

        auto preset_hdr = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->gravity(Gravity::CenterVertical)
            ->build();

        auto preset_title = TextViewBuilder::create()
            ->text("Security Preset")
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        LayoutParams phdr_lp;
        phdr_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        phdr_lp.weight = 1.0f;
        phdr_lp.gravity = Gravity::CenterVertical;
        preset_hdr->add_view(preset_title, phdr_lp);

        auto btn_preset = ButtonBuilder::create()
            ->text("Apply Preset")
            ->style(ButtonStyle::Standard)
            ->textColor(config->colors.primary)
            ->padding(10, 4)
            ->cornerRadius(get_btn_radius(config))
            ->onClick([this, item]() {
                item->last_action_message = "Applying " + item->firewall_cfg.selected_preset + " preset (authenticating)...";
                rebuild_preview_column();
                std::thread([this, item]() {
                    bool ok = FirewallBackend::apply_preset(item->firewall_cfg.selected_preset);
                    if (auto* eng = AppEngine::instance()) {
                        eng->post_delayed(1000, [this, item, ok]() {
                            FirewallBackend::sync_item_state(item);
                            item->last_action_message = ok ? (item->firewall_cfg.selected_preset + " preset applied") : "Authentication failed or cancelled";
                            deferred_rebuild_preview();
                            notify_state_changed();
                        });
                    }
                }).detach();
            })
            ->build();
        LayoutParams bp_lp;
        bp_lp.width = static_cast<int>(LayoutDimension::WrapContent);
        bp_lp.height = 28;
        bp_lp.gravity = Gravity::CenterVertical;
        preset_hdr->add_view(btn_preset, bp_lp);
        preset_card->add_view(preset_hdr);

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
            ->style(ButtonStyle::Standard)
            ->textColor(config->colors.primary)
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
                ->bold(true)
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
                ->style(ButtonStyle::Standard)
                ->textColor(config->colors.on_surface_variant)
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
                ->text("Add & Allow")
                ->style(ButtonStyle::Standard)
                ->primary(true)
                ->textColor(config->colors.on_primary)
                ->backgroundColor(config->colors.primary)
                ->padding(14, 4)
                ->cornerRadius(get_btn_radius(config))
                ->onClick([this, item, name_edit, port_edit]() {
                    std::string r_name = name_edit ? name_edit->get_text() : "";
                    std::string r_port = port_edit ? port_edit->get_text() : "";
                    if (r_name.empty()) r_name = "Custom Port";
                    if (r_port.empty()) r_port = "8080/tcp";

                    if (m_active_popup) {
                        m_active_popup->dismiss();
                        m_active_popup = nullptr;
                    }

                    item->last_action_message = "Adding rule for " + r_port + " (authenticating)...";
                    rebuild_preview_column();

                    std::thread([this, item, r_name, r_port]() {
                        bool ok = FirewallBackend::add_custom_rule(r_name, r_port);
                        if (auto* eng = AppEngine::instance()) {
                            eng->post_delayed(1000, [this, item, ok]() {
                                FirewallBackend::sync_item_state(item);
                                item->last_action_message = ok ? "Rule applied successfully" : "Authentication failed or cancelled";
                                deferred_rebuild_preview();
                                notify_state_changed();
                            });
                        }
                    }).detach();
                })
                ->build();
            btn_box->add_view(submit_btn);

            popup_col->add_view(btn_box);

            m_active_popup = PopupWindowBuilder::create()
                ->content(popup_col)
                ->width(300)
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
                ->onCheckedChanged([on_toggle](bool c) {
                    if (on_toggle) on_toggle(c);
                })
                ->build();
            LayoutParams sw_lp;
            sw_lp.width = 44;
            sw_lp.height = 24;
            sw_lp.gravity = Gravity::CenterVertical;
            row->add_view(sw, sw_lp);

            rules_card->add_view(row);
        };

        // Predefined common rules (with Polkit authentication)
        add_rule_row("KDE Connect", "Allow local device pairing & clipboard sync (UDP/TCP 1714–1764)",
                     item->firewall_cfg.allow_kdeconnect, [this, item](bool c) {
                         item->firewall_cfg.allow_kdeconnect = c;
                         item->last_action_message = c ? "Allowing KDE Connect (authenticating)..." : "Revoking KDE Connect (authenticating)...";
                         rebuild_preview_column();
                         std::thread([this, item, c]() {
                             bool ok = FirewallBackend::toggle_service("kdeconnect", c);
                             if (auto* eng = AppEngine::instance()) {
                                 eng->post_delayed(800, [this, item, ok]() {
                                     FirewallBackend::sync_item_state(item);
                                     item->last_action_message = ok ? "" : "Authentication failed";
                                     deferred_rebuild_preview();
                                     notify_state_changed();
                                 });
                             }
                         }).detach();
                     });

        add_rule_row("SSH Server", "Allow secure shell remote access (TCP 22)",
                     item->firewall_cfg.allow_ssh, [this, item](bool c) {
                         item->firewall_cfg.allow_ssh = c;
                         item->last_action_message = c ? "Allowing SSH (authenticating)..." : "Revoking SSH (authenticating)...";
                         rebuild_preview_column();
                         std::thread([this, item, c]() {
                             bool ok = FirewallBackend::toggle_service("ssh", c);
                             if (auto* eng = AppEngine::instance()) {
                                 eng->post_delayed(800, [this, item, ok]() {
                                     FirewallBackend::sync_item_state(item);
                                     item->last_action_message = ok ? "" : "Authentication failed";
                                     deferred_rebuild_preview();
                                     notify_state_changed();
                                 });
                             }
                         }).detach();
                     });

        add_rule_row("Samba File Sharing", "Allow local network folder sharing (TCP 445 / UDP 137–138)",
                     item->firewall_cfg.allow_samba, [this, item](bool c) {
                         item->firewall_cfg.allow_samba = c;
                         item->last_action_message = c ? "Allowing Samba (authenticating)..." : "Revoking Samba (authenticating)...";
                         rebuild_preview_column();
                         std::thread([this, item, c]() {
                             bool ok = FirewallBackend::toggle_service("samba", c);
                             if (auto* eng = AppEngine::instance()) {
                                 eng->post_delayed(800, [this, item, ok]() {
                                     FirewallBackend::sync_item_state(item);
                                     item->last_action_message = ok ? "" : "Authentication failed";
                                     deferred_rebuild_preview();
                                     notify_state_changed();
                                 });
                             }
                         }).detach();
                     });

        add_rule_row("Network Printing (CUPS)", "Allow local IPP printer discovery (UDP/TCP 631)",
                     item->firewall_cfg.allow_cups, [this, item](bool c) {
                         item->firewall_cfg.allow_cups = c;
                         item->last_action_message = c ? "Allowing CUPS (authenticating)..." : "Revoking CUPS (authenticating)...";
                         rebuild_preview_column();
                         std::thread([this, item, c]() {
                             bool ok = FirewallBackend::toggle_service("cups", c);
                             if (auto* eng = AppEngine::instance()) {
                                 eng->post_delayed(800, [this, item, ok]() {
                                     FirewallBackend::sync_item_state(item);
                                     item->last_action_message = ok ? "" : "Authentication failed";
                                     deferred_rebuild_preview();
                                     notify_state_changed();
                                 });
                             }
                         }).detach();
                     });

        // User-added custom rules with toggle and delete actions
        for (size_t i = 0; i < item->firewall_cfg.custom_rules.size(); ++i) {
            auto& crule = item->firewall_cfg.custom_rules[i];
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
                ->text(crule.name)
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.on_surface)
                ->build();
            box->add_view(lbl);

            auto sub = TextViewBuilder::create()
                ->text("Port rule: " + crule.port_proto)
                ->caption(true)
                ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
                ->build();
            box->add_view(sub);

            LayoutParams box_lp;
            box_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            box_lp.weight = 1.0f;
            box_lp.gravity = Gravity::CenterVertical;
            row->add_view(box, box_lp);

            auto del_btn = ButtonBuilder::create()
                ->text("✕")
                ->style(ButtonStyle::Standard)
                ->textColor(config->colors.on_surface_variant)
                ->padding(8, 4)
                ->cornerRadius(get_btn_radius(config))
                ->onClick([this, item, crule]() {
                    item->last_action_message = "Deleting rule " + crule.port_proto + " (authenticating)...";
                    rebuild_preview_column();
                    std::thread([this, item, crule]() {
                        bool ok = FirewallBackend::delete_custom_rule(crule);
                        if (auto* eng = AppEngine::instance()) {
                            eng->post_delayed(800, [this, item, ok]() {
                                FirewallBackend::sync_item_state(item);
                                item->last_action_message = ok ? "Rule deleted" : "Authentication failed";
                                deferred_rebuild_preview();
                                notify_state_changed();
                            });
                        }
                    }).detach();
                })
                ->build();
            LayoutParams del_lp;
            del_lp.width = static_cast<int>(LayoutDimension::WrapContent);
            del_lp.height = 24;
            del_lp.gravity = Gravity::CenterVertical;
            row->add_view(del_btn, del_lp);

            rules_card->add_view(row);
        }

        add_preview_card(rules_card);

        // 5. Monitor Card: Logging toggle + Live socket & probe auditing telemetry
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
            ->text("Monitor & Packet Audit")
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

        auto refresh_telem_btn = ButtonBuilder::create()
            ->text("↻")
            ->style(ButtonStyle::Standard)
            ->textColor(config->colors.primary)
            ->padding(8, 2)
            ->cornerRadius(get_btn_radius(config))
            ->onClick([this, item]() {
                FirewallBackend::sync_item_state(item);
                rebuild_preview_column();
            })
            ->build();
        LayoutParams rbtn_lp;
        rbtn_lp.width = 28;
        rbtn_lp.height = 24;
        rbtn_lp.gravity = Gravity::CenterVertical;
        mon_hdr->add_view(refresh_telem_btn, rbtn_lp);

        auto mon_sw = SwitchBuilder::create()
            ->checked(item->firewall_cfg.monitor_enabled)
            ->onCheckedChanged([this, item](bool checked) {
                item->firewall_cfg.monitor_enabled = checked;
                item->last_action_message = checked ? "Enabling logging (authenticating)..." : "Disabling logging (authenticating)...";
                rebuild_preview_column();
                std::thread([this, item, checked]() {
                    bool ok = FirewallBackend::set_logging(checked);
                    if (auto* eng = AppEngine::instance()) {
                        eng->post_delayed(800, [this, item, ok]() {
                            FirewallBackend::sync_item_state(item);
                            item->last_action_message = ok ? "" : "Authentication failed";
                            deferred_rebuild_preview();
                            notify_state_changed();
                        });
                    }
                }).detach();
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

            if (!item->firewall_cfg.recent_blocks.empty()) {
                auto r_hdr = TextViewBuilder::create()
                    ->text("Recent Blocked Events")
                    ->caption(true)
                    ->bold(true)
                    ->textColor(config->colors.on_surface_variant.with_alpha(0.8f))
                    ->build();
                r_hdr->set_margin(0, 6, 0, 2);
                monitor_card->add_view(r_hdr);

                for (const auto& blk : item->firewall_cfg.recent_blocks) {
                    auto b_tv = TextViewBuilder::create()
                        ->text(blk)
                        ->caption(true)
                        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
                        ->ellipsize(true)
                        ->build();
                    monitor_card->add_view(b_tv);
                }
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
