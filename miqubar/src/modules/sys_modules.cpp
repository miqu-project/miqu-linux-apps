#include "sys_modules.hpp"
#include "../config/bar_config.hpp"
#include "miqutoolkit/core/config.hpp"
#include "miqutoolkit/system/window_manager.hpp"
#include "miqutoolkit/system/app_manager.hpp"
#include <fstream>
#include <sstream>
#include <ctime>
#include <cmath>
#include <unistd.h>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <pango/pangocairo.h>

namespace miqubar {

namespace fs = std::filesystem;

static int get_toolkit_font_size() {
    auto config = miqu::Config::get();
    return config->metrics.font_size > 0 ? config->metrics.font_size : 11;
}

// ==========================================
// CPU Module
// ==========================================
CpuModuleView::CpuModuleView() {
    const auto& cfg = BarConfig::get();
    int item_h = cfg.get_item_height();

    set_flat(true);
    set_text_size(get_toolkit_font_size());
    set_padding(4, 2);
    set_icon("cpu");
    set_layout_params(miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::WrapContent), item_h));
    update_metrics();
}

void CpuModuleView::update_metrics() {
    std::ifstream file("/proc/stat");
    if (!file.is_open()) return;

    std::string cpu;
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
    if (file >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal) {
        unsigned long long current_idle = idle + iowait;
        unsigned long long current_total = user + nice + system + idle + iowait + irq + softirq + steal;

        if (m_prev_total > 0 && current_total > m_prev_total) {
            unsigned long long total_diff = current_total - m_prev_total;
            unsigned long long idle_diff = current_idle - m_prev_idle;
            m_cpu_pct = static_cast<int>(100.0 * (total_diff - idle_diff) / total_diff);
        }

        m_prev_idle = current_idle;
        m_prev_total = current_total;
    }
    set_text(std::to_string(m_cpu_pct) + "%");
    set_tooltip("CPU: " + std::to_string(m_cpu_pct) + "%");
    request_redraw();
}

// ==========================================
// Memory Module
// ==========================================
MemoryModuleView::MemoryModuleView() {
    const auto& cfg = BarConfig::get();
    int item_h = cfg.get_item_height();

    set_flat(true);
    set_text_size(get_toolkit_font_size());
    set_padding(4, 2);
    set_icon("media-flash");
    set_layout_params(miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::WrapContent), item_h));
    update_metrics();
}

void MemoryModuleView::update_metrics() {
    std::ifstream file("/proc/meminfo");
    if (!file.is_open()) return;

    std::string key;
    unsigned long long val;
    std::string unit;
    unsigned long long total = 0, avail = 0;

    while (file >> key >> val >> unit) {
        if (key == "MemTotal:") total = val;
        else if (key == "MemAvailable:") {
            avail = val;
            break;
        }
    }

    if (total > 0) {
        m_mem_pct = static_cast<int>(100.0 * (total - avail) / total);
    }
    set_text(std::to_string(m_mem_pct) + "%");
    set_tooltip("Memory: " + std::to_string(m_mem_pct) + "%");
    request_redraw();
}

// ==========================================
// Battery Module
// ==========================================
BatteryModuleView::BatteryModuleView() {
    const auto& cfg = BarConfig::get();
    int item_h = cfg.get_item_height();

    set_flat(true);
    set_text_size(get_toolkit_font_size());
    set_icon_size(20);
    set_padding(6, 2);
    set_layout_params(miqu::LayoutParams(item_h, item_h));
    set_text(""); // icon-only
    update_metrics();
}

void BatteryModuleView::update_metrics() {
    std::string path = "/sys/class/power_supply/BAT0";
    if (!fs::exists(path)) path = "/sys/class/power_supply/BAT1";

    if (!fs::exists(path)) {
        m_present = false;
        set_visibility(miqu::Visibility::Gone);
        return;
    }

    m_present = true;
    set_visibility(miqu::Visibility::Visible);

    std::ifstream cap_file(path + "/capacity");
    if (cap_file.is_open()) cap_file >> m_capacity;

    std::ifstream stat_file(path + "/status");
    if (stat_file.is_open()) {
        std::string stat;
        stat_file >> stat;
        m_charging = (stat == "Charging");
    }

    // Dynamic filling battery icon
    if (m_charging) {
        if (m_capacity < 15) {
            set_icon("battery-empty-charging");
        } else if (m_capacity < 35) {
            set_icon("battery-low-charging");
        } else if (m_capacity < 75) {
            set_icon("battery-good-charging");
        } else {
            set_icon("battery-full-charging");
        }
    } else {
        if (m_capacity < 10) {
            set_icon("battery-empty");
        } else if (m_capacity < 25) {
            set_icon("battery-caution");
        } else if (m_capacity < 45) {
            set_icon("battery-low");
        } else if (m_capacity < 80) {
            set_icon("battery-good");
        } else {
            set_icon("battery-full");
        }
    }

    if (!m_present) {
        set_tooltip("No Battery");
    } else {
        set_tooltip("Battery: " + std::to_string(m_capacity) + "%" + (m_charging ? " (Charging)" : ""));
    }

    request_redraw();
}

