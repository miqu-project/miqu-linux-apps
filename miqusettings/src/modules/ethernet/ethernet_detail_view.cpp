#include "modules/ethernet/ethernet_detail_view.hpp"
#include "modules/ethernet/ethernet_backend.hpp"
#include <miqutoolkit/core/config.hpp>
#include <miqutoolkit/core/app_engine.hpp>
#include <miqutoolkit/view/edit_text.hpp>
#include <cstdlib>

namespace miqu {

EthernetDetailView::EthernetDetailView() {
    set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    load_data();
    setup_ui();
    start_polling();
}

EthernetDetailView::~EthernetDetailView() {
    stop_polling();
}

void EthernetDetailView::set_is_fullscreen(bool fullscreen) {
    m_is_fullscreen = fullscreen;
    setup_ui();
}

void EthernetDetailView::refresh_state() {
    load_data();
    setup_ui();
}

void EthernetDetailView::load_data() {
    m_device = EthernetBackend::get_device();
}

void EthernetDetailView::start_polling() {
    stop_polling();
    auto engine = AppEngine::instance();
    if (!engine) return;

    m_poll_timer_id = engine->post_delayed(3000, [this]() {
        m_poll_timer_id = 0;
        if (!m_active_modal) {
            load_data();
            setup_ui();
        }
        start_polling();
    });
}

void EthernetDetailView::stop_polling() {
    if (m_poll_timer_id != 0) {
        if (auto engine = AppEngine::instance()) {
            engine->cancel_delayed(m_poll_timer_id);
        }
        m_poll_timer_id = 0;
    }
}

void EthernetDetailView::setup_ui() {
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
    m_content_layout->set_divider_spacing(16);
    m_content_layout->set_padding(m_is_fullscreen ? 28 : 14);

    // 1. Hero Card
    m_content_layout->add_view(create_hero_card());

    // 2. Action Row (Configure IP, Copy IP)
    m_content_layout->add_view(create_action_row());

    // 3. Telemetry Section
    m_content_layout->add_view(create_telemetry_section());

    m_scroll->set_content_view(m_content_layout);
    add_view(m_scroll);

    if (m_active_modal) {
        add_view(m_active_modal);
    }
}

std::shared_ptr<View> EthernetDetailView::create_hero_card() {
    auto config = Config::get();
    int btn_radius = config->metrics.corner_radius > 0 ? std::max(4, config->metrics.corner_radius - 4) : 8;

    auto hero = std::make_shared<CardView>();
    hero->set_card_background_color(config->colors.surface);
    hero->set_radius(config->metrics.corner_radius);
    hero->set_padding(16);

    auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    row->set_divider_spacing(14);
    row->set_gravity(Gravity::CenterVertical);

    // Status Icon
    std::string icon_name = m_device.is_connected ? "network-wired" : "network-wired-disconnected";
    auto icon = std::make_shared<ImageView>(icon_name);
    icon->set_target_size(m_is_fullscreen ? 38 : 32);
    LayoutParams icon_lp(m_is_fullscreen ? 38 : 32, m_is_fullscreen ? 38 : 32);
    icon_lp.gravity = Gravity::CenterVertical;
    row->add_view(icon, icon_lp);

    // Text Column
    auto text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams text_lp;
    text_lp.width = 0;
    text_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    text_lp.weight = 1.0f;
    text_col->set_layout_params(text_lp);

    auto title = TextViewBuilder::create()
        ->text("Wired (Ethernet)")
        ->bold(true)
        ->textSize(m_is_fullscreen ? config->metrics.h1_size : config->metrics.h2_size)
        ->textColor(config->colors.on_surface)
        ->build();
    text_col->add_view(title);

    std::string sub = m_device.name + " · " + m_device.state_str;
    if (m_device.is_connected && m_device.speed_mbps > 0) {
        sub += " (" + std::to_string(m_device.speed_mbps) + " Mbps " + m_device.duplex + ")";
    }
    auto subtitle = TextViewBuilder::create()
        ->text(sub)
        ->textSize(config->metrics.caption_size)
        ->textColor(m_device.is_connected ? config->colors.primary : config->colors.on_surface_variant)
        ->build();
    text_col->add_view(subtitle);
    row->add_view(text_col);

    // Power / Connection Toggle Switch
    auto sw = SwitchBuilder::create()
        ->checked(m_device.is_enabled)
        ->onCheckedChanged([this](bool checked) {
            toggle_power(checked);
        })
        ->build();
    row->add_view(sw);

    // Fullscreen toggle button if in 3-pillar mode
    if (!m_is_fullscreen) {
        auto btn_expand = ButtonBuilder::create()
            ->text("⛶")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.primary)
            ->backgroundColor(config->colors.surface_variant)
            ->cornerRadius(btn_radius)
            ->onClick([this]() {
                if (m_on_request_fullscreen) m_on_request_fullscreen();
            })
            ->build();
        LayoutParams btn_lp(32, 32);
        btn_expand->set_layout_params(btn_lp);
        row->add_view(btn_expand);
    }

