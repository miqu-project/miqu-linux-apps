#include "overview_config.hpp"

#include <sys/inotify.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <cctype>
#include <chrono>
#include <ctime>

namespace miquoverview {

namespace fs = std::filesystem;

static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n\"'");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n\"'");
    return str.substr(first, (last - first + 1));
}

Color Color::from_hex(const std::string& hex_str, float default_alpha) {
    Color c{0.0f, 0.0f, 0.0f, default_alpha};
    std::string s = hex_str;
    if (!s.empty() && s[0] == '#') {
        s = s.substr(1);
    }

    if (s.length() == 3) {
        char buf[7] = {s[0], s[0], s[1], s[1], s[2], s[2], '\0'};
        s = buf;
    } else if (s.length() == 4) {
        char buf[9] = {s[0], s[0], s[1], s[1], s[2], s[2], s[3], s[3], '\0'};
        s = buf;
    }

    if (s.length() == 6) {
        unsigned int val = 0;
        std::stringstream ss;
        ss << std::hex << s;
        if (ss >> val) {
            c.r = ((val >> 16) & 0xFF) / 255.0f;
            c.g = ((val >> 8) & 0xFF) / 255.0f;
            c.b = (val & 0xFF) / 255.0f;
            c.a = default_alpha;
        }
    } else if (s.length() == 8) {
        unsigned long long val = 0;
        std::stringstream ss;
        ss << std::hex << s;
        if (ss >> val) {
            c.r = ((val >> 24) & 0xFF) / 255.0f;
            c.g = ((val >> 16) & 0xFF) / 255.0f;
            c.b = ((val >> 8) & 0xFF) / 255.0f;
            c.a = (val & 0xFF) / 255.0f;
        }
    }
    return c;
}

OverviewConfig::OverviewConfig() {
    set_defaults();
}

OverviewConfig::~OverviewConfig() {
    cleanup_watcher();
}

void OverviewConfig::set_defaults() {
    m_edge_margin = 48;
    m_corner_radius = 12;
    m_font = "Sans Bold 10";

    m_animation = true;
    m_duration = 200;
    m_blur = true;
    m_blur_alpha = 1.0f;
    m_shadow = true;
    m_shadow_radius = 16;
    m_shadow_blur = 18.0f;
    m_shadow_color = Color::from_hex("#000000aa");

    // Miqu Ecosystem baseline dark palette
    m_theme.background         = Color::from_hex("#11131cb0");
    m_theme.backdrop           = Color::from_hex("#00000080");
    m_theme.surface            = Color::from_hex("#151924f5");
    m_theme.surface_variant    = Color::from_hex("#1f2537f0");
    m_theme.on_surface         = Color::from_hex("#e6ecf8");
    m_theme.on_surface_variant = Color::from_hex("#8a99b5");
    m_theme.primary            = Color::from_hex("#40a0ff");
    m_theme.on_primary         = Color::from_hex("#ffffff");
    m_theme.outline            = Color::from_hex("#2e384d80");
    m_theme.outline_variant    = Color::from_hex("#4070b099");
}

std::string OverviewConfig::expand_home(const std::string& path) {
    if (path.empty()) return path;
    if (path[0] == '~') {
        const char* home = getenv("HOME");
        if (home) {
            return std::string(home) + path.substr(1);
        }
    }
    return path;
}

std::string OverviewConfig::resolve_config_path() {
    // 1. Tier 2: User Standalone
    std::string user_conf = expand_home("~/.config/miquoverview/miquoverview.conf");
    if (fs::exists(user_conf)) {
        return user_conf;
    }

    // 2. Tier 1: System Baseline
    std::string sys_conf = "/usr/share/miquoverview/miquoverview.conf";
    if (fs::exists(sys_conf)) {
        return sys_conf;
    }

    return "";
}

