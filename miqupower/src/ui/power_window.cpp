#include "ui/power_window.hpp"
#include "config/power_config.hpp"
#include <miqutoolkit/core/app_engine.hpp>
#include <miqutoolkit/view/button.hpp>
#include <miqutoolkit/view/image_view.hpp>

#include <iostream>
#include <fstream>
#include <algorithm>
#include <unistd.h>
#include <cstring>

namespace miqu {

namespace {

std::string get_system_summary() {
    std::string user = getenv("USER") ? getenv("USER") : "User";
    char host[256];
    if (gethostname(host, sizeof(host)) != 0) {
        std::strncpy(host, "miqu", sizeof(host));
    }
    std::string summary = user + "@" + host;

    std::ifstream upfile("/proc/uptime");
    double uptime_sec = 0;
    if (upfile >> uptime_sec) {
        int hours = static_cast<int>(uptime_sec) / 3600;
        int mins = (static_cast<int>(uptime_sec) % 3600) / 60;
        summary += "  •  Uptime " + std::to_string(hours) + "h " + std::to_string(mins) + "m";
    }
    return summary;
}

} // namespace

PowerWindow::PowerWindow() {
}

PowerWindow::~PowerWindow() {
    dismiss();
}

void PowerWindow::show(std::optional<PowerAction> direct_action) {
    const auto& cfg = PowerConfig::get();
    auto config = Config::get();

    int card_w = cfg.width > 0 ? cfg.width : 720;
    int card_h = cfg.height > 0 ? cfg.height : 460;

    // 1. Setup NavigationView
    m_nav_view = std::make_shared<NavigationView>();
    m_nav_view->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    m_nav_view->set_auto_back(true);
    m_nav_view->set_back_on_escape(false); // We handle escape explicitly in onKey

    // Push initial main page
    m_nav_view->push(create_main_page(), "main");

    // 2. Outer modal card with generous padding
    int radius = config->metrics.corner_radius > 0 ? config->metrics.corner_radius : 18;
    m_root_card = CardViewBuilder::create()
        ->backgroundColor(config->colors.background)
        ->stroke(config->metrics.border_width, config->colors.outline)
        ->cornerRadius(radius)
        ->elevation(0)
        ->padding(28, 24, 28, 24)
        ->addView(m_nav_view, LayoutParams(static_cast<int>(LayoutDimension::MatchParent), static_cast<int>(LayoutDimension::MatchParent)))
        ->build();

    // 3. Wayland Layer-shell Overlay Window
    m_window = WindowBuilder::create()
        ->role(WindowRole::LayerOverlay)
        ->appId("miqupower")
        ->keyboardInteractive(true)
        ->preferredSize(card_w, card_h)
        ->contentSize(card_w, card_h)
        ->closeOnClickOutside(cfg.close_on_click_outside)
        ->closeOnEscape(false)
        ->contentView(m_root_card)
        ->anchors(0)
        ->transparent(true)
        ->onKey([this](const KeyPressEvent& event) {
            handle_key_event(event);
        })
        ->onClose([this]() {
            dismiss();
        })
        ->build();

    if (!m_window) {
        std::cerr << "[miqupower] Failed to create overlay window" << std::endl;
        return;
    }

    m_direct_action = direct_action;
    if (direct_action.has_value()) {
        handle_action_selected(*direct_action);
    }

    m_window->show();
}

void PowerWindow::dismiss() {
    if (m_window) {
        m_window->close();
        m_window = nullptr;
    }
    auto engine = AppEngine::instance();
    if (engine) {
        engine->quit(0);
    }
}

std::shared_ptr<View> PowerWindow::create_main_page() {
    m_action_cards.clear();
    const auto& cfg = PowerConfig::get();
    auto config = Config::get();

    // Header Title & Subtitle
    auto headerTitle = TextViewBuilder::create()
        ->text("Session & Power")
        ->bold(true)
        ->textSize(18)
        ->textAlignment(TextAlignment::Center)
        ->textColor(config->colors.on_surface)
        ->build();

    auto headerSub = TextViewBuilder::create()
        ->text("Choose a power action or press a shortcut key")
        ->textSize(13)
        ->textAlignment(TextAlignment::Center)
        ->textColor(config->colors.on_surface_variant)
        ->build();

    auto headerLayout = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->gravity(Gravity::CenterHorizontal)
        ->spacing(4)
        ->addView(headerTitle, LayoutParams(static_cast<int>(LayoutDimension::MatchParent), static_cast<int>(LayoutDimension::WrapContent), Gravity::CenterHorizontal))
        ->addView(headerSub, LayoutParams(static_cast<int>(LayoutDimension::MatchParent), static_cast<int>(LayoutDimension::WrapContent), Gravity::CenterHorizontal))
        ->build();

    // Action Cards Row
    auto actionRowBuilder = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->gravity(Gravity::Center)
        ->spacing(14);

    auto add_card = [this, &actionRowBuilder](
        PowerAction act,
        const std::string& title,
        const std::string& icon,
        char hotkey,
        const std::string& hotkey_badge
    ) {
        auto card = std::make_shared<ActionCard>(
            act, title, icon, hotkey, hotkey_badge,
            [this](PowerAction a) { handle_action_selected(a); }
        );
        m_action_cards.push_back(card);
        actionRowBuilder->addView(card->get_view(), LayoutParams(122, 142));
    };

    if (cfg.enable_shutdown) {
        add_card(PowerAction::Shutdown, "Shut Down", "system-shutdown-symbolic", 's', "[S]");
    }
    if (cfg.enable_reboot) {
        add_card(PowerAction::Reboot, "Restart", "system-reboot-symbolic", 'r', "[R]");
    }
    if (cfg.enable_suspend) {
        add_card(PowerAction::Suspend, "Sleep", "weather-clear-night-symbolic", 'u', "[U]");
    }
    if (cfg.enable_lock) {
        add_card(PowerAction::Lock, "Lock", "system-lock-screen-symbolic", 'l', "[L]");
    }
    if (cfg.enable_logout) {
        add_card(PowerAction::Logout, "Log Out", "system-log-out-symbolic", 'e', "[E]");
    }
    if (cfg.enable_hibernate) {
        add_card(PowerAction::Hibernate, "Hibernate", "drive-harddisk-symbolic", 'h', "[H]");
    }

    auto actionRow = actionRowBuilder->build();

    // Bottom System summary badge
    auto summaryText = TextViewBuilder::create()
        ->text(get_system_summary())
        ->textSize(11)
        ->textAlignment(TextAlignment::Center)
        ->textColor(config->colors.on_surface_variant)
        ->build();

    auto summaryCard = CardViewBuilder::create()
        ->backgroundColor(config->colors.surface_variant)
        ->stroke(1, config->colors.outline_variant)
        ->cornerRadius(10)
        ->elevation(0)
        ->padding(16, 6, 16, 6)
        ->addView(summaryText, LayoutParams(static_cast<int>(LayoutDimension::WrapContent), 18, Gravity::Center))
        ->build();

    // Root page container with comfortable vertical layout
    auto pageLayout = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->gravity(Gravity::CenterHorizontal)
        ->spacing(28)
        ->addView(headerLayout, LayoutParams(static_cast<int>(LayoutDimension::MatchParent), static_cast<int>(LayoutDimension::WrapContent), Gravity::CenterHorizontal))
        ->addView(actionRow, LayoutParams(static_cast<int>(LayoutDimension::WrapContent), 142, Gravity::CenterHorizontal))
        ->addView(summaryCard, LayoutParams(static_cast<int>(LayoutDimension::WrapContent), static_cast<int>(LayoutDimension::WrapContent), Gravity::CenterHorizontal))
        ->build();

    select_card_index(0);
    return pageLayout;
}

std::shared_ptr<View> PowerWindow::create_warning_page(PowerAction action, const std::vector<AppInfo>& apps) {
    auto config = Config::get();

    // 1. Header: Icon + Title + Description
    auto iconView = ImageViewBuilder::create()
        ->source("dialog-warning-symbolic")
        ->targetSize(38)
        ->build();

    auto titleView = TextViewBuilder::create()
        ->text("Applications Are Still Running")
        ->bold(true)
        ->textSize(17)
        ->textAlignment(TextAlignment::Center)
        ->textColor(config->colors.on_surface)
        ->build();

    std::string action_name = (action == PowerAction::Shutdown) ? "shut down" :
                              (action == PowerAction::Reboot) ? "restart" : "log out";
    std::string desc = "If you " + action_name + " now, unsaved changes in open applications may be lost.";

    auto descView = TextViewBuilder::create()
        ->text(desc)
        ->textSize(13)
        ->multiline(true)
        ->wrap(true)
        ->ellipsize(false)
        ->textAlignment(TextAlignment::Center)
        ->textColor(config->colors.on_surface_variant)
        ->build();

    auto headerLayout = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->gravity(Gravity::CenterHorizontal)
        ->spacing(4)
        ->addView(iconView, LayoutParams(38, 38, Gravity::CenterHorizontal))
        ->addView(titleView, LayoutParams(static_cast<int>(LayoutDimension::MatchParent), static_cast<int>(LayoutDimension::WrapContent), Gravity::CenterHorizontal))
        ->addView(descView, LayoutParams(static_cast<int>(LayoutDimension::MatchParent), static_cast<int>(LayoutDimension::WrapContent), Gravity::CenterHorizontal))
        ->build();

    // 2. Scrollable App List
    auto appsListLayout = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(8);

    for (const auto& app : apps) {
        std::string icon_name = app.app_id.empty() ? "application-x-executable" : app.app_id;
        auto appIcon = ImageViewBuilder::create()
            ->source(icon_name)
            ->targetSize(26)
            ->build();

        std::string display_title = app.title.empty() ? (app.app_id.empty() ? "Application Window" : app.app_id) : app.title;
        auto titleText = TextViewBuilder::create()
            ->text(display_title)
            ->bold(true)
            ->textSize(13)
            ->ellipsize(true)
            ->textColor(config->colors.on_surface)
            ->build();

        std::string display_sub = app.app_id.empty() ? "Unknown App" : app.app_id;
        auto subText = TextViewBuilder::create()
            ->text(display_sub)
            ->textSize(11)
            ->textColor(config->colors.on_surface_variant)
            ->build();

        auto textContainer = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->gravity(Gravity::CenterVertical)
            ->spacing(2)
            ->addView(titleText, LayoutParams(static_cast<int>(LayoutDimension::MatchParent), static_cast<int>(LayoutDimension::WrapContent)))
            ->addView(subText, LayoutParams(static_cast<int>(LayoutDimension::MatchParent), static_cast<int>(LayoutDimension::WrapContent)))
            ->build();

        std::string ws_label = "WS " + std::to_string(app.workspace_id);
        auto wsText = TextViewBuilder::create()
            ->text(ws_label)
            ->bold(true)
            ->textSize(11)
            ->textAlignment(TextAlignment::Center)
            ->textColor(config->colors.primary)
            ->build();

        auto wsBadge = CardViewBuilder::create()
            ->backgroundColor(config->colors.surface)
            ->stroke(1, config->colors.outline_variant)
            ->cornerRadius(6)
            ->elevation(0)
            ->padding(8, 2, 8, 2)
            ->addView(wsText, LayoutParams(static_cast<int>(LayoutDimension::WrapContent), static_cast<int>(LayoutDimension::WrapContent), Gravity::Center))
            ->build();

        auto itemLayout = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->gravity(Gravity::CenterVertical)
            ->spacing(12)
            ->addView(appIcon, LayoutParams(26, 26))
            ->addView(textContainer, LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f))
            ->addView(wsBadge, LayoutParams(static_cast<int>(LayoutDimension::WrapContent), static_cast<int>(LayoutDimension::WrapContent)))
            ->build();

        auto itemCard = CardViewBuilder::create()
            ->backgroundColor(config->colors.surface_variant)
            ->stroke(1, config->colors.outline_variant)
            ->cornerRadius(10)
            ->elevation(0)
            ->padding(12, 8, 12, 8)
            ->addView(itemLayout, LayoutParams(static_cast<int>(LayoutDimension::MatchParent), static_cast<int>(LayoutDimension::MatchParent)))
            ->build();

        appsListLayout->addView(itemCard, LayoutParams(static_cast<int>(LayoutDimension::MatchParent), static_cast<int>(LayoutDimension::WrapContent)));
    }

