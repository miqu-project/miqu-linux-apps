#pragma once

#include <string>
#include <vector>

namespace miqu {

enum class ModeType {
    App,
    Window,
    Workspace,
    Run,
    Script,
    Dmenu
};

struct ModeSpec {
    ModeType type = ModeType::App;
    std::string name;          // e.g. "drun", "window", "workspaces", "run", or custom name
    std::string display_label; // Shown in SearchView pill, e.g. "apps", "window", "power"
    std::string hint;          // Placeholder in search input
    std::string script_path;   // Used if type == ModeType::Script
};

struct LauncherConfig {
    std::vector<ModeSpec> modes;
    size_t active_mode_index = 0;
    bool allow_mode_switch = true;  // true for multi-mode, false for single -show or -dmenu
    std::string initial_query = ""; // from -filter or -q
    bool dmenu_mode = false;

    // Window & Grid dimensions
    int width = 800;
    int height = 460;
    int cell_size = 100;
    int spacing = 10;
    bool dim_backdrop = false;
    bool show_subtitles = true;
    std::string icon_theme = "";
    std::string config_path = "";
};

} // namespace miqu
