#pragma once

#include "model/launcher_item.hpp"
#include <vector>
#include <string>
#include <functional>
#include <memory>
#include <map>

namespace miqu {

struct AppEntry {
    LauncherItem item;
    std::string lower_title;
    std::string lower_subtitle;
    std::string lower_exec;
    std::string lower_id;
    std::string lower_keywords;
    int launch_count = 0;
};

class AppProvider {
public:
    AppProvider() = default;

    void ensure_loaded();
    void load_async(std::function<void()> on_loaded = nullptr);
    std::vector<LauncherItem> get_items(const std::string& query = "");
    void launch(const LauncherItem& item);

private:
    void load_usage();
    void save_usage();

    std::vector<AppEntry> m_entries;
    std::map<std::string, int> m_usage_counts;
    bool m_loaded = false;
};

} // namespace miqu
