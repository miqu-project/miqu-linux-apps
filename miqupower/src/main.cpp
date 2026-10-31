#include "config/power_config.hpp"
#include "ui/power_window.hpp"
#include <miqutoolkit/core/app_engine.hpp>
#include <miqutoolkit/core/config.hpp>

#include <iostream>
#include <csignal>
#include <optional>

static void sig_handler(int sig) {
    if (miqu::AppEngine::instance()) {
        miqu::AppEngine::instance()->quit(0);
    }
}

int main(int argc, char* argv[]) {
    std::string custom_config = "";
    std::optional<miqu::PowerAction> direct_action;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: miqupower [OPTIONS]\n\n"
                      << "Modern Desktop Power & Session Management Dialog built with miqutoolkit.\n\n"
                      << "Options:\n"
                      << "  -h, --help               Show this help message and exit\n"
                      << "  --init-config            Generate default user configuration file\n"
                      << "  -c, --config <path>      Specify custom configuration file\n"
                      << "  --action <action>        Directly trigger an action (shutdown|reboot|suspend|hibernate|lock|logout)\n"
                      << "  --shutdown               Directly trigger shutdown\n"
                      << "  --reboot                 Directly trigger reboot\n"
                      << "  --suspend                Directly trigger suspend\n"
                      << "  --hibernate              Directly trigger hibernate\n"
                      << "  --lock                   Directly trigger lock\n"
                      << "  --logout                 Directly trigger logout\n";
            return 0;
        } else if (arg == "--init-config") {
            std::string res = miqu::Config::init_user_config("miqupower", "miqupower.conf");
            if (!res.empty()) {
                std::cout << "[miqupower] Configuration initialized at: " << res << "\n";
            } else {
                std::cout << "[miqupower] Configuration file already exists or could not be created.\n";
            }
            return 0;
        } else if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
            custom_config = argv[++i];
        } else if (arg == "--action" && i + 1 < argc) {
            std::string act = argv[++i];
            if (act == "shutdown") direct_action = miqu::PowerAction::Shutdown;
            else if (act == "reboot") direct_action = miqu::PowerAction::Reboot;
            else if (act == "suspend") direct_action = miqu::PowerAction::Suspend;
            else if (act == "hibernate") direct_action = miqu::PowerAction::Hibernate;
            else if (act == "lock") direct_action = miqu::PowerAction::Lock;
            else if (act == "logout") direct_action = miqu::PowerAction::Logout;
        } else if (arg == "--shutdown") {
            direct_action = miqu::PowerAction::Shutdown;
        } else if (arg == "--reboot") {
            direct_action = miqu::PowerAction::Reboot;
        } else if (arg == "--suspend") {
            direct_action = miqu::PowerAction::Suspend;
        } else if (arg == "--hibernate") {
            direct_action = miqu::PowerAction::Hibernate;
        } else if (arg == "--lock") {
            direct_action = miqu::PowerAction::Lock;
        } else if (arg == "--logout") {
            direct_action = miqu::PowerAction::Logout;
        }
    }

    std::signal(SIGINT, sig_handler);
    std::signal(SIGTERM, sig_handler);

    // Initialize configuration
    miqu::PowerConfig::get().load(custom_config);

    auto engine = miqu::AppEngine::create();
    if (!engine) {
        std::cerr << "[miqupower] Error: Failed to connect to Wayland display. Is a Wayland compositor running?" << std::endl;
        return 1;
    }

    engine->setup_config_watcher();
    engine->set_quit_on_last_window_closed(true);

    auto window = std::make_shared<miqu::PowerWindow>();
    window->show(direct_action);

    return engine->enter_loop();
}
