#include "modules/display/display_backend.hpp"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <sstream>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cmath>

namespace miqu {

namespace fs = std::filesystem;

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

    // 1. Zero-fork fast path: read directly from sysfs backlight (<0.01ms)
    std::error_code ec;
    const fs::path sysfs_backlight("/sys/class/backlight");
    if (fs::exists(sysfs_backlight, ec) && fs::is_directory(sysfs_backlight, ec)) {
        for (const auto& entry : fs::directory_iterator(sysfs_backlight, ec)) {
            fs::path cur_p = entry.path() / "brightness";
            fs::path max_p = entry.path() / "max_brightness";
            if (fs::exists(cur_p, ec) && fs::exists(max_p, ec)) {
                std::ifstream cur_f(cur_p);
                std::ifstream max_f(max_p);
                long cur_val = 0, max_val = 0;
                if (cur_f >> cur_val && max_f >> max_val && max_val > 0) {
                    info.device_name = entry.path().filename().string();
                    info.current_brightness = static_cast<int>(cur_val);
                    info.max_brightness = static_cast<int>(max_val);
                    info.percent = std::clamp(static_cast<int>(std::round((cur_val * 100.0) / max_val)), 1, 100);
                    return info;
                }
            }
        }
    }

    // 2. Fallback to brightnessctl -m
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
    std::string out = run_cmd("miquctl -j gamma 2>/dev/null");
    if (out.find("\"enabled\":true") != std::string::npos) {
        return true;
    }
    // Fallback check if external gamma utility is running
    std::string gs = run_cmd("pgrep -x gammastep");
    return !gs.empty();
}

int DisplayBackend::get_gamma_temperature() {
    std::string out = run_cmd("miquctl -j gamma 2>/dev/null");
    size_t tpos = out.find("\"temperature\":");
    if (tpos != std::string::npos) {
        try {
            return std::clamp(std::stoi(out.substr(tpos + 14)), 1000, 10000);
        } catch (...) {}
    }
    return 4500;
}

void DisplayBackend::set_night_light_enabled(bool enabled, int temp_k) {
    if (enabled) {
        int k = std::clamp(temp_k, 1000, 10000);
        std::string cmd = "miquctl gamma " + std::to_string(k) + " >/dev/null 2>&1 &";
        std::system(cmd.c_str());
    } else {
        std::system("miquctl gamma off >/dev/null 2>&1 &");
    }
}

void DisplayBackend::set_gamma_temperature(int temp_k) {
    int k = std::clamp(temp_k, 1000, 10000);
    std::string cmd = "miquctl gamma " + std::to_string(k) + " >/dev/null 2>&1 &";
    std::system(cmd.c_str());
}

void DisplayBackend::lock_screen_now() {
    std::system("miqulock &");
}

} // namespace miqu