    auto scrollView = ScrollViewBuilder::create()
        ->contentView(appsListLayout->build())
        ->scrollbar(true)
        ->build();

    // 3. Action Buttons
    auto cancelBtn = ButtonBuilder::create()
        ->text("Cancel  [Esc]")
        ->bold(true)
        ->cornerRadius(12)
        ->padding(24, 10)
        ->onClick([this]() {
            if (m_direct_action.has_value()) {
                dismiss();
                return;
            }
            if (m_nav_view) m_nav_view->pop();
            select_card_index(m_selected_index);
            if (m_window) m_window->schedule_redraw();
        })
        ->build();
    cancelBtn->set_custom_colors(config->colors.surface_variant, config->colors.on_surface);

    std::string proceed_text = (action == PowerAction::Shutdown) ? "Shut Down Anyway  [Enter]" :
                               (action == PowerAction::Reboot) ? "Restart Anyway  [Enter]" : "Log Out Anyway  [Enter]";

    auto proceedBtn = ButtonBuilder::create()
        ->text(proceed_text)
        ->bold(true)
        ->cornerRadius(12)
        ->padding(24, 10)
        ->onClick([this, action]() {
            execute_confirmed_action(action);
        })
        ->build();
    proceedBtn->set_custom_colors(config->colors.primary, config->colors.on_primary);

