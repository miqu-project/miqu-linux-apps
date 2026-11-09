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
