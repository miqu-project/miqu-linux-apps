#include "modules/applications/apps_backend.hpp"
#include <miqutoolkit/system/app_manager.hpp>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <unordered_map>
#include <unistd.h>

namespace miqu {

namespace fs = std::filesystem;

struct PipeCloser {
    void operator()(FILE* fp) const {
        if (fp) pclose(fp);
    }
};

std::string AppsBackend::run_cmd(const std::string& cmd) {
    std::string result;
    std::unique_ptr<FILE, PipeCloser> pipe(popen(cmd.c_str(), "r"));
    if (!pipe) return result;

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe.get())) {
        result += buffer;
    }
    // Trim trailing newline
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }
    return result;
}

std::string AppsBackend::resolve_desktop_path(const std::string& desktop_id) {
    const char* home = getenv("HOME");
    std::vector<std::string> search_dirs;
    if (home) {
        search_dirs.push_back(std::string(home) + "/.local/share/applications");
    }
    search_dirs.push_back("/usr/share/applications");
    search_dirs.push_back("/usr/local/share/applications");
    search_dirs.push_back("/var/lib/flatpak/exports/share/applications");

    for (const auto& dir : search_dirs) {
        std::string path = dir + "/" + desktop_id;
        if (fs::exists(path)) {
            return path;
        }
    }
    return "";
}

std::vector<std::string> AppsBackend::parse_desktop_mimes(const std::string& desktop_path) {
    std::vector<std::string> mimes;
    if (desktop_path.empty() || !fs::exists(desktop_path)) return mimes;

    std::ifstream file(desktop_path);
    if (!file.is_open()) return mimes;

    std::string line;
    while (std::getline(file, line)) {
        if (line.rfind("MimeType=", 0) == 0) {
            std::string raw = line.substr(9);
            std::stringstream ss(raw);
            std::string item;
            while (std::getline(ss, item, ';')) {
                if (!item.empty()) {
                    mimes.push_back(item);
                }
            }
            break;
        }
    }
    return mimes;
}

std::vector<AppDetailInfo> AppsBackend::get_installed_apps(bool force_refresh) {
    static std::vector<AppDetailInfo> s_cached_apps;
    static bool s_has_cached = false;

    if (s_has_cached && !force_refresh) {
        return s_cached_apps;
    }

    const auto& toolkit_apps = AppManager::get()->get_installed_apps(force_refresh);
    std::vector<AppDetailInfo> result;
    result.reserve(toolkit_apps.size());

    for (const auto& app : toolkit_apps) {
        if (app.no_display) continue;

        AppDetailInfo info;
        info.id = app.id;
        info.name = app.name;
        info.generic_name = app.generic_name;
        info.comment = app.comment;
        info.icon_name = app.icon_name;
        info.icon_path = app.icon_path;
        info.exec_cmd = app.exec_cmd;
        info.categories = app.categories;
        info.desktop_path = resolve_desktop_path(app.id);

        if (!info.desktop_path.empty()) {
            info.mime_types = parse_desktop_mimes(info.desktop_path);
            if (info.desktop_path.find("/.local/share/applications") != std::string::npos) {
                info.package_type = "User Application";
                info.is_user_app = true;
            } else if (info.desktop_path.find("/flatpak/") != std::string::npos) {
                info.package_type = "Flatpak";
                info.is_flatpak = true;
            } else {
                info.package_type = "Arch Linux Package (pacman)";
            }
        } else {
            info.package_type = "System Application";
        }

        result.push_back(std::move(info));
    }

    std::sort(result.begin(), result.end(), [](const AppDetailInfo& a, const AppDetailInfo& b) {
        std::string a_lower = a.name;
        std::string b_lower = b.name;
        std::transform(a_lower.begin(), a_lower.end(), a_lower.begin(), ::tolower);
        std::transform(b_lower.begin(), b_lower.end(), b_lower.begin(), ::tolower);
        return a_lower < b_lower;
    });

    s_cached_apps = result;
    s_has_cached = true;
    return result;
}

