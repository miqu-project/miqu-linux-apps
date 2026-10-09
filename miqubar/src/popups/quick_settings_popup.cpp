#include "quick_settings_popup.hpp"
#include "../config/bar_config.hpp"
#include <pango/pangocairo.h>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace miqubar {

QuickSettingsPopupView::QuickSettingsPopupView(std::function<void()> on_close)
    : m_on_close(std::move(on_close)) {
    update_audio_info();
}

void QuickSettingsPopupView::update_audio_info() {
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

void QuickSettingsPopupView::set_volume(int pct) {
    m_volume = std::clamp(pct, 0, 100);
    std::string cmd = "wpctl set-volume @DEFAULT_AUDIO_SINK@ " + std::to_string(m_volume / 100.0f) + " 2>/dev/null";
    std::system(cmd.c_str());
    request_redraw();
}

miqu::Size QuickSettingsPopupView::measure_size() const {
    return {300, 260};
}

void QuickSettingsPopupView::draw(cairo_t* cr, const miqu::Rect& bounds) {
    if (!cr) return;
    const auto& cfg = BarConfig::get();

    // Flyout backdrop
    double r = cfg.corner_radius + 4;
    cairo_new_sub_path(cr);
    cairo_arc(cr, bounds.x + bounds.width - r, bounds.y + r, r, -M_PI / 2, 0);
    cairo_arc(cr, bounds.x + bounds.width - r, bounds.y + bounds.height - r, r, 0, M_PI / 2);
    cairo_arc(cr, bounds.x + r, bounds.y + bounds.height - r, r, M_PI / 2, M_PI);
    cairo_arc(cr, bounds.x + r, bounds.y + r, r, M_PI, 3 * M_PI / 2);
    cairo_close_path(cr);

    cairo_set_source_rgba(cr, 0.08, 0.10, 0.16, 0.98);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.12);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    // Title
    PangoLayout* title_layout = pango_cairo_create_layout(cr);
    PangoFontDescription* title_desc = pango_font_description_from_string((cfg.font_family + " Bold 11").c_str());
    pango_layout_set_font_description(title_layout, title_desc);
    pango_layout_set_text(title_layout, "Quick Settings", -1);

    cairo_move_to(cr, bounds.x + 16, bounds.y + 14);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    pango_cairo_show_layout(cr, title_layout);

    pango_font_description_free(title_desc);
    g_object_unref(title_layout);

    // Quick toggle tiles (2x2 grid: Wi-Fi, Bluetooth, Night Light, Performance)
    struct Tile { std::string label; bool active; };
    Tile tiles[] = {
        {"Wi-Fi", m_wifi_on},
        {"Bluetooth", m_bluetooth_on},
        {"Night Light", m_night_light_on},
        {"Power Save", false}
    };

    int tile_w = 126;
    int tile_h = 40;
    int gx = bounds.x + 16;
    int gy = bounds.y + 44;

    for (int i = 0; i < 4; ++i) {
        int tx = gx + (i % 2) * (tile_w + 16);
        int ty = gy + (i / 2) * (tile_h + 10);
        bool act = tiles[i].active;
        bool hov = (m_hovered_tile == i);

        cairo_new_sub_path(cr);
        double tr = 8.0;
        cairo_arc(cr, tx + tile_w - tr, ty + tr, tr, -M_PI / 2, 0);
        cairo_arc(cr, tx + tile_w - tr, ty + tile_h - tr, tr, 0, M_PI / 2);
        cairo_arc(cr, tx + tr, ty + tile_h - tr, tr, M_PI / 2, M_PI);
        cairo_arc(cr, tx + tr, ty + tr, tr, M_PI, 3 * M_PI / 2);
        cairo_close_path(cr);

        if (act) {
            cairo_set_source_rgba(cr, 0.0, 0.90, 1.0, hov ? 0.9 : 0.8); // #00e5ff active
            cairo_fill(cr);
            cairo_set_source_rgb(cr, 0.05, 0.07, 0.12);
        } else {
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, hov ? 0.12 : 0.06);
            cairo_fill_preserve(cr);
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.10);
            cairo_set_line_width(cr, 1.0);
            cairo_stroke(cr);
            cairo_set_source_rgba(cr, 0.85, 0.90, 0.96, 0.85);
        }

        PangoLayout* t_layout = pango_cairo_create_layout(cr);
        PangoFontDescription* t_desc = pango_font_description_from_string((cfg.font_family + " 10").c_str());
        pango_layout_set_font_description(t_layout, t_desc);
        pango_layout_set_text(t_layout, tiles[i].label.c_str(), -1);

        int tw, th;
        pango_layout_get_pixel_size(t_layout, &tw, &th);
        cairo_move_to(cr, tx + (tile_w - tw) / 2.0, ty + (tile_h - th) / 2.0);
        pango_cairo_show_layout(cr, t_layout);

        pango_font_description_free(t_desc);
        g_object_unref(t_layout);
    }

    // Volume Slider Section
    int slider_y = bounds.y + 160;
    int slider_x = bounds.x + 16;
    int slider_w = bounds.width - 32;

    PangoLayout* v_layout = pango_cairo_create_layout(cr);
    PangoFontDescription* v_desc = pango_font_description_from_string((cfg.font_family + " 10").c_str());
    pango_layout_set_font_description(v_layout, v_desc);
    std::string vol_lbl = "Volume: " + std::to_string(m_volume) + "%";
    pango_layout_set_text(v_layout, vol_lbl.c_str(), -1);

    cairo_move_to(cr, slider_x, slider_y);
    cairo_set_source_rgba(cr, 0.85, 0.90, 0.96, 0.9);
    pango_cairo_show_layout(cr, v_layout);

    pango_font_description_free(v_desc);
    g_object_unref(v_layout);

    // Track
    int track_y = slider_y + 24;
    int track_h = 6;
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.12);
    cairo_rectangle(cr, slider_x, track_y, slider_w, track_h);
    cairo_fill(cr);

    // Active progress fill
    double fill_w = (slider_w) * (m_volume / 100.0);
    cairo_set_source_rgb(cr, 0.92, 0.29, 0.60); // Pink #ec4899
    cairo_rectangle(cr, slider_x, track_y, fill_w, track_h);
    cairo_fill(cr);

    // Thumb
    double thumb_cx = slider_x + fill_w;
    double thumb_cy = track_y + track_h / 2.0;
    cairo_arc(cr, thumb_cx, thumb_cy, 7.0, 0, 2 * M_PI);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 0.92, 0.29, 0.60, 0.8);
    cairo_set_line_width(cr, 2.0);
    cairo_stroke(cr);
}

