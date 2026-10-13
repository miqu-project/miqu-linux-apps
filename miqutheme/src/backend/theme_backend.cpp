#include "theme_backend.hpp"
#include <miqutoolkit/miqutoolkit.hpp>

#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <array>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>

extern "C" char** environ;

namespace miqutheme {

namespace fs = std::filesystem;

static bool run_command_sync(const std::vector<std::string>& args) {
    if (args.empty()) return false;
    std::vector<char*> c_args;
    c_args.reserve(args.size() + 1);
    for (const auto& a : args) {
        c_args.push_back(const_cast<char*>(a.c_str()));
    }
    c_args.push_back(nullptr);

    pid_t pid;
    int status = posix_spawnp(&pid, c_args[0], nullptr, nullptr, c_args.data(), environ);
    if (status != 0) {
        return false;
    }

    int wait_status = 0;
    while (waitpid(pid, &wait_status, 0) == -1) {
        if (errno != EINTR) return false;
    }
    return WIFEXITED(wait_status) && WEXITSTATUS(wait_status) == 0;
}

static void run_command_async(const std::vector<std::string>& args) {
    if (args.empty()) return;
    std::vector<char*> c_args;
    c_args.reserve(args.size() + 1);
    for (const auto& a : args) {
        c_args.push_back(const_cast<char*>(a.c_str()));
    }
    c_args.push_back(nullptr);

    pid_t pid;
    posix_spawnp(&pid, c_args[0], nullptr, nullptr, c_args.data(), environ);
}

static std::string read_file_string(const fs::path& p) {
    if (!fs::exists(p)) return "";
    std::ifstream f(p);
    if (!f.is_open()) return "";
    std::string s;
    f >> s;
    return s;
}

static void write_file_string(const fs::path& p, const std::string& val) {
    fs::create_directories(p.parent_path());
    std::ofstream f(p, std::ios::trunc);
    if (f.is_open()) {
        f << val << "\n";
    }
}

static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n\"'");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n\"'");
    return str.substr(first, (last - first + 1));
}

ThemeBackend::ThemeBackend() {
    std::string user_cfg = miqu::FsUtils::get_user_config_dir("miqutheme");
    if (user_cfg.empty()) {
        const char* home = getenv("HOME");
        user_cfg = (home ? std::string(home) + "/.config/miqutheme" : ".config/miqutheme");
    }
    config_dir_ = user_cfg;
    current_dir_ = config_dir_ / "current";
    schemes_dir_ = config_dir_ / "schemes";
    wallpapers_dir_ = config_dir_ / "wallpapers";
    matugen_dir_ = config_dir_ / "matugen";

    ensure_directories_and_seed();
}

void ThemeBackend::ensure_directories_and_seed() {
    fs::create_directories(config_dir_);
    fs::create_directories(current_dir_);

    seed_default_assets_if_needed();

    // Default state files if missing
    if (!fs::exists(config_dir_ / "mode")) {
        write_file_string(config_dir_ / "mode", "dark");
    }
    if (!fs::exists(config_dir_ / "current_dark")) {
        write_file_string(config_dir_ / "current_dark", "dynamic-matugen");
    }
    if (!fs::exists(config_dir_ / "current_light")) {
        write_file_string(config_dir_ / "current_light", "dynamic-matugen");
    }
    if (!fs::exists(config_dir_ / "matugen_type")) {
        write_file_string(config_dir_ / "matugen_type", "scheme-tonal-spot");
    }
    if (!fs::exists(config_dir_ / "transparency")) {
        write_file_string(config_dir_ / "transparency", "75%");
    }

    // Ensure active scheme exists in current/
    if (!fs::exists(current_dir_ / "miqutoolkit.conf")) {
        ThemeMode mode = get_current_mode();
        std::string scheme = get_current_scheme(mode);
        apply_scheme(scheme, get_matugen_type());
    }
}

