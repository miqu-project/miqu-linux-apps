#include "modules/windowmanager/wm_layout_detail_view.hpp"
#include "modules/windowmanager/wm_backend.hpp"
#include <miqutoolkit/core/config.hpp>
#include <miqutoolkit/view/divider_view.hpp>

namespace miqu {

WmLayoutDetailView::WmLayoutDetailView() {
    WmOptions opts = WmBackend::get_options();
    m_current_layout = opts.layout;
    m_smart_gaps = opts.smart_gaps;

    set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    setup_ui();
}


void WmLayoutDetailView::setup_ui() {
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
    m_content_layout->add_view(create_layout_modes_card());
    m_content_layout->add_view(create_smart_gaps_card());
    m_content_layout->add_view(create_actions_card());

    m_scroll->set_content_view(m_content_layout);
    add_view(m_scroll);
}

std::shared_ptr<View> WmLayoutDetailView::create_hero_card() {
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

    auto icon = std::make_shared<ImageView>("preferences-system-windows");
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
        ->text("Tiling Layout Mode")
        ->bold(true)
        ->textSize(20)
        ->textColor(config->colors.on_surface)
        ->build();
    text_col->add_view(title);

    auto subtitle = TextViewBuilder::create()
        ->text("Active Engine: " + m_current_layout + " (miquland Wayland Compositor)")
        ->textSize(12)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    text_col->add_view(subtitle);

    auto badge = TextViewBuilder::create()
        ->text("Live Compositor IPC Binding ✓")
        ->textSize(11)
        ->textColor(config->colors.primary)
        ->build();
    text_col->add_view(badge);

    row->add_view(text_col);
    card->add_view(row);
    return card;
}

std::shared_ptr<View> WmLayoutDetailView::create_layout_modes_card() {
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

    auto title = TextViewBuilder::create()
        ->text("Select Active Tiling Algorithm")
        ->bold(true)
        ->textSize(15)
        ->textColor(config->colors.on_surface)
        ->build();
    col->add_view(title);

    std::vector<std::pair<std::string, std::string>> layouts = {
        {"dwindle", "Binary Space Partitioning (BSP dynamic spiral)"},
        {"stack", "Master & Stack (Master column on left, stack on right)"},
        {"bstack", "Bottom Stack (Master view on top, stack on bottom)"},
        {"centeredmaster", "Centered Master (Flanked by left & right stacks)"},
        {"spiral", "Fibonacci Spiral (Golden ratio alternating splits)"},
        {"columns", "Equal Vertical Columns"},
        {"grid", "Equal N x M Matrix Grid"},
        {"monocle", "Single Maximized Pane"}
    };

    for (const auto& [mode, desc] : layouts) {
        bool is_selected = (mode == m_current_layout);

        auto btn_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
        btn_row->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));
        btn_row->set_divider_spacing(12);

        auto btn = std::make_shared<Button>();
        btn->set_text(is_selected ? "●  " + mode : "○  " + mode);
        btn->set_style(is_selected ? ButtonStyle::Primary : ButtonStyle::Outlined);
        btn->set_on_click_listener([this, mode]() {
            m_current_layout = mode;
            WmBackend::set_layout(mode);
            setup_ui();
        });
        btn_row->add_view(btn);

        auto desc_view = TextViewBuilder::create()
            ->text(desc)
            ->textSize(12)
            ->textColor(config->colors.outline)
            ->build();
        LayoutParams d_lp;
        d_lp.width = 0;
        d_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        d_lp.weight = 1.0f;
        d_lp.gravity = Gravity::CenterVertical;
        btn_row->add_view(desc_view, d_lp);

        col->add_view(btn_row);
    }

    card->add_view(col);
    return card;
}

std::shared_ptr<View> WmLayoutDetailView::create_smart_gaps_card() {
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

    auto text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams text_lp;
    text_lp.width = 0;
    text_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    text_lp.weight = 1.0f;
    text_col->set_layout_params(text_lp);
    text_col->set_divider_spacing(3);

    auto title = TextViewBuilder::create()
        ->text("Smart Window Gaps")
        ->bold(true)
        ->textSize(15)
        ->textColor(config->colors.on_surface)
        ->build();
    text_col->add_view(title);

    auto desc = TextViewBuilder::create()
        ->text("Automatically remove gaps and border padding when only a single window is open")
        ->textSize(12)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    text_col->add_view(desc);

    row->add_view(text_col);

    auto toggle_btn = std::make_shared<Button>();
    toggle_btn->set_text(m_smart_gaps ? "Enabled" : "Disabled");
    toggle_btn->set_style(m_smart_gaps ? ButtonStyle::Primary : ButtonStyle::Standard);
    toggle_btn->set_on_click_listener([this]() {
        m_smart_gaps = !m_smart_gaps;
        WmBackend::set_smart_gaps(m_smart_gaps);
        setup_ui();
    });
    LayoutParams btn_lp;
    btn_lp.gravity = Gravity::CenterVertical;
    row->add_view(toggle_btn, btn_lp);

    card->add_view(row);
    return card;
}

std::shared_ptr<View> WmLayoutDetailView::create_actions_card() {
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

    auto title = TextViewBuilder::create()
        ->text("Tile Splitting Actions")
        ->bold(true)
        ->textSize(15)
        ->textColor(config->colors.on_surface)
        ->build();
    col->add_view(title);

    auto btn = std::make_shared<Button>();
    btn->set_text("Toggle Split Orientation (Horizontal / Vertical)");
    btn->set_style(ButtonStyle::Outlined);
    btn->set_on_click_listener([]() {
        WmBackend::toggle_split();
    });
    col->add_view(btn);

    card->add_view(col);
    return card;
}

} // namespace miqu
