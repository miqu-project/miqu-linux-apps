#include "modules/airplane/airplane_detail_view.hpp"
#include "modules/airplane/airplane_backend.hpp"
#include <miqutoolkit/core/config.hpp>
#include <miqutoolkit/core/app_engine.hpp>

namespace miqu {

AirplaneDetailView::AirplaneDetailView() {
    set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    load_data();
    setup_ui();
    start_polling();
}

AirplaneDetailView::~AirplaneDetailView() {
    stop_polling();
}

void AirplaneDetailView::set_is_fullscreen(bool fullscreen) {
    m_is_fullscreen = fullscreen;
    setup_ui();
}

void AirplaneDetailView::refresh_state() {
    load_data();
    setup_ui();
}

void AirplaneDetailView::load_data() {
    m_status = AirplaneBackend::get_status();
}

void AirplaneDetailView::start_polling() {
    stop_polling();
    auto engine = AppEngine::instance();
    if (!engine) return;

    m_poll_timer_id = engine->post_delayed(3000, [this]() {
        m_poll_timer_id = 0;
        load_data();
        setup_ui();
        start_polling();
    });
}

void AirplaneDetailView::stop_polling() {
    if (m_poll_timer_id != 0) {
        if (auto engine = AppEngine::instance()) {
            engine->cancel_delayed(m_poll_timer_id);
        }
        m_poll_timer_id = 0;
    }
}

void AirplaneDetailView::on_toggle_mode(bool enabled) {
    AirplaneBackend::set_enabled(enabled);
    if (auto engine = AppEngine::instance()) {
        engine->post_delayed(600, [this]() {
            load_data();
            setup_ui();
        });
    }
}

void AirplaneDetailView::setup_ui() {
    clear_views();

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

    m_content_layout->add_view(create_hero_card());
    m_content_layout->add_view(create_radios_card());
    m_content_layout->add_view(create_info_card());

    m_scroll->set_content_view(m_content_layout);
    add_view(m_scroll);
}

std::shared_ptr<View> AirplaneDetailView::create_hero_card() {
    auto config = Config::get();
    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_radius(config->metrics.corner_radius);
    card->set_padding(20);

    auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    row->set_divider_spacing(16);

    auto icon = std::make_shared<ImageView>("airplane-mode");
    int icon_size = m_is_fullscreen ? 54 : 44;
    icon->set_target_size(icon_size);
    LayoutParams icon_lp;
    icon_lp.width = icon_size;
    icon_lp.height = icon_size;
    icon_lp.gravity = Gravity::CenterVertical;
    row->add_view(icon, icon_lp);

    auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams col_lp;
    col_lp.width = 0;
    col_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    col_lp.weight = 1.0f;
    col->set_layout_params(col_lp);

    auto title = TextViewBuilder::create()
        ->text("Airplane Mode")
        ->bold(true)
        ->textSize(m_is_fullscreen ? 20 : 17)
        ->textColor(config->colors.on_surface)
        ->build();
    col->add_view(title);

    std::string sub = m_status.enabled
        ? "All wireless signals are currently turned off"
        : "Turn off Wi-Fi, Bluetooth and cellular signals";
    auto subtitle = TextViewBuilder::create()
        ->text(sub)
        ->textSize(config->metrics.caption_size)
        ->textColor(m_status.enabled ? config->colors.primary : config->colors.on_surface_variant)
        ->build();
    col->add_view(subtitle);
    row->add_view(col);

    auto sw = SwitchBuilder::create()
        ->checked(m_status.enabled)
        ->onCheckedChanged([this](bool checked) {
            on_toggle_mode(checked);
        })
        ->build();
    LayoutParams sw_lp;
    sw_lp.width = static_cast<int>(LayoutDimension::WrapContent);
    sw_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    sw_lp.gravity = Gravity::CenterVertical;
    sw->set_layout_params(sw_lp);
    row->add_view(sw);

    if (!m_is_fullscreen && m_on_request_fullscreen) {
        auto btn_expand = ButtonBuilder::create()
            ->text("⛶")
            ->textSize(13)
            ->textColor(config->colors.primary)
            ->backgroundColor(config->colors.surface_variant)
            ->cornerRadius(6)
            ->onClick([this]() { if (m_on_request_fullscreen) m_on_request_fullscreen(); })
            ->build();
        LayoutParams exp_lp;
        exp_lp.width = 34;
        exp_lp.height = 34;
        exp_lp.gravity = Gravity::CenterVertical;
        btn_expand->set_layout_params(exp_lp);
        row->add_view(btn_expand);
    }

    card->add_view(row);
    return card;
}

std::shared_ptr<View> AirplaneDetailView::create_radio_row(const std::string& icon_name,
                                                         const std::string& title_str,
                                                         const std::string& subtitle_str,
                                                         bool is_active) {
    auto config = Config::get();
    auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    row->set_divider_spacing(14);
    row->set_padding(0, 4);

    auto icon = std::make_shared<ImageView>(icon_name);
    icon->set_target_size(24);
    LayoutParams icon_lp;
    icon_lp.width = 24;
    icon_lp.height = 24;
    icon_lp.gravity = Gravity::CenterVertical;
    row->add_view(icon, icon_lp);

    auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams col_lp;
    col_lp.width = 0;
    col_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    col_lp.weight = 1.0f;
    col->set_layout_params(col_lp);

    auto title = TextViewBuilder::create()
        ->text(title_str)
        ->bold(true)
        ->textSize(14)
        ->textColor(config->colors.on_surface)
        ->build();
    col->add_view(title);

    auto sub = TextViewBuilder::create()
        ->text(subtitle_str)
        ->textSize(11)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    col->add_view(sub);
    row->add_view(col);

    auto badge = TextViewBuilder::create()
        ->text(is_active ? "Active" : "Disabled (Off)")
        ->bold(true)
        ->textSize(12)
        ->textColor(is_active ? config->colors.primary : config->colors.outline)
        ->build();
    LayoutParams badge_lp;
    badge_lp.width = static_cast<int>(LayoutDimension::WrapContent);
    badge_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    badge_lp.gravity = Gravity::CenterVertical;
    badge->set_layout_params(badge_lp);
    row->add_view(badge);

    return row;
}

std::shared_ptr<View> AirplaneDetailView::create_radios_card() {
    auto config = Config::get();
    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface_variant);
    card->set_radius(config->metrics.corner_radius);
    card->set_padding(18);

