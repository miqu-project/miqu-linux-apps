#pragma once

#include "settings_model.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace miqu {

inline int get_card_radius(const std::shared_ptr<Config>& config = nullptr) {
    auto cfg = config ? config : Config::get();
    return cfg ? cfg->metrics.corner_radius : 12;
}

inline int get_inner_radius(const std::shared_ptr<Config>& config = nullptr) {
    auto cfg = config ? config : Config::get();
    return cfg ? std::max(0, cfg->metrics.corner_radius - 4) : 8;
}

inline int get_btn_radius(const std::shared_ptr<Config>& config = nullptr) {
    auto cfg = config ? config : Config::get();
    return cfg ? std::max(0, cfg->metrics.corner_radius - 6) : 6;
}

inline int get_chip_radius(const std::shared_ptr<Config>& config = nullptr) {
    auto cfg = config ? config : Config::get();
    return cfg ? std::max(0, cfg->metrics.corner_radius / 3) : 4;
}

class HoverCardView : public CardView {
public:
    HoverCardView() {
        update_visual_state();
    }
    ~HoverCardView() override = default;

    void set_selected(bool sel, const Color& bg, int stroke_w = 0, const Color& stroke_col = Color::transparent()) {
        m_selected = sel;
        m_selected_bg = bg;
        m_selected_stroke_w = stroke_w;
        m_selected_stroke_col = stroke_col;
        update_visual_state();
    }

    void set_default_background(const Color& bg) {
        m_default_bg = bg;
        update_visual_state();
    }

    bool on_mouse_move(int lx, int ly, const Rect& bounds) override {
        bool was_hovered = m_hovered;
        m_hovered = bounds.contains(Point(lx, ly));
        if (m_hovered != was_hovered) {
            update_visual_state();
            request_redraw();
        }
        CardView::on_mouse_move(lx, ly, bounds);
        return (m_hovered != was_hovered) || m_hovered;
    }

private:
    void update_visual_state() {
        auto config = Config::get();
        if (m_selected) {
            set_card_background_color(m_selected_bg);
            set_stroke(m_selected_stroke_w, m_selected_stroke_col);
            set_style(m_selected_stroke_w > 0 ? CardStyle::Outlined : CardStyle::Filled);
        } else if (m_hovered) {
            Color hover_col = config ? config->colors.on_surface.with_alpha(0.08f) : Color(1, 1, 1, 0.08f);
            set_card_background_color(hover_col);
            set_stroke(0, Color::transparent());
            set_style(CardStyle::Filled);
        } else {
            set_card_background_color(m_default_bg);
            set_stroke(0, Color::transparent());
            set_style(CardStyle::Filled);
        }
    }

    bool m_selected = false;
    bool m_hovered = false;
    Color m_default_bg = Color::transparent();
    Color m_selected_bg = Color::transparent();
    int m_selected_stroke_w = 0;
    Color m_selected_stroke_col = Color::transparent();
};

class HoverCardViewBuilder : public std::enable_shared_from_this<HoverCardViewBuilder> {
public:
    HoverCardViewBuilder() : m_view(std::make_shared<HoverCardView>()) {}

    static std::shared_ptr<HoverCardViewBuilder> create() {
        return std::make_shared<HoverCardViewBuilder>();
    }

    std::shared_ptr<HoverCardViewBuilder> cornerRadius(int radius) {
        m_view->set_radius(radius);
        return shared_from_this();
    }

    std::shared_ptr<HoverCardViewBuilder> elevation(double elev) {
        m_view->set_elevation(elev);
        return shared_from_this();
    }

    std::shared_ptr<HoverCardViewBuilder> defaultBackground(const Color& col) {
        m_view->set_default_background(col);
        return shared_from_this();
    }

    std::shared_ptr<HoverCardViewBuilder> selected(bool sel, const Color& bg, int stroke_w = 0, const Color& stroke_col = Color::transparent()) {
        m_view->set_selected(sel, bg, stroke_w, stroke_col);
        return shared_from_this();
    }

    std::shared_ptr<HoverCardViewBuilder> addView(std::shared_ptr<View> child) {
        m_view->add_view(std::move(child));
        return shared_from_this();
    }

    std::shared_ptr<HoverCardView> build() {
        return m_view;
    }

private:
    std::shared_ptr<HoverCardView> m_view;
};

inline std::shared_ptr<View> create_telemetry_card(
    const std::vector<TelemetryEntry>& telemetry,
    const std::shared_ptr<Config>& config,
    const std::string& card_title = "Technical specifications"
) {
    auto telem_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(8)
        ->padding(16, 14)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto telem_title = TextViewBuilder::create()
        ->text(card_title.empty() ? "Technical specifications" : card_title)
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    telem_card->add_view(telem_title);

    for (const auto& entry : telemetry) {
        auto row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->padding(2, 2)
            ->build();

        auto k = TextViewBuilder::create()
            ->text(entry.key)
            ->caption(true)
            ->textColor(config->colors.on_surface_variant)
            ->build();
        LayoutParams k_lp;
        k_lp.width = 140;
        row->add_view(k, k_lp);

        auto v = TextViewBuilder::create()
            ->text(entry.value)
            ->caption(true)
            ->textColor(config->colors.on_surface)
            ->build();
        LayoutParams v_lp;
        v_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        v_lp.weight = 1.0f;
        row->add_view(v, v_lp);

        telem_card->add_view(row);
    }
    LayoutParams card_lp;
    card_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    card_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    telem_card->set_layout_params(card_lp);
    return telem_card;
}

} // namespace miqu
