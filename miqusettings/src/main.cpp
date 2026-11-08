#include "config/settings_config.hpp"
#include "ui/settings_window.hpp"
#include <miqutoolkit/core/app_engine.hpp>
#include <miqutoolkit/core/config.hpp>
#include <miqutoolkit/core/fs_utils.hpp>
#include <iostream>
#include <string>

using namespace miqu;

int main(int argc, char* argv[]) {
    std::string target_page;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: miqusettings [OPTIONS] [PAGE_ID]\n\n"
                      << "Settings - Modern Miller-columns centralized system settings built with miqutoolkit.\n\n"
                      << "Options:\n"
                      << "  -h, --help       Show this help message and exit\n"
                      << "  -p, --page ID    Open directly to specified page (e.g. bluetooth, wifi, sound)\n"
                      << "  --init-config    Generate default user configuration file with safe backup\n";
            return 0;
        } else if (arg == "--init-config") {
            std::string res = FsUtils::init_user_config("miqusettings", "miqusettings.conf");
            if (!res.empty()) {
                std::cout << "[miqusettings] Configuration initialized at: " << res << "\n";
            } else {
                std::cout << "[miqusettings] Configuration file already exists or could not be created.\n";
            }
            return 0;
        } else if (arg == "-p" || arg == "--page") {
            if (i + 1 < argc) {
                target_page = argv[++i];
            }
        } else if (!arg.empty() && arg[0] != '-') {
            target_page = arg;
        }
    }

    auto engine = AppEngine::create();
    if (!engine) {
        std::cerr << "[miqusettings] Failed to initialize AppEngine.\n";
        return 1;
    }

    // 1. Load configuration according to Two-Tier Single-File Authority Model
    SettingsConfig::get().load();

    // 2. Set up dynamic config & theme reload watcher
    engine->setup_config_watcher();

    // 3. Create Settings Window
    auto settings_window = SettingsWindow::create(engine);
    if (!settings_window) {
        std::cerr << "[miqusettings] Failed to create SettingsWindow.\n";
        return 1;
    }

    // 4. Hook up live dynamic theme change propagation across the desktop ecosystem
    engine->add_theme_change_listener([&settings_window]() {
        SettingsConfig::get().sync_defaults_from_toolkit();
        if (settings_window) {
            settings_window->sync_theme();
        }
    });

    if (!target_page.empty()) {
        settings_window->navigate_to_id(target_page);
    }

    settings_window->show();
    return engine->enter_loop();
}
