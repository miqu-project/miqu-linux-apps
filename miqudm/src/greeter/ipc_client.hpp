#pragma once

#include "common/types.hpp"
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>

namespace miqudm {

class IpcClient {
public:
    using InitialDataCallback = std::function<void(const std::vector<UserInfo>& users,
                                                   const std::vector<SessionInfo>& sessions,
                                                   const std::string& default_session,
                                                   const std::string& last_user)>;
    using AuthCallback = std::function<void(bool success, const std::string& message)>;
    using SessionCallback = std::function<void(bool success, const std::string& message)>;
    using PowerCallback = std::function<void(bool success)>;

    explicit IpcClient(bool test_mode = false);
    ~IpcClient();

    bool connect_to_daemon(const std::string& socket_path = "");
    bool is_connected() const { return m_connected || m_test_mode; }

    void request_initial_data(InitialDataCallback cb);
    void authenticate(const std::string& username, const std::string& password, AuthCallback cb);
    void start_session(const std::string& username, const std::string& session_id, SessionCallback cb);
    void send_power_action(PowerAction action, PowerCallback cb);

    // Call from event loop or thread to process pending socket responses
    void poll_responses();

private:
    void send_raw(const std::string& data);

    bool m_test_mode = false;
    bool m_connected = false;
    int m_socket_fd = -1;
    std::string m_incoming_buffer;
    std::mutex m_mutex;

    InitialDataCallback m_initial_data_cb;
    AuthCallback m_auth_cb;
    SessionCallback m_session_cb;
    PowerCallback m_power_cb;
};

} // namespace miqudm