void ThemeBackend::seed_default_assets_if_needed() {
    if (fs::exists(schemes_dir_) && !fs::is_empty(schemes_dir_)) {
        return;
    }

    // Candidate asset directories to copy from
    std::vector<fs::path> candidates = {
        "/usr/share/miqutheme",
        "/usr/local/share/miqutheme",
        fs::current_path() / "assets",
        fs::current_path() / "../assets"
    };

    const char* home = getenv("HOME");
    if (home) {
        candidates.push_back(fs::path(home) / "miqu-project/miqu-linux-apps/miqutheme/assets");
    }

    for (const auto& src : candidates) {
        if (fs::exists(src / "schemes")) {
            std::error_code ec;
            fs::copy(src / "schemes", schemes_dir_, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
            if (fs::exists(src / "wallpapers")) {
                fs::copy(src / "wallpapers", wallpapers_dir_, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
            }
            if (fs::exists(src / "matugen")) {
                fs::copy(src / "matugen", matugen_dir_, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
            }
            if (fs::exists(src / "default_dark_wallpaper.png")) {
                fs::copy(src / "default_dark_wallpaper.png", config_dir_ / "default_dark_wallpaper.png", ec);
            }
            if (fs::exists(src / "default_light_wallpaper.png")) {
                fs::copy(src / "default_light_wallpaper.png", config_dir_ / "default_light_wallpaper.png", ec);
            }
            break;
        }
    }
}

ThemeMode ThemeBackend::get_current_mode() const {
    std::string m = read_file_string(config_dir_ / "mode");
    return (m == "light") ? ThemeMode::Light : ThemeMode::Dark;
}

void ThemeBackend::set_mode(ThemeMode mode) {
    ThemeMode prev = get_current_mode();
    std::string m_str = (mode == ThemeMode::Light) ? "light" : "dark";
    write_file_string(config_dir_ / "mode", m_str);

    if (prev != mode) {
        std::string target_wp = get_active_wallpaper(mode);
        std::string fit = get_wallpaper_fit();
        if (!target_wp.empty() && fs::exists(target_wp)) {
            apply_wallpaper(target_wp, fit);
        }

        std::string scheme = get_current_scheme(mode);
        apply_scheme(scheme, get_matugen_type());
    }
}

std::string ThemeBackend::get_current_scheme(ThemeMode mode) const {
    fs::path p = (mode == ThemeMode::Light) ? (config_dir_ / "current_light") : (config_dir_ / "current_dark");
    std::string s = read_file_string(p);
    return s.empty() ? "dynamic-matugen" : s;
}

std::string ThemeBackend::get_matugen_type() const {
    std::string t = read_file_string(config_dir_ / "matugen_type");
    return t.empty() ? "scheme-tonal-spot" : t;
}

void ThemeBackend::set_matugen_type(const std::string& type) {
    write_file_string(config_dir_ / "matugen_type", type);
    ThemeMode mode = get_current_mode();
    if (get_current_scheme(mode) == "dynamic-matugen") {
        apply_scheme("dynamic-matugen", type);
    }
}

const std::vector<std::pair<std::string, std::string>>& ThemeBackend::get_matugen_variants() {
    static const std::vector<std::pair<std::string, std::string>> variants = {
        {"Tonal Spot", "scheme-tonal-spot"},
        {"Vibrant", "scheme-vibrant"},
        {"Expressive", "scheme-expressive"},
        {"Fidelity", "scheme-fidelity"},
        {"Fruit Salad", "scheme-fruit-salad"},
        {"Rainbow", "scheme-rainbow"},
        {"Neutral", "scheme-neutral"},
        {"Monochrome", "scheme-monochrome"},
        {"Content", "scheme-content"}
    };
    return variants;
}

std::string ThemeBackend::format_display_name(const std::string& raw) {
    if (raw == "dynamic-matugen") return "Material You (Dynamic Wallpaper)";
    std::string res;
    bool capitalize_next = true;
    for (char c : raw) {
        if (c == '-' || c == '_') {
            res += ' ';
            capitalize_next = true;
        } else if (capitalize_next) {
            res += static_cast<char>(std::toupper(c));
            capitalize_next = false;
        } else {
            res += c;
        }
    }
    return res;
}

SchemeInfo ThemeBackend::parse_scheme_file(const fs::path& conf_path, const std::string& name) {
    SchemeInfo info;
    info.name = name;
    info.display_name = format_display_name(name);
    info.is_dynamic = (name == "dynamic-matugen");

    if (!fs::exists(conf_path)) return info;

    std::ifstream file(conf_path);
    std::string line;
    std::vector<std::string> discovered_colors;

    while (std::getline(file, line)) {
        size_t comment = line.find('#');
        if (comment != std::string::npos) line = line.substr(0, comment);
        line = trim(line);
        if (line.empty()) continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));
        if (val.length() > 7) val = val.substr(0, 7); // strip alpha for chips

        if (key == "background") info.background = val;
        else if (key == "surface") info.surface = val;
        else if (key == "primary") info.primary = val;

        if (val.rfind("#", 0) == 0 && val.length() == 7) {
            if (std::find(discovered_colors.begin(), discovered_colors.end(), val) == discovered_colors.end()) {
                discovered_colors.push_back(val);
            }
        }
    }

    if (!info.primary.empty()) info.swatches.push_back(info.primary);
    if (!info.surface.empty()) info.swatches.push_back(info.surface);
    if (!info.background.empty()) info.swatches.push_back(info.background);

    for (const auto& c : discovered_colors) {
        if (info.swatches.size() >= 5) break;
        if (std::find(info.swatches.begin(), info.swatches.end(), c) == info.swatches.end()) {
            info.swatches.push_back(c);
        }
    }

    return info;
}

SchemeInfo ThemeBackend::get_dynamic_scheme_info(ThemeMode mode) {
    SchemeInfo info;
    info.name = "dynamic-matugen";
    info.display_name = "Material You (Dynamic Wallpaper)";
    info.is_dynamic = true;

    fs::path conf = current_dir_ / "miqutoolkit.conf";
    if (fs::exists(conf)) {
        return parse_scheme_file(conf, "dynamic-matugen");
    }

    std::string mode_str = (mode == ThemeMode::Light) ? "light" : "dark";
    fs::path default_conf = schemes_dir_ / mode_str / "dynamic-matugen" / "miqutoolkit.conf";
    return parse_scheme_file(default_conf, "dynamic-matugen");
}

std::vector<SchemeInfo> ThemeBackend::get_available_schemes(ThemeMode mode) {
    std::vector<SchemeInfo> list;
    std::string mode_str = (mode == ThemeMode::Light) ? "light" : "dark";
    fs::path dir = schemes_dir_ / mode_str;

    if (!fs::exists(dir)) return list;

    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_directory()) continue;
        std::string sname = entry.path().filename().string();
        if (sname == "dynamic-matugen") continue; // Handled separately as featured full-width card

        fs::path conf_path = entry.path() / "miqutoolkit.conf";
        list.push_back(parse_scheme_file(conf_path, sname));
    }

    std::sort(list.begin(), list.end(), [](const SchemeInfo& a, const SchemeInfo& b) {
        return a.display_name < b.display_name;
    });

    return list;
}

