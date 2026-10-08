#pragma once

#include "core/widget.hpp"
#include <string>

namespace miqudesk {

class ClockWidget : public Widget {
public:
    ClockWidget();
    ~ClockWidget() override = default;

    std::string get_id() const override { return "clock"; }
    std::string get_title() const override { return "Clock & Calendar"; }
    std::shared_ptr<miqu::View> get_view() override { return m_card; }

    void tick() override;
    void on_config_reload() override;

private:
    void update_time_strings();
    void toggle_time_format();

    std::shared_ptr<miqu::CardView> m_card;
    std::shared_ptr<miqu::CardView> m_ampm_badge;
    std::shared_ptr<miqu::CardView> m_weekday_badge;
    std::shared_ptr<miqu::TextView> m_time_view;
    std::shared_ptr<miqu::TextView> m_seconds_view;
    std::shared_ptr<miqu::TextView> m_ampm_view;
    std::shared_ptr<miqu::TextView> m_weekday_view;
    std::shared_ptr<miqu::TextView> m_date_view;

    bool m_format_24h = false;
};

} // namespace miqudesk
