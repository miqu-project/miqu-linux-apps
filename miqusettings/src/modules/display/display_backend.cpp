#include "modules/display/display_backend.hpp"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <sstream>
#include <vector>
#include <algorithm>

namespace miqu {

struct PipeCloser {
    void operator()(FILE* fp) const {
        if (fp) pclose(fp);
    }
};

static std::string run_cmd(const std::string& cmd) {
    std::string result;
    std::unique_ptr<FILE, PipeCloser> pipe(popen(cmd.c_str(), "r"));
    if (!pipe) return result;

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe.get())) {
        result += buffer;
    }
    return result;
}

DisplayBrightnessInfo DisplayBackend::get_brightness_info() {
    DisplayBrightnessInfo info;
    std::string out = run_cmd("brightnessctl -m");
    if (out.empty()) {
        return info;
    }

    // Output format: device,class,curr,percent%,max
    std::stringstream ss(out);
    std::string token;
    std::vector<std::string> tokens;
    while (std::getline(ss, token, ',')) {
        tokens.push_back(token);
    }

    if (tokens.size() >= 5) {
        info.device_name = tokens[0];
        try {
            info.current_brightness = std::stoi(tokens[2]);
        } catch (...) {}
        
        std::string pct_str = tokens[3];
        if (!pct_str.empty() && pct_str.back() == '%') {
            pct_str.pop_back();
        }
        try {
            info.percent = std::clamp(std::stoi(pct_str), 1, 100);
        } catch (...) {
            info.percent = 50;
        }

        try {
            info.max_brightness = std::stoi(tokens[4]);
        } catch (...) {}
    }

    return info;
}

int DisplayBackend::get_brightness_percent() {
    return get_brightness_info().percent;
}

void DisplayBackend::set_brightness_percent(int percent) {
    int p = std::clamp(percent, 1, 100);
    std::string cmd = "brightnessctl set " + std::to_string(p) + "% >/dev/null 2>&1 &";
    std::system(cmd.c_str());
}

bool DisplayBackend::is_night_light_active() {
    std::string out = run_cmd("pgrep -x gammastep");
    return !out.empty();
}

void DisplayBackend::set_night_light_enabled(bool enabled, int temp_k) {
    if (enabled) {
        int k = std::clamp(temp_k, 2500, 6500);
        std::string cmd = "pkill -x gammastep >/dev/null 2>&1; gammastep -O " + std::to_string(k) + " >/dev/null 2>&1 &";
        std::system(cmd.c_str());
    } else {
        std::system("pkill -x gammastep >/dev/null 2>&1; gammastep -x >/dev/null 2>&1 &");
    }
}

void DisplayBackend::set_gamma_temperature(int temp_k) {
    int k = std::clamp(temp_k, 2500, 6500);
    std::string cmd = "pkill -x gammastep >/dev/null 2>&1; gammastep -O " + std::to_string(k) + " >/dev/null 2>&1 &";
    std::system(cmd.c_str());
}

void DisplayBackend::lock_screen_now() {
    std::system("miqulock &");
}

} // namespace miqu
