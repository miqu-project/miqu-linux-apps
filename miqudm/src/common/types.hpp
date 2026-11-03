#pragma once

#include <string>
#include <vector>

namespace miqudm {

struct UserInfo {
    std::string username;
    std::string display_name;
    std::string avatar_path;
    int uid = 0;
    int gid = 0;
    std::string home_dir;
    std::string shell;
};

struct SessionInfo {
    std::string id;          // e.g. "miquland"
    std::string name;        // e.g. "MiquLand"
    std::string exec;        // e.g. "miquland"
    std::string comment;     // e.g. "Miqu Desktop Environment"
    std::string desktop_names;
    bool is_wayland = true;
};

enum class PowerAction {
    PowerOff,
    Reboot,
    Suspend,
    Hibernate
};

} // namespace miqudm
