#include "bar_window.hpp"
#include "../config/bar_config.hpp"
#include "miqutoolkit/core/app_engine.hpp"
#include "miqutoolkit/core/config.hpp"
#include <iostream>
#include <cmath>

namespace miqubar {

BarRootView::BarRootView() {
    const auto& cfg = BarConfig::get();
    auto config = miqu::Config::get();

    set_background_color(config->colors.background);

    // 1. Left container (pinned left)
    m_left_box = std::make_shared<miqu::LinearLayout>(miqu::Orientation::Horizontal);
    m_left_box->set_gravity(miqu::Gravity::CenterVertical);
    m_left_box->set_margin(6, 0, 0, 0);
    m_left_box->set_layout_params(miqu::LayoutParams(
        static_cast<int>(miqu::LayoutDimension::WrapContent),
        static_cast<int>(miqu::LayoutDimension::MatchParent),
        miqu::Gravity::Left | miqu::Gravity::CenterVertical
    ));

    if (cfg.show_workspace_btn) {
        m_ws_btn = std::make_shared<WorkspaceButtonView>();
        m_left_box->add_view(m_ws_btn);
    }
    add_view(m_left_box);

    // 2. Center container (centered dock)
    m_center_box = std::make_shared<miqu::LinearLayout>(miqu::Orientation::Horizontal);
    m_center_box->set_gravity(miqu::Gravity::CenterVertical);
    m_center_box->set_divider_spacing(4);
    m_center_box->set_layout_params(miqu::LayoutParams(
        static_cast<int>(miqu::LayoutDimension::WrapContent),
        static_cast<int>(miqu::LayoutDimension::MatchParent),
        miqu::Gravity::CenterHorizontal | miqu::Gravity::CenterVertical
    ));

    if (cfg.show_start) {
        m_start_btn = std::make_shared<StartButtonView>();
        m_center_box->add_view(m_start_btn);
    }
    if (cfg.show_taskbar) {
        m_taskbar = std::make_shared<TaskbarView>();
        m_center_box->add_view(m_taskbar);
    }
    add_view(m_center_box);

    // 3. Right container (system telemetry & tray pinned right)
    m_right_box = std::make_shared<miqu::LinearLayout>(miqu::Orientation::Horizontal);
    m_right_box->set_gravity(miqu::Gravity::CenterVertical);
    m_right_box->set_divider_spacing(2);
    m_right_box->set_margin(0, 0, 4, 0);
    m_right_box->set_layout_params(miqu::LayoutParams(
        static_cast<int>(miqu::LayoutDimension::WrapContent),
        static_cast<int>(miqu::LayoutDimension::MatchParent),
        miqu::Gravity::Right | miqu::Gravity::CenterVertical
    ));

    if (cfg.show_cpu) {
        m_cpu = std::make_shared<CpuModuleView>();
        m_right_box->add_view(m_cpu);
    }
    if (cfg.show_memory) {
        m_mem = std::make_shared<MemoryModuleView>();
        m_right_box->add_view(m_mem);
    }
    if (cfg.show_battery) {
        m_battery = std::make_shared<BatteryModuleView>();
        m_right_box->add_view(m_battery);
    }
    if (cfg.show_volume) {
        m_volume = std::make_shared<VolumeModuleView>([this]() {
            toggle_quick_settings();
        });
        m_right_box->add_view(m_volume);
    }
    if (cfg.show_clock) {
        m_clock = std::make_shared<ClockModuleView>([this]() {
            toggle_calendar();
        });
        m_right_box->add_view(m_clock);
    }
    if (cfg.show_peek) {
        m_peek = std::make_shared<PeekButtonView>();
        m_right_box->add_view(m_peek);
    }
    add_view(m_right_box);
}

BarRootView::~BarRootView() {
    if (m_quick_settings_win) m_quick_settings_win->close();
    if (m_calendar_win) m_calendar_win->close();
}

void BarRootView::sync_theme() {
    auto config = miqu::Config::get();
    set_background_color(config->colors.background);
    request_redraw();
}

bool BarRootView::on_mouse_move(int lx, int ly, const miqu::Rect& bounds) {
    m_last_mouse_x = lx;
    m_last_mouse_y = ly;
    return miqu::FrameLayout::on_mouse_move(lx, ly, bounds);
}

bool BarRootView::on_scroll(double delta) {
    if (m_ws_btn && m_ws_btn->is_visible() && m_ws_btn->get_bounds().contains(m_last_mouse_x, m_last_mouse_y)) {
        return m_ws_btn->on_scroll(delta);
    }
    if (m_volume && m_volume->is_visible() && m_volume->get_bounds().contains(m_last_mouse_x, m_last_mouse_y)) {
        return m_volume->on_scroll(delta);
    }
    return miqu::FrameLayout::on_scroll(delta);
}

void BarRootView::toggle_quick_settings() {
    if (m_quick_settings_win) {
        m_quick_settings_win->close();
        m_quick_settings_win = nullptr;
        request_redraw();
        return;
    }
    if (m_calendar_win) {
        m_calendar_win->close();
        m_calendar_win = nullptr;
    }

    auto content = std::make_shared<QuickSettingsPopupView>([this]() {
        if (m_quick_settings_win) {
            m_quick_settings_win->close();
            m_quick_settings_win = nullptr;
            request_redraw();
        }
    });

    const auto& cfg = BarConfig::get();
    bool is_top = (cfg.position == "top");
    miqu::Gravity gravity = is_top ? (miqu::Gravity::Top | miqu::Gravity::Right)
                                   : (miqu::Gravity::Bottom | miqu::Gravity::Right);
    miqu::Margin margin;
    margin.right = 12;
    if (is_top) {
        margin.top = cfg.height + 8;
    } else {
        margin.bottom = cfg.height + 8;
    }

    m_quick_settings_win = miqu::WindowBuilder::create()
        ->role(miqu::WindowRole::LayerOverlay)
        ->layerNamespace("miqubar-quicksettings")
        ->contentSize(300, 260)
        ->contentGravity(gravity)
        ->contentMargin(margin)
        ->dimBackdrop(false)
        ->transparent(true)
        ->exclusiveZone(-1)
        ->closeOnClickOutside(true)
        ->closeOnEscape(true)
        ->contentView(content)
        ->onClose([this]() {
            m_quick_settings_win = nullptr;
            request_redraw();
        })
        ->build();

    if (m_quick_settings_win) m_quick_settings_win->show();
}

void BarRootView::toggle_calendar() {
    if (m_calendar_win) {
        m_calendar_win->close();
        m_calendar_win = nullptr;
        request_redraw();
        return;
    }
    if (m_quick_settings_win) {
        m_quick_settings_win->close();
        m_quick_settings_win = nullptr;
    }

    auto content = std::make_shared<CalendarPopupView>([this]() {
        if (m_calendar_win) {
            m_calendar_win->close();
            m_calendar_win = nullptr;
            request_redraw();
        }
    });

    const auto& cfg = BarConfig::get();
    bool is_top = (cfg.position == "top");
    miqu::Gravity gravity = is_top ? (miqu::Gravity::Top | miqu::Gravity::Right)
                                   : (miqu::Gravity::Bottom | miqu::Gravity::Right);
    miqu::Margin margin;
    margin.right = 12;
    if (is_top) {
        margin.top = cfg.height + 8;
    } else {
        margin.bottom = cfg.height + 8;
    }

    m_calendar_win = miqu::WindowBuilder::create()
        ->role(miqu::WindowRole::LayerOverlay)
        ->layerNamespace("miqubar-calendar")
        ->contentSize(310, 360)
        ->contentGravity(gravity)
        ->contentMargin(margin)
        ->dimBackdrop(false)
        ->transparent(true)
        ->exclusiveZone(-1)
        ->closeOnClickOutside(true)
        ->closeOnEscape(true)
        ->contentView(content)
        ->onClose([this]() {
            m_calendar_win = nullptr;
            request_redraw();
        })
        ->build();

    if (m_calendar_win) m_calendar_win->show();
}

void BarRootView::update_telemetry() {
    if (m_cpu) m_cpu->update_metrics();
    if (m_mem) m_mem->update_metrics();
    if (m_battery) m_battery->update_metrics();
    if (m_volume) m_volume->update_metrics();
    request_redraw();
}

void BarRootView::update_clock() {
    if (m_clock) m_clock->update_time();
    request_redraw();
}

// ==========================================
// BarWindow
// ==========================================
BarWindow::BarWindow(miqu::AppEngine* engine) : m_engine(engine) {}

BarWindow::~BarWindow() {
    stop_telemetry_timer();
    if (m_window) m_window->close();
}

bool BarWindow::init() {
    if (!m_engine) return false;

    const auto& cfg = BarConfig::get();
    m_root_view = std::make_shared<BarRootView>();

    uint32_t anchors = 2 | 4 | 8; // Bottom | Left | Right
    miqu::WindowRole role = miqu::WindowRole::LayerBottom;

    if (cfg.position == "top") {
        anchors = 1 | 4 | 8; // Top | Left | Right
        role = miqu::WindowRole::LayerTop;
    }

    m_window = miqu::WindowBuilder::create()
        ->role(role)
        ->title("miqubar")
        ->appId("miqubar")
        ->layerNamespace("miqubar")
        ->anchors(anchors)
        ->exclusiveZone(cfg.exclusive_zone)
        ->preferredSize(0, cfg.height)
        ->transparent(true)
        ->keyboardInteractive(false)
        ->closeOnClickOutside(false)
        ->closeOnEscape(false)
        ->contentView(m_root_view)
        ->build();

    if (!m_window) {
        std::cerr << "[miqubar] Failed to create bar window." << std::endl;
        return false;
    }

    m_window->show();
    start_telemetry_timer();
    return true;
}

void BarWindow::request_redraw() {
    if (m_window) m_window->schedule_redraw();
}

void BarWindow::sync_theme() {
    if (m_root_view) m_root_view->sync_theme();
    request_redraw();
}

void BarWindow::start_telemetry_timer() {
    m_running = true;
    m_timer_thread = std::thread([this]() {
        int clock_ticks = 0;
        const auto& cfg = BarConfig::get();
        int interval_ms = cfg.telemetry_interval_ms;

        while (m_running) {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (m_cv.wait_for(lock, std::chrono::milliseconds(500), [this]() { return !m_running; })) {
                break;
            }

            if (!m_running) break;

            clock_ticks += 500;
            if (m_root_view) {
                m_root_view->update_clock();
                if (clock_ticks >= interval_ms) {
                    m_root_view->update_telemetry();
                    clock_ticks = 0;
                }
            }
        }
    });
}

void BarWindow::stop_telemetry_timer() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_running = false;
    }
    m_cv.notify_all();
    if (m_timer_thread.joinable()) {
        m_timer_thread.join();
    }
}

} // namespace miqubar
