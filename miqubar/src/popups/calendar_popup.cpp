#include "calendar_popup.hpp"
#include "../config/bar_config.hpp"
#include <pango/pangocairo.h>
#include <ctime>
#include <cmath>
#include <vector>

namespace miqubar {

CalendarPopupView::CalendarPopupView(std::function<void()> on_close)
    : m_on_close(std::move(on_close)) {
    update_time();
    m_view_year = m_cur_year;
    m_view_month = m_cur_month;
}

void CalendarPopupView::update_time() {
    std::time_t t = std::time(nullptr);
    std::tm* tm = std::localtime(&t);
    if (!tm) return;

    char time_buf[32];
    char date_buf[64];
    std::strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm);
    std::strftime(date_buf, sizeof(date_buf), "%A, %d %B %Y", tm);

    m_time_str = time_buf;
    m_date_full_str = date_buf;
    m_cur_year = tm->tm_year + 1900;
    m_cur_month = tm->tm_mon + 1;
    m_cur_day = tm->tm_mday;
}

int CalendarPopupView::get_days_in_month(int year, int month) const {
    if (month == 2) {
        bool leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
        return leap ? 29 : 28;
    }
    if (month == 4 || month == 6 || month == 9 || month == 11) return 30;
    return 31;
}

int CalendarPopupView::get_first_weekday_of_month(int year, int month) const {
    std::tm time_in = { 0, 0, 0, 1, month - 1, year - 1900, 0, 0, 0, 0, nullptr };
    std::time_t time_temp = std::mktime(&time_in);
    const std::tm* time_out = std::localtime(&time_temp);
    // Sunday is 0, Monday is 1... Convert to Monday=0
    int w = time_out->tm_wday;
    return (w == 0) ? 6 : (w - 1);
}

miqu::Size CalendarPopupView::measure_size() const {
    return {280, 310};
}

