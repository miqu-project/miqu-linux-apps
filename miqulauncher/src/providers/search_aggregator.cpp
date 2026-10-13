#include "search_aggregator.hpp"
#include <unordered_set>
#include <algorithm>

namespace miqu {

static std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

SearchAggregator::SearchAggregator(
    AppProvider* app_provider,
    WindowProvider* window_provider,
    WorkspaceProvider* workspace_provider,
    RunProvider* run_provider
) : m_app_provider(app_provider),
    m_window_provider(window_provider),
    m_workspace_provider(workspace_provider),
    m_run_provider(run_provider) {}

std::vector<LauncherItem> SearchAggregator::query(const std::string& search_text) {
    std::vector<LauncherItem> results;
    std::unordered_set<std::string> seen_keys;

    auto add_unique = [&](LauncherItem item) {
        std::string key = to_lower(item.title);
        if (item.category == ItemCategory::App && !item.id.empty()) {
            key = "app:" + item.id;
        } else if (item.category == ItemCategory::Window) {
            key = "win:" + item.id;
        } else if (item.category == ItemCategory::File) {
            key = "file:" + item.exec_cmd;
        }

        if (seen_keys.find(key) != seen_keys.end()) return false;
        seen_keys.insert(key);
        results.push_back(std::move(item));
        return true;
    };

    if (search_text.empty()) {
        // 1. Curated defaults: Active windows first
        if (m_window_provider) {
            auto windows = m_window_provider->get_items("");
            for (auto& w : windows) {
                w.category = ItemCategory::Window;
                w.badge = "Window";
                add_unique(std::move(w));
            }
        }

        // 2. Frequently used applications (top 8)
        if (m_app_provider) {
            auto apps = m_app_provider->get_items("");
            size_t count = 0;
            for (auto& a : apps) {
                a.category = ItemCategory::App;
                a.badge = "App";
                if (add_unique(std::move(a))) {
                    if (++count >= 8) break;
                }
            }
        }

        // 3. Recent files (top 5)
        auto files = m_file_provider.get_items("");
        size_t file_count = 0;
        for (auto& f : files) {
            if (add_unique(std::move(f))) {
                if (++file_count >= 5) break;
            }
        }

        return results;
    }

    // 1. Check Calculator first
    auto calc_res = m_calc_provider.evaluate(search_text);
    if (calc_res) {
        results.push_back(std::move(*calc_res));
    }

    // 2. Check Web Bangs (!g, !w, !yt, etc.)
    if (search_text[0] == '!') {
        auto web_items = m_web_provider.get_items(search_text);
        for (auto& w : web_items) {
            results.push_back(std::move(w));
        }
        return results;
    }

    // 3. Open Windows matching query (High priority to switch tasks)
    if (m_window_provider) {
        auto windows = m_window_provider->get_items(search_text);
        for (auto& w : windows) {
            w.category = ItemCategory::Window;
            w.badge = "Window";
            add_unique(std::move(w));
        }
    }

    // 4. Applications matching query
    if (m_app_provider) {
        auto apps = m_app_provider->get_items(search_text);
        for (auto& a : apps) {
            a.category = ItemCategory::App;
            a.badge = "App";
            add_unique(std::move(a));
        }
    }

    // 5. Workspaces matching query
    if (m_workspace_provider) {
        auto workspaces = m_workspace_provider->get_items(search_text);
        for (auto& ws : workspaces) {
            ws.category = ItemCategory::Workspace;
            ws.badge = "Workspace";
            add_unique(std::move(ws));
        }
    }

    // 6. Fast Fuzzy Files matching query
    auto files = m_file_provider.get_items(search_text);
    for (auto& f : files) {
        add_unique(std::move(f));
    }

    // 7. System Binaries from PATH
    if (m_run_provider) {
        auto bins = m_run_provider->get_items(search_text);
        size_t bin_limit = 0;
        for (auto& b : bins) {
            // Deduplicate if binary name matches an app already present
            std::string lower_bin = to_lower(b.title);
            if (seen_keys.find(lower_bin) == seen_keys.end()) {
                b.category = ItemCategory::Run;
                b.badge = "Run";
                if (add_unique(std::move(b))) {
                    if (++bin_limit >= 12) break;
                }
            }
        }
    }

    // 8. Fallback Web Search at the end
    auto fallback_web = m_web_provider.get_items(search_text);
    for (auto& fw : fallback_web) {
        results.push_back(std::move(fw));
    }

    // Cap at top 50 items
    if (results.size() > 50) {
        results.resize(50);
    }

    return results;
}

void SearchAggregator::activate(const LauncherItem& item) {
    switch (item.category) {
        case ItemCategory::App:
            if (m_app_provider) m_app_provider->launch(item);
            break;
        case ItemCategory::Window:
            if (m_window_provider) m_window_provider->activate(item);
            break;
        case ItemCategory::Workspace:
            if (m_workspace_provider) m_workspace_provider->activate(item);
            break;
        case ItemCategory::Run:
            if (m_run_provider) m_run_provider->launch(item);
            break;
        case ItemCategory::File:
            m_file_provider.activate(item);
            break;
        case ItemCategory::Calc:
            m_calc_provider.activate(item);
            break;
        case ItemCategory::Web:
            m_web_provider.activate(item);
            break;
        default:
            if (m_app_provider) m_app_provider->launch(item);
            break;
    }
}

} // namespace miqu
