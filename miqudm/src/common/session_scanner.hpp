#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace miqudm {

struct SessionInfo {
    std::string id;
    std::string name;
    std::string exec;
    std::string comment;
    std::string desktop_names;
    std::string desktop_file;
    bool is_wayland = true;
};

class SessionScanner {
public:
    static std::vector<SessionInfo> scan_all() {
        std::vector<SessionInfo> sessions;

        // Scan standard Wayland session directories
        scan_directory("/usr/share/wayland-sessions", true, sessions);
        scan_directory("/usr/local/share/wayland-sessions", true, sessions);

        if (sessions.empty()) {
            // Fallback default session if none found
            SessionInfo fallback;
            fallback.id = "miquland";
            fallback.name = "miquland (Wayland)";
            fallback.exec = "miquland";
            fallback.comment = "Default miquland Wayland Compositor";
            fallback.desktop_names = "miquland";
            fallback.is_wayland = true;
            sessions.push_back(fallback);
        }

        return sessions;
    }

private:
    static void scan_directory(const std::string& path, bool is_wayland, std::vector<SessionInfo>& out_sessions) {
        std::error_code ec;
        if (!std::filesystem::exists(path, ec) || !std::filesystem::is_directory(path, ec)) {
            return;
        }

        for (const auto& entry : std::filesystem::directory_iterator(path, ec)) {
            if (ec) break;
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() != ".desktop") continue;

            SessionInfo session = parse_desktop_file(entry.path().string(), is_wayland);
            if (!session.name.empty() && !session.exec.empty()) {
                // Avoid duplicates by ID/name
                auto it = std::find_if(out_sessions.begin(), out_sessions.end(), [&](const SessionInfo& s) {
                    return s.id == session.id || s.name == session.name;
                });
                if (it == out_sessions.end()) {
                    out_sessions.push_back(std::move(session));
                }
            }
        }
    }

    static SessionInfo parse_desktop_file(const std::string& file_path, bool is_wayland) {
        SessionInfo info;
        info.desktop_file = file_path;
        info.id = std::filesystem::path(file_path).stem().string();
        info.is_wayland = is_wayland;

        std::ifstream file(file_path);
        if (!file.is_open()) return info;

        std::string line;
        bool in_desktop_entry = false;

        while (std::getline(file, line)) {
            // Trim whitespace
            auto start = line.find_first_not_of(" \t\r\n");
            if (start == std::string::npos) continue;
            auto end = line.find_last_not_of(" \t\r\n");
            line = line.substr(start, end - start + 1);

            if (line.empty() || line[0] == '#') continue;

            if (line == "[Desktop Entry]") {
                in_desktop_entry = true;
                continue;
            } else if (line[0] == '[' && line != "[Desktop Entry]") {
                in_desktop_entry = false;
                continue;
            }

            if (!in_desktop_entry) continue;

            auto eq_pos = line.find('=');
            if (eq_pos == std::string::npos) continue;

            std::string key = line.substr(0, eq_pos);
            std::string val = line.substr(eq_pos + 1);

            if (key == "Name" && info.name.empty()) {
                info.name = val;
            } else if (key == "Exec" && info.exec.empty()) {
                info.exec = val;
            } else if (key == "Comment" && info.comment.empty()) {
                info.comment = val;
            } else if (key == "DesktopNames") {
                info.desktop_names = val;
            }
        }

        if (info.name.empty()) {
            info.name = info.id;
        }
        if (info.desktop_names.empty()) {
            info.desktop_names = info.name;
        }

        return info;
    }
};

} // namespace miqudm
