#pragma once

#include "miqutoolkit/view/view.hpp"
#include <string>
#include <functional>

namespace miqubar {

// CPU Usage
class CpuModuleView : public miqu::View {
public:
    CpuModuleView();
    void update_metrics();
    void draw(cairo_t* cr, const miqu::Rect& bounds) override;
    miqu::Size measure_size() const override;

private:
    int m_cpu_pct = 0;
    unsigned long long m_prev_idle = 0;
    unsigned long long m_prev_total = 0;
};

// Memory Usage
class MemoryModuleView : public miqu::View {
public:
    MemoryModuleView();
    void update_metrics();
    void draw(cairo_t* cr, const miqu::Rect& bounds) override;
    miqu::Size measure_size() const override;

private:
    int m_mem_pct = 0;
};

// Battery Status
class BatteryModuleView : public miqu::View {
public:
    BatteryModuleView();
    void update_metrics();
    void draw(cairo_t* cr, const miqu::Rect& bounds) override;
    miqu::Size measure_size() const override;

private:
    int m_capacity = 100;
    bool m_charging = false;
    bool m_present = false;
};

// Volume Control
class VolumeModuleView : public miqu::View {
public:
    VolumeModuleView(std::function<void()> on_open_quick_settings);
    void update_metrics();
    void draw(cairo_t* cr, const miqu::Rect& bounds) override;
    miqu::Size measure_size() const override;

    bool on_mouse_move(int lx, int ly, const miqu::Rect& bounds) override;
    bool on_mouse_button(int lx, int ly, miqu::MouseButton button, bool pressed, const miqu::Rect& bounds) override;
    bool on_scroll(double delta) override;

    int get_volume() const { return m_volume; }
    bool is_muted() const { return m_muted; }

private:
    int m_volume = 70;
    bool m_muted = false;
    bool m_hovered = false;
    std::function<void()> m_on_open_quick_settings;
};

// Windows-style Stacked Clock & Date
class ClockModuleView : public miqu::View {
public:
    ClockModuleView(std::function<void()> on_open_calendar);
    void update_time();
    void draw(cairo_t* cr, const miqu::Rect& bounds) override;
    miqu::Size measure_size() const override;

    bool on_mouse_move(int lx, int ly, const miqu::Rect& bounds) override;
    bool on_mouse_button(int lx, int ly, miqu::MouseButton button, bool pressed, const miqu::Rect& bounds) override;

private:
    std::string m_time_str = "12:00";
    std::string m_date_str = "01/01/2026";
    bool m_hovered = false;
    std::function<void()> m_on_open_calendar;
};

// Windows-style "Show Desktop" peek button on far right edge
class PeekButtonView : public miqu::View {
public:
    PeekButtonView();
    void draw(cairo_t* cr, const miqu::Rect& bounds) override;
    miqu::Size measure_size() const override;

    bool on_mouse_move(int lx, int ly, const miqu::Rect& bounds) override;
    bool on_mouse_button(int lx, int ly, miqu::MouseButton button, bool pressed, const miqu::Rect& bounds) override;

private:
    bool m_hovered = false;
    bool m_pressed = false;
};

} // namespace miqubar
