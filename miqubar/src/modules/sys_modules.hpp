#pragma once

#include "miqutoolkit/view/button.hpp"
#include <string>
#include <functional>

namespace miqubar {

// CPU Usage Button
class CpuModuleView : public miqu::Button {
public:
    CpuModuleView();
    void update_metrics();

private:
    int m_cpu_pct = 0;
    unsigned long long m_prev_idle = 0;
    unsigned long long m_prev_total = 0;
};

// Memory Usage Button
class MemoryModuleView : public miqu::Button {
public:
    MemoryModuleView();
    void update_metrics();

private:
    int m_mem_pct = 0;
};

// Battery Status Button
class BatteryModuleView : public miqu::Button {
public:
    BatteryModuleView();
    void update_metrics();

private:
    int m_capacity = 100;
    bool m_charging = false;
    bool m_present = false;
};

// Volume Control Button
class VolumeModuleView : public miqu::Button {
public:
    explicit VolumeModuleView(std::function<void()> on_open_quick_settings);
    void update_metrics();

    bool on_mouse_button(int lx, int ly, miqu::MouseButton button, bool pressed, const miqu::Rect& bounds) override;
    bool on_scroll(double delta) override;

    int get_volume() const { return m_volume; }
    bool is_muted() const { return m_muted; }

private:
    int m_volume = 70;
    bool m_muted = false;
    std::function<void()> m_on_open_quick_settings;
};

// Windows-style Stacked Clock & Date Button
class ClockModuleView : public miqu::Button {
public:
    explicit ClockModuleView(std::function<void()> on_open_calendar);
    void update_time();

private:
    std::string m_time_str;
    std::string m_date_str;
    std::function<void()> m_on_open_calendar;
};

// Windows-style "Show Desktop" peek button on far right edge
class PeekButtonView : public miqu::Button {
public:
    PeekButtonView();
};

} // namespace miqubar