void AppsBackend::populate_package_metadata(AppDetailInfo& app) {
    if (app.desktop_path.empty() || app.is_user_app) return;

    if (app.is_flatpak) {
        // Flatpak query
        std::string flatpak_id = app.id;
        if (flatpak_id.size() > 8 && flatpak_id.substr(flatpak_id.size() - 8) == ".desktop") {
            flatpak_id = flatpak_id.substr(0, flatpak_id.size() - 8);
        }
        std::string out = run_cmd("flatpak info " + flatpak_id + " 2>/dev/null");
        if (!out.empty()) {
            std::stringstream ss(out);
            std::string line;
            while (std::getline(ss, line)) {
                if (line.find("Version:") != std::string::npos) {
                    app.version = line.substr(line.find(':') + 1);
                    while (!app.version.empty() && app.version.front() == ' ') app.version.erase(0, 1);
                } else if (line.find("Installed:") != std::string::npos) {
                    app.installed_size = line.substr(line.find(':') + 1);
                    while (!app.installed_size.empty() && app.installed_size.front() == ' ') app.installed_size.erase(0, 1);
                }
            }
        }
        return;
    }

    // Arch Linux pacman query
    std::string q_out = run_cmd("pacman -Qo " + app.desktop_path + " 2>/dev/null");
    // Format: "/usr/share/applications/firefox.desktop is owned by firefox 158.0.1-1"
    size_t owned_pos = q_out.find("is owned by ");
    if (owned_pos != std::string::npos) {
        std::string pkg_str = q_out.substr(owned_pos + 12);
        size_t sp = pkg_str.find(' ');
        if (sp != std::string::npos) {
            app.package_name = pkg_str.substr(0, sp);
            app.version = pkg_str.substr(sp + 1);
        } else {
            app.package_name = pkg_str;
        }

        if (!app.package_name.empty()) {
            std::string i_out = run_cmd("pacman -Qi " + app.package_name + " 2>/dev/null");
            std::stringstream ss(i_out);
            std::string line;
            while (std::getline(ss, line)) {
                if (line.rfind("Installed Size  :", 0) == 0) {
                    app.installed_size = line.substr(17);
                    while (!app.installed_size.empty() && app.installed_size.front() == ' ') app.installed_size.erase(0, 1);
                } else if (app.comment.empty() && line.rfind("Description     :", 0) == 0) {
                    app.comment = line.substr(17);
                    while (!app.comment.empty() && app.comment.front() == ' ') app.comment.erase(0, 1);
                }
            }
        }
    }
}

AppDetailInfo AppsBackend::get_app_details(const std::string& desktop_id) {
    auto apps = get_installed_apps();
    for (const auto& app : apps) {
        if (app.id == desktop_id) {
            return app;  // Caller must call populate_package_metadata if needed
        }
    }
    AppDetailInfo missing;
    missing.id = desktop_id;
    missing.name = desktop_id;
    missing.desktop_path = resolve_desktop_path(desktop_id);
    return missing;
}

bool AppsBackend::launch_app(const AppDetailInfo& app) {
    if (app.exec_cmd.empty()) return false;
    AppManager::launch_command(app.exec_cmd, false);
    return true;
}

