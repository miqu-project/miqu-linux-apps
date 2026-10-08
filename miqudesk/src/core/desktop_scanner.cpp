#include "desktop_scanner.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <unistd.h>

namespace miqudesk {

namespace fs = std::filesystem;

static std::string trim_str(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n\"'");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n\"'");
    return str.substr(first, (last - first + 1));
}

std::string DesktopScanner::clean_exec(const std::string& raw) {
    std::string result;
    result.reserve(raw.size());

    size_t i = 0;
    while (i < raw.size()) {
        if (raw[i] == '%' && i + 1 < raw.size() && raw[i + 1] == '%') {
            result += '%';
            i += 2;
            continue;
        }

        if (raw[i] == '%' && i + 1 < raw.size()) {
            char code = raw[i + 1];
            if (code == 'f' || code == 'F' || code == 'u' || code == 'U' ||
                code == 'd' || code == 'D' || code == 'n' || code == 'N' ||
                code == 'i' || code == 'c' || code == 'k' || code == 'v' ||
                code == 'm') {
                if (!result.empty() && result.back() == ' ') {
                    if (i + 2 == raw.size() || raw[i + 2] == ' ' || raw[i + 2] == '"' || raw[i + 2] == '\'') {
                        result.pop_back();
                    }
                }
                if (result.size() >= 1 && (result.back() == '"' || result.back() == '\'')) {
                    char quote = result.back();
                    if (i + 2 < raw.size() && raw[i + 2] == quote) {
                        result.pop_back();
                        if (!result.empty() && result.back() == ' ') {
                            result.pop_back();
                        }
                        i += 3;
                        continue;
                    }
                }
                i += 2;
                continue;
            }
        }
        result += raw[i++];
    }

    while (!result.empty() && (result.back() == ' ' || result.back() == '\t')) {
        result.pop_back();
    }
    return result;
}

static bool parse_desktop_file(const std::string& file_path, DesktopShortcut& out) {
    std::ifstream f(file_path);
    if (!f.is_open()) return false;

    std::string line;
    bool in_entry = false;
    std::string name, exec, icon;
    bool terminal = false;
    bool nodisplay = false;

    while (std::getline(f, line)) {
        line = trim_str(line);
        if (line.empty() || line[0] == '#') continue;

        if (line == "[Desktop Entry]") {
            in_entry = true;
            continue;
        } else if (!line.empty() && line[0] == '[' && line != "[Desktop Entry]") {
            in_entry = false;
        }

        if (!in_entry) continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim_str(line.substr(0, eq));
        std::string val = trim_str(line.substr(eq + 1));

        if (key == "Name" && name.empty()) {
            name = val;
        } else if (key == "Exec" && exec.empty()) {
            exec = val;
        } else if (key == "Icon" && icon.empty()) {
            icon = val;
        } else if (key == "Terminal") {
            terminal = (val == "true" || val == "1");
        } else if (key == "NoDisplay") {
            nodisplay = (val == "true" || val == "1");
        }
    }

    if (nodisplay || name.empty() || exec.empty()) {
        return false;
    }

    fs::path p(file_path);
    std::string stem = p.stem().string();

    out.id = stem;
    out.name = name;
    out.exec = DesktopScanner::clean_exec(exec);
    out.icon = icon.empty() ? stem : icon;
    out.terminal = terminal;
    out.type = DesktopEntryType::Application;
    return true;
}

static std::string escape_shell_arg(const std::string& arg) {
    std::string res = "'";
    for (char c : arg) {
        if (c == '\'') {
            res += "'\\''";
        } else {
            res += c;
        }
    }
    res += "'";
    return res;
}

std::string DesktopScanner::get_icon_for_directory(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });

    if (lower == "download" || lower == "downloads") return "folder-download";
    if (lower == "document" || lower == "documents") return "folder-documents";
    if (lower == "picture" || lower == "pictures" || lower == "photos" || lower == "images") return "folder-pictures";
    if (lower == "music" || lower == "audio") return "folder-music";
    if (lower == "video" || lower == "videos" || lower == "movies") return "folder-videos";
    if (lower == "desktop") return "user-desktop";
    if (lower == "public") return "folder-publicshare";
    if (lower == "templates") return "folder-templates";

    return "folder";
}

