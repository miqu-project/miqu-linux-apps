#include "dmenu_provider.hpp"
#include <miqutoolkit/view/image_view.hpp>
#include <miqutoolkit/core/fs_utils.hpp>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <unistd.h>

namespace miqu {

namespace fs = std::filesystem;

DmenuProvider::DmenuProvider() {}

LauncherItem DmenuProvider::parse_line(const std::string& line, size_t index) {
    LauncherItem item;
    item.id = std::to_string(index);
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
            if (!tok.empty()) {
                tokens.push_back(tok);
            }
        }

        for (size_t i = 0; i + 1 < tokens.size(); i += 2) {
            std::string key = tokens[i];
            std::string val = tokens[i + 1];
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);

            if (key == "icon") {
                std::string expanded = FsUtils::expand_user_path(val);
                if (FsUtils::has_image_extension(expanded) || expanded.find('/') != std::string::npos) {
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
        std::string expanded = FsUtils::expand_user_path(line);
        if (FsUtils::has_image_extension(expanded) && (expanded[0] == '/' || expanded.find('/') != std::string::npos)) {
            item.title = fs::path(expanded).filename().string();
            item.icon_path = expanded;
            item.is_image = true;
            item.return_value = line;
        } else {
            item.title = line;
            item.return_value = line;
        }
    }

    return item;
}

void DmenuProvider::load_from_stdin() {
    if (isatty(STDIN_FILENO)) {
        return;
    }

    m_items.clear();
    std::string line;
    size_t index = 0;
    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty()) {
            auto item = parse_line(line, index++);
            if (item.is_image) {
                ImageView::preload(item.icon_path.empty() ? item.icon_name : item.icon_path);
            }
            m_items.push_back(std::move(item));
        }
    }
}

std::vector<LauncherItem> DmenuProvider::query(const std::string& filter_text) {
    if (filter_text.empty()) {
        return m_items;
    }

    std::string lower_filter = filter_text;
    std::transform(lower_filter.begin(), lower_filter.end(), lower_filter.begin(), ::tolower);

    std::vector<LauncherItem> results;
    for (const auto& item : m_items) {
        std::string lower_title = item.title;
        std::transform(lower_title.begin(), lower_title.end(), lower_title.begin(), ::tolower);

        std::string lower_meta = item.meta_tags;
        std::transform(lower_meta.begin(), lower_meta.end(), lower_meta.begin(), ::tolower);

        if (lower_title.find(lower_filter) != std::string::npos ||
            (!lower_meta.empty() && lower_meta.find(lower_filter) != std::string::npos)) {
            results.push_back(item);
        }
    }
    return results;
}

void DmenuProvider::activate(const LauncherItem& item) {
    if (item.non_selectable) return;
    std::cout << (item.return_value.empty() ? item.title : item.return_value) << std::endl;
}

void DmenuProvider::submit(const std::string& raw_text) {
    std::cout << raw_text << std::endl;
}

} // namespace miqu
