#include "daemon.hpp"
#include <csignal>
#include <iostream>
#include <memory>
#include <unistd.h>

static std::unique_ptr<miqudm::Daemon> s_daemon;

static void signal_handler(int sig) {
    if (s_daemon) {
        std::cout << "[miqudm-daemon] Caught signal " << sig << ", stopping..." << std::endl;
        s_daemon->stop();
    }
}

int main(int argc, char* argv[]) {
    // Check if root
    if (geteuid() != 0) {
        std::cerr << "[miqudm-daemon] Error: miqudm daemon must run as root." << std::endl;
        std::cerr << "Note: If you want to test the UI, run 'miqudm-greeter --test' as a normal user." << std::endl;
        return 1;
    }

    struct sigaction sa = {};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    signal(SIGPIPE, SIG_IGN);

    s_daemon = std::make_unique<miqudm::Daemon>();
    if (!s_daemon->init()) {
        std::cerr << "[miqudm-daemon] Initialization failed, exiting." << std::endl;
        return 1;
    }

    s_daemon->run();
    std::cout << "[miqudm-daemon] Daemon shutdown cleanly." << std::endl;
    return 0;
}