// ==========================================
// Network Module (Wi-Fi / Ethernet)
// ==========================================
NetworkModuleView::NetworkModuleView(std::function<void()> on_open_network)
    : m_on_open_network(std::move(on_open_network)) {
    const auto& cfg = BarConfig::get();
    int item_h = cfg.get_item_height();

    set_flat(true);
    set_text_size(get_toolkit_font_size());
    set_icon_size(20);
    set_padding(6, 2);
    set_layout_params(miqu::LayoutParams(item_h, item_h));
    set_text(""); // icon-only
    set_on_click_listener(m_on_open_network);
    update_metrics();
}

void NetworkModuleView::update_metrics() {
    m_connected = false;
    m_is_wifi = false;
    m_ssid.clear();
    m_signal_strength = 0;

    FILE* fp = popen("nmcli -t -f TYPE,STATE,CONNECTION dev 2>/dev/null", "r");
    if (fp) {
        char buf[256];
        while (fgets(buf, sizeof(buf), fp)) {
            std::string line(buf);
            if (!line.empty() && line.back() == '\n') line.pop_back();

            // e.g. "wifi:connected:MyWiFi" or "ethernet:connected:Wired"
            size_t p1 = line.find(':');
            if (p1 == std::string::npos) continue;
            std::string type = line.substr(0, p1);
            size_t p2 = line.find(':', p1 + 1);
            if (p2 == std::string::npos) continue;
            std::string state = line.substr(p1 + 1, p2 - p1 - 1);
            std::string conn = line.substr(p2 + 1);

            if (state == "connected") {
                m_connected = true;
                m_ssid = conn;
                if (type == "wifi") {
                    m_is_wifi = true;
                    break;
                } else if (type == "ethernet") {
                    m_is_wifi = false;
                    break;
                }
            }
        }
        pclose(fp);
    }

    if (m_connected) {
        if (m_is_wifi) {
            set_icon("network-wireless");
            set_tooltip(!m_ssid.empty() ? ("Wi-Fi: " + m_ssid) : "Wi-Fi: Connected");
        } else {
            set_icon("network-wired");
            set_tooltip(!m_ssid.empty() ? ("Ethernet: " + m_ssid) : "Ethernet: Connected");
        }
    } else {
        set_icon("network-wireless-disconnected");
        set_tooltip("Network: Disconnected");
    }

    request_redraw();
}

// ==========================================
// Bluetooth Module
// ==========================================
BluetoothModuleView::BluetoothModuleView(std::function<void()> on_open_bluetooth)
    : m_on_open_bluetooth(std::move(on_open_bluetooth)) {
    const auto& cfg = BarConfig::get();
    int item_h = cfg.get_item_height();

    set_flat(true);
    set_text_size(get_toolkit_font_size());
    set_icon_size(20);
    set_padding(6, 2);
    set_layout_params(miqu::LayoutParams(item_h, item_h));
    set_text(""); // icon-only
    set_on_click_listener(m_on_open_bluetooth);
    update_metrics();
}

void BluetoothModuleView::update_metrics() {
    m_powered = false;
    m_connected = false;
    m_connected_device.clear();

    FILE* fp = popen("bluetoothctl show 2>/dev/null", "r");
    if (fp) {
        char buf[256];
        while (fgets(buf, sizeof(buf), fp)) {
            std::string line(buf);
            if (line.find("Powered: yes") != std::string::npos) {
                m_powered = true;
                break;
            }
        }
        pclose(fp);
    }

    if (m_powered) {
        FILE* dev_fp = popen("bluetoothctl devices Connected 2>/dev/null", "r");
        if (dev_fp) {
            char buf[256];
            if (fgets(buf, sizeof(buf), dev_fp)) {
                std::string line(buf);
                if (!line.empty() && line.back() == '\n') line.pop_back();
                // Device XX:XX:XX:XX:XX:XX DeviceName
                size_t p = line.find("Device ");
                if (p != std::string::npos) {
                    size_t p2 = line.find(' ', p + 7);
                    if (p2 != std::string::npos) {
                        m_connected_device = line.substr(p2 + 1);
                        m_connected = true;
                    }
                }
            }
            pclose(dev_fp);
        }
    }

    if (!m_powered) {
        set_icon("bluetooth-disabled");
        set_tooltip("Bluetooth: Off");
    } else if (m_connected) {
        set_icon("bluetooth-active");
        set_tooltip(!m_connected_device.empty() ? ("Bluetooth: " + m_connected_device) : "Bluetooth: Connected");
    } else {
        set_icon("bluetooth");
        set_tooltip("Bluetooth: On");
    }

    request_redraw();
}

