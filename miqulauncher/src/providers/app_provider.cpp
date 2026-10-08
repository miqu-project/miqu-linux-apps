#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <algorithm>
#include "app_provider.hpp"
#include "system/package_manager.hpp"

namespace miqu {

namespace fs = std::filesystem;

static std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

static std::string get_usage_cache_path() {
    const char* home = getenv("HOME");
    if (!home) return "";
    const char* xdg_cache = getenv("XDG_CACHE_HOME");
    std::string base = (xdg_cache && *xdg_cache) ? xdg_cache : (std::string(home) + "/.cache");
    std::string dir = base + "/miqulauncher";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir + "/app_usage.txt";
}

void AppProvider::load_usage() {
    std::string path = get_usage_cache_path();
    if (path.empty() || !fs::exists(path)) return;

    std::ifstream file(path);
    if (!file.is_open()) return;

    std::string id;
    int count = 0;
    while (file >> id >> count) {
        if (!id.empty() && count > 0) {
            m_usage_counts[id] = count;
        }
    }
}

void AppProvider::save_usage() {
    std::string path = get_usage_cache_path();
    if (path.empty()) return;

    std::ofstream file(path);
    if (!file.is_open()) return;

    for (const auto& pair : m_usage_counts) {
        file << pair.first << " " << pair.second << "\n";
    }
}

void AppProvider::ensure_loaded() {
    if (m_loaded) return;

    load_usage();

    auto raw_apps = PackageManager::get_installed_applications();
    m_entries.clear();
    m_entries.reserve(raw_apps.size());

    for (auto& app : raw_apps) {
        AppEntry entry;
        entry.item = std::move(app);
        entry.lower_title = to_lower(entry.item.title);
        entry.lower_subtitle = to_lower(entry.item.subtitle);
        entry.lower_exec = to_lower(entry.item.exec_cmd);
        entry.lower_id = to_lower(entry.item.id);
        entry.lower_keywords = to_lower(entry.item.keywords);

        auto it = m_usage_counts.find(entry.item.id);
        entry.launch_count = (it != m_usage_counts.end()) ? it->second : 0;

        m_entries.push_back(std::move(entry));
    }

    // Sort by usage count descending, then alphabetical by title
    std::sort(m_entries.begin(), m_entries.end(), [](const AppEntry& a, const AppEntry& b) {
        if (a.launch_count != b.launch_count) {
            return a.launch_count > b.launch_count;
        }
        return a.lower_title < b.lower_title;
    });

    m_loaded = true;
}

void AppProvider::load_async(std::function<void()> on_loaded) {
    if (m_loaded) {
        if (on_loaded) on_loaded();
        return;
    }

    std::thread([this, on_loaded]() {
        ensure_loaded();
        if (on_loaded) {
            on_loaded();
        }
    }).detach();
}

static int compute_score(const AppEntry& entry, const std::string& query, const std::string& lower_query) {
    if (lower_query.empty()) return entry.launch_count;

    int score = 0;

    // 1. Exact match on title or ID
    if (entry.lower_title == lower_query || entry.lower_id == lower_query) {
        return 10000 + entry.launch_count * 10;
    }

    // 2. Prefix match on title (e.g. "fire" -> "Firefox")
    if (entry.lower_title.rfind(lower_query, 0) == 0) {
        score = std::max(score, 5000 + (100 - static_cast<int>(entry.lower_title.size())) + entry.launch_count * 5);
    }

    // 3. Word boundary prefix match in title (e.g. "code" -> "Visual Studio Code")
    size_t word_pos = entry.lower_title.find(' ' + lower_query);
    if (word_pos == std::string::npos) {
        word_pos = entry.lower_title.find('-' + lower_query);
    }
    if (word_pos != std::string::npos) {
        score = std::max(score, 4000 + entry.launch_count * 5);
    }

    // 4. Prefix match on ID (e.g. "org.gnome.nautilus")
    if (entry.lower_id.rfind(lower_query, 0) == 0) {
        score = std::max(score, 3500 + entry.launch_count * 3);
    }

    // 5. Keyword match (e.g. "browser" -> Firefox)
    if (!entry.lower_keywords.empty()) {
        size_t kw_pos = entry.lower_keywords.find(lower_query);
        if (kw_pos != std::string::npos) {
            score = std::max(score, 3000 + entry.launch_count * 3);
        }
    }

    // 6. Subtitle / GenericName match (e.g. "web browser")
    if (!entry.lower_subtitle.empty()) {
        if (entry.lower_subtitle.rfind(lower_query, 0) == 0) {
            score = std::max(score, 2500 + entry.launch_count * 2);
        } else if (entry.lower_subtitle.find(lower_query) != std::string::npos) {
            score = std::max(score, 1800 + entry.launch_count * 2);
        }
    }

    // 7. General substring in title
    size_t sub_pos = entry.lower_title.find(lower_query);
    if (sub_pos != std::string::npos) {
        score = std::max(score, 1500 - static_cast<int>(sub_pos) * 10 + entry.launch_count);
    }

    // 8. Substring in exec command (e.g. typing binary name directly)
    if (entry.lower_exec.find(lower_query) != std::string::npos) {
        score = std::max(score, 1000 + entry.launch_count);
    }

    // 9. Acronym / fuzzy initialism match (e.g. "vsc" for "Visual Studio Code")
    if (query.size() >= 2 && query.size() <= 6) {
        size_t qi = 0;
        bool word_start = true;
        for (char c : entry.lower_title) {
            if (c == ' ' || c == '-' || c == '_') {
                word_start = true;
            } else if (word_start && c == lower_query[qi]) {
                qi++;
                word_start = false;
                if (qi == lower_query.size()) {
                    score = std::max(score, 2200 + entry.launch_count * 2);
                    break;
                }
            } else {
                word_start = false;
            }
        }
    }

    return score;
}

std::vector<LauncherItem> AppProvider::get_items(const std::string& query) {
    ensure_loaded();

    if (query.empty()) {
        std::vector<LauncherItem> items;
        items.reserve(m_entries.size());
        for (const auto& entry : m_entries) {
            items.push_back(entry.item);
        }
        return items;
    }

    std::string lower_query = to_lower(query);
    std::vector<std::pair<int, LauncherItem>> scored;
    scored.reserve(m_entries.size());

    for (const auto& entry : m_entries) {
        int s = compute_score(entry, query, lower_query);
        if (s > 0) {
            scored.push_back({s, entry.item});
        }
    }

    std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
        return a.first > b.first;
    });

    std::vector<LauncherItem> result;
    result.reserve(scored.size());
    for (auto& item : scored) {
        result.push_back(std::move(item.second));
    }
    return result;
}

void AppProvider::launch(const LauncherItem& item) {
    if (!item.id.empty()) {
        m_usage_counts[item.id]++;
        save_usage();
    }
    PackageManager::launch(item);
}

} // namespace miqu