std::string OverviewConfig::init_user_config() {
    std::string user_dir = expand_home("~/.config/miquoverview");
    std::string config_path = user_dir + "/miquoverview.conf";
    std::error_code ec;

    // 1. Create ~/.config/miquoverview/ directory if not present
    fs::create_directories(user_dir, ec);

    // 2. Safe Archiving: If configuration already exists, archive to .bak.<YYYYMMDD-HHMMSS>
    if (fs::exists(config_path)) {
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf{};
        localtime_r(&now_time, &tm_buf);
        char ts[32];
        std::strftime(ts, sizeof(ts), "%Y%m%d-%H%M%S", &tm_buf);

        std::string bak_path = config_path + ".bak." + ts;
        fs::rename(config_path, bak_path, ec);
        if (!ec) {
            std::cout << "[miquoverview] Backed up existing configuration to: " << bak_path << "\n";
        }
    }

    // 3. Search for template candidate
    std::vector<std::string> search_candidates = {
        "/usr/share/miquoverview/miquoverview.conf",
        "/usr/local/share/miquoverview/miquoverview.conf",
        "assets/miquoverview.conf",
        "../assets/miquoverview.conf"
    };

    bool copied = false;
    for (const auto& cand : search_candidates) {
        if (fs::exists(cand)) {
            fs::copy_file(cand, config_path, fs::copy_options::overwrite_existing, ec);
            if (!ec) {
                copied = true;
                std::cout << "[miquoverview] Initialized default configuration: copied " << cand << " to " << config_path << "\n";
                break;
            }
        }
    }

    if (!copied) {
        // Fallback: write built-in default template directly
        std::ofstream out(config_path);
        if (out.is_open()) {
            out << "# ==============================================================================\n"
                << "# miquoverview Configuration\n"
                << "# GPU-accelerated workspace expose and overview plugin for miquland\n"
                << "# ==============================================================================\n\n"
                << "# 1. Dimensions & Metrics\n"
                << "edge_margin   = 48\n"
                << "corner_radius = 12\n"
                << "font          = Sans Bold 10\n\n"
                << "# 2. Baseline Fallback Theme\n"
                << "background          = #11131cfa\n"
                << "surface             = #151924f5\n"
                << "surface_variant     = #1f2537f0\n"
                << "on_surface          = #e6ecf8\n"
                << "on_surface_variant  = #8a99b5\n"
                << "primary             = #40a0ff\n"
                << "on_primary          = #ffffff\n"
                << "outline             = #2e384d80\n"
                << "outline_variant     = #4070b099\n\n"
                << "# 3. Dynamic Theme Integration (Optional)\n"
                << "# source = ~/.config/miqutheme/current/miqutoolkit.conf\n";
            std::cout << "[miquoverview] Initialized built-in configuration template at: " << config_path << "\n";
            copied = true;
        }
    }

    return config_path;
}

void OverviewConfig::load() {
    set_defaults();
    m_watched_files.clear();

    m_loaded_file = resolve_config_path();
    if (!m_loaded_file.empty()) {
        load_file(m_loaded_file, 0);
    }
}

void OverviewConfig::load_file(const std::string& raw_path, int depth) {
    if (depth > 5) {
        std::cerr << "[miquoverview] Max source nesting depth reached: " << raw_path << "\n";
        return;
    }

    std::string path = expand_home(raw_path);
    if (!fs::exists(path)) {
        return;
    }

    m_watched_files.push_back(path);

    std::ifstream file(path);
    if (!file.is_open()) {
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty()) continue;

        // Skip full-line comments
        if (line[0] == '#' || line.rfind("//", 0) == 0 || line[0] == ';') {
            continue;
        }

        auto eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue;

        std::string key = trim(line.substr(0, eq_pos));
        std::string val = trim(line.substr(eq_pos + 1));
        if (key.empty() || val.empty()) continue;

        // Strip inline comments from val:
        // If val starts with '#', it's a hex color (e.g. #fff7fc7D), so an inline comment
        // can only start at a '#' that is preceded by whitespace (e.g. #fff7fc7D # comment)
        size_t c_pos = std::string::npos;
        if (val[0] == '#') {
            size_t space_pos = val.find_first_of(" \t");
            if (space_pos != std::string::npos) {
                c_pos = val.find('#', space_pos);
            }
        } else {
            c_pos = val.find('#');
        }

        size_t slash_pos = val.find("//");
        if (slash_pos != std::string::npos && (c_pos == std::string::npos || slash_pos < c_pos)) {
            c_pos = slash_pos;
        }

        if (c_pos != std::string::npos) {
            val = trim(val.substr(0, c_pos));
        }

        if (val.empty()) continue;

        apply_key_value(key, val, depth);
    }
}