    hero->add_view(row);
    return hero;
}

std::shared_ptr<View> EthernetDetailView::create_action_row() {
    auto config = Config::get();
    int btn_radius = config->metrics.corner_radius > 0 ? std::max(4, config->metrics.corner_radius - 4) : 8;

    auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    row->set_divider_spacing(10);

    // 1. Configure IP & DNS button
    auto btn_config = ButtonBuilder::create()
        ->text("⚙ Configure IPv4 & DNS")
        ->textSize(config->metrics.font_size)
        ->primary(true)
        ->cornerRadius(btn_radius)
        ->padding(14, 8)
        ->onClick([this]() {
            show_ip_config_modal();
        })
        ->build();
    LayoutParams cfg_lp(0, 38, 1.0f);
    btn_config->set_layout_params(cfg_lp);
    row->add_view(btn_config);

    // 2. Copy IPv4 button (if assigned)
    if (!m_device.ipv4_address.empty()) {
        auto btn_copy = ButtonBuilder::create()
            ->text("📋 Copy IP")
            ->textSize(config->metrics.font_size)
            ->backgroundColor(config->colors.surface_variant)
            ->textColor(config->colors.on_surface)
            ->cornerRadius(btn_radius)
            ->padding(12, 8)
            ->onClick([this]() {
                EthernetBackend::copy_to_clipboard(m_device.ipv4_address);
            })
            ->build();
        LayoutParams copy_lp(110, 38);
        btn_copy->set_layout_params(copy_lp);
        row->add_view(btn_copy);
    }

    return row;
}

