#include "battery_backend.hpp"
#include "apps_backend.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <iomanip>
#include <thread>
#include <cmath>

namespace fs = std::filesystem;

namespace miqu {

std::string BatteryBackend::read_sysfs(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::string line;
    if (std::getline(file, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ' || line.back() == '\t')) {
            line.pop_back();
        }
        size_t s = line.find_first_not_of(" \t");
        return (s != std::string::npos) ? line.substr(s) : "";
    }
    return "";
}

long long BatteryBackend::read_sysfs_ll(const std::string& path, long long default_val) {
    std::string val = read_sysfs(path);
    if (val.empty()) return default_val;
    try {
        return std::stoll(val);
    } catch (...) {
        return default_val;
    }
}

namespace {

std::string find_battery_dir() {
    std::error_code ec;
    if (!fs::exists("/sys/class/power_supply", ec)) return "";
    for (const auto& entry : fs::directory_iterator("/sys/class/power_supply", ec)) {
        std::string name = entry.path().filename().string();
        if (name.rfind("BAT", 0) == 0) {
            std::string present = BatteryBackend::read_sysfs(entry.path().string() + "/present");
            if (present.empty() || present == "1") {
                return entry.path().string();
            }
        }
    }
    return "";
}

std::string format_wh(float val) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1) << val << " Wh";
    return ss.str();
}

std::string format_time_str(const BatteryConfig& cfg, float draw_rate, float v_calc) {
    (void)v_calc;
    if (!cfg.has_battery) {
        return "Connected to AC power adapter (No battery detected)";
    }
    if (cfg.status == "Full" || cfg.percentage >= 100) {
        return "Fully charged (100%)";
    }
    if (cfg.status == "Discharging" && draw_rate > 0.1f && cfg.energy_now_wh > 0.05f) {
        float hours = cfg.energy_now_wh / draw_rate;
        if (hours > 0.01f && hours < 72.0f) {
            int h = static_cast<int>(hours);
            int m = static_cast<int>((hours - static_cast<float>(h)) * 60.0f);
            if (h > 0) {
                return "Approximately " + std::to_string(h) + " hour" + (h > 1 ? "s " : " ") +
                       std::to_string(m) + " minute" + (m != 1 ? "s" : "") + " remaining";
            }
            return "Approximately " + std::to_string(m) + " minute" + (m != 1 ? "s" : "") + " remaining";
        }
    } else if (cfg.status == "Charging" && draw_rate > 0.1f && cfg.full_capacity_wh > cfg.energy_now_wh) {
        float needed = cfg.full_capacity_wh - cfg.energy_now_wh;
        float hours = needed / draw_rate;
        if (hours > 0.01f && hours < 48.0f) {
            int h = static_cast<int>(hours);
            int m = static_cast<int>((hours - static_cast<float>(h)) * 60.0f);
            if (h > 0) {
                return std::to_string(h) + " hour" + (h > 1 ? "s " : " ") +
                       std::to_string(m) + " minutes until fully charged";
            }
            return std::to_string(m) + " minutes until fully charged";
        }
        return "Almost fully charged";
    }
    return cfg.is_charging ? "Plugged in, charging" : "Discharging on battery power";
}

} // namespace

