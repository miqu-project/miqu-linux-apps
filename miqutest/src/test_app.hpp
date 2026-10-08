#pragma once

#include <miqutoolkit/miqutoolkit.hpp>
#include <memory>
#include <string>

namespace miqutest {

class TestApp {
public:
    explicit TestApp(std::shared_ptr<miqu::AppEngine> engine);
    ~TestApp() = default;

    bool init();
    int run();

private:
    void setup_ui();
    void log_activity(const std::string& msg);

    std::shared_ptr<miqu::AppEngine> m_engine;
    std::shared_ptr<miqu::Window> m_window;

    // UI Widgets
    std::shared_ptr<miqu::TextView> m_status_text;
    std::shared_ptr<miqu::Spinner> m_test_spinner;

    // Menus
    std::shared_ptr<miqu::PopupMenu> m_header_menu;
    std::shared_ptr<miqu::PopupMenu> m_doc_menu;
    std::shared_ptr<miqu::PopupMenu> m_archive_menu;
    std::shared_ptr<miqu::PopupMenu> m_actions_menu;
    std::shared_ptr<miqu::PopupMenu> m_export_menu;
    std::shared_ptr<miqu::PopupMenu> m_above_menu;
};

} // namespace miqutest
