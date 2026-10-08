#include "dns_view.hpp"
#include "ui_components.hpp"
#include <thread>

using namespace miqu;

namespace miqusecure {

static const Color COLOR_ACTIVE_GREEN(0.188f, 0.820f, 0.345f, 1.0f); // #30d158
static const Color COLOR_INACTIVE_GRAY(0.545f, 0.545f, 0.600f, 1.0f); // #8b8b99

DnsView::DnsView(const DnsInfo& info, std::function<void()> on_changed)
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

    setup_master_card();
    setup_resolver_card();

    // Docked 3px indeterminate progress bar
    m_progress_bar = ProgressBarBuilder::create()
        ->style(ProgressBarStyle::Linear)
        ->indeterminate(true)
        ->trackHeight(3)
        ->build();
    m_progress_bar->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        3
    ));
    m_progress_bar->set_margin(0, 0, 0, 10);
    m_progress_bar->set_visibility(Visibility::Invisible);
    m_layout->add_view(m_progress_bar);

    set_content_view(m_layout);
}

// =============================================================================
// 1. MASTER DNS PROTECTION CARD
// =============================================================================
void DnsView::setup_master_card() {
    auto card = CardViewBuilder::create()
        ->style(CardStyle::Outlined)
        ->padding(18, 14)
        ->build();
    card->set_margin(0, 4, 0, 14);

    auto master_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    master_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));

    // Crisp 8px indicator dot
    m_status_dot = std::make_shared<FrameLayout>();
    m_status_dot->set_layout_params(LayoutParams(8, 8, Gravity::CenterVertical));
    m_status_dot->set_corner_radius(4);
    m_status_dot->set_margin(0, 0, 14, 0);
    update_status_indicator(m_info.encrypted);
    master_row->add_view(m_status_dot);

    // Text column
    auto text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    text_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

    m_status_lbl = TextViewBuilder::create()
        ->text(m_info.encrypted ? "DNS Protection is Active" : "DNS Protection is Disabled")
        ->bold(true)
        ->build();
    m_status_lbl->set_margin(0, 0, 0, 3);

    std::string sub_str = "Route and encrypt lookups via ";
    if (m_info.encrypted) {
        sub_str += m_info.active_provider.empty() ? "Encrypted DNS" : m_info.active_provider;
    } else {
        sub_str = "Using unencrypted upstream nameservers (DHCP / ISP)";
    }
    if (!m_info.active_connection_name.empty()) {
        sub_str += " • " + m_info.active_connection_name;
    }

    m_subtitle_lbl = TextViewBuilder::create()
        ->text(sub_str)
        ->caption()
        ->muted()
        ->multiline(true)
        ->ellipsize(false)
        ->build();

    text_col->add_view(m_status_lbl);
    text_col->add_view(m_subtitle_lbl);
    master_row->add_view(text_col);

    m_switch_master = SwitchBuilder::create()
        ->checked(m_info.encrypted)
        ->build();
    m_switch_master->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));

    std::weak_ptr<Switch> weak_master = m_switch_master;
    m_switch_master->set_on_checked_changed_listener([this, weak_master](bool checked) {
        if (m_updating_ui) return;
        if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Visible);

        std::thread([this, checked, weak_master]() {
            bool ok = false;
            if (checked) {
                int idx = m_spinner_provider ? m_spinner_provider->get_selected_index() : 0;
                std::string p_ip = "9.9.9.9";
                std::string s_ip = "149.112.112.112";
                if (idx == 1) { p_ip = "94.140.14.14"; s_ip = "94.140.15.15"; }
                else if (idx == 2) { p_ip = "194.242.2.3"; s_ip = "194.242.2.4"; }
                else if (idx == 3) { p_ip = "1.1.1.1"; s_ip = "1.0.0.1"; }
                ok = SecurityBackend::apply_dns(p_ip, s_ip);
            } else {
                ok = SecurityBackend::restore_default_dns();
            }

            if (auto engine = AppEngine::instance()) {
                engine->post([this, checked, ok, weak_master]() {
                    if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Invisible);
                    if (ok) {
                        if (m_on_changed) m_on_changed();
                    } else {
                        m_updating_ui = true;
                        if (auto s = weak_master.lock()) {
                            s->set_checked(!checked);
                        }
                        m_updating_ui = false;
                    }
                });
            }
        }).detach();
    });
    master_row->add_view(m_switch_master);

    card->add_view(master_row);
    m_layout->add_view(card);
}

