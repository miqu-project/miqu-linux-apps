#include "session.hpp"
#include <unistd.h>
#include <sys/wait.h>
#include <grp.h>
#include <pwd.h>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <cstring>
#include <filesystem>

namespace miqudm {

SessionLauncher::~SessionLauncher() {
    terminate();
}

bool SessionLauncher::launch(const UserInfo& user,
                             const SessionInfo& session,
                             const std::vector<std::string>& pam_envs,
                             int vt_number)
{
    if (m_session_pid > 0 && is_running()) {
        std::cerr << "[miqudm-session] A session is already active (PID " << m_session_pid << ")" << std::endl;
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "[miqudm-session] fork() failed: " << strerror(errno) << std::endl;
        return false;
    }

    if (pid == 0) {
        // Child Process: Setup session environment & drop privileges
        setsid();

        // 1. Drop privileges to user
        if (initgroups(user.username.c_str(), user.gid) != 0) {
            std::cerr << "[miqudm-session] initgroups failed: " << strerror(errno) << std::endl;
            _exit(1);
        }

        if (setgid(user.gid) != 0) {
            std::cerr << "[miqudm-session] setgid failed: " << strerror(errno) << std::endl;
            _exit(1);
        }

        if (setuid(user.uid) != 0) {
            std::cerr << "[miqudm-session] setuid failed: " << strerror(errno) << std::endl;
            _exit(1);
        }

        // 2. Setup standard user environment
        clearenv();

        setenv("HOME", user.home_dir.c_str(), 1);
        setenv("USER", user.username.c_str(), 1);
        setenv("LOGNAME", user.username.c_str(), 1);
        setenv("SHELL", user.shell.c_str(), 1);
        setenv("PATH", "/usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin", 1);
        setenv("XDG_DATA_DIRS", "/usr/local/share:/usr/share", 1);
        setenv("XDG_CONFIG_DIRS", "/etc/xdg", 1);

        // Wayland session identification
        setenv("XDG_SESSION_TYPE", "wayland", 1);
        setenv("XDG_SESSION_CLASS", "user", 1);
        setenv("XDG_SEAT", "seat0", 1);
        setenv("XDG_VTNR", std::to_string(vt_number).c_str(), 1);

        if (!session.desktop_names.empty()) {
            setenv("XDG_CURRENT_DESKTOP", session.desktop_names.c_str(), 1);
            setenv("XDG_SESSION_DESKTOP", session.desktop_names.c_str(), 1);
        } else {
            setenv("XDG_CURRENT_DESKTOP", session.name.c_str(), 1);
            setenv("XDG_SESSION_DESKTOP", session.name.c_str(), 1);
        }

        // Standard user runtime dir
        std::string runtime_dir = "/run/user/" + std::to_string(user.uid);
        setenv("XDG_RUNTIME_DIR", runtime_dir.c_str(), 1);

        // Standard D-Bus session bus address
        std::string dbus_addr = "unix:path=" + runtime_dir + "/bus";
        setenv("DBUS_SESSION_BUS_ADDRESS", dbus_addr.c_str(), 1);

        // 3. Apply PAM environment variables
        for (const auto& env_entry : pam_envs) {
            auto eq = env_entry.find('=');
            if (eq != std::string::npos) {
                std::string k = env_entry.substr(0, eq);
                std::string v = env_entry.substr(eq + 1);
                setenv(k.c_str(), v.c_str(), 1);
            }
        }

        // 4. Working directory
        std::error_code ec;
        if (!user.home_dir.empty() && std::filesystem::exists(user.home_dir, ec)) {
            chdir(user.home_dir.c_str());
        } else {
            chdir("/");
        }

        std::cout << "[miqudm-session] Launching session command: " << session.exec << std::endl;

        // 5. Execute user session via wayland-session wrapper or login shell
        if (std::filesystem::exists("/usr/lib/miqudm/wayland-session", ec)) {
            execl("/usr/lib/miqudm/wayland-session", "wayland-session", "/bin/sh", "-c", session.exec.c_str(), nullptr);
        }

        execl(user.shell.c_str(), user.shell.c_str(), "-l", "-c", session.exec.c_str(), nullptr);
        execl("/bin/sh", "sh", "-l", "-c", session.exec.c_str(), nullptr);

        std::cerr << "[miqudm-session] execl failed: " << strerror(errno) << std::endl;
        _exit(127);
    }

    // Parent process
    m_session_pid = pid;
    std::cout << "[miqudm-session] Started user session for " << user.username
              << " (" << session.name << ") with PID " << m_session_pid << std::endl;
    return true;
}

bool SessionLauncher::is_running() const {
    if (m_session_pid <= 0) return false;
    int status = 0;
    pid_t res = waitpid(m_session_pid, &status, WNOHANG);
    if (res == 0) return true; // Still running
    return false;
}

int SessionLauncher::wait_for_exit() {
    if (m_session_pid <= 0) return 0;

    int status = 0;
    while (true) {
        pid_t res = waitpid(m_session_pid, &status, 0);
        if (res == m_session_pid) {
            m_session_pid = -1;
            if (WIFEXITED(status)) {
                return WEXITSTATUS(status);
            }
            if (WIFSIGNALED(status)) {
                return 128 + WTERMSIG(status);
            }
            return status;
        } else if (res < 0 && errno != EINTR) {
            m_session_pid = -1;
            return -1;
        }
    }
}

void SessionLauncher::terminate() {
    if (m_session_pid > 0) {
        std::cout << "[miqudm-session] Sending SIGTERM to session PID " << m_session_pid << std::endl;
        kill(-m_session_pid, SIGTERM);
        kill(m_session_pid, SIGTERM);

        // Give process up to 2 seconds to gracefully exit before SIGKILL
        for (int i = 0; i < 20; ++i) {
            if (!is_running()) {
                m_session_pid = -1;
                return;
            }
            usleep(100000); // 100ms
        }

        if (is_running()) {
            std::cout << "[miqudm-session] Sending SIGKILL to session PID " << m_session_pid << std::endl;
            kill(-m_session_pid, SIGKILL);
            kill(m_session_pid, SIGKILL);
            waitpid(m_session_pid, nullptr, 0);
        }
        m_session_pid = -1;
    }
}

} // namespace miqudm
