#include "daemon.hpp"
#include "logind_manager.hpp"
#include "session_runner.hpp"
#include "user_discovery.hpp"
#include "session_discovery.hpp"
#include "config.hpp"
#include "ipc_protocol.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <pwd.h>
#include <grp.h>
#include <csignal>
#include <iostream>
#include <filesystem>
#include <cstring>
#include <algorithm>

namespace miqudm {

namespace fs = std::filesystem;

static void clear_vt(int vt_number) {
    if (vt_number <= 0) return;
    std::string tty_dev = "/dev/tty" + std::to_string(vt_number);
    int fd = open(tty_dev.c_str(), O_RDWR | O_NOCTTY | O_CLOEXEC);
    if (fd >= 0) {
        // \033[H: home cursor, \033[2J: clear visible screen, \033[3J: clear scrollback, \033[?25l: hide cursor
        const char clear_seq[] = "\033[H\033[2J\033[3J\033[?25l";
        [[maybe_unused]] auto n = write(fd, clear_seq, sizeof(clear_seq) - 1);
        close(fd);
    }
}

Daemon::Daemon() = default;

Daemon::~Daemon() {
    stop();
}

bool Daemon::setup_ipc_socket() {
    cleanup_ipc_socket();

    fs::path p(m_socket_path);
    std::error_code ec;
    if (p.has_parent_path()) {
        fs::create_directories(p.parent_path(), ec);
        chmod(p.parent_path().c_str(), 0755);
    }

    m_server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_server_fd < 0) {
        std::cerr << "[miqudm-daemon] Failed to create socket: " << strerror(errno) << std::endl;
        return false;
    }

    int flags = fcntl(m_server_fd, F_GETFL, 0);
    fcntl(m_server_fd, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, m_socket_path.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(m_server_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "[miqudm-daemon] Failed to bind socket " << m_socket_path << ": " << strerror(errno) << std::endl;
        close(m_server_fd);
        m_server_fd = -1;
        return false;
    }

    // Allow permissions for greeter user to connect
    chmod(m_socket_path.c_str(), 0666);

    if (listen(m_server_fd, 8) < 0) {
        std::cerr << "[miqudm-daemon] Failed to listen on socket: " << strerror(errno) << std::endl;
        close(m_server_fd);
        m_server_fd = -1;
        return false;
    }

    std::cout << "[miqudm-daemon] Listening on IPC socket: " << m_socket_path << std::endl;
    return true;
}

void Daemon::cleanup_ipc_socket() {
    if (m_server_fd >= 0) {
        close(m_server_fd);
        m_server_fd = -1;
    }
    std::error_code ec;
    fs::remove(m_socket_path, ec);
}

bool Daemon::init() {
    std::cout << "[miqudm-daemon] Initializing miqudm display manager..." << std::endl;

    ConfigManager::get().load_daemon_config();

    m_cached_users = UserDiscovery::discover_users();
    m_cached_sessions = SessionDiscovery::discover_all();

    std::cout << "[miqudm-daemon] Discovered " << m_cached_users.size() << " users and "
              << m_cached_sessions.size() << " sessions." << std::endl;

    LogindManager::get().init();

    if (!setup_ipc_socket()) {
        return false;
    }

    return true;
}

void Daemon::start_greeter() {
    if (m_greeter_pid > 0) return;

    const auto& conf = ConfigManager::get().daemon();
    std::string greeter_user = conf.greeter_user;
    struct passwd* pw = getpwnam(greeter_user.c_str());

    // Fallbacks if specified greeter user doesn't exist
    if (!pw) pw = getpwnam("greeter");
    if (!pw) pw = getpwnam("sddm");
    if (!pw) pw = getpwnam("miqudm");

    std::string runtime_dir = pw ? ("/run/user/" + std::to_string(pw->pw_uid)) : "/run/miqudm/runtime";
    std::error_code ec;
    fs::create_directories(runtime_dir, ec);
    if (pw) {
        chmod(runtime_dir.c_str(), 0700);
        chown(runtime_dir.c_str(), pw->pw_uid, pw->pw_gid);
    }

    // 1. Switch to configured VT & clear text console / hide cursor
    if (conf.vt > 0) {
        LogindManager::get().switch_to_vt(conf.vt);
        clear_vt(conf.vt);
    }

    std::string cmd = conf.greeter_command;
    std::string bin_name = cmd.substr(0, cmd.find(' '));
    if (!bin_name.empty() && bin_name.front() != '/') {
        bin_name = "/usr/bin/" + bin_name;
    }
    if (!bin_name.empty() && access(bin_name.c_str(), X_OK) != 0) {
        if (access("/usr/bin/miquland", X_OK) == 0) {
            std::cout << "[miqudm-daemon] Notice: '" << bin_name << "' not found, falling back to 'miquland -s /usr/bin/miqudm-greeter'" << std::endl;
            cmd = "miquland -s /usr/bin/miqudm-greeter";
        }
    }

    std::cout << "[miqudm-daemon] Spawning greeter command: " << cmd << std::endl;

    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "[miqudm-daemon] Failed to fork greeter: " << strerror(errno) << std::endl;
        return;
    }