// =============================================================================
// 2. DNS RESOLVER CONFIGURATION CARD (PROVIDER DROPDOWN & CUSTOM NAMESERVERS)
// =============================================================================
void DnsView::setup_resolver_card() {
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

    // Row 1: DNS Provider Dropdown
    auto provider_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    provider_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    provider_row->set_padding(0, 10);

    auto prov_title = TextViewBuilder::create()->text("DNS Provider")->bold(true)->build();
    prov_title->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));
    provider_row->add_view(prov_title);

    std::vector<std::string> provider_items = {
        "Quad9 (Malware Block)",
        "AdGuard DNS (Ad Block)",
        "Mullvad DNS (No-Log)",
        "Cloudflare (High Speed)"
    };

    m_spinner_provider = SpinnerBuilder::create()
        ->items(provider_items)
        ->selectedIndex(provider_to_index(m_info.active_provider))
        ->padding(12, 6)
        ->cornerRadius(8)
        ->onItemSelected([this](int idx, const std::string&) {
            if (m_updating_ui) return;

            std::string p_ip = "9.9.9.9";
            std::string s_ip = "149.112.112.112";
            switch (idx) {
                case 1: p_ip = "94.140.14.14"; s_ip = "94.140.15.15"; break;
                case 2: p_ip = "194.242.2.3"; s_ip = "194.242.2.4"; break;
                case 3: p_ip = "1.1.1.1"; s_ip = "1.0.0.1"; break;
                case 0:
                default: p_ip = "9.9.9.9"; s_ip = "149.112.112.112"; break;
            }

            if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Visible);

            std::thread([this, p_ip, s_ip]() {
                bool ok = SecurityBackend::apply_dns(p_ip, s_ip);
                if (auto engine = AppEngine::instance()) {
                    engine->post([this, ok]() {
                        if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Invisible);
                        if (ok) {
                            if (m_on_changed) m_on_changed();
                        } else {
                            m_updating_ui = true;
                            if (m_spinner_provider) {
                                m_spinner_provider->set_selected_index(provider_to_index(m_info.active_provider));
                            }
                            m_updating_ui = false;
                        }
                    });
                }
            }).detach();
        })
        ->build();
    m_spinner_provider->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    provider_row->add_view(m_spinner_provider);
    box->add_view(provider_row);

    box->add_view(DividerViewBuilder::create()->build());

    // Row 2: Custom DNS Trigger (Popup Button)
    auto custom_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    custom_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    custom_row->set_padding(0, 10);

    auto custom_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    custom_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

    auto custom_title = TextViewBuilder::create()->text("Custom Nameservers")->bold(true)->build();
    m_custom_dns_lbl = TextViewBuilder::create()
        ->text("Not configured")
        ->caption()
        ->muted()
        ->build();
    m_custom_dns_lbl->set_margin(0, 2, 0, 0);

    custom_col->add_view(custom_title);
    custom_col->add_view(m_custom_dns_lbl);
    custom_row->add_view(custom_col);

    m_btn_custom_dns = ButtonBuilder::create()
        ->text("Configure...")
        ->flat(true)
        ->padding(14, 6)
        ->onClick([this]() {
            show_custom_dns_popup(m_btn_custom_dns);
        })
        ->build();
    m_btn_custom_dns->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    custom_row->add_view(m_btn_custom_dns);
    box->add_view(custom_row);

    card->add_view(box);
    m_layout->add_view(card);

    update_custom_dns_label();
}

