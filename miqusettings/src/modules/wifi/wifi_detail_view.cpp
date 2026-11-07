#include "modules/wifi/wifi_detail_view.hpp"
#include "modules/wifi/wifi_backend.hpp"
#include <miqutoolkit/core/config.hpp>
#include <miqutoolkit/core/app_engine.hpp>
#include <miqutoolkit/view/edit_text.hpp>
#include <cstdlib>

namespace miqu {

WifiDetailView::WifiDetailView() {
    set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    load_data();
    setup_ui();
}

WifiDetailView::~WifiDetailView() {
    *m_alive = false;
}


void WifiDetailView::refresh_state() {
    load_data();
    setup_ui();
}

void WifiDetailView::load_data() {
    m_adapter = WifiBackend::get_adapter_info();
    if (m_adapter.radio_enabled) {
        m_networks = WifiBackend::scan_nearby_networks();
    } else {
        m_networks.clear();
    }
}

void WifiDetailView::setup_ui() {
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
    m_content_layout->set_padding(28);

    // 1. Hero Card
    m_content_layout->add_view(create_hero_card());

    if (m_adapter.radio_enabled) {
        // 2. Connected Network Card (if connected)
        if (m_adapter.is_connected && !m_adapter.connected_ssid.empty()) {
            m_content_layout->add_view(create_connected_section());
        }

        // 3. Available Networks Section
        m_content_layout->add_view(create_available_section());
    } else {
        // Disabled State
        m_content_layout->add_view(create_disabled_state());
    }

    m_scroll->set_content_view(m_content_layout);
    add_view(m_scroll);

    if (m_active_modal) {
        add_view(m_active_modal);
    }
}

std::shared_ptr<View> WifiDetailView::create_hero_card() {
    auto config = Config::get();
    int btn_radius = config->metrics.corner_radius > 0 ? std::max(4, config->metrics.corner_radius - 4) : 8;

    auto hero = std::make_shared<CardView>();
    hero->set_card_background_color(config->colors.surface);
    hero->set_radius(config->metrics.corner_radius);
    hero->set_padding(16);

    auto hero_layout = std::make_shared<LinearLayout>(Orientation::Vertical);
    hero_layout->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    hero_layout->set_divider_spacing(12);

    auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    row->set_divider_spacing(14);
    row->set_gravity(Gravity::CenterVertical);

    // Wi-Fi Icon
    std::string icon_name = m_adapter.radio_enabled
        ? WifiBackend::resolve_signal_icon(m_adapter.signal_percent)
        : "network-wireless-offline";
    auto icon = std::make_shared<ImageView>(icon_name);
    icon->set_target_size(38);
    LayoutParams icon_lp(38, 38);
    icon_lp.gravity = Gravity::CenterVertical;
    row->add_view(icon, icon_lp);

    // Text column
    auto text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams text_lp;
    text_lp.width = 0;
    text_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    text_lp.weight = 1.0f;
    text_col->set_layout_params(text_lp);

    auto title = TextViewBuilder::create()
        ->text("Wi-Fi")
        ->bold(true)
        ->textSize(config->metrics.h1_size)
        ->textColor(config->colors.on_surface)
        ->build();
    text_col->add_view(title);

    std::string sub;
    if (!m_adapter.available) {
        sub = "No Wi-Fi adapter detected";
    } else if (!m_adapter.radio_enabled) {
        sub = "Disabled · Radios powered off";
    } else if (m_adapter.is_connected && !m_adapter.connected_ssid.empty()) {
        sub = "Connected to \"" + m_adapter.connected_ssid + "\" · " + std::to_string(m_adapter.signal_percent) + "% (" + m_adapter.frequency + ")";
    } else {
        sub = "Disconnected · Searching for nearby networks";
    }

    auto subtitle = TextViewBuilder::create()
        ->text(sub)
        ->textSize(config->metrics.caption_size)
        ->textColor(m_adapter.is_connected ? config->colors.primary : config->colors.on_surface_variant)
        ->build();
    text_col->add_view(subtitle);
    row->add_view(text_col);

    // Radio Toggle Switch
    auto sw = SwitchBuilder::create()
        ->checked(m_adapter.radio_enabled)
        ->onCheckedChanged([this](bool checked) {
            toggle_radio(checked);
        })
        ->build();
    row->add_view(sw);

        
    hero_layout->add_view(row);

    // Android-style prominent 'Scan for Networks' action pill
    if (m_adapter.radio_enabled) {
        auto btn_scan = ButtonBuilder::create()
            ->text(m_scanning ? "󰑐 Scanning for Nearby Networks... (Tap to Refresh)" : "󰑐 Scan for Networks")
            ->textSize(config->metrics.font_size)
            ->primary(!m_scanning)
            ->backgroundColor(m_scanning ? config->colors.surface_variant : config->colors.primary)
            ->textColor(m_scanning ? config->colors.primary : config->colors.on_primary)
            ->cornerRadius(btn_radius + 2)
            ->padding(14, 8)
            ->onClick([this]() { trigger_scan(); })
            ->build();
        btn_scan->set_content_gravity(Gravity::Center);
        LayoutParams scan_lp(static_cast<int>(LayoutDimension::MatchParent), 38);
        btn_scan->set_layout_params(scan_lp);
        hero_layout->add_view(btn_scan);
    }

    hero->add_view(hero_layout);
    return hero;
}

std::shared_ptr<View> WifiDetailView::create_connected_section() {
    auto config = Config::get();
    int btn_radius = config->metrics.corner_radius > 0 ? std::max(4, config->metrics.corner_radius - 4) : 8;

    auto sec = std::make_shared<LinearLayout>(Orientation::Vertical);
    sec->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    sec->set_divider_spacing(6);

    auto lbl = TextViewBuilder::create()
        ->text("CONNECTED NETWORK")
        ->bold(true)
        ->textSize(config->metrics.caption_size)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    sec->add_view(lbl);

    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_radius(config->metrics.corner_radius);
    card->set_padding(12);

    auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    row->set_divider_spacing(12);
    row->set_gravity(Gravity::CenterVertical);

    auto icon = std::make_shared<ImageView>(WifiBackend::resolve_signal_icon(m_adapter.signal_percent));
    icon->set_target_size(28);
    LayoutParams icon_lp(28, 28);
    icon_lp.gravity = Gravity::CenterVertical;
    row->add_view(icon, icon_lp);

    auto text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams text_lp;
    text_lp.width = 0;
    text_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    text_lp.weight = 1.0f;
    text_col->set_layout_params(text_lp);

    auto title = TextViewBuilder::create()
        ->text(m_adapter.connected_ssid)
        ->bold(true)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    text_col->add_view(title);

    std::string sub1 = m_adapter.frequency + " · " + m_adapter.security;
    if (!m_adapter.bitrate.empty()) sub1 += " · " + m_adapter.bitrate;
    auto sub_lbl1 = TextViewBuilder::create()
        ->text(sub1)
        ->textSize(config->metrics.caption_size)
        ->textColor(config->colors.primary)
        ->build();
    text_col->add_view(sub_lbl1);

    if (!m_adapter.ipv4_address.empty()) {
        std::string sub2 = "IP: " + m_adapter.ipv4_address + "  Gateway: " + m_adapter.ipv4_gateway;
        auto sub_lbl2 = TextViewBuilder::create()
            ->text(sub2)
            ->textSize(std::max(10, config->metrics.caption_size - 1))
            ->textColor(config->colors.on_surface_variant)
            ->build();
        text_col->add_view(sub_lbl2);
    }
    row->add_view(text_col);

    // Disconnect button
    auto btn_disc = ButtonBuilder::create()
        ->text("Disconnect")
        ->textSize(config->metrics.caption_size)
        ->cornerRadius(btn_radius)
        ->backgroundColor(config->colors.surface_variant)
        ->textColor(config->colors.on_surface)
        ->padding(10, 4)
        ->onClick([this]() { on_disconnect_clicked(); })
        ->build();
    LayoutParams disc_lp(90, 30);
    disc_lp.gravity = Gravity::CenterVertical;
    btn_disc->set_layout_params(disc_lp);
    row->add_view(btn_disc);

    // Forget button
    auto btn_forget = ButtonBuilder::create()
        ->text("✕")
        ->textSize(config->metrics.caption_size)
        ->cornerRadius(btn_radius)
        ->backgroundColor(config->colors.surface_variant)
        ->textColor(config->colors.on_surface_variant)
        ->onClick([this, ssid = m_adapter.connected_ssid]() {
            on_forget_clicked(ssid);
        })
        ->build();
    LayoutParams forget_lp(30, 30);
    forget_lp.gravity = Gravity::CenterVertical;
    btn_forget->set_layout_params(forget_lp);
    row->add_view(btn_forget);

    card->add_view(row);
    sec->add_view(card);
    return sec;
}

std::shared_ptr<View> WifiDetailView::create_available_section() {
    auto config = Config::get();
    int btn_radius = config->metrics.corner_radius > 0 ? std::max(4, config->metrics.corner_radius - 4) : 8;

    auto sec = std::make_shared<LinearLayout>(Orientation::Vertical);
    sec->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    sec->set_divider_spacing(6);

    auto header_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    header_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    header_row->set_gravity(Gravity::CenterVertical);

    auto lbl = TextViewBuilder::create()
        ->text("AVAILABLE NETWORKS")
        ->bold(true)
        ->textSize(config->metrics.caption_size)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    LayoutParams lbl_lp;
    lbl_lp.width = 0;
    lbl_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    lbl_lp.weight = 1.0f;
    lbl->set_layout_params(lbl_lp);
    header_row->add_view(lbl);

    auto btn_rescan = ButtonBuilder::create()
        ->text("Rescan")
        ->textSize(config->metrics.caption_size)
        ->cornerRadius(btn_radius)
        ->backgroundColor(config->colors.surface_variant)
        ->textColor(config->colors.on_surface)
        ->padding(10, 2)
        ->onClick([this]() { trigger_scan(); })
        ->build();
    LayoutParams rescan_lp(64, 26);
    btn_rescan->set_layout_params(rescan_lp);
    header_row->add_view(btn_rescan);

    sec->add_view(header_row);

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

    if (m_networks.empty()) {
        auto empty_text = TextViewBuilder::create()
            ->text(m_scanning ? "Scanning for nearby Wi-Fi access points..." : "No Wi-Fi networks found nearby.")
            ->textSize(config->metrics.caption_size)
            ->textColor(config->colors.on_surface_variant)
            ->padding(8)
            ->build();
        list_box->add_view(empty_text);
    } else {
        for (const auto& net : m_networks) {
            list_box->add_view(create_network_row(net));
        }
    }

    card->add_view(list_box);
    sec->add_view(card);
    return sec;
}

std::shared_ptr<View> WifiDetailView::create_network_row(const WifiNetwork& net) {
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

    // Wi-Fi signal icon
    auto icon = std::make_shared<ImageView>(WifiBackend::resolve_signal_icon(net.signal_percent));
    icon->set_target_size(24);
    LayoutParams icon_lp(24, 24);
    icon_lp.gravity = Gravity::CenterVertical;
    row->add_view(icon, icon_lp);

    // Text column
    auto text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams text_lp;
    text_lp.width = 0;
    text_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    text_lp.weight = 1.0f;
    text_col->set_layout_params(text_lp);

    auto title = TextViewBuilder::create()
        ->text(net.ssid)
        ->bold(true)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    text_col->add_view(title);

    std::string sub = net.security + " · " + net.frequency + " (" + std::to_string(net.signal_percent) + "%)";
    if (net.is_saved && !net.is_connected) sub += " · Saved";
    auto sub_lbl = TextViewBuilder::create()
        ->text(sub)
        ->textSize(config->metrics.caption_size)
        ->textColor(net.is_connected ? config->colors.primary : config->colors.on_surface_variant)
        ->build();
    text_col->add_view(sub_lbl);
    row->add_view(text_col);

    if (net.is_connected) {
        auto active_badge = TextViewBuilder::create()
            ->text("✓ Active")
            ->textSize(config->metrics.caption_size)
            ->textColor(config->colors.primary)
            ->bold(true)
            ->build();
        row->add_view(active_badge);
    } else {
        auto btn_conn = ButtonBuilder::create()
            ->text(net.is_saved ? "Connect" : (net.is_secured ? "Connect..." : "Connect"))
            ->primary(!net.is_saved)
            ->backgroundColor(net.is_saved ? config->colors.surface_variant : config->colors.primary)
            ->textColor(net.is_saved ? config->colors.on_surface : config->colors.on_primary)
            ->textSize(config->metrics.caption_size)
            ->cornerRadius(btn_radius)
            ->padding(10, 4)
            ->onClick([this, net]() { on_connect_clicked(net); })
            ->build();
        LayoutParams btn_lp(90, 30);
        btn_lp.gravity = Gravity::CenterVertical;
        btn_conn->set_layout_params(btn_lp);
        row->add_view(btn_conn);
    }

    return row;
}

std::shared_ptr<View> WifiDetailView::create_disabled_state() {
    auto config = Config::get();
    int btn_radius = config->metrics.corner_radius > 0 ? std::max(4, config->metrics.corner_radius - 4) : 8;

    auto box = std::make_shared<LinearLayout>(Orientation::Vertical);
    box->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    box->set_divider_spacing(14);
    box->set_padding(24);
    box->set_gravity(Gravity::CenterHorizontal);

    auto empty_icon = std::make_shared<ImageView>("network-wireless-offline");
    empty_icon->set_target_size(48);
    empty_icon->set_dim(0.3f);
    box->add_view(empty_icon);

    auto title = TextViewBuilder::create()
        ->text("Wi-Fi is Turned Off")
        ->bold(true)
        ->textSize(config->metrics.h2_size)
        ->center(true)
        ->textColor(config->colors.on_surface)
        ->build();
    box->add_view(title);

    auto desc = TextViewBuilder::create()
        ->text("Turn on Wi-Fi to scan for nearby wireless networks and connect to the internet.")
        ->textSize(config->metrics.font_size)
        ->center(true)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    box->add_view(desc);

    auto btn_on = ButtonBuilder::create()
        ->text("Turn On Wi-Fi")
        ->primary(true)
        ->textSize(config->metrics.font_size)
        ->cornerRadius(btn_radius)
        ->padding(16, 8)
        ->onClick([this]() { toggle_radio(true); })
        ->build();
    box->add_view(btn_on);

    return box;
}

void WifiDetailView::show_password_modal(const WifiNetwork& net) {
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
    LayoutParams card_lp(440, static_cast<int>(LayoutDimension::WrapContent));
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
        ->text("Connect to " + net.ssid)
        ->bold(true)
        ->textSize(config->metrics.h2_size)
        ->textColor(config->colors.on_surface)
        ->build();
    content->add_view(title);

    // Subtitle
    auto desc = TextViewBuilder::create()
        ->text("Enter the network password (" + net.security + "):")
        ->textSize(config->metrics.caption_size)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    content->add_view(desc);

    // Password input
    auto pwd_input = std::make_shared<EditText>();
    pwd_input->set_hint("Password / Security Key");
    pwd_input->set_password_mode(true);
    LayoutParams pwd_lp(static_cast<int>(LayoutDimension::MatchParent), 36);
    pwd_input->set_layout_params(pwd_lp);
    content->add_view(pwd_input);

    // Show password toggle
    auto show_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    show_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    show_row->set_divider_spacing(8);
    show_row->set_gravity(Gravity::CenterVertical);

    auto sw_reveal = SwitchBuilder::create()
        ->checked(false)
        ->onCheckedChanged([pwd_input](bool checked) {
            pwd_input->set_password_mode(!checked);
            pwd_input->request_redraw();
        })
        ->build();
    show_row->add_view(sw_reveal);

    auto show_lbl = TextViewBuilder::create()
        ->text("Show password")
        ->textSize(config->metrics.caption_size)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    show_row->add_view(show_lbl);
    content->add_view(show_row);

    // Action buttons row
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

    auto btn_conn = ButtonBuilder::create()
        ->text("Connect")
        ->textSize(config->metrics.font_size)
        ->cornerRadius(btn_radius)
        ->primary(true)
        ->padding(14, 6)
        ->onClick([this, overlay, net, pwd_input]() {
            std::string pwd = pwd_input->get_text();
            remove_view(overlay);
            m_active_modal.reset();
            WifiBackend::connect_network(net.ssid, pwd);
            if (auto engine = AppEngine::instance()) {
                auto alive = m_alive;
                engine->post_delayed(2500, [this, alive]() {
                    if (!*alive) return;
                    load_data();
                    setup_ui();
                });
            }
        })
        ->build();
    btn_box->add_view(btn_conn);

    content->add_view(btn_box);
    card->add_view(content);
    overlay->add_view(card);
    m_active_modal = overlay;
    add_view(overlay);
}

void WifiDetailView::toggle_radio(bool enabled) {
    m_adapter.radio_enabled = enabled;
    WifiBackend::set_radio_enabled(enabled);
    load_data();
    setup_ui();
}

void WifiDetailView::on_connect_clicked(const WifiNetwork& net) {
    if (!net.is_secured || net.is_saved) {
        WifiBackend::connect_network(net.ssid);
        if (auto engine = AppEngine::instance()) {
            auto alive = m_alive;
            engine->post_delayed(2500, [this, alive]() {
                if (!*alive) return;
                load_data();
                setup_ui();
            });
        }
    } else {
        show_password_modal(net);
    }
}

void WifiDetailView::on_disconnect_clicked() {
    WifiBackend::disconnect_current(m_adapter.iface);
    load_data();
    setup_ui();
}

void WifiDetailView::on_forget_clicked(const std::string& ssid) {
    WifiBackend::forget_network(ssid);
    load_data();
    setup_ui();
}

void WifiDetailView::trigger_scan() {
    m_scanning = true;
    WifiBackend::rescan_async();
    setup_ui();
    if (auto engine = AppEngine::instance()) {
        auto alive = m_alive;
        engine->post_delayed(2000, [this, alive]() {
            if (!*alive) return;
            m_scanning = false;
            load_data();
            setup_ui();
        });
    }
}

} // namespace miqu
