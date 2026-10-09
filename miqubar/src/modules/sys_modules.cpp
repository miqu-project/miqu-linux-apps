#include "sys_modules.hpp"
#include "../config/bar_config.hpp"
#include "miqutoolkit/system/window_manager.hpp"
#include <pango/pangocairo.h>
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

// ==========================================
// CPU Module
// ==========================================
CpuModuleView::CpuModuleView() {
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
}

miqu::Size CpuModuleView::measure_size() const {
    return {64, BarConfig::get().height};
}

void CpuModuleView::draw(cairo_t* cr, const miqu::Rect& bounds) {
    if (!cr) return;
    const auto& cfg = BarConfig::get();

    // CPU icon glyph (small chip)
    double ix = bounds.x + 6;
    double iy = bounds.y + (bounds.height - 14) / 2.0;

    cairo_set_source_rgb(cr, 0.0, 0.90, 1.0); // #00e5ff
    cairo_set_line_width(cr, 1.2);
    cairo_rectangle(cr, ix, iy, 12, 12);
    cairo_stroke(cr);
    cairo_rectangle(cr, ix + 3, iy + 3, 6, 6);
    cairo_fill(cr);

    // Text: XX%
    std::string text = std::to_string(m_cpu_pct) + "%";
    PangoLayout* layout = pango_cairo_create_layout(cr);
    PangoFontDescription* desc = pango_font_description_from_string((cfg.font_family + " 10").c_str());
    pango_layout_set_font_description(layout, desc);
    pango_layout_set_text(layout, text.c_str(), -1);

    int lw, lh;
    pango_layout_get_pixel_size(layout, &lw, &lh);
    cairo_move_to(cr, ix + 16, bounds.y + (bounds.height - lh) / 2.0);
    cairo_set_source_rgba(cr, 0.88, 0.92, 0.96, 0.9);
    pango_cairo_show_layout(cr, layout);

    pango_font_description_free(desc);
    g_object_unref(layout);
}

// ==========================================
// Memory Module
// ==========================================
MemoryModuleView::MemoryModuleView() {
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
}

miqu::Size MemoryModuleView::measure_size() const {
    return {64, BarConfig::get().height};
}

void MemoryModuleView::draw(cairo_t* cr, const miqu::Rect& bounds) {
    if (!cr) return;
    const auto& cfg = BarConfig::get();

    double ix = bounds.x + 6;
    double iy = bounds.y + (bounds.height - 14) / 2.0;

    cairo_set_source_rgb(cr, 0.65, 0.33, 0.97); // #a855f7
    cairo_set_line_width(cr, 1.2);
    cairo_rectangle(cr, ix, iy + 2, 14, 10);
    cairo_stroke(cr);
    cairo_line_to(cr, ix + 4, iy + 12);
    cairo_line_to(cr, ix + 4, iy + 14);
    cairo_stroke(cr);

    std::string text = std::to_string(m_mem_pct) + "%";
    PangoLayout* layout = pango_cairo_create_layout(cr);
    PangoFontDescription* desc = pango_font_description_from_string((cfg.font_family + " 10").c_str());
    pango_layout_set_font_description(layout, desc);
    pango_layout_set_text(layout, text.c_str(), -1);

    int lw, lh;
    pango_layout_get_pixel_size(layout, &lw, &lh);
    cairo_move_to(cr, ix + 18, bounds.y + (bounds.height - lh) / 2.0);
    cairo_set_source_rgba(cr, 0.88, 0.92, 0.96, 0.9);
    pango_cairo_show_layout(cr, layout);

    pango_font_description_free(desc);
    g_object_unref(layout);
}

// ==========================================
// Battery Module
// ==========================================
BatteryModuleView::BatteryModuleView() {
    update_metrics();
}

