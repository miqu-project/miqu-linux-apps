#include "modules/display/timeout_detail_view.hpp"
#include <miqutoolkit/core/config.hpp>
#include <miqutoolkit/view/divider_view.hpp>

namespace miqu {

TimeoutDetailView::TimeoutDetailView() {
    set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    setup_ui();
}


void TimeoutDetailView::setup_ui() {
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
    m_content_layout->add_view(create_display_timeout_card());
    m_content_layout->add_view(create_suspend_timeout_card());
    m_content_layout->add_view(create_options_card());

    m_scroll->set_content_view(m_content_layout);
    add_view(m_scroll);
}

std::shared_ptr<View> TimeoutDetailView::create_hero_card() {
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

    auto icon = std::make_shared<ImageView>("preferences-desktop-screensaver");
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
        ->text("Screen Timeout & Inactivity")
        ->bold(true)
        ->textSize(20)
        ->textColor(config->colors.on_surface)
        ->build();
    text_col->add_view(title);

    auto subtitle = TextViewBuilder::create()
        ->text("Power-saving and display sleep policies via miquidle")
        ->textSize(12)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    text_col->add_view(subtitle);

    auto badge = TextViewBuilder::create()
        ->text("ext-idle-notify-v1 daemon active ✓")
        ->textSize(11)
        ->textColor(config->colors.primary)
        ->build();
    text_col->add_view(badge);

    row->add_view(text_col);

    
    card->add_view(row);
    return card;
}

std::shared_ptr<View> TimeoutDetailView::create_display_timeout_card() {
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

    auto header = TextViewBuilder::create()
        ->text("TURN OFF DISPLAY AFTER INACTIVITY")
        ->bold(true)
        ->textSize(11)
        ->textColor(config->colors.outline)
        ->build();
    col->add_view(header);

    std::vector<std::string> options = {
        "2 Minutes",
        "5 Minutes",
        "10 Minutes (Recommended)",
        "15 Minutes",
        "Never"
    };

    for (size_t i = 0; i < options.size(); ++i) {
        bool is_sel = (static_cast<int>(i) == m_display_timeout_idx);
        auto btn = ButtonBuilder::create()
            ->text((is_sel ? "●  " : "○  ") + options[i])
            ->textSize(13)
            ->textColor(is_sel ? config->colors.primary : config->colors.on_surface)
            ->backgroundColor(is_sel ? config->colors.surface_variant : config->colors.surface)
            ->cornerRadius(8)
            ->onClick([this, i]() {
                m_display_timeout_idx = static_cast<int>(i);
                setup_ui();
            })
            ->build();
        LayoutParams btn_lp;
        btn_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        btn_lp.height = 40;
        btn->set_layout_params(btn_lp);
        col->add_view(btn);
    }

    card->add_view(col);
    return card;
}

std::shared_ptr<View> TimeoutDetailView::create_suspend_timeout_card() {
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

    auto header = TextViewBuilder::create()
        ->text("SUSPEND COMPUTER AFTER INACTIVITY")
        ->bold(true)
        ->textSize(11)
        ->textColor(config->colors.outline)
        ->build();
    col->add_view(header);

    std::vector<std::string> options = {
        "15 Minutes",
        "30 Minutes (Recommended)",
        "1 Hour",
        "Never"
    };

    for (size_t i = 0; i < options.size(); ++i) {
        bool is_sel = (static_cast<int>(i) == m_suspend_timeout_idx);
        auto btn = ButtonBuilder::create()
            ->text((is_sel ? "●  " : "○  ") + options[i])
            ->textSize(13)
            ->textColor(is_sel ? config->colors.primary : config->colors.on_surface)
            ->backgroundColor(is_sel ? config->colors.surface_variant : config->colors.surface)
            ->cornerRadius(8)
            ->onClick([this, i]() {
                m_suspend_timeout_idx = static_cast<int>(i);
                setup_ui();
            })
            ->build();
        LayoutParams btn_lp;
        btn_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        btn_lp.height = 40;
        btn->set_layout_params(btn_lp);
        col->add_view(btn);
    }

    card->add_view(col);
    return card;
}

std::shared_ptr<View> TimeoutDetailView::create_options_card() {
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

    auto header = TextViewBuilder::create()
        ->text("ADVANCED IDLE OPTIONS")
        ->bold(true)
        ->textSize(11)
        ->textColor(config->colors.outline)
        ->build();
    col->add_view(header);

    // Dim before off row
    auto r1 = std::make_shared<LinearLayout>(Orientation::Horizontal);
    r1->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    r1->set_divider_spacing(12);

    auto t1 = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams t1_lp; t1_lp.width = 0; t1_lp.height = static_cast<int>(LayoutDimension::WrapContent); t1_lp.weight = 1.0f;
    t1->set_layout_params(t1_lp);

    auto lbl1 = TextViewBuilder::create()->text("Dim Display Before Sleep")->bold(true)->textSize(13)->textColor(config->colors.on_surface)->build();
    t1->add_view(lbl1);
    auto sub1 = TextViewBuilder::create()->text("Gradually dim backlight 30 seconds before display powers off")->textSize(11)->textColor(config->colors.on_surface_variant)->build();
    t1->add_view(sub1);
    r1->add_view(t1);

    auto sw1 = SwitchBuilder::create()
        ->checked(m_dim_before_off)
        ->onCheckedChanged([this](bool checked) {
            m_dim_before_off = checked;
            setup_ui();
        })
        ->build();
    LayoutParams sw1_lp; sw1_lp.gravity = Gravity::CenterVertical;
    sw1->set_layout_params(sw1_lp);
    r1->add_view(sw1);
    col->add_view(r1);

    auto div = std::make_shared<DividerView>();
    div->set_color(config->colors.outline_variant);
    div->set_thickness(1);
    col->add_view(div);

    // Respect inhibitors row
    auto r2 = std::make_shared<LinearLayout>(Orientation::Horizontal);
    r2->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    r2->set_divider_spacing(12);

    auto t2 = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams t2_lp; t2_lp.width = 0; t2_lp.height = static_cast<int>(LayoutDimension::WrapContent); t2_lp.weight = 1.0f;
    t2->set_layout_params(t2_lp);

    auto lbl2 = TextViewBuilder::create()->text("Respect Idle Inhibitors")->bold(true)->textSize(13)->textColor(config->colors.on_surface)->build();
    t2->add_view(lbl2);
    auto sub2 = TextViewBuilder::create()->text("Prevent sleep when apps play video (mpv, Firefox) or present slides")->textSize(11)->textColor(config->colors.on_surface_variant)->build();
    t2->add_view(sub2);
    r2->add_view(t2);

    auto sw2 = SwitchBuilder::create()
        ->checked(m_respect_inhibitors)
        ->onCheckedChanged([this](bool checked) {
            m_respect_inhibitors = checked;
            setup_ui();
        })
        ->build();
    LayoutParams sw2_lp; sw2_lp.gravity = Gravity::CenterVertical;
    sw2->set_layout_params(sw2_lp);
    r2->add_view(sw2);
    col->add_view(r2);

    card->add_view(col);
    return card;
}

} // namespace miqu
