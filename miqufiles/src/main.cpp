#include "config/files_config.hpp"
#include "ui/files_window.hpp"
#include <miqutoolkit/core/app_engine.hpp>
#include <miqutoolkit/core/config.hpp>
#include <miqutoolkit/core/fs_utils.hpp>
#include <iostream>
#include <string>

using namespace miqu;

int main(int argc, char* argv[]) {
    std::string start_path = "";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: miqufiles [OPTIONS] [path]\n\n"
                      << "Files - Modern Yazi-style Miller columns file manager built with miqutoolkit.\n\n"
                      << "Arguments:\n"
                      << "  path             Directory path to open\n\n"
                      << "Options:\n"
                      << "  -h, --help       Show this help message and exit\n"
                      << "  --init-config    Generate default user configuration file with safe backup\n";
            return 0;
        } else if (arg == "--init-config") {
            std::string res = FsUtils::init_user_config("miqufiles", "miqufiles.conf");
            if (!res.empty()) {
                std::cout << "[miqufiles] Configuration initialized at: " << res << "\n";
            } else {
                std::cout << "[miqufiles] Configuration file already exists or could not be created.\n";
            }
            return 0;
        } else if (!arg.empty() && arg[0] != '-') {
            start_path = arg;
        }
    }

    auto engine = AppEngine::create();
    if (!engine) {
        std::cerr << "[miqufiles] Failed to initialize AppEngine.\n";
        return 1;
    }

    // 1. Load configuration according to Two-Tier Single-File Authority Model
    FilesConfig::get().load();

    // 2. Set up dynamic config & theme reload watcher
    engine->setup_config_watcher();

    // 3. Create Files Window
    auto files_window = FilesWindow::create(engine, start_path);
    if (!files_window) {
        std::cerr << "[miqufiles] Failed to create FilesWindow.\n";
        return 1;
    }

    // 4. Hook up live dynamic theme change propagation across the desktop ecosystem
    engine->add_theme_change_listener([&files_window]() {
        FilesConfig::get().sync_defaults_from_toolkit();
        if (files_window) {
            files_window->sync_theme();
        }
    });

    files_window->show();

    return engine->enter_loop();
}
