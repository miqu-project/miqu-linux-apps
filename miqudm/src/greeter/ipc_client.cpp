#include "ipc_client.hpp"
#include "common/ipc_protocol.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <cstring>
#include <pwd.h>

namespace miqudm {

IpcClient::IpcClient(bool test_mode) : m_test_mode(test_mode) {}

IpcClient::~IpcClient() {
    if (m_socket_fd >= 0) {
        close(m_socket_fd);
        m_socket_fd = -1;
    }
}

bool IpcClient::connect_to_daemon(const std::string& custom_socket_path) {
    if (m_test_mode) {
        m_connected = true;
        return true;
    }

    std::string path = custom_socket_path;
    if (path.empty()) {
        const char* env_sock = getenv("MIQUDM_SOCK");
        path = env_sock ? env_sock : "/run/miqudm/socket";
    }

    int attempts = 15;
    while (attempts-- > 0) {
        m_socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (m_socket_fd < 0) {
            std::cerr << "[miqudm-greeter] Failed to create socket: " << strerror(errno) << std::endl;
            return false;
        }

        struct sockaddr_un addr = {};
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

        if (connect(m_socket_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0) {
            int flags = fcntl(m_socket_fd, F_GETFL, 0);
            fcntl(m_socket_fd, F_SETFL, flags | O_NONBLOCK);
            m_connected = true;
            std::cout << "[miqudm-greeter] Connected to miqudm daemon at " << path << std::endl;
            return true;
        }

        close(m_socket_fd);
        m_socket_fd = -1;
        usleep(100000); // 100ms
    }

    std::cerr << "[miqudm-greeter] Failed to connect to daemon at " << path << ": " << strerror(errno) << std::endl;
    return false;
}

void IpcClient::send_raw(const std::string& data) {
    if (m_socket_fd >= 0) {
        write(m_socket_fd, data.c_str(), data.size());
    }
}

void IpcClient::request_initial_data(InitialDataCallback cb) {
    m_initial_data_cb = std::move(cb);

    if (m_test_mode) {
        // Provide rich mock data for instant desktop testing
        std::vector<UserInfo> mock_users;

        std::string current_user = "user";
        struct passwd* pw = getpwuid(getuid());
        if (pw && pw->pw_name) {
            current_user = pw->pw_name;
        }

        UserInfo u1;
        u1.username = current_user;
        u1.display_name = current_user;
        if (!u1.display_name.empty()) {
            u1.display_name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(u1.display_name[0])));
        }
        u1.uid = getuid();
        mock_users.push_back(u1);

        UserInfo u2;
        u2.username = "guest";
        u2.display_name = "Guest User";
        u2.uid = 1001;
        mock_users.push_back(u2);

        std::vector<SessionInfo> mock_sessions = {
            {"miquland", "MiquLand", "miquland", "Miqu Desktop Compositor", "miquland", true},
            {"sway", "Sway", "sway", "SirCmpwn's Wayland window manager", "sway", true},
            {"hyprland", "Hyprland", "Hyprland", "Dynamic tiling Wayland compositor", "hyprland", true}
        };

        if (m_initial_data_cb) {
            m_initial_data_cb(mock_users, mock_sessions, "miquland", current_user);
        }
        return;
    }

    send_raw(IpcProtocol::serialize_get_initial_data());
}

void IpcClient::authenticate(const std::string& username, const std::string& password, AuthCallback cb) {
    m_auth_cb = std::move(cb);

    if (m_test_mode) {
        // In test mode, succeed on any password or test
        bool success = !password.empty();
        std::string msg = success ? "Success" : "Password cannot be empty";
        if (m_auth_cb) m_auth_cb(success, msg);
        return;
    }

    send_raw(IpcProtocol::serialize_authenticate(username, password));
}

void IpcClient::start_session(const std::string& username, const std::string& session_id, SessionCallback cb) {
    m_session_cb = std::move(cb);

    if (m_test_mode) {
        std::cout << "[miqudm-greeter] [TEST MODE] Simulated session start for user: "
                  << username << " session: " << session_id << std::endl;
        if (m_session_cb) m_session_cb(true, "Test session launched");
        return;
    }

    send_raw(IpcProtocol::serialize_start_session(username, session_id));
}

void IpcClient::send_power_action(PowerAction action, PowerCallback cb) {
    m_power_cb = std::move(cb);

    if (m_test_mode) {
        std::cout << "[miqudm-greeter] [TEST MODE] Simulated power action invoked" << std::endl;
        if (m_power_cb) m_power_cb(true);
        return;
    }

    send_raw(IpcProtocol::serialize_power_action(action));
}

void IpcClient::poll_responses() {
    if (m_test_mode || m_socket_fd < 0) return;

    char chunk[2048];
    ssize_t n = read(m_socket_fd, chunk, sizeof(chunk) - 1);
    if (n > 0) {
        chunk[n] = '\0';
        m_incoming_buffer.append(chunk, n);

        size_t pos = 0;
        while ((pos = m_incoming_buffer.find('\n')) != std::string::npos) {
            std::string line = m_incoming_buffer.substr(0, pos);
            m_incoming_buffer.erase(0, pos + 1);

            Json::Value root;
            if (!IpcProtocol::parse_json(line, root)) continue;

            std::string type = root.get("type", "").asString();

            if (type == "initial_data" && m_initial_data_cb) {
                std::vector<UserInfo> users;
                for (const auto& u : root["users"]) {
                    UserInfo info;
                    info.username = u.get("username", "").asString();
                    info.display_name = u.get("display_name", "").asString();
                    info.avatar_path = u.get("avatar_path", "").asString();
                    info.uid = u.get("uid", 0).asInt();
                    info.gid = u.get("gid", 0).asInt();
                    info.home_dir = u.get("home_dir", "").asString();
                    users.push_back(info);
                }

                std::vector<SessionInfo> sessions;
                for (const auto& s : root["sessions"]) {
                    SessionInfo info;
                    info.id = s.get("id", "").asString();
                    info.name = s.get("name", "").asString();
                    info.exec = s.get("exec", "").asString();
                    info.comment = s.get("comment", "").asString();
                    info.is_wayland = s.get("is_wayland", true).asBool();
                    sessions.push_back(info);
                }

                std::string def_sess = root.get("default_session", "").asString();
                std::string last_user = root.get("last_user", "").asString();

                m_initial_data_cb(users, sessions, def_sess, last_user);
            } else if (type == "auth_result" && m_auth_cb) {
                bool ok = root.get("success", false).asBool();
                std::string msg = root.get("message", "").asString();
                m_auth_cb(ok, msg);
            } else if (type == "session_started" && m_session_cb) {
                bool ok = root.get("success", false).asBool();
                std::string msg = root.get("message", "").asString();
                m_session_cb(ok, msg);
            } else if (type == "power_result" && m_power_cb) {
                bool ok = root.get("success", false).asBool();
                m_power_cb(ok);
            }
        }
    }
}

} // namespace miqudm
