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
    int item_h = std::max(28, cfg.height - 8);

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
    request_redraw();
}

// ==========================================
// Memory Module
// ==========================================
MemoryModuleView::MemoryModuleView() {
    const auto& cfg = BarConfig::get();
    int item_h = std::max(28, cfg.height - 8);

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
    request_redraw();
}

// ==========================================
// Battery Module
// ==========================================
BatteryModuleView::BatteryModuleView() {
    const auto& cfg = BarConfig::get();
    int item_h = std::max(28, cfg.height - 8);

    set_flat(true);
    set_text_size(get_toolkit_font_size());
    set_icon_size(20);
    set_padding(6, 2);
    set_layout_params(miqu::LayoutParams(item_h, item_h));
    set_text(""); // no percentage text
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

    const auto& cfg = BarConfig::get();
    int item_h = std::max(28, cfg.height - 8);
    if (m_is_hovered) {
        set_text(std::to_string(m_capacity) + "%");
        set_layout_params(miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::WrapContent), item_h));
    } else {
        set_text(""); // icon-only when not hovered
        set_layout_params(miqu::LayoutParams(item_h, item_h));
    }
    request_redraw();
}

bool BatteryModuleView::on_mouse_move(int lx, int ly, const miqu::Rect& bounds) {
    bool hovered = bounds.contains(miqu::Point(lx, ly));
    if (hovered != m_is_hovered) {
        m_is_hovered = hovered;
        const auto& cfg = BarConfig::get();
        int item_h = std::max(28, cfg.height - 8);
        if (m_is_hovered) {
            set_text(std::to_string(m_capacity) + "%");
            set_layout_params(miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::WrapContent), item_h));
        } else {
            set_text("");
            set_layout_params(miqu::LayoutParams(item_h, item_h));
        }
    }
    return miqu::Button::on_mouse_move(lx, ly, bounds);
}

// ==========================================
// Network Module (Wi-Fi / Ethernet)
// ==========================================
NetworkModuleView::NetworkModuleView(std::function<void()> on_open_network)
    : m_on_open_network(std::move(on_open_network)) {
    const auto& cfg = BarConfig::get();
    int item_h = std::max(28, cfg.height - 8);

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
        } else {
            set_icon("network-wired");
        }
    } else {
        set_icon("network-wireless-disconnected");
    }

    const auto& cfg = BarConfig::get();
    int item_h = std::max(28, cfg.height - 8);
    if (m_is_hovered) {
        set_text(m_connected ? (!m_ssid.empty() ? m_ssid : (m_is_wifi ? "Wi-Fi" : "Wired")) : "Disconnected");
        set_layout_params(miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::WrapContent), item_h));
    } else {
        set_text(""); // icon-only when not hovered
        set_layout_params(miqu::LayoutParams(item_h, item_h));
    }
    request_redraw();
}

bool NetworkModuleView::on_mouse_move(int lx, int ly, const miqu::Rect& bounds) {
    bool hovered = bounds.contains(miqu::Point(lx, ly));
    if (hovered != m_is_hovered) {
        m_is_hovered = hovered;
        const auto& cfg = BarConfig::get();
        int item_h = std::max(28, cfg.height - 8);
        if (m_is_hovered) {
            set_text(m_connected ? (!m_ssid.empty() ? m_ssid : (m_is_wifi ? "Wi-Fi" : "Wired")) : "Disconnected");
            set_layout_params(miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::WrapContent), item_h));
        } else {
            set_text("");
            set_layout_params(miqu::LayoutParams(item_h, item_h));
        }
    }
    return miqu::Button::on_mouse_move(lx, ly, bounds);
}

