#include "daemon.hpp"
#include "logind.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <csignal>
#include <iostream>
#include <filesystem>
#include <cstring>
#include <algorithm>

namespace miqudm {

Daemon::Daemon() = default;

Daemon::~Daemon() {
    stop();
}

bool Daemon::setup_ipc_socket() {
    m_socket_path = get_socket_path();

    // Ensure parent directory exists
    std::filesystem::path p(m_socket_path);
    std::error_code ec;
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path(), ec);
        // If /run/miqudm, allow full permissions for greeter user
        if (p.parent_path() == "/run/miqudm") {
            chmod("/run/miqudm", 0755);
        }
    }

    unlink(m_socket_path.c_str());

    m_server_socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_server_socket_fd < 0) {
        std::cerr << "[miqudm-daemon] Failed to create unix socket: " << strerror(errno) << std::endl;
        return false;
    }

    // Set non-blocking
    int flags = fcntl(m_server_socket_fd, F_GETFL, 0);
    fcntl(m_server_socket_fd, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, m_socket_path.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(m_server_socket_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "[miqudm-daemon] Failed to bind unix socket " << m_socket_path << ": " << strerror(errno) << std::endl;
        close(m_server_socket_fd);
        m_server_socket_fd = -1;
        return false;
    }

    // Allow permissions for greeter user to connect
    chmod(m_socket_path.c_str(), 0666);

    if (listen(m_server_socket_fd, 10) < 0) {
        std::cerr << "[miqudm-daemon] Failed to listen on socket: " << strerror(errno) << std::endl;
        close(m_server_socket_fd);
        m_server_socket_fd = -1;
        return false;
    }

    std::cout << "[miqudm-daemon] IPC Socket listening at " << m_socket_path << std::endl;
    return true;
}

void Daemon::cleanup_ipc_socket() {
    if (m_server_socket_fd >= 0) {
        close(m_server_socket_fd);
        m_server_socket_fd = -1;
    }
    if (!m_socket_path.empty()) {
        unlink(m_socket_path.c_str());
    }
}

bool Daemon::init() {
    std::cout << "[miqudm-daemon] Initializing miqudm display manager..." << std::endl;

    // Load configuration
    Config::get().load();

    // Cache users and sessions
    m_cached_users = UserScanner::scan_users();
    m_cached_sessions = SessionScanner::scan_all();

    std::cout << "[miqudm-daemon] Found " << m_cached_users.size() << " users and "
              << m_cached_sessions.size() << " Wayland sessions." << std::endl;

    // Setup Logind Manager
    LogindManager::get().init();

    // Setup IPC socket
    if (!setup_ipc_socket()) {
        return false;
    }

    return true;
}

#include <pwd.h>
#include <grp.h>

