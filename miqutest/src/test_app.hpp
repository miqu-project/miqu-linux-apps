#pragma once

#include <miqutoolkit/miqutoolkit.hpp>
#include <memory>
#include <string>

namespace miqutest {

class TestApp {
public:
    explicit TestApp(std::shared_ptr<miqu::AppEngine> engine, std::string layer_ns = "miqutest");
    ~TestApp() = default;

    bool init();
    int run();

private:
    std::shared_ptr<miqu::AppEngine> m_engine;
    std::string m_layer_ns;
    std::shared_ptr<miqu::Window> m_window;
};

} // namespace miqutest
