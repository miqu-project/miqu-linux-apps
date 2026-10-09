#pragma once

#include "miqutoolkit/view/view.hpp"
#include <functional>
#include <string>

namespace miqubar {

class CalendarPopupView : public miqu::View {
public:
    CalendarPopupView(std::function<void()> on_close);
    ~CalendarPopupView() override = default;

    void draw(cairo_t* cr, const miqu::Rect& bounds) override;
    miqu::Size measure_size() const override;

    bool on_mouse_move(int lx, int ly, const miqu::Rect& bounds) override;
    bool on_mouse_button(int lx, int ly, miqu::MouseButton button, bool pressed, const miqu::Rect& bounds) override;

private:
    void update_time();
    int get_days_in_month(int year, int month) const;
    int get_first_weekday_of_month(int year, int month) const;

    std::string m_time_str = "12:00:00";
    std::string m_date_full_str = "Thursday, 9 October 2026";
    int m_cur_year = 2026;
    int m_cur_month = 10;
    int m_cur_day = 9;

    int m_view_year = 2026;
    int m_view_month = 10;

    std::function<void()> m_on_close;
};

} // namespace miqubar
