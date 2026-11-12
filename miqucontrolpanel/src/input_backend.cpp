#include "input_backend.hpp"
#include "apps_backend.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <mutex>
#include <regex>

namespace fs = std::filesystem;

namespace miqu {

namespace {

static std::mutex s_input_mutex;
static bool s_baseline_loaded = false;

// In-memory state overrides (reboot persistence deferred)
static std::string s_active_layout = "us";
static int s_repeat_rate = 25;
static int s_repeat_delay = 600;
static bool s_tap_to_click = true;
static bool s_natural_scroll = false;
static bool s_disable_while_typing = true;
static float s_pointer_speed = 0.5f;
static std::string s_accel_profile = "adaptive";
static bool s_workspace_swipe = true;
static float s_gesture_multiplier = 1.0f;

std::string get_miquland_config_path() {
    const char* home = getenv("HOME");
    if (home) {
        std::string user_conf = std::string(home) + "/.config/miquland/miquland.conf";
        if (fs::exists(user_conf)) return user_conf;
    }
    if (fs::exists("/usr/share/miquland/miquland.conf")) {
        return "/usr/share/miquland/miquland.conf";
    }
    return "";
}

void load_baseline_config() {
    if (s_baseline_loaded) return;
    s_baseline_loaded = true;

    std::string path = get_miquland_config_path();
    if (path.empty()) return;

    std::ifstream file(path);
    if (!file.is_open()) return;

    std::string line;
    std::string current_section;

    while (std::getline(file, line)) {
        size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos || line[first] == '#') continue;

        std::string trimmed = line.substr(first);
        size_t brace_open = trimmed.find('{');
        size_t brace_close = trimmed.find('}');

        if (brace_open != std::string::npos) {
            std::string sec_name = trimmed.substr(0, brace_open);
            size_t s = sec_name.find_first_not_of(" \t");
            size_t e = sec_name.find_last_not_of(" \t");
            if (s != std::string::npos && e != std::string::npos) {
                current_section = sec_name.substr(s, e - s + 1);
            }
            continue;
        }

        if (brace_close != std::string::npos) {
            current_section = "";
            continue;
        }

        size_t eq = trimmed.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trimmed.substr(0, eq);
        std::string val = trimmed.substr(eq + 1);

        auto trim_str = [](std::string& s) {
            size_t start = s.find_first_not_of(" \t\r\n");
            size_t end = s.find_last_not_of(" \t\r\n");
            s = (start != std::string::npos && end != std::string::npos) ? s.substr(start, end - start + 1) : "";
        };
        trim_str(key);
        trim_str(val);

        if (key == "layout" && !val.empty()) s_active_layout = val;
        else if (key == "repeat_rate") {
            try { s_repeat_rate = std::stoi(val); } catch (...) {}
        } else if (key == "repeat_delay") {
            try { s_repeat_delay = std::stoi(val); } catch (...) {}
        } else if (key == "tap_to_click") {
            s_tap_to_click = (val == "true" || val == "1");
        } else if (key == "natural_scroll") {
            s_natural_scroll = (val == "true" || val == "1");
        } else if (key == "disable_while_typing") {
            s_disable_while_typing = (val == "true" || val == "1");
        } else if (key == "accel_profile" && !val.empty()) {
            s_accel_profile = val;
        } else if (key == "accel_speed") {
            try {
                float raw = std::stof(val); // -1.0 to 1.0
                s_pointer_speed = std::clamp((raw + 1.0f) * 0.5f, 0.0f, 1.0f);
            } catch (...) {}
        } else if (key == "touchpad_multiplier") {
            try { s_gesture_multiplier = std::stof(val); } catch (...) {}
        }
    }
}

bool is_auxiliary_keyboard(const std::string& name) {
    std::string lower = name;
    for (char& c : lower) c = static_cast<char>(std::tolower(c));
    return (lower.find("power button") != std::string::npos ||
            lower.find("sleep button") != std::string::npos ||
            lower.find("video bus") != std::string::npos ||
            lower.find("intel hid") != std::string::npos ||
            lower.find("dell wmi") != std::string::npos);
}

} // namespace

