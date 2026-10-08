#pragma once

#include "pam_auth.hpp"
#include "session.hpp"
#include "ipc.hpp"
#include "config.hpp"
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <memory>

namespace miqudm {

class Daemon {
public:
    Daemon();
    ~Daemon();

    bool init();
    void run();
    void stop();

private:
    bool setup_ipc_socket();
    void cleanup_ipc_socket();

    void start_greeter();
    void stop_greeter();

    void handle_client_connection(int client_fd);
    void process_message(int client_fd, const IpcMessage& req);

    void handle_login_request(int client_fd, const IpcMessage& req);
    void handle_power_action(int client_fd, const IpcMessage& req);

    std::string m_socket_path;
    int m_server_socket_fd = -1;
    pid_t m_greeter_pid = -1;
    std::atomic<bool> m_running{false};

    std::unique_ptr<PamAuth> m_greeter_pam;
    std::unique_ptr<PamAuth> m_active_pam;
    SessionLauncher m_session_launcher;

    std::vector<UserInfo> m_cached_users;
    std::vector<SessionInfo> m_cached_sessions;
};

} // namespace miqudm
