#pragma once

#include "model/launcher_item.hpp"
#include <vector>
#include <string>

namespace miqu {

class RunProvider {
public:
    RunProvider() = default;

    std::vector<LauncherItem> get_items(const std::string& query = "") const;
    void launch(const LauncherItem& item) const;
    void launch_raw(const std::string& cmd) const;
};

} // namespace miqu
