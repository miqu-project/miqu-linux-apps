#pragma once

#include "sys_info.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <memory>
#include <array>

namespace miquinfo {

class InfoApp {
public:
    explicit InfoApp(std::shared_ptr<miqu::AppEngine> engine);
    ~InfoApp() = default;

    bool init();
    int run();

    void refresh();
    void copy_to_clipboard();
    void select_page(int index);

private:
    void setup_ui();
    void handle_key(const miqu::KeyPressEvent& ev);

    std::shared_ptr<miqu::AppEngine> m_engine;
    std::shared_ptr<miqu::Window> m_window;
    std::shared_ptr<miqu::Toolbar> m_toolbar;
    std::shared_ptr<miqu::ViewPager> m_pager;
    std::shared_ptr<miqu::BottomNavigationView> m_bottom_nav;

    std::array<std::shared_ptr<miqu::LinearLayout>, 5> m_page_cols;
    SystemInfo m_info;
};

} // namespace miquinfo
