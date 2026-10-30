#include <miqutoolkit/miqutoolkit.hpp>
#include <iostream>
#include <chrono>

using namespace miqu;

int main(int argc, char* argv[]) {
    auto t_start = std::chrono::steady_clock::now();

    auto engine = AppEngine::create();
    if (!engine) {
        std::cerr << "[miqutest] Failed to initialize AppEngine.\n";
        return 1;
    }
    auto t_engine = std::chrono::steady_clock::now();

    engine->setup_config_watcher();

    std::string layer_ns = "miqutest";
    bool auto_exit = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--auto-exit") {
            auto_exit = true;
        } else if (arg.rfind("-", 0) != 0) {
            layer_ns = arg;
        }
    }

    auto config = Config::get();

    // Centered Hello World text view
    auto label = TextViewBuilder::create()
        ->text("Hello, World!")
        ->h1()
        ->center(true)
        ->build();

    LayoutParams text_lp;
    text_lp.width = static_cast<int>(LayoutDimension::WrapContent);
    text_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    text_lp.gravity = Gravity::Center;

    // Background card matching miqulauncher layout & styling
    auto root_card = CardViewBuilder::create()
        ->backgroundColor(config->colors.background)
        ->stroke(config->metrics.border_width, config->colors.outline)
        ->cornerRadius(config->metrics.corner_radius)
        ->elevation(0)
        ->padding(40)
        ->addView(label, text_lp)
        ->build();

    auto t_views = std::chrono::steady_clock::now();

    // LayerOverlay window matching miqulauncher geometry & properties (800x460)
    auto window = WindowBuilder::create()
        ->role(WindowRole::LayerOverlay)
        ->appId(layer_ns)
        ->layerNamespace(layer_ns)
        ->keyboardInteractive(true)
        ->preferredSize(800, 460)
        ->contentSize(800, 460)
        ->anchors(0)
        ->transparent(true)
        ->closeOnClickOutside(true)
        ->closeOnEscape(true)
        ->contentView(root_card)
        ->onClose([engine]() {
            engine->quit();
        })
        ->build();

    auto t_window = std::chrono::steady_clock::now();

    if (!window) {
        std::cerr << "[miqutest] Failed to create layer window.\n";
        return 1;
    }

    auto d_engine = std::chrono::duration_cast<std::chrono::microseconds>(t_engine - t_start).count();
    auto d_views = std::chrono::duration_cast<std::chrono::microseconds>(t_views - t_engine).count();
    auto d_window = std::chrono::duration_cast<std::chrono::microseconds>(t_window - t_views).count();
    auto d_total = std::chrono::duration_cast<std::chrono::microseconds>(t_window - t_start).count();

    std::cout << "[miqutest profiler] AppEngine::create: " << d_engine / 1000.0 << " ms\n";
    std::cout << "[miqutest profiler] Views build:       " << d_views / 1000.0 << " ms\n";
    std::cout << "[miqutest profiler] Window build:      " << d_window / 1000.0 << " ms (includes Wayland roundtrip & 1st frame render)\n";
    std::cout << "[miqutest profiler] Total to 1st frame:" << d_total / 1000.0 << " ms\n";

    if (auto_exit) {
        return 0;
    }

    return engine->enter_loop();
}
