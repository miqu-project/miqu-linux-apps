#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <filesystem>

namespace miqutheme {

enum class ThemeMode {
    Dark,
    Light
};

struct SchemeInfo {
    std::string name;          // e.g. "dynamic-matugen", "catppuccin-mocha"
    std::string display_name;  // e.g. "Material You (Dynamic)", "Catppuccin Mocha"
    bool is_dynamic = false;
    std::string background = "#1E1E2E";
    std::string surface = "#313244";
    std::string primary = "#89B4FA";
    std::vector<std::string> swatches;
};

struct WallpaperInfo {
    std::string filename;
    std::string full_path;
    ThemeMode mode = ThemeMode::Dark;
};

class ThemeBackend {
public:
    ThemeBackend();
    ~ThemeBackend() = default;

    // Initialization & Seeding
    void ensure_directories_and_seed();

    // Mode
    ThemeMode get_current_mode() const;
    void set_mode(ThemeMode mode);

    // Schemes
    std::string get_current_scheme(ThemeMode mode) const;
    std::vector<SchemeInfo> get_available_schemes(ThemeMode mode);
    SchemeInfo get_dynamic_scheme_info(ThemeMode mode);
    void apply_scheme(const std::string& scheme_name, const std::string& matugen_type = "");

    // Matugen Variants
    std::string get_matugen_type() const;
    void set_matugen_type(const std::string& type);
    static const std::vector<std::pair<std::string, std::string>>& get_matugen_variants();

    // Wallpapers
    std::string get_active_wallpaper(ThemeMode mode) const;
    std::string get_wallpaper_fit() const;
    std::vector<WallpaperInfo> get_wallpapers(ThemeMode mode);
    void apply_wallpaper(const std::string& path, const std::string& fit = "fill");

    // Transparency
    int get_transparency() const; // 0 to 100
    void apply_transparency(int percentage, bool notify_listeners = true);

    // Notification / Callback
    void set_on_theme_changed(std::function<void()> cb);

private:
    std::filesystem::path config_dir_;
    std::filesystem::path current_dir_;
    std::filesystem::path schemes_dir_;
    std::filesystem::path wallpapers_dir_;
    std::filesystem::path matugen_dir_;

    std::function<void()> on_theme_changed_;

    void seed_default_assets_if_needed();
    void sync_symlinks();
    void regenerate_matugen(ThemeMode mode, const std::string& wp_path, const std::string& matugen_type);
    void update_ide_theme(ThemeMode mode, const std::string& miqutoolkit_conf);
    void update_gtk_theme(ThemeMode mode);
    void reload_daemons();
    void notify_theme_changed(const std::string& title, const std::string& message);

    static SchemeInfo parse_scheme_file(const std::filesystem::path& conf_path, const std::string& name);
    static std::string format_display_name(const std::string& raw_name);
};

} // namespace miqutheme