// ==========================================
// Volume Module
// ==========================================
VolumeModuleView::VolumeModuleView(std::function<void()> on_open_sound)
    : m_on_open_sound(std::move(on_open_sound)) {
    const auto& cfg = BarConfig::get();
    int item_h = cfg.get_item_height();

    set_flat(true);
    set_text_size(get_toolkit_font_size());
    set_icon_size(20);
    set_padding(6, 2);
    set_layout_params(miqu::LayoutParams(item_h, item_h));
    set_text(""); // icon-only
    set_on_click_listener(m_on_open_sound);
    update_metrics();
}

void VolumeModuleView::update_metrics() {
    FILE* fp = popen("wpctl get-volume @DEFAULT_AUDIO_SINK@ 2>/dev/null", "r");
    if (fp) {
        char buf[64];
        if (fgets(buf, sizeof(buf), fp)) {
            std::string str(buf);
            size_t pos = str.find("Volume:");
            if (pos != std::string::npos) {
                float v = std::stof(str.substr(pos + 7));
                m_volume = static_cast<int>(std::round(v * 100.0f));
            }
            m_muted = (str.find("[MUTED]") != std::string::npos);
        }
        pclose(fp);
    }

    if (m_muted || m_volume == 0) {
        set_icon("audio-volume-muted");
    } else if (m_volume < 33) {
        set_icon("audio-volume-low");
    } else if (m_volume < 67) {
        set_icon("audio-volume-medium");
    } else {
        set_icon("audio-volume-high");
    }

    set_tooltip(m_muted ? "Volume: Muted" : ("Volume: " + std::to_string(m_volume) + "%"));
    request_redraw();
}

bool VolumeModuleView::on_mouse_button(int lx, int ly, miqu::MouseButton button, bool pressed, const miqu::Rect& bounds) {
    if (pressed && button == miqu::MouseButton::Middle) {
        std::system("wpctl set-mute @DEFAULT_AUDIO_SINK@ toggle 2>/dev/null || pactl set-sink-mute @DEFAULT_SINK@ toggle 2>/dev/null");
        update_metrics();
        return true;
    }
    return miqu::Button::on_mouse_button(lx, ly, button, pressed, bounds);
}

bool VolumeModuleView::on_scroll(double delta) {
    if (std::abs(delta) > 0.05) {
        int diff = (delta < 0) ? 5 : -5;
        int next_vol = std::clamp(m_volume + diff, 0, 100);
        std::string cmd = "wpctl set-volume -l 1.0 @DEFAULT_AUDIO_SINK@ " + std::to_string(next_vol / 100.0f) + " 2>/dev/null";
        std::system(cmd.c_str());
        update_metrics();
        return true;
    }
    return false;
}

// ==========================================
// Brightness Module
// ==========================================
BrightnessModuleView::BrightnessModuleView(std::function<void()> on_open_brightness)
    : m_on_open_brightness(std::move(on_open_brightness)) {
    const auto& cfg = BarConfig::get();
    int item_h = cfg.get_item_height();

    set_flat(true);
    set_text_size(get_toolkit_font_size());
    set_icon_size(20);
    set_padding(6, 2);
    set_layout_params(miqu::LayoutParams(item_h, item_h));
    set_text(""); // icon-only by default
    set_on_click_listener(m_on_open_brightness);
    update_metrics();
}