void OverviewConfig::apply_key_value(const std::string& key, const std::string& val, int depth) {
    auto parse_bool = [](const std::string& s, bool default_val) -> bool {
        std::string lower = s;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower == "true" || lower == "1" || lower == "yes" || lower == "on") return true;
        if (lower == "false" || lower == "0" || lower == "no" || lower == "off") return false;
        return default_val;
    };

    if (key == "source") {
        load_file(val, depth + 1);
    } else if (key == "background") {
        m_theme.background = Color::from_hex(val);
    } else if (key == "backdrop") {
        m_theme.backdrop = Color::from_hex(val);
    } else if (key == "surface") {
        m_theme.surface = Color::from_hex(val);
    } else if (key == "surface_variant") {
        m_theme.surface_variant = Color::from_hex(val);
    } else if (key == "on_surface") {
        m_theme.on_surface = Color::from_hex(val);
    } else if (key == "on_surface_variant") {
        m_theme.on_surface_variant = Color::from_hex(val);
    } else if (key == "primary" || key == "accent") {
        m_theme.primary = Color::from_hex(val);
    } else if (key == "on_primary" || key == "on_accent") {
        m_theme.on_primary = Color::from_hex(val);
    } else if (key == "outline") {
        m_theme.outline = Color::from_hex(val);
    } else if (key == "outline_variant") {
        m_theme.outline_variant = Color::from_hex(val);
    } else if (key == "edge_margin" || key == "margin") {
        try { m_edge_margin = std::clamp(std::stoi(val), 0, 400); } catch (...) {}
    } else if (key == "corner_radius") {
        try { m_corner_radius = std::clamp(std::stoi(val), 0, 32); } catch (...) {}
    } else if (key == "font") {
        m_font = val;
    } else if (key == "animation") {
        m_animation = parse_bool(val, true);
    } else if (key == "duration") {
        try { m_duration = std::clamp(std::stoi(val), 20, 1000); } catch (...) {}
    } else if (key == "blur") {
        m_blur = parse_bool(val, true);
    } else if (key == "blur_alpha") {
        try { m_blur_alpha = std::clamp(std::stof(val), 0.0f, 1.0f); } catch (...) {}
    } else if (key == "shadow") {
        m_shadow = parse_bool(val, true);
    } else if (key == "shadow_radius") {
        try { m_shadow_radius = std::clamp(std::stoi(val), 0, 64); } catch (...) {}
    } else if (key == "shadow_blur") {
        try { m_shadow_blur = std::clamp(std::stof(val), 0.0f, 64.0f); } catch (...) {}
    } else if (key == "shadow_color") {
        m_shadow_color = Color::from_hex(val);
    }
}

