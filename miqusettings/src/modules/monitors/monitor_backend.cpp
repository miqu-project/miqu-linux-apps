#include "modules/monitors/monitor_backend.hpp"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <sstream>
#include <algorithm>
#include <cmath>

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

static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

std::vector<MonitorInfo> MonitorBackend::get_monitors() {
    std::vector<MonitorInfo> monitors;
    std::string out = run_cmd("wlr-randr");
    if (out.empty()) {
        // Fallback default
        MonitorInfo def;
        def.name = "eDP-1";
        def.description = "Built-in Display";
        def.current_width = 1920;
        def.current_height = 1080;
        def.current_refresh = 60.0f;
        def.is_primary = true;
        def.scale = 1.0f;
        def.transform = "normal";
        def.modes.push_back({1920, 1080, 60.0f, true, true});
        monitors.push_back(def);
        return monitors;
    }

    std::stringstream ss(out);
    std::string line;
    MonitorInfo* cur = nullptr;

    while (std::getline(ss, line)) {
        if (line.empty()) continue;

        if (line[0] != ' ' && line[0] != '\t') {
            // New output header: <name> "<desc>"
            size_t sp = line.find(' ');
            if (sp != std::string::npos) {
                MonitorInfo mon;
                mon.name = line.substr(0, sp);
                size_t q1 = line.find('"', sp);
                size_t q2 = (q1 != std::string::npos) ? line.rfind('"') : std::string::npos;
                if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1) {
                    mon.description = line.substr(q1 + 1, q2 - q1 - 1);
                } else {
                    mon.description = trim(line.substr(sp + 1));
                }
                monitors.push_back(mon);
                cur = &monitors.back();
            }
        } else if (cur) {
            std::string l = trim(line);
            if (l.rfind("Make:", 0) == 0) {
                cur->make = trim(l.substr(5));
            } else if (l.rfind("Model:", 0) == 0) {
                cur->model = trim(l.substr(6));
            } else if (l.rfind("Enabled:", 0) == 0) {
                cur->enabled = (l.find("yes") != std::string::npos);
            } else if (l.rfind("Position:", 0) == 0) {
                std::string pos_str = trim(l.substr(9));
                size_t comma = pos_str.find(',');
                if (comma != std::string::npos) {
                    try {
                        cur->pos_x = std::stoi(pos_str.substr(0, comma));
                        cur->pos_y = std::stoi(pos_str.substr(comma + 1));
                    } catch (...) {}
                    cur->is_primary = (cur->pos_x == 0 && cur->pos_y == 0);
                }
            } else if (l.rfind("Transform:", 0) == 0) {
                cur->transform = trim(l.substr(10));
            } else if (l.rfind("Scale:", 0) == 0) {
                try {
                    cur->scale = std::stof(trim(l.substr(6)));
                } catch (...) {}
            } else if (l.find("px,") != std::string::npos) {
                // Mode: e.g. "1920x1080 px, 60.049000 Hz (preferred, current)"
                size_t px_pos = l.find("px,");
                std::string res_part = trim(l.substr(0, px_pos));
                size_t x_pos = res_part.find('x');
                if (x_pos != std::string::npos) {
                    MonitorMode mode;
                    try {
                        mode.width = std::stoi(res_part.substr(0, x_pos));
                        mode.height = std::stoi(res_part.substr(x_pos + 1));
                    } catch (...) { continue; }

                    std::string rest = trim(l.substr(px_pos + 3));
                    size_t hz_pos = rest.find("Hz");
                    if (hz_pos != std::string::npos) {
                        try {
                            mode.refresh_rate = std::stof(trim(rest.substr(0, hz_pos)));
                        } catch (...) {}
                    }
                    mode.is_preferred = (rest.find("preferred") != std::string::npos);
                    mode.is_current = (rest.find("current") != std::string::npos);

                    if (mode.is_current) {
                        cur->current_width = mode.width;
                        cur->current_height = mode.height;
                        cur->current_refresh = mode.refresh_rate;
                    }

                    cur->modes.push_back(mode);
                }
            }
        }
    }

    if (monitors.empty()) {
        MonitorInfo def;
        def.name = "eDP-1";
        def.description = "Built-in Display";
        def.current_width = 1920;
        def.current_height = 1080;
        def.current_refresh = 60.0f;
        def.is_primary = true;
        monitors.push_back(def);
    }

    return monitors;
}

MonitorInfo MonitorBackend::get_monitor(const std::string& name) {
    auto list = get_monitors();
    for (const auto& m : list) {
        if (m.name == name) return m;
    }
    if (!list.empty()) return list.front();
    return MonitorInfo{};
}

bool MonitorBackend::apply_settings(const std::string& name, int width, int height, float refresh, float scale, const std::string& transform) {
    if (name.empty()) return false;

    std::string cmd = "wlr-randr --output " + name;

    if (width > 0 && height > 0) {
        cmd += " --mode " + std::to_string(width) + "x" + std::to_string(height);
        if (refresh > 0.0f) {
            // Find exact refresh string if matches
            auto mon = get_monitor(name);
            for (const auto& m : mon.modes) {
                if (m.width == width && m.height == height && std::abs(m.refresh_rate - refresh) < 0.5f) {
                    char hz_buf[32];
                    std::snprintf(hz_buf, sizeof(hz_buf), "%.3f", m.refresh_rate);
                    cmd += "@" + std::string(hz_buf) + "Hz";
                    break;
                }
            }
        }
    }

    if (scale > 0.1f) {
        char sc_buf[32];
        std::snprintf(sc_buf, sizeof(sc_buf), "%.2f", scale);
        cmd += " --scale " + std::string(sc_buf);
    }

    if (!transform.empty()) {
        cmd += " --transform " + transform;
    }

    cmd += " >/dev/null 2>&1 &";
    int ret = std::system(cmd.c_str());
    return (ret == 0);
}

} // namespace miqu
