#include "greeter_app.hpp"
#include "common/config.hpp"
#include <miqutoolkit/core/config.hpp>
#include <miqutoolkit/view/image_view.hpp>
#include <miqutoolkit/view/image_button.hpp>
#include <miqutoolkit/view/popup_window.hpp>
#include <xkbcommon/xkbcommon-keysyms.h>

#include <ctime>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace miqudm {

using namespace miqu;

GreeterApp::GreeterApp(bool test_mode, std::string config_path)
    : m_test_mode(test_mode),
      m_config_path(std::move(config_path)),
      m_ipc_client(std::make_shared<IpcClient>(test_mode)) {}

bool GreeterApp::init(std::shared_ptr<AppEngine> engine) {
    m_engine = engine;

    // Load configuration and theme from custom path, /etc/miqudm/miqudm.conf, or system baseline
    ConfigManager::get().load_greeter_config(m_config_path);

    if (!m_test_mode) {
        if (!m_ipc_client->connect_to_daemon()) {
            std::cerr << "[miqudm-greeter] Error: Cannot connect to miqudm daemon at /run/miqudm/socket. Is miqudm.service running?\n"
                      << "               (To preview the greeter UI offline without a running daemon, run: miqudm-greeter --test)\n";
            return false;
        }
    }

    setup_ui();

    // Request initial data from daemon
    m_ipc_client->request_initial_data([this](const std::vector<UserInfo>& users,
                                              const std::vector<SessionInfo>& sessions,
                                              const std::string& default_session,
                                              const std::string& last_user) {
        on_initial_data(users, sessions, default_session, last_user);
    });

    // 1-second clock timer
    m_engine->add_repeating_timer(1000, [this]() {
        update_clock();
    });

    // 50ms IPC response poll timer
    if (!m_test_mode) {
        m_engine->add_repeating_timer(50, [this]() {
            m_ipc_client->poll_responses();
        });
    }

    return true;
}

