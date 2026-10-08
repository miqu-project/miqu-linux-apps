#include "ipc.hpp"
#include <json/json.h>
#include <sstream>
#include <filesystem>
#include <iostream>

namespace miqudm {

std::string get_socket_path() {
    std::error_code ec;
    if (std::filesystem::exists("/run/miqudm", ec)) {
        return DEFAULT_SOCKET_PATH;
    }
    return FALLBACK_SOCKET_PATH;
}

static std::string type_to_string(IpcMessageType type) {
    switch (type) {
        case IpcMessageType::Ping: return "ping";
        case IpcMessageType::GetUsers: return "get_users";
        case IpcMessageType::GetSessions: return "get_sessions";
        case IpcMessageType::Login: return "login";
        case IpcMessageType::PowerAction: return "power_action";
        case IpcMessageType::Response: return "response";
        case IpcMessageType::SessionStatus: return "session_status";
        default: return "unknown";
    }
}

static IpcMessageType string_to_type(const std::string& str) {
    if (str == "ping") return IpcMessageType::Ping;
    if (str == "get_users") return IpcMessageType::GetUsers;
    if (str == "get_sessions") return IpcMessageType::GetSessions;
    if (str == "login") return IpcMessageType::Login;
    if (str == "power_action") return IpcMessageType::PowerAction;
    if (str == "response") return IpcMessageType::Response;
    if (str == "session_status") return IpcMessageType::SessionStatus;
    return IpcMessageType::Unknown;
}

std::string IpcMessage::serialize() const {
    Json::Value root;
    root["type"] = type_to_string(type);
    root["success"] = success;
    root["message"] = message;

    if (!username.empty()) root["username"] = username;
    if (!password.empty()) root["password"] = password;
    if (!session_exec.empty()) root["session_exec"] = session_exec;
    if (!desktop_name.empty()) root["desktop_name"] = desktop_name;
    if (!power_action.empty()) root["power_action"] = power_action;

    if (!users.empty()) {
        Json::Value users_array(Json::arrayValue);
        for (const auto& u : users) {
            Json::Value uval;
            uval["username"] = u.username;
            uval["display_name"] = u.display_name;
            uval["home_dir"] = u.home_dir;
            uval["avatar_path"] = u.avatar_path;
            uval["uid"] = static_cast<Json::UInt64>(u.uid);
            uval["gid"] = static_cast<Json::UInt64>(u.gid);
            uval["shell"] = u.shell;
            users_array.append(uval);
        }
        root["users"] = users_array;
    }

    if (!sessions.empty()) {
        Json::Value sess_array(Json::arrayValue);
        for (const auto& s : sessions) {
            Json::Value sval;
            sval["id"] = s.id;
            sval["name"] = s.name;
            sval["exec"] = s.exec;
            sval["comment"] = s.comment;
            sval["desktop_names"] = s.desktop_names;
            sval["desktop_file"] = s.desktop_file;
            sval["is_wayland"] = s.is_wayland;
            sess_array.append(sval);
        }
        root["sessions"] = sess_array;
    }

    Json::StreamWriterBuilder builder;
    builder["indentation"] = ""; // Compact single line JSON
    std::string out = Json::writeString(builder, root);
    out += "\n";
    return out;
}

IpcMessage IpcMessage::deserialize(const std::string& raw_json) {
    IpcMessage msg;
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errs;
    std::istringstream s(raw_json);

    if (!Json::parseFromStream(builder, s, &root, &errs)) {
        msg.type = IpcMessageType::Unknown;
        msg.message = "Failed to parse JSON: " + errs;
        return msg;
    }

    msg.type = string_to_type(root.get("type", "unknown").asString());
    msg.success = root.get("success", false).asBool();
    msg.message = root.get("message", "").asString();
    msg.username = root.get("username", "").asString();
    msg.password = root.get("password", "").asString();
    msg.session_exec = root.get("session_exec", "").asString();
    msg.desktop_name = root.get("desktop_name", "").asString();
    msg.power_action = root.get("power_action", "").asString();

    if (root.isMember("users") && root["users"].isArray()) {
        for (const auto& uval : root["users"]) {
            UserInfo u;
            u.username = uval.get("username", "").asString();
            u.display_name = uval.get("display_name", "").asString();
            u.home_dir = uval.get("home_dir", "").asString();
            u.avatar_path = uval.get("avatar_path", "").asString();
            u.uid = static_cast<uid_t>(uval.get("uid", 0).asUInt64());
            u.gid = static_cast<gid_t>(uval.get("gid", 0).asUInt64());
            u.shell = uval.get("shell", "/bin/bash").asString();
            msg.users.push_back(std::move(u));
        }
    }

    if (root.isMember("sessions") && root["sessions"].isArray()) {
        for (const auto& sval : root["sessions"]) {
            SessionInfo s_info;
            s_info.id = sval.get("id", "").asString();
            s_info.name = sval.get("name", "").asString();
            s_info.exec = sval.get("exec", "").asString();
            s_info.comment = sval.get("comment", "").asString();
            s_info.desktop_names = sval.get("desktop_names", "").asString();
            s_info.desktop_file = sval.get("desktop_file", "").asString();
            s_info.is_wayland = sval.get("is_wayland", true).asBool();
            msg.sessions.push_back(std::move(s_info));
        }
    }

    return msg;
}

} // namespace miqudm
