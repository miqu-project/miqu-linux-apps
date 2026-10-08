#pragma once

#include <string>
#include <cstdint>
#include <vector>

namespace miquinfo {

struct PartitionInfo {
    std::string mount_point;
    std::string device;
    std::string fs_type;
    uint64_t total_mb = 0;
    uint64_t used_mb = 0;
    float fraction = 0.0f;
    std::string formatted;
};

struct SystemInfo {
    // 1. Core OS & Host
    std::string os_name;
    std::string os_id;
    std::string logo_icon;
    std::string host_name;
    std::string kernel;
    std::string arch;
    std::string uptime;

    // Load & Tasks
    float load_1m = 0.0f;
    float load_5m = 0.0f;
    float load_15m = 0.0f;
    int running_tasks = 0;
    int total_tasks = 0;
    float cpu_load_fraction = 0.0f; // load_1m / cpu_cores capped at 1.0f

    // 2. CPU
    std::string cpu_model;
    int cpu_cores = 0;
    int cpu_threads = 0;
    int cpu_freq_cur_mhz = 0;
    int cpu_freq_max_mhz = 0;
    int cpu_freq_min_mhz = 0;
    std::string cpu_governor;
    std::string cpu_cache_l1;
    std::string cpu_cache_l2;
    std::string cpu_cache_l3;

    // 3. Motherboard & BIOS
    std::string mb_vendor;
    std::string mb_model;
    std::string bios_vendor;
    std::string bios_version;
    std::string bios_date;
    std::string boot_mode; // "UEFI" or "Legacy BIOS"
    bool secure_boot = false;

    // 4. GPU & Displays
    std::string gpu_model;
    std::string gpu_driver;
    std::string mesa_version;
    std::string displays;

    // 5. Memory
    uint64_t ram_total_mb = 0;
    uint64_t ram_used_mb = 0;
    uint64_t ram_free_mb = 0;
    uint64_t ram_buffers_mb = 0;
    uint64_t ram_cached_mb = 0;
    float ram_fraction = 0.0f; // 0.0f to 1.0f
    std::string ram_formatted;

    // Swap & ZRAM
    uint64_t swap_total_mb = 0;
    uint64_t swap_used_mb = 0;
    float swap_fraction = 0.0f;
    std::string swap_formatted;
    bool swap_is_zram = false;

    // 6. Storage Partitions
    uint64_t disk_total_gb = 0;
    uint64_t disk_used_gb = 0;
    float disk_fraction = 0.0f;
    std::string disk_formatted;
    std::vector<PartitionInfo> partitions;

    // 7. Power, Thermals & Fans
    int cpu_temp_c = 0;
    int pch_temp_c = 0;
    int wifi_temp_c = 0;
    int fan_rpm = 0;
    std::string power_profile; // "performance", "balanced", "power-saver"

    bool ac_online = false;
    bool has_battery = false;
    int battery_percent = 0;
    std::string battery_status;
    std::string battery_formatted;
    int battery_health_percent = 0;
    uint64_t battery_charge_now_mah = 0;
    uint64_t battery_charge_full_mah = 0;
    uint64_t battery_charge_design_mah = 0;
    float battery_power_watts = 0.0f;
    float battery_voltage_v = 0.0f;
    std::string battery_model;
    std::string battery_technology;
    int battery_cycle_count = 0;

    // 8. Desktop & Environment
    std::string compositor = "miquland (Wayland)";
    std::string toolkit = "miqutoolkit";
    std::string wayland_display;
    std::string shell;
    std::string terminal;
    std::string cursor_theme;
    std::string desktop_theme;
    std::string font_name;
    std::string packages;

    // 9. Network & Audio
    std::string network_interface;
    std::string network_type; // "Wi-Fi" or "Ethernet"
    std::string ip_address;
    bool bluetooth_available = false;
    std::string audio_server;

    std::string to_plain_text() const;
    std::string to_markdown() const;
};

class SysInfoReader {
public:
    static SystemInfo gather();
};

} // namespace miquinfo

