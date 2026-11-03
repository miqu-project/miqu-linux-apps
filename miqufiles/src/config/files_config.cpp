#include "config/files_config.hpp"
#include <miqutoolkit/core/config.hpp>
#include <miqutoolkit/core/fs_utils.hpp>
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace miqu {

namespace fs = std::filesystem;

static FilesConfig s_files_config;

FilesConfig& FilesConfig::get() {
    return s_files_config;
}

void FilesConfig::sync_defaults_from_toolkit() {
    // Appearance tokens and metrics are sourced directly from Config::get()
}

static std::string trim_str(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n\"'");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n\"'");
    return str.substr(first, (last - first + 1));
}

static bool parse_bool(const std::string& val) {
    std::string s = val;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return (s == "true" || s == "1" || s == "yes" || s == "on");
}

void FilesConfig::load(const std::string& custom_path) {
    sync_defaults_from_toolkit();
    std::string target_path;
    if (!custom_path.empty() && fs::exists(custom_path)) {
        target_path = custom_path;
    } else {
        std::string user_conf = FsUtils::get_user_config_file("miqufiles");
        target_path = fs::exists(user_conf) ? user_conf : FsUtils::get_system_config_file("miqufiles");
    }

    if (target_path.empty() || !fs::exists(target_path)) {
        return;
    }

    // 1. Synchronize toolkit theme
    Config::get()->load_from_file(target_path);

    // 2. Parse options
    std::ifstream file(target_path);
    if (!file.is_open()) return;

    std::string line;
    std::string current_section = "";

    while (std::getline(file, line)) {
        line = trim_str(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;

        if (line.front() == '[' && line.back() == ']') {
            current_section = trim_str(line.substr(1, line.size() - 2));
            std::transform(current_section.begin(), current_section.end(), current_section.begin(), ::tolower);
            continue;
        }

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim_str(line.substr(0, eq));
        std::string val = trim_str(line.substr(eq + 1));
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);

        try {
            if (key == "window_width") {
                int w = std::stoi(val);
                if (w >= 400 && w <= 3840) window_width = w;
            } else if (key == "window_height") {
                int h = std::stoi(val);
                if (h >= 300 && h <= 2160) window_height = h;
            } else if (key == "default_path") {
                default_path = val;
            } else if (key == "show_hidden") {
                show_hidden = parse_bool(val);
            } else if (key == "vim_keys") {
                vim_keys = parse_bool(val);
            } else if (key == "terminal_cmd") {
                terminal_cmd = val;
            } else if (key == "preview_max_text_lines") {
                size_t l = std::stoul(val);
                if (l >= 10 && l <= 5000) preview_max_text_lines = l;
            } else if (key == "preview_max_file_size_kb") {
                size_t sz = std::stoul(val);
                if (sz >= 64 && sz <= 1048576) preview_max_file_size_kb = sz;
            } else if (key == "image_preview_quality") {
                image_preview_quality = val;
            }
        } catch (...) {
            // Ignore parse errors on malformed values
        }
    }
}

} // namespace miqu