InputConfigState InputBackend::get_state() {
    std::lock_guard<std::mutex> lock(s_input_mutex);
    load_baseline_config();

    InputConfigState state;
    state.active_layout = s_active_layout;
    state.repeat_rate = s_repeat_rate;
    state.repeat_delay = s_repeat_delay;
    state.tap_to_click = s_tap_to_click;
    state.natural_scroll = s_natural_scroll;
    state.disable_while_typing = s_disable_while_typing;
    state.pointer_speed = s_pointer_speed;
    state.accel_profile = s_accel_profile;
    state.workspace_swipe = s_workspace_swipe;
    state.gesture_multiplier = s_gesture_multiplier;

    // Introspect live devices via miquland IPC
    std::string json = AppsBackend::run_cmd("miquctl -j devices 2>/dev/null");
    if (json.empty()) {
        // Fallback placeholder
        KeyboardDevice def_kb;
        def_kb.name = "Standard PS/2 Keyboard";
        def_kb.layout = "English (US)";
        def_kb.repeat_rate = s_repeat_rate;
        def_kb.repeat_delay = s_repeat_delay;
        def_kb.is_primary = true;
        state.keyboards.push_back(def_kb);

        PointerDevice def_ptr;
        def_ptr.name = "Precision Touchpad";
        def_ptr.is_touchpad = true;
        def_ptr.display_type = "Precision Touchpad";
        state.pointers.push_back(def_ptr);
        return state;
    }

    size_t kb_pos = json.find("\"keyboards\"");
    size_t ptr_pos = json.find("\"pointers\"");
    size_t touch_pos = json.find("\"touch\"");

    // 1. Keyboards
    if (kb_pos != std::string::npos) {
        std::string kb_section = json.substr(kb_pos, (ptr_pos != std::string::npos ? ptr_pos - kb_pos : std::string::npos));
        std::regex obj_regex(R"(\{[^{}]+\})");
        auto begin = std::sregex_iterator(kb_section.begin(), kb_section.end(), obj_regex);
        auto end = std::sregex_iterator();

        for (auto it = begin; it != end; ++it) {
            std::string obj = it->str();
            KeyboardDevice kb;

            std::smatch m;
            std::regex name_re("\"name\"\\s*:\\s*\"([^\"]+)\"");
            if (std::regex_search(obj, m, name_re) && m.size() > 1) kb.name = m[1].str();

            std::regex layout_re("\"layout\"\\s*:\\s*\"([^\"]+)\"");
            if (std::regex_search(obj, m, layout_re) && m.size() > 1) kb.layout = m[1].str();

            std::regex rate_re("\"repeat_rate\"\\s*:\\s*([0-9]+)");
            if (std::regex_search(obj, m, rate_re) && m.size() > 1) {
                try { kb.repeat_rate = std::stoi(m[1].str()); } catch (...) {}
            }

            std::regex delay_re("\"repeat_delay\"\\s*:\\s*([0-9]+)");
            if (std::regex_search(obj, m, delay_re) && m.size() > 1) {
                try { kb.repeat_delay = std::stoi(m[1].str()); } catch (...) {}
            }

            if (!kb.name.empty()) {
                kb.is_primary = !is_auxiliary_keyboard(kb.name);
                state.keyboards.push_back(kb);
            }
        }
    }

    // Ensure at least one primary keyboard is marked
    bool has_primary = false;
    for (const auto& kb : state.keyboards) {
        if (kb.is_primary) { has_primary = true; break; }
    }
    if (!has_primary && !state.keyboards.empty()) {
        state.keyboards[0].is_primary = true;
    }

    // 2. Pointers
    if (ptr_pos != std::string::npos) {
        std::string ptr_section = json.substr(ptr_pos, (touch_pos != std::string::npos ? touch_pos - ptr_pos : std::string::npos));
        std::regex obj_regex(R"(\{[^{}]+\})");
        auto begin = std::sregex_iterator(ptr_section.begin(), ptr_section.end(), obj_regex);
        auto end = std::sregex_iterator();

        for (auto it = begin; it != end; ++it) {
            std::string obj = it->str();
            PointerDevice ptr;
            std::smatch m;
            std::regex name_re("\"name\"\\s*:\\s*\"([^\"]+)\"");
            if (std::regex_search(obj, m, name_re) && m.size() > 1) ptr.name = m[1].str();

            if (!ptr.name.empty()) {
                std::string lower = ptr.name;
                for (char& c : lower) c = static_cast<char>(std::tolower(c));

                if (lower.find("touchpad") != std::string::npos || lower.find("044e:120b") != std::string::npos || lower.find("synaptics") != std::string::npos) {
                    ptr.is_touchpad = true;
                    ptr.display_type = "Precision Touchpad";
                } else if (lower.find("stick") != std::string::npos || lower.find("trackpoint") != std::string::npos || lower.find("dualpoint") != std::string::npos) {
                    ptr.is_trackpoint = true;
                    ptr.display_type = "Pointing Stick (TrackPoint)";
                } else {
                    ptr.display_type = "Mouse / Optical Pointer";
                }
                state.pointers.push_back(ptr);
            }
        }
    }

    // 3. Touch
    if (touch_pos != std::string::npos) {
        std::string touch_section = json.substr(touch_pos);
        std::regex obj_regex(R"(\{[^{}]+\})");
        auto begin = std::sregex_iterator(touch_section.begin(), touch_section.end(), obj_regex);
        auto end = std::sregex_iterator();

        for (auto it = begin; it != end; ++it) {
            std::string obj = it->str();
            TouchDevice tch;
            std::smatch m;
            std::regex name_re("\"name\"\\s*:\\s*\"([^\"]+)\"");
            if (std::regex_search(obj, m, name_re) && m.size() > 1) tch.name = m[1].str();

            if (!tch.name.empty()) {
                tch.mapped_output = "eDP-1";
                state.touch_devices.push_back(tch);
            }
        }
    }

    return state;
}

void InputBackend::set_keyboard_layout(const std::string& layout) {
    std::lock_guard<std::mutex> lock(s_input_mutex);
    s_active_layout = layout;
}

void InputBackend::set_repeat_info(int rate, int delay) {
    std::lock_guard<std::mutex> lock(s_input_mutex);
    s_repeat_rate = std::clamp(rate, 5, 60);
    s_repeat_delay = std::clamp(delay, 100, 1500);
}

void InputBackend::set_touchpad_options(bool tap_to_click, bool natural_scroll, bool disable_while_typing) {
    std::lock_guard<std::mutex> lock(s_input_mutex);
    s_tap_to_click = tap_to_click;
    s_natural_scroll = natural_scroll;
    s_disable_while_typing = disable_while_typing;
}

void InputBackend::set_pointer_speed(float speed) {
    std::lock_guard<std::mutex> lock(s_input_mutex);
    s_pointer_speed = std::clamp(speed, 0.0f, 1.0f);
}

void InputBackend::set_accel_profile(const std::string& profile) {
    std::lock_guard<std::mutex> lock(s_input_mutex);
    s_accel_profile = profile;
}

void InputBackend::set_workspace_swipe(bool enabled, float multiplier) {
    std::lock_guard<std::mutex> lock(s_input_mutex);
    s_workspace_swipe = enabled;
    s_gesture_multiplier = std::clamp(multiplier, 0.25f, 3.0f);
}

} // namespace miqu
