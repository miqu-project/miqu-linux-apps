#pragma once

#include "model/launcher_item.hpp"
#include <vector>
#include <string>

namespace miqu {

class DmenuProvider {
public:
    DmenuProvider();

    void load_from_stdin();
    std::vector<LauncherItem> query(const std::string& filter_text);
    void activate(const LauncherItem& item);
    void submit(const std::string& raw_text);

private:
    static LauncherItem parse_line(const std::string& line, size_t index);
    std::vector<LauncherItem> m_items;
};

} // namespace miqu
