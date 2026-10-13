#include "backend/theme_backend.hpp"
#include "ui/color_scheme_view.hpp"
#include "ui/wallpaper_view.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <iostream>
#include <fstream>
#include <memory>
#include <array>
#include <filesystem>

namespace fs = std::filesystem;
using namespace miqu;
using namespace miqutheme;

static std::string trim_str(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n\"'");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n\"'");
    return str.substr(first, (last - first + 1));
}

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: miqutheme [OPTIONS]\n\n"
                      << "Modern native theme & wallpaper manager built with miqutoolkit.\n\n"
                      << "Options:\n"
                      << "  -h, --help            Show this help message and exit\n"
                      << "  --init-config         Generate default user configuration file\n"
                      << "  -r, --restore         Restore active wallpaper and theme colors on startup\n"
                      << "  --refresh, --apply    Alias for --restore\n"
                      << "  -t, --toggle-mode     Toggle between dark and light mode headless\n"
                      << "  --mode <dark|light>   Set theme mode headless\n";
            return 0;
        } else if (arg == "--init-config") {
            std::string res = Config::init_user_config("miqutheme", "miqutheme.conf");
            if (!res.empty()) {
                std::cout << "[miqutheme] Configuration initialized at: " << res << "\n";
            } else {
                std::cout << "[miqutheme] Configuration file already exists or could not be created.\n";
            }
            return 0;
        } else if (arg == "--restore" || arg == "-r" || arg == "--refresh" || arg == "--apply" || arg == "-a") {
            auto backend = std::make_shared<ThemeBackend>();
            ThemeMode mode = backend->get_current_mode();
            std::string wp = backend->get_active_wallpaper(mode);
            std::string fit = backend->get_wallpaper_fit();
            if (!wp.empty()) {
                backend->apply_wallpaper(wp, fit);
            }
            std::string scheme = backend->get_current_scheme(mode);
            backend->apply_scheme(scheme, backend->get_matugen_type());
            std::cout << "[miqutheme] Restored " << (mode == ThemeMode::Dark ? "dark" : "light")
                      << " theme ('" << scheme << "') and wallpaper.\n";
            return 0;
        } else if (arg == "--toggle-mode" || arg == "-t") {
            auto backend = std::make_shared<ThemeBackend>();
            ThemeMode current = backend->get_current_mode();
            ThemeMode next = (current == ThemeMode::Dark) ? ThemeMode::Light : ThemeMode::Dark;
            backend->set_mode(next);
            std::cout << "[miqutheme] Toggled mode to " << (next == ThemeMode::Dark ? "dark" : "light") << ".\n";
            return 0;
        } else if (arg == "--mode" && i + 1 < argc) {
            std::string target_m = argv[++i];
            auto backend = std::make_shared<ThemeBackend>();
            if (target_m == "light") {
                backend->set_mode(ThemeMode::Light);
            } else {
                backend->set_mode(ThemeMode::Dark);
            }
            std::cout << "[miqutheme] Set mode to " << target_m << ".\n";
            return 0;
        }
    }

    auto engine = AppEngine::create();
    if (!engine) {
        std::cerr << "[miqutheme] Failed to initialize AppEngine.\n";
        return 1;
    }

    // Resolve user configuration file if present
    std::string user_cfg_dir = FsUtils::get_user_config_dir("miqutheme");
    std::string target_conf;
    if (!user_cfg_dir.empty()) {
        std::string p = user_cfg_dir + "/miqutheme.conf";
        if (fs::exists(p)) {
            target_conf = p;
        }
    }

    int default_tab = 0;

    if (!target_conf.empty()) {
        Config::get()->load_from_file(target_conf);
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
                try { default_tab = std::clamp(std::stoi(v), 0, 1); } catch (...) {}
            }
        }
    }

    auto backend = std::make_shared<ThemeBackend>();

    // Root Container
    auto root_container = std::make_shared<LinearLayout>(Orientation::Vertical);
    root_container->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    root_container->set_padding(0);

    const std::array<std::string, 2> SECTION_SUBTITLES = {
        "Color Schemes & Material 3 Engine",
        "Wallpaper Collection & Dynamic Theming"
    };

    std::shared_ptr<Window> window;
    std::shared_ptr<Toolbar> toolbar;
    std::shared_ptr<ViewPager> pager;
    std::shared_ptr<BottomNavigationView> bottom_nav;
    std::shared_ptr<ColorSchemeView> color_scheme_view;
    std::shared_ptr<WallpaperView> wallpaper_view;

    auto update_section_subtitle = [&](int idx) {
        if (toolbar && idx >= 0 && idx < static_cast<int>(SECTION_SUBTITLES.size())) {
            toolbar->set_subtitle(SECTION_SUBTITLES[idx]);
        }
    };

    auto refresh_pending = std::make_shared<bool>(false);
    auto refresh_all = [&, engine, refresh_pending]() {
        if (*refresh_pending) return;
        *refresh_pending = true;
        engine->post([&, refresh_pending]() {
            *refresh_pending = false;
            if (color_scheme_view) color_scheme_view->refresh_ui();
            if (wallpaper_view) wallpaper_view->refresh_ui();
            if (window) window->schedule_redraw();
        });
    };

    backend->set_on_theme_changed([&]() {
        refresh_all();
    });

    // 1. TOP TOOLBAR
    toolbar = ToolbarBuilder::create()
        ->title("Theme Manager")
        ->subtitle(SECTION_SUBTITLES[default_tab])
        ->titleAlignment(TitleAlignment::Center)
        ->onRefresh(refresh_all)
        ->onClose([&window, engine]() {
            if (window) window->request_close();
            else engine->quit();
        })
        ->build();
    toolbar->set_margin(14, 12, 14, 8);
    root_container->add_view(toolbar);

    // 2. VIEW PAGER
    pager = ViewPagerBuilder::create()
        ->currentPage(default_tab)
        ->onPageChanged([&](int old_idx, int new_idx) {
            if (bottom_nav && bottom_nav->get_selected_index() != new_idx) {
                bottom_nav->set_selected_index(new_idx);
            }
            update_section_subtitle(new_idx);
            refresh_all();
        })
        ->build();
    pager->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        0,
        1.0f
    ));
    pager->set_margin(0, 0, 0, 0);

    // Page 0: Color Scheme View
    color_scheme_view = std::make_shared<ColorSchemeView>(backend, refresh_all);
    pager->add_page(color_scheme_view);

    // Page 1: Wallpapers View
    wallpaper_view = std::make_shared<WallpaperView>(backend, refresh_all);
    pager->add_page(wallpaper_view);

    root_container->add_view(pager);

    // 3. MASTER BOTTOM NAVIGATION BAR
    bottom_nav = BottomNavigationViewBuilder::create()
        ->addItem("Color Scheme", "preferences-desktop-theme")
        ->addItem("Wallpapers", "preferences-desktop-wallpaper")
        ->selectedIndex(default_tab)
        ->onItemSelected([&](int idx) {
            if (pager && pager->get_current_page() != idx) {
                pager->set_current_page(idx);
            }
        })
        ->build();
    bottom_nav->set_margin(14, 6, 14, 12);
    root_container->add_view(bottom_nav);

    // 4. WAYLAND WINDOW
    window = WindowBuilder::create()
        ->title("Theme Manager - miquland")
        ->appId("miqutheme")
        ->role(WindowRole::Toplevel)
        ->preferredSize(650, 720)
        ->closeOnEscape(true)
        ->contentView(root_container)
        ->onClose([engine]() {
            engine->quit();
        })
        ->onKey([&](const KeyPressEvent& ev) {
            if (!ev.pressed) return;
            switch (ev.keysym) {
                case XKB_KEY_1:
                    if (pager) pager->set_current_page(0);
                    break;
                case XKB_KEY_2:
                    if (pager) pager->set_current_page(1);
                    break;
                case XKB_KEY_r:
                case XKB_KEY_R:
                    refresh_all();
                    break;
                case XKB_KEY_Escape:
                case XKB_KEY_q:
                case XKB_KEY_Q:
                    if (window) window->request_close();
                    else engine->quit();
                    break;
                default:
                    break;
            }
        })
        ->build();

    if (!window) {
        std::cerr << "[miqutheme] Failed to create Wayland window.\n";
        return 1;
    }

    // Live theme updates across the ecosystem
    engine->add_theme_change_listener([&]() {
        refresh_all();
    });

    window->show();
    return engine->enter_loop();
}