void BatteryModuleView::update_metrics() {
    std::string path = "/sys/class/power_supply/BAT0";
    if (!fs::exists(path)) path = "/sys/class/power_supply/BAT1";

    if (!fs::exists(path)) {
        m_present = false;
        return;
    }

    m_present = true;
    std::ifstream cap_file(path + "/capacity");
    if (cap_file.is_open()) cap_file >> m_capacity;

    std::ifstream stat_file(path + "/status");
    if (stat_file.is_open()) {
        std::string stat;
        stat_file >> stat;
        m_charging = (stat == "Charging");
    }
}

miqu::Size BatteryModuleView::measure_size() const {
    if (!m_present) return {0, 0};
    return {54, BarConfig::get().height};
}

void BatteryModuleView::draw(cairo_t* cr, const miqu::Rect& bounds) {
    if (!cr || !m_present) return;
    const auto& cfg = BarConfig::get();

    double ix = bounds.x + 4;
    double iy = bounds.y + (bounds.height - 12) / 2.0;

    // Battery outline
    cairo_set_line_width(cr, 1.2);
    if (m_charging) {
        cairo_set_source_rgb(cr, 0.13, 0.77, 0.37); // Green #22c55e
    } else {
        cairo_set_source_rgba(cr, 0.88, 0.92, 0.96, 0.8);
    }
    cairo_rectangle(cr, ix, iy, 14, 11);
    cairo_stroke(cr);
    cairo_rectangle(cr, ix + 14, iy + 3, 2, 5); // terminal
    cairo_fill(cr);

    // Fill level
    double fill_w = std::max(1.0, 10.0 * (m_capacity / 100.0));
    cairo_rectangle(cr, ix + 2, iy + 2, fill_w, 7);
    cairo_fill(cr);

    // Percentage text
    std::string text = std::to_string(m_capacity) + "%";
    PangoLayout* layout = pango_cairo_create_layout(cr);
    PangoFontDescription* desc = pango_font_description_from_string((cfg.font_family + " 9").c_str());
    pango_layout_set_font_description(layout, desc);
    pango_layout_set_text(layout, text.c_str(), -1);

    int lw, lh;
    pango_layout_get_pixel_size(layout, &lw, &lh);
    cairo_move_to(cr, ix + 20, bounds.y + (bounds.height - lh) / 2.0);
    cairo_set_source_rgba(cr, 0.88, 0.92, 0.96, 0.85);
    pango_cairo_show_layout(cr, layout);

    pango_font_description_free(desc);
    g_object_unref(layout);
}

// ==========================================
// Volume Module
// ==========================================
VolumeModuleView::VolumeModuleView(std::function<void()> on_open_quick_settings)
    : m_on_open_quick_settings(std::move(on_open_quick_settings)) {
    update_metrics();
}

void VolumeModuleView::update_metrics() {
    // Attempt reading via pactl or wpctl
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
}

miqu::Size VolumeModuleView::measure_size() const {
    return {54, BarConfig::get().height};
}

