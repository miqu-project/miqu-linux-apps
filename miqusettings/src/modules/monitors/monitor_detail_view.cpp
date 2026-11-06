#include "modules/monitors/monitor_detail_view.hpp"
#include <miqutoolkit/core/config.hpp>
#include <miqutoolkit/view/divider_view.hpp>

namespace miqu {

MonitorDetailView::MonitorDetailView() {
    set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    setup_ui();
}

void MonitorDetailView::set_monitor_info(const std::string& name, const std::string& subtitle) {
    m_selected_monitor = name;
    m_subtitle = subtitle;
    m_is_primary = (subtitle.find("Primary") != std::string::npos);

    if (name.find("HDMI") != std::string::npos) {
        m_current_res = "2560x1440";
        m_current_hz = "144 Hz";
    } else {
        m_current_res = "1920x1080";
        m_current_hz = "60 Hz";
    }
    setup_ui();
}

void MonitorDetailView::set_is_fullscreen(bool fullscreen) {
    if (m_is_fullscreen != fullscreen) {
        m_is_fullscreen = fullscreen;
        setup_ui();
    }
}

void MonitorDetailView::setup_ui() {
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
    m_content_layout->add_view(create_primary_card());
    m_content_layout->add_view(create_resolution_card());
    m_content_layout->add_view(create_scaling_card());
    m_content_layout->add_view(create_orientation_card());
    m_content_layout->add_view(create_actions_card());

    m_scroll->set_content_view(m_content_layout);
    add_view(m_scroll);
}

std::shared_ptr<View> MonitorDetailView::create_hero_card() {
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

    auto icon = std::make_shared<ImageView>("video-display");
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
        ->text(m_selected_monitor)
        ->bold(true)
        ->textSize(m_is_fullscreen ? 20 : 17)
        ->textColor(config->colors.on_surface)
        ->build();
    text_col->add_view(title);

    auto subtitle = TextViewBuilder::create()
        ->text(m_current_res + " @ " + m_current_hz + " · " + m_current_scale + " Scale")
        ->textSize(12)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    text_col->add_view(subtitle);

    auto badge = TextViewBuilder::create()
        ->text(m_is_primary ? "Primary Display ✓" : "Connected Display")
        ->textSize(11)
        ->textColor(m_is_primary ? config->colors.primary : config->colors.outline)
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

std::shared_ptr<View> MonitorDetailView::create_primary_card() {
    auto config = Config::get();
    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_radius(config->metrics.corner_radius);
    card->set_padding(16);
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

    auto title = TextViewBuilder::create()
        ->text("Set as Primary Display")
        ->bold(true)
        ->textSize(14)
        ->textColor(config->colors.on_surface)
        ->build();
    text_col->add_view(title);

    auto sub = TextViewBuilder::create()
        ->text("System bar and new windows will appear on this display by default")
        ->textSize(12)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    text_col->add_view(sub);
    row->add_view(text_col);

    auto sw = SwitchBuilder::create()
        ->checked(m_is_primary)
        ->onCheckedChanged([this](bool checked) {
            m_is_primary = checked;
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

std::shared_ptr<View> MonitorDetailView::create_resolution_card() {
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
        ->text("RESOLUTION & REFRESH RATE")
        ->bold(true)
        ->textSize(11)
        ->textColor(config->colors.outline)
        ->build();
    col->add_view(title);

    std::vector<std::string> resolutions = {
        "1920x1080 (16:9 · Recommended)",
        "2560x1440 (16:9 · 2K QHD)",
        "1680x1050 (16:10)",
        "1280x720 (16:9 · HD)"
    };

    for (size_t i = 0; i < resolutions.size(); ++i) {
        std::string res_clean = resolutions[i].substr(0, resolutions[i].find(' '));
        bool is_selected = (res_clean == m_current_res);
        auto opt_btn = ButtonBuilder::create()
            ->text((is_selected ? "●  " : "○  ") + resolutions[i])
            ->textSize(13)
            ->textColor(is_selected ? config->colors.primary : config->colors.on_surface)
            ->backgroundColor(is_selected ? config->colors.surface_variant : config->colors.surface)
            ->cornerRadius(8)
            ->onClick([this, res_clean]() {
                m_current_res = res_clean;
                setup_ui();
            })
            ->build();
        LayoutParams opt_lp;
        opt_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        opt_lp.height = 42;
        opt_btn->set_layout_params(opt_lp);
        col->add_view(opt_btn);
    }

    auto div = std::make_shared<DividerView>();
    div->set_color(config->colors.outline_variant);
    div->set_thickness(1);
    col->add_view(div);

    auto hz_title = TextViewBuilder::create()
        ->text("Refresh Rate")
        ->bold(true)
        ->textSize(13)
        ->textColor(config->colors.on_surface)
        ->build();
    col->add_view(hz_title);

    auto hz_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    hz_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    hz_row->set_divider_spacing(8);

    std::vector<std::string> rates = {"60 Hz", "75 Hz", "120 Hz", "144 Hz"};
    for (const auto& hz : rates) {
        bool is_sel = (hz == m_current_hz);
        auto btn = ButtonBuilder::create()
            ->text(hz)
            ->textSize(12)
            ->textColor(is_sel ? config->colors.on_primary : config->colors.on_surface)
            ->backgroundColor(is_sel ? config->colors.primary : config->colors.surface_variant)
            ->cornerRadius(8)
            ->onClick([this, hz]() {
                m_current_hz = hz;
                setup_ui();
            })
            ->build();
        LayoutParams btn_lp;
        btn_lp.width = 0;
        btn_lp.height = 36;
        btn_lp.weight = 1.0f;
        btn->set_layout_params(btn_lp);
        hz_row->add_view(btn);
    }
    col->add_view(hz_row);

    card->add_view(col);
    return card;
}

std::shared_ptr<View> MonitorDetailView::create_scaling_card() {
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
        ->text("DISPLAY SCALING")
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

    std::vector<std::string> scales = {"100%", "125%", "150%", "200%"};
    for (const auto& s : scales) {
        bool is_sel = (s == m_current_scale);
        auto btn = ButtonBuilder::create()
            ->text(s)
            ->textSize(13)
            ->textColor(is_sel ? config->colors.on_primary : config->colors.on_surface)
            ->backgroundColor(is_sel ? config->colors.primary : config->colors.surface_variant)
            ->cornerRadius(8)
            ->onClick([this, s]() {
                m_current_scale = s;
                setup_ui();
            })
            ->build();
        LayoutParams btn_lp;
        btn_lp.width = 0;
        btn_lp.height = 38;
        btn_lp.weight = 1.0f;
        btn->set_layout_params(btn_lp);
        row->add_view(btn);
    }

    col->add_view(row);
    card->add_view(col);
    return card;
}

std::shared_ptr<View> MonitorDetailView::create_orientation_card() {
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
        ->text("ORIENTATION")
        ->bold(true)
        ->textSize(11)
        ->textColor(config->colors.outline)
        ->build();
    col->add_view(title);

    std::vector<std::string> orients = {"Landscape (Standard)", "Portrait (90°)", "Inverted Landscape (180°)", "Inverted Portrait (270°)"};
    for (size_t i = 0; i < orients.size(); ++i) {
        bool is_sel = (orients[i] == m_current_orient);
        auto btn = ButtonBuilder::create()
            ->text((is_sel ? "●  " : "○  ") + orients[i])
            ->textSize(13)
            ->textColor(is_sel ? config->colors.primary : config->colors.on_surface)
            ->backgroundColor(is_sel ? config->colors.surface_variant : config->colors.surface)
            ->cornerRadius(8)
            ->onClick([this, o = orients[i]]() {
                m_current_orient = o;
                setup_ui();
            })
            ->build();
        LayoutParams btn_lp;
        btn_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        btn_lp.height = 42;
        btn->set_layout_params(btn_lp);
        col->add_view(btn);
    }

    card->add_view(col);
    return card;
}

std::shared_ptr<View> MonitorDetailView::create_actions_card() {
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

    auto btn_apply = ButtonBuilder::create()
        ->text("✓  Apply Monitor Settings")
        ->textSize(13)
        ->textColor(config->colors.on_primary)
        ->backgroundColor(config->colors.primary)
        ->cornerRadius(6)
        ->build();
    LayoutParams app_lp;
    app_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    app_lp.height = 42;
    btn_apply->set_layout_params(app_lp);
    row->add_view(btn_apply);

    card->add_view(row);
    return card;
}

} // namespace miqu
