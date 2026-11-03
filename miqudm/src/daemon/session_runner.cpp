#include "session_runner.hpp"
#include <unistd.h>
#include <grp.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <cstring>
#include <iostream>
#include <filesystem>

namespace miqudm {

namespace fs = std::filesystem;

pid_t SessionRunner::launch_user_session(const UserInfo& user,
                                         const SessionInfo& session,
                                         std::unique_ptr<PamService> pam,
                                         int vt_number) {
    if (!pam) {
        std::cerr << "[miqudm-session] No PAM handle provided for session launch" << std::endl;
        return -1;
    }

    pid_t worker_pid = fork();
    if (worker_pid < 0) {
        std::cerr << "[miqudm-session] Failed to fork session worker: " << strerror(errno) << std::endl;
        return -1;
    }

    if (worker_pid == 0) {
        // Child Process: Session Worker (runs as root to supervise session & PAM lifecycle)
        setsid();

        // 1. Configure PAM items for user session
        std::string user_tty = "tty" + std::to_string(vt_number);
        pam->set_item(PAM_TTY, user_tty.c_str());
        pam->set_env("XDG_SEAT", "seat0");
        pam->set_env("XDG_VTNR", std::to_string(vt_number));
        pam->set_env("XDG_SESSION_CLASS", "user");
        pam->set_env("XDG_SESSION_TYPE", session.is_wayland ? "wayland" : "x11");
        if (!session.desktop_names.empty()) {
            pam->set_env("XDG_CURRENT_DESKTOP", session.desktop_names);
            pam->set_env("XDG_SESSION_DESKTOP", session.desktop_names);
        }

        // Establish credentials in session worker
        pam->set_credentials(PAM_ESTABLISH_CRED);

        // Open PAM session (registers session with logind via pam_systemd)
        if (!pam->open_session()) {
            std::cerr << "[miqudm-session] Failed to open PAM session: " << pam->get_last_error() << std::endl;
            pam->set_credentials(PAM_DELETE_CRED);
            pam->end();
            _exit(1);
        }

        // 2. Fork desktop application process
        pid_t app_pid = fork();
        if (app_pid < 0) {
            std::cerr << "[miqudm-session] Failed to fork desktop app: " << strerror(errno) << std::endl;
            pam->close_session();
            pam->set_credentials(PAM_DELETE_CRED);
            pam->end();
            _exit(1);
        }

        if (app_pid == 0) {
            // Desktop process: Drop privileges to user and exec session
            if (initgroups(user.username.c_str(), user.gid) != 0) {
                std::cerr << "[miqudm-session] initgroups failed: " << strerror(errno) << std::endl;
            }
            if (setgid(user.gid) != 0) {
                std::cerr << "[miqudm-session] setgid failed: " << strerror(errno) << std::endl;
                _exit(1);
            }
            if (setuid(user.uid) != 0) {
                std::cerr << "[miqudm-session] setuid failed: " << strerror(errno) << std::endl;
                _exit(1);
            }

            // Environment setup
            clearenv();
            setenv("PATH", "/usr/local/bin:/usr/bin:/bin", 1);
            setenv("USER", user.username.c_str(), 1);
            setenv("LOGNAME", user.username.c_str(), 1);
            setenv("HOME", user.home_dir.c_str(), 1);
            setenv("SHELL", user.shell.c_str(), 1);

            std::string runtime_dir = "/run/user/" + std::to_string(user.uid);
            setenv("XDG_RUNTIME_DIR", runtime_dir.c_str(), 1);
            setenv("XDG_SESSION_TYPE", session.is_wayland ? "wayland" : "x11", 1);
            setenv("XDG_SESSION_CLASS", "user", 1);
            setenv("XDG_SEAT", "seat0", 1);
            if (vt_number > 0) {
                setenv("XDG_VTNR", std::to_string(vt_number).c_str(), 1);
            }
            if (!session.desktop_names.empty()) {
                setenv("XDG_CURRENT_DESKTOP", session.desktop_names.c_str(), 1);
                setenv("XDG_SESSION_DESKTOP", session.desktop_names.c_str(), 1);
            }

            // Apply all PAM environment variables (including XDG_SESSION_ID)
            for (const auto& env : pam->get_env_list()) {
                auto eq = env.find('=');
                if (eq != std::string::npos) {
                    std::string k = env.substr(0, eq);
                    std::string v = env.substr(eq + 1);
                    setenv(k.c_str(), v.c_str(), 1);
                }
            }

            std::string dbus_socket = runtime_dir + "/bus";
            setenv("DBUS_SESSION_BUS_ADDRESS", ("unix:path=" + dbus_socket).c_str(), 1);

            // Working directory
            std::error_code ec;
            if (!user.home_dir.empty() && fs::exists(user.home_dir, ec)) {
                chdir(user.home_dir.c_str());
            } else {
                chdir("/tmp");
            }

            // Exec session command
            std::string session_wrapper = "/usr/lib/miqudm/wayland-session";
            if (fs::exists(session_wrapper, ec)) {
                execl(session_wrapper.c_str(), session_wrapper.c_str(), "/bin/sh", "-c", session.exec.c_str(), nullptr);
            }
            execl("/bin/sh", "sh", "-c", session.exec.c_str(), nullptr);
            std::cerr << "[miqudm-session] execl failed: " << strerror(errno) << std::endl;
            _exit(127);
        }

        // Session Worker: Wait for desktop process to terminate
        int status = 0;
        waitpid(app_pid, &status, 0);

        std::cout << "[miqudm-session] User session exited, closing PAM session..." << std::endl;
        pam->close_session();
        pam->set_credentials(PAM_DELETE_CRED);
        pam->end();

        _exit(WIFEXITED(status) ? WEXITSTATUS(status) : 0);
    }

    // In Parent Daemon: release parent's pam reference
    pam.reset();

    return worker_pid;
}

} // namespace miqudm