void GreeterApp::setup_ui() {
    auto config = Config::get();
    const auto& g_conf = ConfigManager::get().greeter();

    int win_w = m_test_mode ? 960 : 1920;
    int win_h = m_test_mode ? 600 : 1080;

    // Root FrameLayout
    auto root = std::make_shared<FrameLayout>();
    root->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    root->set_background_color(config->colors.background);

    // Optional Wallpaper
    if (!g_conf.wallpaper.empty()) {
        auto wallpaper_view = std::make_shared<ImageView>();
        wallpaper_view->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::MatchParent)
        ));
        wallpaper_view->set_image_resource(g_conf.wallpaper);
        wallpaper_view->set_fit_mode(FitMode::Cover);
        root->add_view(wallpaper_view);
    }

    // Top Bar (Clock + Power Actions)
    auto top_bar = std::make_shared<LinearLayout>(Orientation::Horizontal);
    top_bar->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::Top
    ));
    top_bar->set_padding(28);

    // Left: Clock & Date
    auto clock_layout = std::make_shared<LinearLayout>(Orientation::Vertical);
    clock_layout->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::Left | Gravity::CenterVertical
    ));

    m_time_label = TextViewBuilder::create()
        ->text("00:00")
        ->textSize(34)
        ->bold(true)
        ->textColor(config->colors.on_background)
        ->build();
    m_time_label->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    clock_layout->add_view(m_time_label);

    m_date_label = TextViewBuilder::create()
        ->text("Loading...")
        ->textSize(14)
        ->bold(false)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    m_date_label->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    m_date_label->set_margin(0, 4, 0, 0);
    clock_layout->add_view(m_date_label);

    top_bar->add_view(clock_layout);

    // Flexible Spacer
    auto spacer = std::make_shared<FrameLayout>();
    LayoutParams spacer_lp(
        0, 1,
        Gravity::CenterVertical
    );
    spacer_lp.weight = 1.0f;
    spacer->set_layout_params(spacer_lp);
    top_bar->add_view(spacer);

    // Right: Power Menu
    if (g_conf.show_power_buttons) {
        auto power_layout = std::make_shared<LinearLayout>(Orientation::Horizontal);
        power_layout->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::WrapContent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::Right | Gravity::CenterVertical
        ));

        // Suspend Button
        auto suspend_btn = std::make_shared<Button>("Suspend");
        suspend_btn->set_style(ButtonStyle::Standard);
        suspend_btn->set_layout_params(LayoutParams(static_cast<int>(LayoutDimension::WrapContent), 36));
        suspend_btn->set_on_click_listener([this]() {
            m_ipc_client->send_power_action(PowerAction::Suspend, [](bool) {});
        });
        power_layout->add_view(suspend_btn);

        // Reboot Button
        auto reboot_btn = std::make_shared<Button>("Reboot");
        reboot_btn->set_style(ButtonStyle::Standard);
        reboot_btn->set_layout_params(LayoutParams(static_cast<int>(LayoutDimension::WrapContent), 36));
        reboot_btn->set_margin(8, 0, 0, 0);
        reboot_btn->set_on_click_listener([this]() {
            m_ipc_client->send_power_action(PowerAction::Reboot, [](bool) {});
        });
        power_layout->add_view(reboot_btn);

        // Poweroff Button
        auto poweroff_btn = std::make_shared<Button>("Shut Down");
        poweroff_btn->set_style(ButtonStyle::Standard);
        poweroff_btn->set_layout_params(LayoutParams(static_cast<int>(LayoutDimension::WrapContent), 36));
        poweroff_btn->set_margin(8, 0, 0, 0);
        poweroff_btn->set_on_click_listener([this]() {
            m_ipc_client->send_power_action(PowerAction::PowerOff, [](bool) {});
        });
        power_layout->add_view(poweroff_btn);

        top_bar->add_view(power_layout);
    }

    root->add_view(top_bar);

    // Center Login Card
    auto card = std::make_shared<CardView>();
    card->set_layout_params(LayoutParams(
        380,
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::Center
    ));
    card->set_style(CardStyle::Elevated);
    card->set_radius(g_conf.card_radius);
    card->set_padding(32);

    auto card_content = std::make_shared<LinearLayout>(Orientation::Vertical);
    card_content->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    // Avatar
    m_avatar = AvatarViewBuilder::create()
        ->size(g_conf.avatar_size)
        ->circle()
        ->bold(true)
        ->textSize(24)
        ->build();
    m_avatar->set_layout_params(LayoutParams(
        g_conf.avatar_size, g_conf.avatar_size,
        Gravity::CenterHorizontal
    ));
    m_avatar->set_margin(0, 0, 0, 16);
    card_content->add_view(m_avatar);

    // Display Name
    m_user_name_label = TextViewBuilder::create()
        ->text("User")
        ->textSize(20)
        ->bold(true)
        ->center()
        ->textColor(config->colors.on_surface)
        ->build();
    m_user_name_label->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterHorizontal
    ));
    m_user_name_label->set_margin(0, 0, 0, 8);
    card_content->add_view(m_user_name_label);

    // User Switcher Spinner (if multiple users)
    m_user_spinner = std::make_shared<Spinner>();
    m_user_spinner->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        36,
        Gravity::CenterHorizontal
    ));
    m_user_spinner->set_margin(0, 0, 0, 14);
    m_user_spinner->set_on_item_selected_listener([this](int idx, const std::string&) {
        on_user_selected(idx);
    });
    card_content->add_view(m_user_spinner);

    // Password input
    m_password_input = EditTextBuilder::create()
        ->hint("Password")
        ->build();
    m_password_input->set_password_mode(true);
    m_password_input->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        44,
        Gravity::CenterHorizontal
    ));
    m_password_input->set_margin(0, 0, 0, 8);
    m_password_input->set_on_submit_listener([this](const std::string&) {
        do_login();
    });
    card_content->add_view(m_password_input);

    // Status / Error message
    m_status_label = TextViewBuilder::create()
        ->text("")
        ->textSize(12)
        ->center()
        ->textColor(Color(0.85f, 0.25f, 0.25f, 1.0f))
        ->build();
    m_status_label->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        22,
        Gravity::CenterHorizontal
    ));
    m_status_label->set_margin(0, 0, 0, 14);
    card_content->add_view(m_status_label);

    // Bottom Action Row (Session picker + Sign In button)
    auto action_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    action_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        42,
        Gravity::CenterHorizontal
    ));

    // Session Picker
    m_session_spinner = std::make_shared<Spinner>();
    LayoutParams session_lp(
        0, 42,
        Gravity::CenterVertical
    );
    session_lp.weight = 1.0f;
    m_session_spinner->set_layout_params(session_lp);
    m_session_spinner->set_on_item_selected_listener([this](int idx, const std::string&) {
        on_session_selected(idx);
    });
    action_row->add_view(m_session_spinner);

    // Login Button
    m_login_button = std::make_shared<Button>("Sign In");
    m_login_button->set_style(ButtonStyle::Primary);
    m_login_button->set_layout_params(LayoutParams(
        100, 42,
        Gravity::CenterVertical
    ));
    m_login_button->set_margin(10, 0, 0, 0);
    m_login_button->set_on_click_listener([this]() {
        do_login();
    });
    action_row->add_view(m_login_button);

    card_content->add_view(action_row);
    card->add_view(card_content);
    root->add_view(card);

    // Build Wayland Window
    m_window = WindowBuilder::create()
        ->title("miqudm - Login")
        ->appId("miqudm-greeter")
        ->role(WindowRole::Toplevel)
        ->preferredSize(win_w, win_h)
        ->contentView(root)
        ->closeOnEscape(false)
        ->onKey([this](const KeyPressEvent& ev) {
            if (!ev.pressed) return;
            if (ev.keysym == XKB_KEY_Escape && m_test_mode) {
                m_engine->quit();
            } else if (ev.keysym == XKB_KEY_Return || ev.keysym == XKB_KEY_KP_Enter) {
                do_login();
            }
        })
        ->onClose([this]() {
            m_engine->quit();
        })
        ->build();

    if (m_window) {
        m_window->show();
        if (m_password_input) {
            m_password_input->request_focus();
        }
    }

    update_clock();
}

