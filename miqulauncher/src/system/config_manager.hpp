#pragma once

#include "model/launcher_mode.hpp"
#include <string>

namespace miqu {

class ConfigManager {
public:
    static void load(LauncherConfig& config);
    static std::string get_user_config_path();
};

} // namespace miqu
