#include "greeter_app.hpp"
#include <iostream>

static void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n\n"
              << "Options:\n"
              << "  -t, --test             Run in test/preview mode (windowed, test authentication)\n"
              << "  -h, --help             Show this help message\n\n";
}

int main(int argc, char* argv[]) {
    bool test_mode = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-t" || arg == "--test") {
            test_mode = true;
        }
    }

    auto engine = miqu::AppEngine::create();
    if (!engine) {
        std::cerr << "[miqudm-greeter] Failed to create AppEngine!" << std::endl;
        return 1;
    }

    miqudm::GreeterApp app(engine, test_mode);
    if (!app.init()) {
        std::cerr << "[miqudm-greeter] Failed to initialize Greeter UI!" << std::endl;
        return 1;
    }

    return app.run();
}
