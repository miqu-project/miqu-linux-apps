#include "start_button.hpp"
#include "../config/bar_config.hpp"
#include <unistd.h>
#include <sys/wait.h>
#include <cmath>

namespace miqubar {

StartButtonView::StartButtonView() = default;

miqu::Size StartButtonView::measure_size() const {
    const auto& cfg = BarConfig::get();
    return {cfg.height, cfg.height};
}

void StartButtonView::draw(cairo_t* cr, const miqu::Rect& bounds) {
    if (!cr) return;

    const auto& cfg = BarConfig::get();
    int pad = 4;
    double x = bounds.x + pad;
    double y = bounds.y + pad;
    double w = bounds.width - (pad * 2);
    double h = bounds.height - (pad * 2);
    double r = cfg.corner_radius;

    // Button Background
    if (m_pressed) {
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.16);
    } else if (m_hovered) {
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.08);
    } else {
        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
    }

    if (m_hovered || m_pressed) {
        cairo_new_sub_path(cr);
        cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2, 0);
        cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI / 2);
        cairo_arc(cr, x + r, y + h - r, r, M_PI / 2, M_PI);
        cairo_arc(cr, x + r, y + r, r, M_PI, 3 * M_PI / 2);
        cairo_close_path(cr);
        cairo_fill(cr);
    }

    // Draw Miqu Start Emblem (Stylized layered M mark)
    double cx = bounds.x + bounds.width / 2.0;
    double cy = bounds.y + bounds.height / 2.0;
    double s = cfg.icon_size / 2.0;

    // Glow background around logo on hover
    if (m_hovered) {
        cairo_pattern_t* glow = cairo_pattern_create_radial(cx, cy, 2, cx, cy, s * 1.5);
        cairo_pattern_add_color_stop_rgba(glow, 0, 1.0, 0.34, 0.47, 0.4);
        cairo_pattern_add_color_stop_rgba(glow, 1, 0.65, 0.33, 0.97, 0.0);
        cairo_set_source(cr, glow);
        cairo_arc(cr, cx, cy, s * 1.5, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_pattern_destroy(glow);
    }

    // Start logo 4-square grid / dynamic M shape
    cairo_pattern_t* pat = cairo_pattern_create_linear(cx - s, cy - s, cx + s, cy + s);
    cairo_pattern_add_color_stop_rgb(pat, 0.0, 1.0, 0.34, 0.47); // #ff5778
    cairo_pattern_add_color_stop_rgb(pat, 0.5, 0.65, 0.33, 0.97); // #a855f7
    cairo_pattern_add_color_stop_rgb(pat, 1.0, 0.0, 0.90, 1.0);  // #00e5ff
    cairo_set_source(cr, pat);

    // 4 pill-squares (Windows-style but rounded and vibrant)
    double sz = s * 0.78;
    double gap = 2.5;
    double cr_r = 3.0;

    auto draw_tile = [&](double tx, double ty) {
        cairo_new_sub_path(cr);
        cairo_arc(cr, tx + sz - cr_r, ty + cr_r, cr_r, -M_PI / 2, 0);
        cairo_arc(cr, tx + sz - cr_r, ty + sz - cr_r, cr_r, 0, M_PI / 2);
        cairo_arc(cr, tx + cr_r, ty + sz - cr_r, cr_r, M_PI / 2, M_PI);
        cairo_arc(cr, tx + cr_r, ty + cr_r, cr_r, M_PI, 3 * M_PI / 2);
        cairo_close_path(cr);
        cairo_fill(cr);
    };

    draw_tile(cx - sz - gap, cy - sz - gap); // top-left
    draw_tile(cx + gap,      cy - sz - gap); // top-right
    draw_tile(cx - sz - gap, cy + gap);      // bottom-left
    draw_tile(cx + gap,      cy + gap);      // bottom-right

    cairo_pattern_destroy(pat);
}

bool StartButtonView::on_mouse_move(int lx, int ly, const miqu::Rect& bounds) {
    bool hov = bounds.contains(lx, ly);
    if (hov != m_hovered) {
        m_hovered = hov;
        request_redraw();
    }
    return hov;
}

bool StartButtonView::on_mouse_button(int, int, miqu::MouseButton button, bool pressed, const miqu::Rect&) {
    if (button != miqu::MouseButton::Left) return false;

    if (pressed) {
        m_pressed = true;
        request_redraw();
        return true;
    } else {
        if (m_pressed) {
            m_pressed = false;
            launch_menu();
            request_redraw();
            return true;
        }
    }
    return false;
}

void StartButtonView::launch_menu() {
    const auto& cmd = BarConfig::get().launcher_cmd;
    if (cmd.empty()) return;

    pid_t pid = fork();
    if (pid == 0) {
        // Child: detach completely
        setsid();
        pid_t second_child = fork();
        if (second_child == 0) {
            execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
            _exit(127);
        }
        _exit(0);
    } else if (pid > 0) {
        // Parent reaps the immediate child so no zombies occur
        waitpid(pid, nullptr, 0);
    }
}

} // namespace miqubar
