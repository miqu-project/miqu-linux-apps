#include "modules/display/brightness_detail_view.hpp"
#include "modules/display/display_backend.hpp"
#include <miqutoolkit/core/config.hpp>
#include <miqutoolkit/view/divider_view.hpp>

namespace miqu {

BrightnessDetailView::BrightnessDetailView() {
    int pct = DisplayBackend::get_brightness_percent();
    m_brightness = pct / 100.0f;
    set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    setup_ui();
}


void BrightnessDetailView::setup_ui() {
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
    m_content_layout->set_padding(32);

    m_content_layout->add_view(create_hero_card());
    m_content_layout->add_view(create_slider_card());
    m_content_layout->add_view(create_presets_card());
    m_content_layout->add_view(create_hardware_card());

    m_scroll->set_content_view(m_content_layout);
    add_view(m_scroll);
}

std::shared_ptr<View> BrightnessDetailView::create_hero_card() {
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

    auto icon = std::make_shared<ImageView>("display-brightness");
    icon->set_target_size(64);
    LayoutParams icon_lp;
    icon_lp.width = 64;
    icon_lp.height = 64;
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
        ->text("Screen Brightness")
        ->bold(true)
        ->textSize(20)
        ->textColor(config->colors.on_surface)
        ->build();
    text_col->add_view(title);

    auto info = DisplayBackend::get_brightness_info();
    int pct = static_cast<int>(m_brightness * 100.0f + 0.5f);
    auto subtitle = TextViewBuilder::create()
        ->text("Backlight level: " + std::to_string(pct) + "% · " + info.device_name)
        ->textSize(12)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    text_col->add_view(subtitle);

    auto badge = TextViewBuilder::create()
        ->text("Hardware Controlled via brightnessctl ✓")
        ->textSize(11)
        ->textColor(config->colors.primary)
        ->build();
    text_col->add_view(badge);

    row->add_view(text_col);

    
    card->add_view(row);
    return card;
}

std::shared_ptr<View> BrightnessDetailView::create_slider_card() {
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
        ->text("BACKLIGHT INTENSITY")
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

    int pct = static_cast<int>(m_brightness * 100.0f + 0.5f);
    auto val_lbl = TextViewBuilder::create()
        ->text(std::to_string(pct) + "%")
        ->bold(true)
        ->textSize(15)
        ->textColor(config->colors.primary)
        ->build();
    header_row->add_view(val_lbl);
    col->add_view(header_row);

    // Slider Row with Dim and Bright icons
    auto slider_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    slider_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        36
    ));
    slider_row->set_divider_spacing(12);

    auto icon_dim = std::make_shared<ImageView>("display-brightness");
    icon_dim->set_target_size(18);
    icon_dim->set_dim(0.35f);
    LayoutParams idim_lp;
    idim_lp.width = 18;
    idim_lp.height = 18;
    idim_lp.gravity = Gravity::CenterVertical;
    slider_row->add_view(icon_dim, idim_lp);

    auto slider = SliderBuilder::create()
        ->value(m_brightness)
        ->onValueChanged([this, val_lbl](float val, bool) {
            m_brightness = val;
            int p = static_cast<int>(val * 100.0f + 0.5f);
            val_lbl->set_text(std::to_string(p) + "%");
            val_lbl->request_redraw();
            DisplayBackend::set_brightness_percent(p);
        })
        ->build();
    LayoutParams sl_lp;
    sl_lp.width = 0;
    sl_lp.height = 30;
    sl_lp.weight = 1.0f;
    sl_lp.gravity = Gravity::CenterVertical;
    slider->set_layout_params(sl_lp);
    slider_row->add_view(slider);

    auto icon_bright = std::make_shared<ImageView>("display-brightness");
    icon_bright->set_target_size(24);
    LayoutParams ibright_lp;
    ibright_lp.width = 24;
    ibright_lp.height = 24;
    ibright_lp.gravity = Gravity::CenterVertical;
    slider_row->add_view(icon_bright, ibright_lp);

    col->add_view(slider_row);
    card->add_view(col);
    return card;
}

std::shared_ptr<View> BrightnessDetailView::create_presets_card() {
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
        ->text("QUICK BRIGHTNESS PRESETS")
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

    struct Preset {
        std::string label;
        float value;
    };
    std::vector<Preset> presets = {
        {"25% Dim", 0.25f},
        {"50% Medium", 0.50f},
        {"75% Bright", 0.75f},
        {"100% Max", 1.0f}
    };

    for (const auto& p : presets) {
        int cur_p = static_cast<int>(m_brightness * 100.0f + 0.5f);
        int target_p = static_cast<int>(p.value * 100.0f + 0.5f);
        bool is_sel = (std::abs(cur_p - target_p) <= 5);

        auto btn = ButtonBuilder::create()
            ->text(p.label)
            ->textSize(12)
            ->textColor(is_sel ? config->colors.on_primary : config->colors.on_surface)
            ->backgroundColor(is_sel ? config->colors.primary : config->colors.surface_variant)
            ->cornerRadius(8)
            ->onClick([this, v = p.value]() {
                m_brightness = v;
                int p_val = static_cast<int>(v * 100.0f + 0.5f);
                DisplayBackend::set_brightness_percent(p_val);
                setup_ui();
            })
            ->build();
        LayoutParams btn_lp;
        btn_lp.width = 0;
        btn_lp.height = 36;
        btn_lp.weight = 1.0f;
        btn->set_layout_params(btn_lp);
        row->add_view(btn);
    }

    col->add_view(row);
    card->add_view(col);
    return card;
}

std::shared_ptr<View> BrightnessDetailView::create_hardware_card() {
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
        ->text("HARDWARE INFORMATION")
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

    add_row("Device Name", "intel_backlight");
    add_row("Subsystem", "backlight (/sys/class/backlight)");
    add_row("Max Raw Level", "7500 steps");
    add_row("Controller CLI", "brightnessctl (native Arch Linux)");

    card->add_view(col);
    return card;
}

} // namespace miqu
