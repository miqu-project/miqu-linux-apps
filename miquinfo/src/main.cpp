#include "info_app.hpp"
#include "sys_info.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

static std::string trim_str(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n\"'");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n\"'");
    return str.substr(first, (last - first + 1));
}

int main(int argc, char* argv[]) {
    std::string custom_config = "";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--print" || arg == "-p" || arg == "--summary" || arg == "-s") {
            miquinfo::SystemInfo info = miquinfo::SysInfoReader::gather();
            std::cout << info.to_plain_text();
            return 0;
        } else if (arg == "--markdown" || arg == "-m") {
            miquinfo::SystemInfo info = miquinfo::SysInfoReader::gather();
            std::cout << info.to_markdown();
            return 0;
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: miquinfo [options]\n\n"
                      << "Modern native system information utility built with miqutoolkit.\n\n"
                      << "Options:\n"
                      << "  -p, --print          Print system summary to stdout\n"
                      << "  -m, --markdown       Print system summary in markdown format\n"
                      << "  --init-config        Generate default user configuration file\n"
                      << "  -h, --help           Show this help message and exit\n";
            return 0;
        } else if (arg == "--init-config") {
            std::string res = miqu::Config::init_user_config("miquinfo", "miquinfo.conf");
            if (!res.empty()) {
                std::cout << "[miquinfo] Configuration initialized at: " << res << "\n";
            } else {
                std::cout << "[miquinfo] Configuration file already exists or could not be created.\n";
            }
            return 0;
        }
    }

    auto engine = miqu::AppEngine::create();
    if (!engine) {
        std::cerr << "[miquinfo] Failed to initialize AppEngine.\n";
        return 1;
    }

    // 1. Resolve user configuration file if present (no auto-seeding on startup)
    std::string user_cfg_dir = miqu::FsUtils::get_user_config_dir("miquinfo");
    std::string target_conf;
    if (!user_cfg_dir.empty()) {
        std::string p = user_cfg_dir + "/miquinfo.conf";
        if (fs::exists(p)) {
            target_conf = p;
        }
    }

    int default_tab = 0;

    if (!target_conf.empty()) {
        // Overlay any toolkit appearance overrides (colors, fonts, metrics, icon_theme)
        miqu::Config::get()->load_from_file(target_conf);
        engine->setup_config_watcher();

        std::ifstream file(target_conf);
        std::string line;
        while (std::getline(file, line)) {
            line = trim_str(line);
            if (line.empty() || line[0] == '#' || line[0] == ';') continue;
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string k = trim_str(line.substr(0, eq));
            std::string v = trim_str(line.substr(eq + 1));
            size_t cp = v.find('#');
            if (cp != std::string::npos) v = trim_str(v.substr(0, cp));

            if (k == "default_tab" && !v.empty()) {
                try { default_tab = std::clamp(std::stoi(v), 0, 4); } catch (...) {}
            }
        }
    }

    miquinfo::InfoApp app(engine);
    if (!app.init()) {
        std::cerr << "[miquinfo] Failed to initialize UI window.\n";
        return 1;
    }

    if (default_tab > 0) {
        app.select_page(default_tab);
    }

    return app.run();
}
