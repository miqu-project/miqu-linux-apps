#include "calendar_popup.hpp"
#include "../config/bar_config.hpp"
#include "miqutoolkit/core/config.hpp"
#include "miqutoolkit/view/divider_view.hpp"
#include "miqutoolkit/view/text_view.hpp"
#include "miqutoolkit/view/button.hpp"
#include <ctime>
#include <vector>
#include <array>
#include <iostream>

namespace miqubar {

CalendarPopupView::CalendarPopupView(std::function<void()> on_close)
    : m_on_close(std::move(on_close)) {
    auto config = miqu::Config::get();
    set_style(miqu::CardStyle::Outlined);
    set_radius(config->metrics.corner_radius);
    set_elevation(6);

    update_time();
    m_view_year = m_cur_year;
    m_view_month = m_cur_month;

    setup_ui();
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
    int w = time_out->tm_wday;
    return (w == 0) ? 6 : (w - 1); // Monday = 0, Sunday = 6
}

void CalendarPopupView::setup_ui() {
    auto config = miqu::Config::get();
    int base_font_size = config->metrics.font_size > 0 ? config->metrics.font_size : 11;
    int h1_font_size = config->metrics.h1_size > 0 ? config->metrics.h1_size : 22;
    int h3_font_size = config->metrics.h3_size > 0 ? config->metrics.h3_size : 13;

    auto root = std::make_shared<miqu::LinearLayout>(miqu::Orientation::Vertical);
    root->set_layout_params(miqu::LayoutParams(
        static_cast<int>(miqu::LayoutDimension::MatchParent),
        static_cast<int>(miqu::LayoutDimension::MatchParent)
    ));
    root->set_padding(16, 16, 16, 16);

    // 1. Digital Clock (large bold, no ellipsize)
    m_time_text = miqu::TextViewBuilder::create()
        ->text(m_time_str)
        ->bold(true)
        ->textSize(h1_font_size)
        ->ellipsize(false)
        ->build();
    m_time_text->set_layout_params(miqu::LayoutParams(
        static_cast<int>(miqu::LayoutDimension::MatchParent),
        static_cast<int>(miqu::LayoutDimension::WrapContent)
    ));
    m_time_text->set_margin(0, 0, 0, 2);
    root->add_view(m_time_text);

    // 2. Full Date (synced font size, no ellipsize)
    m_date_text = miqu::TextViewBuilder::create()
        ->text(m_date_full_str)
        ->textSize(base_font_size)
        ->ellipsize(false)
        ->muted(true)
        ->build();
    m_date_text->set_layout_params(miqu::LayoutParams(
        static_cast<int>(miqu::LayoutDimension::MatchParent),
        static_cast<int>(miqu::LayoutDimension::WrapContent)
    ));
    m_date_text->set_margin(0, 0, 0, 12);
    root->add_view(m_date_text);

    // 3. Divider
    auto div = std::make_shared<miqu::DividerView>(miqu::Orientation::Horizontal, 1);
    div->set_layout_params(miqu::LayoutParams(
        static_cast<int>(miqu::LayoutDimension::MatchParent),
        1
    ));
    div->set_margin(0, 0, 0, 12);
    root->add_view(div);

    // 4. Month Navigation Header (Prev Button | Month Year | Next Button)
    auto nav_row = std::make_shared<miqu::LinearLayout>(miqu::Orientation::Horizontal);
    nav_row->set_layout_params(miqu::LayoutParams(
        static_cast<int>(miqu::LayoutDimension::MatchParent),
        static_cast<int>(miqu::LayoutDimension::WrapContent)
    ));
    nav_row->set_gravity(miqu::Gravity::CenterVertical);
    nav_row->set_margin(0, 0, 0, 8);

    auto prev_btn = miqu::ButtonBuilder::create()
        ->text("<")
        ->flat(true)
        ->textSize(base_font_size)
        ->bold(true)
        ->build();
    prev_btn->set_layout_params(miqu::LayoutParams(32, 30));
    prev_btn->set_padding(1, 0, 1, 0);
    prev_btn->set_on_click_listener([this]() {
        m_view_month--;
        if (m_view_month < 1) {
            m_view_month = 12;
            m_view_year--;
        }
        rebuild_calendar();
    });
    nav_row->add_view(prev_btn);

    m_month_label = miqu::TextViewBuilder::create()
        ->bold(true)
        ->textSize(h3_font_size)
        ->ellipsize(false)
        ->textAlignment(miqu::TextAlignment::Center)
        ->build();
    m_month_label->set_layout_params(miqu::LayoutParams(0, 30, 1.0f));
    nav_row->add_view(m_month_label);

    auto next_btn = miqu::ButtonBuilder::create()
        ->text(">")
        ->flat(true)
        ->textSize(base_font_size)
        ->bold(true)
        ->build();
    next_btn->set_layout_params(miqu::LayoutParams(32, 30));
    next_btn->set_padding(1, 0, 1, 0);
    next_btn->set_on_click_listener([this]() {
        m_view_month++;
        if (m_view_month > 12) {
            m_view_month = 1;
            m_view_year++;
        }
        rebuild_calendar();
    });
    nav_row->add_view(next_btn);

    root->add_view(nav_row);

    // 5. Day names header row (MatchParent width + ellipsize=false ensures 2-char names never get cut)
    auto days_header = std::make_shared<miqu::LinearLayout>(miqu::Orientation::Horizontal);
    days_header->set_layout_params(miqu::LayoutParams(
        static_cast<int>(miqu::LayoutDimension::MatchParent),
        static_cast<int>(miqu::LayoutDimension::WrapContent)
    ));
    days_header->set_margin(0, 0, 0, 6);
    const std::array<const char*, 7> day_names = {"Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"};
    for (const auto* name : day_names) {
        auto th = miqu::TextViewBuilder::create()
            ->text(name)
            ->textSize(base_font_size)
            ->ellipsize(false)
            ->muted(true)
            ->textAlignment(miqu::TextAlignment::Center)
            ->build();
        th->set_layout_params(miqu::LayoutParams(0, 22, 1.0f));
        days_header->add_view(th);
    }
    root->add_view(days_header);

    // 6. Days Grid container
    m_grid_layout = std::make_shared<miqu::LinearLayout>(miqu::Orientation::Vertical);
    m_grid_layout->set_layout_params(miqu::LayoutParams(
        static_cast<int>(miqu::LayoutDimension::MatchParent),
        static_cast<int>(miqu::LayoutDimension::WrapContent)
    ));
    m_grid_layout->set_divider_spacing(3);
    root->add_view(m_grid_layout);

    add_view(root);

    rebuild_calendar();
}

void CalendarPopupView::rebuild_calendar() {
    auto config = miqu::Config::get();
    int base_font_size = config->metrics.font_size > 0 ? config->metrics.font_size : 11;

    const std::array<const char*, 12> month_names = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };

