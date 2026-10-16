#include "test_app.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    auto engine = miqu::AppEngine::create();
    if (!engine) {
        std::cerr << "[miqutest] Failed to initialize AppEngine.\n";
        return 1;
    }

    std::string layer_ns = (argc > 1) ? argv[1] : "miqutest";
    std::cout << "[miqutest] Starting simple bar with namespace '" << layer_ns << "'...\n";

    miqutest::TestApp app(engine, layer_ns);
    if (!app.init()) {
        std::cerr << "[miqutest] Failed to initialize bar window.\n";
        return 1;
    }

    return app.run();
}
