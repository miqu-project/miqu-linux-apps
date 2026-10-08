#include "clock_widget.hpp"
#include "core/desk_config.hpp"
#include <ctime>
#include <cstring>
#include <cctype>

namespace miqudesk {

ClockWidget::ClockWidget() {
    const auto& cfg = DeskConfig::get();
    m_format_24h = cfg.clock_24h;

    // 1. Digital Time (Hours:Minutes)
    m_time_view = miqu::TextViewBuilder::create()
        ->text("00:00")
        ->fontFamily(cfg.font_family)
        ->textSize(std::max(24, cfg.font_size * 4))
        ->bold(cfg.clock_bold)
        ->textColor(cfg.font_color)
        ->textAlignment(miqu::TextAlignment::Center)
        ->build();

    // 2. Seconds Counter
    m_seconds_view = miqu::TextViewBuilder::create()
        ->text(":00")
        ->fontFamily(cfg.font_family)
        ->textSize(std::max(10, cfg.font_size + 3))
        ->bold(cfg.font_bold)
        ->textColor(cfg.accent_color.mix(cfg.font_color_muted, 0.4f))
        ->textAlignment(miqu::TextAlignment::Left)
        ->build();

    // 3. AM/PM / 24H Pill Badge
    m_ampm_view = miqu::TextViewBuilder::create()
        ->text("AM")
        ->fontFamily(cfg.font_family)
        ->textSize(std::max(8, cfg.font_size - 2))
        ->bold(cfg.font_bold)
        ->textColor(cfg.accent_color)
        ->textAlignment(miqu::TextAlignment::Center)
        ->padding(5, 2)
        ->build();

    m_ampm_badge = miqu::CardViewBuilder::create()
        ->backgroundColor(cfg.accent_color.with_alpha(0.18f))
        ->stroke(1, cfg.accent_color.with_alpha(0.40f))
        ->cornerRadius(std::max(4, cfg.widget_corner_radius / 3))
        ->addView(m_ampm_view)
        ->build();

    auto time_sub_col = miqu::LinearLayoutBuilder::create()
        ->orientation(miqu::Orientation::Vertical)
        ->gravity(miqu::Gravity::Center)
        ->spacing(4)
        ->margin(6, 4, 0, 0);

    if (cfg.clock_show_seconds) {
        time_sub_col->addView(m_seconds_view);
    }
    time_sub_col->addView(m_ampm_badge);

    auto time_row = miqu::LinearLayoutBuilder::create()
        ->orientation(miqu::Orientation::Horizontal)
        ->gravity(miqu::Gravity::Center)
        ->addView(m_time_view)
        ->addView(time_sub_col->build())
        ->build();

    // 4. Weekday Capsule Badge
    m_weekday_view = miqu::TextViewBuilder::create()
        ->text("MONDAY")
        ->fontFamily(cfg.font_family)
        ->textSize(std::max(8, cfg.font_size - 1))
        ->bold(cfg.font_bold)
        ->textColor(cfg.font_color.mix(cfg.accent_color, 0.3f))
        ->textAlignment(miqu::TextAlignment::Center)
        ->padding(8, 2)
        ->build();

    m_weekday_badge = miqu::CardViewBuilder::create()
        ->backgroundColor(cfg.widget_border_color.with_alpha(0.12f))
        ->stroke(1, cfg.widget_border_color)
        ->cornerRadius(std::max(4, cfg.widget_corner_radius / 3))
        ->addView(m_weekday_view)
        ->build();

    // 5. Date Text
    m_date_view = miqu::TextViewBuilder::create()
        ->text("Loading date...")
        ->fontFamily(cfg.font_family)
        ->textSize(std::max(10, cfg.font_size + 2))
        ->bold(false)
        ->textColor(cfg.font_color_muted)
        ->textAlignment(miqu::TextAlignment::Left)
        ->margin(6, 0, 0, 0)
        ->build();

    auto date_row = miqu::LinearLayoutBuilder::create()
        ->orientation(miqu::Orientation::Horizontal)
        ->gravity(miqu::Gravity::Center)
        ->spacing(8)
        ->addView(m_weekday_badge)
        ->addView(m_date_view)
        ->build();

    update_time_strings();

    // 6. Overall Content Container
    auto content = miqu::LinearLayoutBuilder::create()
        ->orientation(miqu::Orientation::Vertical)
        ->gravity(miqu::Gravity::Center)
        ->spacing(8)
        ->padding(20, 14)
        ->addView(time_row, miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::WrapContent), static_cast<int>(miqu::LayoutDimension::WrapContent)))
        ->addView(date_row, miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::WrapContent), static_cast<int>(miqu::LayoutDimension::WrapContent)))
        ->build();

    // 7. Glassmorphic Pill Root Card
    m_card = miqu::CardViewBuilder::create()
        ->backgroundColor(cfg.widget_background)
        ->stroke(cfg.widget_border_width, cfg.widget_border_color)
        ->cornerRadius(cfg.widget_corner_radius)
        ->addView(content, miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::MatchParent), static_cast<int>(miqu::LayoutDimension::MatchParent)))
        ->build();

    m_card->set_on_click_listener([this]() {
        toggle_time_format();
    });
}

