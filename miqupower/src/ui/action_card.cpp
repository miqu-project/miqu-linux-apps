#include "ui/action_card.hpp"
#include <miqutoolkit/view/linear_layout.hpp>

namespace miqu {

ActionCard::ActionCard(
    PowerAction action,
    const std::string& title,
    const std::string& icon_name,
    char hotkey,
    const std::string& hotkey_label,
    std::function<void(PowerAction)> on_click
) : m_action(action), m_hotkey(hotkey), m_on_click(std::move(on_click)) {
    auto config = Config::get();

    // 1. Crisp icon
    m_icon_view = ImageViewBuilder::create()
        ->source(icon_name)
        ->targetSize(40)
        ->build();

    // 2. Action Title (with comfortable line height and no vertical truncation)
    m_title_view = TextViewBuilder::create()
        ->text(title)
        ->bold(true)
        ->textSize(13)
        ->textAlignment(TextAlignment::Center)
        ->textColor(config->colors.on_surface)
        ->build();

    // 3. Hotkey keyboard keycap badge
    m_hotkey_view = TextViewBuilder::create()
        ->text(hotkey_label)
        ->bold(true)
        ->textSize(11)
        ->textAlignment(TextAlignment::Center)
        ->textColor(config->colors.on_surface_variant)
        ->build();

    m_badge_card = CardViewBuilder::create()
        ->backgroundColor(config->colors.surface_variant)
        ->stroke(1, config->colors.outline_variant)
        ->cornerRadius(6)
        ->elevation(0)
        ->padding(8, 2, 8, 2)
        ->addView(m_hotkey_view, LayoutParams(static_cast<int>(LayoutDimension::WrapContent), static_cast<int>(LayoutDimension::WrapContent), Gravity::Center))
        ->build();

    // 4. Inner vertical layout with spacious padding
    auto innerLayout = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->gravity(Gravity::Center)
        ->spacing(8)
        ->addView(m_icon_view, LayoutParams(40, 40, Gravity::CenterHorizontal))
        ->addView(m_title_view, LayoutParams(static_cast<int>(LayoutDimension::MatchParent), static_cast<int>(LayoutDimension::WrapContent), Gravity::CenterHorizontal))
        ->addView(m_badge_card, LayoutParams(static_cast<int>(LayoutDimension::WrapContent), 22, Gravity::CenterHorizontal))
        ->build();

    // 5. Outer CardView with breathable padding
    int radius = config->metrics.corner_radius > 0 ? config->metrics.corner_radius : 16;
    m_card = CardViewBuilder::create()
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline_variant)
        ->cornerRadius(radius)
        ->elevation(0)
        ->padding(12, 16, 12, 14)
        ->addView(innerLayout, LayoutParams(static_cast<int>(LayoutDimension::MatchParent), static_cast<int>(LayoutDimension::MatchParent)))
        ->build();

    m_card->set_on_click_listener([this]() {
        if (m_on_click) {
            m_on_click(m_action);
        }
    });

    update_colors();
}

void ActionCard::set_selected(bool selected) {
    if (m_selected == selected) return;
    m_selected = selected;
    update_colors();
    if (m_card) {
        m_card->request_redraw();
    }
}

void ActionCard::update_colors() {
    if (!m_card) return;
    auto config = Config::get();

    if (m_selected) {
        m_card->set_stroke(2, config->colors.primary);
        m_card->set_card_background_color(config->colors.surface_variant);
        if (m_title_view) {
            m_title_view->set_text_color(config->colors.primary);
        }
        if (m_badge_card) {
            m_badge_card->set_card_background_color(config->colors.primary);
            m_badge_card->set_stroke(1, config->colors.primary);
        }
        if (m_hotkey_view) {
            m_hotkey_view->set_text_color(config->colors.on_primary);
        }
    } else {
        m_card->set_stroke(1, config->colors.outline_variant);
        m_card->set_card_background_color(config->colors.surface);
        if (m_title_view) {
            m_title_view->set_text_color(config->colors.on_surface);
        }
        if (m_badge_card) {
            m_badge_card->set_card_background_color(config->colors.surface_variant);
            m_badge_card->set_stroke(1, config->colors.outline_variant);
        }
        if (m_hotkey_view) {
            m_hotkey_view->set_text_color(config->colors.on_surface_variant);
        }
    }
}

} // namespace miqu
