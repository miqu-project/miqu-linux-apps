#include "run_provider.hpp"
#include "system/binary_manager.hpp"
#include <miqutoolkit/view/image_view.hpp>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <map>

namespace miqu {

namespace fs = std::filesystem;

static std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

static std::string get_run_usage_path() {
    const char* home = getenv("HOME");
    if (!home) return "";
    const char* xdg_cache = getenv("XDG_CACHE_HOME");
    std::string base = (xdg_cache && *xdg_cache) ? xdg_cache : (std::string(home) + "/.cache");
    std::string dir = base + "/miqulauncher";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir + "/run_usage.txt";
}

static std::map<std::string, int> load_run_usage() {
    std::map<std::string, int> usage;
    std::string path = get_run_usage_path();
    if (path.empty() || !fs::exists(path)) return usage;
    std::ifstream file(path);
    std::string cmd;
    int count = 0;
    while (file >> cmd >> count) {
        if (!cmd.empty() && count > 0) usage[cmd] = count;
    }
    return usage;
}

static void record_run_usage(const std::string& raw_cmd) {
    if (raw_cmd.empty()) return;
    std::string bin = raw_cmd;
    size_t sp = bin.find_first_of(" \t");
    if (sp != std::string::npos) bin = bin.substr(0, sp);
    size_t sl = bin.find_last_of('/');
    if (sl != std::string::npos) bin = bin.substr(sl + 1);

    auto usage = load_run_usage();
    usage[bin]++;

    std::string path = get_run_usage_path();
    if (path.empty()) return;
    std::ofstream file(path);
    if (!file.is_open()) return;
    for (const auto& pair : usage) {
        file << pair.first << " " << pair.second << "\n";
    }
}

std::vector<LauncherItem> RunProvider::get_items(const std::string& query) const {
    const auto& binaries = BinaryManager::get_binary_entries();
    auto usage = load_run_usage();

    std::vector<LauncherItem> items;
    const size_t MAX_RESULTS = 80;
    items.reserve(MAX_RESULTS);

    std::string lower_query = to_lower(query);

    if (lower_query.empty()) {
        // Build list with usage counts
        std::vector<std::pair<int, const BinaryInfo*>> scored;
        scored.reserve(binaries.size());
        for (const auto& bin : binaries) {
            auto it = usage.find(bin.name);
            int count = (it != usage.end()) ? it->second : 0;
            scored.push_back({count, &bin});
        }
        std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
            if (a.first != b.first) return a.first > b.first;
            return a.second->name < b.second->name;
        });

        for (size_t i = 0; i < scored.size() && items.size() < MAX_RESULTS; ++i) {
            const auto& bin = *scored[i].second;
            LauncherItem info;
            info.id = bin.name;
            info.title = bin.name;
            info.terminal = bin.is_terminal;
            info.subtitle = info.terminal ? "Terminal Command" : "System Binary";
            info.icon_name = info.terminal ? "utilities-terminal" : "system-run";
            info.icon_path = "";
            info.exec_cmd = bin.name;
            items.push_back(std::move(info));
        }
    } else {
        std::vector<std::pair<int, const BinaryInfo*>> scored;
        scored.reserve(binaries.size());

        for (const auto& bin : binaries) {
            int score = 0;
            auto it = usage.find(bin.name);
            int count = (it != usage.end()) ? it->second : 0;

            if (bin.lower_name == lower_query) {
                score = 10000 + count * 10;
            } else if (bin.lower_name.rfind(lower_query, 0) == 0) {
                score = 5000 + (100 - static_cast<int>(bin.lower_name.size())) + count * 5;
            } else if (bin.lower_name.find('-' + lower_query) != std::string::npos ||
                       bin.lower_name.find('_' + lower_query) != std::string::npos) {
                score = 3500 + count * 3;
            } else if (bin.lower_name.find(lower_query) != std::string::npos) {
                score = 1000 + count;
            }

            if (score > 0) {
                scored.push_back({score, &bin});
            }
        }

        std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
            return a.first > b.first;
        });

        for (size_t i = 0; i < scored.size() && items.size() < MAX_RESULTS; ++i) {
            const auto& bin = *scored[i].second;
            LauncherItem info;
            info.id = bin.name;
            info.title = bin.name;
            info.terminal = bin.is_terminal;
            info.subtitle = info.terminal ? "Terminal Command" : "System Binary";
            info.icon_name = info.terminal ? "utilities-terminal" : "system-run";
            info.icon_path = "";
            info.exec_cmd = bin.name;
            items.push_back(std::move(info));
        }
    }

    return items;
}

void RunProvider::launch(const LauncherItem& item) const {
    record_run_usage(item.exec_cmd);
    BinaryManager::launch_command(item.exec_cmd, item.terminal);
}

void RunProvider::launch_raw(const std::string& cmd) const {
    record_run_usage(cmd);
    BinaryManager::launch_command(cmd, false);
}

} // namespace miqu
