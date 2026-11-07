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
    m_info = MonitorBackend::get_monitor(name);
    m_subtitle = subtitle.empty() ? m_info.description : subtitle;
    m_is_primary = m_info.is_primary;

    if (m_info.current_width > 0 && m_info.current_height > 0) {
        m_current_res = std::to_string(m_info.current_width) + "x" + std::to_string(m_info.current_height);
    } else {
        m_current_res = "1920x1080";
    }

    int hz_int = static_cast<int>(m_info.current_refresh + 0.5f);
    m_current_hz = (hz_int > 0 ? std::to_string(hz_int) : "60") + " Hz";

    int sc_int = static_cast<int>(m_info.scale * 100.0f + 0.5f);
    m_current_scale = (sc_int > 0 ? std::to_string(sc_int) : "100") + "%";

    if (m_info.transform == "90") m_current_orient = "Portrait (90°)";
    else if (m_info.transform == "180") m_current_orient = "Inverted Landscape (180°)";
    else if (m_info.transform == "270") m_current_orient = "Inverted Portrait (270°)";
    else m_current_orient = "Landscape (Standard)";

    m_applied_feedback = false;
    setup_ui();
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
    m_content_layout->set_padding(32);

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
        ->text(m_selected_monitor)
        ->bold(true)
        ->textSize(20)
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

    std::vector<std::string> resolutions;
    std::vector<std::string> seen_res;
    for (const auto& mode : m_info.modes) {
        std::string res_key = std::to_string(mode.width) + "x" + std::to_string(mode.height);
        if (std::find(seen_res.begin(), seen_res.end(), res_key) == seen_res.end()) {
            seen_res.push_back(res_key);
            std::string label = res_key;
            if (mode.is_preferred) label += " (Recommended)";
            resolutions.push_back(label);
        }
    }
    if (resolutions.empty()) {
        resolutions = {"1920x1080 (Recommended)", "1600x900", "1280x720"};
    }

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
                m_applied_feedback = false;
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

    std::vector<std::string> rates;
    int sel_w = 1920, sel_h = 1080;
    size_t x_pos = m_current_res.find('x');
    if (x_pos != std::string::npos) {
        try {
            sel_w = std::stoi(m_current_res.substr(0, x_pos));
            sel_h = std::stoi(m_current_res.substr(x_pos + 1));
        } catch (...) {}
    }

    for (const auto& mode : m_info.modes) {
        if (mode.width == sel_w && mode.height == sel_h) {
            int hz_val = static_cast<int>(mode.refresh_rate + 0.5f);
            std::string hz_str = std::to_string(hz_val) + " Hz";
            if (std::find(rates.begin(), rates.end(), hz_str) == rates.end()) {
                rates.push_back(hz_str);
            }
        }
    }
    if (rates.empty()) {
        rates = {"60 Hz", "75 Hz", "120 Hz", "144 Hz"};
    }

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
                m_applied_feedback = false;
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
        ->text(m_applied_feedback ? "✓  Settings Applied Successfully" : "✓  Apply Monitor Settings")
        ->textSize(13)
        ->textColor(config->colors.on_primary)
        ->backgroundColor(m_applied_feedback ? config->colors.outline : config->colors.primary)
        ->cornerRadius(6)
        ->onClick([this]() {
            int w = 1920, h = 1080;
            size_t x_pos = m_current_res.find('x');
            if (x_pos != std::string::npos) {
                try {
                    w = std::stoi(m_current_res.substr(0, x_pos));
                    h = std::stoi(m_current_res.substr(x_pos + 1));
                } catch (...) {}
            }

            float hz = 60.0f;
            try {
                hz = std::stof(m_current_hz.substr(0, m_current_hz.find(' ')));
            } catch (...) {}

            float sc = 1.0f;
            if (m_current_scale == "125%") sc = 1.25f;
            else if (m_current_scale == "150%") sc = 1.50f;
            else if (m_current_scale == "200%") sc = 2.00f;
            else sc = 1.0f;

            std::string tr = "normal";
            if (m_current_orient.find("90") != std::string::npos) tr = "90";
            else if (m_current_orient.find("180") != std::string::npos) tr = "180";
            else if (m_current_orient.find("270") != std::string::npos) tr = "270";

            MonitorBackend::apply_settings(m_selected_monitor, w, h, hz, sc, tr);
            m_applied_feedback = true;
            setup_ui();
        })
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
