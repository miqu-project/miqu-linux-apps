#include "firewall_view.hpp"
#include "ui_components.hpp"
#include <thread>
#include <algorithm>

using namespace miqu;

namespace miqusecure {

static const Color COLOR_ACTIVE_GREEN(0.188f, 0.820f, 0.345f, 1.0f); // #30d158
static const Color COLOR_INACTIVE_GRAY(0.545f, 0.545f, 0.600f, 1.0f); // #8b8b99
static const Color COLOR_DENY_RED(0.953f, 0.545f, 0.659f, 1.0f);     // #f38ba8

FirewallView::FirewallView(const FirewallInfo& info, std::function<void()> on_rules_changed)
    : m_info(info), m_on_changed(std::move(on_rules_changed)) {
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

    setup_status_hero();
    setup_network_profile();
    setup_local_services();
    setup_rules_card();

    rebuild_rules_list();
    set_content_view(m_layout);
}

// =============================================================================
// 1. MASTER STATUS HERO INSET CARD
// =============================================================================
void FirewallView::setup_status_hero() {
    auto card = CardViewBuilder::create()
        ->style(CardStyle::Outlined)
        ->padding(18, 14)
        ->build();
    card->set_margin(0, 4, 0, 6);

    auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));

    // Crisp 8px indicator dot
    m_status_dot = std::make_shared<FrameLayout>();
    m_status_dot->set_layout_params(LayoutParams(8, 8, Gravity::CenterVertical));
    m_status_dot->set_corner_radius(4);
    m_status_dot->set_margin(0, 0, 14, 0);
    update_status_indicator(m_info.active);
    row->add_view(m_status_dot);

    // Text column
    auto text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    text_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

    m_status_lbl = TextViewBuilder::create()
        ->text(m_info.active ? "Firewall is Active" : "Firewall Protection is Disabled")
        ->h3()
        ->bold(true)
        ->build();
    m_status_lbl->set_margin(0, 0, 0, 3);

    std::string net_str = "Filtering inbound traffic on ";
    if (!m_info.active_network_name.empty()) {
        net_str += m_info.active_network_name;
    } else {
        net_str += "all interfaces";
    }
    if (m_info.metered) {
        net_str += " (Metered)";
    }
    net_str += " • ufw/nftables";

    m_network_lbl = TextViewBuilder::create()
        ->text(net_str)
        ->caption()
        ->muted()
        ->multiline(true)
        ->ellipsize(false)
        ->build();

    text_col->add_view(m_status_lbl);
    text_col->add_view(m_network_lbl);
    row->add_view(text_col);

    // Master switch
    m_switch_master = SwitchBuilder::create()
        ->checked(m_info.active)
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
            bool ok = SecurityBackend::set_firewall_enabled(checked);
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
    row->add_view(m_switch_master);
    card->add_view(row);
    m_layout->add_view(card);

    // Docked indeterminate progress bar
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
}

