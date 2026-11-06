#include "modules/windowmanager/wm_effects_detail_view.hpp"
#include "modules/windowmanager/wm_backend.hpp"
#include <miqutoolkit/core/config.hpp>
#include <algorithm>

namespace miqu {

WmEffectsDetailView::WmEffectsDetailView() {
    WmOptions opts = WmBackend::get_options();
    m_opacity_active = opts.opacity_active;
    m_opacity_inactive = opts.opacity_inactive;
    m_animations = opts.animations;
    m_blur = opts.blur;

    set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    setup_ui();
}

void WmEffectsDetailView::set_is_fullscreen(bool fullscreen) {
    if (m_is_fullscreen != fullscreen) {
        m_is_fullscreen = fullscreen;
        setup_ui();
    }
}

void WmEffectsDetailView::setup_ui() {
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
    m_content_layout->add_view(create_opacity_card());
    m_content_layout->add_view(create_toggles_card());

    m_scroll->set_content_view(m_content_layout);
    add_view(m_scroll);
}

std::shared_ptr<View> WmEffectsDetailView::create_hero_card() {
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

    auto icon = std::make_shared<ImageView>("color-management");
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
        ->text("Opacity & Visual Effects")
        ->bold(true)
        ->textSize(m_is_fullscreen ? 20 : 17)
        ->textColor(config->colors.on_surface)
        ->build();
    text_col->add_view(title);

    int act_pct = static_cast<int>(m_opacity_active * 100.0f + 0.5f);
    int inact_pct = static_cast<int>(m_opacity_inactive * 100.0f + 0.5f);
    auto subtitle = TextViewBuilder::create()
        ->text("Active Opacity: " + std::to_string(act_pct) + "% · Inactive Opacity: " + std::to_string(inact_pct) + "%")
        ->textSize(12)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    text_col->add_view(subtitle);

    auto badge = TextViewBuilder::create()
        ->text("GPU SceneFX Dual Kawase Shaders ✓")
        ->textSize(11)
        ->textColor(config->colors.primary)
        ->build();
    text_col->add_view(badge);

    row->add_view(text_col);
    card->add_view(row);
    return card;
}

std::shared_ptr<View> WmEffectsDetailView::create_opacity_card() {
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
    col->set_divider_spacing(16);

    // Active Opacity
    {
        auto h_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
        h_row->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));

        auto title = TextViewBuilder::create()
            ->text("FOCUSED WINDOW OPACITY")
            ->bold(true)
            ->textSize(11)
            ->textColor(config->colors.outline)
            ->build();
        LayoutParams t_lp;
        t_lp.width = 0;
        t_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        t_lp.weight = 1.0f;
        title->set_layout_params(t_lp);
        h_row->add_view(title);

        int pct = static_cast<int>(m_opacity_active * 100.0f + 0.5f);
        auto val_lbl = TextViewBuilder::create()
            ->text(std::to_string(pct) + "%")
            ->bold(true)
            ->textSize(14)
            ->textColor(config->colors.primary)
            ->build();
        h_row->add_view(val_lbl);
        col->add_view(h_row);

