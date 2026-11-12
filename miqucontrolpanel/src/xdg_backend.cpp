#include "xdg_backend.hpp"
#include "apps_backend.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <thread>
#include <mutex>

namespace fs = std::filesystem;

namespace miqu {

namespace {

static std::mutex s_xdg_mutex;
static bool s_cache_loaded = false;
static std::unordered_map<std::string, std::string> s_defaults;
static std::unordered_map<std::string, std::string> s_desktop_names;

std::vector<std::string> get_application_dirs() {
    std::vector<std::string> dirs;
    const char* home = getenv("HOME");
    if (home) {
        dirs.push_back(std::string(home) + "/.local/share/applications");
    }
    dirs.push_back("/usr/share/applications");
    dirs.push_back("/usr/local/share/applications");
    dirs.push_back("/var/lib/flatpak/exports/share/applications");
    if (home) {
        dirs.push_back(std::string(home) + "/.local/share/flatpak/exports/share/applications");
    }
    return dirs;
}

std::vector<std::string> split_mimes(const std::string& csv) {
    std::vector<std::string> result;
    std::stringstream ss(csv);
    std::string token;
    while (std::getline(ss, token, ',')) {
        size_t start = token.find_first_not_of(" \t\r\n");
        size_t end = token.find_last_not_of(" \t\r\n");
        if (start != std::string::npos && end != std::string::npos) {
            result.push_back(token.substr(start, end - start + 1));
        }
    }
    return result;
}

void parse_mimeapps_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return;

    std::string line;
    bool in_defaults = false;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;

        if (line[0] == '[') {
            in_defaults = (line == "[Default Applications]");
            continue;
        }

        if (!in_defaults) continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string mime = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        // Take primary desktop ID before first semicolon
        size_t semi = val.find(';');
        std::string desktop_id = (semi != std::string::npos) ? val.substr(0, semi) : val;

        size_t s = desktop_id.find_first_not_of(" \t");
        size_t e = desktop_id.find_last_not_of(" \t");
        if (s != std::string::npos && e != std::string::npos) {
            desktop_id = desktop_id.substr(s, e - s + 1);
        }

        if (!mime.empty() && !desktop_id.empty()) {
            s_defaults[mime] = desktop_id;
        }
    }
}

struct ParsedDesktop {
    std::string name;
    std::string generic_name;
    std::string comment;
    std::string icon;
    std::string exec;
    std::string categories;
    bool no_display = false;
};

bool parse_desktop_entry(const std::string& path, ParsedDesktop& out) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    bool in_entry = false;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;

        if (line[0] == '[') {
            in_entry = (line == "[Desktop Entry]");
            continue;
        }
        if (!in_entry) continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        if (key == "Name" && out.name.empty()) out.name = val;
        else if (key == "GenericName" && out.generic_name.empty()) out.generic_name = val;
        else if (key == "Comment" && out.comment.empty()) out.comment = val;
        else if (key == "Icon" && out.icon.empty()) out.icon = val;
        else if (key == "Exec" && out.exec.empty()) out.exec = val;
        else if (key == "Categories" && out.categories.empty()) out.categories = val;
        else if (key == "NoDisplay" && val == "true") out.no_display = true;
    }
    return !out.name.empty();
}

void collect_candidates_from_cache(const std::string& cache_path,
                                  const std::vector<std::string>& mimes,
                                  std::unordered_set<std::string>& out_candidates) {
    std::ifstream file(cache_path);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#' || line[0] == '[') continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string mime = line.substr(0, eq);
        bool match = false;
        for (const auto& m : mimes) {
            if (mime == m) {
                match = true;
                break;
            }
        }
        if (!match) continue;

        std::string list = line.substr(eq + 1);
        std::stringstream ss(list);
        std::string item;
        while (std::getline(ss, item, ';')) {
            size_t s = item.find_first_not_of(" \t");
            size_t e = item.find_last_not_of(" \t");
            if (s != std::string::npos && e != std::string::npos) {
                out_candidates.insert(item.substr(s, e - s + 1));
            }
        }
    }
}

void collect_terminal_candidates(std::unordered_set<std::string>& out_candidates) {
    auto dirs = get_application_dirs();
    for (const auto& dir : dirs) {
        if (!fs::exists(dir)) continue;
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            if (entry.is_regular_file() && entry.path().extension() == ".desktop") {
                ParsedDesktop pd;
                if (parse_desktop_entry(entry.path().string(), pd)) {
                    if (pd.categories.find("TerminalEmulator") != std::string::npos) {
                        out_candidates.insert(entry.path().filename().string());
                    }
                }
            }
        }
    }
}

} // namespace

