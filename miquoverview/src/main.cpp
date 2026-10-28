#include "overview_config.hpp"

#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <glob.h>

static void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n\n"
              << "Options:\n"
              << "  --init-config    Generate user configuration (~/.config/miquoverview/miquoverview.conf)\n"
              << "                   with safe timestamped archiving of existing config\n"
              << "  --toggle         Toggle overview via miquland IPC\n"
              << "  --open           Open overview via miquland IPC\n"
              << "  --close          Close overview via miquland IPC\n"
              << "  -h, --help       Show this help message\n"
              << "  -v, --version    Show version\n";
}

static bool send_ipc_command(const std::string& cmd) {
    const char* runtime_dir = getenv("XDG_RUNTIME_DIR");
    const char* wayland_display = getenv("WAYLAND_DISPLAY");
    std::string sock_path;

    if (runtime_dir && wayland_display) {
        std::string cand = std::string(runtime_dir) + "/miquland-" + wayland_display + ".sock";
        if (access(cand.c_str(), F_OK) == 0) {
            sock_path = cand;
        }
    }

    if (sock_path.empty() && runtime_dir) {
        glob_t g;
        if (glob((std::string(runtime_dir) + "/miquland-*.sock").c_str(), GLOB_NOSORT, nullptr, &g) == 0) {
            if (g.gl_pathc > 0) sock_path = g.gl_pathv[0];
            globfree(&g);
        }
    }

    if (sock_path.empty()) {
        std::cerr << "[miquoverview] Could not locate active miquland IPC socket\n";
        return false;
    }

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        std::cerr << "[miquoverview] Failed to create socket\n";
        return false;
    }

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        close(fd);
        std::cerr << "[miquoverview] Failed to connect to miquland IPC socket: " << sock_path << "\n";
        return false;
    }

    // IPC command format: "dispatch <cmd>\n"
    std::string msg = "dispatch " + cmd + "\n";
    ssize_t ret = write(fd, msg.data(), msg.size());
    close(fd);
    return ret == static_cast<ssize_t>(msg.size());
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 0;
    }

    std::string arg = argv[1];
    if (arg == "--init-config") {
        miquoverview::OverviewConfig::init_user_config();
        return 0;
    } else if (arg == "--toggle") {
        return send_ipc_command("overview_toggle") ? 0 : 1;
    } else if (arg == "--open") {
        return send_ipc_command("overview_open") ? 0 : 1;
    } else if (arg == "--close") {
        return send_ipc_command("overview_close") ? 0 : 1;
    } else if (arg == "-h" || arg == "--help") {
        print_usage(argv[0]);
        return 0;
    } else if (arg == "-v" || arg == "--version") {
        std::cout << "miquoverview 1.2.0 (Miqu Desktop Ecosystem)\n";
        return 0;
    }

    std::cerr << "Unknown option: " << arg << "\n";
    print_usage(argv[0]);
    return 1;
}