        auto slider = SliderBuilder::create()
            ->value(m_opacity_active)
            ->onValueChanged([this, val_lbl](float val, bool) {
                m_opacity_active = std::clamp(val, 0.40f, 1.0f);
                int p = static_cast<int>(m_opacity_active * 100.0f + 0.5f);
                val_lbl->set_text(std::to_string(p) + "%");
                val_lbl->request_redraw();
                WmBackend::set_opacities(m_opacity_active, m_opacity_inactive);
            })
            ->build();
        LayoutParams sl_lp;
        sl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        sl_lp.height = 30;
        slider->set_layout_params(sl_lp);
        col->add_view(slider);
    }

    // Inactive Opacity
    {
        auto h_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
        h_row->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));

        auto title = TextViewBuilder::create()
            ->text("UNFOCUSED WINDOW OPACITY (DIMMING)")
            ->bold(true)
            ->textSize(11)
            ->textColor(config->colors.outline)
            ->build();
        LayoutParams t_lp;
        t_lp.width = 0;
        t_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        t_lp.weight = 1.0f;
        title->set_layout_params(t_lp);
        h_row->add_view(title);

        int pct = static_cast<int>(m_opacity_inactive * 100.0f + 0.5f);
        auto val_lbl = TextViewBuilder::create()
            ->text(std::to_string(pct) + "%")
            ->bold(true)
            ->textSize(14)
            ->textColor(config->colors.primary)
            ->build();
        h_row->add_view(val_lbl);
        col->add_view(h_row);

        auto slider = SliderBuilder::create()
            ->value(m_opacity_inactive)
            ->onValueChanged([this, val_lbl](float val, bool) {
                m_opacity_inactive = std::clamp(val, 0.30f, 1.0f);
                int p = static_cast<int>(m_opacity_inactive * 100.0f + 0.5f);
                val_lbl->set_text(std::to_string(p) + "%");
                val_lbl->request_redraw();
                WmBackend::set_opacities(m_opacity_active, m_opacity_inactive);
            })
            ->build();
        LayoutParams sl_lp;
        sl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        sl_lp.height = 30;
        slider->set_layout_params(sl_lp);
        col->add_view(slider);
    }

    card->add_view(col);
    return card;
}

std::shared_ptr<View> WmEffectsDetailView::create_toggles_card() {
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
    col->set_divider_spacing(14);

    // Animations toggle
    {
        auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
        row->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));
        row->set_divider_spacing(16);

        auto t_col = std::make_shared<LinearLayout>(Orientation::Vertical);
        LayoutParams t_lp;
        t_lp.width = 0;
        t_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        t_lp.weight = 1.0f;
        t_col->set_layout_params(t_lp);

        auto title = TextViewBuilder::create()
            ->text("Window & Workspace Animations")
            ->bold(true)
            ->textSize(14)
            ->textColor(config->colors.on_surface)
            ->build();
        t_col->add_view(title);

        auto desc = TextViewBuilder::create()
            ->text("Smooth physics springs and cubic bézier transitions")
            ->textSize(12)
            ->textColor(config->colors.on_surface_variant)
            ->build();
        t_col->add_view(desc);
        row->add_view(t_col);

        auto btn = std::make_shared<Button>();
        btn->set_text(m_animations ? "Enabled" : "Disabled");
        btn->set_style(m_animations ? ButtonStyle::Primary : ButtonStyle::Standard);
        btn->set_on_click_listener([this]() {
            m_animations = !m_animations;
            WmBackend::set_animations(m_animations);
            setup_ui();
        });
        LayoutParams btn_lp;
        btn_lp.gravity = Gravity::CenterVertical;
        row->add_view(btn, btn_lp);
        col->add_view(row);
    }

    // Blur toggle
    {
        auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
        row->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));
        row->set_divider_spacing(16);

        auto t_col = std::make_shared<LinearLayout>(Orientation::Vertical);
        LayoutParams t_lp;
        t_lp.width = 0;
        t_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        t_lp.weight = 1.0f;
        t_col->set_layout_params(t_lp);

        auto title = TextViewBuilder::create()
            ->text("SceneFX Background Blur")
            ->bold(true)
            ->textSize(14)
            ->textColor(config->colors.on_surface)
            ->build();
        t_col->add_view(title);

        auto desc = TextViewBuilder::create()
            ->text("Multi-pass dual-kawase blur filter behind translucent views")
            ->textSize(12)
            ->textColor(config->colors.on_surface_variant)
            ->build();
        t_col->add_view(desc);
        row->add_view(t_col);

        auto btn = std::make_shared<Button>();
        btn->set_text(m_blur ? "Enabled" : "Disabled");
        btn->set_style(m_blur ? ButtonStyle::Primary : ButtonStyle::Standard);
        btn->set_on_click_listener([this]() {
            m_blur = !m_blur;
            WmBackend::set_blur(m_blur);
            setup_ui();
        });
        LayoutParams btn_lp;
        btn_lp.gravity = Gravity::CenterVertical;
        row->add_view(btn, btn_lp);
        col->add_view(row);
    }

    card->add_view(col);
    return card;
}

} // namespace miqu