bool QuickSettingsPopupView::on_mouse_enter(int, int) {
    request_redraw();
    return true;
}

bool QuickSettingsPopupView::on_mouse_leave() {
    m_hovered_tile = -1;
    m_dragging_slider = false;
    request_redraw();
    return true;
}

bool QuickSettingsPopupView::on_mouse_button(int lx, int ly, miqu::MouseButton button, bool pressed, const miqu::Rect& bounds) {
    if (button != miqu::MouseButton::Left) return false;

    if (pressed) {
        // Check toggle tiles
        int tile_w = 126;
        int tile_h = 40;
        int gx = bounds.x + 16;
        int gy = bounds.y + 44;

        for (int i = 0; i < 4; ++i) {
            int tx = gx + (i % 2) * (tile_w + 16);
            int ty = gy + (i / 2) * (tile_h + 10);
            if (lx >= tx && lx <= tx + tile_w && ly >= ty && ly <= ty + tile_h) {
                if (i == 0) m_wifi_on = !m_wifi_on;
                else if (i == 1) m_bluetooth_on = !m_bluetooth_on;
                else if (i == 2) m_night_light_on = !m_night_light_on;
                request_redraw();
                return true;
            }
        }

        // Check slider
        int track_y = bounds.y + 160 + 24;
        int slider_x = bounds.x + 16;
        int slider_w = bounds.width - 32;
        if (ly >= track_y - 10 && ly <= track_y + 16 && lx >= slider_x && lx <= slider_x + slider_w) {
            m_dragging_slider = true;
            int pct = static_cast<int>(100.0 * (lx - slider_x) / slider_w);
            set_volume(pct);
            return true;
        }
    } else {
        m_dragging_slider = false;
    }
    return false;
}

bool QuickSettingsPopupView::on_mouse_move(int lx, int, const miqu::Rect& bounds) {
    if (m_dragging_slider) {
        int slider_x = bounds.x + 16;
        int slider_w = bounds.width - 32;
        int pct = static_cast<int>(100.0 * (lx - slider_x) / slider_w);
        set_volume(pct);
        return true;
    }
    return false;
}

} // namespace miqubar
