#pragma once

#include "types.hpp"
#include <vector>
#include <string>

namespace miqudm {

class UserDiscovery {
public:
    static std::vector<UserInfo> discover_users();
    static std::string find_user_avatar(const std::string& username, const std::string& home_dir);
};

} // namespace miqudm
