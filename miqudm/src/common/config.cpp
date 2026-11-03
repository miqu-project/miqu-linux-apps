#include "config.hpp"
#include <miqutoolkit/core/config.hpp>
#include <miqutoolkit/core/fs_utils.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <iomanip>

namespace miqudm {

namespace fs = std::filesystem;

static ConfigManager s_instance;

ConfigManager& ConfigManager::get() {
    return s_instance;
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

void ConfigManager::load_daemon_config(const std::string& custom_path) {
    std::string path;
    if (!custom_path.empty() && fs::exists(custom_path)) {
        path = custom_path;
    } else if (fs::exists("/etc/miqudm/miqudm.conf")) {
        path = "/etc/miqudm/miqudm.conf";
    } else if (fs::exists("/usr/share/miqudm/miqudm.conf")) {
        path = "/usr/share/miqudm/miqudm.conf";
    }

    if (path.empty()) return;
    m_daemon_config_path = path;

    std::ifstream file(path);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        line = trim_str(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        if (line.front() == '[' && line.back() == ']') continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim_str(line.substr(0, eq));
        std::string val = trim_str(line.substr(eq + 1));
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);

        if (key == "greeter_command") {
            m_daemon.greeter_command = val;
        } else if (key == "greeter_user") {
            m_daemon.greeter_user = val;
        } else if (key == "default_session") {
            m_daemon.default_session = val;
        } else if (key == "remember_last_user") {
            m_daemon.remember_last_user = parse_bool(val);
        } else if (key == "last_user") {
            m_daemon.last_user = val;
        } else if (key == "vt") {
            try { m_daemon.vt = std::stoi(val); } catch (...) {}
        } else if (key == "numlock") {
            m_daemon.numlock = parse_bool(val);
        }
    }
}

void ConfigManager::save_last_user(const std::string& username) {
    if (!m_daemon.remember_last_user || m_daemon_config_path.empty()) return;
    m_daemon.last_user = username;

    // Read full contents, replace last_user line, and write back
    std::ifstream infile(m_daemon_config_path);
    if (!infile.is_open()) return;

    std::vector<std::string> lines;
    std::string line;
    bool found = false;
    while (std::getline(infile, line)) {
        std::string trimmed = trim_str(line);
        if (trimmed.rfind("last_user", 0) == 0 && trimmed.find('=') != std::string::npos) {
            lines.push_back("last_user=" + username);
            found = true;
        } else {
            lines.push_back(line);
        }
    }
    infile.close();

    if (!found) {
        lines.push_back("last_user=" + username);
    }

    std::ofstream outfile(m_daemon_config_path);
    if (outfile.is_open()) {
        for (const auto& l : lines) {
            outfile << l << "\n";
        }
    }
}

void ConfigManager::load_greeter_config(const std::string& custom_path) {
    std::string target_path;
    if (!custom_path.empty() && fs::exists(custom_path)) {
        target_path = custom_path;
    } else if (fs::exists("/etc/miqudm/miqudm.conf")) {
        target_path = "/etc/miqudm/miqudm.conf";
    } else {
        target_path = "/usr/share/miqudm/miqudm.conf";
    }

    m_greeter_config_path = target_path;

    if (fs::exists(target_path)) {
        // 1. Sync miqutoolkit theme colors and tokens
        miqu::Config::get()->load_from_file(target_path);

        // 2. Parse greeter specific keys
        std::ifstream file(target_path);
        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                line = trim_str(line);
                if (line.empty() || line[0] == '#' || line[0] == ';') continue;
                if (line.front() == '[' && line.back() == ']') continue;

                auto eq = line.find('=');
                if (eq == std::string::npos) continue;

                std::string key = trim_str(line.substr(0, eq));
                std::string val = trim_str(line.substr(eq + 1));
                std::transform(key.begin(), key.end(), key.begin(), ::tolower);

                if (key == "wallpaper") {
                    m_greeter.wallpaper = val;
                } else if (key == "clock_format_24h") {
                    m_greeter.clock_format_24h = parse_bool(val);
                } else if (key == "show_power_buttons") {
                    m_greeter.show_power_buttons = parse_bool(val);
                } else if (key == "show_sessions") {
                    m_greeter.show_sessions = parse_bool(val);
                } else if (key == "allow_user_switching") {
                    m_greeter.allow_user_switching = parse_bool(val);
                } else if (key == "card_radius") {
                    try { m_greeter.card_radius = std::stoi(val); } catch (...) {}
                } else if (key == "avatar_size") {
                    try { m_greeter.avatar_size = std::stoi(val); } catch (...) {}
                }
            }
        }
    }
}

} // namespace miqudm
