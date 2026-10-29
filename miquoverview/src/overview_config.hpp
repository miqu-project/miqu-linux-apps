#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <filesystem>
#include <wayland-server-core.h>

namespace miquoverview {

struct Color {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;

    static Color from_hex(const std::string& hex_str, float default_alpha = 1.0f);
};

struct OverviewTheme {
    Color background;
    Color backdrop;
    Color surface;
    Color surface_variant;
    Color on_surface;
    Color on_surface_variant;
    Color primary;
    Color on_primary;
    Color outline;
    Color outline_variant;
};

class OverviewConfig {
public:
    static OverviewConfig& get() {
        static OverviewConfig s_instance;
        return s_instance;
    }

    static std::string init_user_config();

    using ReloadCallback = std::function<void()>;
    void set_reload_callback(ReloadCallback cb) { m_reload_cb = std::move(cb); }

    void load();
    void setup_watcher(struct wl_event_loop* loop = nullptr);
    void cleanup_watcher();

    const OverviewTheme& theme() const { return m_theme; }
    int edge_margin() const { return m_edge_margin; }
    int corner_radius() const { return m_corner_radius; }
    const std::string& font() const { return m_font; }

    bool animation() const { return m_animation; }
    int duration() const { return m_duration; }
    bool blur() const { return m_blur; }
    float blur_alpha() const { return m_blur_alpha; }
    bool shadow() const { return m_shadow; }
    int shadow_radius() const { return m_shadow_radius; }
    float shadow_blur() const { return m_shadow_blur; }
    const Color& shadow_color() const { return m_shadow_color; }

private:
    OverviewConfig();
    ~OverviewConfig();

    void set_defaults();
    void load_file(const std::string& path, int depth = 0);
    void apply_key_value(const std::string& key, const std::string& value, int depth);
    std::string resolve_config_path();
    static std::string expand_home(const std::string& path);

    OverviewTheme m_theme;
    int m_edge_margin = 48;
    int m_corner_radius = 12;
    std::string m_font = "Sans Bold 10";

    bool m_animation = true;
    int m_duration = 200;
    bool m_blur = true;
    float m_blur_alpha = 1.0f;
    bool m_shadow = true;
    int m_shadow_radius = 16;
    float m_shadow_blur = 18.0f;
    Color m_shadow_color;

    std::string m_loaded_file;
    std::vector<std::string> m_watched_files;

    int m_inotify_fd = -1;
    struct wl_event_source* m_event_source = nullptr;
    struct wl_event_source* m_debounce_timer = nullptr;
    struct wl_event_loop* m_loop = nullptr;
    ReloadCallback m_reload_cb;
    std::unordered_map<int, std::vector<std::string>> m_wd_to_targets;
};

} // namespace miquoverview