void GreeterApp::update_clock() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::localtime(&now_c);

    bool is_24h = ConfigManager::get().greeter().clock_format_24h;

    std::stringstream time_ss;
    if (is_24h) {
        time_ss << std::put_time(tm, "%H:%M");
    } else {
        time_ss << std::put_time(tm, "%I:%M %p");
    }

    std::stringstream date_ss;
    date_ss << std::put_time(tm, "%A, %B %d");

    if (m_time_label) m_time_label->set_text(time_ss.str());
    if (m_date_label) m_date_label->set_text(date_ss.str());
}

void GreeterApp::on_initial_data(const std::vector<UserInfo>& users,
                                 const std::vector<SessionInfo>& sessions,
                                 const std::string& default_session,
                                 const std::string& last_user) {
    std::cout << "[miqudm-greeter] Received initial data: " << users.size() << " users, "
              << sessions.size() << " sessions." << std::endl;

    m_users = users;
    m_sessions = sessions;

    // 1. Populate Users
    std::vector<std::string> user_items;
    int target_user_idx = 0;

    for (size_t i = 0; i < m_users.size(); ++i) {
        user_items.push_back(m_users[i].display_name);
        if (!last_user.empty() && m_users[i].username == last_user) {
            target_user_idx = static_cast<int>(i);
        }
    }

    if (m_user_spinner) {
        m_user_spinner->set_items(user_items);
        if (!user_items.empty()) {
            m_user_spinner->set_selected_index(target_user_idx);
        }
        // Only show spinner if multiple users exist and switching is allowed
        bool show_user_spinner = (m_users.size() > 1 && ConfigManager::get().greeter().allow_user_switching);
        m_user_spinner->set_visibility(show_user_spinner ? Visibility::Visible : Visibility::Gone);
    }

    on_user_selected(target_user_idx);

    // 2. Populate Sessions
    std::vector<std::string> session_items;
    int target_sess_idx = 0;

    for (size_t i = 0; i < m_sessions.size(); ++i) {
        session_items.push_back(m_sessions[i].name);
        if (!default_session.empty() && (m_sessions[i].id == default_session || m_sessions[i].name == default_session)) {
            target_sess_idx = static_cast<int>(i);
        }
    }

    if (m_session_spinner) {
        m_session_spinner->set_items(session_items);
        if (!session_items.empty()) {
            m_session_spinner->set_selected_index(target_sess_idx);
        }
        m_session_spinner->set_visibility(ConfigManager::get().greeter().show_sessions ? Visibility::Visible : Visibility::Gone);
    }

    on_session_selected(target_sess_idx);

    if (m_password_input) {
        m_password_input->request_focus();
    }
    if (m_window) {
        m_window->schedule_redraw();
    }
}

