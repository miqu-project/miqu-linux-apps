#include "modules/windowmanager/wm_backend.hpp"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <algorithm>
#include <regex>

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

void WmBackend::set_layout(const std::string& layout) {
    run_miquctl("dispatch layout " + layout);
}

void WmBackend::toggle_split() {
    run_miquctl("dispatch togglesplit");
}

void WmBackend::set_smart_gaps([[maybe_unused]] bool enabled) {
}

void WmBackend::set_gaps([[maybe_unused]] int inner, [[maybe_unused]] int outer) {
}

void WmBackend::set_borders([[maybe_unused]] int width, [[maybe_unused]] int radius) {
}

void WmBackend::set_opacities([[maybe_unused]] float active, [[maybe_unused]] float inactive) {
}

void WmBackend::set_animations([[maybe_unused]] bool enabled) {
}

void WmBackend::set_blur([[maybe_unused]] bool enabled) {
}

void WmBackend::reload_compositor() {
    run_miquctl("reload");
}

} // namespace miqu
