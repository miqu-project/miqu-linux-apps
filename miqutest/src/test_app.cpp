#include "test_app.hpp"
#include <iostream>

using namespace miqu;

namespace miqutest {

TestApp::TestApp(std::shared_ptr<AppEngine> engine, std::string layer_ns)
    : m_engine(std::move(engine)), m_layer_ns(std::move(layer_ns)) {}

bool TestApp::init() {
    if (!m_engine) return false;

    // Simple bar root view: CardView with elevation 0, no color set
    auto card = std::make_shared<CardView>();
    card->set_elevation(0.0);
    card->set_corner_radius(0);

    // Anchors: Bottom | Left | Right (2 | 4 | 8)
    uint32_t anchors = 2 | 4 | 8;

    m_window = WindowBuilder::create()
        ->role(WindowRole::LayerTop)
        ->title("miqutest")
        ->appId("miqutest")
        ->layerNamespace(m_layer_ns)
        ->anchors(anchors)
        ->exclusiveZone(48)
        ->preferredSize(0, 48)
        ->transparent(true)
        ->keyboardInteractive(false)
        ->closeOnClickOutside(false)
        ->closeOnEscape(false)
        ->contentView(card)
        ->build();

    if (!m_window) {
        std::cerr << "[miqutest] Failed to create bar window.\n";
        return false;
    }

    m_window->show();
    return true;
}

int TestApp::run() {
    if (!m_engine) return 1;
    return m_engine->enter_loop();
}

} // namespace miqutest
