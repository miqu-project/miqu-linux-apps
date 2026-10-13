#include "web_provider.hpp"
#include <miqutoolkit/system/app_manager.hpp>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace miqu {

struct BangDef {
    std::string prefix;
    std::string name;
    std::string url_template;
    std::string icon;
};

static const std::vector<BangDef> s_bangs = {
    { "!g", "Google", "https://www.google.com/search?q=", "applications-internet" },
    { "!google", "Google", "https://www.google.com/search?q=", "applications-internet" },
    { "!d", "DuckDuckGo", "https://duckduckgo.com/?q=", "applications-internet" },
    { "!ddg", "DuckDuckGo", "https://duckduckgo.com/?q=", "applications-internet" },
    { "!w", "Wikipedia", "https://en.wikipedia.org/wiki/Special:Search?search=", "accessories-dictionary" },
    { "!wiki", "Wikipedia", "https://en.wikipedia.org/wiki/Special:Search?search=", "accessories-dictionary" },
    { "!yt", "YouTube", "https://www.youtube.com/results?search_query=", "video-x-generic" },
    { "!gh", "GitHub", "https://github.com/search?q=", "text-x-script" },
    { "!arch", "ArchWiki", "https://wiki.archlinux.org/index.php?search=", "system-help" },
    { "!aw", "ArchWiki", "https://wiki.archlinux.org/index.php?search=", "system-help" },
    { "!r", "Reddit", "https://www.reddit.com/search/?q=", "applications-internet" },
    { "!b", "Bing", "https://www.bing.com/search?q=", "applications-internet" }
};

std::string WebProvider::url_encode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else if (c == ' ') {
            escaped << '+';
        } else {
            escaped << '%' << std::setw(2) << static_cast<int>(static_cast<unsigned char>(c));
        }
    }

    return escaped.str();
}

std::vector<LauncherItem> WebProvider::get_items(const std::string& query) {
    std::vector<LauncherItem> items;
    if (query.empty()) return items;

    // Check if query starts with a bang '!'
    if (query[0] == '!') {
        size_t sp = query.find(' ');
        std::string bang = (sp != std::string::npos) ? query.substr(0, sp) : query;
        std::string search_term = (sp != std::string::npos) ? query.substr(sp + 1) : "";

        // Trim leading spaces in search term
        size_t first = search_term.find_first_not_of(" \t");
        if (first != std::string::npos) {
            search_term = search_term.substr(first);
        } else {
            search_term = "";
        }

        std::transform(bang.begin(), bang.end(), bang.begin(), ::tolower);

        for (const auto& b : s_bangs) {
            if (b.prefix == bang) {
                LauncherItem item;
                item.id = "web:" + b.name;
                if (!search_term.empty()) {
                    item.title = "Search " + b.name + " for \"" + search_term + "\"";
                    item.subtitle = b.url_template + url_encode(search_term);
                    item.return_value = item.subtitle;
                } else {
                    item.title = "Search with " + b.name;
                    item.subtitle = "Type query after " + b.prefix + " (e.g. " + b.prefix + " linux)";
                    item.return_value = b.url_template;
                }
                item.icon_name = b.icon;
                item.badge = "Web";
                item.category = ItemCategory::Web;
                items.push_back(std::move(item));
                return items;
            }
        }
    }

    // Default web search suggestion for non-bang queries
    LauncherItem item;
    item.id = "web:search";
    item.title = "Search web for \"" + query + "\"";
    item.subtitle = "https://duckduckgo.com/?q=" + url_encode(query);
    item.icon_name = "applications-internet";
    item.badge = "Web";
    item.category = ItemCategory::Web;
    item.return_value = item.subtitle;
    items.push_back(std::move(item));

    return items;
}

void WebProvider::activate(const LauncherItem& item) {
    if (item.return_value.empty()) return;
    std::string cmd = "xdg-open \"" + item.return_value + "\"";
    AppManager::launch_command(cmd);
}

} // namespace miqu
