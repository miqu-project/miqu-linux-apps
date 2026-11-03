#include "greeter_app.hpp"
#include "common/config.hpp"
#include <miqutoolkit/core/app_engine.hpp>
#include <iostream>
#include <string>

using namespace miqu;
using namespace miqudm;

static void print_help(const char* prog) {
    std::cout << "Usage: " << prog << " [OPTIONS]\n\n"
              << "Options:\n"
              << "  -t, --test           Run in test mode inside existing desktop session\n"
              << "  -c, --config <path>  Load custom configuration file\n"
              << "  -h, --help           Show this help message\n"
              << "  -v, --version        Show version information\n";
}

int main(int argc, char* argv[]) {
    bool test_mode = false;
    std::string config_path;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--test" || arg == "-t") {
            test_mode = true;
        } else if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
            config_path = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            print_help(argv[0]);
            return 0;
        } else if (arg == "-v" || arg == "--version") {
            std::cout << "miqudm-greeter 1.0.0 (Miqu Desktop Ecosystem)\n";
            return 0;
        }
    }

    auto engine = AppEngine::create();
    if (!engine) {
        std::cerr << "[miqudm-greeter] Failed to initialize Wayland AppEngine.\n";
        return 1;
    }

    auto greeter = std::make_unique<GreeterApp>(test_mode, config_path);
    if (!greeter->init(engine)) {
        std::cerr << "[miqudm-greeter] Failed to initialize greeter application.\n";
        return 1;
    }

    return engine->enter_loop();
}