bool AppsBackend::uninstall_app(const AppDetailInfo& app, std::string& error_msg) {
    if (app.is_user_app) {
        if (!app.desktop_path.empty() && fs::exists(app.desktop_path)) {
            std::error_code ec;
            fs::remove(app.desktop_path, ec);
            if (ec) {
                error_msg = ec.message();
                return false;
            }
            AppManager::get()->invalidate();
            get_installed_apps(true);
            return true;
        }
        error_msg = "Desktop file not found on disk.";
        return false;
    }

    if (app.is_flatpak) {
        std::string flatpak_id = app.id;
        if (flatpak_id.size() > 8 && flatpak_id.substr(flatpak_id.size() - 8) == ".desktop") {
            flatpak_id = flatpak_id.substr(0, flatpak_id.size() - 8);
        }
        std::string cmd = "flatpak uninstall -y " + flatpak_id;
        int ret = std::system(cmd.c_str());
        if (ret == 0) {
            AppManager::get()->invalidate();
            get_installed_apps(true);
            return true;
        }
        error_msg = "Flatpak uninstallation failed.";
        return false;
    }

    if (!app.package_name.empty()) {
        std::string cmd = "pkexec pacman -R --noconfirm " + app.package_name;
        int ret = std::system(cmd.c_str());
        if (ret == 0) {
            AppManager::get()->invalidate();
            get_installed_apps(true);
            return true;
        }
        error_msg = "Package uninstallation was cancelled or failed.";
        return false;
    }

    error_msg = "No system package manager associated with this application.";
    return false;
}

std::string AppsBackend::query_xdg_default(const std::string& mime_type) {
    // Check ~/.config/mimeapps.list first for fastest direct resolution
    const char* home = getenv("HOME");
    if (home) {
        std::string conf_file = std::string(home) + "/.config/mimeapps.list";
        if (fs::exists(conf_file)) {
            std::ifstream f(conf_file);
            std::string line;
            bool in_default = false;
            while (std::getline(f, line)) {
                if (line == "[Default Applications]") {
                    in_default = true;
                    continue;
                }
                if (in_default && !line.empty() && line.front() == '[') {
                    in_default = false;
                }
                if (in_default && line.rfind(mime_type + "=", 0) == 0) {
                    std::string val = line.substr(mime_type.size() + 1);
                    if (!val.empty() && val.back() == ';') val.pop_back();
                    return val;
                }
            }
        }
    }

    return run_cmd("xdg-mime query default " + mime_type);
}

