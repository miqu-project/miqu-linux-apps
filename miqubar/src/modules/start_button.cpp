#include "start_button.hpp"
#include "../config/bar_config.hpp"
#include "miqutoolkit/core/config.hpp"
#include <unistd.h>
#include <sys/wait.h>

namespace miqubar {

StartButtonView::StartButtonView() {
    const auto& cfg = BarConfig::get();
    auto config = miqu::Config::get();

    set_circle(false);
    set_corner_radius(config->metrics.corner_radius);
    set_icon_size(cfg.icon_size);
    // Use standard Freedesktop application icon (start-here) or miqu logo
    set_icon("start-here");
    set_padding(4);

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