void BrightnessModuleView::update_metrics() {
    // 1. Try reading directly from sysfs backlight
    const fs::path sysfs_backlight("/sys/class/backlight");
    bool found_sysfs = false;
    if (fs::exists(sysfs_backlight) && fs::is_directory(sysfs_backlight)) {
        for (const auto& entry : fs::directory_iterator(sysfs_backlight)) {
            if (entry.is_directory() || entry.is_symlink()) {
                fs::path cur_p = entry.path() / "brightness";
                fs::path max_p = entry.path() / "max_brightness";
                if (fs::exists(cur_p) && fs::exists(max_p)) {
                    std::ifstream cur_f(cur_p);
                    std::ifstream max_f(max_p);
                    long cur_val = 0, max_val = 0;
                    if (cur_f >> cur_val && max_f >> max_val && max_val > 0) {
                        m_backlight_path = entry.path().string();
                        m_brightness = std::clamp(static_cast<int>(std::round((cur_val * 100.0) / max_val)), 0, 100);
                        found_sysfs = true;
                        break;
                    }
                }
            }
        }
    }

    // 2. Fallback to brightnessctl -m
    if (!found_sysfs) {
        FILE* fp = popen("brightnessctl -m 2>/dev/null", "r");
        if (fp) {
            char buf[256];
            if (fgets(buf, sizeof(buf), fp)) {
                std::string line(buf);
                size_t p = line.find('%');
                if (p != std::string::npos) {
                    size_t start = line.rfind(',', p);
                    if (start != std::string::npos) {
                        try {
                            m_brightness = std::clamp(std::stoi(line.substr(start + 1, p - start - 1)), 0, 100);
                        } catch (...) {}
                    }
                }
            }
            pclose(fp);
        }
    }

    if (m_brightness < 30) {
        set_icon("notification-display-brightness-low");
    } else if (m_brightness < 70) {
        set_icon("notification-display-brightness-medium");
    } else if (m_brightness < 95) {
        set_icon("notification-display-brightness-high");
    } else {
        set_icon("notification-display-brightness-full");
    }

    set_tooltip("Brightness: " + std::to_string(m_brightness) + "%");
    request_redraw();
}

void BrightnessModuleView::set_brightness(int pct) {
    m_brightness = std::clamp(pct, 5, 100);
    std::string cmd = "brightnessctl set " + std::to_string(m_brightness) + "% -q 2>/dev/null &";
    std::system(cmd.c_str());

    if (m_brightness < 30) {
        set_icon("notification-display-brightness-low");
    } else if (m_brightness < 70) {
        set_icon("notification-display-brightness-medium");
    } else if (m_brightness < 95) {
        set_icon("notification-display-brightness-high");
    } else {
        set_icon("notification-display-brightness-full");
    }

    set_tooltip("Brightness: " + std::to_string(m_brightness) + "%");
    request_redraw();
}

bool BrightnessModuleView::on_scroll(double delta) {
    if (std::abs(delta) > 0.05) {
        int diff = (delta < 0) ? 5 : -5;
        set_brightness(m_brightness + diff);
        return true;
    }
    return false;
}

// ==========================================
// Dark Mode Toggle Button
// ==========================================
DarkModeModuleView::DarkModeModuleView() {
    const auto& cfg = BarConfig::get();
    int item_h = cfg.get_item_height();

    set_flat(true);
    set_text_size(get_toolkit_font_size());
    set_icon_size(20);
    set_padding(6, 2);
    set_layout_params(miqu::LayoutParams(item_h, item_h));
    set_text(""); // icon-only by default
    set_on_click_listener([this]() {
        toggle_mode();
    });
    sync_theme_state();
}

void DarkModeModuleView::sync_theme_state() {
    // 1. Check user theme preference file from miqutheme
    const char* home = getenv("HOME");
    std::string mode_path = home ? std::string(home) + "/.config/miqutheme/mode" : ".config/miqutheme/mode";
    if (fs::exists(mode_path)) {
        std::ifstream f(mode_path);
        std::string mode_str;
        if (f >> mode_str) {
            m_is_dark = (mode_str != "light");
        }
    } else {
        // Fallback to active toolkit surface color luminance
        auto tk = miqu::Config::get();
        if (tk) {
            float lum = 0.299f * tk->colors.surface.r + 0.587f * tk->colors.surface.g + 0.114f * tk->colors.surface.b;
            m_is_dark = (lum < 0.5f);
        }
    }

    set_icon(m_is_dark ? "weather-clear-night" : "weather-clear");
    set_tooltip(m_is_dark ? "Dark Mode (Click to toggle)" : "Light Mode (Click to toggle)");
    request_redraw();
}

void DarkModeModuleView::toggle_mode() {
    // Launch headless toggle asynchronously
    std::system("miqutheme -t &");
    // Optimistically toggle icon state immediately for responsive feedback
    m_is_dark = !m_is_dark;
    set_icon(m_is_dark ? "weather-clear-night" : "weather-clear");
    set_tooltip(m_is_dark ? "Dark Mode (Click to toggle)" : "Light Mode (Click to toggle)");
    request_redraw();
}