// =============================================================================
// 2. NETWORK PROFILE INSET CARD
// =============================================================================
void FirewallView::setup_network_profile() {
    auto sec_hdr = ui::make_section_header("NETWORK PROFILE");
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

    // Row 1: Active Environment + Dropdown Menu
    auto row_env = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row_env->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    row_env->set_padding(0, 10);

    auto env_text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    env_text_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

    auto env_title = TextViewBuilder::create()->text("Active Environment")->bold(true)->build();
    auto env_sub = TextViewBuilder::create()
        ->text("Preset configuration for network defenses and ports")
        ->caption()
        ->muted()
        ->build();
    env_sub->set_margin(0, 2, 0, 0);

    env_text_col->add_view(env_title);
    env_text_col->add_view(env_sub);
    row_env->add_view(env_text_col);

    std::vector<std::string> mode_items = {
        "Home (Trusted)",
        "Mobile Hotspot",
        "Public Wi-Fi (Stealth)",
        "Lockdown"
    };

    m_spinner_mode = SpinnerBuilder::create()
        ->items(mode_items)
        ->selectedIndex(mode_to_index(m_info.current_mode))
        ->padding(12, 6)
        ->cornerRadius(8)
        ->onItemSelected([this](int idx, const std::string&) {
            if (m_updating_ui) return;
            NetworkMode target_mode = index_to_mode(idx);

            if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Visible);

            std::thread([this, target_mode]() {
                bool ok = SecurityBackend::set_network_mode(target_mode);
                if (auto engine = AppEngine::instance()) {
                    engine->post([this, ok]() {
                        if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Invisible);
                        if (ok) {
                            if (m_on_changed) m_on_changed();
                        } else {
                            m_updating_ui = true;
                            if (m_spinner_mode) {
                                m_spinner_mode->set_selected_index(mode_to_index(m_info.current_mode));
                            }
                            m_updating_ui = false;
                        }
                    });
                }
            }).detach();
        })
        ->build();
    m_spinner_mode->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    row_env->add_view(m_spinner_mode);
    box->add_view(row_env);

    box->add_view(DividerViewBuilder::create()->build());

    // Row 2: Stealth Mode
    auto row_stealth = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row_stealth->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    row_stealth->set_padding(0, 10);

    auto stealth_text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    stealth_text_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

    auto stealth_title = TextViewBuilder::create()->text("Stealth Mode")->bold(true)->build();
    auto stealth_sub = TextViewBuilder::create()
        ->text("Do not respond to ICMP ping or network discovery requests")
        ->caption()
        ->muted()
        ->build();
    stealth_sub->set_margin(0, 2, 0, 0);

    stealth_text_col->add_view(stealth_title);
    stealth_text_col->add_view(stealth_sub);
    row_stealth->add_view(stealth_text_col);

    m_switch_stealth = SwitchBuilder::create()
        ->checked(m_info.strict_mode)
        ->build();
    m_switch_stealth->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));

    std::weak_ptr<Switch> weak_stealth = m_switch_stealth;
    m_switch_stealth->set_on_checked_changed_listener([this, weak_stealth](bool checked) {
        if (m_updating_ui) return;
        if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Visible);

        std::thread([this, checked, weak_stealth]() {
            bool ok = SecurityBackend::set_strict_mode(checked);
            if (auto engine = AppEngine::instance()) {
                engine->post([this, checked, ok, weak_stealth]() {
                    if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Invisible);
                    if (ok) {
                        if (m_on_changed) m_on_changed();
                    } else {
                        m_updating_ui = true;
                        if (auto s = weak_stealth.lock()) {
                            s->set_checked(!checked);
                        }
                        m_updating_ui = false;
                    }
                });
            }
        }).detach();
    });
    row_stealth->add_view(m_switch_stealth);
    box->add_view(row_stealth);

    box->add_view(DividerViewBuilder::create()->build());

    // Row 3: Block All Incoming
    auto row_block = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row_block->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    row_block->set_padding(0, 10);

    auto block_text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    block_text_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

    auto block_title = TextViewBuilder::create()->text("Block All Incoming")->bold(true)->build();
    auto block_sub = TextViewBuilder::create()
        ->text("Strictly isolate machine from all unsolicited inbound traffic")
        ->caption()
        ->muted()
        ->build();
    block_sub->set_margin(0, 2, 0, 0);

    block_text_col->add_view(block_title);
    block_text_col->add_view(block_sub);
    row_block->add_view(block_text_col);

    m_switch_block_all = SwitchBuilder::create()
        ->checked(m_info.current_mode == NetworkMode::Lockdown || m_info.lockdown_active)
        ->build();
    m_switch_block_all->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));

    std::weak_ptr<Switch> weak_block = m_switch_block_all;
    m_switch_block_all->set_on_checked_changed_listener([this, weak_block](bool checked) {
        if (m_updating_ui) return;
        if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Visible);

        std::thread([this, checked, weak_block]() {
            bool ok = SecurityBackend::set_network_mode(checked ? NetworkMode::Lockdown : NetworkMode::Home);
            if (auto engine = AppEngine::instance()) {
                engine->post([this, checked, ok, weak_block]() {
                    if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Invisible);
                    if (ok) {
                        if (m_on_changed) m_on_changed();
                    } else {
                        m_updating_ui = true;
                        if (auto s = weak_block.lock()) {
                            s->set_checked(!checked);
                        }
                        m_updating_ui = false;
                    }
                });
            }
        }).detach();
    });
    row_block->add_view(m_switch_block_all);
    box->add_view(row_block);

    card->add_view(box);
    m_layout->add_view(card);
}

