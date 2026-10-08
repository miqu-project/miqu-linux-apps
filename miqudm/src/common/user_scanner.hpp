#pragma once

#include <string>
#include <vector>
#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>
#include <filesystem>
#include <algorithm>

namespace miqudm {

struct UserInfo {
    std::string username;
    std::string display_name;
    std::string home_dir;
    std::string avatar_path;
    uid_t uid = 0;
    gid_t gid = 0;
    std::string shell = "/bin/bash";
};

class UserScanner {
public:
    static std::vector<UserInfo> scan_users() {
        std::vector<UserInfo> users;

        setpwent();
        struct passwd* pw = nullptr;

        while ((pw = getpwent()) != nullptr) {
            // Filter interactive non-system users (UID >= 1000 and < 60000, excluding nobody)
            if (pw->pw_uid >= 1000 && pw->pw_uid < 60000 && std::string(pw->pw_name) != "nobody") {
                std::string shell = pw->pw_shell ? pw->pw_shell : "";
                if (shell.find("nologin") != std::string::npos || shell.find("false") != std::string::npos) {
                    continue;
                }

                UserInfo user;
                user.username = pw->pw_name;
                user.home_dir = pw->pw_dir ? pw->pw_dir : "";
                user.uid = pw->pw_uid;
                user.gid = pw->pw_gid;
                user.shell = !shell.empty() ? shell : "/bin/bash";

                // Parse display name from GECOS field
                if (pw->pw_gecos && pw->pw_gecos[0] != '\0') {
                    std::string gecos = pw->pw_gecos;
                    auto comma_pos = gecos.find(',');
                    user.display_name = (comma_pos != std::string::npos) ? gecos.substr(0, comma_pos) : gecos;
                }
                if (user.display_name.empty()) {
                    user.display_name = user.username;
                }

                // Locate avatar if available
                user.avatar_path = find_user_avatar(user.username, user.home_dir);

                users.push_back(std::move(user));
            }
        }

        endpwent();

        if (users.empty()) {
            char* login_user = getlogin();
            std::string cur_user = login_user ? login_user : "user";
            UserInfo fallback;
            fallback.username = cur_user;
            fallback.display_name = cur_user;
            fallback.home_dir = "/home/" + cur_user;
            fallback.uid = getuid();
            fallback.gid = getgid();
            fallback.shell = "/bin/bash";
            users.push_back(fallback);
        }

        return users;
    }

private:
    static std::string find_user_avatar(const std::string& username, const std::string& home_dir) {
        std::error_code ec;

        // 1. Check ~/.face or ~/.face.icon
        std::string face = home_dir + "/.face";
        if (std::filesystem::exists(face, ec) && std::filesystem::is_regular_file(face, ec)) {
            return face;
        }

        std::string face_icon = home_dir + "/.face.icon";
        if (std::filesystem::exists(face_icon, ec) && std::filesystem::is_regular_file(face_icon, ec)) {
            return face_icon;
        }

        // 2. Check /var/lib/AccountsService/icons/<username>
        std::string act_icon = "/var/lib/AccountsService/icons/" + username;
        if (std::filesystem::exists(act_icon, ec) && std::filesystem::is_regular_file(act_icon, ec)) {
            return act_icon;
        }

        return "";
    }
};

} // namespace miqudm
