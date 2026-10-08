#include "config.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <vector>

namespace miqudm {

static std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static bool parse_bool(const std::string& val, bool default_val = false) {
    std::string lower = val;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower == "true" || lower == "1" || lower == "yes" || lower == "on") return true;
    if (lower == "false" || lower == "0" || lower == "no" || lower == "off") return false;
    return default_val;
}

Config& Config::get() {
    static Config instance;
    return instance;
}

std::string Config::find_config_file(const std::string& custom_path) {
    if (!custom_path.empty() && std::filesystem::exists(custom_path)) {
        return custom_path;
    }

    const std::vector<std::string> candidates = {
        "/etc/miqudm/miqudm.conf",
        "/etc/miqudm.conf",
        "/usr/share/miqudm/miqudm.conf"
    };

    for (const auto& path : candidates) {
        if (std::filesystem::exists(path)) {
            return path;
        }
    }

    return "";
}

bool Config::load(const std::string& custom_path) {
    m_loaded_path = find_config_file(custom_path);
    if (m_loaded_path.empty()) {
        return false;
    }

    std::ifstream file(m_loaded_path);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    std::string current_section;

    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;

        if (line.front() == '[' && line.back() == ']') {
            current_section = line.substr(1, line.size() - 2);
            continue;
        }

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));

        if (current_section == "General" || current_section.empty()) {
            if (key == "default_session") general.default_session = val;
            else if (key == "remember_last_user") general.remember_last_user = parse_bool(val, true);
            else if (key == "last_user") general.last_user = val;
            else if (key == "greeter_command") general.greeter_command = val;
            else if (key == "greeter_user") general.greeter_user = val;
            else if (key == "vt") {
                try { general.vt = std::stoi(val); } catch (...) {}
            }
            else if (key == "numlock") general.numlock = parse_bool(val, false);
        } else if (current_section == "Autologin") {
            if (key == "enabled") autologin.enabled = parse_bool(val, false);
            else if (key == "user") autologin.user = val;
            else if (key == "session") autologin.session = val;
        }
    }

    return true;
}

bool Config::save(const std::string& custom_path) {
    std::string path = !custom_path.empty() ? custom_path : m_loaded_path;
    if (path.empty()) {
        path = "/etc/miqudm/miqudm.conf";
    }

    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }

    file << "[General]\n";
    file << "default_session=" << general.default_session << "\n";
    file << "remember_last_user=" << (general.remember_last_user ? "true" : "false") << "\n";
    file << "last_user=" << general.last_user << "\n";
    file << "greeter_command=" << general.greeter_command << "\n";
    file << "greeter_user=" << general.greeter_user << "\n";
    file << "vt=" << general.vt << "\n";
    file << "numlock=" << (general.numlock ? "true" : "false") << "\n\n";

    file << "[Autologin]\n";
    file << "enabled=" << (autologin.enabled ? "true" : "false") << "\n";
    file << "user=" << autologin.user << "\n";
    file << "session=" << autologin.session << "\n";

    return true;
}

} // namespace miqudm