// ==========================================
// Bluetooth Module
// ==========================================
BluetoothModuleView::BluetoothModuleView(std::function<void()> on_open_bluetooth)
    : m_on_open_bluetooth(std::move(on_open_bluetooth)) {
    const auto& cfg = BarConfig::get();
    int item_h = std::max(28, cfg.height - 8);

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
    } else if (m_connected) {
        set_icon("bluetooth-active");
    } else {
        set_icon("bluetooth");
    }

    const auto& cfg = BarConfig::get();
    int item_h = std::max(28, cfg.height - 8);
    if (m_is_hovered) {
        if (!m_powered) {
            set_text("Off");
        } else if (m_connected) {
            set_text(!m_connected_device.empty() ? m_connected_device : "Connected");
        } else {
            set_text("On");
        }
        set_layout_params(miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::WrapContent), item_h));
    } else {
        set_text(""); // icon-only when not hovered
        set_layout_params(miqu::LayoutParams(item_h, item_h));
    }
    request_redraw();
}

bool BluetoothModuleView::on_mouse_move(int lx, int ly, const miqu::Rect& bounds) {
    bool hovered = bounds.contains(miqu::Point(lx, ly));
    if (hovered != m_is_hovered) {
        m_is_hovered = hovered;
        const auto& cfg = BarConfig::get();
        int item_h = std::max(28, cfg.height - 8);
        if (m_is_hovered) {
            if (!m_powered) {
                set_text("Off");
            } else if (m_connected) {
                set_text(!m_connected_device.empty() ? m_connected_device : "Connected");
            } else {
                set_text("On");
            }
            set_layout_params(miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::WrapContent), item_h));
        } else {
            set_text("");
            set_layout_params(miqu::LayoutParams(item_h, item_h));
        }
    }
    return miqu::Button::on_mouse_move(lx, ly, bounds);
}

// ==========================================
// Volume Module
// ==========================================
VolumeModuleView::VolumeModuleView(std::function<void()> on_open_sound)
    : m_on_open_sound(std::move(on_open_sound)) {
    const auto& cfg = BarConfig::get();
    int item_h = std::max(28, cfg.height - 8);

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
                m_volume = static_cast<int>(v * 100.0f);
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

    const auto& cfg = BarConfig::get();
    int item_h = std::max(28, cfg.height - 8);
    if (m_is_hovered) {
        set_text(m_muted ? "Muted" : (std::to_string(m_volume) + "%"));
        set_layout_params(miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::WrapContent), item_h));
    } else {
        set_text(""); // icon-only when not hovered
        set_layout_params(miqu::LayoutParams(item_h, item_h));
    }
    request_redraw();
}

bool VolumeModuleView::on_mouse_move(int lx, int ly, const miqu::Rect& bounds) {
    bool hovered = bounds.contains(miqu::Point(lx, ly));
    if (hovered != m_is_hovered) {
        m_is_hovered = hovered;
        const auto& cfg = BarConfig::get();
        int item_h = std::max(28, cfg.height - 8);
        if (m_is_hovered) {
            set_text(m_muted ? "Muted" : (std::to_string(m_volume) + "%"));
            set_layout_params(miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::WrapContent), item_h));
        } else {
            set_text("");
            set_layout_params(miqu::LayoutParams(item_h, item_h));
        }
    }
    return miqu::Button::on_mouse_move(lx, ly, bounds);
}

bool VolumeModuleView::on_mouse_button(int lx, int ly, miqu::MouseButton button, bool pressed, const miqu::Rect& bounds) {
    if (pressed && button == miqu::MouseButton::Middle) {
        std::system("wpctl set-mute @DEFAULT_AUDIO_SINK@ toggle 2>/dev/null || pactl set-sink-mute @DEFAULT_SINK@ toggle 2>/dev/null");
        update_metrics();
        return true;
    }
    return miqu::Button::on_mouse_button(lx, ly, button, pressed, bounds);
}

// ==========================================
// Clock Module
// ==========================================
ClockModuleView::ClockModuleView(std::function<void()> on_open_calendar)
    : m_on_open_calendar(std::move(on_open_calendar)) {
    const auto& cfg = BarConfig::get();
    int item_h = std::max(28, cfg.height - 8);

    set_flat(true);
    set_padding(8, 2);
    set_layout_params(miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::WrapContent), item_h));
    set_on_click_listener(m_on_open_calendar);
    set_text(""); // Drawn via custom draw with Pango horizontal center alignment
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

    request_redraw();
}

