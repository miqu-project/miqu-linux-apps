#pragma once

#include "ipc_client.hpp"
#include <miqutoolkit/core/app_engine.hpp>
#include <miqutoolkit/core/window.hpp>
#include <miqutoolkit/view/text_view.hpp>
#include <miqutoolkit/view/avatar_view.hpp>
#include <miqutoolkit/view/edit_text.hpp>
#include <miqutoolkit/view/spinner.hpp>
#include <miqutoolkit/view/button.hpp>
#include <miqutoolkit/view/card_view.hpp>
#include <miqutoolkit/view/linear_layout.hpp>
#include <miqutoolkit/view/frame_layout.hpp>

#include <memory>
#include <vector>

namespace miqudm {

class GreeterApp {
public:
    explicit GreeterApp(bool test_mode = false, std::string config_path = "");
    ~GreeterApp() = default;

    bool init(std::shared_ptr<miqu::AppEngine> engine);

private:
    void setup_ui();
    void update_clock();
    void on_initial_data(const std::vector<UserInfo>& users,
                         const std::vector<SessionInfo>& sessions,
                         const std::string& default_session,
                         const std::string& last_user);
    void on_user_selected(int index);
    void on_session_selected(int index);
    void do_login();
    void set_status(const std::string& msg, bool is_error = false);

    bool m_test_mode = false;
    std::string m_config_path;
    std::shared_ptr<miqu::AppEngine> m_engine;
    std::shared_ptr<miqu::Window> m_window;
    std::shared_ptr<IpcClient> m_ipc_client;

    std::vector<UserInfo> m_users;
    std::vector<SessionInfo> m_sessions;
    int m_selected_user_idx = 0;
    int m_selected_session_idx = 0;
    bool m_authenticating = false;

    // UI Widgets
    std::shared_ptr<miqu::TextView> m_time_label;
    std::shared_ptr<miqu::TextView> m_date_label;
    std::shared_ptr<miqu::AvatarView> m_avatar;
    std::shared_ptr<miqu::TextView> m_user_name_label;
    std::shared_ptr<miqu::Spinner> m_user_spinner;
    std::shared_ptr<miqu::EditText> m_password_input;
    std::shared_ptr<miqu::TextView> m_status_label;
    std::shared_ptr<miqu::Spinner> m_session_spinner;
    std::shared_ptr<miqu::Button> m_login_button;
};

} // namespace miqudm