// ==========================================
// Clock Module
// ==========================================
ClockModuleView::ClockModuleView(std::function<void()> on_open_calendar)
    : m_on_open_calendar(std::move(on_open_calendar)) {
    const auto& cfg = BarConfig::get();
    int item_h = cfg.get_item_height();

    set_flat(true);
    set_padding(8, 2);
    set_text_alignment(miqu::TextAlignment::Center);
    set_layout_params(miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::WrapContent), item_h));
    set_on_click_listener(m_on_open_calendar);
    update_time();
}

void ClockModuleView::update_time() {
    std::time_t t = std::time(nullptr);
    std::tm* tm = std::localtime(&t);
    if (!tm) return;

    char time_buf[32];
    char date_buf[32];
    const auto& cfg = BarConfig::get();

    std::strftime(time_buf, sizeof(time_buf), cfg.time_format.c_str(), tm);
    std::strftime(date_buf, sizeof(date_buf), cfg.date_format.c_str(), tm);

    m_time_str = time_buf;
    m_date_str = date_buf;

    std::string text = cfg.clock_stacked ? (m_time_str + "\n" + m_date_str)
                                         : (m_time_str + "  " + m_date_str);
    set_text(text);
    set_tooltip(m_date_str + "  " + m_time_str);
}


// ==========================================
// Notification Module
// ==========================================
NotificationModuleView::NotificationModuleView() {
    const auto& cfg = BarConfig::get();
    int item_h = cfg.get_item_height();

    set_flat(true);
    set_text_size(get_toolkit_font_size());
    set_padding(6, 2);
    set_icon_size(20);
    set_icon("bell");
    set_layout_params(miqu::LayoutParams(item_h, item_h));
    update_metrics();
}

void NotificationModuleView::update_metrics() {
    std::string state_path;
    const char* runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (runtime_dir && *runtime_dir) {
        state_path = std::string(runtime_dir) + "/miqunotify_state";
    } else {
        const char* home = getenv("HOME");
        if (home) {
            state_path = std::string(home) + "/.cache/miqunotify/state";
        }
    }

    int unread = 0;
    int dnd_int = 0;

    if (!state_path.empty() && fs::exists(state_path)) {
        std::ifstream file(state_path);
        if (file.is_open()) {
            file >> unread >> dnd_int;
        }
    }

    m_unread_count = std::max(0, unread);
    m_dnd = (dnd_int != 0);

    if (m_dnd) {
        set_icon("notifications-disabled");
        set_tooltip("Notifications: Do Not Disturb");
    } else {
        set_icon("bell");
        if (m_unread_count == 0) {
            set_tooltip("Notifications: None");
        } else {
            set_tooltip("Notifications: " + std::to_string(m_unread_count) + " unread");
        }
    }

    request_redraw();
}

bool NotificationModuleView::on_mouse_button(int lx, int ly, miqu::MouseButton button, bool pressed, const miqu::Rect& bounds) {
    if (pressed) {
        if (button == miqu::MouseButton::Left) {
            miqu::AppManager::launch_command("miqunotify -t");
            return true;
        } else if (button == miqu::MouseButton::Middle || button == miqu::MouseButton::Right) {
            miqu::AppManager::launch_command("miqunotify --dnd");
            return true;
        }
    }
    return miqu::Button::on_mouse_button(lx, ly, button, pressed, bounds);
}

// ==========================================
// Power Button (launches miqupower)
// ==========================================
PowerButtonView::PowerButtonView() {
    const auto& cfg = BarConfig::get();
    int item_h = cfg.get_item_height();

    set_flat(true);
    set_padding(6, 2);
    set_icon("system-shutdown-symbolic");
    set_icon_size(cfg.icon_size > 0 ? cfg.icon_size - 4 : 20);
    set_layout_params(miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::WrapContent), item_h));
    set_tooltip("Session & Power");
    set_on_click_listener([]() {
        miqu::AppManager::launch_command("miqupower");
    });
}

// ==========================================
// Peek Button ("Show Desktop")
// ==========================================
PeekButtonView::PeekButtonView() {
    const auto& cfg = BarConfig::get();
    int item_h = cfg.get_item_height();

    set_flat(true);
    set_padding(2, 0);
    set_layout_params(miqu::LayoutParams(8, item_h));
    set_tooltip("Show Desktop");
    set_on_click_listener([]() {
        auto mgr = miqu::WindowManager::get();
        if (mgr) {
            auto windows = mgr->get_windows();
            bool any_visible = false;
            for (const auto& w : windows) {
                if (!w.is_minimized) { any_visible = true; break; }
            }
            for (auto& w : windows) {
                w.set_minimized(any_visible);
            }
        }
    });
}

} // namespace miqubar
