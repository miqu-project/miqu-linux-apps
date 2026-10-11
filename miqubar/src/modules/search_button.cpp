#include "search_button.hpp"
#include "../config/bar_config.hpp"
#include "miqutoolkit/core/config.hpp"
#include "miqutoolkit/system/app_manager.hpp"

namespace miqubar {

SearchButtonView::SearchButtonView() {
    sync_style();

    set_on_click_listener([this]() {
        launch_search();
    });
}

void SearchButtonView::sync_style() {
    const auto& cfg = BarConfig::get();
    auto config = miqu::Config::get();

    set_icon("system-search");
    int icon_sz = std::max(14, cfg.icon_size - 8);
    set_icon_size(icon_sz);

    int fs = config->metrics.font_size > 0 ? config->metrics.font_size : 11;
    set_text_size(fs);
    set_radius(config->metrics.corner_radius);

    if (cfg.search_style == "icon") {
        set_text("");
        set_flat(true);
        set_padding(6, 4);
        set_layout_params(miqu::LayoutParams(
            static_cast<int>(miqu::LayoutDimension::WrapContent),
            static_cast<int>(miqu::LayoutDimension::WrapContent)
        ));
    } else {
        // "pill" style: modern capsule with subtle outline and soft placeholder
        set_text(cfg.search_placeholder);
        set_style(miqu::ButtonStyle::Standard);

        int pill_h = std::max(26, cfg.height - 14);
        set_radius(pill_h / 2);

        // Soft, non-prominent text color
        set_text_color(config->colors.on_surface_variant);

        // Soft translucent pill background
        set_background_color(config->colors.surface_variant.with_alpha(0.40f));

        // Delicate, subtle outline
        set_border_color(config->colors.outline.with_alpha(0.20f));
        set_border_width(1.0);

        set_content_gravity(miqu::Gravity::Left | miqu::Gravity::CenterVertical);
        set_padding(12, 4);

        set_layout_params(miqu::LayoutParams(
            cfg.search_width,
            pill_h
        ));
    }

    request_redraw();
}

void SearchButtonView::launch_search() {
    const auto& cmd = BarConfig::get().search_cmd;
    if (cmd.empty()) return;

    miqu::AppManager::launch_command(cmd);
}

} // namespace miqubar
