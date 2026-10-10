#include "sys_modules.hpp"
#include "../config/bar_config.hpp"
#include "miqutoolkit/core/config.hpp"
#include "miqutoolkit/system/window_manager.hpp"
#include <fstream>
#include <sstream>
#include <ctime>
#include <cmath>
#include <unistd.h>
#include <cstdlib>
#include <filesystem>
#include <iostream>

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
    set_flat(true);
    set_text_size(get_toolkit_font_size());
    set_padding(4, 2);
    set_icon("cpu");
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
    request_redraw();
}

// ==========================================
// Memory Module
// ==========================================
MemoryModuleView::MemoryModuleView() {
    set_flat(true);
    set_text_size(get_toolkit_font_size());
    set_padding(4, 2);
    set_icon("media-flash");
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
    request_redraw();
}

// ==========================================
// Battery Module
// ==========================================
BatteryModuleView::BatteryModuleView() {
    set_flat(true);
    set_text_size(get_toolkit_font_size());
    set_padding(4, 2);
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

    set_icon(m_charging ? "battery-charging" : "battery");
    set_text(std::to_string(m_capacity) + "%");
    request_redraw();
}

// ==========================================
// Volume Module
// ==========================================
VolumeModuleView::VolumeModuleView(std::function<void()> on_open_quick_settings)
    : m_on_open_quick_settings(std::move(on_open_quick_settings)) {
    set_flat(true);
    set_text_size(get_toolkit_font_size());
    set_padding(4, 2);
    set_on_click_listener(m_on_open_quick_settings);
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
                m_volume = static_cast<int>(v * 100.0f);
            }
            m_muted = (str.find("[MUTED]") != std::string::npos);
        }
        pclose(fp);
    }

    if (m_muted || m_volume == 0) {
        set_icon("audio-volume-muted");
        set_text("Mute");
    } else if (m_volume < 33) {
        set_icon("audio-volume-low");
        set_text(std::to_string(m_volume) + "%");
    } else if (m_volume < 66) {
        set_icon("audio-volume-medium");
        set_text(std::to_string(m_volume) + "%");
    } else {
        set_icon("audio-volume-high");
        set_text(std::to_string(m_volume) + "%");
    }
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
        if (delta > 0) {
            std::system("wpctl set-volume @DEFAULT_AUDIO_SINK@ 5%+ 2>/dev/null || pactl set-sink-volume @DEFAULT_SINK@ +5% 2>/dev/null");
        } else {
            std::system("wpctl set-volume @DEFAULT_AUDIO_SINK@ 5%- 2>/dev/null || pactl set-sink-volume @DEFAULT_SINK@ -5% 2>/dev/null");
        }
        update_metrics();
        return true;
    }
    return false;
}

// ==========================================
// Clock Module
// ==========================================
ClockModuleView::ClockModuleView(std::function<void()> on_open_calendar)
    : m_on_open_calendar(std::move(on_open_calendar)) {
    set_flat(true);
    set_text_size(get_toolkit_font_size());
    set_padding(8, 2);
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

    if (cfg.clock_stacked) {
        set_text(m_time_str + "\n" + m_date_str);
    } else {
        set_text(m_time_str + "  " + m_date_str);
    }
    request_redraw();
}

// ==========================================
// Peek Button ("Show Desktop")
// ==========================================
PeekButtonView::PeekButtonView() {
    set_flat(true);
    set_padding(2, 0);
    set_layout_params(miqu::LayoutParams(10, BarConfig::get().height));
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
