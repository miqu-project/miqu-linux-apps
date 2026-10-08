#include "greeter_app.hpp"
#include <xkbcommon/xkbcommon-keysyms.h>
#include <pango/pangocairo.h>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <cstring>

using namespace miqu;

namespace miqudm {

class UserCardView : public View {
public:
    UserCardView(UserInfo user, bool is_selected = false)
        : m_user(std::move(user)), m_is_selected(is_selected) {}

    void set_selected(bool selected) {
        if (m_is_selected != selected) {
            m_is_selected = selected;
            request_redraw();
        }
    }

    Size measure_size() const override {
        return Size(104, 116);
    }

    void draw(cairo_t* cr, const Rect& bounds) override {
        if (!cr || bounds.width <= 0 || bounds.height <= 0) return;

        auto config = Config::get();
        cairo_save(cr);

        int pad = 2;
        int card_x = bounds.x + pad;
        int card_y = bounds.y + pad;
        int card_w = bounds.width - (2 * pad);
        int card_h = bounds.height - (2 * pad);
        double card_radius = 12.0;

        // 1. Draw Card Background & Selection Highlight
        CardView::draw_rounded_rect(cr, card_x, card_y, card_w, card_h, card_radius);
        if (m_is_selected) {
            cairo_set_source_rgba(cr, config->colors.primary.r,
                                      config->colors.primary.g,
                                      config->colors.primary.b,
                                      0.16f);
            cairo_fill_preserve(cr);

            cairo_set_source_rgba(cr, config->colors.primary.r,
                                      config->colors.primary.g,
                                      config->colors.primary.b,
                                      0.85f);
            cairo_set_line_width(cr, 1.6);
            cairo_stroke(cr);
        } else {
            cairo_set_source_rgba(cr, config->colors.surface_variant.r,
                                      config->colors.surface_variant.g,
                                      config->colors.surface_variant.b,
                                      0.25f);
            cairo_fill_preserve(cr);

            cairo_set_source_rgba(cr, config->colors.outline_variant.r,
                                      config->colors.outline_variant.g,
                                      config->colors.outline_variant.b,
                                      0.30f);
            cairo_set_line_width(cr, 1.0);
            cairo_stroke(cr);
        }

        // 2. Avatar Circle
        int avatar_size = 50;
        int avatar_x = card_x + (card_w - avatar_size) / 2;
        int avatar_y = card_y + 12;

        cairo_save(cr);
        cairo_arc(cr, avatar_x + avatar_size / 2.0, avatar_y + avatar_size / 2.0, avatar_size / 2.0, 0, 2 * M_PI);
        if (m_is_selected) {
            cairo_set_source_rgba(cr, config->colors.primary.r, config->colors.primary.g, config->colors.primary.b, 0.95f);
        } else {
            cairo_set_source_rgba(cr, config->colors.surface_variant.r, config->colors.surface_variant.g, config->colors.surface_variant.b, 0.85f);
        }
        cairo_fill_preserve(cr);

        cairo_set_source_rgba(cr, config->colors.outline.r, config->colors.outline.g, config->colors.outline.b, 0.35f);
        cairo_set_line_width(cr, 1.2);
        cairo_stroke(cr);
        cairo_restore(cr);

        // 3. Avatar Initial Letter
        std::string initial = m_user.display_name.empty() ? "?" : m_user.display_name.substr(0, 1);
        if (initial >= "a" && initial <= "z") initial[0] = toupper(initial[0]);

        PangoLayout* initial_layout = pango_cairo_create_layout(cr);
        pango_layout_set_text(initial_layout, initial.c_str(), -1);

        std::string font_family = config->metrics.font_family.empty() ? "Sans" : config->metrics.font_family;
        std::string init_font = font_family + " Bold 18";
        PangoFontDescription* idesc = pango_font_description_from_string(init_font.c_str());
        pango_layout_set_font_description(initial_layout, idesc);
        pango_font_description_free(idesc);

        int init_w = 0, init_h = 0;
        pango_layout_get_pixel_size(initial_layout, &init_w, &init_h);
        int init_x = avatar_x + (avatar_size - init_w) / 2;
        int init_y = avatar_y + (avatar_size - init_h) / 2;

        cairo_move_to(cr, init_x, init_y);
        if (m_is_selected) {
            cairo_set_source_rgba(cr, config->colors.on_primary.r, config->colors.on_primary.g, config->colors.on_primary.b, 1.0f);
        } else {
            cairo_set_source_rgba(cr, config->colors.primary.r, config->colors.primary.g, config->colors.primary.b, 1.0f);
        }
        pango_cairo_show_layout(cr, initial_layout);
        g_object_unref(initial_layout);

        // 4. User Display Name (Centered)
        PangoLayout* name_layout = pango_cairo_create_layout(cr);
        pango_layout_set_text(name_layout, m_user.display_name.c_str(), -1);
        std::string name_font = font_family + (m_is_selected ? " Bold 11" : " 11");
        PangoFontDescription* ndesc = pango_font_description_from_string(name_font.c_str());
        pango_layout_set_font_description(name_layout, ndesc);
        pango_font_description_free(ndesc);

        int text_area_w = card_w - 8;
        pango_layout_set_width(name_layout, text_area_w * PANGO_SCALE);
        pango_layout_set_ellipsize(name_layout, PANGO_ELLIPSIZE_END);
        pango_layout_set_alignment(name_layout, PANGO_ALIGN_CENTER);

        int name_y = avatar_y + avatar_size + 10;
        cairo_move_to(cr, card_x + 4, name_y);
        cairo_set_source_rgba(cr, config->colors.on_surface.r, config->colors.on_surface.g, config->colors.on_surface.b, 1.0f);
        pango_cairo_show_layout(cr, name_layout);
        g_object_unref(name_layout);

        cairo_restore(cr);
    }

