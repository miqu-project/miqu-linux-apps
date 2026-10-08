#pragma once

#include <string>

namespace miqu {

struct LauncherItem {
    std::string id;
    std::string title;
    std::string subtitle;
    std::string icon_name;
    std::string icon_path;
    std::string badge;
    std::string exec_cmd;
    std::string return_value;
    std::string meta_tags;
    std::string keywords;
    bool is_image = false;
    bool non_selectable = false;
    bool terminal = false;
    bool is_active = false;
};

} // namespace miqu