    if (m_month_label && m_view_month >= 1 && m_view_month <= 12) {
        m_month_label->set_text(std::string(month_names[m_view_month - 1]) + " " + std::to_string(m_view_year));
    }

    if (!m_grid_layout) return;
    m_grid_layout->clear_views();

    int days_in_month = get_days_in_month(m_view_year, m_view_month);
    int first_weekday = get_first_weekday_of_month(m_view_year, m_view_month);

    int prev_month = m_view_month - 1;
    int prev_year = m_view_year;
    if (prev_month < 1) {
        prev_month = 12;
        prev_year--;
    }
    int days_in_prev_month = get_days_in_month(prev_year, prev_month);

    int cur_day_num = 1;
    for (int week = 0; week < 6; ++week) {
        auto row = std::make_shared<miqu::LinearLayout>(miqu::Orientation::Horizontal);
        row->set_layout_params(miqu::LayoutParams(
            static_cast<int>(miqu::LayoutDimension::MatchParent),
            static_cast<int>(miqu::LayoutDimension::WrapContent)
        ));
        row->set_divider_spacing(3);

        for (int d = 0; d < 7; ++d) {
            int slot = week * 7 + d;
            if (slot < first_weekday) {
                // Day from previous month
                int prev_day = days_in_prev_month - (first_weekday - 1 - slot);
                auto btn = miqu::ButtonBuilder::create()
                    ->text(std::to_string(prev_day))
                    ->flat(true)
                    ->textSize(base_font_size)
                    ->build();
                btn->set_layout_params(miqu::LayoutParams(0, 30, 1.0f));
                btn->set_padding(1, 0, 1, 0);
                btn->set_on_click_listener([this]() {
                    m_view_month--;
                    if (m_view_month < 1) {
                        m_view_month = 12;
                        m_view_year--;
                    }
                    rebuild_calendar();
                });
                row->add_view(btn);
            } else if (cur_day_num <= days_in_month) {
                // Day in current view month
                auto btn = miqu::ButtonBuilder::create()
                    ->text(std::to_string(cur_day_num))
                    ->textSize(base_font_size)
                    ->build();
                btn->set_layout_params(miqu::LayoutParams(0, 30, 1.0f));
                btn->set_padding(1, 0, 1, 0);
                if (cur_day_num == m_cur_day && m_view_month == m_cur_month && m_view_year == m_cur_year) {
                    btn->set_primary(true);
                } else {
                    btn->set_flat(true);
                }
                row->add_view(btn);
                cur_day_num++;
            } else {
                // Day from next month
                int next_day = slot - (first_weekday + days_in_month) + 1;
                auto btn = miqu::ButtonBuilder::create()
                    ->text(std::to_string(next_day))
                    ->flat(true)
                    ->textSize(base_font_size)
                    ->build();
                btn->set_layout_params(miqu::LayoutParams(0, 30, 1.0f));
                btn->set_padding(1, 0, 1, 0);
                btn->set_on_click_listener([this]() {
                    m_view_month++;
                    if (m_view_month > 12) {
                        m_view_month = 1;
                        m_view_year++;
                    }
                    rebuild_calendar();
                });
                row->add_view(btn);
            }
        }
        m_grid_layout->add_view(row);
    }

    request_redraw();
}

} // namespace miqubar
