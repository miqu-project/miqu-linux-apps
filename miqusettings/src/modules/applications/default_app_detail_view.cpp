#include "modules/applications/default_app_detail_view.hpp"
#include "modules/applications/apps_backend.hpp"
#include <miqutoolkit/core/config.hpp>
#include <miqutoolkit/view/divider_view.hpp>
#include <algorithm>

namespace miqu {

DefaultAppDetailView::DefaultAppDetailView() {
    set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
}

void DefaultAppDetailView::set_role_id(const std::string& role_id) {
    m_role_id = role_id;
    // Do not call load_data/setup_ui here — caller sets is_fullscreen then calls refresh()
}

void DefaultAppDetailView::set_is_fullscreen(bool fullscreen) {
    m_is_fullscreen = fullscreen;
    load_data();
    setup_ui();
}

void DefaultAppDetailView::load_data() {
    if (m_role_id.empty()) return;

    if (m_is_fullscreen) {
        // Full load: query current default + find candidates (runs xdg-mime shell commands)
        m_role = AppsBackend::get_default_role(m_role_id);
    } else {
        // Preview mode: use cached roles (populated on first fullscreen load or background init).
        // Falls back to full load only if no cache exists yet.
        auto roles = AppsBackend::get_default_roles();
        for (const auto& r : roles) {
            if (r.role_id == m_role_id) {
                m_role = r;
                return;
            }
        }
        // Cache not ready yet — do a full load so at least something shows
        m_role = AppsBackend::get_default_role(m_role_id);
    }
}

void DefaultAppDetailView::setup_ui() {
    clear_views();
    auto config = Config::get();

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
    m_content_layout->add_view(create_current_default_card());
    m_content_layout->add_view(create_candidates_card());

    m_scroll->set_content_view(m_content_layout);
    add_view(m_scroll);
}

std::shared_ptr<View> DefaultAppDetailView::create_hero_card() {
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

    auto icon = std::make_shared<ImageView>(m_role.icon_name.empty() ? "preferences-desktop-default-applications" : m_role.icon_name);
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
        ->text("Default " + m_role.title)
        ->bold(true)
        ->textSize(m_is_fullscreen ? 20 : 17)
        ->textColor(config->colors.on_surface)
        ->build();
    text_col->add_view(title);

    if (!m_role.description.empty()) {
        auto subtitle = TextViewBuilder::create()
            ->text(m_role.description)
            ->textSize(12)
            ->textColor(config->colors.on_surface_variant)
            ->build();
        text_col->add_view(subtitle);
    }

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

std::shared_ptr<View> DefaultAppDetailView::create_current_default_card() {
    auto config = Config::get();
    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_radius(config->metrics.corner_radius);
    card->set_padding(16);
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
        ->text("CURRENT DEFAULT")
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
    row->set_divider_spacing(14);

    std::string icon_name = !m_role.current_default_icon.empty() ? m_role.current_default_icon : "preferences-system";
    auto icon = std::make_shared<ImageView>(icon_name);
    icon->set_target_size(36);
    LayoutParams icon_lp;
    icon_lp.width = 36;
    icon_lp.height = 36;
    icon_lp.gravity = Gravity::CenterVertical;
    row->add_view(icon, icon_lp);

    auto text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams text_lp;
    text_lp.width = 0;
    text_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    text_lp.weight = 1.0f;
    text_col->set_layout_params(text_lp);

    std::string name_str = !m_role.current_default_name.empty() ? m_role.current_default_name : "None Configured";
    auto app_title = TextViewBuilder::create()
        ->text(name_str)
        ->bold(true)
        ->textSize(14)
        ->textColor(config->colors.on_surface)
        ->build();
    text_col->add_view(app_title);

    std::string id_str = !m_role.current_default_id.empty() ? m_role.current_default_id : "No standard handler registered";
    auto app_id = TextViewBuilder::create()
        ->text(id_str)
        ->textSize(11)
        ->textColor(config->colors.outline)
        ->build();
    text_col->add_view(app_id);

    row->add_view(text_col);

    if (!m_role.current_default_id.empty()) {
        auto badge = TextViewBuilder::create()
            ->text("Active ✓")
            ->bold(true)
            ->textSize(12)
            ->textColor(config->colors.primary)
            ->build();
        LayoutParams badge_lp;
        badge_lp.gravity = Gravity::CenterVertical;
        badge->set_layout_params(badge_lp);
        row->add_view(badge);
    }

    col->add_view(row);
    card->add_view(col);
    return card;
}

std::shared_ptr<View> DefaultAppDetailView::create_candidate_row(const AppDetailInfo& app, bool is_current) {
    auto config = Config::get();
    auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    row->set_divider_spacing(14);
    row->set_padding(8);

    auto icon = std::make_shared<ImageView>(app.icon_name.empty() ? "application-x-executable" : app.icon_name);
    icon->set_target_size(32);
    LayoutParams icon_lp;
    icon_lp.width = 32;
    icon_lp.height = 32;
    icon_lp.gravity = Gravity::CenterVertical;
    row->add_view(icon, icon_lp);

    auto text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams text_lp;
    text_lp.width = 0;
    text_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    text_lp.weight = 1.0f;
    text_col->set_layout_params(text_lp);

    auto title = TextViewBuilder::create()
        ->text(app.name)
        ->bold(is_current)
        ->textSize(13)
        ->textColor(config->colors.on_surface)
        ->build();
    text_col->add_view(title);

    std::string sub = !app.generic_name.empty() ? app.generic_name : app.id;
    auto subtitle = TextViewBuilder::create()
        ->text(sub)
        ->textSize(11)
        ->textColor(config->colors.outline)
        ->build();
    text_col->add_view(subtitle);

    row->add_view(text_col);

    if (is_current) {
        auto current_tag = TextViewBuilder::create()
            ->text("✓ Default")
            ->bold(true)
            ->textSize(12)
            ->textColor(config->colors.primary)
            ->build();
        LayoutParams tag_lp;
        tag_lp.gravity = Gravity::CenterVertical;
        current_tag->set_layout_params(tag_lp);
        row->add_view(current_tag);
    } else {
        auto btn_set = ButtonBuilder::create()
            ->text("Set Default")
            ->textSize(11)
            ->textColor(config->colors.primary)
            ->backgroundColor(config->colors.surface_variant)
            ->cornerRadius(6)
            ->onClick([this, app_id = app.id]() {
                select_candidate(app_id);
            })
            ->build();
        LayoutParams btn_lp;
        btn_lp.width = 96;
        btn_lp.height = 32;
        btn_lp.gravity = Gravity::CenterVertical;
        btn_set->set_layout_params(btn_lp);
        row->add_view(btn_set);
    }

    return row;
}

std::shared_ptr<View> DefaultAppDetailView::create_candidates_card() {
    auto config = Config::get();
    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_radius(config->metrics.corner_radius);
    card->set_padding(16);
    card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
    col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    col->set_divider_spacing(8);

    auto title = TextViewBuilder::create()
        ->text("AVAILABLE APPLICATIONS")
        ->bold(true)
        ->textSize(11)
        ->textColor(config->colors.outline)
        ->build();
    col->add_view(title);

    auto div = std::make_shared<DividerView>();
    div->set_color(config->colors.outline_variant);
    div->set_thickness(1);
    col->add_view(div);

    if (m_role.candidates.empty()) {
        auto empty_txt = TextViewBuilder::create()
            ->text("No alternate applications found for this role.")
            ->textSize(12)
            ->textColor(config->colors.outline)
            ->build();
        col->add_view(empty_txt);
    } else {
        for (const auto& cand : m_role.candidates) {
            bool is_curr = (cand.id == m_role.current_default_id);
            col->add_view(create_candidate_row(cand, is_curr));
        }
    }

    card->add_view(col);
    return card;
}

void DefaultAppDetailView::select_candidate(const std::string& desktop_id) {
    bool ok = AppsBackend::set_default_for_role(m_role_id, desktop_id);
    if (ok) {
        load_data();
        setup_ui();
        if (m_on_default_changed) {
            m_on_default_changed(m_role_id, m_role.current_default_name);
        }
    }
}

} // namespace miqu