void Daemon::start_greeter() {
    if (m_greeter_pid > 0) {
        return; // Greeter already active
    }

    const auto& cmd = Config::get().general.greeter_command;
    if (cmd.empty()) {
        std::cerr << "[miqudm-daemon] No greeter command configured!" << std::endl;
        return;
    }

    // Resolve greeter user
    std::string greeter_user = Config::get().general.greeter_user;
    struct passwd* pw = nullptr;
    if (!greeter_user.empty()) {
        pw = getpwnam(greeter_user.c_str());
    }

    if (!pw) {
        const std::vector<std::string> fallbacks = {"sddm", "greeter", "miqudm", "nobody"};
        for (const auto& candidate : fallbacks) {
            pw = getpwnam(candidate.c_str());
            if (pw) {
                greeter_user = candidate;
                break;
            }
        }
    }

    // Initialize PAM session for greeter (registers session with logind via pam_systemd)
    m_greeter_pam = std::make_unique<PamAuth>();
    std::string service_name = "miqudm-greeter";
    std::string user_name = pw ? pw->pw_name : (greeter_user.empty() ? "sddm" : greeter_user);

    if (m_greeter_pam->start(user_name, service_name)) {
        m_greeter_pam->set_env("XDG_SESSION_CLASS", "greeter");
        m_greeter_pam->set_env("XDG_SESSION_TYPE", "wayland");
        m_greeter_pam->set_env("XDG_SEAT", "seat0");
        m_greeter_pam->set_env("XDG_VTNR", std::to_string(Config::get().general.vt));
        m_greeter_pam->establish_credentials();
        m_greeter_pam->open_session();
    } else {
        std::cerr << "[miqudm-daemon] Warning: Failed to initialize PAM for greeter: "
                  << m_greeter_pam->get_error_message() << std::endl;
    }

    // Ensure XDG_RUNTIME_DIR exists and has correct permissions
    std::string runtime_dir;
    if (pw) {
        runtime_dir = "/run/user/" + std::to_string(pw->pw_uid);
        std::error_code ec;
        if (!std::filesystem::exists(runtime_dir, ec)) {
            std::filesystem::create_directories(runtime_dir, ec);
        }
        if (std::filesystem::exists(runtime_dir, ec)) {
            chmod(runtime_dir.c_str(), 0700);
            chown(runtime_dir.c_str(), pw->pw_uid, pw->pw_gid);
        }
    } else {
        runtime_dir = "/run/miqudm/runtime";
        std::error_code ec;
        std::filesystem::create_directories(runtime_dir, ec);
        chmod(runtime_dir.c_str(), 0700);
    }

    std::cout << "[miqudm-daemon] Starting greeter compositor (" << user_name << "): " << cmd << std::endl;

    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "[miqudm-daemon] Failed to fork greeter process: " << strerror(errno) << std::endl;
        return;
    }

    if (pid == 0) {
        // Child: Execute greeter compositor command
        setsid();

        if (pw && pw->pw_uid != 0) {
            if (initgroups(pw->pw_name, pw->pw_gid) != 0) {
                std::cerr << "[miqudm-daemon] Greeter initgroups failed: " << strerror(errno) << std::endl;
            }
            if (setgid(pw->pw_gid) != 0) {
                std::cerr << "[miqudm-daemon] Greeter setgid failed: " << strerror(errno) << std::endl;
            }
            if (setuid(pw->pw_uid) != 0) {
                std::cerr << "[miqudm-daemon] Greeter setuid failed: " << strerror(errno) << std::endl;
            }
        }

        clearenv();

        if (pw) {
            setenv("HOME", pw->pw_dir, 1);
            setenv("USER", pw->pw_name, 1);
            setenv("LOGNAME", pw->pw_name, 1);
            setenv("SHELL", pw->pw_shell, 1);
        }
        setenv("PATH", "/usr/local/bin:/usr/bin:/bin", 1);
        setenv("XDG_SESSION_CLASS", "greeter", 1);
        setenv("XDG_SESSION_TYPE", "wayland", 1);
        setenv("XDG_SEAT", "seat0", 1);
        setenv("XDG_VTNR", std::to_string(Config::get().general.vt).c_str(), 1);
        setenv("XDG_RUNTIME_DIR", runtime_dir.c_str(), 1);

        if (m_greeter_pam) {
            for (const auto& env : m_greeter_pam->get_environment_list()) {
                auto eq = env.find('=');
                if (eq != std::string::npos) {
                    std::string k = env.substr(0, eq);
                    std::string v = env.substr(eq + 1);
                    setenv(k.c_str(), v.c_str(), 1);
                }
            }
        }

        if (pw && pw->pw_dir && *pw->pw_dir) {
            std::error_code ec;
            if (std::filesystem::exists(pw->pw_dir, ec)) {
                chdir(pw->pw_dir);
            } else {
                chdir("/tmp");
            }
        } else {
            chdir("/tmp");
        }

        execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
        std::cerr << "[miqudm-daemon] Failed to execute greeter command: " << strerror(errno) << std::endl;
        _exit(127);
    }

    m_greeter_pid = pid;
    std::cout << "[miqudm-daemon] Greeter running with PID " << m_greeter_pid << std::endl;
}

void Daemon::stop_greeter() {
    if (m_greeter_pid > 0) {
        std::cout << "[miqudm-daemon] Stopping greeter (PID " << m_greeter_pid << ")..." << std::endl;
        kill(-m_greeter_pid, SIGTERM);
        kill(m_greeter_pid, SIGTERM);

        for (int i = 0; i < 20; ++i) {
            int status = 0;
            pid_t res = waitpid(m_greeter_pid, &status, WNOHANG);
            if (res == m_greeter_pid || res < 0) {
                m_greeter_pid = -1;
                break;
            }
            usleep(100000); // 100ms
        }

        if (m_greeter_pid > 0) {
            kill(-m_greeter_pid, SIGKILL);
            kill(m_greeter_pid, SIGKILL);
            waitpid(m_greeter_pid, nullptr, 0);
            m_greeter_pid = -1;
        }
    }

    if (m_greeter_pam) {
        m_greeter_pam->close_session();
        m_greeter_pam->delete_credentials();
        m_greeter_pam->end();
        m_greeter_pam.reset();
    }

    // Give kernel DRM driver and logind time to release VT and device locks
    usleep(200000); // 200ms
}