void GreeterApp::on_user_selected(int index) {
    if (index < 0 || index >= static_cast<int>(m_users.size())) return;
    m_selected_user_idx = index;

    const auto& user = m_users[m_selected_user_idx];
    if (m_user_name_label) {
        m_user_name_label->set_text(user.display_name);
    }

    if (m_avatar) {
        if (!user.avatar_path.empty()) {
            m_avatar->set_image_resource(user.avatar_path);
        } else {
            std::string initial = user.display_name.empty() ? "?" : user.display_name.substr(0, 1);
            if (initial >= "a" && initial <= "z") initial[0] = toupper(initial[0]);
            m_avatar->set_text(initial);
        }
    }

    if (m_password_input) {
        m_password_input->clear();
        m_password_input->request_focus();
    }
    set_status("");

    if (m_window) {
        m_window->schedule_redraw();
    }
}

void GreeterApp::on_session_selected(int index) {
    if (index < 0 || index >= static_cast<int>(m_sessions.size())) return;
    m_selected_session_idx = index;
    if (m_window) {
        m_window->schedule_redraw();
    }
}

void GreeterApp::set_status(const std::string& msg, bool is_error) {
    if (!m_status_label) return;
    auto config = Config::get();
    m_status_label->set_text_color(is_error ? Color(0.85f, 0.25f, 0.25f, 1.0f) : config->colors.on_surface_variant);
    m_status_label->set_text(msg);
    if (m_window) {
        m_window->schedule_redraw();
    }
}

void GreeterApp::do_login() {
    if (m_authenticating || m_users.empty()) return;

    std::string password = m_password_input ? m_password_input->get_text() : "";
    if (password.empty() && !m_test_mode) {
        set_status("Please enter your password", true);
        return;
    }

    m_authenticating = true;
    set_status("Authenticating...", false);

    const auto& user = m_users[m_selected_user_idx];
    std::string session_id = m_sessions.empty() ? "miquland" : m_sessions[m_selected_session_idx].id;

    m_ipc_client->authenticate(user.username, password, [this, user, session_id](bool success, const std::string& msg) {
        if (!success) {
            m_authenticating = false;
            set_status(msg.empty() ? "Authentication failed" : msg, true);
            if (m_password_input) m_password_input->clear();
        } else {
            set_status("Launching desktop session...", false);
            m_ipc_client->start_session(user.username, session_id, [this](bool start_ok, const std::string& start_msg) {
                if (start_ok) {
                    // Session successfully spawned, cleanly exit greeter
                    m_engine->quit(0);
                } else {
                    m_authenticating = false;
                    set_status(start_msg.empty() ? "Failed to start session" : start_msg, true);
                }
            });
        }
    });
}

} // namespace miqudm
