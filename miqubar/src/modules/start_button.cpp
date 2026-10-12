#include "start_button.hpp"
#include "../config/bar_config.hpp"
#include "miqutoolkit/core/config.hpp"
#include "miqutoolkit/view/image_view.hpp"
#include <unistd.h>
#include <sys/wait.h>
#include <filesystem>

namespace miqubar {

StartButtonView::StartButtonView() {
    const auto& cfg = BarConfig::get();
    auto config = miqu::Config::get();

    int item_h = std::max(28, cfg.height - 8);
    set_circle(false);
    set_corner_radius(config->metrics.corner_radius);
    // Make start button icon prominent and bold (24px or item_h - 16)
    int start_sz = std::max(20, std::min(cfg.icon_size, item_h - 8));
    set_icon_size(start_sz);
    set_padding(4);
    set_layout_params(miqu::LayoutParams(item_h, item_h));

    // Subtle rounded tile background like Windows 11 start button
    set_background_color(config->colors.surface_variant.with_alpha(0.35f));
    set_hover_color(config->colors.surface_variant.with_alpha(0.70f));

    // Resolve beautiful colorful Miqu logo or configured start icon
    std::string icon_to_use = !cfg.start_icon.empty() ? cfg.start_icon : "miquland";
    std::string resolved = miqu::ImageView::resolve_icon_path(icon_to_use);
    if (resolved.empty()) {
        if (std::filesystem::exists("/usr/share/icons/hicolor/256x256/apps/miquland.png")) {
            resolved = "/usr/share/icons/hicolor/256x256/apps/miquland.png";
        } else if (std::filesystem::exists("assets/miquland.png")) {
            resolved = "assets/miquland.png";
        }
    }

    if (!resolved.empty()) {
        set_image_resource(resolved);
    } else {
        set_icon(icon_to_use);
    }

    set_on_click_listener([this]() {
        launch_menu();
    });
}

void StartButtonView::launch_menu() {
    const auto& cmd = BarConfig::get().launcher_cmd;
    if (cmd.empty()) return;

    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        pid_t second_child = fork();
        if (second_child == 0) {
            execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
            _exit(127);
        }
        _exit(0);
    } else if (pid > 0) {
        waitpid(pid, nullptr, 0);
    }
}

} // namespace miqubar
