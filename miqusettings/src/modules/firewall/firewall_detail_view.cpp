#include "modules/firewall/firewall_detail_view.hpp"
#include <miqutoolkit/core/config.hpp>
#include <miqutoolkit/view/divider_view.hpp>
#include <miqutoolkit/view/edit_text.hpp>
#include <algorithm>

namespace miqu {

// =============================================================================
// FirewallDetailView Implementation
// =============================================================================

FirewallDetailView::FirewallDetailView() {
    set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    setup_ui();
}

FirewallDetailView::~FirewallDetailView() {
    if (m_alive) {
        *m_alive = false;
    }
}

void FirewallDetailView::setup_ui() {
    clear_views();
    auto config = Config::get();

    m_scroll = std::make_shared<ScrollView>();
    m_scroll->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));

    m_content_layout = std::make_shared<LinearLayout>(Orientation::Vertical);
    m_content_layout->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    m_content_layout->set_divider_spacing(14);
    m_content_layout->set_padding(24);

    // Status banner if Polkit is authenticating or if an action failed
    if (!m_status_message.empty()) {
        m_content_layout->add_view(create_status_banner());
    }

    // 1. Core Settings: Firewall Switch, Incoming, Outgoing, Active Preset
    m_content_layout->add_view(create_core_settings_card());

    // 2. Quick Allow: SSH, KDE Connect, Samba, Plex
    m_content_layout->add_view(create_quick_allow_card());

    // 3. Custom Rules: List & Add Rule popup button
    m_content_layout->add_view(create_custom_rules_card());

    // 4. Logging: Enable Switch & Log Level Spinner
    m_content_layout->add_view(create_logging_card());

    m_scroll->set_content_view(m_content_layout);
    add_view(m_scroll);
}

// =============================================================================
// Status / Polkit Notification Banner
// =============================================================================
std::shared_ptr<View> FirewallDetailView::create_status_banner() {
    auto config = Config::get();
    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface_variant);
    card->set_stroke(1, m_is_busy ? config->colors.primary : config->colors.outline);
    card->set_radius(8);
    card->set_padding(12, 10);
    card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    row->set_divider_spacing(10);

    auto lbl = TextViewBuilder::create()
        ->text(m_status_message)
        ->textSize(12)
        ->bold(true)
        ->textColor(m_is_busy ? config->colors.primary : config->colors.on_surface)
        ->build();
    LayoutParams lbl_lp;
    lbl_lp.width = 0;
    lbl_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    lbl_lp.weight = 1.0f;
    lbl_lp.gravity = Gravity::CenterVertical;
    row->add_view(lbl, lbl_lp);

    if (!m_is_busy) {
        auto btn_dismiss = ButtonBuilder::create()
            ->text("Dismiss")
            ->textSize(11)
            ->textColor(config->colors.on_surface)
            ->backgroundColor(config->colors.surface)
            ->cornerRadius(4)
            ->onClick([this]() {
                m_status_message.clear();
                setup_ui();
            })
            ->build();
        LayoutParams btn_lp;
        btn_lp.width = 68;
        btn_lp.height = 26;
        btn_lp.gravity = Gravity::CenterVertical;
        row->add_view(btn_dismiss, btn_lp);
    }

    card->add_view(row);
    return card;
}

