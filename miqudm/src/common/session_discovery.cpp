#include "session_discovery.hpp"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <iostream>

namespace miqudm {

namespace fs = std::filesystem;

static std::string trim_str(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n\"'");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n\"'");
    return str.substr(first, (last - first + 1));
}

bool SessionDiscovery::parse_desktop_file(const std::string& file_path, bool is_wayland, SessionInfo& out_session) {
    std::ifstream file(file_path);
    if (!file.is_open()) return false;

    fs::path p(file_path);
    out_session.id = p.stem().string();
    out_session.is_wayland = is_wayland;

    std::string line;
    bool in_desktop_entry = false;
    bool no_display = false;
    bool hidden = false;

    while (std::getline(file, line)) {
        line = trim_str(line);
        if (line.empty() || line[0] == '#') continue;

        if (line.front() == '[' && line.back() == ']') {
            in_desktop_entry = (line == "[Desktop Entry]");
            continue;
        }

        if (!in_desktop_entry) continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim_str(line.substr(0, eq));
        std::string val = trim_str(line.substr(eq + 1));

        if (key == "Name") {
            out_session.name = val;
        } else if (key == "Exec") {
            out_session.exec = val;
        } else if (key == "Comment") {
            out_session.comment = val;
        } else if (key == "DesktopNames") {
            out_session.desktop_names = val;
        } else if (key == "NoDisplay") {
            std::string v = val;
            std::transform(v.begin(), v.end(), v.begin(), ::tolower);
            if (v == "true" || v == "1") no_display = true;
        } else if (key == "Hidden") {
            std::string v = val;
            std::transform(v.begin(), v.end(), v.begin(), ::tolower);
            if (v == "true" || v == "1") hidden = true;
        }
    }

    if (no_display || hidden || out_session.exec.empty()) {
        return false;
    }

    if (out_session.name.empty()) {
        out_session.name = out_session.id;
    }

    return true;
}

std::vector<SessionInfo> SessionDiscovery::scan_directory(const std::string& dir_path, bool is_wayland) {
    std::vector<SessionInfo> sessions;
    std::error_code ec;

    if (!fs::exists(dir_path, ec) || !fs::is_directory(dir_path, ec)) {
        return sessions;
    }

    for (const auto& entry : fs::directory_iterator(dir_path, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        if (entry.path().extension() != ".desktop") continue;

        SessionInfo s;
        if (parse_desktop_file(entry.path().string(), is_wayland, s)) {
            sessions.push_back(std::move(s));
        }
    }

    return sessions;
}

std::vector<SessionInfo> SessionDiscovery::discover_wayland_sessions() {
    std::vector<SessionInfo> sessions;

    auto s1 = scan_directory("/usr/share/wayland-sessions", true);
    sessions.insert(sessions.end(), s1.begin(), s1.end());

    auto s2 = scan_directory("/usr/local/share/wayland-sessions", true);
    for (auto& s : s2) {
        // Avoid duplicates
        if (std::none_of(sessions.begin(), sessions.end(), [&](const SessionInfo& existing) {
            return existing.id == s.id;
        })) {
            sessions.push_back(std::move(s));
        }
    }

    return sessions;
}

std::vector<SessionInfo> SessionDiscovery::discover_x11_sessions() {
    return scan_directory("/usr/share/xsessions", false);
}

std::vector<SessionInfo> SessionDiscovery::discover_all() {
    auto list = discover_wayland_sessions();
    auto x11 = discover_x11_sessions();
    list.insert(list.end(), x11.begin(), x11.end());

    // Prefer miquland first if available, otherwise alphabetical
    std::sort(list.begin(), list.end(), [](const SessionInfo& a, const SessionInfo& b) {
        if (a.id == "miquland") return true;
        if (b.id == "miquland") return false;
        return a.name < b.name;
    });

    return list;
}

} // namespace miqudm
