#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>

namespace miqu {

struct AppInfo {
    uint64_t id = 0;
    std::string title;
    std::string app_id;
    size_t workspace_id = 0;
};

enum class PowerAction {
    Shutdown,
    Reboot,
    Suspend,
    Hibernate,
    Lock,
    Logout
};

class SystemPower {
public:
    static SystemPower& instance();

    // Query running client windows from miquland
    std::vector<AppInfo> get_running_apps();

    // Checks if compositor is reachable
    bool is_compositor_available();

    // Executes a power action
    // If on_status_update is provided, it is invoked during graceful shutdown phases
    void execute_action(
        PowerAction action,
        std::function<void(const std::string& status, const std::string& subtitle)> on_status_update,
        std::function<void()> on_finished
    );

private:
    SystemPower() = default;

    std::string find_socket_path();
    bool send_ipc_command(const std::string& cmd, std::string& response);
    void run_command(const std::string& cmd);
    void execute_graceful_teardown(
        uint32_t timeout_ms,
        const std::string& post_cmd,
        std::function<void(const std::string& status, const std::string& subtitle)> on_status_update,
        std::function<void()> on_finished
    );
};

} // namespace miqu
