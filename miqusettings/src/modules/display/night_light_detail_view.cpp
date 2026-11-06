#include "modules/display/night_light_detail_view.hpp"
#include "modules/display/display_backend.hpp"
#include <miqutoolkit/core/config.hpp>
#include <miqutoolkit/view/divider_view.hpp>

namespace miqu {

NightLightDetailView::NightLightDetailView() {
    m_enabled = DisplayBackend::is_night_light_active();
    set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    setup_ui();
}

void NightLightDetailView::set_is_fullscreen(bool fullscreen) {
    if (m_is_fullscreen != fullscreen) {
        m_is_fullscreen = fullscreen;
        setup_ui();
    }
}

void NightLightDetailView::setup_ui() {
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
    m_content_layout->set_padding(m_is_fullscreen ? 32 : 16);

    m_content_layout->add_view(create_hero_card());
    m_content_layout->add_view(create_toggle_card());
    if (m_enabled) {
        m_content_layout->add_view(create_warmth_card());
        m_content_layout->add_view(create_schedule_card());
    }
    m_content_layout->add_view(create_engine_card());

    m_scroll->set_content_view(m_content_layout);
    add_view(m_scroll);
}

std::shared_ptr<View> NightLightDetailView::create_hero_card() {
    auto config = Config::get();
    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_radius(config->metrics.corner_radius);
    card->set_padding(18);
    card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    row->set_divider_spacing(16);

    auto icon = std::make_shared<ImageView>("weather-clear-night");
    icon->set_target_size(m_is_fullscreen ? 64 : 48);
    LayoutParams icon_lp;
    icon_lp.width = m_is_fullscreen ? 64 : 48;
    icon_lp.height = m_is_fullscreen ? 64 : 48;
    icon_lp.gravity = Gravity::CenterVertical;
    row->add_view(icon, icon_lp);

    auto text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams text_lp;
    text_lp.width = 0;
    text_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    text_lp.weight = 1.0f;
    text_col->set_layout_params(text_lp);
    text_col->set_divider_spacing(3);

    auto title = TextViewBuilder::create()
        ->text("Night Light (Sunset & Gamma)")
        ->bold(true)
        ->textSize(m_is_fullscreen ? 20 : 17)
        ->textColor(config->colors.on_surface)
        ->build();
    text_col->add_view(title);

    int kelvin = static_cast<int>(3000 + m_warmth * 3500);
    auto subtitle = TextViewBuilder::create()
        ->text(m_enabled ? "Active at " + std::to_string(kelvin) + "K · Blue light filter" : "Disabled · Standard 6500K daylight")
        ->textSize(12)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    text_col->add_view(subtitle);

    auto badge = TextViewBuilder::create()
        ->text(m_enabled ? "wlr-gamma-control active ✓" : "Gamma table unmodified")
        ->textSize(11)
        ->textColor(m_enabled ? config->colors.primary : config->colors.outline)
        ->build();
    text_col->add_view(badge);

    row->add_view(text_col);

    if (!m_is_fullscreen) {
        auto btn_expand = ButtonBuilder::create()
            ->text("⛶")
            ->textSize(14)
            ->textColor(config->colors.primary)
            ->backgroundColor(config->colors.surface_variant)
            ->cornerRadius(6)
            ->onClick([this]() {
                if (m_on_request_fullscreen) m_on_request_fullscreen();
            })
            ->build();
        LayoutParams btn_lp;
        btn_lp.width = 34;
        btn_lp.height = 34;
        btn_lp.gravity = Gravity::CenterVertical;
        btn_expand->set_layout_params(btn_lp);
        row->add_view(btn_expand);
    }

    card->add_view(row);
    return card;
}

std::shared_ptr<View> NightLightDetailView::create_toggle_card() {
    auto config = Config::get();
    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_radius(config->metrics.corner_radius);
    card->set_padding(18);
    card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    row->set_divider_spacing(12);

    auto text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams text_lp;
    text_lp.width = 0;
    text_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    text_lp.weight = 1.0f;
    text_col->set_layout_params(text_lp);

    auto toggle_title = TextViewBuilder::create()
        ->text("Enable Night Light")
        ->bold(true)
        ->textSize(14)
        ->textColor(config->colors.on_surface)
        ->build();
    text_col->add_view(toggle_title);

    auto toggle_sub = TextViewBuilder::create()
        ->text("Warmer screen colors help reduce eye strain and promote restful sleep")
        ->textSize(12)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    text_col->add_view(toggle_sub);
    row->add_view(text_col);

    auto sw = SwitchBuilder::create()
        ->checked(m_enabled)
        ->onCheckedChanged([this](bool checked) {
            m_enabled = checked;
            int k = static_cast<int>(3000 + m_warmth * 3500);
            DisplayBackend::set_night_light_enabled(m_enabled, k);
            setup_ui();
        })
        ->build();
    LayoutParams sw_lp;
    sw_lp.gravity = Gravity::CenterVertical;
    sw->set_layout_params(sw_lp);
    row->add_view(sw);

    card->add_view(row);
    return card;
}

std::shared_ptr<View> NightLightDetailView::create_warmth_card() {
    auto config = Config::get();
    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_radius(config->metrics.corner_radius);
    card->set_padding(18);
    card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
    col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    col->set_divider_spacing(12);

    auto header_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    header_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto title = TextViewBuilder::create()
        ->text("COLOR TEMPERATURE (WARMTH)")
        ->bold(true)
        ->textSize(11)
        ->textColor(config->colors.outline)
        ->build();
    LayoutParams t_lp;
    t_lp.width = 0;
    t_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    t_lp.weight = 1.0f;
    title->set_layout_params(t_lp);
    header_row->add_view(title);

    int kelvin = static_cast<int>(3000 + m_warmth * 3500);
    auto kelvin_lbl = TextViewBuilder::create()
        ->text(std::to_string(kelvin) + "K")
        ->bold(true)
        ->textSize(15)
        ->textColor(config->colors.primary)
        ->build();
    header_row->add_view(kelvin_lbl);
    col->add_view(header_row);

    // Warmth slider
    auto slider = SliderBuilder::create()
        ->value(m_warmth)
        ->onValueChanged([this, kelvin_lbl](float val, bool) {
            m_warmth = val;
            int k = static_cast<int>(3000 + val * 3500);
            kelvin_lbl->set_text(std::to_string(k) + "K");
            kelvin_lbl->request_redraw();
            if (m_enabled) {
                DisplayBackend::set_gamma_temperature(k);
            }
        })
        ->build();
    LayoutParams sl_lp;
    sl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    sl_lp.height = 30;
    slider->set_layout_params(sl_lp);
    col->add_view(slider);

    // Labels row
    auto labels_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    labels_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    auto warm_lbl = TextViewBuilder::create()->text("Warm (3000K Sunset)")->textSize(11)->textColor(config->colors.outline)->build();
    LayoutParams wl_lp; wl_lp.width = 0; wl_lp.height = static_cast<int>(LayoutDimension::WrapContent); wl_lp.weight = 1.0f;
    warm_lbl->set_layout_params(wl_lp);
    labels_row->add_view(warm_lbl);

    auto cool_lbl = TextViewBuilder::create()->text("Cool (6500K Daylight)")->textSize(11)->textColor(config->colors.outline)->build();
    labels_row->add_view(cool_lbl);
    col->add_view(labels_row);

    auto div = std::make_shared<DividerView>();
    div->set_color(config->colors.outline_variant);
    div->set_thickness(1);
    col->add_view(div);

    // Presets Row
    auto presets_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    presets_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    presets_row->set_divider_spacing(8);

    struct WarmPreset { std::string label; float val; };
    std::vector<WarmPreset> presets = {
        {"3500K Sunset", (3500.0f - 3000.0f) / 3500.0f},
        {"4500K Balanced", (4500.0f - 3000.0f) / 3500.0f},
        {"5500K Mild", (5500.0f - 3000.0f) / 3500.0f}
    };

    for (const auto& p : presets) {
        int cur_k = static_cast<int>(3000 + m_warmth * 3500);
        int target_k = static_cast<int>(3000 + p.val * 3500);
        bool is_sel = (std::abs(cur_k - target_k) < 200);

        auto btn = ButtonBuilder::create()
            ->text(p.label)
            ->textSize(12)
            ->textColor(is_sel ? config->colors.on_primary : config->colors.on_surface)
            ->backgroundColor(is_sel ? config->colors.primary : config->colors.surface_variant)
            ->cornerRadius(8)
            ->onClick([this, v = p.val]() {
                m_warmth = v;
                int k = static_cast<int>(3000 + v * 3500);
                if (m_enabled) {
                    DisplayBackend::set_gamma_temperature(k);
                }
                setup_ui();
            })
            ->build();
        LayoutParams btn_lp;
        btn_lp.width = 0;
        btn_lp.height = 36;
        btn_lp.weight = 1.0f;
        btn->set_layout_params(btn_lp);
        presets_row->add_view(btn);
    }
    col->add_view(presets_row);

    card->add_view(col);
    return card;
}

std::shared_ptr<View> NightLightDetailView::create_schedule_card() {
    auto config = Config::get();
    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_radius(config->metrics.corner_radius);
    card->set_padding(18);
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

    auto title = TextViewBuilder::create()
        ->text("SUNSET SCHEDULE")
        ->bold(true)
        ->textSize(11)
        ->textColor(config->colors.outline)
        ->build();
    col->add_view(title);

    auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    row->set_divider_spacing(8);

    auto btn_sunset = ButtonBuilder::create()
        ->text(m_schedule_mode == 0 ? "●  🌅 Sunset to Sunrise" : "○  🌅 Sunset to Sunrise")
        ->textSize(12)
        ->textColor(m_schedule_mode == 0 ? config->colors.primary : config->colors.on_surface)
        ->backgroundColor(m_schedule_mode == 0 ? config->colors.surface_variant : config->colors.surface)
        ->cornerRadius(8)
        ->onClick([this]() {
            m_schedule_mode = 0;
            setup_ui();
        })
        ->build();
    LayoutParams s_lp; s_lp.width = 0; s_lp.height = 40; s_lp.weight = 1.0f;
    btn_sunset->set_layout_params(s_lp);
    row->add_view(btn_sunset);

    auto btn_manual = ButtonBuilder::create()
        ->text(m_schedule_mode == 1 ? "●  🌙 Always On (Manual)" : "○  🌙 Always On (Manual)")
        ->textSize(12)
        ->textColor(m_schedule_mode == 1 ? config->colors.primary : config->colors.on_surface)
        ->backgroundColor(m_schedule_mode == 1 ? config->colors.surface_variant : config->colors.surface)
        ->cornerRadius(8)
        ->onClick([this]() {
            m_schedule_mode = 1;
            setup_ui();
        })
        ->build();
    LayoutParams m_lp; m_lp.width = 0; m_lp.height = 40; m_lp.weight = 1.0f;
    btn_manual->set_layout_params(m_lp);
    row->add_view(btn_manual);

    col->add_view(row);
    card->add_view(col);
    return card;
}

std::shared_ptr<View> NightLightDetailView::create_engine_card() {
    auto config = Config::get();
    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_radius(config->metrics.corner_radius);
    card->set_padding(18);
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

    auto title = TextViewBuilder::create()
        ->text("GAMMA ENGINE & INTEGRATION")
        ->bold(true)
        ->textSize(11)
        ->textColor(config->colors.outline)
        ->build();
    col->add_view(title);

    auto add_row = [&](const std::string& label, const std::string& val) {
        auto r = std::make_shared<LinearLayout>(Orientation::Horizontal);
        r->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));
        r->set_divider_spacing(12);

        auto l = TextViewBuilder::create()->text(label)->textSize(12)->textColor(config->colors.outline)->build();
        LayoutParams l_lp; l_lp.width = 130; l_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        l->set_layout_params(l_lp);
        r->add_view(l);

        auto v = TextViewBuilder::create()->text(val)->textSize(12)->textColor(config->colors.on_surface)->build();
        LayoutParams v_lp; v_lp.width = 0; v_lp.height = static_cast<int>(LayoutDimension::WrapContent); v_lp.weight = 1.0f;
        v->set_layout_params(v_lp);
        r->add_view(v);

        col->add_view(r);
    };

    add_row("Wayland Protocol", "wlr-gamma-control-v1");
    add_row("Engine Daemon", "gammastep / miqusunset");
    add_row("Autostart In", "~/.config/miquland/miquland.conf");
    add_row("Hardware Adjustment", "Direct DRM CRTC Gamma Ramps");

    card->add_view(col);
    return card;
}

} // namespace miqu