void VolumeModuleView::draw(cairo_t* cr, const miqu::Rect& bounds) {
    if (!cr) return;
    const auto& cfg = BarConfig::get();

    // Hover background
    if (m_hovered) {
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.08);
        cairo_rectangle(cr, bounds.x + 2, bounds.y + 4, bounds.width - 4, bounds.height - 8);
        cairo_fill(cr);
    }

    double ix = bounds.x + 6;
    double iy = bounds.y + (bounds.height - 12) / 2.0;

    // Speaker glyph
    if (m_muted) {
        cairo_set_source_rgb(cr, 1.0, 0.35, 0.45); // Red when muted
    } else {
        cairo_set_source_rgb(cr, 0.92, 0.29, 0.60); // Pink/magenta #ec4899
    }

    cairo_new_path(cr);
    cairo_move_to(cr, ix, iy + 3);
    cairo_line_to(cr, ix + 4, iy + 3);
    cairo_line_to(cr, ix + 8, iy);
    cairo_line_to(cr, ix + 8, iy + 12);
    cairo_line_to(cr, ix + 4, iy + 9);
    cairo_line_to(cr, ix, iy + 9);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Sound waves if unmuted
    if (!m_muted && m_volume > 0) {
        cairo_set_line_width(cr, 1.2);
        cairo_arc(cr, ix + 7, iy + 6, 4, -M_PI / 3, M_PI / 3);
        cairo_stroke(cr);
        if (m_volume > 50) {
            cairo_arc(cr, ix + 7, iy + 6, 7, -M_PI / 3, M_PI / 3);
            cairo_stroke(cr);
        }
    }

    std::string text = m_muted ? "Mute" : std::to_string(m_volume) + "%";
    PangoLayout* layout = pango_cairo_create_layout(cr);
    PangoFontDescription* desc = pango_font_description_from_string((cfg.font_family + " 9").c_str());
    pango_layout_set_font_description(layout, desc);
    pango_layout_set_text(layout, text.c_str(), -1);

    int lw, lh;
    pango_layout_get_pixel_size(layout, &lw, &lh);
    cairo_move_to(cr, ix + 18, bounds.y + (bounds.height - lh) / 2.0);
    cairo_set_source_rgba(cr, 0.88, 0.92, 0.96, 0.85);
    pango_cairo_show_layout(cr, layout);

    pango_font_description_free(desc);
    g_object_unref(layout);
}

bool VolumeModuleView::on_mouse_move(int lx, int ly, const miqu::Rect& bounds) {
    bool hov = bounds.contains(lx, ly);
    if (hov != m_hovered) {
        m_hovered = hov;
        request_redraw();
    }
    return hov;
}

bool VolumeModuleView::on_mouse_button(int, int, miqu::MouseButton button, bool pressed, const miqu::Rect&) {
    if (!pressed) return false;

    if (button == miqu::MouseButton::Left) {
        if (m_on_open_quick_settings) m_on_open_quick_settings();
        return true;
    } else if (button == miqu::MouseButton::Middle) {
        // Toggle mute
        std::system("wpctl set-mute @DEFAULT_AUDIO_SINK@ toggle 2>/dev/null || pactl set-sink-mute @DEFAULT_SINK@ toggle 2>/dev/null");
        update_metrics();
        request_redraw();
        return true;
    }
    return false;
}

bool VolumeModuleView::on_scroll(double delta) {
    if (std::abs(delta) > 0.05) {
        if (delta > 0) {
            std::system("wpctl set-volume @DEFAULT_AUDIO_SINK@ 5%+ 2>/dev/null || pactl set-sink-volume @DEFAULT_SINK@ +5% 2>/dev/null");
        } else {
            std::system("wpctl set-volume @DEFAULT_AUDIO_SINK@ 5%- 2>/dev/null || pactl set-sink-volume @DEFAULT_SINK@ -5% 2>/dev/null");
        }
        update_metrics();
        request_redraw();
        return true;
    }
    return false;
}

// ==========================================
// Clock Module
// ==========================================
ClockModuleView::ClockModuleView(std::function<void()> on_open_calendar)
    : m_on_open_calendar(std::move(on_open_calendar)) {
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
}

miqu::Size ClockModuleView::measure_size() const {
    return {74, BarConfig::get().height};
}