    if (pid == 0) {
        setsid();

        // 2. Open PAM session for greeter user IN THE CHILD so logind registers seat0
        PamService greeter_pam("miqudm-greeter");
        std::string greeter_name = pw ? pw->pw_name : "greeter";
        std::string tty_name = "tty" + std::to_string(conf.vt);
        if (greeter_pam.start(greeter_name)) {
            greeter_pam.set_item(PAM_TTY, tty_name.c_str());
            greeter_pam.set_env("XDG_SEAT", "seat0");
            greeter_pam.set_env("XDG_VTNR", std::to_string(conf.vt));
            greeter_pam.set_env("XDG_SESSION_CLASS", "greeter");
            greeter_pam.set_env("XDG_SESSION_TYPE", "wayland");
            greeter_pam.authenticate("");
            greeter_pam.open_session();
        } else {
            std::cerr << "[miqudm-daemon] Warning: Failed to initialize PAM for greeter: "
                      << greeter_pam.get_last_error() << std::endl;
        }

        if (pw && pw->pw_uid != 0) {
            initgroups(pw->pw_name, pw->pw_gid);

            // Ensure supplementary groups for DRM, libinput, and seat access
            std::vector<gid_t> supp_groups = { pw->pw_gid };
            struct group* gr = nullptr;
            if ((gr = getgrnam("video")) != nullptr) supp_groups.push_back(gr->gr_gid);
            if ((gr = getgrnam("input")) != nullptr) supp_groups.push_back(gr->gr_gid);
            if ((gr = getgrnam("seat")) != nullptr) supp_groups.push_back(gr->gr_gid);
            setgroups(supp_groups.size(), supp_groups.data());

            setgid(pw->pw_gid);
            setuid(pw->pw_uid);
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
        setenv("XDG_VTNR", std::to_string(conf.vt).c_str(), 1);
        setenv("XDG_RUNTIME_DIR", runtime_dir.c_str(), 1);
        setenv("MIQUDM_SOCK", m_socket_path.c_str(), 1);

        // Preload kiosk input hook to enable touchpad tap-to-click & touch mapping in Cage
        const char* hook_candidates[] = {
            "/usr/lib/miqudm/libmiqudm-kiosk-input.so",
            "/usr/lib64/miqudm/libmiqudm-kiosk-input.so",
            "/usr/local/lib/miqudm/libmiqudm-kiosk-input.so",
            nullptr
        };
        for (int i = 0; hook_candidates[i]; ++i) {
            if (access(hook_candidates[i], R_OK) == 0) {
                setenv("LD_PRELOAD", hook_candidates[i], 1);
                break;
            }
        }

        // Apply PAM environment variables (including XDG_SESSION_ID)
        for (const auto& env : greeter_pam.get_env_list()) {
            auto eq = env.find('=');
            if (eq != std::string::npos) {
                std::string k = env.substr(0, eq);
                std::string v = env.substr(eq + 1);
                setenv(k.c_str(), v.c_str(), 1);
            }
        }

        if (pw && pw->pw_dir && *pw->pw_dir && fs::exists(pw->pw_dir, ec)) {
            chdir(pw->pw_dir);
        } else {
            chdir("/tmp");
        }

        // Parse command and exec directly without intermediate shell wrapper
        std::vector<std::string> args;
        std::stringstream ss(cmd);
        std::string token;
        while (ss >> token) {
            args.push_back(token);
        }
        std::vector<char*> c_args;
        for (auto& a : args) c_args.push_back(a.data());
        c_args.push_back(nullptr);

        if (!c_args.empty() && c_args[0] != nullptr) {
            execvp(c_args[0], c_args.data());
        }

        // Fallback to /bin/sh if execvp failed
        execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
        std::cerr << "[miqudm-daemon] Failed to exec greeter: " << strerror(errno) << std::endl;
        _exit(127);
    }

    m_greeter_pid = pid;
    std::cout << "[miqudm-daemon] Greeter process running (PID " << m_greeter_pid << ")" << std::endl;
}

void Daemon::stop_greeter() {
    if (m_greeter_pid > 0) {
        std::cout << "[miqudm-daemon] Terminating greeter (PID " << m_greeter_pid << ")..." << std::endl;
        kill(-m_greeter_pid, SIGTERM);
        kill(m_greeter_pid, SIGTERM);

        // Allow up to 500ms for graceful shutdown before SIGKILL
        int status = 0;
        int checks = 5;
        while (checks-- > 0) {
            pid_t waited = waitpid(m_greeter_pid, &status, WNOHANG);
            if (waited == m_greeter_pid) {
                m_greeter_pid = -1;
                break;
            }
            usleep(100000);
        }

        if (m_greeter_pid > 0) {
            kill(-m_greeter_pid, SIGKILL);
            kill(m_greeter_pid, SIGKILL);
            waitpid(m_greeter_pid, &status, 0);
            m_greeter_pid = -1;
        }
    }
}

void Daemon::handle_client_message(int client_fd, const std::string& line) {
    Json::Value root;
    if (!IpcProtocol::parse_json(line, root)) return;

    std::string type = root.get("type", "").asString();

    if (type == "get_initial_data") {
        const auto& conf = ConfigManager::get().daemon();
        std::cout << "[miqudm-daemon] Greeter requested initial data. Sending "
                  << m_cached_users.size() << " users and "
                  << m_cached_sessions.size() << " sessions." << std::endl;
        std::string reply = IpcProtocol::serialize_initial_data(
            m_cached_users,
            m_cached_sessions,
            conf.default_session,
            conf.last_user
        );
        write(client_fd, reply.c_str(), reply.size());
    } else if (type == "authenticate") {
        std::string user = root.get("username", "").asString();
        std::string pass = root.get("password", "").asString();
        std::cout << "[miqudm-daemon] Authenticating user: " << user << std::endl;

        bool ok = false;
        std::string err_msg;

        auto pam = std::make_unique<PamService>("miqudm");
        if (pam->start(user)) {
            ok = pam->authenticate(pass);
            if (!ok) {
                err_msg = pam->get_last_error();
                if (err_msg.empty()) err_msg = "Authentication failed";
                pam->end();
            } else {
                m_authenticated_user = user;
                m_authenticated_pam = std::move(pam);
            }
        } else {
            err_msg = pam->get_last_error();
        }

        std::string reply = IpcProtocol::serialize_auth_result(ok, err_msg);
        write(client_fd, reply.c_str(), reply.size());
    } else if (type == "start_session") {
        std::string user = root.get("username", "").asString();
        std::string session_id = root.get("session_id", "").asString();

        if (user != m_authenticated_user || !m_authenticated_pam || user.empty()) {
            std::string reply = IpcProtocol::serialize_session_started(false, "User not authenticated");
            write(client_fd, reply.c_str(), reply.size());
            return;
        }

        // Find user info
        auto u_it = std::find_if(m_cached_users.begin(), m_cached_users.end(), [&](const UserInfo& u) {
            return u.username == user;
        });
        if (u_it == m_cached_users.end()) {
            std::string reply = IpcProtocol::serialize_session_started(false, "User not found");
            write(client_fd, reply.c_str(), reply.size());
            return;
        }

        // Find session info
        auto s_it = std::find_if(m_cached_sessions.begin(), m_cached_sessions.end(), [&](const SessionInfo& s) {
            return s.id == session_id;
        });
        if (s_it == m_cached_sessions.end()) {
            std::string reply = IpcProtocol::serialize_session_started(false, "Session not found");
            write(client_fd, reply.c_str(), reply.size());
            return;
        }

        // Send acknowledgement before tearing down greeter
        std::string reply = IpcProtocol::serialize_session_started(true, "Launching session...");
        write(client_fd, reply.c_str(), reply.size());

        // 1. Clear VT text buffer & hide cursor so DRM release shows black instead of boot text
        clear_vt(ConfigManager::get().daemon().vt);

        // 2. Terminate greeter compositor cleanly
        stop_greeter();

        // 2. Save last user
        ConfigManager::get().save_last_user(user);

        // 3. Launch session with dedicated worker process
        std::cout << "[miqudm-daemon] Launching user session (" << user << "): " << s_it->exec << std::endl;
        m_user_session_pid = SessionRunner::launch_user_session(
            *u_it,
            *s_it,
            std::move(m_authenticated_pam),
            ConfigManager::get().daemon().vt
        );
        m_authenticated_user.clear();
    } else if (type == "power_action") {
        std::string action = root.get("action", "").asString();
        bool ok = false;
        if (action == "reboot") ok = LogindManager::get().reboot();
        else if (action == "poweroff") ok = LogindManager::get().power_off();
        else if (action == "suspend") ok = LogindManager::get().suspend();

        std::string reply = IpcProtocol::serialize_power_result(ok);
        write(client_fd, reply.c_str(), reply.size());
    }
}

void Daemon::run() {
    m_running = true;
    start_greeter();

    std::vector<pollfd> fds;
    fds.push_back({m_server_fd, POLLIN, 0});

    std::string buffer;
    char chunk[2048];

    while (m_running) {
        int ret = poll(fds.data(), fds.size(), 500);

        // Check if user session has exited
        if (m_user_session_pid > 0) {
            int status = 0;
            pid_t waited = waitpid(m_user_session_pid, &status, WNOHANG);
            if (waited == m_user_session_pid) {
                std::cout << "[miqudm-daemon] User session exited with status " << status << std::endl;
                m_user_session_pid = -1;
                m_authenticated_user.clear();
                m_authenticated_pam.reset();

                // Re-launch greeter
                start_greeter();
            }
        }

        // Check if greeter died unexpectedly
        if (m_greeter_pid > 0 && m_user_session_pid <= 0) {
            int status = 0;
            pid_t waited = waitpid(m_greeter_pid, &status, WNOHANG);
            if (waited == m_greeter_pid) {
                std::cout << "[miqudm-daemon] Greeter exited (status " << status << "), restarting in 1s..." << std::endl;
                m_greeter_pid = -1;
                sleep(1);
                start_greeter();
            }
        }

        if (ret <= 0) continue;

        // New client connection on server socket
        if (fds[0].revents & POLLIN) {
            int client_fd = accept(m_server_fd, nullptr, nullptr);
            if (client_fd >= 0) {
                int flags = fcntl(client_fd, F_GETFL, 0);
                fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
                fds.push_back({client_fd, POLLIN, 0});
            }
        }

        // Process client messages
        for (size_t i = 1; i < fds.size();) {
            if (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                close(fds[i].fd);
                fds.erase(fds.begin() + i);
                continue;
            }

            if (fds[i].revents & POLLIN) {
                ssize_t n = read(fds[i].fd, chunk, sizeof(chunk) - 1);
                if (n > 0) {
                    chunk[n] = '\0';
                    buffer.append(chunk, n);

                    size_t pos = 0;
                    while ((pos = buffer.find('\n')) != std::string::npos) {
                        std::string line = buffer.substr(0, pos);
                        buffer.erase(0, pos + 1);
                        if (!line.empty()) {
                            handle_client_message(fds[i].fd, line);
                        }
                    }
                } else if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                    close(fds[i].fd);
                    fds.erase(fds.begin() + i);
                    continue;
                }
            }
            ++i;
        }
    }

    stop_greeter();
    cleanup_ipc_socket();
}

void Daemon::stop() {
    m_running = false;
    stop_greeter();
    if (m_user_session_pid > 0) {
        kill(-m_user_session_pid, SIGTERM);
        kill(m_user_session_pid, SIGTERM);
        m_user_session_pid = -1;
    }
    m_authenticated_pam.reset();
    cleanup_ipc_socket();
}

} // namespace miqudm
