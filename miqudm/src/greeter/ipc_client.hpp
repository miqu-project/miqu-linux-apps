#pragma once

#include "ipc.hpp"
#include <string>
#include <vector>
#include <functional>

namespace miqudm {

class IpcClient {
public:
    IpcClient();
    ~IpcClient() = default;

    bool ping();
    bool get_users(std::vector<UserInfo>& out_users);
    bool get_sessions(std::vector<SessionInfo>& out_sessions);
    bool login(const std::string& username,
               const std::string& password,
               const std::string& session_exec,
               const std::string& desktop_name,
               std::string& out_error_message);
    bool power_action(const std::string& action);

    bool send_request(const IpcMessage& req, IpcMessage& out_resp, int timeout_ms = 3000);

private:
    std::string m_socket_path;
};

} // namespace miqudm