    auto buttonsLayout = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->gravity(Gravity::Center)
        ->spacing(16)
        ->addView(cancelBtn, LayoutParams(static_cast<int>(LayoutDimension::WrapContent), 42))
        ->addView(proceedBtn, LayoutParams(static_cast<int>(LayoutDimension::WrapContent), 42))
        ->build();

    // 4. Root Container Layout
    return LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(14)
        ->addView(headerLayout, LayoutParams(static_cast<int>(LayoutDimension::MatchParent), static_cast<int>(LayoutDimension::WrapContent), Gravity::CenterHorizontal))
        ->addView(scrollView, LayoutParams(static_cast<int>(LayoutDimension::MatchParent), 0, 1.0f))
        ->addView(buttonsLayout, LayoutParams(static_cast<int>(LayoutDimension::WrapContent), 42, Gravity::CenterHorizontal))
        ->build();
}

std::shared_ptr<View> PowerWindow::create_progress_page(PowerAction action) {
    auto config = Config::get();

    std::string icon_name = "system-shutdown-symbolic";
    std::string default_title = "Shutting Down Desktop...";
    std::string default_sub = "Gracefully closing applications and saving session state";

    if (action == PowerAction::Reboot) {
        icon_name = "system-reboot-symbolic";
        default_title = "Restarting System...";
        default_sub = "Closing open applications and restarting system";
    } else if (action == PowerAction::Logout) {
        icon_name = "system-log-out-symbolic";
        default_title = "Logging Out...";
        default_sub = "Terminating user desktop session";
    }

    auto iconView = ImageViewBuilder::create()
        ->source(icon_name)
        ->targetSize(48)
        ->build();

    m_progress_title = TextViewBuilder::create()
        ->text(default_title)
        ->bold(true)
        ->textSize(18)
        ->textAlignment(TextAlignment::Center)
        ->textColor(config->colors.on_surface)
        ->build();

    m_progress_sub = TextViewBuilder::create()
        ->text(default_sub)
        ->textSize(13)
        ->textAlignment(TextAlignment::Center)
        ->textColor(config->colors.on_surface_variant)
        ->build();

    auto progressBar = std::make_shared<ProgressBar>();
    progressBar->set_indeterminate(true);
    progressBar->set_style(ProgressBarStyle::Linear);
    progressBar->set_track_height(8);
    progressBar->set_corner_radius(4);
    progressBar->set_progress_color(config->colors.primary);
    progressBar->set_track_color(config->colors.surface_variant);

    return LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->gravity(Gravity::Center)
        ->spacing(20)
        ->addView(iconView, LayoutParams(48, 48, Gravity::CenterHorizontal))
        ->addView(m_progress_title, LayoutParams(static_cast<int>(LayoutDimension::MatchParent), 28, Gravity::CenterHorizontal))
        ->addView(m_progress_sub, LayoutParams(static_cast<int>(LayoutDimension::MatchParent), 22, Gravity::CenterHorizontal))
        ->addView(progressBar, LayoutParams(320, 8, Gravity::CenterHorizontal))
        ->build();
}

