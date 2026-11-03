#include "user_discovery.hpp"
#include <pwd.h>
#include <unistd.h>
#include <filesystem>
#include <algorithm>
#include <iostream>

namespace miqudm {

namespace fs = std::filesystem;

std::string UserDiscovery::find_user_avatar(const std::string& username, const std::string& home_dir) {
    if (!home_dir.empty()) {
        std::error_code ec;
        fs::path p1 = fs::path(home_dir) / ".face";
        if (fs::exists(p1, ec) && fs::is_regular_file(p1, ec)) return p1.string();

        fs::path p2 = fs::path(home_dir) / ".face.icon";
        if (fs::exists(p2, ec) && fs::is_regular_file(p2, ec)) return p2.string();
    }

    if (!username.empty()) {
        std::error_code ec;
        fs::path p3 = fs::path("/var/lib/AccountsService/icons") / username;
        if (fs::exists(p3, ec) && fs::is_regular_file(p3, ec)) return p3.string();
    }

    return "";
}

std::vector<UserInfo> UserDiscovery::discover_users() {
    std::vector<UserInfo> users;
    setpwent();

    struct passwd* pw = nullptr;
    while ((pw = getpwent()) != nullptr) {
        // UID 1000 - 65533 for standard interactive human users
        if (pw->pw_uid < 1000 || pw->pw_uid >= 65534) {
            continue;
        }

        std::string shell = pw->pw_shell ? pw->pw_shell : "";
        if (shell.find("nologin") != std::string::npos || shell.find("/false") != std::string::npos) {
            continue;
        }

        UserInfo u;
        u.username = pw->pw_name ? pw->pw_name : "";
        u.uid = pw->pw_uid;
        u.gid = pw->pw_gid;
        u.home_dir = pw->pw_dir ? pw->pw_dir : "";
        u.shell = shell;

        // Parse GECOS (Real Name)
        std::string gecos = pw->pw_gecos ? pw->pw_gecos : "";
        auto comma_pos = gecos.find(',');
        if (comma_pos != std::string::npos) {
            gecos = gecos.substr(0, comma_pos);
        }

        if (!gecos.empty()) {
            u.display_name = gecos;
        } else {
            // Fallback: capitalized username
            u.display_name = u.username;
            if (!u.display_name.empty()) {
                u.display_name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(u.display_name[0])));
            }
        }

        u.avatar_path = find_user_avatar(u.username, u.home_dir);
        users.push_back(std::move(u));
    }

    endpwent();

    // Sort alphabetically by display name
    std::sort(users.begin(), users.end(), [](const UserInfo& a, const UserInfo& b) {
        return a.display_name < b.display_name;
    });

    return users;
}

} // namespace miqudm