bool BatteryBackend::query_battery_info(BatteryConfig& cfg) {
    std::string bat_dir = find_battery_dir();
    if (bat_dir.empty()) {
        cfg.has_battery = false;
        cfg.is_charging = false;
        cfg.status = "AC Power";
        cfg.percentage = 100;
        cfg.time_remaining_str = "Running on AC Power (Desktop system)";
        cfg.power_profile = get_power_profile();
        cfg.power_saver = (cfg.power_profile == "Power Saver");
        cfg.populated = true;
        return true;
    }

    cfg.has_battery = true;

    // 1. Capacity & Status
    long long cap = read_sysfs_ll(bat_dir + "/capacity", 0);
    cfg.percentage = static_cast<int>(std::clamp(cap, 0LL, 100LL));

    std::string st = read_sysfs(bat_dir + "/status");
    cfg.status = st.empty() ? "Discharging" : st;
    cfg.is_charging = (cfg.status == "Charging");

    // 2. Hardware Model & Tech
    std::string model = read_sysfs(bat_dir + "/model_name");
    cfg.model_name = model.empty() ? "Standard Battery" : model;

    std::string mfr = read_sysfs(bat_dir + "/manufacturer");
    cfg.manufacturer = mfr;

    std::string tech = read_sysfs(bat_dir + "/technology");
    cfg.technology = tech.empty() ? "Li-ion" : tech;

    long long cyc = read_sysfs_ll(bat_dir + "/cycle_count", -1);
    cfg.cycle_count = (cyc >= 0) ? static_cast<int>(cyc) : 0;

    // 3. Voltages and Currents
    long long v_now = read_sysfs_ll(bat_dir + "/voltage_now", 0);
    long long v_min = read_sysfs_ll(bat_dir + "/voltage_min_design", 0);
    cfg.voltage_v = (v_now > 0) ? (v_now / 1000000.0f) : ((v_min > 0) ? (v_min / 1000000.0f) : 0.0f);
    float v_calc = (v_now > 0) ? (v_now / 1000000.0f) : ((v_min > 0) ? (v_min / 1000000.0f) : 7.6f);

    long long i_now = read_sysfs_ll(bat_dir + "/current_now", 0);
    cfg.current_a = (i_now > 0) ? (i_now / 1000000.0f) : 0.0f;

    long long p_now = read_sysfs_ll(bat_dir + "/power_now", 0);
    if (p_now > 0) {
        cfg.power_draw_w = p_now / 1000000.0f;
    } else if (cfg.voltage_v > 0.0f && cfg.current_a > 0.0f) {
        cfg.power_draw_w = cfg.voltage_v * cfg.current_a;
    }

    // 4. Energy & Capacities
    long long e_now = read_sysfs_ll(bat_dir + "/energy_now", 0);
    long long e_full = read_sysfs_ll(bat_dir + "/energy_full", 0);
    long long e_design = read_sysfs_ll(bat_dir + "/energy_full_design", 0);

    if (e_now > 0) cfg.energy_now_wh = e_now / 1000000.0f;
    else {
        long long c_now = read_sysfs_ll(bat_dir + "/charge_now", 0);
        cfg.energy_now_wh = (c_now / 1000000.0f) * v_calc;
    }

    if (e_full > 0) cfg.full_capacity_wh = e_full / 1000000.0f;
    else {
        long long c_full = read_sysfs_ll(bat_dir + "/charge_full", 0);
        cfg.full_capacity_wh = (c_full / 1000000.0f) * v_calc;
    }

    if (e_design > 0) cfg.design_capacity_wh = e_design / 1000000.0f;
    else {
        long long c_design = read_sysfs_ll(bat_dir + "/charge_full_design", 0);
        cfg.design_capacity_wh = (c_design / 1000000.0f) * v_calc;
    }

    // 5. Health
    if (cfg.design_capacity_wh > 0.0f && cfg.full_capacity_wh > 0.0f) {
        cfg.health_pct = (cfg.full_capacity_wh / cfg.design_capacity_wh) * 100.0f;
        if (cfg.health_pct > 100.0f) cfg.health_pct = 100.0f;
        if (cfg.health_pct >= 80.0f) cfg.health_status = "Good";
        else if (cfg.health_pct >= 60.0f) cfg.health_status = "Fair";
        else cfg.health_status = "Degraded";
    }

    // 6. Time remaining string
    float draw_rate = (cfg.power_draw_w > 0.1f) ? cfg.power_draw_w : (cfg.current_a * v_calc);
    cfg.time_remaining_str = format_time_str(cfg, draw_rate, v_calc);

    // 7. Thermal
    long long t_raw = read_sysfs_ll("/sys/class/thermal/thermal_zone0/temp", 0);
    if (t_raw > 0) {
        cfg.temp_c = t_raw / 1000.0f;
    }

    // 8. Charge control threshold
    long long end_th = read_sysfs_ll(bat_dir + "/charge_control_end_threshold", -1);
    if (end_th > 0) {
        cfg.charge_limit_pct = static_cast<int>(end_th);
        cfg.charge_limit_enabled = (cfg.charge_limit_pct < 100);
    }

    // 9. Power profile
    cfg.power_profile = get_power_profile();
    cfg.power_saver = (cfg.power_profile == "Power Saver");
    cfg.populated = true;
    return true;
}

std::string BatteryBackend::get_battery_icon(const BatteryConfig& cfg) {
    if (!cfg.has_battery) return "󰚥";
    if (cfg.is_charging) return "󰂄";
    if (cfg.power_saver) return "󰂃";
    int p = cfg.percentage;
    if (p >= 90) return "󰁹";
    if (p >= 70) return "󰂀";
    if (p >= 50) return "󰁾";
    if (p >= 30) return "󰁼";
    if (p >= 15) return "󰁺";
    return "󰂎";
}