std::shared_ptr<View> EthernetDetailView::create_telemetry_section() {
    auto config = Config::get();
    auto sec = std::make_shared<LinearLayout>(Orientation::Vertical);
    sec->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    sec->set_divider_spacing(6);

    auto lbl = TextViewBuilder::create()
        ->text("NETWORK CONFIGURATION & TELEMETRY")
        ->bold(true)
        ->textSize(config->metrics.caption_size)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    sec->add_view(lbl);

    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_radius(config->metrics.corner_radius);
    card->set_padding(8);

    auto list_box = std::make_shared<LinearLayout>(Orientation::Vertical);
    list_box->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    list_box->set_divider_spacing(6);

    // Row 1: IPv4 Address
    std::string ip_val = m_device.ipv4_address.empty() ? "None (Cable Unplugged)" : m_device.ipv4_address + " (/" + m_device.ipv4_prefix + ")";
    std::string ip_mode = "Mode: " + (m_device.ipv4_method == "manual" ? std::string("Manual (Static IP)") : std::string("Automatic (DHCP)"));
    list_box->add_view(create_telemetry_row("network-wired", "IPv4 Address", ip_val, ip_mode, m_device.ipv4_address));

    // Row 2: Subnet Mask
    int prefix = 24;
    try { prefix = std::stoi(m_device.ipv4_prefix); } catch (...) {}
    std::string mask = EthernetBackend::prefix_to_netmask(prefix);
    list_box->add_view(create_telemetry_row("preferences-system-network", "Subnet Mask", mask, "Prefix: /" + m_device.ipv4_prefix));

    // Row 3: Default Gateway
    std::string gw = m_device.ipv4_gateway.empty() ? "None" : m_device.ipv4_gateway;
    list_box->add_view(create_telemetry_row("network-server", "Default Gateway", gw, "Router upstream address", m_device.ipv4_gateway));

    // Row 4: DNS Servers
    std::string dns = m_device.dns_servers.empty() ? "Default / Provided by Router" : m_device.dns_servers;
    list_box->add_view(create_telemetry_row("applications-internet", "DNS Name Servers", dns, "Domain Name Resolution"));

    // Row 5: Link Speed & Duplex
    std::string speed_str = m_device.has_carrier
        ? (std::to_string(m_device.speed_mbps) + " Mbps · " + m_device.duplex)
        : "No Physical Link (Cable Unplugged)";
    list_box->add_view(create_telemetry_row("network-transmit-receive", "Link Speed & Duplex", speed_str, "Physical carrier status"));

    // Row 6: Hardware MAC Address
    list_box->add_view(create_telemetry_row("drive-harddisk", "Hardware MAC Address", m_device.mac_address, "Physical network interface card ID", m_device.mac_address));

    // Row 7: Interface Name
    std::string iface_str = m_device.name + " (MTU: " + std::to_string(m_device.mtu) + ")";
    list_box->add_view(create_telemetry_row("dialog-information", "Network Interface", iface_str, "Linux kernel network device"));

    card->add_view(list_box);
    sec->add_view(card);
    return sec;
}

std::shared_ptr<View> EthernetDetailView::create_telemetry_row(
    const std::string& icon_name,
    const std::string& title,
    const std::string& value,
    const std::string& subvalue,
    const std::string& copy_text
) {
    auto config = Config::get();
    int btn_radius = config->metrics.corner_radius > 0 ? std::max(4, config->metrics.corner_radius - 4) : 8;

    auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    row->set_padding(8, 6);
    row->set_divider_spacing(10);
    row->set_gravity(Gravity::CenterVertical);

    auto icon = std::make_shared<ImageView>(icon_name);
    icon->set_target_size(24);
    LayoutParams icon_lp(24, 24);
    icon_lp.gravity = Gravity::CenterVertical;
    row->add_view(icon, icon_lp);

    auto text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams text_lp;
    text_lp.width = 0;
    text_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    text_lp.weight = 1.0f;
    text_col->set_layout_params(text_lp);

    auto title_lbl = TextViewBuilder::create()
        ->text(title)
        ->bold(true)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    text_col->add_view(title_lbl);

    auto val_lbl = TextViewBuilder::create()
        ->text(value)
        ->textSize(config->metrics.caption_size)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    text_col->add_view(val_lbl);

    if (!subvalue.empty()) {
        auto sub_lbl = TextViewBuilder::create()
            ->text(subvalue)
            ->textSize(std::max(10, config->metrics.caption_size - 1))
            ->textColor(config->colors.outline)
            ->build();
        text_col->add_view(sub_lbl);
    }
    row->add_view(text_col);

    if (!copy_text.empty()) {
        auto btn_copy = ButtonBuilder::create()
            ->text("Copy")
            ->textSize(config->metrics.caption_size)
            ->cornerRadius(btn_radius)
            ->backgroundColor(config->colors.surface_variant)
            ->textColor(config->colors.on_surface_variant)
            ->padding(8, 4)
            ->onClick([copy_text]() {
                EthernetBackend::copy_to_clipboard(copy_text);
            })
            ->build();
        LayoutParams copy_btn_lp(52, 28);
        copy_btn_lp.gravity = Gravity::CenterVertical;
        btn_copy->set_layout_params(copy_btn_lp);
        row->add_view(btn_copy);
    }

    return row;
}