std::string ThemeBackend::get_active_wallpaper(ThemeMode mode) const {
    std::string m_str = (mode == ThemeMode::Light) ? "light" : "dark";
    fs::path mode_wp = current_dir_ / ("wallpaper_" + m_str);
    std::string p = read_file_string(mode_wp);
    if (!p.empty() && fs::exists(p)) return p;

    fs::path def = config_dir_ / ("default_" + m_str + "_wallpaper.png");
    if (fs::exists(def)) return def.string();

    fs::path wp_dir = wallpapers_dir_ / m_str;
    if (fs::exists(wp_dir)) {
        for (const auto& e : fs::directory_iterator(wp_dir)) {
            if (e.is_regular_file()) return e.path().string();
        }
    }

    fs::path cur = current_dir_ / "wallpaper";
    p = read_file_string(cur);
    if (!p.empty() && fs::exists(p)) return p;

    return "";
}

std::string ThemeBackend::get_wallpaper_fit() const {
    std::string f = read_file_string(current_dir_ / "wallpaper_fit");
    return f.empty() ? "fill" : f;
}

std::vector<WallpaperInfo> ThemeBackend::get_wallpapers(ThemeMode mode) {
    std::vector<WallpaperInfo> res;
    std::string m_str = (mode == ThemeMode::Light) ? "light" : "dark";
    fs::path dir = wallpapers_dir_ / m_str;

    if (!fs::exists(dir)) return res;

    for (const auto& e : fs::directory_iterator(dir)) {
        if (!e.is_regular_file()) continue;
        std::string ext = e.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".webp") {
            WallpaperInfo w;
            w.filename = e.path().filename().string();
            w.full_path = e.path().string();
            w.mode = mode;
            res.push_back(w);
        }
    }

    std::sort(res.begin(), res.end(), [](const WallpaperInfo& a, const WallpaperInfo& b) {
        return a.filename < b.filename;
    });

    return res;
}

