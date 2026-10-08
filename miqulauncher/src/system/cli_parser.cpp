#include "cli_parser.hpp"
#include <miqutoolkit/core/config.hpp>
#include <iostream>
#include <sstream>
#include <cstring>
#include <algorithm>

namespace miqu {

static ModeSpec create_builtin_mode(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "drun" || lower == "apps" || lower == "app") {
        return ModeSpec{ ModeType::App, "drun", "apps", "Search applications...", "" };
    }
    if (lower == "window" || lower == "windows") {
        return ModeSpec{ ModeType::Window, "window", "window", "Switch to open window...", "" };
    }
    if (lower == "workspaces" || lower == "workspace") {
        return ModeSpec{ ModeType::Workspace, "workspaces", "workspaces", "Switch workspace...", "" };
    }
    if (lower == "run") {
        return ModeSpec{ ModeType::Run, "run", "run", "Run command line or binary...", "" };
    }

    // Default to app if unknown builtin
    return ModeSpec{ ModeType::App, lower, lower, "Search...", "" };
}

static ModeSpec parse_mode_entry(const std::string& entry) {
    size_t colon = entry.find(':');
    if (colon != std::string::npos && colon > 0 && colon + 1 < entry.size()) {
        std::string name = entry.substr(0, colon);
        std::string script = entry.substr(colon + 1);
        return ModeSpec{ ModeType::Script, name, name, "Filter " + name + "...", script };
    }
    return create_builtin_mode(entry);
}

void CliParser::print_help(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [options]\n\n"
              << "A modern, minimalist application launcher and dmenu runner.\n\n"
              << "Options:\n"
              << "  -show <mode>         Run in dedicated mode (drun, window, workspaces, run)\n"
              << "  -modes <list>        Comma-separated list of enabled modes (e.g. 'drun,run' or 'drun,power:~/bin/power.sh')\n"
              << "  -dmenu               Run in dmenu mode (read items from stdin, write choice to stdout)\n"
              << "                       Supports Rofi metadata: label\\0icon\\x1f<path>\\x1finfo\\x1f<val>\\x1fmeta\\x1f<tags>\n"
              << "  -c, --config <path>  Custom configuration file path\n"
              << "  -p, -mesg <text>     Custom prompt label for the search pill\n"
              << "  -filter, -q <text>   Pre-fill search filter query\n"
              << "  -cell-size <px>      Override grid cell size (e.g. 140 for image/wallpaper thumbnails)\n"
              << "  -width <px>          Override launcher window width\n"
              << "  -height <px>         Override launcher window height\n"
              << "  --dim                Enable dimming background overlay outside window\n"
              << "  --no-dim             Disable dimming background overlay outside window (default)\n"
              << "  --show-subtitles     Show item subtitles across all launcher modes (default)\n"
              << "  --no-subtitles       Hide item subtitles across all launcher modes\n"
              << "  -icon-theme <name>   Override active icon theme\n"
              << "  -h, --help           Show this help message and exit\n"
              << "  -v, --version        Show version information and exit\n"
              << std::endl;
}

void CliParser::print_version() {
    std::cout << "miqulauncher 1.0.0 (miqutoolkit)\n"
              << "Wayland Desktop Launcher & Dmenu\n"
              << std::endl;
}