void XdgBackend::init_cache() {
    std::lock_guard<std::mutex> lock(s_xdg_mutex);
    if (s_cache_loaded) return;

    const char* home = getenv("HOME");
    if (home) {
        std::string user_mimeapps = std::string(home) + "/.config/mimeapps.list";
        parse_mimeapps_file(user_mimeapps);
    }
    parse_mimeapps_file("/etc/xdg/mimeapps.list");

    s_cache_loaded = true;
}

std::string XdgBackend::resolve_desktop_path(const std::string& desktop_id) {
    if (desktop_id.empty()) return "";
    auto dirs = get_application_dirs();
    for (const auto& dir : dirs) {
        std::string path = dir + "/" + desktop_id;
        if (fs::exists(path)) return path;
    }
    return "";
}

std::string XdgBackend::get_app_name_from_desktop(const std::string& desktop_id) {
    if (desktop_id.empty()) return "";
    {
        std::lock_guard<std::mutex> lock(s_xdg_mutex);
        auto it = s_desktop_names.find(desktop_id);
        if (it != s_desktop_names.end()) return it->second;
    }

    std::string path = resolve_desktop_path(desktop_id);
    if (path.empty()) return desktop_id;

    ParsedDesktop pd;
    if (parse_desktop_entry(path, pd)) {
        std::lock_guard<std::mutex> lock(s_xdg_mutex);
        s_desktop_names[desktop_id] = pd.name;
        return pd.name;
    }
    return desktop_id;
}

std::string XdgBackend::get_default_desktop_id(const std::string& primary_mime) {
    init_cache();
    {
        std::lock_guard<std::mutex> lock(s_xdg_mutex);
        auto it = s_defaults.find(primary_mime);
        if (it != s_defaults.end() && !it->second.empty()) {
            return it->second;
        }
    }

    // Fallback: query xdg-mime
    std::string cmd = "xdg-mime query default \"" + primary_mime + "\" 2>/dev/null";
    std::string res = AppsBackend::run_cmd(cmd);
    if (!res.empty()) {
        std::lock_guard<std::mutex> lock(s_xdg_mutex);
        s_defaults[primary_mime] = res;
        return res;
    }
    return "";
}

std::string XdgBackend::get_default_app_name(const std::string& primary_mime, const std::string& fallback_name) {
    init_cache();
    std::string desktop_id;
    {
        std::lock_guard<std::mutex> lock(s_xdg_mutex);
        auto it = s_defaults.find(primary_mime);
        if (it != s_defaults.end() && !it->second.empty()) {
            desktop_id = it->second;
        }
    }
    if (desktop_id.empty()) return fallback_name;

    std::string name = get_app_name_from_desktop(desktop_id);
    return name.empty() ? fallback_name : name;
}

std::string XdgBackend::get_app_icon(const std::string& desktop_id, const std::string& name, const std::string& categories) {
    std::string lower = desktop_id + " " + name + " " + categories;
    for (char& c : lower) c = static_cast<char>(std::tolower(c));

    if (lower.find("firefox") != std::string::npos) return "󰈹";
    if (lower.find("chrome") != std::string::npos || lower.find("chromium") != std::string::npos ||
        lower.find("brave") != std::string::npos || lower.find("mullvad") != std::string::npos ||
        lower.find("librewolf") != std::string::npos || lower.find("webbrowser") != std::string::npos) return "󰖟";
    if (lower.find("kitty") != std::string::npos || lower.find("foot") != std::string::npos ||
        lower.find("alacritty") != std::string::npos || lower.find("terminal") != std::string::npos) return "󰄛";
    if (lower.find("code") != std::string::npos || lower.find("neovim") != std::string::npos ||
        lower.find("nvim") != std::string::npos || lower.find("geany") != std::string::npos ||
        lower.find("kate") != std::string::npos || lower.find("texteditor") != std::string::npos) return "󰨞";
    if (lower.find("evince") != std::string::npos || lower.find("pdf") != std::string::npos ||
        lower.find("mupdf") != std::string::npos || lower.find("okular") != std::string::npos ||
        lower.find("zathura") != std::string::npos) return "󰈦";
    if (lower.find("gallery") != std::string::npos || lower.find("image") != std::string::npos ||
        lower.find("photo") != std::string::npos || lower.find("viewer") != std::string::npos ||
        lower.find("loupe") != std::string::npos || lower.find("feh") != std::string::npos) return "󰋩";
    if (lower.find("gimp") != std::string::npos || lower.find("inkscape") != std::string::npos) return "󰽉";
    if (lower.find("vlc") != std::string::npos || lower.find("celluloid") != std::string::npos ||
        lower.find("mpv") != std::string::npos || lower.find("video") != std::string::npos) return "󰕼";
    if (lower.find("music") != std::string::npos || lower.find("audio") != std::string::npos ||
        lower.find("amberol") != std::string::npos || lower.find("spotify") != std::string::npos) return "󰝚";
    if (lower.find("thunderbird") != std::string::npos || lower.find("mail") != std::string::npos) return "󰇮";
    if (lower.find("nemo") != std::string::npos || lower.find("nautilus") != std::string::npos ||
        lower.find("thunar") != std::string::npos || lower.find("dolphin") != std::string::npos ||
        lower.find("files") != std::string::npos || lower.find("yazi") != std::string::npos ||
        lower.find("filemanager") != std::string::npos) return "󰉋";
    return "󰀻";
}

