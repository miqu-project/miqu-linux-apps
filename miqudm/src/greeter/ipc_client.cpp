#include "ipc_client.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>

namespace miqudm {

IpcClient::IpcClient() {
    m_socket_path = get_socket_path();
}

bool IpcClient::send_request(const IpcMessage& req, IpcMessage& out_resp, int timeout_ms) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return false;
    }

    struct sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, m_socket_path.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(fd);
        return false;
    }

    // Send request
    std::string payload = req.serialize();
    ssize_t written = write(fd, payload.data(), payload.size());
    if (written < static_cast<ssize_t>(payload.size())) {
        close(fd);
        return false;
    }

    // Wait for reply with timeout
    struct pollfd pfd = {};
    pfd.fd = fd;
    pfd.events = POLLIN;

    int ret = poll(&pfd, 1, timeout_ms);
    if (ret <= 0 || !(pfd.revents & POLLIN)) {
        close(fd);
        return false;
    }

    char buffer[8192];
    std::string response_raw;

    while (true) {
        ssize_t bytes = read(fd, buffer, sizeof(buffer) - 1);
        if (bytes <= 0) break;
        buffer[bytes] = '\0';
        response_raw += buffer;

        auto newline_pos = response_raw.find('\n');
        if (newline_pos != std::string::npos) {
            std::string line = response_raw.substr(0, newline_pos);
            out_resp = IpcMessage::deserialize(line);
            close(fd);
            return true;
        }
    }

    close(fd);
    if (!response_raw.empty()) {
        out_resp = IpcMessage::deserialize(response_raw);
        return true;
    }

    return false;
}

bool IpcClient::ping() {
    IpcMessage req;
    req.type = IpcMessageType::Ping;
    IpcMessage resp;
    return send_request(req, resp, 500) && resp.success;
}

bool IpcClient::get_users(std::vector<UserInfo>& out_users) {
    IpcMessage req;
    req.type = IpcMessageType::GetUsers;
    IpcMessage resp;
    if (send_request(req, resp, 1000) && resp.success) {
        out_users = std::move(resp.users);
        return true;
    }
    return false;
}

bool IpcClient::get_sessions(std::vector<SessionInfo>& out_sessions) {
    IpcMessage req;
    req.type = IpcMessageType::GetSessions;
    IpcMessage resp;
    if (send_request(req, resp, 1000) && resp.success) {
        out_sessions = std::move(resp.sessions);
        return true;
    }
    return false;
}

bool IpcClient::login(const std::string& username,
                      const std::string& password,
                      const std::string& session_exec,
                      const std::string& desktop_name,
                      std::string& out_error_message)
{
    IpcMessage req;
    req.type = IpcMessageType::Login;
    req.username = username;
    req.password = password;
    req.session_exec = session_exec;
    req.desktop_name = desktop_name;

    IpcMessage resp;
    if (!send_request(req, resp, 10000)) {
        out_error_message = "Failed to communicate with miqudm daemon";
        return false;
    }

    if (!resp.success) {
        out_error_message = !resp.message.empty() ? resp.message : "Authentication failed";
        return false;
    }

    return true;
}

bool IpcClient::power_action(const std::string& action) {
    IpcMessage req;
    req.type = IpcMessageType::PowerAction;
    req.power_action = action;
    IpcMessage resp;
    return send_request(req, resp, 2000) && resp.success;
}

} // namespace miqudm
