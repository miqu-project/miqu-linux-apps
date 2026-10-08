#include "script_provider.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <unistd.h>
#include <sys/wait.h>

namespace miqu {

static std::string expand_path(const std::string& path) {
    if (!path.empty() && path[0] == '~') {
        const char* home = getenv("HOME");
        if (home) {
            return std::string(home) + path.substr(1);
        }
    }
    return path;
}

static bool has_image_extension(const std::string& path) {
    std::string lower = path;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    const std::vector<std::string> exts = {
        ".png", ".jpg", ".jpeg", ".webp", ".svg", ".bmp", ".gif", ".ico", ".avif"
    };
    for (const auto& ext : exts) {
        if (lower.size() >= ext.size() && lower.compare(lower.size() - ext.size(), ext.size(), ext) == 0) {
            return true;
        }
    }
    return false;
}

ScriptProvider::ScriptProvider(std::string script_path)
    : m_script_path(std::move(script_path)) {
    refresh();
}

void ScriptProvider::refresh() {
    m_raw_items.clear();
    std::string full_path = expand_path(m_script_path);

    FILE* pipe = popen(full_path.c_str(), "r");
    if (!pipe) return;

    char* line_buf = nullptr;
    size_t line_len = 0;
    ssize_t read_bytes;

    while ((read_bytes = getline(&line_buf, &line_len, pipe)) != -1) {
        std::string line(line_buf, read_bytes);
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
            line.pop_back();
        }
        if (!line.empty()) {
            m_raw_items.push_back(line);
        }
    }
    if (line_buf) {
        free(line_buf);
    }
    pclose(pipe);
}

std::vector<LauncherItem> ScriptProvider::query(const std::string& filter_text) {
    std::string lower_filter = filter_text;
    std::transform(lower_filter.begin(), lower_filter.end(), lower_filter.begin(), ::tolower);

    std::vector<LauncherItem> results;
    for (size_t i = 0; i < m_raw_items.size(); ++i) {
        const auto& line = m_raw_items[i];
        LauncherItem item;
        item.id = std::to_string(i);
        item.icon_name = "application-x-executable";

        size_t null_pos = line.find('\0');
        if (null_pos != std::string::npos) {
            item.title = line.substr(0, null_pos);
            item.return_value = item.title;

            std::string meta_part = line.substr(null_pos + 1);
            std::vector<std::string> tokens;
            std::stringstream ss(meta_part);
            std::string tok;
            while (std::getline(ss, tok, '\x1f')) {
                if (!tok.empty()) tokens.push_back(tok);
            }

            for (size_t k = 0; k + 1 < tokens.size(); k += 2) {
                std::string key = tokens[k];
                std::string val = tokens[k + 1];
                std::transform(key.begin(), key.end(), key.begin(), ::tolower);

                if (key == "icon") {
                    std::string expanded = expand_path(val);
                    if (has_image_extension(expanded) || expanded.find('/') != std::string::npos) {
                        item.icon_path = expanded;
                        item.is_image = true;
                    } else {
                        item.icon_name = val;
                    }
                } else if (key == "info") {
                    item.return_value = val;
                } else if (key == "meta") {
                    item.meta_tags = val;
                } else if (key == "nonselectable") {
                    std::transform(val.begin(), val.end(), val.begin(), ::tolower);
                    item.non_selectable = (val == "true" || val == "1");
                }
            }
        } else {
            std::string expanded = expand_path(line);
            if (has_image_extension(expanded) && (expanded[0] == '/' || expanded.find('/') != std::string::npos)) {
                item.title = expanded.substr(expanded.find_last_of("/\\") + 1);
                item.icon_path = expanded;
                item.is_image = true;
                item.return_value = line;
            } else {
                item.title = line;
                item.return_value = line;
            }
        }

        if (!filter_text.empty()) {
            std::string lower_title = item.title;
            std::transform(lower_title.begin(), lower_title.end(), lower_title.begin(), ::tolower);

            std::string lower_meta = item.meta_tags;
            std::transform(lower_meta.begin(), lower_meta.end(), lower_meta.begin(), ::tolower);

            if (lower_title.find(lower_filter) == std::string::npos &&
                (lower_meta.empty() || lower_meta.find(lower_filter) == std::string::npos)) {
                continue;
            }
        }

        results.push_back(item);
    }
    return results;
}

void ScriptProvider::activate(const LauncherItem& item) {
    if (item.non_selectable) return;
    std::string full_path = expand_path(m_script_path);
    std::string choice = item.return_value.empty() ? item.title : item.return_value;
    std::string cmd = full_path + " \"" + choice + "\" &";

    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
        _exit(127);
    }
}

} // namespace miqu
