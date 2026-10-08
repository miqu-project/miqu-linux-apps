#pragma once

#include "user_scanner.hpp"
#include "session_scanner.hpp"
#include <string>
#include <vector>
#include <sys/types.h>

namespace miqudm {

class SessionLauncher {
public:
    SessionLauncher() = default;
    ~SessionLauncher();

    bool launch(const UserInfo& user,
                const SessionInfo& session,
                const std::vector<std::string>& pam_envs,
                int vt_number = 1);

    bool is_running() const;
    int wait_for_exit();
    void terminate();

    pid_t get_pid() const { return m_session_pid; }

private:
    pid_t m_session_pid = -1;
};

} // namespace miqudm
