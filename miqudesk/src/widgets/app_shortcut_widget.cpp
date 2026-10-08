#include "app_shortcut_widget.hpp"
#include "core/desk_config.hpp"
#include <unistd.h>
#include <cstdlib>

namespace miqudesk {

static std::string truncate_label(const std::string& text, int max_chars) {
    if (max_chars <= 0 || text.empty()) return text;
    if (static_cast<int>(text.length()) <= max_chars) return text;
    if (max_chars <= 3) {
        return text.substr(0, max_chars);
    }
    return text.substr(0, max_chars - 1) + "…";
}

AppShortcutWidget::AppShortcutWidget(DesktopShortcut shortcut)
    : m_shortcut(std::move(shortcut))
{
    const auto& cfg = DeskConfig::get();

    m_icon_view = miqu::ImageViewBuilder::create()
        ->imageResource(m_shortcut.icon)
        ->targetSize(cfg.icon_size)
        ->build();

    m_title_view = miqu::TextViewBuilder::create()
        ->text(truncate_label(m_shortcut.name, cfg.desktop_max_chars))
        ->fontFamily(cfg.font_family)
        ->textSize(cfg.font_size)
        ->bold(cfg.shortcut_bold)
        ->textColor(cfg.font_color)
        ->textAlignment(miqu::TextAlignment::Center)
        ->padding(4, 2)
        ->build();

    auto content = miqu::LinearLayoutBuilder::create()
        ->orientation(miqu::Orientation::Vertical)
        ->gravity(miqu::Gravity::Center)
        ->spacing(6)
        ->padding(8, 6)
        ->addView(m_icon_view)
        ->addView(m_title_view)
        ->build();

    m_card = miqu::CardViewBuilder::create()
        ->backgroundColor(cfg.shortcut_background)
        ->stroke(1, cfg.shortcut_border_color)
        ->cornerRadius(cfg.shortcut_corner_radius)
        ->addView(content, miqu::LayoutParams(static_cast<int>(miqu::LayoutDimension::MatchParent), static_cast<int>(miqu::LayoutDimension::MatchParent)))
        ->build();

    m_card->set_on_click_listener([this]() {
        const auto& cfg = DeskConfig::get();
        if (!cfg.double_click_to_launch) {
            launch();
            return;
        }

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_click_time).count();
        if (m_last_click_time.time_since_epoch().count() > 0 && elapsed <= cfg.double_click_time_ms) {
            m_last_click_time = std::chrono::steady_clock::time_point{};
            launch();
        } else {
            m_last_click_time = now;
        }
    });
}

void AppShortcutWidget::launch() {
    if (m_shortcut.exec.empty()) return;

    std::string cmd = m_shortcut.exec;
    if (m_shortcut.terminal) {
        const char* term = getenv("TERMINAL");
        std::string term_bin = (term && *term) ? term : "kitty";
        cmd = term_bin + " -e " + cmd;
    }

    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
        _exit(1);
    }
}

void AppShortcutWidget::on_config_reload() {
    const auto& cfg = DeskConfig::get();
    if (m_card) {
        m_card->set_card_background_color(cfg.shortcut_background);
        m_card->set_stroke(1, cfg.shortcut_border_color);
        m_card->set_radius(cfg.shortcut_corner_radius);
    }
    if (m_title_view) {
        m_title_view->set_text(truncate_label(m_shortcut.name, cfg.desktop_max_chars));
        m_title_view->set_font_family(cfg.font_family);
        m_title_view->set_text_size(cfg.font_size);
        m_title_view->set_bold(cfg.shortcut_bold);
        m_title_view->set_text_color(cfg.font_color);
    }
}

} // namespace miqudesk
