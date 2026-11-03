#pragma once

#include "types.hpp"
#include "pam_service.hpp"
#include <string>
#include <vector>
#include <atomic>
#include <sys/types.h>

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

    void handle_client_message(int client_fd, const std::string& line);
    void process_client(int client_fd);

    std::string m_socket_path = "/run/miqudm/socket";
    int m_server_fd = -1;
    pid_t m_greeter_pid = -1;
    pid_t m_user_session_pid = -1;

    std::atomic<bool> m_running{false};
    std::unique_ptr<PamService> m_authenticated_pam;
    std::string m_authenticated_user;

    std::vector<UserInfo> m_cached_users;
    std::vector<SessionInfo> m_cached_sessions;
};

} // namespace miqudm
