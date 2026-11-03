#include "ipc_protocol.hpp"
#include <sstream>
#include <iostream>

namespace miqudm {

static std::string json_to_string(const Json::Value& val) {
    Json::StreamWriterBuilder writer;
    writer["indentation"] = ""; // Compact single-line
    return Json::writeString(writer, val) + "\n";
}

bool IpcProtocol::parse_json(const std::string& raw, Json::Value& out_root) {
    Json::CharReaderBuilder reader;
    std::string errs;
    std::istringstream s(raw);
    return Json::parseFromStream(reader, s, &out_root, &errs);
}

std::string IpcProtocol::serialize_get_initial_data() {
    Json::Value root;
    root["type"] = "get_initial_data";
    return json_to_string(root);
}

std::string IpcProtocol::serialize_initial_data(const std::vector<UserInfo>& users,
                                               const std::vector<SessionInfo>& sessions,
                                               const std::string& default_session,
                                               const std::string& last_user) {
    Json::Value root;
    root["type"] = "initial_data";
    root["default_session"] = default_session;
    root["last_user"] = last_user;

    Json::Value users_arr(Json::arrayValue);
    for (const auto& u : users) {
        Json::Value user_obj;
        user_obj["username"] = u.username;
        user_obj["display_name"] = u.display_name;
        user_obj["avatar_path"] = u.avatar_path;
        user_obj["uid"] = u.uid;
        user_obj["gid"] = u.gid;
        user_obj["home_dir"] = u.home_dir;
        users_arr.append(user_obj);
    }
    root["users"] = users_arr;

    Json::Value sessions_arr(Json::arrayValue);
    for (const auto& s : sessions) {
        Json::Value sess_obj;
        sess_obj["id"] = s.id;
        sess_obj["name"] = s.name;
        sess_obj["exec"] = s.exec;
        sess_obj["comment"] = s.comment;
        sess_obj["is_wayland"] = s.is_wayland;
        sessions_arr.append(sess_obj);
    }
    root["sessions"] = sessions_arr;

    return json_to_string(root);
}

std::string IpcProtocol::serialize_authenticate(const std::string& username, const std::string& password) {
    Json::Value root;
    root["type"] = "authenticate";
    root["username"] = username;
    root["password"] = password;
    return json_to_string(root);
}

std::string IpcProtocol::serialize_auth_result(bool success, const std::string& message) {
    Json::Value root;
    root["type"] = "auth_result";
    root["success"] = success;
    root["message"] = message;
    return json_to_string(root);
}

std::string IpcProtocol::serialize_start_session(const std::string& username, const std::string& session_id) {
    Json::Value root;
    root["type"] = "start_session";
    root["username"] = username;
    root["session_id"] = session_id;
    return json_to_string(root);
}

std::string IpcProtocol::serialize_session_started(bool success, const std::string& message) {
    Json::Value root;
    root["type"] = "session_started";
    root["success"] = success;
    root["message"] = message;
    return json_to_string(root);
}

std::string IpcProtocol::serialize_power_action(PowerAction action) {
    Json::Value root;
    root["type"] = "power_action";
    switch (action) {
        case PowerAction::PowerOff:  root["action"] = "poweroff"; break;
        case PowerAction::Reboot:    root["action"] = "reboot"; break;
        case PowerAction::Suspend:   root["action"] = "suspend"; break;
        case PowerAction::Hibernate: root["action"] = "hibernate"; break;
    }
    return json_to_string(root);
}

std::string IpcProtocol::serialize_power_result(bool success) {
    Json::Value root;
    root["type"] = "power_result";
    root["success"] = success;
    return json_to_string(root);
}

} // namespace miqudm