// =============================================================================
// 3. LOCAL SERVICES & SHARING INSET CARD
// =============================================================================
void FirewallView::setup_local_services() {
    auto sec_hdr = ui::make_section_header("LOCAL SERVICES & SHARING");
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

    auto make_service_row = [this](const std::string& title, const std::string& port,
                                   bool is_allowed, std::shared_ptr<Switch>& out_switch) {
        auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
        row->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));
        row->set_padding(0, 10);

        auto text_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
        auto text_row_lp = LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f);
        text_row_lp.gravity = Gravity::CenterVertical;
        text_row->set_layout_params(text_row_lp);

        auto name_tv = TextViewBuilder::create()->text(title)->bold(true)->build();
        name_tv->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::WrapContent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));

        auto bullet_tv = TextViewBuilder::create()->text(" • ")->caption()->muted()->build();
        bullet_tv->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::WrapContent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));

        auto port_tv = TextViewBuilder::create()->text("Port " + port)->caption()->muted()->build();
        port_tv->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::WrapContent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));

        text_row->add_view(name_tv);
        text_row->add_view(bullet_tv);
        text_row->add_view(port_tv);
        row->add_view(text_row);

        auto sw = SwitchBuilder::create()
            ->checked(is_allowed)
            ->build();
        sw->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::WrapContent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));

        std::weak_ptr<Switch> weak_sw = sw;
        sw->set_on_checked_changed_listener([this, port, weak_sw](bool checked) {
            if (m_updating_ui) return;
            if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Visible);

            std::thread([this, port, checked, weak_sw]() {
                bool ok = SecurityBackend::toggle_service(port, checked);
                if (auto engine = AppEngine::instance()) {
                    engine->post([this, checked, ok, weak_sw]() {
                        if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Invisible);
                        if (ok) {
                            if (m_on_changed) m_on_changed();
                        } else {
                            m_updating_ui = true;
                            if (auto s = weak_sw.lock()) {
                                s->set_checked(!checked);
                            }
                            m_updating_ui = false;
                        }
                    });
                }
            }).detach();
        });
        out_switch = sw;
        row->add_view(out_switch);
        return row;
    };

    box->add_view(make_service_row("Remote Login (SSH)", "22", m_info.ssh_allowed, m_switch_ssh));
    box->add_view(DividerViewBuilder::create()->build());

    box->add_view(make_service_row("Local Web Development", "8080", m_info.web_dev_allowed, m_switch_web));
    box->add_view(DividerViewBuilder::create()->build());

    box->add_view(make_service_row("Syncthing Peer Sync", "22000", m_info.syncthing_allowed, m_switch_syncthing));
    box->add_view(DividerViewBuilder::create()->build());

    box->add_view(make_service_row("File Sharing (Samba)", "445", m_info.samba_allowed, m_switch_samba));

    card->add_view(box);
    m_layout->add_view(card);
}

