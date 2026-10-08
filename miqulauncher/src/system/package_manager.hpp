#pragma once

#include "model/launcher_item.hpp"
#include <vector>
#include <string>

namespace miqu {

class PackageManager {
public:
    static std::vector<LauncherItem> get_installed_applications();
    static void launch(const LauncherItem& app);
    static std::string clean_exec(const std::string& raw);
};

} // namespace miqu
