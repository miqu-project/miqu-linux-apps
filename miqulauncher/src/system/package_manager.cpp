#include "package_manager.hpp"
#include <miqutoolkit/system/app_manager.hpp>

namespace miqu {

std::string PackageManager::clean_exec(const std::string& raw) {
    return AppManager::clean_exec(raw);
}

std::vector<LauncherItem> PackageManager::get_installed_applications() {
    std::vector<LauncherItem> items;
    const auto& apps = AppManager::get()->get_installed_apps();
    items.reserve(apps.size());

    for (const auto& app : apps) {
        LauncherItem item;
        item.id = app.id;
        item.title = app.name;
        item.subtitle = !app.generic_name.empty() ? app.generic_name : app.comment;
        item.icon_name = app.icon_name;
        item.icon_path = app.icon_path;
        item.exec_cmd = app.exec_cmd;
        item.keywords = app.keywords;
        item.terminal = app.terminal;
        items.push_back(std::move(item));
    }

    return items;
}

void PackageManager::launch(const LauncherItem& item) {
    if (item.exec_cmd.empty()) return;
    AppManager::launch_command(item.exec_cmd, item.terminal);
}

} // namespace miqu
