#include "modules/display/screen_lock_detail_view.hpp"
#include "modules/display/display_backend.hpp"
#include <miqutoolkit/core/config.hpp>
#include <miqutoolkit/view/divider_view.hpp>
#include <cstdlib>

namespace miqu {

ScreenLockDetailView::ScreenLockDetailView() {
    set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    setup_ui();
}

void ScreenLockDetailView::set_is_fullscreen(bool fullscreen) {
    if (m_is_fullscreen != fullscreen) {
        m_is_fullscreen = fullscreen;
        setup_ui();
    }
}

void ScreenLockDetailView::setup_ui() {
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
    m_content_layout->add_view(create_lock_triggers_card());
    m_content_layout->add_view(create_grace_period_card());
    m_content_layout->add_view(create_appearance_card());
    m_content_layout->add_view(create_actions_card());

    m_scroll->set_content_view(m_content_layout);
    add_view(m_scroll);
}

std::shared_ptr<View> ScreenLockDetailView::create_hero_card() {
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

    auto icon = std::make_shared<ImageView>("system-lock-screen");
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
        ->text("Screen Lock")
        ->bold(true)
        ->textSize(m_is_fullscreen ? 20 : 17)
        ->textColor(config->colors.on_surface)
        ->build();
    text_col->add_view(title);

    auto subtitle = TextViewBuilder::create()
        ->text("Session security and authentication via miqulock")
        ->textSize(12)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    text_col->add_view(subtitle);

    auto badge = TextViewBuilder::create()
        ->text("ext-session-lock-v1 protocol active ✓")
        ->textSize(11)
        ->textColor(config->colors.primary)
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

std::shared_ptr<View> ScreenLockDetailView::create_lock_triggers_card() {
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
        ->text("LOCK TRIGGERS & BEHAVIOR")
        ->bold(true)
        ->textSize(11)
        ->textColor(config->colors.outline)
        ->build();
    col->add_view(header);

    // Row 1: Lock before sleep
    auto r1 = std::make_shared<LinearLayout>(Orientation::Horizontal);
    r1->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    r1->set_divider_spacing(12);

    auto t1 = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams t1_lp; t1_lp.width = 0; t1_lp.height = static_cast<int>(LayoutDimension::WrapContent); t1_lp.weight = 1.0f;
    t1->set_layout_params(t1_lp);

    auto lbl1 = TextViewBuilder::create()->text("Lock Screen Before Suspend")->bold(true)->textSize(13)->textColor(config->colors.on_surface)->build();
    t1->add_view(lbl1);
    auto sub1 = TextViewBuilder::create()->text("Present lock screen on all displays before system sleeps (miqulock -f)")->textSize(11)->textColor(config->colors.on_surface_variant)->build();
    t1->add_view(sub1);
    r1->add_view(t1);

    auto sw1 = SwitchBuilder::create()
        ->checked(m_lock_on_sleep)
        ->onCheckedChanged([this](bool checked) {
            m_lock_on_sleep = checked;
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

    // Row 2: Lock when display turns off
    auto r2 = std::make_shared<LinearLayout>(Orientation::Horizontal);
    r2->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    r2->set_divider_spacing(12);

    auto t2 = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams t2_lp; t2_lp.width = 0; t2_lp.height = static_cast<int>(LayoutDimension::WrapContent); t2_lp.weight = 1.0f;
    t2->set_layout_params(t2_lp);

    auto lbl2 = TextViewBuilder::create()->text("Lock Screen on Display Sleep")->bold(true)->textSize(13)->textColor(config->colors.on_surface)->build();
    t2->add_view(lbl2);
    auto sub2 = TextViewBuilder::create()->text("Immediately engage session lock when monitors turn off due to idle")->textSize(11)->textColor(config->colors.on_surface_variant)->build();
    t2->add_view(sub2);
    r2->add_view(t2);

    auto sw2 = SwitchBuilder::create()
        ->checked(m_lock_on_display_off)
        ->onCheckedChanged([this](bool checked) {
            m_lock_on_display_off = checked;
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

std::shared_ptr<View> ScreenLockDetailView::create_grace_period_card() {
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
        ->text("REQUIRE PASSWORD AFTER LOCK")
        ->bold(true)
        ->textSize(11)
        ->textColor(config->colors.outline)
        ->build();
    col->add_view(header);

    std::vector<std::string> options = {
        "Immediately (Most Secure)",
        "After 15 seconds (Grace Period)",
        "After 1 minute",
        "After 5 minutes"
    };

    for (size_t i = 0; i < options.size(); ++i) {
        bool is_sel = (static_cast<int>(i) == m_grace_period_idx);
        auto btn = ButtonBuilder::create()
            ->text((is_sel ? "●  " : "○  ") + options[i])
            ->textSize(13)
            ->textColor(is_sel ? config->colors.primary : config->colors.on_surface)
            ->backgroundColor(is_sel ? config->colors.surface_variant : config->colors.surface)
            ->cornerRadius(8)
            ->onClick([this, i]() {
                m_grace_period_idx = static_cast<int>(i);
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

std::shared_ptr<View> ScreenLockDetailView::create_appearance_card() {
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

    auto header = TextViewBuilder::create()
        ->text("LOCK SCREEN APPEARANCE (miqulock)")
        ->bold(true)
        ->textSize(11)
        ->textColor(config->colors.outline)
        ->build();
    col->add_view(header);

    auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    row->set_divider_spacing(12);

    auto t = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams t_lp; t_lp.width = 0; t_lp.height = static_cast<int>(LayoutDimension::WrapContent); t_lp.weight = 1.0f;
    t->set_layout_params(t_lp);

    auto lbl = TextViewBuilder::create()->text("Show Power Actions on Lock Screen")->bold(true)->textSize(13)->textColor(config->colors.on_surface)->build();
    t->add_view(lbl);
    auto sub = TextViewBuilder::create()->text("Display Shut Down, Restart, and Suspend buttons on lock screen")->textSize(11)->textColor(config->colors.on_surface_variant)->build();
    t->add_view(sub);
    row->add_view(t);

    auto sw = SwitchBuilder::create()
        ->checked(m_show_power_buttons)
        ->onCheckedChanged([this](bool checked) {
            m_show_power_buttons = checked;
            setup_ui();
        })
        ->build();
    LayoutParams sw_lp; sw_lp.gravity = Gravity::CenterVertical;
    sw->set_layout_params(sw_lp);
    row->add_view(sw);
    col->add_view(row);

    card->add_view(col);
    return card;
}

std::shared_ptr<View> ScreenLockDetailView::create_actions_card() {
    auto config = Config::get();
    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_radius(config->metrics.corner_radius);
    card->set_padding(14);
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

    auto btn_lock = ButtonBuilder::create()
        ->text("🔒  Lock Screen Now")
        ->textSize(13)
        ->textColor(config->colors.on_primary)
        ->backgroundColor(config->colors.primary)
        ->cornerRadius(6)
        ->onClick([]() {
            DisplayBackend::lock_screen_now();
        })
        ->build();
    LayoutParams bl_lp; bl_lp.width = static_cast<int>(LayoutDimension::MatchParent); bl_lp.height = 42;
    btn_lock->set_layout_params(bl_lp);
    row->add_view(btn_lock);

    card->add_view(row);
    return card;
}

} // namespace miqu
