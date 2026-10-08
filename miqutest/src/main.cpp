#include "test_app.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    auto engine = miqu::AppEngine::create();
    if (!engine) {
        std::cerr << "[miqutest] Failed to initialize AppEngine.\n";
        return 1;
    }

    miqutest::TestApp app(engine);
    if (!app.init()) {
        std::cerr << "[miqutest] Failed to initialize UI window.\n";
        return 1;
    }

    return app.run();
}