void PowerWindow::select_card_index(int index) {
    if (m_action_cards.empty()) return;
    int safe_idx = std::clamp(index, 0, static_cast<int>(m_action_cards.size()) - 1);
    m_selected_index = safe_idx;

    for (size_t i = 0; i < m_action_cards.size(); ++i) {
        m_action_cards[i]->set_selected(static_cast<int>(i) == safe_idx);
    }
}

void PowerWindow::navigate_selection(int direction) {
    if (m_action_cards.empty()) return;
    int count = static_cast<int>(m_action_cards.size());
    int next = (m_selected_index + direction) % count;
    if (next < 0) next += count;
    select_card_index(next);
}

void PowerWindow::handle_action_selected(PowerAction action) {
    m_pending_action = action;
    const auto& cfg = PowerConfig::get();

    // Immediate non-destructive actions (Lock, Suspend, Hibernate)
    if (action == PowerAction::Lock || action == PowerAction::Suspend || action == PowerAction::Hibernate) {
        execute_confirmed_action(action);
        return;
    }

    // Check for running applications if warning is enabled
    if (cfg.warn_running_apps) {
        auto apps = SystemPower::instance().get_running_apps();
        if (!apps.empty()) {
            if (m_nav_view) {
                m_nav_view->push(create_warning_page(action, apps), "warning");
                if (m_window) m_window->schedule_redraw();
            }
            return;
        }
    }

    execute_confirmed_action(action);
}