    const UserInfo& get_user() const { return m_user; }

private:
    UserInfo m_user;
    bool m_is_selected = false;
};

static std::string get_current_time_str() {
    std::time_t now = std::time(nullptr);
    std::tm* tm = std::localtime(&now);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%H:%M", tm);
    return buf;
}

static std::string get_current_date_str() {
    std::time_t now = std::time(nullptr);
    std::tm* tm = std::localtime(&now);
    char buf[128];
    std::strftime(buf, sizeof(buf), "%A, %B %d", tm);
    return buf;
}

GreeterApp::GreeterApp(std::shared_ptr<AppEngine> engine, bool test_mode)
    : m_engine(std::move(engine)), m_test_mode(test_mode) {
}

bool GreeterApp::init() {
    if (!m_engine) return false;

    // Load users and sessions: try IPC first, fallback to local scanner if standalone/test
    bool loaded_ipc = false;
    if (!m_test_mode) {
        if (m_ipc.get_users(m_users) && m_ipc.get_sessions(m_sessions)) {
            loaded_ipc = true;
        }
    }

    if (!loaded_ipc || m_users.empty()) {
        m_users = UserScanner::scan_users();
    }
    if (!loaded_ipc || m_sessions.empty()) {
        m_sessions = SessionScanner::scan_all();
    }

    setup_ui();
    return m_window != nullptr;
}

void GreeterApp::setup_ui() {
    // 1. Root FrameLayout
    auto root_frame = std::make_shared<FrameLayout>();
    root_frame->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));

    // 2. Power Actions Bar (Top-Right)
    auto power_actions = std::make_shared<LinearLayout>(Orientation::Horizontal);
    power_actions->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::Right | Gravity::Top
    ));
    power_actions->set_margin(0, 16, 20, 0);

    auto sleep_btn = ButtonBuilder::create()
        ->text("💤")
        ->flat(true)
        ->padding(10, 6)
        ->textSize(14)
        ->onClick([this]() { handle_power_action("suspend"); })
        ->build();
    sleep_btn->set_margin(0, 0, 4, 0);
    power_actions->add_view(sleep_btn);

    auto reboot_btn = ButtonBuilder::create()
        ->text("🔄")
        ->flat(true)
        ->padding(10, 6)
        ->textSize(14)
        ->onClick([this]() { handle_power_action("reboot"); })
        ->build();
    reboot_btn->set_margin(0, 0, 4, 0);
    power_actions->add_view(reboot_btn);

    auto shutdown_btn = ButtonBuilder::create()
        ->text("⏻")
        ->flat(true)
        ->padding(10, 6)
        ->textSize(14)
        ->onClick([this]() { handle_power_action("poweroff"); })
        ->build();
    power_actions->add_view(shutdown_btn);

    root_frame->add_view(power_actions);

    // 3. Centered Main Greeter Container
    int user_count = static_cast<int>(m_users.size());
    int grid_cols = std::min(5, std::max(1, user_count));
    int cell_w = 104;
    int cell_h = 116;
    int spacing = 12;

    int grid_w = grid_cols * cell_w + (grid_cols - 1) * spacing;
    int total_rows = (user_count + grid_cols - 1) / grid_cols;
    int grid_h = total_rows * cell_h + (total_rows - 1) * spacing;

    int center_box_w = std::max(340, grid_w);

    auto greeter_center = std::make_shared<LinearLayout>(Orientation::Vertical);
    greeter_center->set_layout_params(LayoutParams(
        center_box_w,
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::Center
    ));
    greeter_center->set_gravity(Gravity::CenterHorizontal);

    // Clock
    auto clock_label = TextViewBuilder::create()
        ->text(get_current_time_str())
        ->textSize(52)
        ->bold(true)
        ->textAlignment(TextAlignment::Center)
        ->build();
    clock_label->set_margin(0, 0, 0, 2);
    greeter_center->add_view(clock_label);

    // Date
    auto date_label = TextViewBuilder::create()
        ->text(get_current_date_str())
        ->textSize(13)
        ->muted(true)
        ->textAlignment(TextAlignment::Center)
        ->build();
    date_label->set_margin(0, 0, 0, 28);
    greeter_center->add_view(date_label);

    // GridView for User Selection
    m_users_grid = GridViewBuilder::create()
        ->numColumns(grid_cols)
        ->cellSize(cell_w, cell_h)
        ->spacing(spacing, spacing)
        ->stretchMode(StretchMode::None)
        ->build();

    m_users_grid->set_layout_params(LayoutParams(
        grid_w,
        grid_h,
        Gravity::CenterHorizontal
    ));
    m_users_grid->set_margin(0, 0, 0, 24);

    for (size_t i = 0; i < m_users.size(); ++i) {
        auto card = std::make_shared<UserCardView>(m_users[i], i == m_selected_user_idx);
        m_users_grid->add_item(card);
    }

    m_users_grid->set_selected_index(static_cast<int>(m_selected_user_idx));
    m_users_grid->set_on_item_click_listener([this](size_t idx, std::shared_ptr<View>) {
        select_user(idx);
    });

    greeter_center->add_view(m_users_grid);

    // Auth Form Container (Compact 300px width)
    int form_w = 300;
    auto auth_form = std::make_shared<LinearLayout>(Orientation::Vertical);
    auth_form->set_layout_params(LayoutParams(
        form_w,
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterHorizontal
    ));

    // Password Input
    m_password_input = EditTextBuilder::create()
        ->hint("Password")
        ->passwordMode(true)
        ->focused(true)
        ->padding(14, 11)
        ->onSubmit([this](const std::string&) { attempt_login(); })
        ->build();
    m_password_input->set_layout_params(LayoutParams(
        form_w,
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    m_password_input->set_margin(0, 0, 0, 10);
    auth_form->add_view(m_password_input);

    // Session Dropdown (Spinner)
    std::vector<std::string> session_names;
    for (const auto& s : m_sessions) {
        session_names.push_back(s.name);
    }

    m_session_spinner = SpinnerBuilder::create()
        ->items(session_names)
        ->selectedIndex(m_selected_session_idx)
        ->padding(14, 11)
        ->onItemSelected([this](int idx, const std::string&) {
            select_session(idx);
        })
        ->build();
    m_session_spinner->set_layout_params(LayoutParams(
        form_w,
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    m_session_spinner->set_margin(0, 0, 0, 14);
    auth_form->add_view(m_session_spinner);

    // Sign In Button
    m_login_btn = ButtonBuilder::create()
        ->text("Sign In")
        ->primary(true)
        ->cornerRadius(8)
        ->padding(16, 11)
        ->bold(true)
        ->onClick([this]() { attempt_login(); })
        ->build();
    m_login_btn->set_layout_params(LayoutParams(
        form_w,
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    m_login_btn->set_margin(0, 0, 0, 10);
    auth_form->add_view(m_login_btn);

    // Status Label
    m_status_label = TextViewBuilder::create()
        ->text("")
        ->textSize(11)
        ->muted(true)
        ->textAlignment(TextAlignment::Center)
        ->build();
    m_status_label->set_layout_params(LayoutParams(
        form_w,
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    auth_form->add_view(m_status_label);

    greeter_center->add_view(auth_form);
    root_frame->add_view(greeter_center);

    // 4. Create Window
    m_window = WindowBuilder::create()
        ->title("miqudm - Greeter")
        ->appId("miqudm")
        ->role(WindowRole::Toplevel)
        ->preferredSize(680, 680)
        ->closeOnEscape(m_test_mode)
        ->contentView(root_frame)
        ->onClose([this]() {
            if (m_engine) m_engine->quit();
        })
        ->onKey([this](const KeyPressEvent& ev) {
            handle_key(ev);
        })
        ->build();

    if (m_window) {
        m_window->show();
    }
}

void GreeterApp::select_user(size_t index) {
    if (index >= m_users.size()) return;
    m_selected_user_idx = index;

    if (m_users_grid) {
        for (size_t i = 0; i < m_users.size(); ++i) {
            auto item = m_users_grid->get_item_at(i);
            if (auto card = std::dynamic_pointer_cast<UserCardView>(item)) {
                card->set_selected(i == m_selected_user_idx);
            }
        }
    }

    if (m_password_input) {
        m_password_input->clear();
        m_password_input->set_focused(true);
    }
    update_status("");
}

void GreeterApp::select_session(int index) {
    if (index < 0 || index >= static_cast<int>(m_sessions.size())) return;
    m_selected_session_idx = index;
    update_status("");
}

void GreeterApp::update_status(const std::string& msg, bool is_error) {
    if (m_status_label) {
        m_status_label->set_text(msg);
    }
    if (m_window) {
        m_window->schedule_redraw();
    }
}

void GreeterApp::attempt_login() {
    if (m_users.empty() || m_sessions.empty()) return;

    const auto& user = m_users[m_selected_user_idx];
    const auto& session = m_sessions[m_selected_session_idx];
    std::string password = m_password_input ? m_password_input->get_text() : "";

    update_status("Authenticating...");

    if (m_test_mode) {
        // Test mode: Simulated authentication
        std::string msg = "Test Mode: Authenticated as " + user.username + " (" + session.name + ")";
        update_status(msg);
        std::cout << "[miqudm-greeter] " << msg << std::endl;
        return;
    }

    std::string error_msg;
    bool success = m_ipc.login(
        user.username,
        password,
        session.exec,
        session.id,
        error_msg
    );

    // Wipe password string memory
    if (!password.empty()) {
        explicit_bzero(password.data(), password.size());
        password.clear();
    }

    if (!success) {
        update_status(error_msg.empty() ? "Authentication failed" : error_msg, true);
        if (m_password_input) {
            m_password_input->clear();
            m_password_input->set_focused(true);
        }
    } else {
        update_status("Login successful. Launching session...");
        if (m_window) {
            m_window->close();
        }
        if (m_engine) {
            m_engine->quit();
        }
    }
}

void GreeterApp::handle_power_action(const std::string& action) {
    update_status("Power: " + action);
    if (!m_test_mode) {
        m_ipc.power_action(action);
    }
}

void GreeterApp::handle_key(const KeyPressEvent& ev) {
    if (!ev.pressed) return;
    if (ev.keysym == XKB_KEY_q && (ev.modifiers & static_cast<uint32_t>(KeyboardModifier::Control))) {
        if (m_test_mode && m_engine) m_engine->quit();
    } else if (ev.keysym == XKB_KEY_Escape) {
        if (m_test_mode && m_engine) m_engine->quit();
    }
}

int GreeterApp::run() {
    if (!m_engine) return 1;
    return m_engine->enter_loop();
}

} // namespace miqudm