void EthernetDetailView::show_ip_config_modal() {
    auto config = Config::get();
    int btn_radius = config->metrics.corner_radius > 0 ? std::max(4, config->metrics.corner_radius - 4) : 8;

    auto overlay = std::make_shared<FrameLayout>();
    overlay->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    overlay->set_background_color(config->colors.backdrop);

    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_radius(config->metrics.corner_radius + 4);
    card->set_padding(22);
    LayoutParams card_lp(460, static_cast<int>(LayoutDimension::WrapContent));
    card_lp.gravity = Gravity::Center;
    card->set_layout_params(card_lp);

    auto content = std::make_shared<LinearLayout>(Orientation::Vertical);
    content->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    content->set_divider_spacing(14);

    // Title
    auto title = TextViewBuilder::create()
        ->text("Configure IPv4 Network")
        ->bold(true)
        ->textSize(config->metrics.h2_size)
        ->textColor(config->colors.on_surface)
        ->build();
    content->add_view(title);

    // Subtitle
    auto desc = TextViewBuilder::create()
        ->text("Choose whether to obtain your IP address automatically via DHCP or specify static parameters.")
        ->textSize(config->metrics.caption_size)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    content->add_view(desc);

    // Mode Selector Row
    auto mode_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    mode_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        36
    ));
    mode_row->set_divider_spacing(8);

    auto is_manual = std::make_shared<bool>(m_device.ipv4_method == "manual");

    auto btn_dhcp = ButtonBuilder::create()
        ->text("Automatic (DHCP)")
        ->textSize(config->metrics.caption_size)
        ->cornerRadius(btn_radius)
        ->primary(!*is_manual)
        ->backgroundColor(!*is_manual ? config->colors.primary : config->colors.surface_variant)
        ->textColor(!*is_manual ? config->colors.on_primary : config->colors.on_surface)
        ->build();
    LayoutParams dhcp_lp(0, 36, 1.0f);
    btn_dhcp->set_layout_params(dhcp_lp);
    mode_row->add_view(btn_dhcp);

    auto btn_static = ButtonBuilder::create()
        ->text("Manual (Static IP)")
        ->textSize(config->metrics.caption_size)
        ->cornerRadius(btn_radius)
        ->primary(*is_manual)
        ->backgroundColor(*is_manual ? config->colors.primary : config->colors.surface_variant)
        ->textColor(*is_manual ? config->colors.on_primary : config->colors.on_surface)
        ->build();
    LayoutParams static_lp(0, 36, 1.0f);
    btn_static->set_layout_params(static_lp);
    mode_row->add_view(btn_static);

    content->add_view(mode_row);

    // Input fields container for manual
    auto manual_container = std::make_shared<LinearLayout>(Orientation::Vertical);
    manual_container->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    manual_container->set_divider_spacing(10);
    manual_container->set_visibility(*is_manual ? Visibility::Visible : Visibility::Gone);

    // IP Address Input
    auto ip_lbl = TextViewBuilder::create()
        ->text("IP Address & Subnet (e.g. 192.168.1.100/24)")
        ->textSize(config->metrics.caption_size)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    manual_container->add_view(ip_lbl);

    auto ip_input = std::make_shared<EditText>();
    ip_input->set_hint("192.168.1.100/24");
    if (!m_device.ipv4_address.empty()) {
        ip_input->set_text(m_device.ipv4_address + "/" + m_device.ipv4_prefix);
    }
    LayoutParams ip_in_lp(static_cast<int>(LayoutDimension::MatchParent), 34);
    ip_input->set_layout_params(ip_in_lp);
    manual_container->add_view(ip_input);

    // Gateway Input
    auto gw_lbl = TextViewBuilder::create()
        ->text("Default Gateway (Router IP)")
        ->textSize(config->metrics.caption_size)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    manual_container->add_view(gw_lbl);

    auto gw_input = std::make_shared<EditText>();
    gw_input->set_hint("192.168.1.1");
    gw_input->set_text(m_device.ipv4_gateway);
    LayoutParams gw_in_lp(static_cast<int>(LayoutDimension::MatchParent), 34);
    gw_input->set_layout_params(gw_in_lp);
    manual_container->add_view(gw_input);

    // DNS Input
    auto dns_lbl = TextViewBuilder::create()
        ->text("DNS Servers (comma separated)")
        ->textSize(config->metrics.caption_size)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    manual_container->add_view(dns_lbl);

    auto dns_input = std::make_shared<EditText>();
    dns_input->set_hint("1.1.1.1, 8.8.8.8");
    dns_input->set_text(m_device.dns_servers);
    LayoutParams dns_in_lp(static_cast<int>(LayoutDimension::MatchParent), 34);
    dns_input->set_layout_params(dns_in_lp);
    manual_container->add_view(dns_input);

    content->add_view(manual_container);

    // Mode Toggle Callback
    btn_dhcp->set_on_click_listener([btn_dhcp, btn_static, manual_container, config, is_manual]() {
        *is_manual = false;
        btn_dhcp->set_primary(true);
        btn_dhcp->set_background_color(config->colors.primary);
        btn_dhcp->set_text_color(config->colors.on_primary);
        btn_static->set_primary(false);
        btn_static->set_background_color(config->colors.surface_variant);
        btn_static->set_text_color(config->colors.on_surface);
        manual_container->set_visibility(Visibility::Gone);
    });

    btn_static->set_on_click_listener([btn_dhcp, btn_static, manual_container, config, is_manual]() {
        *is_manual = true;
        btn_static->set_primary(true);
        btn_static->set_background_color(config->colors.primary);
        btn_static->set_text_color(config->colors.on_primary);
        btn_dhcp->set_primary(false);
        btn_dhcp->set_background_color(config->colors.surface_variant);
        btn_dhcp->set_text_color(config->colors.on_surface);
        manual_container->set_visibility(Visibility::Visible);
    });

    // Action Buttons Row (Cancel / Apply)
    auto btn_box = std::make_shared<LinearLayout>(Orientation::Horizontal);
    btn_box->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    btn_box->set_divider_spacing(10);
    btn_box->set_gravity(Gravity::Right);

    auto btn_cancel = ButtonBuilder::create()
        ->text("Cancel")
        ->textSize(config->metrics.font_size)
        ->cornerRadius(btn_radius)
        ->padding(14, 6)
        ->backgroundColor(config->colors.surface_variant)
        ->textColor(config->colors.on_surface)
        ->onClick([this, overlay]() {
            remove_view(overlay);
            m_active_modal.reset();
        })
        ->build();
    btn_box->add_view(btn_cancel);

    auto btn_apply = ButtonBuilder::create()
        ->text("Save & Apply")
        ->textSize(config->metrics.font_size)
        ->cornerRadius(btn_radius)
        ->primary(true)
        ->padding(14, 6)
        ->onClick([this, overlay, is_manual, ip_input, gw_input, dns_input]() {
            if (!*is_manual) {
                EthernetBackend::configure_ipv4_auto(m_device.connection_name);
            } else {
                std::string ip = ip_input->get_text();
                std::string gw = gw_input->get_text();
                std::string dns = dns_input->get_text();
                EthernetBackend::configure_ipv4_manual(m_device.connection_name, ip, gw, dns);
            }
            remove_view(overlay);
            m_active_modal.reset();
            load_data();
            setup_ui();
        })
        ->build();
    btn_box->add_view(btn_apply);

    content->add_view(btn_box);
    card->add_view(content);
    overlay->add_view(card);
    m_active_modal = overlay;
    add_view(overlay);
}

void EthernetDetailView::toggle_power(bool power) {
    m_device.is_enabled = power;
    EthernetBackend::set_device_enabled(m_device.name, power);
    load_data();
    setup_ui();
}

} // namespace miqu