// =============================================================================
// 4. CONFIGURED PORT RULES INSET CARD & POPUP WINDOW
// =============================================================================
void FirewallView::setup_rules_card() {
    auto sec_hdr = ui::make_section_header("CONFIGURED PORT RULES");
    m_layout->add_view(sec_hdr);

    auto card = CardViewBuilder::create()
        ->style(CardStyle::Outlined)
        ->padding(18, 12)
        ->build();
    card->set_margin(0, 0, 0, 24);

    auto box = std::make_shared<LinearLayout>(Orientation::Vertical);
    box->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    // Table Header Row
    auto table_hdr = std::make_shared<LinearLayout>(Orientation::Horizontal);
    table_hdr->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    table_hdr->set_padding(0, 2, 0, 8);

    auto h_svc = TextViewBuilder::create()->text("SERVICE / PORT")->caption()->bold(true)->muted()->build();
    h_svc->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.5f));

    auto h_proto = TextViewBuilder::create()->text("PROTOCOL")->caption()->bold(true)->muted()->build();
    h_proto->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 0.8f));

    auto h_pol = TextViewBuilder::create()->text("POLICY")->caption()->bold(true)->muted()->build();
    h_pol->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 0.8f));

    auto h_act = TextViewBuilder::create()->text("ACTIONS")->caption()->bold(true)->muted()->textAlignment(TextAlignment::Right)->build();
    h_act->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 0.5f));

    table_hdr->add_view(h_svc);
    table_hdr->add_view(h_proto);
    table_hdr->add_view(h_pol);
    table_hdr->add_view(h_act);
    box->add_view(table_hdr);

    box->add_view(DividerViewBuilder::create()->build());

    // Dynamic rules container
    m_rules_table = std::make_shared<LinearLayout>(Orientation::Vertical);
    m_rules_table->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    box->add_view(m_rules_table);

    box->add_view(DividerViewBuilder::create()->build());

    // Bottom Action Row with "+ Add Port Rule..." button
    auto bottom_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    bottom_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::Center
    ));
    bottom_row->set_padding(0, 6, 0, 0);

    m_btn_add_rule = ButtonBuilder::create()
        ->text("+ Add Port Rule...")
        ->flat(true)
        ->padding(16, 8)
        ->onClick([this]() {
            show_add_rule_popup(m_btn_add_rule);
        })
        ->build();
    m_btn_add_rule->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::Center
    ));
    bottom_row->add_view(m_btn_add_rule);
    box->add_view(bottom_row);

    card->add_view(box);
    m_layout->add_view(card);
}

void FirewallView::rebuild_rules_list() {
    if (!m_rules_table) return;
    m_rules_table->clear_views();

    if (m_info.rules.empty()) {
        auto empty_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
        empty_row->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));
        empty_row->set_padding(0, 14);

        auto empty_tv = TextViewBuilder::create()
            ->text("No custom port rules configured. Default security policy is active.")
            ->caption()
            ->muted()
            ->build();
        empty_row->add_view(empty_tv);
        m_rules_table->add_view(empty_row);
        return;
    }

    for (size_t i = 0; i < m_info.rules.size(); ++i) {
        const auto& rule = m_info.rules[i];

        auto r_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
        r_row->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));
        r_row->set_padding(0, 10);

        // Service / Port
        auto port_tv = TextViewBuilder::create()
            ->text(rule.port_or_service)
            ->bold(true)
            ->ellipsize(true)
            ->build();
        port_tv->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.5f));

        // Protocol
        std::string proto_upper = rule.protocol;
        std::transform(proto_upper.begin(), proto_upper.end(), proto_upper.begin(), ::toupper);
        auto proto_tv = TextViewBuilder::create()
            ->text(proto_upper)
            ->caption()
            ->muted()
            ->build();
        proto_tv->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 0.8f));

        // Policy
        bool is_allow = (rule.action == "ALLOW");
        auto pol_tv = TextViewBuilder::create()
            ->text(is_allow ? "Allow" : "Deny")
            ->caption()
            ->bold(true)
            ->textColor(is_allow ? COLOR_ACTIVE_GREEN : COLOR_DENY_RED)
            ->build();
        pol_tv->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 0.8f));

        // Actions
        auto act_box = std::make_shared<LinearLayout>(Orientation::Horizontal);
        auto act_box_lp = LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 0.5f);
        act_box_lp.gravity = Gravity::End;
        act_box->set_layout_params(act_box_lp);

        FirewallRule current_rule = rule;
        auto del_btn = ButtonBuilder::create()
            ->text("Delete")
            ->flat(true)
            ->padding(8, 4)
            ->onClick([this, current_rule]() {
                if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Visible);
                std::thread([this, current_rule]() {
                    bool ok = SecurityBackend::delete_firewall_rule(current_rule);
                    if (auto engine = AppEngine::instance()) {
                        engine->post([this, ok]() {
                            if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Invisible);
                            if (ok && m_on_changed) m_on_changed();
                        });
                    }
                }).detach();
            })
            ->build();
        act_box->add_view(del_btn);

        r_row->add_view(port_tv);
        r_row->add_view(proto_tv);
        r_row->add_view(pol_tv);
        r_row->add_view(act_box);

        m_rules_table->add_view(r_row);

        if (i + 1 < m_info.rules.size()) {
            m_rules_table->add_view(DividerViewBuilder::create()->build());
        }
    }
}

