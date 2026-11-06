#include "backlight_backend.hpp"
#include <filesystem>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace miqubar {

namespace fs = std::filesystem;

BacklightInfo BacklightBackend::get_info() {
    BacklightInfo info;

    // 1. Zero-fork fast path: direct sysfs reading (<0.01ms)
    std::error_code ec;
    const fs::path sysfs_backlight("/sys/class/backlight");
    if (fs::exists(sysfs_backlight, ec) && fs::is_directory(sysfs_backlight, ec)) {
        for (const auto& entry : fs::directory_iterator(sysfs_backlight, ec)) {
            if (entry.is_directory() || entry.is_symlink()) {
                fs::path cur_p = entry.path() / "brightness";
                fs::path max_p = entry.path() / "max_brightness";
                if (fs::exists(cur_p, ec) && fs::exists(max_p, ec)) {
                    std::ifstream cur_f(cur_p);
                    std::ifstream max_f(max_p);
                    long cur_val = 0, max_val = 0;
                    if (cur_f >> cur_val && max_f >> max_val && max_val > 0) {
                        info.device_path = entry.path().string();
                        info.percent = std::clamp(static_cast<int>(std::round((cur_val * 100.0) / max_val)), 0, 100);
                        return info;
                    }
                }
            }
        }
    }

    // 2. Fallback to brightnessctl -m
    FILE* fp = popen("brightnessctl -m 2>/dev/null", "r");
    if (fp) {
        char buf[256];
        if (fgets(buf, sizeof(buf), fp)) {
            std::string line(buf);
            size_t p = line.find('%');
            if (p != std::string::npos) {
                size_t start = line.rfind(',', p);
                if (start != std::string::npos) {
                    try {
                        info.percent = std::clamp(std::stoi(line.substr(start + 1, p - start - 1)), 0, 100);
                    } catch (...) {}
                }
            }
        }
        pclose(fp);
    }

    return info;
}

void BacklightBackend::set_brightness(int percent) {
    int p = std::clamp(percent, 5, 100);
    std::string cmd = "brightnessctl set " + std::to_string(p) + "% -q >/dev/null 2>&1 &";
    std::system(cmd.c_str());
}

} // namespace miqubar