std::string DesktopScanner::get_icon_for_file(const fs::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

    std::string filename = path.filename().string();
    std::string lower_filename = filename;
    std::transform(lower_filename.begin(), lower_filename.end(), lower_filename.begin(), [](unsigned char c) { return std::tolower(c); });

    if (lower_filename.size() > 7 && lower_filename.rfind(".tar.gz") == lower_filename.size() - 7) return "package-x-generic";
    if (lower_filename.size() > 7 && lower_filename.rfind(".tar.xz") == lower_filename.size() - 7) return "package-x-generic";
    if (lower_filename.size() > 8 && lower_filename.rfind(".tar.bz2") == lower_filename.size() - 8) return "package-x-generic";

    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".gif" ||
        ext == ".webp" || ext == ".svg" || ext == ".bmp" || ext == ".ico" ||
        ext == ".tiff" || ext == ".tif" || ext == ".avif") {
        return "image-x-generic";
    }

    if (ext == ".mp3" || ext == ".flac" || ext == ".wav" || ext == ".ogg" ||
        ext == ".m4a" || ext == ".aac" || ext == ".opus" || ext == ".wma" ||
        ext == ".alac" || ext == ".aiff") {
        return "audio-x-generic";
    }

    if (ext == ".mp4" || ext == ".mkv" || ext == ".avi" || ext == ".webm" ||
        ext == ".mov" || ext == ".flv" || ext == ".wmv" || ext == ".m4v" ||
        ext == ".mpg" || ext == ".mpeg" || ext == ".3gp") {
        return "video-x-generic";
    }

    if (ext == ".pdf") return "application-pdf";

    if (ext == ".doc" || ext == ".docx" || ext == ".odt" || ext == ".rtf" ||
        ext == ".epub" || ext == ".mobi") {
        return "x-office-document";
    }

    if (ext == ".xls" || ext == ".xlsx" || ext == ".ods" || ext == ".csv" || ext == ".tsv") {
        return "x-office-spreadsheet";
    }

    if (ext == ".ppt" || ext == ".pptx" || ext == ".odp") {
        return "x-office-presentation";
    }

    if (ext == ".zip" || ext == ".tar" || ext == ".gz" || ext == ".xz" ||
        ext == ".bz2" || ext == ".7z" || ext == ".rar" || ext == ".zst" ||
        ext == ".tgz" || ext == ".tbz2") {
        return "package-x-generic";
    }

    if (ext == ".sh" || ext == ".bash" || ext == ".zsh" || ext == ".fish") {
        return "application-x-executable";
    }
    if (ext == ".py") return "text-x-python";
    if (ext == ".c" || ext == ".cpp" || ext == ".cc" || ext == ".cxx" ||
        ext == ".h" || ext == ".hpp" || ext == ".hh") {
        return "text-x-c++";
    }
    if (ext == ".rs") return "text-x-rust";
    if (ext == ".go") return "text-x-go";
    if (ext == ".java") return "text-x-java";
    if (ext == ".js" || ext == ".mjs" || ext == ".cjs" || ext == ".ts" ||
        ext == ".tsx" || ext == ".jsx") {
        return "text-x-javascript";
    }
    if (ext == ".html" || ext == ".htm") return "text-html";
    if (ext == ".css" || ext == ".scss" || ext == ".sass" || ext == ".less") return "text-css";
    if (ext == ".json") return "application-json";
    if (ext == ".xml") return "text-xml";
    if (ext == ".iso" || ext == ".img" || ext == ".dmg") return "media-optical";

    std::error_code ec;
    auto status = fs::status(path, ec);
    if (!ec && fs::is_regular_file(status)) {
        auto perms = status.permissions();
        if ((perms & (fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec)) != fs::perms::none) {
            return "application-x-executable";
        }
    }

    return "text-x-generic";
}

static DesktopShortcut create_directory_shortcut(const fs::directory_entry& entry) {
    DesktopShortcut s;
    std::string name = entry.path().filename().string();
    s.id = "dir_" + name;
    s.name = name;
    s.exec = "xdg-open " + escape_shell_arg(entry.path().string());
    s.icon = DesktopScanner::get_icon_for_directory(name);
    s.terminal = false;
    s.type = DesktopEntryType::Directory;
    return s;
}

static DesktopShortcut create_file_shortcut(const fs::directory_entry& entry) {
    DesktopShortcut s;
    std::string name = entry.path().filename().string();
    s.id = "file_" + name;
    s.name = name;
    s.exec = "xdg-open " + escape_shell_arg(entry.path().string());
    s.icon = DesktopScanner::get_icon_for_file(entry.path());
    s.terminal = false;
    s.type = DesktopEntryType::File;
    return s;
}

static bool compare_shortcuts(const DesktopShortcut& a, const DesktopShortcut& b) {
    bool a_is_dir = (a.type == DesktopEntryType::Directory);
    bool b_is_dir = (b.type == DesktopEntryType::Directory);
    if (a_is_dir != b_is_dir) {
        return a_is_dir;
    }

    std::string a_lower = a.name;
    std::transform(a_lower.begin(), a_lower.end(), a_lower.begin(), [](unsigned char c) { return std::tolower(c); });
    std::string b_lower = b.name;
    std::transform(b_lower.begin(), b_lower.end(), b_lower.begin(), [](unsigned char c) { return std::tolower(c); });

    if (a_lower != b_lower) {
        return a_lower < b_lower;
    }
    return a.name < b.name;
}

std::string DesktopScanner::get_desktop_dir() {
    const char* xdg_desktop = getenv("XDG_DESKTOP_DIR");
    if (xdg_desktop && *xdg_desktop && fs::exists(xdg_desktop)) {
        return xdg_desktop;
    }
    const char* home = getenv("HOME");
    if (home && *home) {
        std::string d = std::string(home) + "/Desktop";
        if (fs::exists(d)) return d;
    }
    return "";
}

std::vector<DesktopShortcut> DesktopScanner::get_shortcuts() {
    std::vector<DesktopShortcut> shortcuts;
    std::string desktop_dir = get_desktop_dir();
    if (desktop_dir.empty()) return shortcuts;

    std::error_code ec;
    if (!fs::exists(desktop_dir, ec) || !fs::is_directory(desktop_dir, ec)) {
        return shortcuts;
    }

    for (const auto& entry : fs::directory_iterator(desktop_dir, fs::directory_options::skip_permission_denied, ec)) {
        if (ec) break;

        std::string filename = entry.path().filename().string();
        if (filename.empty() || filename[0] == '.') {
            continue;
        }

        if (fs::is_directory(entry.path(), ec)) {
            shortcuts.push_back(create_directory_shortcut(entry));
            continue;
        }

        if (fs::is_regular_file(entry.path(), ec)) {
            if (entry.path().extension() == ".desktop") {
                DesktopShortcut s;
                if (parse_desktop_file(entry.path().string(), s)) {
                    shortcuts.push_back(std::move(s));
                }
            } else {
                shortcuts.push_back(create_file_shortcut(entry));
            }
        }
    }

    std::sort(shortcuts.begin(), shortcuts.end(), compare_shortcuts);
    return shortcuts;
}

} // namespace miqudesk