std::vector<DefaultRoleSpec> AppsBackend::get_default_roles(bool force_refresh) {
    static std::vector<DefaultRoleSpec> s_cached_roles;
    static bool s_has_cached_roles = false;

    if (s_has_cached_roles && !force_refresh) {
        return s_cached_roles;
    }

    std::vector<DefaultRoleSpec> roles = {
        {
            "web_browser",
            "Web Browser",
            "Opening web links, bookmarks and HTML documents",
            "web-browser",
            {"x-scheme-handler/http", "x-scheme-handler/https", "text/html", "application/xhtml+xml"},
            "WebBrowser",
            "", "", "", {}
        },
        {
            "file_manager",
            "File Manager",
            "Browsing folders, directories and storage devices",
            "system-file-manager",
            {"inode/directory"},
            "FileManager",
            "", "", "", {}
        },
        {
            "terminal",
            "Terminal Emulator",
            "Interactive command-line shells and developer tools",
            "utilities-terminal",
            {},
            "TerminalEmulator",
            "", "", "", {}
        },
        {
            "mail",
            "Mail Client",
            "Composing emails and handling mailto links",
            "mail-client",
            {"x-scheme-handler/mailto"},
            "Email",
            "", "", "", {}
        },
        {
            "music_player",
            "Music Player",
            "Audio files, music tracks and playlist playback",
            "audio-player",
            {"audio/mpeg", "audio/flac", "audio/ogg", "audio/wav", "audio/x-vorbis+ogg"},
            "Audio",
            "", "", "", {}
        },
        {
            "video_player",
            "Video Player",
            "Movies, video clips and media streams",
            "video-player",
            {"video/mp4", "video/x-matroska", "video/webm", "video/quicktime", "video/x-msvideo"},
            "Video",
            "", "", "", {}
        },
        {
            "image_viewer",
            "Image Viewer",
            "Viewing photos, pictures and graphics files",
            "image-viewer",
            {"image/jpeg", "image/png", "image/webp", "image/svg+xml", "image/gif"},
            "Graphics",
            "", "", "", {}
        },
        {
            "text_editor",
            "Text Editor",
            "Plain text, code files and script editing",
            "accessories-text-editor",
            {"text/plain"},
            "TextEditor",
            "", "", "", {}
        },
        {
            "pdf_viewer",
            "PDF Document Viewer",
            "Portable Document Format (PDF) and eBooks",
            "document-viewer",
            {"application/pdf"},
            "Viewer",
            "", "", "", {}
        }
    };

    auto all_apps = get_installed_apps();
    std::unordered_map<std::string, const AppDetailInfo*> app_map;
    for (const auto& app : all_apps) {
        app_map[app.id] = &app;
    }

    for (auto& role : roles) {
        // Query current default
        if (!role.mime_types.empty()) {
            role.current_default_id = query_xdg_default(role.mime_types[0]);
        }
        if (role.role_id == "web_browser" && role.current_default_id.empty()) {
            role.current_default_id = run_cmd("xdg-settings get default-web-browser");
        }

        if (!role.current_default_id.empty()) {
            auto it = app_map.find(role.current_default_id);
            if (it != app_map.end()) {
                role.current_default_name = it->second->name;
                role.current_default_icon = it->second->icon_name;
            } else {
                role.current_default_name = role.current_default_id;
                role.current_default_icon = role.icon_name;
            }
        }

        // Find candidate applications
        for (const auto& app : all_apps) {
            bool matches = false;

            // Direct ID match
            if (!role.current_default_id.empty() && app.id == role.current_default_id) {
                matches = true;
            }

            // Category match
            if (!matches && !role.category_keyword.empty()) {
                if (app.categories.find(role.category_keyword) != std::string::npos) {
                    matches = true;
                }
            }

            // MimeType match
            if (!matches) {
                for (const auto& mime : role.mime_types) {
                    if (std::find(app.mime_types.begin(), app.mime_types.end(), mime) != app.mime_types.end()) {
                        matches = true;
                        break;
                    }
                }
            }

            // Terminal special case
            if (!matches && role.role_id == "terminal") {
                if (app.id.find("kitty") != std::string::npos ||
                    app.id.find("alacritty") != std::string::npos ||
                    app.id.find("foot") != std::string::npos ||
                    app.id.find("wezterm") != std::string::npos) {
                    matches = true;
                }
            }

            if (matches) {
                role.candidates.push_back(app);
            }
        }

        // Put current default at front of candidates if found
        if (!role.current_default_id.empty()) {
            for (size_t i = 0; i < role.candidates.size(); ++i) {
                if (role.candidates[i].id == role.current_default_id) {
                    if (i > 0) {
                        std::swap(role.candidates[0], role.candidates[i]);
                    }
                    break;
                }
            }
        }
    }

    s_cached_roles = roles;
    s_has_cached_roles = true;
    return s_cached_roles;
}

DefaultRoleSpec AppsBackend::get_default_role(const std::string& role_id) {
    auto roles = get_default_roles();
    for (const auto& r : roles) {
        if (r.role_id == role_id) return r;
    }
    return DefaultRoleSpec{};
}

bool AppsBackend::set_default_for_role(const std::string& role_id, const std::string& desktop_id) {
    auto roles = get_default_roles();
    for (const auto& role : roles) {
        if (role.role_id == role_id) {
            if (role_id == "web_browser") {
                run_cmd("xdg-settings set default-web-browser " + desktop_id);
            }
            if (!role.mime_types.empty()) {
                std::string cmd = "xdg-mime default " + desktop_id;
                for (const auto& m : role.mime_types) {
                    cmd += " " + m;
                }
                run_cmd(cmd);
            }
            // Invalidate cache so next load re-queries the updated default
            get_default_roles(true);
            return true;
        }
    }
    return false;
}

} // namespace miqu
