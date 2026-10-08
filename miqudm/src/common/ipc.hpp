#pragma once

#include "user_scanner.hpp"
#include "session_scanner.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace miqudm {

constexpr const char* DEFAULT_SOCKET_PATH = "/run/miqudm/socket";
constexpr const char* FALLBACK_SOCKET_PATH = "/tmp/miqudm.sock";

enum class IpcMessageType {
    Unknown,
    Ping,
    GetUsers,
    GetSessions,
    Login,
    PowerAction,
    Response,
    SessionStatus
};

struct IpcMessage {
    IpcMessageType type = IpcMessageType::Unknown;
    bool success = false;
    std::string message;

    // Login request fields
    std::string username;
    std::string password;
    std::string session_exec;
    std::string desktop_name;

    // Power action fields
    std::string power_action; // "poweroff", "reboot", "suspend", "hibernate"

    // Data lists
    std::vector<UserInfo> users;
    std::vector<SessionInfo> sessions;

    std::string serialize() const;
    static IpcMessage deserialize(const std::string& raw_json);
};

std::string get_socket_path();

} // namespace miqudm
