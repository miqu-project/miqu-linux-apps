#pragma once

#include <string>
#include <vector>

namespace miqu {

struct AppDetailInfo {
    std::string id;            // e.g. "firefox.desktop"
    std::string name;          // e.g. "Firefox"
    std::string generic_name;  // e.g. "Web Browser"
    std::string comment;       // e.g. "Fast, Private & Safe Web Browser"
    std::string icon_name;
    std::string icon_path;
    std::string exec_cmd;
    std::string desktop_path;  // e.g. "/usr/share/applications/firefox.desktop"
    std::string categories;
    std::vector<std::string> mime_types;
    std::string package_name;  // e.g. "firefox"
    std::string version;       // e.g. "158.0.1-1"
    std::string installed_size;// e.g. "301.96 MiB"
    std::string package_type;  // "Arch Linux Package (pacman)", "Flatpak", "User Application"
    bool is_flatpak = false;
    bool is_user_app = false;
    bool can_uninstall = true;
};

struct DefaultRoleSpec {
    std::string role_id;            // e.g. "web_browser", "file_manager", etc.
    std::string title;              // e.g. "Web Browser"
    std::string description;        // e.g. "Default application for opening web links and HTML documents"
    std::string icon_name;          // e.g. "web-browser"
    std::vector<std::string> mime_types;
    std::string category_keyword;   // e.g. "WebBrowser"
    std::string current_default_id; // e.g. "firefox.desktop"
    std::string current_default_name;
    std::string current_default_icon;
    std::vector<AppDetailInfo> candidates;
};

} // namespace miqu
