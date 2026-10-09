#pragma once

#include "miqutoolkit/view/view.hpp"
#include "miqutoolkit/core/window.hpp"
#include "../modules/start_button.hpp"
#include "../modules/workspace_button.hpp"
#include "../modules/taskbar.hpp"
#include "../modules/sys_modules.hpp"
#include "../popups/quick_settings_popup.hpp"
#include "../popups/calendar_popup.hpp"
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace miqubar {

class BarRootView : public miqu::View {
public:
    BarRootView();
    ~BarRootView() override;

    void draw(cairo_t* cr, const miqu::Rect& bounds) override;

    bool on_mouse_enter(int lx, int ly);
    bool on_mouse_leave();
    bool on_mouse_move(int lx, int ly, const miqu::Rect& bounds) override;
    bool on_mouse_button(int lx, int ly, miqu::MouseButton button, bool pressed, const miqu::Rect& bounds) override;
    bool on_scroll(double delta) override;

    void update_telemetry();
    void update_clock();

    void toggle_quick_settings();
    void toggle_calendar();

private:
    std::shared_ptr<StartButtonView> m_start_btn;
    std::shared_ptr<WorkspaceButtonView> m_ws_btn;
    std::shared_ptr<TaskbarView> m_taskbar;
    std::shared_ptr<CpuModuleView> m_cpu;
    std::shared_ptr<MemoryModuleView> m_mem;
    std::shared_ptr<BatteryModuleView> m_battery;
    std::shared_ptr<VolumeModuleView> m_volume;
    std::shared_ptr<ClockModuleView> m_clock;
    std::shared_ptr<PeekButtonView> m_peek;

    std::shared_ptr<miqu::Window> m_quick_settings_win;
    std::shared_ptr<miqu::Window> m_calendar_win;

    int m_last_mouse_x = -1;
    int m_last_mouse_y = -1;
};

class BarWindow {
public:
    BarWindow(miqu::AppEngine* engine);
    ~BarWindow();

    bool init();
    void request_redraw();

private:
    void start_telemetry_timer();
    void stop_telemetry_timer();

    miqu::AppEngine* m_engine = nullptr;
    std::shared_ptr<miqu::Window> m_window;
    std::shared_ptr<BarRootView> m_root_view;

    std::thread m_timer_thread;
    std::atomic<bool> m_running{false};
    std::mutex m_mutex;
    std::condition_variable m_cv;
};

} // namespace miqubar
