#include <miqutoolkit/miqutoolkit.hpp>
#include "ui/launcher_window.hpp"
#include "system/cli_parser.hpp"
#include "system/config_manager.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    auto t_start = std::chrono::steady_clock::now();
    miqu::LauncherConfig config;
    bool auto_exit = false;

    // Scan if custom config file path or --init-config was specified on CLI
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--auto-exit") {
            auto_exit = true;
        } else if (arg == "--init-config") {
            std::string res = miqu::Config::init_user_config("miqulauncher", "miqulauncher.conf");
            if (!res.empty()) {
                std::cout << "[miqulauncher] Configuration initialized at: " << res << "\n";
            } else {
                std::cout << "[miqulauncher] Configuration file already exists or could not be created.\n";
            }
            return 0;
        } else if ((arg == "-c" || arg == "--config" || arg == "-config") && i + 1 < argc) {
            config.config_path = argv[++i];
        }
    }

    auto t_before_engine = std::chrono::steady_clock::now();
    auto engine = miqu::AppEngine::create();
    if (!engine) {
        return 1;
    }
    auto t_engine = std::chrono::steady_clock::now();

    // Load independent launcher configuration & theme
    miqu::ConfigManager::load(config);
    auto t_config = std::chrono::steady_clock::now();

    // Parse CLI options (CLI options override config file settings)
    auto parse_res = miqu::CliParser::parse(argc, argv, config);
    if (parse_res == miqu::CliParser::ParseResult::ExitSuccess) {
        return 0;
    }
    if (parse_res == miqu::CliParser::ParseResult::Error) {
        return 1;
    }
    auto t_cli = std::chrono::steady_clock::now();

    auto launcher = std::make_shared<miqu::LauncherWindow>(engine.get(), std::move(config));
    auto t_launcher_create = std::chrono::steady_clock::now();
    if (!launcher->init()) {
        return 1;
    }
    auto t_launcher_init = std::chrono::steady_clock::now();

    if (auto_exit) {
        auto d_engine = std::chrono::duration_cast<std::chrono::microseconds>(t_engine - t_before_engine).count();
        auto d_config = std::chrono::duration_cast<std::chrono::microseconds>(t_config - t_engine).count();
        auto d_cli = std::chrono::duration_cast<std::chrono::microseconds>(t_cli - t_config).count();
        auto d_init = std::chrono::duration_cast<std::chrono::microseconds>(t_launcher_init - t_launcher_create).count();
        auto d_total = std::chrono::duration_cast<std::chrono::microseconds>(t_launcher_init - t_start).count();

        std::cout << "[miqulauncher profiler] Engine create: " << d_engine / 1000.0 << " ms\n";
        std::cout << "[miqulauncher profiler] Config load:   " << d_config / 1000.0 << " ms\n";
        std::cout << "[miqulauncher profiler] CLI parse:     " << d_cli / 1000.0 << " ms\n";
        std::cout << "[miqulauncher profiler] Launcher init: " << d_init / 1000.0 << " ms\n";
        std::cout << "[miqulauncher profiler] Total to Frame 0:" << d_total / 1000.0 << " ms\n";
        return 0;
    }

    return engine->enter_loop();
}