void ThemeBackend::apply_wallpaper(const std::string& path, const std::string& fit) {
    if (!fs::exists(path)) return;

    ThemeMode mode = get_current_mode();
    std::string m_str = (mode == ThemeMode::Light) ? "light" : "dark";

    write_file_string(current_dir_ / "wallpaper", path);
    write_file_string(current_dir_ / ("wallpaper_" + m_str), path);
    write_file_string(current_dir_ / "wallpaper_fit", fit);

    // Invoke miqubg via IPC / spawn
    run_command_async({"miqubg", "-i", path, "-m", fit});

    // If dynamic matugen is active, re-extract colors
    if (get_current_scheme(mode) == "dynamic-matugen") {
        apply_scheme("dynamic-matugen", get_matugen_type());
    }
}

int ThemeBackend::get_transparency() const {
    std::string s = read_file_string(config_dir_ / "transparency");
    int val = 75;
    try {
        std::string num_str;
        for (char c : s) if (std::isdigit(c)) num_str += c;
        if (!num_str.empty()) val = std::clamp(std::stoi(num_str), 0, 100);
    } catch (...) {}
    return val;
}

void ThemeBackend::apply_transparency(int percentage, bool notify_listeners) {
    int val = std::clamp(percentage, 0, 100);
    write_file_string(config_dir_ / "transparency", std::to_string(val) + "%");

    double opacity_dec = val / 100.0;
    std::ostringstream oss_dec;
    oss_dec.precision(2);
    oss_dec << std::fixed << opacity_dec;
    std::string op_dec_str = (val == 100) ? "1.0" : (val == 0 ? "0.0" : oss_dec.str());

    int alpha_int = (val * 255 + 50) / 100;
    char hex_buf[4];
    std::snprintf(hex_buf, sizeof(hex_buf), "%02X", alpha_int);
    std::string alpha_hex(hex_buf);

    // 1. Update miqutoolkit.conf
    fs::path miqu_conf = current_dir_ / "miqutoolkit.conf";
    if (fs::exists(miqu_conf)) {
        std::ifstream in(miqu_conf);
        std::ostringstream out;
        std::string line;
        while (std::getline(in, line)) {
            std::string t = trim(line);
            if (t.rfind("background", 0) == 0 || t.rfind("surface", 0) == 0) {
                auto eq = line.find('=');
                if (eq != std::string::npos) {
                    std::string k = line.substr(0, eq);
                    std::string v = trim(line.substr(eq + 1));
                    if (v.length() >= 7 && v[0] == '#') {
                        v = v.substr(0, 7);
                        if (val < 100) v += alpha_hex;
                    }
                    out << k << "= " << v << "\n";
                    continue;
                }
            }
            out << line << "\n";
        }
        write_file_string(miqu_conf, out.str());

        // Copy to ~/.config/miqutoolkit/miqutoolkit.conf
        const char* home = getenv("HOME");
        if (home) {
            fs::path dest = fs::path(home) / ".config/miqutoolkit/miqutoolkit.conf";
            std::error_code ec;
            fs::copy_file(miqu_conf, dest, fs::copy_options::overwrite_existing, ec);
        }
    }

    // 2. Update kitty.conf
    fs::path kitty_conf = current_dir_ / "kitty.conf";
    if (fs::exists(kitty_conf)) {
        std::ifstream in(kitty_conf);
        std::ostringstream out;
        std::string line;
        bool found = false;
        while (std::getline(in, line)) {
            if (line.rfind("background_opacity", 0) == 0) {
                out << "background_opacity " << op_dec_str << "\n";
                found = true;
            } else {
                out << line << "\n";
            }
        }
        if (!found) out << "\nbackground_opacity " << op_dec_str << "\n";
        write_file_string(kitty_conf, out.str());

        // Signal kitty
        run_command_async({"killall", "-SIGUSR1", "kitty"});
    }

    // 3. Dunst notifications reload
    fs::path dunst_conf = current_dir_ / "dunst.conf";
    if (fs::exists(dunst_conf)) {
        run_command_async({"dunstctl", "reload"});
    }

    // 4. Reload local miqutoolkit config
    miqu::Config::get()->load_from_file(miqu_conf.string());

    if (notify_listeners && on_theme_changed_) {
        on_theme_changed_();
    }
}

