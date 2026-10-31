#include "core/system_power.hpp"
#include "config/power_config.hpp"

#include <iostream>
#include <sstream>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <poll.h>
#include <glob.h>
#include <thread>
#include <chrono>

namespace miqu {

namespace {

constexpr char IPC_MAGIC[4] = {'M', 'I', 'Q', 'U'};

#pragma pack(push, 1)
struct IpcHeader {
    char magic[4];
    uint32_t type;
    uint32_t payload_len;
};
#pragma pack(pop)

std::string extract_json_string(const std::string& obj, const std::string& key) {
    std::string needle = "\"" + key + "\":\"";
    size_t start = obj.find(needle);
    if (start == std::string::npos) return "";
    start += needle.length();

    std::string result;
    bool escape = false;
    for (size_t i = start; i < obj.length(); ++i) {
        char c = obj[i];
        if (escape) {
            result += c;
            escape = false;
        } else if (c == '\\') {
            escape = true;
        } else if (c == '"') {
            break;
        } else {
            result += c;
        }
    }
    return result;
}

uint64_t extract_json_uint(const std::string& obj, const std::string& key, uint64_t fallback = 0) {
    std::string needle = "\"" + key + "\":";
    size_t start = obj.find(needle);
    if (start == std::string::npos) return fallback;
    start += needle.length();
    while (start < obj.length() && (obj[start] == ' ' || obj[start] == '\t')) start++;
    size_t end = obj.find_first_of(",}\n", start);
    if (end == std::string::npos) end = obj.length();
    try {
        return std::stoull(obj.substr(start, end - start));
    } catch (...) {
        return fallback;
    }
}

} // namespace

SystemPower& SystemPower::instance() {
    static SystemPower s_inst;
    return s_inst;
}

std::string SystemPower::find_socket_path() {
    const char* env_socket = getenv("MIQULAND_SOCKET");
    if (env_socket && env_socket[0] != '\0' && access(env_socket, F_OK) == 0) {
        return env_socket;
    }

    const char* runtime_dir = getenv("XDG_RUNTIME_DIR");
    const char* wayland_display = getenv("WAYLAND_DISPLAY");

    if (runtime_dir && wayland_display) {
        std::string candidate = std::string(runtime_dir) + "/miquland-" + wayland_display + ".sock";
        if (access(candidate.c_str(), F_OK) == 0) {
            return candidate;
        }
    }

    std::vector<std::string> patterns;
    if (runtime_dir) {
        patterns.push_back(std::string(runtime_dir) + "/miquland-*.sock");
    }
    patterns.push_back("/tmp/miquland-" + std::to_string(getuid()) + "-*.sock");

    for (const auto& pattern : patterns) {
        glob_t glob_res;
        if (glob(pattern.c_str(), GLOB_NOSORT, nullptr, &glob_res) == 0) {
            if (glob_res.gl_pathc > 0) {
                std::string found = glob_res.gl_pathv[0];
                globfree(&glob_res);
                return found;
            }
            globfree(&glob_res);
        }
    }

    return "";
}

bool SystemPower::is_compositor_available() {
    return !find_socket_path().empty();
}

bool SystemPower::send_ipc_command(const std::string& cmd, std::string& response) {
    std::string sock_path = find_socket_path();
    if (sock_path.empty()) return false;

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return false;

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, sock_path.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(fd);
        return false;
    }

    IpcHeader hdr{};
    std::memcpy(hdr.magic, IPC_MAGIC, 4);
    hdr.type = 0; // Command
    hdr.payload_len = static_cast<uint32_t>(cmd.length());

