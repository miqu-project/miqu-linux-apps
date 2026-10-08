#pragma once

#include <string>
#include <vector>
#include <memory>
#include <security/pam_appl.h>

namespace miqudm {

class PamAuth {
public:
    PamAuth();
    ~PamAuth();

    // Disable copy, allow move
    PamAuth(const PamAuth&) = delete;
    PamAuth& operator=(const PamAuth&) = delete;
    PamAuth(PamAuth&& other) noexcept;
    PamAuth& operator=(PamAuth&& other) noexcept;

    bool start(const std::string& username, const std::string& service_name = "miqudm");
    bool authenticate(const std::string& password);
    bool check_account();
    bool establish_credentials();
    bool open_session();
    bool close_session();
    bool delete_credentials();
    void end();

    std::vector<std::string> get_environment_list() const;
    bool set_env(const std::string& name, const std::string& value);

    const std::string& get_error_message() const { return m_error_message; }
    const std::string& get_username() const { return m_username; }

private:
    static int pam_conversation(int num_msg, const struct pam_message** msg,
                                struct pam_response** resp, void* appdata_ptr);

    pam_handle_t* m_pam_handle = nullptr;
    std::string m_username;
    std::string m_service_name;
    std::string m_error_message;
    bool m_session_opened = false;
    bool m_credentials_set = false;
};

} // namespace miqudm