// =============================================================================
// Card 1: Core Firewall Settings (Switch, Incoming, Outgoing, Preset)
// =============================================================================
std::shared_ptr<View> FirewallDetailView::create_core_settings_card() {
    auto config = Config::get();
    auto& state = FirewallBackend::get_state();

    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_stroke(1, config->colors.outline_variant);
    card->set_radius(config->metrics.corner_radius);
    card->set_padding(16, 12);
    card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
    col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    col->set_divider_spacing(10);

    // --- Row 1: Firewall Master Switch ---
    auto row_master = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row_master->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    row_master->set_divider_spacing(10);

    auto title_fw = TextViewBuilder::create()
        ->text("Firewall (ufw)")
        ->bold(true)
        ->textSize(15)
        ->textColor(config->colors.on_surface)
        ->build();
    LayoutParams tf_lp;
    tf_lp.width = 0;
    tf_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    tf_lp.weight = 1.0f;
    tf_lp.gravity = Gravity::CenterVertical;
    row_master->add_view(title_fw, tf_lp);

    auto sw_master = SwitchBuilder::create()
        ->checked(state.active)
        ->onCheckedChanged([this](bool checked) {
            if (m_is_busy) return;
            m_is_busy = true;
            m_status_message = "Polkit: Authenticating to change UFW service state...";
            setup_ui();

            FirewallBackend::set_active(checked, [this, alive = m_alive](bool ok, const std::string& msg) {
                if (!*alive) return;
                m_is_busy = false;
                if (!ok) {
                    m_status_message = "Polkit authorization cancelled or failed.";
                } else {
                    m_status_message = "";
                }
                setup_ui();
            });
        })
        ->build();
    LayoutParams sw_lp;
    sw_lp.width = 54;
    sw_lp.height = 30;
    sw_lp.gravity = Gravity::CenterVertical;
    row_master->add_view(sw_master, sw_lp);

            col->add_view(row_master);

    // Divider
    col->add_view(std::make_shared<DividerView>());

    // --- Row 2: Incoming Traffic Spinner ---
    auto row_in = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row_in->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    row_in->set_divider_spacing(10);

    auto lbl_in = TextViewBuilder::create()
        ->text("Incoming Traffic")
        ->textSize(14)
        ->textColor(config->colors.on_surface)
        ->build();
    LayoutParams lin_lp;
    lin_lp.width = 0;
    lin_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    lin_lp.weight = 1.0f;
    lin_lp.gravity = Gravity::CenterVertical;
    row_in->add_view(lbl_in, lin_lp);

    int in_idx = 0;
    if (state.incoming_policy.find("Reject") != std::string::npos) in_idx = 1;
    else if (state.incoming_policy.find("Allow") != std::string::npos) in_idx = 2;

    auto sp_in = SpinnerBuilder::create()
        ->items({"Deny (Default)", "Reject", "Allow"})
        ->selectedIndex(in_idx)
        ->textSize(12)
        ->padding(12, 6)
        ->cornerRadius(8)
        ->onItemSelected([this, in_idx](int idx, const std::string& item) {
            if (m_is_busy || idx == in_idx) return;
            m_is_busy = true;
            m_status_message = "Polkit: Setting incoming policy to " + item + "...";
            setup_ui();

            FirewallBackend::set_incoming_policy(item, [this, alive = m_alive](bool ok, const std::string& msg) {
                if (!*alive) return;
                m_is_busy = false;
                if (!ok) m_status_message = "Polkit authorization cancelled or failed.";
                else m_status_message = "";
                setup_ui();
            });
        })
        ->build();
    sp_in->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    row_in->add_view(sp_in);
    col->add_view(row_in);

    // Divider
    col->add_view(std::make_shared<DividerView>());

    // --- Row 3: Outgoing Traffic Spinner ---
    auto row_out = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row_out->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    row_out->set_divider_spacing(10);

    auto lbl_out = TextViewBuilder::create()
        ->text("Outgoing Traffic")
        ->textSize(14)
        ->textColor(config->colors.on_surface)
        ->build();
    LayoutParams lout_lp;
    lout_lp.width = 0;
    lout_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    lout_lp.weight = 1.0f;
    lout_lp.gravity = Gravity::CenterVertical;
    row_out->add_view(lbl_out, lout_lp);

    int out_idx = 0;
    if (state.outgoing_policy.find("Deny") != std::string::npos) out_idx = 1;
    else if (state.outgoing_policy.find("Reject") != std::string::npos) out_idx = 2;

    auto sp_out = SpinnerBuilder::create()
        ->items({"Allow (Default)", "Deny", "Reject"})
        ->selectedIndex(out_idx)
        ->textSize(12)
        ->padding(12, 6)
        ->cornerRadius(8)
        ->onItemSelected([this, out_idx](int idx, const std::string& item) {
            if (m_is_busy || idx == out_idx) return;
            m_is_busy = true;
            m_status_message = "Polkit: Setting outgoing policy to " + item + "...";
            setup_ui();

            FirewallBackend::set_outgoing_policy(item, [this, alive = m_alive](bool ok, const std::string& msg) {
                if (!*alive) return;
                m_is_busy = false;
                if (!ok) m_status_message = "Polkit authorization cancelled or failed.";
                else m_status_message = "";
                setup_ui();
            });
        })
        ->build();
    sp_out->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    row_out->add_view(sp_out);
    col->add_view(row_out);

    // Divider
    col->add_view(std::make_shared<DividerView>());

    // --- Row 4: Active Preset Spinner ---
    auto row_preset = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row_preset->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    row_preset->set_divider_spacing(10);

    auto lbl_preset = TextViewBuilder::create()
        ->text("Active Preset")
        ->textSize(14)
        ->textColor(config->colors.on_surface)
        ->build();
    LayoutParams lp_lp;
    lp_lp.width = 0;
    lp_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    lp_lp.weight = 1.0f;
    lp_lp.gravity = Gravity::CenterVertical;
    row_preset->add_view(lbl_preset, lp_lp);

    int prof_idx = static_cast<int>(state.active_profile);
    if (prof_idx < 0 || prof_idx > 4) prof_idx = 0;

    auto sp_preset = SpinnerBuilder::create()
        ->items({"Home", "Work", "Cellular", "Public", "Custom"})
        ->selectedIndex(prof_idx)
        ->textSize(12)
        ->padding(12, 6)
        ->cornerRadius(8)
        ->onItemSelected([this, prof_idx](int idx, const std::string& item) {
            if (m_is_busy || idx == prof_idx) return;
            FirewallProfile target_prof = static_cast<FirewallProfile>(idx);
            m_is_busy = true;
            m_status_message = "Polkit: Applying " + item + " preset...";
            setup_ui();

            FirewallBackend::set_profile(target_prof, [this, alive = m_alive](bool ok, const std::string& msg) {
                if (!*alive) return;
                m_is_busy = false;
                if (!ok) m_status_message = "Polkit authorization cancelled or failed.";
                else m_status_message = "";
                setup_ui();
            });
        })
        ->build();
    sp_preset->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    row_preset->add_view(sp_preset);
    col->add_view(row_preset);

    card->add_view(col);
    return card;
}

