#pragma once

#include "miller_view.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <memory>

namespace miqu {

class TestApp {
public:
    explicit TestApp(std::shared_ptr<AppEngine> engine);
    ~TestApp() = default;

    bool init();
    void sync_theme();
    void handle_key_press(const KeyPressEvent& event);

    std::shared_ptr<Window> get_window() const { return m_window; }

private:
    void setup_ui();
    void update_header();

    std::shared_ptr<AppEngine> m_engine;
    std::shared_ptr<Window> m_window;

    std::shared_ptr<LinearLayout> m_root_layout;
    std::shared_ptr<Toolbar> m_toolbar;
    std::shared_ptr<LinearLayout> m_ribbon;
    std::shared_ptr<CardView> m_back_btn;
    std::shared_ptr<TextView> m_back_label;
    std::shared_ptr<TextView> m_path_label;
    std::shared_ptr<SearchView> m_search_view;
    std::shared_ptr<MillerView> m_miller_view;
};

} // namespace miqu
