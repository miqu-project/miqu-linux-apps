#include "display_backend.hpp"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
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
                    info.available = true;
                    return info;
                }
            }
        }
    }

    // 2. Fallback to brightnessctl -m
    std::string out = run_cmd("brightnessctl -m 2>/dev/null");
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
        info.available = true;
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

void DisplayBackend::sync_item_state(const std::shared_ptr<SettingItem>& item) {
    if (!item) return;

    DisplayBrightnessInfo info = get_brightness_info();
    if (!info.available) return;

    item->brightness_val = std::clamp(info.percent / 100.0f, 0.01f, 1.0f);

    std::vector<TelemetryEntry> tele;
    tele.push_back({"Backlight Interface", info.device_name + " (/sys/class/backlight)"});
    tele.push_back({"Current Level", std::to_string(info.current_brightness) + " / " + std::to_string(info.max_brightness) + " steps (" + std::to_string(info.percent) + "%)"});
    tele.push_back({"Controller CLI", "brightnessctl (native Arch Linux)"});
    tele.push_back({"Minimum Brightness", "1% (Prevents full blackout)"});
    item->telemetry = tele;
    item->telemetry_title = "Backlight & panel specifications";
}

bool DisplayBackend::is_available() {
    std::error_code ec;
    return fs::exists("/usr/bin/miquctl", ec) || fs::exists("/sys/class/backlight", ec);
}

bool DisplayBackend::is_night_light_active() {
    std::string out = run_cmd("miquctl -j gamma 2>/dev/null");
    if (out.find("\"enabled\":true") != std::string::npos) {
        return true;
    }
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
    static int s_last_k = -1;
    if (s_last_k == k) return;
    s_last_k = k;
    std::string cmd = "miquctl gamma " + std::to_string(k) + " >/dev/null 2>&1 &";
    std::system(cmd.c_str());
}

int DisplayBackend::kelvin_from_warmth(float warmth) {
    warmth = std::clamp(warmth, 0.0f, 1.0f);
    return std::clamp(static_cast<int>(std::round(6500.0f - warmth * 3800.0f)), 2700, 6500);
}

float DisplayBackend::warmth_from_kelvin(int kelvin) {
    kelvin = std::clamp(kelvin, 2700, 6500);
    return std::clamp((6500.0f - static_cast<float>(kelvin)) / 3800.0f, 0.0f, 1.0f);
}

void DisplayBackend::sync_nightlight_state(const std::shared_ptr<SettingItem>& item) {
    if (!item) return;

    bool active = is_night_light_active();
    int temp_k = get_gamma_temperature();

    item->nightlight_cfg.enabled = active;
    item->nightlight_cfg.warmth = warmth_from_kelvin(temp_k);

    std::string temp_desc;
    if (temp_k >= 6000) temp_desc = "Daylight (Standard D65)";
    else if (temp_k >= 5000) temp_desc = "Mild Soft White";
    else if (temp_k >= 4000) temp_desc = "Balanced Warm Neutral";
    else if (temp_k >= 3000) temp_desc = "Sunset Amber";
    else temp_desc = "Deep Candle Glow";

    item->telemetry = {
        {"Active State", active ? "Enabled (Filtering blue wavelengths)" : "Disabled (Standard Daylight)"},
        {"Color Temperature", std::to_string(temp_k) + " K (" + temp_desc + ")"},
        {"Daylight Target", "6500 K (CIE Standard Illuminant D65)"},
        {"Wayland Protocol", "wlr-gamma-control-v1 (Direct Hardware DRM)"},
        {"Compositor Engine", "miquland Native (Planckian Blackbody LUT)"},
        {"IPC Controller", "miquctl gamma [temp|on|off|toggle]"}
    };
    item->telemetry_title = "Gamma & color specifications";
}

} // namespace miqu
