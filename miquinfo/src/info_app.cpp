#include "info_app.hpp"
#include "ui/page_builder.hpp"
#include <xkbcommon/xkbcommon-keysyms.h>
#include <iostream>

using namespace miqu;

namespace miquinfo {

static const std::array<std::string, 5> SECTION_SUBTITLES = {
    "System Health & At-a-Glance Metrics",
    "Processor, Motherboard, GPU & Displays",
    "Thermals, Cooling, Battery Health & AC",
    "Partitions, Filesystems & Memory Topology",
    "Wayland, Themes, Packages & Connectivity"
};

static void copy_to_clipboard_backend(const std::string& text) {
    FILE* pipe = popen("wl-copy 2>/dev/null", "w");
    if (pipe) {
        fputs(text.c_str(), pipe);
        pclose(pipe);
    }
}

InfoApp::InfoApp(std::shared_ptr<AppEngine> engine)
    : m_engine(std::move(engine)) {
}

bool InfoApp::init() {
    if (!m_engine) return false;

    m_info = SysInfoReader::gather();

    for (int i = 0; i < 5; ++i) {
        m_page_cols[i] = std::make_shared<LinearLayout>(Orientation::Vertical);
        m_page_cols[i]->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));
        m_page_cols[i]->set_padding(2, 2);
    }

    PageBuilder::populate_overview(m_page_cols[0], m_info);
    PageBuilder::populate_hardware(m_page_cols[1], m_info);
    PageBuilder::populate_thermals(m_page_cols[2], m_info);
    PageBuilder::populate_storage(m_page_cols[3], m_info);
    PageBuilder::populate_system(m_page_cols[4], m_info);

    setup_ui();
    return m_window != nullptr;
}

void InfoApp::setup_ui() {
    auto root_container = std::make_shared<LinearLayout>(Orientation::Vertical);
    root_container->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    root_container->set_padding(0);

    // Toolbar
    m_toolbar = ToolbarBuilder::create()
        ->title("System Info")
        ->subtitle(SECTION_SUBTITLES[0])
        ->titleAlignment(TitleAlignment::Center)
        ->onRefresh([this]() { refresh(); })
        ->onClose([this]() {
            if (m_window) m_window->request_close();
            else if (m_engine) m_engine->quit();
        })
        ->build();
    m_toolbar->set_margin(14, 12, 14, 8);
    m_toolbar->add_action("📋", [this]() { copy_to_clipboard(); });
    root_container->add_view(m_toolbar);

    // ViewPager
    m_pager = ViewPagerBuilder::create()
        ->currentPage(0)
        ->onPageChanged([this](int old_idx, int new_idx) {
            (void)old_idx;
            if (m_bottom_nav && m_bottom_nav->get_selected_index() != new_idx) {
                m_bottom_nav->set_selected_index(new_idx);
            }
            if (m_toolbar && new_idx >= 0 && new_idx < static_cast<int>(SECTION_SUBTITLES.size())) {
                m_toolbar->set_subtitle(SECTION_SUBTITLES[new_idx]);
            }
        })
        ->build();
    m_pager->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        0,
        1.0f
    ));
    m_pager->set_margin(14, 0, 14, 0);

    for (int i = 0; i < 5; ++i) {
        m_pager->add_page(ScrollViewBuilder::create()->contentView(m_page_cols[i])->build());
    }
    root_container->add_view(m_pager);

    // Bottom Navigation
    m_bottom_nav = BottomNavigationViewBuilder::create()
        ->addItem("Overview", "dialog-information")
        ->addItem("Hardware", "computer")
        ->addItem("Sensors", "applications-system")
        ->addItem("Storage", "drive-harddisk")
        ->addItem("System", "preferences-system-network")
        ->selectedIndex(0)
        ->pillSize(44, 26)
        ->cornerRadius(24)
        ->barHeight(56)
        ->itemWidth(74)
        ->showDivider(false)
        ->onItemSelected([this](int idx) {
            if (m_pager) m_pager->set_current_page(idx);
        })
        ->build();
    m_bottom_nav->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        56,
        Gravity::CenterHorizontal
    ));
    m_bottom_nav->set_margin(16, 6, 16, 12);
    root_container->add_view(m_bottom_nav);

    // Window
    m_window = WindowBuilder::create()
        ->title("System Information - miquinfo")
        ->appId("miquinfo")
        ->role(WindowRole::Toplevel)
        ->preferredSize(600, 680)
        ->closeOnEscape(true)
        ->contentView(root_container)
        ->onClose([this]() {
            if (m_engine) m_engine->quit();
        })
        ->onKey([this](const KeyPressEvent& ev) {
            handle_key(ev);
        })
        ->build();

    if (m_window) {
        m_window->show();
    }
}

void InfoApp::handle_key(const KeyPressEvent& ev) {
    if (!ev.pressed) return;
    if (ev.keysym == XKB_KEY_r || ev.keysym == XKB_KEY_R) {
        refresh();
    } else if (ev.keysym == XKB_KEY_c || ev.keysym == XKB_KEY_C) {
        copy_to_clipboard();
    } else if (ev.keysym == XKB_KEY_1) {
        select_page(0);
    } else if (ev.keysym == XKB_KEY_2) {
        select_page(1);
    } else if (ev.keysym == XKB_KEY_3) {
        select_page(2);
    } else if (ev.keysym == XKB_KEY_4) {
        select_page(3);
    } else if (ev.keysym == XKB_KEY_5) {
        select_page(4);
    } else if (ev.keysym == XKB_KEY_q || ev.keysym == XKB_KEY_Q || ev.keysym == XKB_KEY_Escape) {
        if (m_engine) m_engine->quit();
    }
}

void InfoApp::select_page(int index) {
    if (m_pager) m_pager->set_current_page(index);
}

void InfoApp::refresh() {
    m_info = SysInfoReader::gather();
    PageBuilder::populate_overview(m_page_cols[0], m_info);
    PageBuilder::populate_hardware(m_page_cols[1], m_info);
    PageBuilder::populate_thermals(m_page_cols[2], m_info);
    PageBuilder::populate_storage(m_page_cols[3], m_info);
    PageBuilder::populate_system(m_page_cols[4], m_info);
    if (m_window) m_window->schedule_redraw();
}

void InfoApp::copy_to_clipboard() {
    copy_to_clipboard_backend(m_info.to_markdown());
    if (m_toolbar) {
        m_toolbar->set_subtitle("📋 Report Copied to Clipboard!");
        if (m_window) m_window->schedule_redraw();
    }
}

int InfoApp::run() {
    if (!m_engine) return 1;
    return m_engine->enter_loop();
}

} // namespace miquinfo
