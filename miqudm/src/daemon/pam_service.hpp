#pragma once

#include <string>
#include <vector>
#include <memory>
#include <security/pam_appl.h>

namespace miqudm {

class PamService {
public:
    explicit PamService(std::string service_name = "miqudm");
    ~PamService();

    PamService(const PamService&) = delete;
    PamService& operator=(const PamService&) = delete;

    PamService(PamService&& other) noexcept;
    PamService& operator=(PamService&& other) noexcept;

    bool start(const std::string& username);
    bool authenticate(const std::string& password);
    bool set_credentials(int flags = PAM_ESTABLISH_CRED);
    bool open_session();
    void close_session();
    void end();

    bool set_item(int item_type, const void* item);
    bool set_env(const std::string& key, const std::string& value);
    std::vector<std::string> get_env_list() const;
    const std::string& get_last_error() const { return m_last_error; }

private:
    static int conv_func(int num_msg, const struct pam_message** msg,
                         struct pam_response** resp, void* appdata_ptr);

    std::string m_service_name;
    pam_handle_t* m_pamh = nullptr;
    std::string m_current_user;
    std::string m_password_buffer;
    std::string m_last_error;
    bool m_session_opened = false;
};

} // namespace miqudm