miqu::Size ClockModuleView::measure_size() const {
    const auto& cfg = BarConfig::get();
    int item_h = std::max(28, cfg.height - 8);

    cairo_surface_t* temp_surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
    cairo_t* cr = cairo_create(temp_surf);
    PangoLayout* layout = pango_cairo_create_layout(cr);

    std::string text = cfg.clock_stacked ? (m_time_str + "\n" + m_date_str)
                                         : (m_time_str + "  " + m_date_str);
    pango_layout_set_text(layout, text.c_str(), -1);
    pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);

    auto config = miqu::Config::get();
    std::string font_family = config->metrics.font_family.empty() ? "Sans" : config->metrics.font_family;
    int font_size = get_toolkit_font_size();
    std::string font_desc_str = font_family + " " + std::to_string(font_size);
    PangoFontDescription* desc = pango_font_description_from_string(font_desc_str.c_str());
    pango_layout_set_font_description(layout, desc);
    pango_font_description_free(desc);

    int text_w = 0, text_h = 0;
    pango_layout_get_pixel_size(layout, &text_w, &text_h);

    g_object_unref(layout);
    cairo_destroy(cr);
    cairo_surface_destroy(temp_surf);

    return miqu::Size(text_w + 16, item_h);
}

void ClockModuleView::draw(cairo_t* cr, const miqu::Rect& bounds) {
    if (!is_visible() || !cr || bounds.width <= 0 || bounds.height <= 0) return;

    // Draw button background, rounded corners, hover/pressed feedback
    miqu::Button::draw(cr, bounds);

    const auto& cfg = BarConfig::get();
    auto config = miqu::Config::get();

    PangoLayout* layout = pango_cairo_create_layout(cr);
    std::string text = cfg.clock_stacked ? (m_time_str + "\n" + m_date_str)
                                         : (m_time_str + "  " + m_date_str);
    pango_layout_set_text(layout, text.c_str(), -1);
    pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);

    std::string font_family = config->metrics.font_family.empty() ? "Sans" : config->metrics.font_family;
    int font_size = get_toolkit_font_size();
    std::string font_desc_str = font_family + " " + std::to_string(font_size);
    PangoFontDescription* desc = pango_font_description_from_string(font_desc_str.c_str());
    pango_layout_set_font_description(layout, desc);
    pango_font_description_free(desc);

    int text_w = 0, text_h = 0;
    pango_layout_get_pixel_size(layout, &text_w, &text_h);

    double text_x = bounds.x + (bounds.width - text_w) / 2.0;
    double text_y = bounds.y + (bounds.height - text_h) / 2.0;

    cairo_move_to(cr, text_x, text_y);
    auto fg = config->colors.on_surface;
    cairo_set_source_rgba(cr, fg.r, fg.g, fg.b, fg.a);
    pango_cairo_show_layout(cr, layout);
    g_object_unref(layout);
}

// ==========================================
// Notification Module
// ==========================================
NotificationModuleView::NotificationModuleView() {
    const auto& cfg = BarConfig::get();
    int item_h = std::max(28, cfg.height - 8);

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

    const auto& cfg = BarConfig::get();
    int item_h = std::max(28, cfg.height - 8);

    if (m_dnd) {
        set_icon("notifications-disabled");
    } else {
        set_icon("bell");
    }

    if (m_unread_count > 0) {
        set_text(std::to_string(m_unread_count));
        set_layout_params(miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::WrapContent), item_h));
    } else {
        set_text("");
        set_layout_params(miqu::LayoutParams(item_h, item_h));
    }

    request_redraw();
}

bool NotificationModuleView::on_mouse_move(int lx, int ly, const miqu::Rect& bounds) {
    bool hovered = bounds.contains(miqu::Point(lx, ly));
    if (hovered != m_is_hovered) {
        m_is_hovered = hovered;
    }
    return miqu::Button::on_mouse_move(lx, ly, bounds);
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
// Peek Button ("Show Desktop")
// ==========================================
PeekButtonView::PeekButtonView() {
    const auto& cfg = BarConfig::get();
    int item_h = std::max(28, cfg.height - 8);

    set_flat(true);
    set_padding(2, 0);
    set_layout_params(miqu::LayoutParams(8, item_h));
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