// =============================================================================
// Card 2: Quick Allow (SSH, KDE Connect, Samba, Plex)
// =============================================================================
std::shared_ptr<View> FirewallDetailView::create_quick_allow_card() {
    auto config = Config::get();
    auto& state = FirewallBackend::get_state();

    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_stroke(1, config->colors.outline_variant);
    card->set_radius(config->metrics.corner_radius);
    card->set_padding(16, 12);
    card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
    col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    col->set_divider_spacing(10);

    auto header = TextViewBuilder::create()
        ->text("Quick Allow")
        ->bold(true)
        ->textSize(14)
        ->textColor(config->colors.on_surface)
        ->build();
    col->add_view(header);

    // Grid of 4 switches: 2 rows of 2 for clean readability
    auto make_switch_cell = [&](const std::string& name, bool checked) {
        auto box = std::make_shared<LinearLayout>(Orientation::Horizontal);
        box->set_layout_params(LayoutParams(
            0,
            32,
            1.0f
        ));
        box->set_divider_spacing(8);

        auto sw = SwitchBuilder::create()
            ->checked(checked)
            ->onCheckedChanged([this, name](bool val) {
                if (m_is_busy) return;
                m_is_busy = true;
                m_status_message = "Polkit: Updating UFW rule for " + name + "...";
                setup_ui();

                FirewallBackend::set_app_toggle(name, val, [this, alive = m_alive](bool ok, const std::string& msg) {
                    if (!*alive) return;
                    m_is_busy = false;
                    if (!ok) m_status_message = "Polkit authorization cancelled or failed.";
                    else m_status_message = "";
                    setup_ui();
                });
            })
            ->build();
        LayoutParams sw_lp;
        sw_lp.width = 44;
        sw_lp.height = 24;
        sw_lp.gravity = Gravity::CenterVertical;
        box->add_view(sw, sw_lp);

        auto lbl = TextViewBuilder::create()
            ->text(name)
            ->textSize(13)
            ->textColor(config->colors.on_surface)
            ->build();
        LayoutParams lbl_lp;
        lbl_lp.width = 0;
        lbl_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        lbl_lp.weight = 1.0f;
        lbl_lp.gravity = Gravity::CenterVertical;
        box->add_view(lbl, lbl_lp);

        return box;
    };

    // Row 1: SSH & KDE Connect
    auto r1 = std::make_shared<LinearLayout>(Orientation::Horizontal);
    r1->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    r1->set_divider_spacing(16);
    r1->add_view(make_switch_cell("SSH", state.ssh_enabled));
    r1->add_view(make_switch_cell("KDE Connect", state.kdeconnect_enabled));
    col->add_view(r1);

    // Row 2: Samba & Plex
    auto r2 = std::make_shared<LinearLayout>(Orientation::Horizontal);
    r2->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    r2->set_divider_spacing(16);
    r2->add_view(make_switch_cell("Samba", state.samba_enabled));
    r2->add_view(make_switch_cell("Plex", state.plex_enabled));
    col->add_view(r2);

    card->add_view(col);
    return card;
}