void CalendarPopupView::draw(cairo_t* cr, const miqu::Rect& bounds) {
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

    // Large Digital Time
    PangoLayout* t_layout = pango_cairo_create_layout(cr);
    PangoFontDescription* t_desc = pango_font_description_from_string((cfg.font_family + " Bold 20").c_str());
    pango_layout_set_font_description(t_layout, t_desc);
    pango_layout_set_text(t_layout, m_time_str.c_str(), -1);

    cairo_move_to(cr, bounds.x + 18, bounds.y + 14);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    pango_cairo_show_layout(cr, t_layout);
    pango_font_description_free(t_desc);
    g_object_unref(t_layout);

    // Full Date subtitle
    PangoLayout* d_layout = pango_cairo_create_layout(cr);
    PangoFontDescription* d_desc = pango_font_description_from_string((cfg.font_family + " 10").c_str());
    pango_layout_set_font_description(d_layout, d_desc);
    pango_layout_set_text(d_layout, m_date_full_str.c_str(), -1);

    cairo_move_to(cr, bounds.x + 18, bounds.y + 44);
    cairo_set_source_rgba(cr, 0.85, 0.90, 0.96, 0.7);
    pango_cairo_show_layout(cr, d_layout);
    pango_font_description_free(d_desc);
    g_object_unref(d_layout);

    // Divider
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.08);
    cairo_move_to(cr, bounds.x + 16, bounds.y + 68);
    cairo_line_to(cr, bounds.x + bounds.width - 16, bounds.y + 68);
    cairo_stroke(cr);

    // Calendar Header (< Month Year >)
    static const char* months[] = {"January", "February", "March", "April", "May", "June",
                                   "July", "August", "September", "October", "November", "December"};
    std::string month_title = std::string(months[m_view_month - 1]) + " " + std::to_string(m_view_year);

    PangoLayout* m_layout = pango_cairo_create_layout(cr);
    PangoFontDescription* m_desc = pango_font_description_from_string((cfg.font_family + " Bold 11").c_str());
    pango_layout_set_font_description(m_layout, m_desc);
    pango_layout_set_text(m_layout, month_title.c_str(), -1);

    cairo_move_to(cr, bounds.x + 20, bounds.y + 78);
    cairo_set_source_rgb(cr, 0.0, 0.90, 1.0); // #00e5ff
    pango_cairo_show_layout(cr, m_layout);
    pango_font_description_free(m_desc);
    g_object_unref(m_layout);

    // Nav arrows
    PangoLayout* arr_layout = pango_cairo_create_layout(cr);
    PangoFontDescription* arr_desc = pango_font_description_from_string((cfg.font_family + " Bold 11").c_str());
    pango_layout_set_font_description(arr_layout, arr_desc);
    pango_layout_set_text(arr_layout, "<   >", -1);

    int aw, ah;
    pango_layout_get_pixel_size(arr_layout, &aw, &ah);
    cairo_move_to(cr, bounds.x + bounds.width - aw - 20, bounds.y + 78);
    cairo_set_source_rgba(cr, 0.85, 0.90, 0.96, 0.8);
    pango_cairo_show_layout(cr, arr_layout);
    pango_font_description_free(arr_desc);
    g_object_unref(arr_layout);

    // Day of week headers
    static const char* days[] = {"Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"};
    int cell_w = 34;
    int cell_h = 26;
    int grid_x = bounds.x + 18;
    int grid_y = bounds.y + 104;

    PangoFontDescription* day_desc = pango_font_description_from_string((cfg.font_family + " 9").c_str());
    for (int col = 0; col < 7; ++col) {
        PangoLayout* day_layout = pango_cairo_create_layout(cr);
        pango_layout_set_font_description(day_layout, day_desc);
        pango_layout_set_text(day_layout, days[col], -1);

        int dw, dh;
        pango_layout_get_pixel_size(day_layout, &dw, &dh);
        cairo_move_to(cr, grid_x + col * cell_w + (cell_w - dw) / 2.0, grid_y);
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.4);
        pango_cairo_show_layout(cr, day_layout);
        g_object_unref(day_layout);
    }

    // Days grid
    int first_wd = get_first_weekday_of_month(m_view_year, m_view_month);
    int total_days = get_days_in_month(m_view_year, m_view_month);

    int day_num = 1;
    int start_cell = first_wd;
    grid_y += 24;

    for (int row = 0; row < 6 && day_num <= total_days; ++row) {
        for (int col = 0; col < 7 && day_num <= total_days; ++col) {
            if (row == 0 && col < start_cell) continue;

            int cx = grid_x + col * cell_w;
            int cy = grid_y + row * cell_h;
            bool is_today = (m_view_year == m_cur_year && m_view_month == m_cur_month && day_num == m_cur_day);

            if (is_today) {
                // Today highlighted circle
                cairo_set_source_rgb(cr, 0.0, 0.90, 1.0); // #00e5ff
                cairo_arc(cr, cx + cell_w / 2.0, cy + cell_h / 2.0, 11, 0, 2 * M_PI);
                cairo_fill(cr);
            }

            PangoLayout* num_layout = pango_cairo_create_layout(cr);
            pango_layout_set_font_description(num_layout, day_desc);
            std::string d_str = std::to_string(day_num);
            pango_layout_set_text(num_layout, d_str.c_str(), -1);

            int nw, nh;
            pango_layout_get_pixel_size(num_layout, &nw, &nh);
            cairo_move_to(cr, cx + (cell_w - nw) / 2.0, cy + (cell_h - nh) / 2.0);

            if (is_today) {
                cairo_set_source_rgb(cr, 0.05, 0.07, 0.12);
            } else {
                cairo_set_source_rgba(cr, 0.88, 0.92, 0.96, 0.85);
            }
            pango_cairo_show_layout(cr, num_layout);
            g_object_unref(num_layout);

            day_num++;
        }
    }

    pango_font_description_free(day_desc);
}

bool CalendarPopupView::on_mouse_move(int, int, const miqu::Rect&) {
    request_redraw();
    return true;
}

bool CalendarPopupView::on_mouse_button(int lx, int ly, miqu::MouseButton button, bool pressed, const miqu::Rect& bounds) {
    if (button != miqu::MouseButton::Left || !pressed) return false;

    // Check prev/next month buttons
    if (ly >= bounds.y + 70 && ly <= bounds.y + 98) {
        if (lx >= bounds.x + bounds.width - 50 && lx <= bounds.x + bounds.width - 32) {
            // '<'
            m_view_month--;
            if (m_view_month < 1) { m_view_month = 12; m_view_year--; }
            request_redraw();
            return true;
        } else if (lx >= bounds.x + bounds.width - 30 && lx <= bounds.x + bounds.width - 10) {
            // '>'
            m_view_month++;
            if (m_view_month > 12) { m_view_month = 1; m_view_year++; }
            request_redraw();
            return true;
        }
    }

    return false;
}

} // namespace miqubar