CliParser::ParseResult CliParser::parse(int argc, char* argv[], LauncherConfig& out_config) {
    std::string show_mode = "";
    std::string modes_str = "";
    bool is_dmenu = false;
    std::string prompt_override = "";
    std::string query_filter = "";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            print_help(argv[0]);
            return ParseResult::ExitSuccess;
        }
        if (arg == "-v" || arg == "--version") {
            print_version();
            return ParseResult::ExitSuccess;
        }
        if (arg == "-dmenu" || arg == "--dmenu") {
            is_dmenu = true;
        } else if ((arg == "-show" || arg == "--show") && i + 1 < argc) {
            show_mode = argv[++i];
        } else if ((arg == "-modes" || arg == "--modes") && i + 1 < argc) {
            modes_str = argv[++i];
        } else if ((arg == "-c" || arg == "--config" || arg == "-config") && i + 1 < argc) {
            out_config.config_path = argv[++i];
        } else if ((arg == "-p" || arg == "--prompt" || arg == "-mesg") && i + 1 < argc) {
            prompt_override = argv[++i];
        } else if ((arg == "-filter" || arg == "--filter" || arg == "-q" || arg == "--query") && i + 1 < argc) {
            query_filter = argv[++i];
        } else if ((arg == "-cell-size" || arg == "--cell-size") && i + 1 < argc) {
            try { out_config.cell_size = std::max(30, std::stoi(argv[++i])); } catch (...) {}
        } else if ((arg == "-width" || arg == "--width") && i + 1 < argc) {
            try { out_config.width = std::max(200, std::stoi(argv[++i])); } catch (...) {}
        } else if ((arg == "-height" || arg == "--height") && i + 1 < argc) {
            try { out_config.height = std::max(150, std::stoi(argv[++i])); } catch (...) {}
        } else if (arg == "--dim" || arg == "-dim") {
            out_config.dim_backdrop = true;
        } else if (arg == "--no-dim" || arg == "-no-dim") {
            out_config.dim_backdrop = false;
        } else if (arg == "--show-subtitles") {
            out_config.show_subtitles = true;
        } else if (arg == "--no-subtitles") {
            out_config.show_subtitles = false;
        } else if ((arg == "-icon-theme" || arg == "--icon-theme") && i + 1 < argc) {
            out_config.icon_theme = argv[++i];
            Config::get()->metrics.icon_theme = out_config.icon_theme;
            Config::get()->notify_changed();
        } else {
            std::cerr << "Unknown option: " << arg << "\nRun '" << argv[0] << " --help' for usage." << std::endl;
            return ParseResult::Error;
        }
    }

    out_config.initial_query = query_filter;

    // 1. Dmenu Mode
    if (is_dmenu) {
        out_config.dmenu_mode = true;
        out_config.allow_mode_switch = false;
        std::string label = prompt_override.empty() ? "menu" : prompt_override;
        out_config.modes.push_back(ModeSpec{ ModeType::Dmenu, "dmenu", label, "Type to filter...", "" });
        out_config.active_mode_index = 0;
        return ParseResult::Success;
    }

    // 2. Custom Modes List
    if (!modes_str.empty()) {
        std::stringstream ss(modes_str);
        std::string item;
        while (std::getline(ss, item, ',')) {
            if (!item.empty()) {
                out_config.modes.push_back(parse_mode_entry(item));
            }
        }
    }

    // 3. Single -show mode without -modes
    if (!show_mode.empty() && out_config.modes.empty()) {
        auto spec = create_builtin_mode(show_mode);
        if (!prompt_override.empty()) {
            spec.display_label = prompt_override;
        }
        out_config.modes.push_back(spec);
        out_config.active_mode_index = 0;
        out_config.allow_mode_switch = false;
        return ParseResult::Success;
    }

    // If no modes specified yet, default to all 4 standard modes
    if (out_config.modes.empty()) {
        out_config.modes.push_back(ModeSpec{ ModeType::App, "drun", "apps", "Search applications...", "" });
        out_config.modes.push_back(ModeSpec{ ModeType::Window, "window", "window", "Switch to open window...", "" });
        out_config.modes.push_back(ModeSpec{ ModeType::Workspace, "workspaces", "workspaces", "Switch workspace...", "" });
        out_config.modes.push_back(ModeSpec{ ModeType::Run, "run", "run", "Run command line or binary...", "" });
    }

    // Find initial active mode if -show was specified with -modes or default
    out_config.active_mode_index = 0;
    if (!show_mode.empty()) {
        std::string lower_show = show_mode;
        std::transform(lower_show.begin(), lower_show.end(), lower_show.begin(), ::tolower);
        for (size_t i = 0; i < out_config.modes.size(); ++i) {
            if (out_config.modes[i].name == lower_show ||
                (lower_show == "apps" && out_config.modes[i].name == "drun") ||
                (lower_show == "drun" && out_config.modes[i].name == "apps") ||
                (lower_show == "windows" && out_config.modes[i].name == "window")) {
                out_config.active_mode_index = i;
                break;
            }
        }
    }

    out_config.allow_mode_switch = (out_config.modes.size() > 1);

    if (!prompt_override.empty() && !out_config.modes.empty()) {
        out_config.modes[out_config.active_mode_index].display_label = prompt_override;
    }

    return ParseResult::Success;
}

} // namespace miqu
