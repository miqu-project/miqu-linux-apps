#include "monitor_backend.hpp"
#include "settings_model.hpp"
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

static void parse_mode_line(const std::string& l, MonitorInfo* cur) {
    size_t px_pos = l.find("px,");
    std::string res_part = trim(l.substr(0, px_pos));
    size_t x_pos = res_part.find('x');
    if (x_pos == std::string::npos) return;

    MonitorMode mode;
    try {
        mode.width = std::stoi(res_part.substr(0, x_pos));
        mode.height = std::stoi(res_part.substr(x_pos + 1));
    } catch (...) { return; }

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

static void parse_monitor_property(const std::string& l, MonitorInfo* cur) {
    if (l.rfind("Make:", 0) == 0) {
        cur->make = trim(l.substr(5));
    } else if (l.rfind("Model:", 0) == 0) {
        cur->model = trim(l.substr(6));
    } else if (l.rfind("Physical size:", 0) == 0) {
        std::string sz = trim(l.substr(14));
        size_t x_pos = sz.find('x');
        if (x_pos != std::string::npos) {
            try {
                cur->physical_width_mm = std::stoi(sz.substr(0, x_pos));
                size_t mm_pos = sz.find("mm");
                cur->physical_height_mm = std::stoi(sz.substr(x_pos + 1, mm_pos != std::string::npos ? mm_pos - x_pos - 1 : std::string::npos));
            } catch (...) {}
        }
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
    } else if (l.rfind("Adaptive Sync:", 0) == 0) {
        cur->adaptive_sync = (l.find("enabled") != std::string::npos);
    } else if (l.find("px,") != std::string::npos) {
        parse_mode_line(l, cur);
    }
}

static MonitorInfo* parse_header_line(const std::string& line, std::vector<MonitorInfo>& monitors) {
    size_t sp = line.find(' ');
    if (sp == std::string::npos) return nullptr;

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
    return &monitors.back();
}

std::vector<MonitorInfo> MonitorBackend::get_monitors() {
    std::vector<MonitorInfo> monitors;
    std::string out = run_cmd("wlr-randr");
    if (out.empty()) {
        MonitorInfo def;
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
            cur = parse_header_line(line, monitors);
        } else if (cur) {
            parse_monitor_property(trim(line), cur);
        }
    }

    for (auto& mon : monitors) {
        if (mon.current_width == 0 && !mon.modes.empty()) {
            mon.current_width = mon.modes.front().width;
            mon.current_height = mon.modes.front().height;
            mon.current_refresh = mon.modes.front().refresh_rate;
        }
    }

    if (monitors.empty()) {
        MonitorInfo def;
        def.modes.push_back({1920, 1080, 60.0f, true, true});
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

bool MonitorBackend::set_mode(const std::string& name, int width, int height, float refresh) {
    if (name.empty() || width <= 0 || height <= 0) return false;

    std::string cmd = "wlr-randr --output " + name + " --mode " + std::to_string(width) + "x" + std::to_string(height);
    if (refresh > 0.0f) {
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
    cmd += " >/dev/null 2>&1";
    return (std::system(cmd.c_str()) == 0);
}

bool MonitorBackend::set_scale(const std::string& name, float scale) {
    if (name.empty() || scale < 0.1f) return false;
    char sc_buf[32];
    std::snprintf(sc_buf, sizeof(sc_buf), "%.2f", scale);
    std::string cmd = "wlr-randr --output " + name + " --scale " + std::string(sc_buf) + " >/dev/null 2>&1";
    return (std::system(cmd.c_str()) == 0);
}

bool MonitorBackend::set_transform(const std::string& name, const std::string& transform) {
    if (name.empty() || transform.empty()) return false;
    std::string cmd = "wlr-randr --output " + name + " --transform " + transform + " >/dev/null 2>&1";
    return (std::system(cmd.c_str()) == 0);
}

bool MonitorBackend::set_primary(const std::string& name) {
    if (name.empty()) return false;
    std::string cmd = "wlr-randr --output " + name + " --pos 0,0 >/dev/null 2>&1";
    return (std::system(cmd.c_str()) == 0);
}

bool MonitorBackend::apply_settings(const std::string& name, int width, int height, float refresh, float scale, const std::string& transform) {
    if (name.empty()) return false;

    std::string cmd = "wlr-randr --output " + name;

    if (width > 0 && height > 0) {
        cmd += " --mode " + std::to_string(width) + "x" + std::to_string(height);
        if (refresh > 0.0f) {
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

    cmd += " >/dev/null 2>&1";
    return (std::system(cmd.c_str()) == 0);
}

std::pair<int, int> MonitorBackend::parse_resolution(const std::string& res_str) {
    int w = 1920, h = 1080;
    size_t sep = res_str.find("×");
    if (sep != std::string::npos) {
        try {
            w = std::stoi(trim(res_str.substr(0, sep)));
            sep += std::string("×").length();
            h = std::stoi(trim(res_str.substr(sep)));
        } catch (...) {}
    } else {
        sep = res_str.find('x');
        if (sep != std::string::npos) {
            try {
                w = std::stoi(trim(res_str.substr(0, sep)));
                h = std::stoi(trim(res_str.substr(sep + 1)));
            } catch (...) {}
        }
    }
    return {w, h};
}

float MonitorBackend::parse_refresh_rate(const std::string& hz_str) {
    try {
        size_t sp = hz_str.find(' ');
        std::string num = (sp != std::string::npos) ? hz_str.substr(0, sp) : hz_str;
        return std::stof(num);
    } catch (...) {
        return 60.0f;
    }
}

float MonitorBackend::parse_scale(const std::string& scale_str) {
    try {
        size_t pct = scale_str.find('%');
        std::string num = (pct != std::string::npos) ? scale_str.substr(0, pct) : scale_str;
        float val = std::stof(num);
        if (val > 10.0f) val /= 100.0f;
        return val;
    } catch (...) {
        return 1.0f;
    }
}

std::string MonitorBackend::transform_to_ui(const std::string& transform) {
    if (transform == "90") return "90° (Portrait)";
    if (transform == "180") return "180°";
    if (transform == "270") return "270° (Portrait)";
    return "Standard (0°)";
}

std::string MonitorBackend::ui_to_transform(const std::string& ui_str) {
    if (ui_str.find("90") != std::string::npos) return "90";
    if (ui_str.find("180") != std::string::npos) return "180";
    if (ui_str.find("270") != std::string::npos) return "270";
    return "normal";
}

std::vector<std::string> MonitorBackend::get_available_refresh_rates(const std::string& name, int width, int height) {
    auto mon = get_monitor(name);
    std::vector<std::string> rates;
    for (const auto& m : mon.modes) {
        if (m.width == width && m.height == height) {
            int hz_int = static_cast<int>(std::round(m.refresh_rate));
            std::string s = std::to_string(hz_int) + " Hz";
            if (std::find(rates.begin(), rates.end(), s) == rates.end()) {
                rates.push_back(s);
            }
        }
    }
    if (rates.empty()) {
        rates = {"60 Hz"};
    }
    return rates;
}

static std::vector<TelemetryEntry> build_telemetry(const MonitorInfo& info, const MonitorConfig& cfg) {
    std::vector<TelemetryEntry> t;
    t.push_back({"Connector", info.name});
    if (!info.make.empty() || !info.model.empty()) {
        t.push_back({"Panel Model", (info.make.empty() ? "" : info.make + " ") + info.model});
    }
    t.push_back({"Current Mode", cfg.current_resolution + " @" + cfg.current_refresh_rate});
    if (info.physical_width_mm > 0 && info.physical_height_mm > 0) {
        t.push_back({"Physical Dimensions", std::to_string(info.physical_width_mm) + " × " + std::to_string(info.physical_height_mm) + " mm"});
    }
    t.push_back({"Scale Factor", cfg.current_scale + " (" + std::to_string(info.scale).substr(0, 4) + "x)"});
    t.push_back({"Orientation", cfg.current_orientation});
    t.push_back({"Position", std::to_string(info.pos_x) + ", " + std::to_string(info.pos_y) + (info.is_primary ? " (Primary)" : "")});
    t.push_back({"Adaptive Sync", info.adaptive_sync ? "Enabled" : "Disabled"});
    return t;
}

void MonitorBackend::sync_monitor_item(const std::shared_ptr<SettingItem>& item, const MonitorInfo& info) {
    if (!item) return;
    item->is_monitor = true;
    item->monitor_cfg.connector = info.name;
    item->monitor_cfg.model_name = !info.description.empty() ? info.description : (!info.model.empty() ? (info.make + " " + info.model) : info.name);
    item->monitor_cfg.is_primary = info.is_primary;
    item->monitor_cfg.current_resolution = std::to_string(info.current_width) + " × " + std::to_string(info.current_height);

    item->monitor_cfg.available_resolutions.clear();
    for (const auto& m : info.modes) {
        std::string r = std::to_string(m.width) + " × " + std::to_string(m.height);
        if (std::find(item->monitor_cfg.available_resolutions.begin(), item->monitor_cfg.available_resolutions.end(), r) == item->monitor_cfg.available_resolutions.end()) {
            item->monitor_cfg.available_resolutions.push_back(r);
        }
    }
    if (item->monitor_cfg.available_resolutions.empty()) {
        item->monitor_cfg.available_resolutions = {"1920 × 1080"};
    }

    int hz_int = static_cast<int>(std::round(info.current_refresh));
    item->monitor_cfg.current_refresh_rate = std::to_string(hz_int > 0 ? hz_int : 60) + " Hz";
    item->monitor_cfg.available_refresh_rates = get_available_refresh_rates(info.name, info.current_width, info.current_height);

    int sc_int = static_cast<int>(std::round(info.scale * 100.0f));
    item->monitor_cfg.current_scale = std::to_string(sc_int > 0 ? sc_int : 100) + "%";
    item->monitor_cfg.available_scales = {"100%", "125%", "150%", "175%", "200%"};
    if (std::find(item->monitor_cfg.available_scales.begin(), item->monitor_cfg.available_scales.end(), item->monitor_cfg.current_scale) == item->monitor_cfg.available_scales.end()) {
        item->monitor_cfg.available_scales.push_back(item->monitor_cfg.current_scale);
        std::sort(item->monitor_cfg.available_scales.begin(), item->monitor_cfg.available_scales.end(), [](const std::string& a, const std::string& b) {
            return std::stoi(a) < std::stoi(b);
        });
    }

    item->monitor_cfg.current_orientation = transform_to_ui(info.transform);
    item->monitor_cfg.available_orientations = {"Standard (0°)", "90° (Portrait)", "180°", "270° (Portrait)"};
    item->telemetry = build_telemetry(info, item->monitor_cfg);
}

} // namespace miqu