void OverviewConfig::setup_watcher(struct wl_event_loop* loop) {
    if (loop) m_loop = loop;
    if (!m_loop) return;

    cleanup_watcher();

    m_inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (m_inotify_fd < 0) {
        std::cerr << "[miquoverview] Failed to initialize inotify\n";
        return;
    }

    m_wd_to_targets.clear();

    std::string user_dir = expand_home("~/.config/miquoverview");
    std::string config_root = expand_home("~/.config");

    // 1. If user app config directory doesn't exist yet, watch parent ~/.config for creation of "miquoverview"
    if (!fs::exists(user_dir)) {
        if (fs::exists(config_root)) {
            int wd = inotify_add_watch(m_inotify_fd, config_root.c_str(), IN_CREATE | IN_MOVED_TO);
            if (wd >= 0) {
                m_wd_to_targets[wd].push_back("miquoverview");
            }
        }
    } else {
        // Watch ~/.config/miquoverview directory for miquoverview.conf
        int wd = inotify_add_watch(m_inotify_fd, user_dir.c_str(), IN_CLOSE_WRITE | IN_MOVED_TO | IN_CREATE | IN_MODIFY);
        if (wd >= 0) {
            m_wd_to_targets[wd].push_back("miquoverview.conf");
        }
    }

    // 2. System app config directory /usr/share/miquoverview (only if user config doesn't exist)
    if (!fs::exists(user_dir + "/miquoverview.conf") && fs::exists("/usr/share/miquoverview")) {
        int wd = inotify_add_watch(m_inotify_fd, "/usr/share/miquoverview", IN_CLOSE_WRITE | IN_MOVED_TO | IN_CREATE | IN_MODIFY);
        if (wd >= 0) {
            m_wd_to_targets[wd].push_back("miquoverview.conf");
        }
    }

    // 3. Sourced files (e.g. ~/.config/miqutheme/current/miqutoolkit.conf)
    for (const auto& file_path : m_watched_files) {
        fs::path p(file_path);
        fs::path parent = p.parent_path();
        if (fs::exists(parent)) {
            int wd = inotify_add_watch(m_inotify_fd, parent.c_str(), IN_CLOSE_WRITE | IN_MOVED_TO | IN_CREATE | IN_MODIFY);
            if (wd >= 0) {
                m_wd_to_targets[wd].push_back(p.filename().string());
            }
            if (parent.has_parent_path() && fs::exists(parent.parent_path())) {
                int gwd = inotify_add_watch(m_inotify_fd, parent.parent_path().c_str(), IN_CLOSE_WRITE | IN_MOVED_TO | IN_CREATE | IN_MODIFY);
                if (gwd >= 0) {
                    m_wd_to_targets[gwd].push_back(parent.filename().string());
                }
            }
        }
    }

    // Debounce timer: wait 250ms for rapid file writes to settle
    m_debounce_timer = wl_event_loop_add_timer(m_loop,
        [](void* data) -> int {
            auto* self = static_cast<OverviewConfig*>(data);
            if (!self) return 0;
            self->load();
            self->setup_watcher();
            if (self->m_reload_cb) {
                self->m_reload_cb();
            }
            return 0;
        }, this);

    m_event_source = wl_event_loop_add_fd(m_loop, m_inotify_fd, WL_EVENT_READABLE,
        [](int fd, uint32_t /*mask*/, void* data) -> int {
            auto* self = static_cast<OverviewConfig*>(data);
            if (!self) return 0;

            alignas(struct inotify_event) char buf[4096];
            ssize_t len;
            bool should_reload = false;

            while ((len = read(fd, buf, sizeof(buf))) > 0) {
                for (char* ptr = buf; ptr < buf + len; ) {
                    auto* event = reinterpret_cast<const struct inotify_event*>(ptr);
                    auto it = self->m_wd_to_targets.find(event->wd);
                    if (it != self->m_wd_to_targets.end()) {
                        if (event->len > 0) {
                            std::string ev_name(event->name);
                            for (const auto& target : it->second) {
                                if (target == ev_name || (target.empty() && ev_name.ends_with(".conf"))) {
                                    should_reload = true;
                                    break;
                                }
                            }
                        } else {
                            should_reload = true;
                        }
                    }
                    ptr += sizeof(struct inotify_event) + event->len;
                }
            }

            if (should_reload && self->m_debounce_timer) {
                wl_event_source_timer_update(self->m_debounce_timer, 250);
            }
            return 0;
        }, this);
}

void OverviewConfig::cleanup_watcher() {
    if (m_debounce_timer) {
        wl_event_source_remove(m_debounce_timer);
        m_debounce_timer = nullptr;
    }
    if (m_event_source) {
        wl_event_source_remove(m_event_source);
        m_event_source = nullptr;
    }
    if (m_inotify_fd >= 0) {
        close(m_inotify_fd);
        m_inotify_fd = -1;
    }
    m_wd_to_targets.clear();
}

} // namespace miquoverview
