#pragma once

#include "modules/applications/apps_types.hpp"
#include <vector>
#include <string>
#include <memory>

namespace miqu {

class AppsBackend {
public:
    static std::vector<AppDetailInfo> get_installed_apps(bool force_refresh = false);
    static AppDetailInfo get_app_details(const std::string& desktop_id);
    static bool launch_app(const AppDetailInfo& app);
    static bool uninstall_app(const AppDetailInfo& app, std::string& error_msg);

    static std::vector<DefaultRoleSpec> get_default_roles(bool force_refresh = false);
    static DefaultRoleSpec get_default_role(const std::string& role_id);
    static bool set_default_for_role(const std::string& role_id, const std::string& desktop_id);

    static std::string run_cmd(const std::string& cmd);
    static std::string resolve_desktop_path(const std::string& desktop_id);

    static void populate_package_metadata(AppDetailInfo& app);

private:
    static std::vector<std::string> parse_desktop_mimes(const std::string& desktop_path);
    static std::string query_xdg_default(const std::string& mime_type);
};

} // namespace miqu