void ClockModuleView::draw(cairo_t* cr, const miqu::Rect& bounds) {
    if (!cr) return;
    const auto& cfg = BarConfig::get();

    if (m_hovered) {
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.08);
        cairo_rectangle(cr, bounds.x + 2, bounds.y + 4, bounds.width - 4, bounds.height - 8);
        cairo_fill(cr);
    }

    if (cfg.clock_stacked) {
        // Stacked 2-row layout (Windows 10/11)
        // Row 1: Time
        PangoLayout* t_layout = pango_cairo_create_layout(cr);
        PangoFontDescription* t_desc = pango_font_description_from_string((cfg.font_family + " Bold 10").c_str());
        pango_layout_set_font_description(t_layout, t_desc);
        pango_layout_set_text(t_layout, m_time_str.c_str(), -1);

        int tw, th;
        pango_layout_get_pixel_size(t_layout, &tw, &th);
        cairo_move_to(cr, bounds.x + (bounds.width - tw) / 2.0, bounds.y + 5);
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        pango_cairo_show_layout(cr, t_layout);

        pango_font_description_free(t_desc);
        g_object_unref(t_layout);

        // Row 2: Date
        PangoLayout* d_layout = pango_cairo_create_layout(cr);
        PangoFontDescription* d_desc = pango_font_description_from_string((cfg.font_family + " 9").c_str());
        pango_layout_set_font_description(d_layout, d_desc);
        pango_layout_set_text(d_layout, m_date_str.c_str(), -1);

        int dw, dh;
        pango_layout_get_pixel_size(d_layout, &dw, &dh);
        cairo_move_to(cr, bounds.x + (bounds.width - dw) / 2.0, bounds.y + bounds.height - dh - 5);
        cairo_set_source_rgba(cr, 0.85, 0.90, 0.95, 0.7);
        pango_cairo_show_layout(cr, d_layout);

        pango_font_description_free(d_desc);
        g_object_unref(d_layout);
    } else {
        std::string full = m_time_str + "  " + m_date_str;
        PangoLayout* layout = pango_cairo_create_layout(cr);
        PangoFontDescription* desc = pango_font_description_from_string((cfg.font_family + " 10").c_str());
        pango_layout_set_font_description(layout, desc);
        pango_layout_set_text(layout, full.c_str(), -1);

        int lw, lh;
        pango_layout_get_pixel_size(layout, &lw, &lh);
        cairo_move_to(cr, bounds.x + (bounds.width - lw) / 2.0, bounds.y + (bounds.height - lh) / 2.0);
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        pango_cairo_show_layout(cr, layout);

        pango_font_description_free(desc);
        g_object_unref(layout);
    }
}

bool ClockModuleView::on_mouse_move(int lx, int ly, const miqu::Rect& bounds) {
    bool hov = bounds.contains(lx, ly);
    if (hov != m_hovered) {
        m_hovered = hov;
        request_redraw();
    }
    return hov;
}

bool ClockModuleView::on_mouse_button(int, int, miqu::MouseButton button, bool pressed, const miqu::Rect&) {
    if (button == miqu::MouseButton::Left && pressed) {
        if (m_on_open_calendar) m_on_open_calendar();
        return true;
    }
    return false;
}

// ==========================================
// Peek Button ("Show Desktop")
// ==========================================
PeekButtonView::PeekButtonView() = default;

miqu::Size PeekButtonView::measure_size() const {
    return {12, BarConfig::get().height};
}

void PeekButtonView::draw(cairo_t* cr, const miqu::Rect& bounds) {
    if (!cr) return;

    if (m_pressed) {
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.25);
        cairo_rectangle(cr, bounds.x, bounds.y, bounds.width, bounds.height);
        cairo_fill(cr);
    } else if (m_hovered) {
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.14);
        cairo_rectangle(cr, bounds.x, bounds.y, bounds.width, bounds.height);
        cairo_fill(cr);
    }

    // Border line on left of peek button
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.15);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, bounds.x, bounds.y + 8);
    cairo_line_to(cr, bounds.x, bounds.y + bounds.height - 8);
    cairo_stroke(cr);
}

bool PeekButtonView::on_mouse_move(int lx, int ly, const miqu::Rect& bounds) {
    bool hov = bounds.contains(lx, ly);
    if (hov != m_hovered) {
        m_hovered = hov;
        request_redraw();
    }
    return hov;
}

bool PeekButtonView::on_mouse_button(int, int, miqu::MouseButton button, bool pressed, const miqu::Rect&) {
    if (button != miqu::MouseButton::Left) return false;

    if (pressed) {
        m_pressed = true;
        request_redraw();
        return true;
    } else {
        if (m_pressed) {
            m_pressed = false;
            // Minimize all windows / Toggle show desktop
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
            request_redraw();
            return true;
        }
    }
    return false;
}

} // namespace miqubar
