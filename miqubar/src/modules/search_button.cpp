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

    int item_h = std::max(28, cfg.height - 8);
    if (cfg.search_style == "icon") {
        set_text("");
        set_flat(true);
        set_padding(6, 4);
        set_layout_params(miqu::LayoutParams(item_h, item_h));
    } else {
        // "pill" style: modern capsule with subtle outline and soft placeholder
        set_text(cfg.search_placeholder);
        set_style(miqu::ButtonStyle::Standard);

        set_radius(item_h / 2);

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
            item_h
        ));
    }

    request_redraw();
}

void SearchButtonView::launch_search() {
    const auto& cmd = BarConfig::get().search_cmd;
    if (cmd.empty()) return;

    std::string bin_name = cmd;
    size_t space_pos = bin_name.find(' ');
    if (space_pos != std::string::npos) {
        bin_name = bin_name.substr(0, space_pos);
    }
    size_t slash_pos = bin_name.find_last_of('/');
    if (slash_pos != std::string::npos) {
        bin_name = bin_name.substr(slash_pos + 1);
    }

    std::string check_cmd = "pgrep -x " + bin_name + " >/dev/null 2>&1";
    if (std::system(check_cmd.c_str()) == 0) {
        std::string kill_cmd = "pkill -x " + bin_name + " 2>/dev/null";
        std::system(kill_cmd.c_str());
        return;
    }

    miqu::AppManager::launch_command(cmd);
}

} // namespace miqubar