void Daemon::run() {
    m_running = true;

    // Launch initial greeter
    start_greeter();

    std::vector<struct pollfd> poll_fds;

    while (m_running) {
        poll_fds.clear();

        // 1. Server socket
        struct pollfd pfd = {};
        pfd.fd = m_server_socket_fd;
        pfd.events = POLLIN;
        poll_fds.push_back(pfd);

        int ret = poll(poll_fds.data(), poll_fds.size(), 500); // 500ms timeout
        if (ret < 0) {
            if (errno == EINTR) continue;
            std::cerr << "[miqudm-daemon] poll error: " << strerror(errno) << std::endl;
            break;
        }

        // Check if server socket has incoming client connection
        if (poll_fds[0].revents & POLLIN) {
            int client_fd = accept(m_server_socket_fd, nullptr, nullptr);
            if (client_fd >= 0) {
                handle_client_connection(client_fd);
                close(client_fd);
            }
        }

        // Check if greeter died unexpectedly
        if (m_greeter_pid > 0 && !m_session_launcher.is_running()) {
            int status = 0;
            pid_t res = waitpid(m_greeter_pid, &status, WNOHANG);
            if (res == m_greeter_pid) {
                std::cout << "[miqudm-daemon] Greeter exited (code " << WEXITSTATUS(status) << "), restarting..." << std::endl;
                m_greeter_pid = -1;
                sleep(1);
                start_greeter();
            }
        }
    }

    stop_greeter();
    m_session_launcher.terminate();
    cleanup_ipc_socket();
}

void Daemon::stop() {
    m_running = false;
}

void Daemon::handle_client_connection(int client_fd) {
    char buffer[8192];
    std::string accumulated;

    while (true) {
        ssize_t bytes = read(client_fd, buffer, sizeof(buffer) - 1);
        if (bytes <= 0) break;

        buffer[bytes] = '\0';
        accumulated += buffer;

        auto newline_pos = accumulated.find('\n');
        if (newline_pos != std::string::npos) {
            std::string line = accumulated.substr(0, newline_pos);
            accumulated.erase(0, newline_pos + 1);

            IpcMessage req = IpcMessage::deserialize(line);
            process_message(client_fd, req);
            break;
        }
    }
}

void Daemon::process_message(int client_fd, const IpcMessage& req) {
    switch (req.type) {
        case IpcMessageType::Ping: {
            IpcMessage resp;
            resp.type = IpcMessageType::Response;
            resp.success = true;
            resp.message = "pong";
            std::string s = resp.serialize();
            write(client_fd, s.data(), s.size());
            break;
        }
        case IpcMessageType::GetUsers: {
            m_cached_users = UserScanner::scan_users();
            IpcMessage resp;
            resp.type = IpcMessageType::Response;
            resp.success = true;
            resp.users = m_cached_users;
            std::string s = resp.serialize();
            write(client_fd, s.data(), s.size());
            break;
        }
        case IpcMessageType::GetSessions: {
            m_cached_sessions = SessionScanner::scan_all();
            IpcMessage resp;
            resp.type = IpcMessageType::Response;
            resp.success = true;
            resp.sessions = m_cached_sessions;
            std::string s = resp.serialize();
            write(client_fd, s.data(), s.size());
            break;
        }
        case IpcMessageType::PowerAction: {
            handle_power_action(client_fd, req);
            break;
        }
        case IpcMessageType::Login: {
            handle_login_request(client_fd, req);
            break;
        }
        default: {
            IpcMessage resp;
            resp.type = IpcMessageType::Response;
            resp.success = false;
            resp.message = "Unknown IPC command";
            std::string s = resp.serialize();
            write(client_fd, s.data(), s.size());
            break;
        }
    }
}

void Daemon::handle_power_action(int client_fd, const IpcMessage& req) {
    bool ok = false;
    if (req.power_action == "poweroff") {
        ok = LogindManager::get().power_off();
    } else if (req.power_action == "reboot") {
        ok = LogindManager::get().reboot();
    } else if (req.power_action == "suspend") {
        ok = LogindManager::get().suspend();
    } else if (req.power_action == "hibernate") {
        ok = LogindManager::get().hibernate();
    }

    IpcMessage resp;
    resp.type = IpcMessageType::Response;
    resp.success = ok;
    resp.message = ok ? "Power action triggered" : "Failed to execute power action";
    std::string s = resp.serialize();
    write(client_fd, s.data(), s.size());
}