void XdgBackend::populate_role_candidates(DefaultAppRole& role) {
    auto mimes = split_mimes(role.mime_types);
    std::string primary_mime = role.primary_mime.empty() ? (mimes.empty() ? "" : mimes.front()) : role.primary_mime;
    std::string current_default = get_default_desktop_id(primary_mime);

    std::unordered_set<std::string> candidate_ids;
    if (role.role_name == "Terminal Emulator") {
        collect_terminal_candidates(candidate_ids);
    } else {
        const char* home = getenv("HOME");
        if (home) {
            std::string user_cache = std::string(home) + "/.local/share/applications/mimeinfo.cache";
            collect_candidates_from_cache(user_cache, mimes, candidate_ids);
        }
        collect_candidates_from_cache("/usr/share/applications/mimeinfo.cache", mimes, candidate_ids);
    }

    if (!current_default.empty()) {
        candidate_ids.insert(current_default);
    }

    std::vector<DefaultAppCandidate> candidates;
    for (const auto& id : candidate_ids) {
        std::string path = resolve_desktop_path(id);
        if (path.empty() && id != current_default) continue;

        ParsedDesktop pd;
        if (!path.empty()) {
            parse_desktop_entry(path, pd);
        }
        if (pd.no_display && id != current_default) continue;

        DefaultAppCandidate cand;
        cand.name = pd.name.empty() ? id : pd.name;
        cand.desktop_id = id;
        cand.exec_cmd = pd.exec;
        cand.description = !pd.comment.empty() ? pd.comment : (!pd.generic_name.empty() ? pd.generic_name : id);
        cand.icon = get_app_icon(id, cand.name, pd.categories);
        cand.is_default = (!current_default.empty() && id == current_default);

        candidates.push_back(std::move(cand));
    }

    // Check if any candidate matched current_default
    bool has_default = false;
    for (const auto& c : candidates) {
        if (c.is_default) {
            has_default = true;
            break;
        }
    }
    if (!has_default && !candidates.empty()) {
        std::sort(candidates.begin(), candidates.end(), [](const DefaultAppCandidate& a, const DefaultAppCandidate& b) {
            return a.name < b.name;
        });
        candidates.front().is_default = true;
    }

    // Sort: default candidate first, then alphabetical by name
    std::sort(candidates.begin(), candidates.end(), [](const DefaultAppCandidate& a, const DefaultAppCandidate& b) {
        if (a.is_default != b.is_default) return a.is_default > b.is_default;
        return a.name < b.name;
    });

    role.candidates = std::move(candidates);
    role.populated = true;
}

bool XdgBackend::set_default_app(const std::string& desktop_id, const std::string& mime_types_csv, std::string& error_msg) {
    if (desktop_id.empty()) {
        error_msg = "Empty desktop ID";
        return false;
    }

    auto mimes = split_mimes(mime_types_csv);
    if (mimes.empty()) {
        error_msg = "No MIME types provided";
        return false;
    }

    std::string mimes_arg;
    for (const auto& m : mimes) {
        mimes_arg += " \"" + m + "\"";
    }

    std::string cmd = "xdg-mime default \"" + desktop_id + "\"" + mimes_arg;
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        error_msg = "xdg-mime exited with code " + std::to_string(ret);
        return false;
    }

    // Update in-memory cache immediately
    {
        std::lock_guard<std::mutex> lock(s_xdg_mutex);
        for (const auto& m : mimes) {
            s_defaults[m] = desktop_id;
        }
    }
    return true;
}

void XdgBackend::set_default_app_async(const std::string& desktop_id, const std::string& mime_types_csv,
                                      std::function<void(bool success, const std::string& msg)> on_complete) {
    std::thread([desktop_id, mime_types_csv, on_complete]() {
        std::string err;
        bool ok = set_default_app(desktop_id, mime_types_csv, err);
        if (on_complete) {
            on_complete(ok, err);
        }
    }).detach();
}

} // namespace miqu
