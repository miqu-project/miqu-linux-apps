#pragma once

#include "settings_model.hpp"
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace miqu {

class AppsBackend {
public:
    static std::vector<std::shared_ptr<SettingItem>> get_desktop_app_items(bool force_refresh = false);
    static std::vector<std::shared_ptr<SettingItem>> get_system_package_items(bool force_refresh = false);

    static void populate_app_details(InstalledAppInfo& info, std::vector<TelemetryEntry>& telemetry);
    static int find_running_pid(const std::string& binary_path);
    static bool launch_app(const InstalledAppInfo& info);
    static bool kill_app(const InstalledAppInfo& info);
    static bool uninstall_app(const InstalledAppInfo& info, std::string& error_msg);
    static bool clear_cache(const InstalledAppInfo& info, std::string& error_msg);
    static std::string query_network_policy(const std::string& binary_path);
    static std::string query_network_policy(const InstalledAppInfo& info);
    static bool set_network_policy(const std::string& binary_path, const std::string& policy, std::string& message);
    static bool set_network_policy(const InstalledAppInfo& info, const std::string& policy, std::string& message);

    static std::string run_cmd(const std::string& cmd);
    static std::string resolve_desktop_path(const std::string& desktop_id);
    static std::string resolve_binary_path(const std::string& exec_cmd);
    static std::string resolve_real_executable(const std::string& path);
    static std::string format_bytes(uint64_t bytes);

private:
    static std::string escape_regex(const std::string& str);
    static std::string get_icon_for_app(const std::string& categories, const std::string& name, const std::string& id);
    static std::string get_icon_for_pkg(const std::string& pkg_name);
    static void query_pacman_metadata(InstalledAppInfo& info, std::vector<TelemetryEntry>& telemetry);
    static void query_flatpak_metadata(InstalledAppInfo& info, std::vector<TelemetryEntry>& telemetry);
    static void calculate_storage_sizes(InstalledAppInfo& info);
};

} // namespace miqu