void Daemon::handle_login_request(int client_fd, const IpcMessage& req) {
    std::cout << "[miqudm-daemon] Received login request for user: " << req.username << std::endl;

    auto pam = std::make_unique<PamAuth>();

    if (!pam->start(req.username, "miqudm")) {
        IpcMessage resp;
        resp.type = IpcMessageType::Response;
        resp.success = false;
        resp.message = "PAM initialization error: " + pam->get_error_message();
        std::string s = resp.serialize();
        write(client_fd, s.data(), s.size());
        return;
    }

    if (!pam->authenticate(req.password)) {
        IpcMessage resp;
        resp.type = IpcMessageType::Response;
        resp.success = false;
        resp.message = "Authentication failed: " + pam->get_error_message();
        std::string s = resp.serialize();
        write(client_fd, s.data(), s.size());
        return;
    }

    if (!pam->check_account()) {
        IpcMessage resp;
        resp.type = IpcMessageType::Response;
        resp.success = false;
        resp.message = "Account check failed: " + pam->get_error_message();
        std::string s = resp.serialize();
        write(client_fd, s.data(), s.size());
        return;
    }

    if (!pam->establish_credentials()) {
        IpcMessage resp;
        resp.type = IpcMessageType::Response;
        resp.success = false;
        resp.message = "Failed to establish credentials: " + pam->get_error_message();
        std::string s = resp.serialize();
        write(client_fd, s.data(), s.size());
        return;
    }

    // Set PAM session properties for logind before opening session
    pam->set_env("XDG_SESSION_CLASS", "user");
    pam->set_env("XDG_SESSION_TYPE", "wayland");
    pam->set_env("XDG_SEAT", "seat0");
    pam->set_env("XDG_VTNR", std::to_string(Config::get().general.vt));
    if (!req.desktop_name.empty()) {
        pam->set_env("XDG_CURRENT_DESKTOP", req.desktop_name);
        pam->set_env("XDG_SESSION_DESKTOP", req.desktop_name);
    }

    if (!pam->open_session()) {
        IpcMessage resp;
        resp.type = IpcMessageType::Response;
        resp.success = false;
        resp.message = "Failed to open session: " + pam->get_error_message();
        std::string s = resp.serialize();
        write(client_fd, s.data(), s.size());
        return;
    }

    // Authentication succeeded! Send success reply before tearing down greeter
    IpcMessage resp;
    resp.type = IpcMessageType::Response;
    resp.success = true;
    resp.message = "Login successful";
    std::string s = resp.serialize();
    write(client_fd, s.data(), s.size());

    // Stop greeter UI
    stop_greeter();

    // Find UserInfo
    UserInfo user;
    auto user_it = std::find_if(m_cached_users.begin(), m_cached_users.end(), [&](const UserInfo& u) {
        return u.username == req.username;
    });
    if (user_it != m_cached_users.end()) {
        user = *user_it;
    } else {
        user.username = req.username;
        user.home_dir = "/home/" + req.username;
        user.shell = "/bin/bash";
    }

    // Find SessionInfo
    SessionInfo session;
    auto sess_it = std::find_if(m_cached_sessions.begin(), m_cached_sessions.end(), [&](const SessionInfo& si) {
        return si.id == req.desktop_name || si.name == req.desktop_name || si.exec == req.session_exec;
    });
    if (sess_it != m_cached_sessions.end()) {
        session = *sess_it;
    } else {
        session.name = req.desktop_name.empty() ? "Wayland" : req.desktop_name;
        session.exec = req.session_exec.empty() ? "miquland" : req.session_exec;
        session.desktop_names = session.name;
    }

    // Remember last user in config
    if (Config::get().general.remember_last_user) {
        Config::get().general.last_user = req.username;
        Config::get().save();
    }

    // Launch User Wayland Session
    m_active_pam = std::move(pam);
    bool launched = m_session_launcher.launch(
        user,
        session,
        m_active_pam->get_environment_list(),
        Config::get().general.vt
    );

    if (launched) {
        // Block until user logs out or session terminates
        int exit_code = m_session_launcher.wait_for_exit();
        std::cout << "[miqudm-daemon] User session exited with status " << exit_code << std::endl;
    } else {
        std::cerr << "[miqudm-daemon] Failed to launch user session!" << std::endl;
    }

    // Close PAM session
    if (m_active_pam) {
        m_active_pam->close_session();
        m_active_pam->delete_credentials();
        m_active_pam->end();
        m_active_pam.reset();
    }

    // Restart greeter for next login
    if (m_running) {
        std::cout << "[miqudm-daemon] Session ended. Respawning greeter..." << std::endl;
        start_greeter();
    }
}

} // namespace miqudm
