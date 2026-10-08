#include "system_widget.hpp"
#include "core/desk_config.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>

namespace miqudesk {

SystemWidget::SystemWidget() {
    const auto& cfg = DeskConfig::get();

    m_title_view = miqu::TextViewBuilder::create()
        ->text("SYSTEM HEALTH")
        ->fontFamily(cfg.font_family)
        ->textSize(std::max(8, cfg.font_size - 1))
        ->bold(cfg.font_bold)
        ->textColor(cfg.font_color.mix(cfg.accent_color, 0.3f))
        ->textAlignment(miqu::TextAlignment::Left)
        ->padding(6, 2)
        ->build();

    m_title_badge = miqu::CardViewBuilder::create()
        ->backgroundColor(cfg.widget_border_color.with_alpha(0.12f))
        ->stroke(1, cfg.widget_border_color)
        ->cornerRadius(std::max(4, cfg.widget_corner_radius / 3))
        ->addView(m_title_view)
        ->build();

    m_uptime_text = miqu::TextViewBuilder::create()
        ->text("UP --:--")
        ->fontFamily(cfg.font_family)
        ->textSize(cfg.font_size)
        ->bold(false)
        ->textColor(cfg.font_color_muted)
        ->textAlignment(miqu::TextAlignment::Right)
        ->margin(8, 0, 0, 0)
        ->build();

    auto header_row = miqu::LinearLayoutBuilder::create()
        ->orientation(miqu::Orientation::Horizontal)
        ->gravity(miqu::Gravity::Center)
        ->addView(m_title_badge)
        ->addView(m_uptime_text, miqu::LayoutParams(1.0f))
        ->build();

    // CPU row
    m_cpu_label = miqu::TextViewBuilder::create()
        ->text("CPU")
        ->fontFamily(cfg.font_family)
        ->textSize(cfg.font_size)
        ->bold(cfg.font_bold)
        ->textColor(cfg.accent_color)
        ->build();

    m_cpu_text = miqu::TextViewBuilder::create()
        ->text("0%")
        ->fontFamily(cfg.font_family)
        ->textSize(std::max(11, cfg.font_size + 3))
        ->bold(cfg.font_bold)
        ->textColor(cfg.font_color)
        ->textAlignment(miqu::TextAlignment::Right)
        ->build();

    auto cpu_col = miqu::LinearLayoutBuilder::create()
        ->orientation(miqu::Orientation::Horizontal)
        ->gravity(miqu::Gravity::Center)
        ->addView(m_cpu_label)
        ->addView(m_cpu_text, miqu::LayoutParams(1.0f))
        ->build();

    // RAM row
    m_ram_label = miqu::TextViewBuilder::create()
        ->text("RAM")
        ->fontFamily(cfg.font_family)
        ->textSize(cfg.font_size)
        ->bold(cfg.font_bold)
        ->textColor(cfg.accent_color.mix(miqu::Color::rgba(0.85f, 0.55f, 1.0f, 1.0f), 0.5f))
        ->build();

    m_ram_text = miqu::TextViewBuilder::create()
        ->text("0.0 / 0.0 GB")
        ->fontFamily(cfg.font_family)
        ->textSize(std::max(11, cfg.font_size + 3))
        ->bold(cfg.font_bold)
        ->textColor(cfg.font_color)
        ->textAlignment(miqu::TextAlignment::Right)
        ->build();

    auto ram_col = miqu::LinearLayoutBuilder::create()
        ->orientation(miqu::Orientation::Horizontal)
        ->gravity(miqu::Gravity::Center)
        ->addView(m_ram_label)
        ->addView(m_ram_text, miqu::LayoutParams(1.0f))
        ->build();

    auto stats_layout = miqu::LinearLayoutBuilder::create()
        ->orientation(miqu::Orientation::Vertical)
        ->spacing(6)
        ->addView(cpu_col)
        ->addView(ram_col)
        ->build();

    auto content = miqu::LinearLayoutBuilder::create()
        ->orientation(miqu::Orientation::Vertical)
        ->spacing(10)
        ->padding(18, 14)
        ->addView(header_row, miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::MatchParent), static_cast<int>(miqu::LayoutDimension::WrapContent)))
        ->addView(stats_layout, miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::MatchParent), static_cast<int>(miqu::LayoutDimension::WrapContent)))
        ->build();

    m_card = miqu::CardViewBuilder::create()
        ->backgroundColor(cfg.widget_background)
        ->stroke(cfg.widget_border_width, cfg.widget_border_color)
        ->cornerRadius(cfg.widget_corner_radius)
        ->addView(content, miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::MatchParent), static_cast<int>(miqu::LayoutDimension::MatchParent)))
        ->build();

    update_metrics();
}