void BatteryBackend::populate_battery_telemetry(const BatteryConfig& cfg, std::vector<TelemetryEntry>& telemetry) {
    telemetry.clear();
    if (!cfg.has_battery) {
        telemetry.push_back({"Power Source", "AC Mains (Connected)"});
        telemetry.push_back({"System Type", "Desktop / Battery-less Platform"});
        telemetry.push_back({"Power Delivery", "Continuous Line Power"});
        return;
    }

    std::ostringstream health_ss;
    health_ss << std::fixed << std::setprecision(1) << cfg.health_pct << "% of design capacity (" << cfg.health_status << ")";
    telemetry.push_back({"Battery Health", health_ss.str()});

    if (cfg.cycle_count > 0) {
        telemetry.push_back({"Cycle Count", std::to_string(cfg.cycle_count) + " cycles"});
    } else {
        telemetry.push_back({"Cycle Count", "N/A (Not reported by ACPI)"});
    }

    std::string cap_str = format_wh(cfg.full_capacity_wh) + " (Design: " + format_wh(cfg.design_capacity_wh) + ")";
    telemetry.push_back({"Full Charge Capacity", cap_str});

    std::string pwr_src = cfg.is_charging ? "AC Adapter (Charging)" :
                         (cfg.status == "Full" ? "AC Adapter (Fully Charged)" : "Battery (Discharging)");
    telemetry.push_back({"Power Source", pwr_src});

    std::ostringstream rate_ss;
    rate_ss << std::fixed << std::setprecision(1) << cfg.power_draw_w << " W";
    if (cfg.power_saver) rate_ss << " (Power Saver Active)";
    else if (cfg.is_charging) rate_ss << " (Charge Rate)";
    telemetry.push_back({cfg.is_charging ? "Charge Rate" : "Discharge Rate", rate_ss.str()});

    if (cfg.voltage_v > 0.05f) {
        std::ostringstream volt_ss;
        volt_ss << std::fixed << std::setprecision(2) << cfg.voltage_v << " V";
        telemetry.push_back({"Cell Voltage", volt_ss.str()});
    }

    if (cfg.temp_c > 0.1f) {
        std::ostringstream temp_ss;
        temp_ss << std::fixed << std::setprecision(1) << cfg.temp_c << " °C";
        telemetry.push_back({"Package / Cell Temp", temp_ss.str()});
    }

    telemetry.push_back({"Technology", cfg.technology});

    std::string dev_str = cfg.model_name;
    if (!cfg.manufacturer.empty() && cfg.manufacturer != cfg.model_name) {
        dev_str += " (" + cfg.manufacturer + ")";
    }
    telemetry.push_back({"Hardware Device", dev_str});
}

void BatteryBackend::populate_power_telemetry(const BatteryConfig& cfg, std::vector<TelemetryEntry>& telemetry) {
    telemetry.clear();

    std::string gov = read_sysfs("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor");
    if (gov.empty()) gov = "powersave (amd-pstate-epp)";

    std::string epp = read_sysfs("/sys/devices/system/cpu/cpu0/cpufreq/energy_performance_preference");
    if (epp.empty()) epp = (cfg.power_profile == "Performance" ? "performance" : "balance_power");

    std::string platform_prof = read_sysfs("/sys/firmware/acpi/platform_profile");
    if (platform_prof.empty()) platform_prof = cfg.power_profile;

    std::ostringstream draw_ss;
    draw_ss << std::fixed << std::setprecision(1) << cfg.power_draw_w << " W";
    if (cfg.power_saver) draw_ss << " (Power Saver Active)";
    else if (cfg.power_profile == "Performance") draw_ss << " (Turbo Mode Active)";

    std::ostringstream temp_ss;
    temp_ss << std::fixed << std::setprecision(1) << cfg.temp_c << " °C";

    telemetry.push_back({"Current Governor", gov});
    telemetry.push_back({"Energy Performance Preference", epp});
    telemetry.push_back({"Platform Profile", platform_prof + " (ACPI platform profile)"});
    telemetry.push_back({"Active Power Draw", draw_ss.str()});
    telemetry.push_back({"Package Temp", temp_ss.str()});
}

void BatteryBackend::sync_battery_item(const std::shared_ptr<SettingItem>& item) {
    if (!item || !item->battery_cfg) return;
    query_battery_info(*item->battery_cfg);

    if (item->battery_cfg->has_battery) {
        item->subtitle = std::to_string(item->battery_cfg->percentage) + "% • " + item->battery_cfg->status;
    } else {
        item->subtitle = "AC Power • Running on Mains";
    }
    item->icon = get_battery_icon(*item->battery_cfg);
    populate_battery_telemetry(*item->battery_cfg, item->telemetry);
}

std::string BatteryBackend::get_power_profile() {
    std::string plat = read_sysfs("/sys/firmware/acpi/platform_profile");
    if (!plat.empty()) {
        if (plat == "quiet" || plat == "cool" || plat == "low-power" || plat == "power-saver") return "Power Saver";
        if (plat == "performance") return "Performance";
        return "Balanced";
    }

    std::string epp = read_sysfs("/sys/devices/system/cpu/cpu0/cpufreq/energy_performance_preference");
    if (!epp.empty()) {
        if (epp == "power") return "Power Saver";
        if (epp == "performance") return "Performance";
        return "Balanced";
    }

    return "Balanced";
}

bool BatteryBackend::set_power_profile(const std::string& profile, std::string& err_msg) {
    std::string cli_arg = "balanced";
    if (profile == "Power Saver") cli_arg = "power-saver";
    else if (profile == "Performance") cli_arg = "performance";

    std::string cmd = "powerprofilesctl set " + cli_arg + " 2>&1";
    std::string out = AppsBackend::run_cmd(cmd);
    if (!out.empty() && out.find("Failed") != std::string::npos) {
        err_msg = out;
        return false;
    }
    return true;
}

void BatteryBackend::set_power_profile_async(const std::string& profile) {
    std::thread([profile]() {
        std::string err;
        set_power_profile(profile, err);
    }).detach();
}

} // namespace miqu