// =============================================================================
// Card 3: Custom Rules (Table & Add Rule Popup Button)
// =============================================================================
std::shared_ptr<View> FirewallDetailView::create_custom_rules_card() {
    auto config = Config::get();
    auto& state = FirewallBackend::get_state();

    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_stroke(1, config->colors.outline_variant);
    card->set_radius(config->metrics.corner_radius);
    card->set_padding(16, 12);
    card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
    col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    col->set_divider_spacing(10);

    // Header row with "Custom Rules" and "+ Add Rule" button
    auto head_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    head_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));

    auto header = TextViewBuilder::create()
        ->text("Custom Rules")
        ->bold(true)
        ->textSize(14)
        ->textColor(config->colors.on_surface)
        ->build();
    LayoutParams hdr_lp;
    hdr_lp.width = 0;
    hdr_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    hdr_lp.weight = 1.0f;
    hdr_lp.gravity = Gravity::CenterVertical;
    head_row->add_view(header, hdr_lp);

    auto btn_add = ButtonBuilder::create()
        ->text("+ Add Rule")
        ->textSize(11)
        ->bold(true)
        ->textColor(config->colors.on_primary)
        ->backgroundColor(config->colors.primary)
        ->cornerRadius(6)
        ->onClick([this]() {
            show_add_rule_modal();
        })
        ->build();
    LayoutParams add_lp;
    add_lp.width = 86;
    add_lp.height = 28;
    head_row->add_view(btn_add, add_lp);
    col->add_view(head_row);

    // If no rules, show a simple one-line placeholder
    if (state.rules.empty()) {
        auto empty_lbl = TextViewBuilder::create()
            ->text("No custom rules configured. Default policies apply.")
            ->textSize(12)
            ->textColor(config->colors.on_surface_variant)
            ->build();
        col->add_view(empty_lbl);
    } else {
        // Table Header
        auto th_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
        th_row->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            24
        ));
        th_row->set_divider_spacing(4);

        auto make_th = [&](const std::string& text, int width, float weight = 0.0f) {
            auto th = TextViewBuilder::create()
                ->text(text)
                ->bold(true)
                ->textSize(11)
                ->textColor(config->colors.on_surface_variant)
                ->build();
            LayoutParams lp;
            lp.width = width;
            lp.height = static_cast<int>(LayoutDimension::WrapContent);
            lp.weight = weight;
            lp.gravity = Gravity::CenterVertical;
            th_row->add_view(th, lp);
        };

        make_th("ID", 28);
        make_th("Action", 55);
        make_th("Protocol", 65);
        make_th("Port", 55);
        make_th("Source", 0, 1.0f);
        make_th("Delete", 45);
        col->add_view(th_row);

        // Data Rows
        for (const auto& r : state.rules) {
            auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
            row->set_layout_params(LayoutParams(
                static_cast<int>(LayoutDimension::MatchParent),
                28
            ));
            row->set_divider_spacing(4);

            auto make_td = [&](const std::string& text, int width, float weight = 0.0f, bool is_action = false) {
                auto td = TextViewBuilder::create()
                    ->text(text)
                    ->textSize(12)
                    ->textColor(is_action ? config->colors.primary : config->colors.on_surface)
                    ->bold(is_action)
                    ->build();
                LayoutParams lp;
                lp.width = width;
                lp.height = static_cast<int>(LayoutDimension::WrapContent);
                lp.weight = weight;
                lp.gravity = Gravity::CenterVertical;
                row->add_view(td, lp);
            };

            make_td(std::to_string(r.id), 28);
            make_td(r.action, 55, 0.0f, true);
            make_td(r.protocol, 65);
            make_td(r.port, 55);
            make_td(r.source, 0, 1.0f);

            auto btn_del = ButtonBuilder::create()
                ->text("🗑")
                ->textSize(11)
                ->textColor(config->colors.on_surface_variant)
                ->backgroundColor(config->colors.surface_variant)
                ->cornerRadius(4)
                ->onClick([this, rule_id = r.id]() {
                    if (m_is_busy) return;
                    m_is_busy = true;
                    m_status_message = "Polkit: Deleting rule #" + std::to_string(rule_id) + "...";
                    setup_ui();

                    FirewallBackend::delete_rule(rule_id, [this, alive = m_alive](bool ok, const std::string& msg) {
                        if (!*alive) return;
                        m_is_busy = false;
                        if (!ok) m_status_message = "Polkit authorization cancelled or failed.";
                        else m_status_message = "";
                        setup_ui();
                    });
                })
                ->build();
            LayoutParams del_lp;
            del_lp.width = 30;
            del_lp.height = 24;
            del_lp.gravity = Gravity::CenterVertical;
            row->add_view(btn_del, del_lp);

            col->add_view(row);
        }
    }

    card->add_view(col);
    return card;
}

