#pragma once

#include "ipc_client.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <memory>
#include <vector>

namespace miqudm {

class GreeterApp {
public:
    GreeterApp(std::shared_ptr<miqu::AppEngine> engine, bool test_mode = false);
    ~GreeterApp() = default;

    bool init();
    int run();

private:
    void setup_ui();
    void select_user(size_t index);
    void select_session(int index);
    void attempt_login();
    void handle_power_action(const std::string& action);
    void handle_key(const miqu::KeyPressEvent& ev);
    void update_status(const std::string& msg, bool is_error = false);

    std::shared_ptr<miqu::AppEngine> m_engine;
    bool m_test_mode = false;
    std::shared_ptr<miqu::Window> m_window;

    IpcClient m_ipc;

    // UI Widgets
    std::shared_ptr<miqu::GridView> m_users_grid;
    std::shared_ptr<miqu::Spinner> m_session_spinner;
    std::shared_ptr<miqu::EditText> m_password_input;
    std::shared_ptr<miqu::TextView> m_status_label;
    std::shared_ptr<miqu::Button> m_login_btn;

    // Data
    std::vector<UserInfo> m_users;
    std::vector<SessionInfo> m_sessions;
    size_t m_selected_user_idx = 0;
    int m_selected_session_idx = 0;
};

} // namespace miqudm
