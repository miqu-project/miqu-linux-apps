#pragma once

#include "miqutoolkit/view/card_view.hpp"
#include "miqutoolkit/view/linear_layout.hpp"
#include "miqutoolkit/view/text_view.hpp"
#include "miqutoolkit/view/button.hpp"
#include <functional>
#include <string>
#include <memory>

namespace miqubar {

class CalendarPopupView : public miqu::CardView {
public:
    explicit CalendarPopupView(std::function<void()> on_close);
    ~CalendarPopupView() override = default;

    miqu::Size measure_size() const override { return {310, 360}; }

private:
    void update_time();
    void setup_ui();
    void rebuild_calendar();
    int get_days_in_month(int year, int month) const;
    int get_first_weekday_of_month(int year, int month) const;

    std::string m_time_str = "12:00:00";
    std::string m_date_full_str = "Thursday, 9 October 2026";
    int m_cur_year = 2026;
    int m_cur_month = 10;
    int m_cur_day = 9;

    int m_view_year = 2026;
    int m_view_month = 10;

    std::shared_ptr<miqu::TextView> m_time_text;
    std::shared_ptr<miqu::TextView> m_date_text;
    std::shared_ptr<miqu::TextView> m_month_label;
    std::shared_ptr<miqu::LinearLayout> m_grid_layout;

    std::function<void()> m_on_close;
};

} // namespace miqubar