void ClockWidget::update_time_strings() {
    std::time_t t = std::time(nullptr);
    std::tm* tm = std::localtime(&t);
    if (!tm) return;

    char time_buf[32];
    char sec_buf[16];
    char ampm_buf[16];

    if (m_format_24h) {
        std::strftime(time_buf, sizeof(time_buf), "%H:%M", tm);
        std::snprintf(ampm_buf, sizeof(ampm_buf), "24H");
    } else {
        std::strftime(time_buf, sizeof(time_buf), "%I:%M", tm);
        if (time_buf[0] == '0') {
            std::memmove(time_buf, time_buf + 1, std::strlen(time_buf));
        }
        std::strftime(ampm_buf, sizeof(ampm_buf), "%p", tm);
    }

    std::strftime(sec_buf, sizeof(sec_buf), ":%S", tm);

    char weekday_buf[64];
    std::strftime(weekday_buf, sizeof(weekday_buf), "%A", tm);
    for (char* p = weekday_buf; *p; ++p) *p = static_cast<char>(std::toupper(*p));

    char date_buf[128];
    std::strftime(date_buf, sizeof(date_buf), "%d %B %Y", tm);

    if (m_time_view) m_time_view->set_text(time_buf);
    if (m_seconds_view) m_seconds_view->set_text(sec_buf);
    if (m_ampm_view) m_ampm_view->set_text(ampm_buf);
    if (m_weekday_view) m_weekday_view->set_text(weekday_buf);
    if (m_date_view) m_date_view->set_text(date_buf);
}

void ClockWidget::toggle_time_format() {
    m_format_24h = !m_format_24h;
    update_time_strings();
}

void ClockWidget::tick() {
    update_time_strings();
}

void ClockWidget::on_config_reload() {
    const auto& cfg = DeskConfig::get();
    m_format_24h = cfg.clock_24h;

    if (m_card) {
        m_card->set_card_background_color(cfg.widget_background);
        m_card->set_stroke(cfg.widget_border_width, cfg.widget_border_color);
        m_card->set_radius(cfg.widget_corner_radius);
    }

    if (m_ampm_badge) {
        m_ampm_badge->set_card_background_color(cfg.accent_color.with_alpha(0.18f));
        m_ampm_badge->set_stroke(1, cfg.accent_color.with_alpha(0.40f));
        m_ampm_badge->set_radius(std::max(4, cfg.widget_corner_radius / 3));
    }

    if (m_weekday_badge) {
        m_weekday_badge->set_card_background_color(cfg.widget_border_color.with_alpha(0.12f));
        m_weekday_badge->set_stroke(1, cfg.widget_border_color);
        m_weekday_badge->set_radius(std::max(4, cfg.widget_corner_radius / 3));
    }

    if (m_time_view) {
        m_time_view->set_font_family(cfg.font_family);
        m_time_view->set_text_size(std::max(24, cfg.font_size * 4));
        m_time_view->set_bold(cfg.clock_bold);
        m_time_view->set_text_color(cfg.font_color);
    }

    if (m_seconds_view) {
        m_seconds_view->set_font_family(cfg.font_family);
        m_seconds_view->set_text_size(std::max(10, cfg.font_size + 3));
        m_seconds_view->set_bold(cfg.font_bold);
        m_seconds_view->set_text_color(cfg.accent_color.mix(cfg.font_color_muted, 0.4f));
    }

    if (m_ampm_view) {
        m_ampm_view->set_font_family(cfg.font_family);
        m_ampm_view->set_text_size(std::max(8, cfg.font_size - 2));
        m_ampm_view->set_bold(cfg.font_bold);
        m_ampm_view->set_text_color(cfg.accent_color);
    }

    if (m_weekday_view) {
        m_weekday_view->set_font_family(cfg.font_family);
        m_weekday_view->set_text_size(std::max(8, cfg.font_size - 1));
        m_weekday_view->set_bold(cfg.font_bold);
        m_weekday_view->set_text_color(cfg.font_color.mix(cfg.accent_color, 0.3f));
    }

    if (m_date_view) {
        m_date_view->set_font_family(cfg.font_family);
        m_date_view->set_text_size(std::max(10, cfg.font_size + 2));
        m_date_view->set_bold(false);
        m_date_view->set_text_color(cfg.font_color_muted);
    }

    update_time_strings();
}

} // namespace miqudesk
