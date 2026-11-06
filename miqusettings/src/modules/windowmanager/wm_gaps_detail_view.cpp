#include "modules/windowmanager/wm_gaps_detail_view.hpp"
#include "modules/windowmanager/wm_backend.hpp"
#include <miqutoolkit/core/config.hpp>
#include <algorithm>

namespace miqu {

WmGapsDetailView::WmGapsDetailView() {
    WmOptions opts = WmBackend::get_options();
    m_gaps_in = opts.gaps_in;
    m_gaps_out = opts.gaps_out;

    set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    setup_ui();
}

void WmGapsDetailView::set_is_fullscreen(bool fullscreen) {
    if (m_is_fullscreen != fullscreen) {
        m_is_fullscreen = fullscreen;
        setup_ui();
    }
}

void WmGapsDetailView::setup_ui() {
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
    m_content_layout->add_view(create_inner_gaps_card());
    m_content_layout->add_view(create_outer_gaps_card());
    m_content_layout->add_view(create_presets_card());

    m_scroll->set_content_view(m_content_layout);
    add_view(m_scroll);
}

std::shared_ptr<View> WmGapsDetailView::create_hero_card() {
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

    auto icon = std::make_shared<ImageView>("preferences-desktop-display");
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
        ->text("Window Gaps & Padding")
        ->bold(true)
        ->textSize(m_is_fullscreen ? 20 : 17)
        ->textColor(config->colors.on_surface)
        ->build();
    text_col->add_view(title);

    auto subtitle = TextViewBuilder::create()
        ->text("Inner: " + std::to_string(m_gaps_in) + "px · Outer: " + std::to_string(m_gaps_out) + "px")
        ->textSize(12)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    text_col->add_view(subtitle);

    auto badge = TextViewBuilder::create()
        ->text("Real-time Compositor Reload ✓")
        ->textSize(11)
        ->textColor(config->colors.primary)
        ->build();
    text_col->add_view(badge);

    row->add_view(text_col);
    card->add_view(row);
    return card;
}

std::shared_ptr<View> WmGapsDetailView::create_inner_gaps_card() {
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
        ->text("INNER GAPS (BETWEEN WINDOWS)")
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

    auto val_lbl = TextViewBuilder::create()
        ->text(std::to_string(m_gaps_in) + " px")
        ->bold(true)
        ->textSize(15)
        ->textColor(config->colors.primary)
        ->build();
    header_row->add_view(val_lbl);
    col->add_view(header_row);

    float slider_val = std::clamp(m_gaps_in / 32.0f, 0.0f, 1.0f);
    auto slider = SliderBuilder::create()
        ->value(slider_val)
        ->onValueChanged([this, val_lbl](float val, bool) {
            m_gaps_in = static_cast<int>(val * 32.0f + 0.5f);
            val_lbl->set_text(std::to_string(m_gaps_in) + " px");
            val_lbl->request_redraw();
            WmBackend::set_gaps(m_gaps_in, m_gaps_out);
        })
        ->build();
    LayoutParams sl_lp;
    sl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    sl_lp.height = 30;
    slider->set_layout_params(sl_lp);
    col->add_view(slider);

    card->add_view(col);
    return card;
}

std::shared_ptr<View> WmGapsDetailView::create_outer_gaps_card() {
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
        ->text("OUTER GAPS (SCREEN EDGES & MARGINS)")
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

    auto val_lbl = TextViewBuilder::create()
        ->text(std::to_string(m_gaps_out) + " px")
        ->bold(true)
        ->textSize(15)
        ->textColor(config->colors.primary)
        ->build();
    header_row->add_view(val_lbl);
    col->add_view(header_row);

    float slider_val = std::clamp(m_gaps_out / 48.0f, 0.0f, 1.0f);
    auto slider = SliderBuilder::create()
        ->value(slider_val)
        ->onValueChanged([this, val_lbl](float val, bool) {
            m_gaps_out = static_cast<int>(val * 48.0f + 0.5f);
            val_lbl->set_text(std::to_string(m_gaps_out) + " px");
            val_lbl->request_redraw();
            WmBackend::set_gaps(m_gaps_in, m_gaps_out);
        })
        ->build();
    LayoutParams sl_lp;
    sl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    sl_lp.height = 30;
    slider->set_layout_params(sl_lp);
    col->add_view(slider);

    card->add_view(col);
    return card;
}

std::shared_ptr<View> WmGapsDetailView::create_presets_card() {
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
        ->text("Gap Presets")
        ->bold(true)
        ->textSize(14)
        ->textColor(config->colors.on_surface)
        ->build();
    col->add_view(title);

    auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    row->set_divider_spacing(8);

    std::vector<std::tuple<std::string, int, int>> presets = {
        {"Zero (0/0)", 0, 0},
        {"Compact (4/6)", 4, 6},
        {"Standard (8/12)", 8, 12},
        {"Spacious (14/20)", 14, 20}
    };

    for (const auto& [label, in, out] : presets) {
        auto btn = std::make_shared<Button>();
        btn->set_text(label);
        btn->set_style(ButtonStyle::Standard);
        btn->set_on_click_listener([this, in, out]() {
            m_gaps_in = in;
            m_gaps_out = out;
            WmBackend::set_gaps(in, out);
            setup_ui();
        });
        row->add_view(btn);
    }

    col->add_view(row);
    card->add_view(col);
    return card;
}

} // namespace miqu
