#pragma once

#include "settings_model.hpp"
#include <string>
#include <vector>
#include <memory>

namespace miqu {

class BatteryBackend {
public:
    // Query live battery & power supply info from Linux sysfs (/sys/class/power_supply)
    static bool query_battery_info(BatteryConfig& cfg);

    // Sync SettingItem with fresh live battery status, subtitle, icon, and telemetry
    static void sync_battery_item(const std::shared_ptr<SettingItem>& item);

    // Get Nerd Font battery icon glyph based on charging state, power profile, and percentage
    static std::string get_battery_icon(const BatteryConfig& cfg);

    // Populate battery telemetry entries from live BatteryConfig
    static void populate_battery_telemetry(const BatteryConfig& cfg, std::vector<TelemetryEntry>& telemetry);

    // Populate power management specifications telemetry
    static void populate_power_telemetry(const BatteryConfig& cfg, std::vector<TelemetryEntry>& telemetry);

    // Active power profile management (via powerprofilesctl and ACPI platform_profile)
    static std::string get_power_profile();
    static bool set_power_profile(const std::string& profile, std::string& err_msg);
    static void set_power_profile_async(const std::string& profile);

    // Helpers to read sysfs attributes
    static std::string read_sysfs(const std::string& path);
    static long long read_sysfs_ll(const std::string& path, long long default_val = 0);
};

} // namespace miqu