    auto layout = std::make_shared<LinearLayout>(Orientation::Vertical);
    layout->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    layout->set_divider_spacing(12);

    auto section_title = TextViewBuilder::create()
        ->text("INDIVIDUAL WIRELESS RADIOS")
        ->bold(true)
        ->textSize(11)
        ->textColor(config->colors.outline)
        ->build();
    layout->add_view(section_title);

    layout->add_view(create_radio_row(
        "network-wireless",
        "Wi-Fi (Wireless LAN)",
        "IEEE 802.11 a/b/g/n/ac/ax wireless network adapter",
        m_status.wifi_enabled
    ));

    layout->add_view(create_radio_row(
        "bluetooth",
        "Bluetooth Adapter",
        "Short-range 2.4 GHz RF peripheral communication",
        m_status.bluetooth_enabled
    ));

    if (m_status.has_wwan) {
        layout->add_view(create_radio_row(
            "network-cellular",
            "Cellular (WWAN)",
            "Mobile broadband data modem",
            m_status.wwan_enabled
        ));
    }

    card->add_view(layout);
    return card;
}

std::shared_ptr<View> AirplaneDetailView::create_info_card() {
    auto config = Config::get();
    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_radius(config->metrics.corner_radius);
    card->set_padding(18);

    auto layout = std::make_shared<LinearLayout>(Orientation::Vertical);
    layout->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    layout->set_divider_spacing(14);

    std::string btn_label = m_status.enabled
        ? "Turn Off Airplane Mode (Restore All Radios)"
        : "Turn On Airplane Mode (Disable All Radios)";

    auto btn = ButtonBuilder::create()
        ->text(btn_label)
        ->primary(true)
        ->cornerRadius(std::max(6, config->metrics.corner_radius - 2))
        ->padding(16, 10)
        ->textSize(13)
        ->onClick([this]() {
            on_toggle_mode(!m_status.enabled);
        })
        ->build();
    LayoutParams btn_lp;
    btn_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    btn_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    btn->set_layout_params(btn_lp);
    layout->add_view(btn);

    auto info = TextViewBuilder::create()
        ->text("Turning on Airplane Mode immediately blocks all wireless transmissions (Wi-Fi, Bluetooth, and cellular modems) via rfkill and NetworkManager to comply with aviation regulations.")
        ->textSize(config->metrics.caption_size)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    layout->add_view(info);

    card->add_view(layout);
    return card;
}

} // namespace miqu