// =============================================================================
// 5. ADD INBOUND PORT RULE POPUP WINDOW
// =============================================================================
void FirewallView::show_add_rule_popup(const std::shared_ptr<View>& anchor) {
    if (!anchor) return;
    if (m_add_popup && m_add_popup->is_showing()) {
        m_add_popup->dismiss();
        return;
    }

    auto popup_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    popup_col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    popup_col->set_padding(18, 16);

    auto pop_title = TextViewBuilder::create()->text("Add Inbound Port Rule")->h3()->bold(true)->build();
    pop_title->set_margin(0, 0, 0, 4);
    popup_col->add_view(pop_title);

    auto pop_desc = TextViewBuilder::create()
        ->text("Configure an inbound firewall rule for ports or services.")
        ->caption()
        ->muted()
        ->build();
    pop_desc->set_margin(0, 0, 0, 14);
    popup_col->add_view(pop_desc);

    // Port input
    auto port_lbl = TextViewBuilder::create()->text("PORT NUMBER OR RANGE")->caption()->bold(true)->muted()->build();
    port_lbl->set_margin(0, 0, 0, 4);
    popup_col->add_view(port_lbl);

    auto edit_port = EditTextBuilder::create()
        ->hint("e.g. 3000, 8080, 5432")
        ->padding(10, 8)
        ->build();
    edit_port->set_margin(0, 0, 0, 12);
    popup_col->add_view(edit_port);

    // Protocol dropdown
    auto proto_lbl = TextViewBuilder::create()->text("PROTOCOL")->caption()->bold(true)->muted()->build();
    proto_lbl->set_margin(0, 0, 0, 4);
    popup_col->add_view(proto_lbl);

    std::vector<std::string> proto_items = {"TCP", "UDP", "TCP & UDP"};
    auto spinner_proto = SpinnerBuilder::create()
        ->items(proto_items)
        ->selectedIndex(0)
        ->padding(10, 6)
        ->build();
    spinner_proto->set_margin(0, 0, 0, 12);
    popup_col->add_view(spinner_proto);

    // Policy dropdown
    auto policy_lbl = TextViewBuilder::create()->text("POLICY ACTION")->caption()->bold(true)->muted()->build();
    policy_lbl->set_margin(0, 0, 0, 4);
    popup_col->add_view(policy_lbl);

    std::vector<std::string> policy_items = {"ALLOW", "DENY"};
    auto spinner_policy = SpinnerBuilder::create()
        ->items(policy_items)
        ->selectedIndex(0)
        ->padding(10, 6)
        ->build();
    spinner_policy->set_margin(0, 0, 0, 16);
    popup_col->add_view(spinner_policy);

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
            if (m_add_popup) m_add_popup->dismiss();
        })
        ->build();
    btn_cancel->set_margin(0, 0, 8, 0);

    auto btn_submit = ButtonBuilder::create()
        ->text("Add Rule")
        ->primary(true)
        ->padding(16, 8)
        ->onClick([this, edit_port, spinner_proto, spinner_policy]() {
            std::string port = edit_port->get_text();
            if (port.empty()) return;

            int p_idx = spinner_proto->get_selected_index();
            std::string proto = (p_idx == 1) ? "udp" : (p_idx == 2 ? "any" : "tcp");

            int pol_idx = spinner_policy->get_selected_index();
            std::string action = (pol_idx == 1) ? "DENY" : "ALLOW";

            if (m_add_popup) m_add_popup->dismiss();
            if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Visible);

            std::thread([this, port, proto, action]() {
                bool ok = SecurityBackend::add_firewall_rule(port, proto, action);
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
    btn_row->add_view(btn_submit);
    popup_col->add_view(btn_row);

    m_add_popup = PopupWindowBuilder::create()
        ->content(popup_col)
        ->width(340)
        ->elevation(6)
        ->cornerRadius(12)
        ->dismissOnOutsideClick(true)
        ->dismissOnEscape(true)
        ->build();

    m_add_popup->show_as_dropdown(anchor, PopupGravity::TopCenter);
}

// =============================================================================
// 6. STATE UPDATES & CONVERSIONS
// =============================================================================
void FirewallView::update_status_indicator(bool active) {
    if (!m_status_dot) return;
    m_status_dot->set_background_color(active ? COLOR_ACTIVE_GREEN : COLOR_INACTIVE_GRAY);
    m_status_dot->request_redraw();
}

void FirewallView::update_info(const FirewallInfo& info) {
    m_info = info;

    update_status_indicator(m_info.active);

    if (m_status_lbl) {
        m_status_lbl->set_text(m_info.active ? "Firewall is Active" : "Firewall Protection is Disabled");
    }

    if (m_network_lbl) {
        std::string net_str = "Filtering inbound traffic on ";
        if (!m_info.active_network_name.empty()) {
            net_str += m_info.active_network_name;
        } else {
            net_str += "all interfaces";
        }
        if (m_info.metered) {
            net_str += " (Metered)";
        }
        net_str += " • ufw/nftables";
        m_network_lbl->set_text(net_str);
    }

    m_updating_ui = true;

    if (m_switch_master) m_switch_master->set_checked(m_info.active);
    if (m_switch_stealth) m_switch_stealth->set_checked(m_info.strict_mode);
    if (m_switch_block_all) m_switch_block_all->set_checked(m_info.current_mode == NetworkMode::Lockdown || m_info.lockdown_active);

    if (m_switch_ssh) m_switch_ssh->set_checked(m_info.ssh_allowed);
    if (m_switch_web) m_switch_web->set_checked(m_info.web_dev_allowed);
    if (m_switch_syncthing) m_switch_syncthing->set_checked(m_info.syncthing_allowed);
    if (m_switch_samba) m_switch_samba->set_checked(m_info.samba_allowed);

    if (m_spinner_mode) {
        m_spinner_mode->set_selected_index(mode_to_index(m_info.current_mode));
    }

    m_updating_ui = false;

    rebuild_rules_list();
}

int FirewallView::mode_to_index(NetworkMode mode) {
    switch (mode) {
        case NetworkMode::Home: return 0;
        case NetworkMode::MobileHotspot: return 1;
        case NetworkMode::PublicWifi: return 2;
        case NetworkMode::Lockdown: return 3;
        default: return 0;
    }
}

NetworkMode FirewallView::index_to_mode(int index) {
    switch (index) {
        case 0: return NetworkMode::Home;
        case 1: return NetworkMode::MobileHotspot;
        case 2: return NetworkMode::PublicWifi;
        case 3: return NetworkMode::Lockdown;
        default: return NetworkMode::Home;
    }
}

} // namespace miqusecure