double SystemWidget::read_cpu_usage() {
    std::ifstream file("/proc/stat");
    if (!file.is_open()) return 0.0;

    std::string line;
    std::getline(file, line);
    std::istringstream ss(line);

    std::string cpu_prefix;
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
    ss >> cpu_prefix >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;

    if (m_first_cpu_read) {
        m_prev_user = user;
        m_prev_nice = nice;
        m_prev_system = system;
        m_prev_idle = idle;
        m_prev_iowait = iowait;
        m_prev_irq = irq;
        m_prev_softirq = softirq;
        m_prev_steal = steal;
        m_first_cpu_read = false;
        return 0.0;
    }

    unsigned long long prev_idle_total = m_prev_idle + m_prev_iowait;
    unsigned long long idle_total = idle + iowait;

    unsigned long long prev_non_idle = m_prev_user + m_prev_nice + m_prev_system + m_prev_irq + m_prev_softirq + m_prev_steal;
    unsigned long long non_idle = user + nice + system + irq + softirq + steal;

    unsigned long long prev_total = prev_idle_total + prev_non_idle;
    unsigned long long total = idle_total + non_idle;

    unsigned long long totald = total - prev_total;
    unsigned long long idled = idle_total - prev_idle_total;

    m_prev_user = user;
    m_prev_nice = nice;
    m_prev_system = system;
    m_prev_idle = idle;
    m_prev_iowait = iowait;
    m_prev_irq = irq;
    m_prev_softirq = softirq;
    m_prev_steal = steal;

    if (totald == 0) return 0.0;
    return (double)(totald - idled) / (double)totald * 100.0;
}

void SystemWidget::read_memory_usage(int& used_mb, int& total_mb) {
    used_mb = 0;
    total_mb = 0;
    std::ifstream file("/proc/meminfo");
    if (!file.is_open()) return;

    std::string key;
    unsigned long value;
    std::string unit;

    unsigned long mem_total = 0;
    unsigned long mem_avail = 0;

    while (file >> key >> value >> unit) {
        if (key == "MemTotal:") mem_total = value;
        else if (key == "MemAvailable:") mem_avail = value;
        if (mem_total > 0 && mem_avail > 0) break;
    }

    total_mb = static_cast<int>(mem_total / 1024);
    used_mb = static_cast<int>((mem_total - mem_avail) / 1024);
}

std::string SystemWidget::read_uptime() {
    std::ifstream file("/proc/uptime");
    if (!file.is_open()) return "UP --:--";

    double uptime_secs = 0.0;
    file >> uptime_secs;

    int total_mins = static_cast<int>(uptime_secs / 60);
    int hours = total_mins / 60;
    int mins = total_mins % 60;

    char buf[32];
    std::snprintf(buf, sizeof(buf), "UP %dh %02dm", hours, mins);
    return std::string(buf);
}

void SystemWidget::update_metrics() {
    double cpu = read_cpu_usage();
    int used_mb = 0, total_mb = 0;
    read_memory_usage(used_mb, total_mb);
    std::string uptime = read_uptime();

    char cpu_buf[16];
    std::snprintf(cpu_buf, sizeof(cpu_buf), "%.1f%%", cpu);

    char ram_buf[32];
    std::snprintf(ram_buf, sizeof(ram_buf), "%.1f / %.1f GB", used_mb / 1024.0, total_mb / 1024.0);

    if (m_cpu_text) m_cpu_text->set_text(cpu_buf);
    if (m_ram_text) m_ram_text->set_text(ram_buf);
    if (m_uptime_text) m_uptime_text->set_text(uptime);
}

void SystemWidget::tick() {
    update_metrics();
}

void SystemWidget::on_config_reload() {
    const auto& cfg = DeskConfig::get();

    if (m_card) {
        m_card->set_card_background_color(cfg.widget_background);
        m_card->set_stroke(cfg.widget_border_width, cfg.widget_border_color);
        m_card->set_radius(cfg.widget_corner_radius);
    }

    if (m_title_badge) {
        m_title_badge->set_card_background_color(cfg.widget_border_color.with_alpha(0.12f));
        m_title_badge->set_stroke(1, cfg.widget_border_color);
        m_title_badge->set_radius(std::max(4, cfg.widget_corner_radius / 3));
    }

    if (m_title_view) {
        m_title_view->set_font_family(cfg.font_family);
        m_title_view->set_text_size(std::max(8, cfg.font_size - 1));
        m_title_view->set_bold(cfg.font_bold);
        m_title_view->set_text_color(cfg.font_color.mix(cfg.accent_color, 0.3f));
    }

    if (m_uptime_text) {
        m_uptime_text->set_font_family(cfg.font_family);
        m_uptime_text->set_text_size(cfg.font_size);
        m_uptime_text->set_bold(false);
        m_uptime_text->set_text_color(cfg.font_color_muted);
    }

    if (m_cpu_label) {
        m_cpu_label->set_font_family(cfg.font_family);
        m_cpu_label->set_text_size(cfg.font_size);
        m_cpu_label->set_bold(cfg.font_bold);
        m_cpu_label->set_text_color(cfg.accent_color);
    }

    if (m_cpu_text) {
        m_cpu_text->set_font_family(cfg.font_family);
        m_cpu_text->set_text_size(std::max(11, cfg.font_size + 3));
        m_cpu_text->set_bold(cfg.font_bold);
        m_cpu_text->set_text_color(cfg.font_color);
    }

    if (m_ram_label) {
        m_ram_label->set_font_family(cfg.font_family);
        m_ram_label->set_text_size(cfg.font_size);
        m_ram_label->set_bold(cfg.font_bold);
        m_ram_label->set_text_color(cfg.accent_color.mix(miqu::Color::rgba(0.85f, 0.55f, 1.0f, 1.0f), 0.5f));
    }

    if (m_ram_text) {
        m_ram_text->set_font_family(cfg.font_family);
        m_ram_text->set_text_size(std::max(11, cfg.font_size + 3));
        m_ram_text->set_bold(cfg.font_bold);
        m_ram_text->set_text_color(cfg.font_color);
    }

    update_metrics();
}

} // namespace miqudesk
