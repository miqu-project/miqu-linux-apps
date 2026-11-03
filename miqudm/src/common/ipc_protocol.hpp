#pragma once

#include "types.hpp"
#include <string>
#include <vector>
#include <memory>
#include <json/json.h>

namespace miqudm {

class IpcProtocol {
public:
    static std::string serialize_get_initial_data();
    static std::string serialize_initial_data(const std::vector<UserInfo>& users,
                                             const std::vector<SessionInfo>& sessions,
                                             const std::string& default_session,
                                             const std::string& last_user);

    static std::string serialize_authenticate(const std::string& username, const std::string& password);
    static std::string serialize_auth_result(bool success, const std::string& message);

    static std::string serialize_start_session(const std::string& username, const std::string& session_id);
    static std::string serialize_session_started(bool success, const std::string& message);

    static std::string serialize_power_action(PowerAction action);
    static std::string serialize_power_result(bool success);

    static bool parse_json(const std::string& raw, Json::Value& out_root);
};

} // namespace miqudm