// =============================================================================
// Card 4: Logging (Enable Switch & Log Level Spinner)
// =============================================================================
std::shared_ptr<View> FirewallDetailView::create_logging_card() {
    auto config = Config::get();
    auto& state = FirewallBackend::get_state();

    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_stroke(1, config->colors.outline_variant);
    card->set_radius(config->metrics.corner_radius);
    card->set_padding(16, 12);
    card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
    col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    col->set_divider_spacing(10);

    bool is_logging_enabled = (state.log_level != "off" && !state.log_level.empty());

    // Row 1: Enable Logs Switch
    auto row_enable = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row_enable->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    row_enable->set_divider_spacing(10);

    auto lbl_enable = TextViewBuilder::create()
        ->text("Enable Logs")
        ->bold(true)
        ->textSize(14)
        ->textColor(config->colors.on_surface)
        ->build();
    LayoutParams le_lp;
    le_lp.width = 0;
    le_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    le_lp.weight = 1.0f;
    le_lp.gravity = Gravity::CenterVertical;
    row_enable->add_view(lbl_enable, le_lp);

    auto sw_log = SwitchBuilder::create()
        ->checked(is_logging_enabled)
        ->onCheckedChanged([this](bool checked) {
            if (m_is_busy) return;
            m_is_busy = true;
            std::string target_level = checked ? "low" : "off";
            m_status_message = "Polkit: " + std::string(checked ? "Enabling" : "Disabling") + " UFW logging...";
            setup_ui();

            FirewallBackend::set_log_level(target_level, [this, alive = m_alive](bool ok, const std::string& msg) {
                if (!*alive) return;
                m_is_busy = false;
                if (!ok) m_status_message = "Polkit authorization cancelled or failed.";
                else m_status_message = "";
                setup_ui();
            });
        })
        ->build();
    LayoutParams sw_lp;
    sw_lp.width = 54;
    sw_lp.height = 30;
    sw_lp.gravity = Gravity::CenterVertical;
    row_enable->add_view(sw_log, sw_lp);
    col->add_view(row_enable);

    // If enabled, show Log Level Spinner
    if (is_logging_enabled) {
        col->add_view(std::make_shared<DividerView>());

        auto row_level = std::make_shared<LinearLayout>(Orientation::Horizontal);
        row_level->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));
        row_level->set_divider_spacing(10);

        auto lbl_level = TextViewBuilder::create()
            ->text("Log Level")
            ->textSize(14)
            ->textColor(config->colors.on_surface)
            ->build();
        LayoutParams ll_lp;
        ll_lp.width = 0;
        ll_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        ll_lp.weight = 1.0f;
        ll_lp.gravity = Gravity::CenterVertical;
        row_level->add_view(lbl_level, ll_lp);

        int lvl_idx = 0;
        if (state.log_level == "medium") lvl_idx = 1;
        else if (state.log_level == "high") lvl_idx = 2;
        else if (state.log_level == "full") lvl_idx = 3;

        auto sp_level = SpinnerBuilder::create()
            ->items({"Low", "Medium", "High", "Full"})
            ->selectedIndex(lvl_idx)
            ->textSize(12)
            ->padding(12, 6)
            ->cornerRadius(8)
            ->onItemSelected([this, lvl_idx](int idx, const std::string& item) {
                if (m_is_busy || idx == lvl_idx) return;
                std::string target = "low";
                if (idx == 1) target = "medium";
                else if (idx == 2) target = "high";
                else if (idx == 3) target = "full";

                m_is_busy = true;
                m_status_message = "Polkit: Setting log level to " + target + "...";
                setup_ui();

                FirewallBackend::set_log_level(target, [this, alive = m_alive](bool ok, const std::string& msg) {
                    if (!*alive) return;
                    m_is_busy = false;
                    if (!ok) m_status_message = "Polkit authorization cancelled or failed.";
                    else m_status_message = "";
                    setup_ui();
                });
            })
            ->build();
        sp_level->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::WrapContent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));
        row_level->add_view(sp_level);
        col->add_view(row_level);
    }

    card->add_view(col);
    return card;
}

