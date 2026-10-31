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

// Network Status Button (Wi-Fi / Ethernet)
class NetworkModuleView : public miqu::Button {
public:
    explicit NetworkModuleView(std::function<void()> on_open_network);
    void update_metrics();

    bool is_connected() const { return m_connected; }
    const std::string& get_ssid() const { return m_ssid; }

private:
    bool m_connected = false;
    bool m_is_wifi = false;
    int m_signal_strength = 0; // 0 - 100
    std::string m_ssid;
    std::function<void()> m_on_open_network;
};

// Bluetooth Status Button
class BluetoothModuleView : public miqu::Button {
public:
    explicit BluetoothModuleView(std::function<void()> on_open_bluetooth);
    void update_metrics();

    bool is_powered() const { return m_powered; }
    bool is_connected() const { return m_connected; }
    const std::string& get_connected_device() const { return m_connected_device; }

private:
    bool m_powered = false;
    bool m_connected = false;
    std::string m_connected_device;
    std::function<void()> m_on_open_bluetooth;
};

// Volume Control Button
class VolumeModuleView : public miqu::Button {
public:
    explicit VolumeModuleView(std::function<void()> on_open_sound);
    void update_metrics();

    bool on_mouse_button(int lx, int ly, miqu::MouseButton button, bool pressed, const miqu::Rect& bounds) override;
    bool on_scroll(double delta) override;

    int get_volume() const { return m_volume; }
    bool is_muted() const { return m_muted; }

private:
    int m_volume = 70;
    bool m_muted = false;
    std::function<void()> m_on_open_sound;
};

// Brightness Control Button
class BrightnessModuleView : public miqu::Button {
public:
    explicit BrightnessModuleView(std::function<void()> on_open_brightness);
    void update_metrics();

    bool on_scroll(double delta) override;

    int get_brightness() const { return m_brightness; }
    void set_brightness(int pct);

private:
    int m_brightness = 100;
    std::string m_backlight_path;
    std::function<void()> m_on_open_brightness;
};

// Dark Mode Toggle Button (Triggers miqutheme headless toggle)
class DarkModeModuleView : public miqu::Button {
public:
    DarkModeModuleView();
    void sync_theme_state();

    bool is_dark() const { return m_is_dark; }

private:
    void toggle_mode();

    bool m_is_dark = true;
};

// Notification Status & History Toggle Button
class NotificationModuleView : public miqu::Button {
public:
    NotificationModuleView();
    void update_metrics();

    bool on_mouse_button(int lx, int ly, miqu::MouseButton button, bool pressed, const miqu::Rect& bounds) override;

    int get_unread_count() const { return m_unread_count; }
    bool is_dnd() const { return m_dnd; }

private:
    int m_unread_count = 0;
    bool m_dnd = false;
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

// Power & Session Management Button (launches miqupower)
class PowerButtonView : public miqu::Button {
public:
    PowerButtonView();
};

// Windows-style "Show Desktop" peek button on far right edge
class PeekButtonView : public miqu::Button {
public:
    PeekButtonView();
};

} // namespace miqubar
