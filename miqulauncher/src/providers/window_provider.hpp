#pragma once

#include "model/launcher_item.hpp"
#include <miqutoolkit/system/window_manager.hpp>
#include <vector>
#include <string>
#include <functional>

namespace miqu {

class WindowProvider {
public:
    WindowProvider() = default;

    std::vector<LauncherItem> get_items(const std::string& query = "") const;
    void activate(const LauncherItem& item) const;
    void on_windows_changed(std::function<void()> cb);
};

} // namespace miqu