void ThemeBackend::regenerate_matugen(ThemeMode mode, const std::string& wp_path, const std::string& matugen_type) {
    if (!fs::exists(wp_path)) return;

    std::string m_str = (mode == ThemeMode::Light) ? "light" : "dark";
    fs::path cfg = matugen_dir_ / ("config_" + m_str + ".toml");
    if (!fs::exists(cfg)) return;

    std::vector<std::string> args = {
        "matugen", "image", wp_path,
        "-c", cfg.string(),
        "-t", matugen_type,
        "--source-color-index", "0",
        "-m", m_str
    };

    if (mode == ThemeMode::Light) {
        args.push_back("--prefer");
        args.push_back("saturation");
        args.push_back("--contrast");
        args.push_back("0.6");
    }

    run_command_sync(args);
}

void ThemeBackend::update_ide_theme(ThemeMode mode, const std::string& miqutoolkit_conf) {
    const char* home = getenv("HOME");
    if (!home) return;
    fs::path gemini_cfg = fs::path(home) / ".gemini/config/config.json";
    if (!fs::exists(gemini_cfg)) return;

    SchemeInfo info = parse_scheme_file(miqutoolkit_conf, "current");

    // Read config.json and patch fields
    std::ifstream in(gemini_cfg);
    if (!in.is_open()) return;
    std::stringstream buf;
    buf << in.rdbuf();
    std::string content = buf.str();

    std::string target_mode_str = (mode == ThemeMode::Dark) ? "THEME_MODE_DARK" : "THEME_MODE_LIGHT";
    // Replace themeMode in userSettings
    size_t pos = content.find("\"themeMode\"");
    if (pos != std::string::npos) {
        size_t colon = content.find(':', pos);
        size_t quote1 = content.find('"', colon);
        size_t quote2 = content.find('"', quote1 + 1);
        if (quote1 != std::string::npos && quote2 != std::string::npos) {
            content.replace(quote1 + 1, quote2 - quote1 - 1, target_mode_str);
            write_file_string(gemini_cfg, content);
        }
    }
}

void ThemeBackend::update_gtk_theme(ThemeMode mode) {
    std::string gtk_theme = (mode == ThemeMode::Dark) ? "Adwaita-dark" : "Adwaita";
    std::string icon_theme = (mode == ThemeMode::Dark) ? "Papirus-Dark" : "Papirus-Light";
    std::string scheme = (mode == ThemeMode::Dark) ? "prefer-dark" : "prefer-light";

    run_command_sync({"gsettings", "set", "org.gnome.desktop.interface", "gtk-theme", gtk_theme});
    run_command_sync({"gsettings", "set", "org.gnome.desktop.interface", "icon-theme", icon_theme});
    run_command_sync({"gsettings", "set", "org.gnome.desktop.interface", "color-scheme", scheme});
}

