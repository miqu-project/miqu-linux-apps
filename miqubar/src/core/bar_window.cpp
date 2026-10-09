#include "bar_window.hpp"
#include "../config/bar_config.hpp"
#include "miqutoolkit/core/app_engine.hpp"
#include <iostream>
#include <cmath>

namespace miqubar {

BarRootView::BarRootView() {
    const auto& cfg = BarConfig::get();

    if (cfg.show_start) {
        m_start_btn = std::make_shared<StartButtonView>();
    }
    if (cfg.show_workspace_btn) {
        m_ws_btn = std::make_shared<WorkspaceButtonView>();
    }
    if (cfg.show_taskbar) {
        m_taskbar = std::make_shared<TaskbarView>();
    }

    if (cfg.show_cpu) m_cpu = std::make_shared<CpuModuleView>();
    if (cfg.show_memory) m_mem = std::make_shared<MemoryModuleView>();
    if (cfg.show_battery) m_battery = std::make_shared<BatteryModuleView>();
    if (cfg.show_volume) {
        m_volume = std::make_shared<VolumeModuleView>([this]() {
            toggle_quick_settings();
        });
    }
    if (cfg.show_clock) {
        m_clock = std::make_shared<ClockModuleView>([this]() {
            toggle_calendar();
        });
    }
    if (cfg.show_peek) {
        m_peek = std::make_shared<PeekButtonView>();
    }
}

BarRootView::~BarRootView() {
    if (m_quick_settings_win) m_quick_settings_win->close();
    if (m_calendar_win) m_calendar_win->close();
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

    uint32_t anchors = 2 | 8; // Bottom (2) | Right (8)
    if (BarConfig::get().position == "top") anchors = 1 | 8; // Top (1) | Right (8)

    m_quick_settings_win = miqu::WindowBuilder::create()
        ->role(miqu::WindowRole::LayerOverlay)
        ->layerNamespace("miqubar-quicksettings")
        ->contentSize(300, 260)
        ->anchors(anchors)
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

    uint32_t anchors = 2 | 8; // Bottom | Right
    if (BarConfig::get().position == "top") anchors = 1 | 8;

    m_calendar_win = miqu::WindowBuilder::create()
        ->role(miqu::WindowRole::LayerOverlay)
        ->layerNamespace("miqubar-calendar")
        ->contentSize(280, 310)
        ->anchors(anchors)
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

void BarRootView::draw(cairo_t* cr, const miqu::Rect& bounds) {
    if (!cr) return;
    const auto& cfg = BarConfig::get();

    // 1. Bar background
    cairo_set_source_rgba(cr, 0.07, 0.09, 0.13, cfg.opacity);
    cairo_rectangle(cr, bounds.x, bounds.y, bounds.width, bounds.height);
    cairo_fill(cr);

    // Subtle edge border line (top border for bottom bar, bottom border for top bar)
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.10);
    cairo_set_line_width(cr, 1.0);
    if (cfg.position == "bottom") {
        cairo_move_to(cr, bounds.x, bounds.y + 0.5);
        cairo_line_to(cr, bounds.x + bounds.width, bounds.y + 0.5);
    } else {
        cairo_move_to(cr, bounds.x, bounds.y + bounds.height - 0.5);
        cairo_line_to(cr, bounds.x + bounds.width, bounds.y + bounds.height - 0.5);
    }
    cairo_stroke(cr);

    // 2. Layout Left Children
    int cur_x = bounds.x + 4;
    if (m_start_btn) {
        auto sz = m_start_btn->measure_size();
        miqu::Rect r{cur_x, bounds.y, sz.width, bounds.height};
        m_start_btn->set_bounds(r);
        m_start_btn->draw(cr, r);
        cur_x += sz.width + 4;
    }

    if (m_ws_btn) {
        auto sz = m_ws_btn->measure_size();
        miqu::Rect r{cur_x, bounds.y, sz.width, bounds.height};
        m_ws_btn->set_bounds(r);
        m_ws_btn->draw(cr, r);
        cur_x += sz.width + 8;
    }

    int left_end_x = cur_x;

    // 3. Layout Right Children (working backwards from right edge)
    int right_x = bounds.x + bounds.width;

    if (m_peek) {
        auto sz = m_peek->measure_size();
        right_x -= sz.width;
        miqu::Rect r{right_x, bounds.y, sz.width, bounds.height};
        m_peek->set_bounds(r);
        m_peek->draw(cr, r);
    }

    if (m_clock) {
        auto sz = m_clock->measure_size();
        right_x -= sz.width + 2;
        miqu::Rect r{right_x, bounds.y, sz.width, bounds.height};
        m_clock->set_bounds(r);
        m_clock->draw(cr, r);
    }

    if (m_battery) {
        auto sz = m_battery->measure_size();
        if (sz.width > 0) {
            right_x -= sz.width + 2;
            miqu::Rect r{right_x, bounds.y, sz.width, bounds.height};
            m_battery->set_bounds(r);
            m_battery->draw(cr, r);
        }
    }

    if (m_volume) {
        auto sz = m_volume->measure_size();
        right_x -= sz.width + 2;
        miqu::Rect r{right_x, bounds.y, sz.width, bounds.height};
        m_volume->set_bounds(r);
        m_volume->draw(cr, r);
    }

    if (m_mem) {
        auto sz = m_mem->measure_size();
        right_x -= sz.width + 2;
        miqu::Rect r{right_x, bounds.y, sz.width, bounds.height};
        m_mem->set_bounds(r);
        m_mem->draw(cr, r);
    }

    if (m_cpu) {
        auto sz = m_cpu->measure_size();
        right_x -= sz.width + 2;
        miqu::Rect r{right_x, bounds.y, sz.width, bounds.height};
        m_cpu->set_bounds(r);
        m_cpu->draw(cr, r);
    }

    // 4. Center: Running Applications Taskbar
    if (m_taskbar) {
        int taskbar_w = std::max(0, right_x - left_end_x - 8);
        miqu::Rect r{left_end_x, bounds.y, taskbar_w, bounds.height};
        m_taskbar->set_bounds(r);
        m_taskbar->draw(cr, r);
    }
}

static std::shared_ptr<miqu::View> find_child_at(
    const std::vector<std::shared_ptr<miqu::View>>& children, int x, int y) {
    for (const auto& child : children) {
        if (child && child->get_bounds().contains(x, y)) {
            return child;
        }
    }
    return nullptr;
}

bool BarRootView::on_mouse_move(int lx, int ly, const miqu::Rect&) {
    m_last_mouse_x = lx;
    m_last_mouse_y = ly;

    std::vector<std::shared_ptr<miqu::View>> views = {
        m_start_btn, m_ws_btn, m_taskbar, m_cpu, m_mem, m_battery, m_volume, m_clock, m_peek
    };

    bool handled = false;
    for (const auto& view : views) {
        if (view) {
            if (view->on_mouse_move(lx, ly, view->get_bounds())) {
                handled = true;
            }
        }
    }
    return handled;
}

bool BarRootView::on_mouse_enter(int lx, int ly) {
    return on_mouse_move(lx, ly, get_bounds());
}

bool BarRootView::on_mouse_leave() {
    m_last_mouse_x = -1;
    m_last_mouse_y = -1;

    std::vector<std::shared_ptr<miqu::View>> views = {
        m_start_btn, m_ws_btn, m_taskbar, m_cpu, m_mem, m_battery, m_volume, m_clock, m_peek
    };
    for (const auto& view : views) {
        if (view) view->on_mouse_move(-999, -999, view->get_bounds());
    }
    request_redraw();
    return true;
}

bool BarRootView::on_mouse_button(int lx, int ly, miqu::MouseButton button, bool pressed, const miqu::Rect&) {
    std::vector<std::shared_ptr<miqu::View>> views = {
        m_start_btn, m_ws_btn, m_taskbar, m_cpu, m_mem, m_battery, m_volume, m_clock, m_peek
    };

    auto target = find_child_at(views, lx, ly);
    if (target) {
        return target->on_mouse_button(lx, ly, button, pressed, target->get_bounds());
    }
    return false;
}

bool BarRootView::on_scroll(double delta) {
    if (m_ws_btn && m_ws_btn->get_bounds().contains(m_last_mouse_x, m_last_mouse_y)) {
        return m_ws_btn->on_scroll(delta);
    }
    if (m_volume && m_volume->get_bounds().contains(m_last_mouse_x, m_last_mouse_y)) {
        return m_volume->on_scroll(delta);
    }
    return false;
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
