#pragma once

#include "types.hpp"
#include <vector>
#include <string>

namespace miqudm {

class SessionDiscovery {
public:
    static std::vector<SessionInfo> discover_all();
    static std::vector<SessionInfo> discover_wayland_sessions();
    static std::vector<SessionInfo> discover_x11_sessions();

private:
    static std::vector<SessionInfo> scan_directory(const std::string& dir_path, bool is_wayland);
    static bool parse_desktop_file(const std::string& file_path, bool is_wayland, SessionInfo& out_session);
};

} // namespace miqudm
