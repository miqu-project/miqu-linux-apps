#include "core/bar_window.hpp"
#include "config/bar_config.hpp"
#include "miqutoolkit/core/app_engine.hpp"
#include "miqutoolkit/core/config.hpp"
#include <iostream>
#include <csignal>
#include <string>

static std::shared_ptr<miqu::AppEngine> s_engine;
static miqubar::BarWindow* s_bar = nullptr;

static void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        if (s_engine) {
            s_engine->quit();
        }
    } else if (sig == SIGHUP || sig == SIGUSR2) {
        if (s_bar) {
            miqubar::BarConfig::get().load();
            s_bar->request_redraw();
        }
    }
}

int main(int argc, char* argv[]) {
    std::string config_path;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
            config_path = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: miqubar [OPTIONS]\n"
                      << "Wayland status bar and taskbar for Miquland\n\n"
                      << "Options:\n"
                      << "  -c, --config <path>   Use specific configuration file\n"
                      << "  --init-config         Generate default user configuration file\n"
                      << "  -h, --help            Show this help message\n";
            return 0;
        } else if (arg == "--init-config") {
            std::string res = miqu::Config::init_user_config("miqubar", "miqubar.conf");
            if (!res.empty()) {
                std::cout << "[miqubar] Configuration initialized at: " << res << "\n";
            } else {
                std::cout << "[miqubar] Configuration file already exists or could not be created.\n";
            }
            return 0;
        }
    }

    std::cout << "[miqubar] Starting Wayland taskbar & status panel..." << std::endl;

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGHUP, signal_handler);
    std::signal(SIGUSR2, signal_handler);

    // 1. Initialize toolkit engine (loads miqutoolkit.conf chain)
    s_engine = miqu::AppEngine::create();
    if (!s_engine) {
        std::cerr << "[miqubar] Failed to initialize AppEngine." << std::endl;
        return 1;
    }

    // 2. Load configuration (syncs from toolkit, optionally overlays user config)
    miqubar::BarConfig::get().load(config_path);

    // 3. Register inotify watcher for live config reloads
    s_engine->setup_config_watcher();

    // 4. Initialize BarWindow
    auto bar = std::make_unique<miqubar::BarWindow>(s_engine.get());
    s_bar = bar.get();

    s_engine->add_theme_change_listener([&bar]() {
        miqubar::BarConfig::get().sync_defaults_from_toolkit();
        bar->sync_theme();
    });

    if (!bar->init()) {
        std::cerr << "[miqubar] Failed to initialize BarWindow." << std::endl;
        return 1;
    }

    std::cout << "[miqubar] Bar active. Running event loop." << std::endl;
    return s_engine->enter_loop();
}
