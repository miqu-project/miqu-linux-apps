#pragma once

#include "core/widget.hpp"
#include <string>

namespace miqudesk {

class SystemWidget : public Widget {
public:
    SystemWidget();
    ~SystemWidget() override = default;

    std::string get_id() const override { return "system"; }
    std::string get_title() const override { return "System Monitor"; }
    std::shared_ptr<miqu::View> get_view() override { return m_card; }

    void tick() override;
    void on_config_reload() override;

private:
    void update_metrics();
    double read_cpu_usage();
    void read_memory_usage(int& used_mb, int& total_mb);
    std::string read_uptime();

    std::shared_ptr<miqu::CardView> m_card;
    std::shared_ptr<miqu::CardView> m_title_badge;
    std::shared_ptr<miqu::TextView> m_title_view;
    std::shared_ptr<miqu::TextView> m_uptime_text;
    std::shared_ptr<miqu::TextView> m_cpu_label;
    std::shared_ptr<miqu::TextView> m_cpu_text;
    std::shared_ptr<miqu::TextView> m_ram_label;
    std::shared_ptr<miqu::TextView> m_ram_text;

    unsigned long long m_prev_user = 0;
    unsigned long long m_prev_nice = 0;
    unsigned long long m_prev_system = 0;
    unsigned long long m_prev_idle = 0;
    unsigned long long m_prev_iowait = 0;
    unsigned long long m_prev_irq = 0;
    unsigned long long m_prev_softirq = 0;
    unsigned long long m_prev_steal = 0;
    bool m_first_cpu_read = true;
};

} // namespace miqudesk