    if (write(fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
        close(fd);
        return false;
    }

    if (!cmd.empty()) {
        if (write(fd, cmd.data(), cmd.length()) != static_cast<ssize_t>(cmd.length())) {
            close(fd);
            return false;
        }
    }

    // Wait for response header
    IpcHeader resp_hdr{};
    ssize_t n = read(fd, &resp_hdr, sizeof(resp_hdr));
    if (n != sizeof(resp_hdr) || std::memcmp(resp_hdr.magic, IPC_MAGIC, 4) != 0) {
        close(fd);
        return false;
    }

    response.clear();
    if (resp_hdr.payload_len > 0) {
        std::vector<char> buf(resp_hdr.payload_len);
        size_t total_read = 0;
        while (total_read < resp_hdr.payload_len) {
            ssize_t chunk = read(fd, buf.data() + total_read, resp_hdr.payload_len - total_read);
            if (chunk <= 0) break;
            total_read += chunk;
        }
        response.assign(buf.data(), total_read);
    }

    close(fd);
    return true;
}

std::vector<AppInfo> SystemPower::get_running_apps() {
    std::vector<AppInfo> apps;
    std::string response;
    if (!send_ipc_command("clients", response)) {
        return apps;
    }

    // Parse array of JSON objects
    size_t pos = 0;
    while ((pos = response.find('{', pos)) != std::string::npos) {
        size_t end = response.find('}', pos);
        if (end == std::string::npos) break;

        std::string obj = response.substr(pos, end - pos + 1);
        AppInfo info;
        info.id = extract_json_uint(obj, "id");
        info.title = extract_json_string(obj, "title");
        info.app_id = extract_json_string(obj, "app_id");
        info.workspace_id = static_cast<size_t>(extract_json_uint(obj, "workspace_id", 1));

        if (!info.title.empty() || !info.app_id.empty()) {
            apps.push_back(info);
        }

        pos = end + 1;
    }

    return apps;
}

void SystemPower::run_command(const std::string& cmd) {
    if (cmd.empty()) return;
    int ret = system(cmd.c_str());
    (void)ret;
}

void SystemPower::execute_graceful_teardown(
    uint32_t timeout_ms,
    const std::string& post_cmd,
    std::function<void(const std::string& status, const std::string& subtitle)> on_status_update,
    std::function<void()> on_finished
) {
    std::thread([this, timeout_ms, post_cmd, on_status_update, on_finished]() {
        if (on_status_update) {
            on_status_update("Closing Applications...", "Gracefully saving session and closing open windows");
        }

        std::string sock_path = find_socket_path();
        if (!sock_path.empty()) {
            int fd = socket(AF_UNIX, SOCK_STREAM, 0);
            if (fd >= 0) {
                struct sockaddr_un addr{};
                addr.sun_family = AF_UNIX;
                std::strncpy(addr.sun_path, sock_path.c_str(), sizeof(addr.sun_path) - 1);

                if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0) {
                    std::string req = "shutdown " + std::to_string(timeout_ms);
                    IpcHeader hdr{};
                    std::memcpy(hdr.magic, IPC_MAGIC, 4);
                    hdr.type = 0;
                    hdr.payload_len = static_cast<uint32_t>(req.length());

                    write(fd, &hdr, sizeof(hdr));
                    write(fd, req.data(), req.length());

                    // Wait for server to finish graceful teardown
                    char dummy[256];
                    while (read(fd, dummy, sizeof(dummy)) > 0) {}
                }
                close(fd);
            }
        }

        if (on_status_update) {
            on_status_update("Teardown Complete", "Proceeding with system power transition...");
        }

        if (!post_cmd.empty()) {
            run_command(post_cmd);
        }

        if (on_finished) {
            on_finished();
        }
    }).detach();
}

void SystemPower::execute_action(
    PowerAction action,
    std::function<void(const std::string& status, const std::string& subtitle)> on_status_update,
    std::function<void()> on_finished
) {
    const auto& cfg = PowerConfig::get();

    switch (action) {
        case PowerAction::Shutdown: {
            std::string post = cfg.shutdown_cmd.empty() ? "systemctl poweroff" : cfg.shutdown_cmd;
            if (on_status_update) {
                on_status_update("Shutting Down Desktop", "Closing applications and preparing to power off...");
            }
            execute_graceful_teardown(cfg.graceful_timeout_ms, post, on_status_update, on_finished);
            break;
        }
        case PowerAction::Reboot: {
            std::string post = cfg.reboot_cmd.empty() ? "systemctl reboot" : cfg.reboot_cmd;
            if (on_status_update) {
                on_status_update("Restarting System", "Closing applications and preparing to restart...");
            }
            execute_graceful_teardown(cfg.graceful_timeout_ms, post, on_status_update, on_finished);
            break;
        }
        case PowerAction::Logout: {
            std::string post = cfg.logout_cmd; // If empty, graceful shutdown closes miquland which terminates session
            if (on_status_update) {
                on_status_update("Logging Out", "Closing applications and terminating session...");
            }
            execute_graceful_teardown(cfg.graceful_timeout_ms, post, on_status_update, on_finished);
            break;
        }
        case PowerAction::Lock: {
            std::string cmd = cfg.lock_cmd.empty() ? "miqulock" : cfg.lock_cmd;
            run_command(cmd + " &");
            if (on_finished) on_finished();
            break;
        }
        case PowerAction::Suspend: {
            if (cfg.lock_before_suspend) {
                std::string lock_cmd = cfg.lock_cmd.empty() ? "miqulock" : cfg.lock_cmd;
                run_command(lock_cmd + " &");
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
            std::string cmd = cfg.suspend_cmd.empty() ? "systemctl suspend" : cfg.suspend_cmd;
            run_command(cmd);
            if (on_finished) on_finished();
            break;
        }
        case PowerAction::Hibernate: {
            if (cfg.lock_before_suspend) {
                std::string lock_cmd = cfg.lock_cmd.empty() ? "miqulock" : cfg.lock_cmd;
                run_command(lock_cmd + " &");
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
            std::string cmd = cfg.hibernate_cmd.empty() ? "systemctl hibernate" : cfg.hibernate_cmd;
            run_command(cmd);
            if (on_finished) on_finished();
            break;
        }
    }
}

} // namespace miqu