void PowerWindow::execute_confirmed_action(PowerAction action) {
    if (action == PowerAction::Shutdown || action == PowerAction::Reboot || action == PowerAction::Logout) {
        if (m_nav_view) {
            m_nav_view->push(create_progress_page(action), "progress");
            if (m_window) m_window->schedule_redraw();
        }
    }

    SystemPower::instance().execute_action(
        action,
        [this](const std::string& title, const std::string& subtitle) {
            if (m_progress_title) {
                m_progress_title->set_text(title);
            }
            if (m_progress_sub) {
                m_progress_sub->set_text(subtitle);
            }
            if (m_window) m_window->schedule_redraw();
        },
        [this]() {
            dismiss();
        }
    );
}

void PowerWindow::handle_key_event(const KeyPressEvent& event) {
    if (!event.pressed) return;

    // Check if on warning or progress page
    if (m_nav_view && m_nav_view->get_depth() > 1) {
        if (event.keysym == XKB_KEY_Escape) {
            if (m_direct_action.has_value()) {
                dismiss();
                return;
            }
            m_nav_view->pop();
            select_card_index(m_selected_index);
            if (m_window) m_window->schedule_redraw();
            return;
        }
        if (event.keysym == XKB_KEY_Return || event.keysym == XKB_KEY_KP_Enter) {
            execute_confirmed_action(m_pending_action);
            return;
        }
        return;
    }

    // Main Page Key Navigation
    if (event.keysym == XKB_KEY_Escape) {
        dismiss();
        return;
    }
    if (event.keysym == XKB_KEY_Left || event.keysym == XKB_KEY_Up) {
        navigate_selection(-1);
        return;
    }
    if (event.keysym == XKB_KEY_Right || event.keysym == XKB_KEY_Down || event.keysym == XKB_KEY_Tab) {
        navigate_selection(1);
        return;
    }
    if (event.keysym == XKB_KEY_Return || event.keysym == XKB_KEY_KP_Enter) {
        if (m_selected_index >= 0 && m_selected_index < static_cast<int>(m_action_cards.size())) {
            handle_action_selected(m_action_cards[m_selected_index]->get_action());
        }
        return;
    }

    // Direct hotkey triggering
    char key_char = '\0';
    if (!event.utf8_text.empty()) {
        key_char = std::tolower(event.utf8_text[0]);
    }
    for (const auto& card : m_action_cards) {
        if (card->get_hotkey() == key_char) {
            handle_action_selected(card->get_action());
            return;
        }
    }
}

} // namespace miqu
