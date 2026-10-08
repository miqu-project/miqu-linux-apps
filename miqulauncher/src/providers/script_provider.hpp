#pragma once

#include "model/launcher_item.hpp"
#include <vector>
#include <string>

namespace miqu {

class ScriptProvider {
public:
    explicit ScriptProvider(std::string script_path);

    void refresh();
    std::vector<LauncherItem> query(const std::string& filter_text);
    void activate(const LauncherItem& item);

private:
    std::string m_script_path;
    std::vector<std::string> m_raw_items;
};

} // namespace miqu
