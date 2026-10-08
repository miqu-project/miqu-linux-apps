#include "daemon.hpp"
#include <csignal>
#include <iostream>
#include <string>

static miqudm::Daemon* g_daemon = nullptr;

static void signal_handler(int sig) {
    std::cout << "\n[miqudm] Caught signal " << sig << ", shutting down display manager..." << std::endl;
    if (g_daemon) {
        g_daemon->stop();
    }
}

static void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n\n"
              << "Options:\n"
              << "  -c, --config <path>    Path to custom configuration file\n"
              << "  -h, --help             Show this help message\n"
              << "  -v, --version          Show version information\n\n"
              << "Description:\n"
              << "  miqudm is a lightweight, pure Wayland display manager with PAM and logind session management.\n";
}

int main(int argc, char* argv[]) {
    std::string config_path;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-v" || arg == "--version") {
            std::cout << "miqudm version 0.1.0 (Wayland Display Manager)" << std::endl;
            return 0;
        } else if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
            config_path = argv[++i];
        }
    }

    // Set up signal handling
    struct sigaction sa = {};
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    miqudm::Daemon daemon;
    g_daemon = &daemon;

    if (!daemon.init()) {
        std::cerr << "[miqudm] Failed to initialize daemon!" << std::endl;
        return 1;
    }

    daemon.run();
    return 0;
}
