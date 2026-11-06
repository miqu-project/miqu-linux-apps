#include "modules/windowmanager/wm_backend.hpp"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <regex>

namespace fs = std::filesystem;

namespace miqu {

struct PipeCloser {
    void operator()(FILE* fp) const {
        if (fp) pclose(fp);
    }
};

std::string WmBackend::run_miquctl(const std::string& args) {
    std::string result;
    std::string cmd = "miquctl " + args;
    std::unique_ptr<FILE, PipeCloser> pipe(popen(cmd.c_str(), "r"));
    if (!pipe) return result;

    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe.get())) {
        result += buffer;
    }
    return result;
}

static std::string extract_json_string(const std::string& json, const std::string& key) {
    std::string pattern = "\"" + key + "\"\\s*:\\s*\"([^\"]+)\"";
    std::regex re(pattern);
    std::smatch m;
    if (std::regex_search(json, m, re) && m.size() > 1) {
        return m[1].str();
    }
    return "";
}

static double extract_json_number(const std::string& json, const std::string& key, double def = 0.0) {
    std::string pattern = "\"" + key + "\"\\s*:\\s*([0-9.]+)";
    std::regex re(pattern);
    std::smatch m;
    if (std::regex_search(json, m, re) && m.size() > 1) {
        try {
            return std::stod(m[1].str());
        } catch (...) {}
    }
    return def;
}

static bool extract_json_bool(const std::string& json, const std::string& key, bool def = false) {
    std::string pattern = "\"" + key + "\"\\s*:\\s*(true|false)";
    std::regex re(pattern);
    std::smatch m;
    if (std::regex_search(json, m, re) && m.size() > 1) {
        return m[1].str() == "true";
    }
    return def;
}

WmOptions WmBackend::get_options() {
    WmOptions opts;
    std::string json = run_miquctl("-j getoption all");
    if (json.empty() || json.front() != '{') {
        return opts;
    }

    std::string l = extract_json_string(json, "layout");
    if (!l.empty()) opts.layout = l;

    opts.gaps_in = static_cast<int>(extract_json_number(json, "gaps_in", opts.gaps_in));
    opts.gaps_out = static_cast<int>(extract_json_number(json, "gaps_out", opts.gaps_out));
    opts.border_width = static_cast<int>(extract_json_number(json, "border_width", opts.border_width));
    opts.border_radius = static_cast<int>(extract_json_number(json, "border_radius", opts.border_radius));
    opts.opacity_active = static_cast<float>(extract_json_number(json, "opacity_active", opts.opacity_active));
    opts.opacity_inactive = static_cast<float>(extract_json_number(json, "opacity_inactive", opts.opacity_inactive));
    opts.animations = extract_json_bool(json, "animations", opts.animations);
    opts.blur = extract_json_bool(json, "blur", opts.blur);
    opts.smart_gaps = extract_json_bool(json, "smart_gaps", opts.smart_gaps);

    return opts;
}

void WmBackend::update_config_file(const std::string& key, const std::string& value) {
    const char* home = std::getenv("HOME");
    if (!home) return;

    fs::path conf_dir = fs::path(home) / ".config" / "miquland";
    fs::path conf_file = conf_dir / "miquland.conf";

    if (!fs::exists(conf_file)) {
        fs::create_directories(conf_dir);
        if (fs::exists("/usr/share/miquland/miquland.conf")) {
            std::error_code ec;
            fs::copy_file("/usr/share/miquland/miquland.conf", conf_file, ec);
        }
    }

    if (!fs::exists(conf_file)) return;

    std::ifstream in(conf_file);
    if (!in.is_open()) return;

    std::vector<std::string> lines;
    std::string line;
    bool replaced = false;

    // Pattern: ^\s*key\s*=
    std::regex key_regex("^\\s*" + key + "\\s*=.*");

    while (std::getline(in, line)) {
        if (!replaced && std::regex_match(line, key_regex)) {
            // Keep leading whitespace indentation
            size_t first_non_space = line.find_first_not_of(" \t");
            std::string indent = (first_non_space != std::string::npos) ? line.substr(0, first_non_space) : "";
            lines.push_back(indent + key + " = " + value);
            replaced = true;
        } else {
            lines.push_back(line);
        }
    }
    in.close();

    if (!replaced) {
        lines.push_back(key + " = " + value);
    }

    std::ofstream out(conf_file, std::ios::trunc);
    if (out.is_open()) {
        for (const auto& l : lines) {
            out << l << "\n";
        }
    }
}

void WmBackend::set_layout(const std::string& layout) {
    run_miquctl("dispatch layout " + layout);
    update_config_file("layout", layout);
}

void WmBackend::toggle_split() {
    run_miquctl("dispatch togglesplit");
}

void WmBackend::set_smart_gaps(bool enabled) {
    std::string val = enabled ? "true" : "false";
    update_config_file("smart_gaps", val);
    reload_compositor();
}

void WmBackend::set_gaps(int inner, int outer) {
    update_config_file("inner", std::to_string(inner));
    update_config_file("outer", std::to_string(outer));
    reload_compositor();
}

void WmBackend::set_borders(int width, int radius) {
    update_config_file("width", std::to_string(width));
    update_config_file("radius", std::to_string(radius));
    reload_compositor();
}

void WmBackend::set_opacities(float active, float inactive) {
    std::ostringstream ss_act, ss_inact;
    ss_act.precision(2);
    ss_act << std::fixed << active;
    ss_inact.precision(2);
    ss_inact << std::fixed << inactive;
    update_config_file("active_opacity", ss_act.str());
    update_config_file("inactive_opacity", ss_inact.str());
    reload_compositor();
}

void WmBackend::set_animations(bool enabled) {
    // Under animations block: enabled = true/false
    update_config_file("enabled", enabled ? "true" : "false");
    reload_compositor();
}

void WmBackend::set_blur(bool enabled) {
    // Under blur block: enabled = true/false
    update_config_file("enabled", enabled ? "true" : "false");
    reload_compositor();
}

void WmBackend::reload_compositor() {
    run_miquctl("reload");
}

} // namespace miqu
