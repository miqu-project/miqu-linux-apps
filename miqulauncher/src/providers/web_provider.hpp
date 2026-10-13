#pragma once

#include "model/launcher_item.hpp"
#include <string>
#include <vector>
#include <optional>

namespace miqu {

class WebProvider {
public:
    WebProvider() = default;

    std::vector<LauncherItem> get_items(const std::string& query);
    void activate(const LauncherItem& item);

    static std::string url_encode(const std::string& value);
};

} // namespace miqu
