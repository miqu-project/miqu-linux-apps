#include <miqutoolkit/core/app_engine.hpp>
#include <miqutoolkit/core/config.hpp>
#include "config/notify_config.hpp"
#include "core/dbus_service.hpp"
#include "core/notification_manager.hpp"
#include <iostream>
#include <string>

static void print_help(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [options]\n\n"
              << "A lightweight, modern Wayland notification daemon and center built with miqutoolkit.\n\n"
              << "Options:\n"
              << "  -t, --toggle         Toggle the Notification Center history drawer\n"
              << "  --open               Open the Notification Center history drawer\n"
              << "  --close              Close the Notification Center history drawer\n"
              << "  --clear              Clear all notification history\n"
              << "  --dnd                Toggle Do Not Disturb mode\n"
              << "  --init-config        Generate default user configuration file\n"
              << "  -c, --config <path>  Load custom configuration file\n"
              << "  -h, --help           Show this help message and exit\n"
              << "  -v, --version        Show version information and exit\n"
              << std::endl;
}

static void print_version() {
    std::cout << "miqunotify 1.0.0 (miqutoolkit)\n"
              << "Wayland Desktop Notification Daemon & Center\n"
              << std::endl;
}

int main(int argc, char* argv[]) {
    std::string custom_config = "";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            print_help(argv[0]);
            return 0;
        }
        if (arg == "-v" || arg == "--version") {
            print_version();
            return 0;
        }
        if (arg == "--init-config") {
            std::string res = miqu::Config::init_user_config("miqunotify", "miqunotify.conf");
            if (!res.empty()) {
                std::cout << "[miqunotify] Configuration initialized at: " << res << "\n";
            } else {
                std::cout << "[miqunotify] Configuration file already exists or could not be created.\n";
            }
            return 0;
        }
        if (arg == "-t" || arg == "--toggle") {
            if (miqunotify::DBusService::send_control_command("ToggleCenter")) {
                return 0;
            } else {
                std::cerr << "[miqunotify] Daemon not running or failed to send command." << std::endl;
                return 1;
            }
        }
        if (arg == "--open") {
            if (miqunotify::DBusService::send_control_command("OpenCenter")) return 0;
            return 1;
        }
        if (arg == "--close") {
            if (miqunotify::DBusService::send_control_command("CloseCenter")) return 0;
            return 1;
        }
        if (arg == "--clear") {
            if (miqunotify::DBusService::send_control_command("ClearAll")) return 0;
            return 1;
        }
        if (arg == "--dnd") {
            if (miqunotify::DBusService::send_control_command("ToggleDnd")) return 0;
            return 1;
        }
        if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
            custom_config = argv[++i];
        }
    }

    // 1. Initialize toolkit engine
    auto engine = miqu::AppEngine::create();
    if (!engine) {
        std::cerr << "[miqunotify] Failed to initialize AppEngine." << std::endl;
        return 1;
    }

    // Daemon does not exit when 0 toast windows are open
    engine->set_quit_on_last_window_closed(false);

    // 2. Load configuration
    miqunotify::NotifyConfig::get().load(custom_config);

    // 3. Register inotify config watcher for live reloads
    engine->setup_config_watcher();

    // 4. Start Notification Manager and D-Bus Service
    auto manager = std::make_shared<miqunotify::NotificationManager>();
    if (!manager->init()) {
        std::cerr << "[miqunotify] Failed to initialize notification manager. (Is another daemon like dunst, mako, or swaync running?)" << std::endl;
        return 1;
    }

    return engine->enter_loop();
}
