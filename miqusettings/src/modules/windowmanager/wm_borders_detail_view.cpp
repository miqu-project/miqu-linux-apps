#include "modules/windowmanager/wm_borders_detail_view.hpp"
#include "modules/windowmanager/wm_backend.hpp"
#include <miqutoolkit/core/config.hpp>
#include <algorithm>

namespace miqu {

WmBordersDetailView::WmBordersDetailView() {
    WmOptions opts = WmBackend::get_options();
    m_border_width = opts.border_width;
    m_border_radius = opts.border_radius;

    set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    setup_ui();
}


void WmBordersDetailView::setup_ui() {
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
    m_content_layout->add_view(create_border_width_card());
    m_content_layout->add_view(create_corner_radius_card());

    m_scroll->set_content_view(m_content_layout);
    add_view(m_scroll);
}

std::shared_ptr<View> WmBordersDetailView::create_hero_card() {
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

    auto icon = std::make_shared<ImageView>("preferences-desktop-theme");
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
        ->text("Window Borders & Rounding")
        ->bold(true)
        ->textSize(20)
        ->textColor(config->colors.on_surface)
        ->build();
    text_col->add_view(title);

    auto subtitle = TextViewBuilder::create()
        ->text("Border Width: " + std::to_string(m_border_width) + "px · Corner Radius: " + std::to_string(m_border_radius) + "px")
        ->textSize(12)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    text_col->add_view(subtitle);

    auto badge = TextViewBuilder::create()
        ->text("Hardware Accelerated Border Stenciling ✓")
        ->textSize(11)
        ->textColor(config->colors.primary)
        ->build();
    text_col->add_view(badge);

    row->add_view(text_col);
    card->add_view(row);
    return card;
}

std::shared_ptr<View> WmBordersDetailView::create_border_width_card() {
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
        ->text("BORDER STROKE THICKNESS")
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
        ->text(std::to_string(m_border_width) + " px")
        ->bold(true)
        ->textSize(15)
        ->textColor(config->colors.primary)
        ->build();
    header_row->add_view(val_lbl);
    col->add_view(header_row);

    float slider_val = std::clamp(m_border_width / 8.0f, 0.0f, 1.0f);
    auto slider = SliderBuilder::create()
        ->value(slider_val)
        ->onValueChanged([this, val_lbl](float val, bool) {
            m_border_width = static_cast<int>(val * 8.0f + 0.5f);
            val_lbl->set_text(std::to_string(m_border_width) + " px");
            val_lbl->request_redraw();
            WmBackend::set_borders(m_border_width, m_border_radius);
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

std::shared_ptr<View> WmBordersDetailView::create_corner_radius_card() {
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
        ->text("WINDOW CORNER RADIUS (ROUNDED CORNERS)")
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
        ->text(std::to_string(m_border_radius) + " px")
        ->bold(true)
        ->textSize(15)
        ->textColor(config->colors.primary)
        ->build();
    header_row->add_view(val_lbl);
    col->add_view(header_row);

    float slider_val = std::clamp(m_border_radius / 24.0f, 0.0f, 1.0f);
    auto slider = SliderBuilder::create()
        ->value(slider_val)
        ->onValueChanged([this, val_lbl](float val, bool) {
            m_border_radius = static_cast<int>(val * 24.0f + 0.5f);
            val_lbl->set_text(std::to_string(m_border_radius) + " px");
            val_lbl->request_redraw();
            WmBackend::set_borders(m_border_width, m_border_radius);
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

} // namespace miqu