// =============================================================================
// 3. CUSTOM DNS POPUP WINDOW
// =============================================================================
void DnsView::show_custom_dns_popup(const std::shared_ptr<View>& anchor) {
    if (!anchor) return;
    if (m_custom_popup && m_custom_popup->is_showing()) {
        m_custom_popup->dismiss();
        return;
    }

    auto popup_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    popup_col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    popup_col->set_padding(18, 16);

    auto pop_title = TextViewBuilder::create()->text("Custom DNS Nameservers")->h3()->bold(true)->build();
    pop_title->set_margin(0, 0, 0, 4);
    popup_col->add_view(pop_title);

    auto pop_desc = TextViewBuilder::create()
        ->text("Specify upstream DNS server addresses for your active connection.")
        ->caption()
        ->muted()
        ->build();
    pop_desc->set_margin(0, 0, 0, 14);
    popup_col->add_view(pop_desc);

    // Primary IP
    auto prim_lbl = TextViewBuilder::create()->text("PRIMARY IP")->caption()->bold(true)->muted()->build();
    prim_lbl->set_margin(0, 0, 0, 4);
    popup_col->add_view(prim_lbl);

    std::string existing_p = m_info.nameservers.size() > 0 ? m_info.nameservers[0] : "";
    std::string existing_s = m_info.nameservers.size() > 1 ? m_info.nameservers[1] : "";

    auto edit_prim = EditTextBuilder::create()
        ->hint("e.g. 1.1.1.1 or 8.8.8.8")
        ->padding(10, 8)
        ->build();
    edit_prim->set_text(existing_p);
    edit_prim->set_margin(0, 0, 0, 12);
    popup_col->add_view(edit_prim);

    // Secondary IP
    auto sec_lbl = TextViewBuilder::create()->text("SECONDARY IP (OPTIONAL)")->caption()->bold(true)->muted()->build();
    sec_lbl->set_margin(0, 0, 0, 4);
    popup_col->add_view(sec_lbl);

    auto edit_sec = EditTextBuilder::create()
        ->hint("e.g. 1.0.0.1 or 8.8.4.4")
        ->padding(10, 8)
        ->build();
    edit_sec->set_text(existing_s);
    edit_sec->set_margin(0, 0, 0, 16);
    popup_col->add_view(edit_sec);

    // Actions Row
    auto btn_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    btn_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::End
    ));

    auto btn_cancel = ButtonBuilder::create()
        ->text("Cancel")
        ->flat(true)
        ->padding(14, 8)
        ->onClick([this]() {
            if (m_custom_popup) m_custom_popup->dismiss();
        })
        ->build();
    btn_cancel->set_margin(0, 0, 8, 0);

    auto btn_apply = ButtonBuilder::create()
        ->text("Apply DNS")
        ->primary(true)
        ->padding(16, 8)
        ->onClick([this, edit_prim, edit_sec]() {
            std::string primary = edit_prim->get_text();
            std::string secondary = edit_sec->get_text();
            if (primary.empty()) return;

            if (m_custom_popup) m_custom_popup->dismiss();
            if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Visible);

            std::thread([this, primary, secondary]() {
                bool ok = SecurityBackend::apply_dns(primary, secondary);
                if (auto engine = AppEngine::instance()) {
                    engine->post([this, ok]() {
                        if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Invisible);
                        if (ok && m_on_changed) m_on_changed();
                    });
                }
            }).detach();
        })
        ->build();

    btn_row->add_view(btn_cancel);
    btn_row->add_view(btn_apply);
    popup_col->add_view(btn_row);

    m_custom_popup = PopupWindowBuilder::create()
        ->content(popup_col)
        ->width(340)
        ->elevation(6)
        ->cornerRadius(12)
        ->dismissOnOutsideClick(true)
        ->dismissOnEscape(true)
        ->build();

    m_custom_popup->show_as_dropdown(anchor, PopupGravity::TopCenter);
}

void DnsView::update_custom_dns_label() {
    if (!m_custom_dns_lbl) return;

    if (m_info.nameservers.empty()) {
        m_custom_dns_lbl->set_text("Not configured (using DHCP default)");
    } else {
        std::string s = "";
        for (size_t i = 0; i < m_info.nameservers.size(); ++i) {
            if (i > 0) s += ", ";
            s += m_info.nameservers[i];
        }
        m_custom_dns_lbl->set_text(s);
    }
}

void DnsView::update_status_indicator(bool encrypted) {
    if (!m_status_dot) return;
    m_status_dot->set_background_color(encrypted ? COLOR_ACTIVE_GREEN : COLOR_INACTIVE_GRAY);
    m_status_dot->request_redraw();
}

int DnsView::provider_to_index(const std::string& provider_name) {
    if (provider_name.find("AdGuard") != std::string::npos) return 1;
    if (provider_name.find("Mullvad") != std::string::npos) return 2;
    if (provider_name.find("Cloudflare") != std::string::npos) return 3;
    return 0; // Quad9 default
}

void DnsView::update_info(const DnsInfo& info) {
    m_info = info;

    update_status_indicator(m_info.encrypted);

    if (m_status_lbl) {
        m_status_lbl->set_text(m_info.encrypted ? "DNS Protection is Active" : "DNS Protection is Disabled");
    }

    if (m_subtitle_lbl) {
        std::string sub_str = "Route and encrypt lookups via ";
        if (m_info.encrypted) {
            sub_str += m_info.active_provider.empty() ? "Encrypted DNS" : m_info.active_provider;
        } else {
            sub_str = "Using unencrypted upstream nameservers (DHCP / ISP)";
        }
        if (!m_info.active_connection_name.empty()) {
            sub_str += " • " + m_info.active_connection_name;
        }
        m_subtitle_lbl->set_text(sub_str);
    }

    m_updating_ui = true;

    if (m_switch_master) m_switch_master->set_checked(m_info.encrypted);

    int idx = provider_to_index(m_info.active_provider);
    if (m_spinner_provider) {
        m_spinner_provider->set_selected_index(idx);
    }

    update_custom_dns_label();

    m_updating_ui = false;
}

} // namespace miqusecure
