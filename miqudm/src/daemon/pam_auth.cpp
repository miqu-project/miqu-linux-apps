#include "pam_auth.hpp"
#include <cstring>
#include <cstdlib>
#include <iostream>

namespace miqudm {

struct PamUserData {
    std::string password;
};

int PamAuth::pam_conversation(int num_msg, const struct pam_message** msg,
                              struct pam_response** resp, void* appdata_ptr)
{
    if (num_msg <= 0 || num_msg > 32) return PAM_CONV_ERR;

    auto* data = static_cast<PamUserData*>(appdata_ptr);
    if (!data) return PAM_CONV_ERR;

    auto* reply = static_cast<struct pam_response*>(calloc(num_msg, sizeof(struct pam_response)));
    if (!reply) return PAM_BUF_ERR;

    for (int i = 0; i < num_msg; ++i) {
        int style = msg[i]->msg_style;
        if (style == PAM_PROMPT_ECHO_OFF || style == PAM_PROMPT_ECHO_ON) {
            reply[i].resp = strdup(data->password.c_str());
            reply[i].resp_retcode = 0;
        } else {
            reply[i].resp = nullptr;
            reply[i].resp_retcode = 0;
        }
    }

    *resp = reply;
    return PAM_SUCCESS;
}

PamAuth::PamAuth() = default;

PamAuth::~PamAuth() {
    end();
}

PamAuth::PamAuth(PamAuth&& other) noexcept
    : m_pam_handle(other.m_pam_handle),
      m_username(std::move(other.m_username)),
      m_service_name(std::move(other.m_service_name)),
      m_error_message(std::move(other.m_error_message)),
      m_session_opened(other.m_session_opened),
      m_credentials_set(other.m_credentials_set)
{
    other.m_pam_handle = nullptr;
    other.m_session_opened = false;
    other.m_credentials_set = false;
}

PamAuth& PamAuth::operator=(PamAuth&& other) noexcept {
    if (this != &other) {
        end();
        m_pam_handle = other.m_pam_handle;
        m_username = std::move(other.m_username);
        m_service_name = std::move(other.m_service_name);
        m_error_message = std::move(other.m_error_message);
        m_session_opened = other.m_session_opened;
        m_credentials_set = other.m_credentials_set;

        other.m_pam_handle = nullptr;
        other.m_session_opened = false;
        other.m_credentials_set = false;
    }
    return *this;
}

bool PamAuth::start(const std::string& username, const std::string& service_name) {
    end();
    m_username = username;
    m_service_name = service_name;

    struct pam_conv conv = {
        &PamAuth::pam_conversation,
        nullptr
    };

    int ret = pam_start(service_name.c_str(), username.c_str(), &conv, &m_pam_handle);
    if (ret != PAM_SUCCESS) {
        // Fallback to "login" service if specific service is not found
        ret = pam_start("login", username.c_str(), &conv, &m_pam_handle);
    }

    if (ret != PAM_SUCCESS) {
        m_error_message = pam_strerror(m_pam_handle, ret);
        m_pam_handle = nullptr;
        return false;
    }

    return true;
}

bool PamAuth::authenticate(const std::string& password) {
    if (!m_pam_handle) {
        m_error_message = "PAM handle not initialized";
        return false;
    }

    PamUserData user_data;
    user_data.password = password;

    struct pam_conv conv = {
        &PamAuth::pam_conversation,
        &user_data
    };

    int ret = pam_set_item(m_pam_handle, PAM_CONV, &conv);
    if (ret != PAM_SUCCESS) {
        m_error_message = pam_strerror(m_pam_handle, ret);
        return false;
    }

    ret = pam_authenticate(m_pam_handle, 0);

    // Wipe password from memory immediately
    if (!user_data.password.empty()) {
        explicit_bzero(user_data.password.data(), user_data.password.size());
        user_data.password.clear();
    }

    if (ret != PAM_SUCCESS) {
        m_error_message = pam_strerror(m_pam_handle, ret);
        return false;
    }

    return true;
}

bool PamAuth::check_account() {
    if (!m_pam_handle) return false;
    int ret = pam_acct_mgmt(m_pam_handle, 0);
    if (ret != PAM_SUCCESS) {
        m_error_message = pam_strerror(m_pam_handle, ret);
        return false;
    }
    return true;
}

bool PamAuth::establish_credentials() {
    if (!m_pam_handle) return false;
    int ret = pam_setcred(m_pam_handle, PAM_ESTABLISH_CRED);
    if (ret != PAM_SUCCESS) {
        m_error_message = pam_strerror(m_pam_handle, ret);
        return false;
    }
    m_credentials_set = true;
    return true;
}

bool PamAuth::delete_credentials() {
    if (!m_pam_handle || !m_credentials_set) return true;
    int ret = pam_setcred(m_pam_handle, PAM_DELETE_CRED);
    m_credentials_set = false;
    if (ret != PAM_SUCCESS) {
        m_error_message = pam_strerror(m_pam_handle, ret);
        return false;
    }
    return true;
}

bool PamAuth::open_session() {
    if (!m_pam_handle) return false;
    int ret = pam_open_session(m_pam_handle, 0);
    if (ret != PAM_SUCCESS) {
        m_error_message = pam_strerror(m_pam_handle, ret);
        return false;
    }
    m_session_opened = true;
    return true;
}

bool PamAuth::close_session() {
    if (!m_pam_handle || !m_session_opened) return true;
    int ret = pam_close_session(m_pam_handle, 0);
    m_session_opened = false;
    if (ret != PAM_SUCCESS) {
        m_error_message = pam_strerror(m_pam_handle, ret);
        return false;
    }
    return true;
}

void PamAuth::end() {
    if (m_pam_handle) {
        if (m_session_opened) {
            close_session();
        }
        if (m_credentials_set) {
            delete_credentials();
        }
        pam_end(m_pam_handle, PAM_SUCCESS);
        m_pam_handle = nullptr;
    }
    m_session_opened = false;
    m_credentials_set = false;
}

std::vector<std::string> PamAuth::get_environment_list() const {
    std::vector<std::string> envs;
    if (!m_pam_handle) return envs;

    char** pam_env = pam_getenvlist(m_pam_handle);
    if (!pam_env) return envs;

    for (int i = 0; pam_env[i] != nullptr; ++i) {
        envs.emplace_back(pam_env[i]);
    }

    return envs;
}

bool PamAuth::set_env(const std::string& name, const std::string& value) {
    if (!m_pam_handle) return false;
    std::string entry = name + "=" + value;
    int ret = pam_putenv(m_pam_handle, entry.c_str());
    return ret == PAM_SUCCESS;
}

} // namespace miqudm