void ThemeBackend::sync_symlinks() {
    const char* home = getenv("HOME");
    if (!home) return;
    fs::path home_path(home);

    std::error_code ec;
    // GTK 3 & 4
    fs::create_directories(home_path / ".config/gtk-3.0");
    fs::create_directories(home_path / ".config/gtk-4.0");
    fs::remove(home_path / ".config/gtk-3.0/colors.css", ec);
    fs::create_symlink(current_dir_ / "gtk.css", home_path / ".config/gtk-3.0/colors.css", ec);
    fs::remove(home_path / ".config/gtk-4.0/colors.css", ec);
    fs::create_symlink(current_dir_ / "gtk.css", home_path / ".config/gtk-4.0/colors.css", ec);

    // Dunst
    fs::create_directories(home_path / ".config/dunst/dunstrc.d");
    fs::remove(home_path / ".config/dunst/dunstrc.d/theme.conf", ec);
    fs::create_symlink(current_dir_ / "dunst.conf", home_path / ".config/dunst/dunstrc.d/theme.conf", ec);

    // Yazi
    fs::create_directories(home_path / ".config/yazi");
    fs::remove(home_path / ".config/yazi/theme.toml", ec);
    fs::create_symlink(current_dir_ / "yazi.toml", home_path / ".config/yazi/theme.toml", ec);

    // miqutoolkit
    fs::create_directories(home_path / ".config/miqutoolkit");
    fs::copy_file(current_dir_ / "miqutoolkit.conf", home_path / ".config/miqutoolkit/miqutoolkit.conf", fs::copy_options::overwrite_existing, ec);
}

void ThemeBackend::reload_daemons() {
    run_command_async({"killall", "-SIGUSR1", "kitty"});
    run_command_async({"dunstctl", "reload"});
}

void ThemeBackend::notify_theme_changed(const std::string& title, const std::string& message) {
    run_command_async({"dunstify", "-t", "2000", "-i", "preferences-desktop-theme", title, message});
}

void ThemeBackend::apply_scheme(const std::string& scheme_name, const std::string& matugen_type) {
    ThemeMode mode = get_current_mode();
    std::string m_str = (mode == ThemeMode::Light) ? "light" : "dark";

    if (mode == ThemeMode::Light) {
        write_file_string(config_dir_ / "current_light", scheme_name);
    } else {
        write_file_string(config_dir_ / "current_dark", scheme_name);
    }

    if (!matugen_type.empty()) {
        write_file_string(config_dir_ / "matugen_type", matugen_type);
    }

    if (scheme_name == "dynamic-matugen") {
        std::string wp = get_active_wallpaper(mode);
        std::string mtype = matugen_type.empty() ? get_matugen_type() : matugen_type;
        regenerate_matugen(mode, wp, mtype);
    }

    fs::path s_dir = schemes_dir_ / m_str / scheme_name;
    if (fs::exists(s_dir)) {
        std::error_code ec;
        for (const auto& item : fs::directory_iterator(s_dir)) {
            if (item.is_regular_file()) {
                fs::copy_file(item.path(), current_dir_ / item.path().filename(), fs::copy_options::overwrite_existing, ec);
            }
        }
    }

    // Apply current transparency setting
    apply_transparency(get_transparency());

    // Update symlinks
    sync_symlinks();

    // Update GTK & IDE
    update_gtk_theme(mode);
    update_ide_theme(mode, (current_dir_ / "miqutoolkit.conf").string());

    // Reload other daemons
    reload_daemons();

    notify_theme_changed("Theme Applied", "Active Scheme: " + format_display_name(scheme_name));

    if (on_theme_changed_) {
        on_theme_changed_();
    }
}

void ThemeBackend::set_on_theme_changed(std::function<void()> cb) {
    on_theme_changed_ = std::move(cb);
}

} // namespace miqutheme
