#include "window_provider.hpp"
#include <miqutoolkit/view/image_view.hpp>
#include <algorithm>

namespace miqu {

static std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

std::vector<LauncherItem> WindowProvider::get_items(const std::string& query) const {
    auto windows = WindowManager::get()->get_windows();
    std::vector<LauncherItem> items;
    items.reserve(windows.size());

    for (const auto& win : windows) {
        LauncherItem item;
        item.id = std::to_string(win.id);
        item.title = !win.title.empty() ? win.title : (!win.app_id.empty() ? win.app_id : "Window");
        item.is_active = win.is_active;

        std::string status;
        if (win.is_active) status = "Active";
        else if (win.is_fullscreen) status = "Fullscreen";
        else if (win.is_maximized) status = "Maximized";
        else if (win.is_minimized) status = "Minimized";

        if (!win.app_id.empty()) {
            item.subtitle = status.empty() ? win.app_id : (win.app_id + " • " + status);
        } else {
            item.subtitle = status;
        }

        item.icon_name = win.app_id;
        // Do not eagerly resolve icon path from disk; ImageView resolves on demand
        item.icon_path = "";
        items.push_back(std::move(item));
    }

    if (query.empty()) {
        std::sort(items.begin(), items.end(), [](const LauncherItem& a, const LauncherItem& b) {
            bool a_act = a.subtitle.find("Active") != std::string::npos;
            bool b_act = b.subtitle.find("Active") != std::string::npos;
            if (a_act != b_act) return a_act > b_act;
            return a.title < b.title;
        });
        return items;
    }

    std::string lower_query = to_lower(query);
    std::vector<std::pair<int, LauncherItem>> scored;
    scored.reserve(items.size());

    for (auto& item : items) {
        std::string lower_title = to_lower(item.title);
        std::string lower_sub = to_lower(item.subtitle);
        int score = 0;

        if (lower_title == lower_query) {
            score = 10000;
        } else if (lower_title.rfind(lower_query, 0) == 0) {
            score = 5000 + (100 - static_cast<int>(lower_title.size()));
        } else if (lower_title.find(' ' + lower_query) != std::string::npos) {
            score = 4000;
        } else if (lower_sub.rfind(lower_query, 0) == 0) {
            score = 3000;
        } else if (lower_title.find(lower_query) != std::string::npos) {
            score = 2000;
        } else if (lower_sub.find(lower_query) != std::string::npos) {
            score = 1000;
        }

        if (score > 0) {
            if (item.subtitle.find("Active") != std::string::npos) {
                score += 100;
            }
            scored.push_back({score, std::move(item)});
        }
    }

    std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
        return a.first > b.first;
    });

    std::vector<LauncherItem> filtered;
    filtered.reserve(scored.size());
    for (auto& s : scored) {
        filtered.push_back(std::move(s.second));
    }
    return filtered;
}

void WindowProvider::activate(const LauncherItem& item) const {
    auto windows = WindowManager::get()->get_windows();
    for (auto& win : windows) {
        if (std::to_string(win.id) == item.id) {
            win.activate();
            break;
        }
    }
}

void WindowProvider::on_windows_changed(std::function<void()> cb) {
    WindowManager::get()->on_windows_changed(std::move(cb));
}

} // namespace miqu