// =============================================================================
// Popup Modal: Add Custom Rule
// =============================================================================
void FirewallDetailView::show_add_rule_modal() {
    auto config = Config::get();

    auto overlay = std::make_shared<FrameLayout>();
    overlay->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    overlay->set_background_color(config->colors.backdrop);

    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_stroke(1, config->colors.outline_variant);
    card->set_radius(config->metrics.corner_radius + 4);
    card->set_padding(20);
    LayoutParams card_lp(420, static_cast<int>(LayoutDimension::WrapContent));
    card_lp.gravity = Gravity::Center;
    card->set_layout_params(card_lp);

    auto content = std::make_shared<LinearLayout>(Orientation::Vertical);
    content->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    content->set_divider_spacing(12);

    // Title
    auto title = TextViewBuilder::create()
        ->text("Add Firewall Rule")
        ->bold(true)
        ->textSize(16)
        ->textColor(config->colors.on_surface)
        ->build();
    content->add_view(title);

    auto desc = TextViewBuilder::create()
        ->text("Create a port or subnet exception via Polkit authentication.")
        ->textSize(12)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    content->add_view(desc);

    // Row 1: Port Input & Protocol Spinner
    auto row_p = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row_p->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    row_p->set_divider_spacing(8);

    auto edit_port = EditTextBuilder::create()
        ->hint("Port (e.g. 8080)")
        ->padding(8, 6)
        ->build();
    LayoutParams ep_lp(0, 36, 1.0f);
    ep_lp.gravity = Gravity::CenterVertical;
    row_p->add_view(edit_port, ep_lp);

    auto selected_proto = std::make_shared<std::string>("TCP");
    auto sp_proto = SpinnerBuilder::create()
        ->items({"TCP", "UDP", "Any"})
        ->selectedIndex(0)
        ->textSize(12)
        ->padding(12, 6)
        ->cornerRadius(6)
        ->onItemSelected([selected_proto](int, const std::string& item) {
            *selected_proto = item;
        })
        ->build();
    sp_proto->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        36,
        Gravity::CenterVertical
    ));
    row_p->add_view(sp_proto);
    content->add_view(row_p);

    // Row 2: Action Spinner & Source IP Input
    auto row_a = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row_a->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    row_a->set_divider_spacing(8);

    auto selected_action = std::make_shared<std::string>("Allow");
    auto sp_action = SpinnerBuilder::create()
        ->items({"Allow", "Deny"})
        ->selectedIndex(0)
        ->textSize(12)
        ->padding(12, 6)
        ->cornerRadius(6)
        ->onItemSelected([selected_action](int, const std::string& item) {
            *selected_action = item;
        })
        ->build();
    sp_action->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        36,
        Gravity::CenterVertical
    ));
    row_a->add_view(sp_action);

    auto edit_source = EditTextBuilder::create()
        ->hint("Source IP (empty for Anywhere)")
        ->padding(8, 6)
        ->build();
    LayoutParams es_lp(0, 36, 1.0f);
    es_lp.gravity = Gravity::CenterVertical;
    row_a->add_view(edit_source, es_lp);
    content->add_view(row_a);

    // Row 3: Buttons (Cancel, Add Rule)
    auto row_btns = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row_btns->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    row_btns->set_divider_spacing(8);

    auto spacer = std::make_shared<FrameLayout>();
    LayoutParams sp_lp(0, 1, 1.0f);
    row_btns->add_view(spacer, sp_lp);

    auto btn_cancel = ButtonBuilder::create()
        ->text("Cancel")
        ->textSize(12)
        ->textColor(config->colors.on_surface)
        ->backgroundColor(config->colors.surface_variant)
        ->cornerRadius(6)
        ->onClick([this, overlay]() {
            remove_view(overlay);
        })
        ->build();
    LayoutParams bc_lp(76, 32);
    row_btns->add_view(btn_cancel, bc_lp);

    auto btn_save = ButtonBuilder::create()
        ->text("Add Rule")
        ->textSize(12)
        ->bold(true)
        ->textColor(config->colors.on_primary)
        ->backgroundColor(config->colors.primary)
        ->cornerRadius(6)
        ->onClick([this, alive = m_alive, overlay, edit_port, edit_source, selected_proto, selected_action]() {
            if (m_is_busy) return;
            std::string port = edit_port->get_text().empty() ? "8080" : edit_port->get_text();
            std::string src = edit_source->get_text();

            FirewallRuleItem rule;
            rule.action = *selected_action;
            rule.protocol = *selected_proto;
            rule.port = port;
            rule.source = src.empty() ? "Anywhere" : src;

            remove_view(overlay);

            m_is_busy = true;
            m_status_message = "Polkit: Adding rule for port " + port + "...";
            setup_ui();

            FirewallBackend::add_rule(rule, [this, alive](bool ok, const std::string& msg) {
                if (!*alive) return;
                m_is_busy = false;
                if (!ok) m_status_message = "Polkit authorization cancelled or rule syntax invalid.";
                else m_status_message = "";
                setup_ui();
            });
        })
        ->build();
    LayoutParams bs_lp(96, 32);
    row_btns->add_view(btn_save, bs_lp);

    content->add_view(row_btns);
    card->add_view(content);
    overlay->add_view(card);

    add_view(overlay);
}

} // namespace miqu
