#pragma once

#include "miqutoolkit/view/frame_layout.hpp"
#include "miqutoolkit/view/linear_layout.hpp"
#include "miqutoolkit/view/card_view.hpp"
#include "miqutoolkit/core/window.hpp"
#include "../modules/start_button.hpp"
#include "../modules/workspace_button.hpp"
#include "../modules/search_button.hpp"
#include "../modules/taskbar.hpp"
#include "../modules/sys_modules.hpp"
#include "../popups/quick_settings_popup.hpp"
#include "../popups/brightness_popup.hpp"
#include "../popups/network_popup.hpp"
#include "../popups/bluetooth_popup.hpp"
#include "../popups/calendar_popup.hpp"
#include "../popups/power_popup.hpp"
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace miqubar {

class BarRootView : public miqu::CardView {
public:
    BarRootView();
    ~BarRootView() override;

    using miqu::FrameLayout::add_view;
    void add_view(std::shared_ptr<miqu::View> child) override {
        miqu::FrameLayout::add_view(std::move(child));
    }

    void sync_theme();
    bool on_mouse_move(int lx, int ly, const miqu::Rect& bounds) override;
    bool on_scroll(double delta) override;

    void update_telemetry();
    void update_clock();

    void toggle_network();
    void toggle_bluetooth();
    void toggle_sound();
    void toggle_brightness();
    void toggle_quick_settings();
    void toggle_calendar();
    void toggle_power();

private:
    std::shared_ptr<miqu::LinearLayout> m_left_box;
    std::shared_ptr<miqu::LinearLayout> m_center_box;
    std::shared_ptr<miqu::LinearLayout> m_right_box;

    std::shared_ptr<StartButtonView> m_start_btn;
    std::shared_ptr<WorkspaceButtonView> m_ws_btn;
    std::shared_ptr<SearchButtonView> m_search_btn;
    std::shared_ptr<TaskbarView> m_taskbar;
    std::shared_ptr<CpuModuleView> m_cpu;
    std::shared_ptr<MemoryModuleView> m_mem;
    std::shared_ptr<NetworkModuleView> m_network;
    std::shared_ptr<BluetoothModuleView> m_bluetooth;
    std::shared_ptr<VolumeModuleView> m_volume;
    std::shared_ptr<BrightnessModuleView> m_brightness;
    std::shared_ptr<BatteryModuleView> m_battery;
    std::shared_ptr<DarkModeModuleView> m_dark_mode;
    std::shared_ptr<NotificationModuleView> m_notifications;
    std::shared_ptr<ClockModuleView> m_clock;
    std::shared_ptr<PowerButtonView> m_power;
    std::shared_ptr<PeekButtonView> m_peek;

    std::shared_ptr<miqu::Window> m_network_win;
    std::shared_ptr<miqu::Window> m_bluetooth_win;
    std::shared_ptr<miqu::Window> m_sound_win;
    std::shared_ptr<miqu::Window> m_brightness_win;
    std::shared_ptr<miqu::Window> m_calendar_win;
    std::shared_ptr<miqu::Window> m_power_win;

    int m_last_mouse_x = -1;
    int m_last_mouse_y = -1;
};

class BarWindow {
public:
    explicit BarWindow(miqu::AppEngine* engine);
    ~BarWindow();

    bool init();
    void request_redraw();
    void sync_theme();

private:
    void start_telemetry_timer();
    void stop_telemetry_timer();
    void schedule_next_telemetry();

    miqu::AppEngine* m_engine = nullptr;
    std::shared_ptr<miqu::Window> m_window;
    std::shared_ptr<BarRootView> m_root_view;

    bool m_running = false;
    uint32_t m_telemetry_timer_id = 0;
    int m_clock_ticks = 0;
};

} // namespace miqubar
