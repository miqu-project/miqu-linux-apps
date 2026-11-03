#include "pam_service.hpp"
#include <cstring>
#include <cstdlib>
#include <iostream>

namespace miqudm {

int PamService::conv_func(int num_msg, const struct pam_message** msg,
                         struct pam_response** resp, void* appdata_ptr) {
    if (num_msg <= 0 || num_msg > 32) return PAM_CONV_ERR;

    auto* service = static_cast<PamService*>(appdata_ptr);
    if (!service) return PAM_CONV_ERR;

    auto* reply = static_cast<struct pam_response*>(calloc(num_msg, sizeof(struct pam_response)));
    if (!reply) return PAM_BUF_ERR;

    for (int i = 0; i < num_msg; ++i) {
        int style = msg[i]->msg_style;
        if (style == PAM_PROMPT_ECHO_OFF || style == PAM_PROMPT_ECHO_ON) {
            reply[i].resp = strdup(service->m_password_buffer.c_str());
            reply[i].resp_retcode = 0;
        } else if (style == PAM_ERROR_MSG || style == PAM_TEXT_INFO) {
            reply[i].resp = nullptr;
            reply[i].resp_retcode = 0;
        } else {
            reply[i].resp = nullptr;
            reply[i].resp_retcode = 0;
        }
    }

    *resp = reply;
    return PAM_SUCCESS;
}

PamService::PamService(std::string service_name)
    : m_service_name(std::move(service_name)) {}

PamService::~PamService() {
    end();
}

PamService::PamService(PamService&& other) noexcept
    : m_service_name(std::move(other.m_service_name)),
      m_pamh(other.m_pamh),
      m_current_user(std::move(other.m_current_user)),
      m_password_buffer(std::move(other.m_password_buffer)),
      m_last_error(std::move(other.m_last_error)),
      m_session_opened(other.m_session_opened) {
    other.m_pamh = nullptr;
    other.m_session_opened = false;
}

PamService& PamService::operator=(PamService&& other) noexcept {
    if (this != &other) {
        end();
        m_service_name = std::move(other.m_service_name);
        m_pamh = other.m_pamh;
        m_current_user = std::move(other.m_current_user);
        m_password_buffer = std::move(other.m_password_buffer);
        m_last_error = std::move(other.m_last_error);
        m_session_opened = other.m_session_opened;

        other.m_pamh = nullptr;
        other.m_session_opened = false;
    }
    return *this;
}

bool PamService::start(const std::string& username) {
    end();
    m_current_user = username;

    struct pam_conv conv = {
        &PamService::conv_func,
        this
    };

    int ret = pam_start(m_service_name.c_str(), m_current_user.c_str(), &conv, &m_pamh);
    if (ret != PAM_SUCCESS) {
        // Fallback to system-login if custom service not installed
        ret = pam_start("system-login", m_current_user.c_str(), &conv, &m_pamh);
    }

    if (ret != PAM_SUCCESS) {
        m_last_error = "Failed to initialize PAM: " + std::string(pam_strerror(m_pamh, ret));
        m_pamh = nullptr;
        return false;
    }

    return true;
}

bool PamService::authenticate(const std::string& password) {
    if (!m_pamh) {
        m_last_error = "PAM session not initialized";
        return false;
    }

    m_password_buffer = password;

    int ret = pam_authenticate(m_pamh, 0);
    // Explicitly wipe memory holding password
    if (!m_password_buffer.empty()) {
        explicit_bzero(m_password_buffer.data(), m_password_buffer.size());
        m_password_buffer.clear();
    }

    if (ret != PAM_SUCCESS) {
        m_last_error = pam_strerror(m_pamh, ret);
        return false;
    }

    ret = pam_acct_mgmt(m_pamh, 0);
    if (ret != PAM_SUCCESS) {
        m_last_error = pam_strerror(m_pamh, ret);
        return false;
    }

    return true;
}

bool PamService::set_credentials(int flags) {
    if (!m_pamh) {
        m_last_error = "PAM session not initialized";
        return false;
    }
    int ret = pam_setcred(m_pamh, flags);
    if (ret != PAM_SUCCESS) {
        m_last_error = pam_strerror(m_pamh, ret);
        return false;
    }
    return true;
}

bool PamService::open_session() {
    if (!m_pamh) return false;

    int ret = pam_open_session(m_pamh, 0);
    if (ret != PAM_SUCCESS) {
        m_last_error = pam_strerror(m_pamh, ret);
        return false;
    }

    m_session_opened = true;
    return true;
}

void PamService::close_session() {
    if (m_pamh && m_session_opened) {
        pam_setcred(m_pamh, PAM_DELETE_CRED);
        pam_close_session(m_pamh, 0);
        m_session_opened = false;
    }
}

void PamService::end() {
    close_session();
    if (m_pamh) {
        pam_end(m_pamh, PAM_SUCCESS);
        m_pamh = nullptr;
    }
    if (!m_password_buffer.empty()) {
        explicit_bzero(m_password_buffer.data(), m_password_buffer.size());
        m_password_buffer.clear();
    }
}

bool PamService::set_item(int item_type, const void* item) {
    if (!m_pamh) return false;
    return pam_set_item(m_pamh, item_type, item) == PAM_SUCCESS;
}

bool PamService::set_env(const std::string& key, const std::string& value) {
    if (!m_pamh) return false;
    std::string env_str = key + "=" + value;
    return pam_putenv(m_pamh, env_str.c_str()) == PAM_SUCCESS;
}

std::vector<std::string> PamService::get_env_list() const {
    std::vector<std::string> envs;
    if (!m_pamh) return envs;

    char** pam_env = pam_getenvlist(m_pamh);
    if (!pam_env) return envs;

    for (char** p = pam_env; *p != nullptr; ++p) {
        envs.emplace_back(*p);
    }
    free(pam_env);
    return envs;
}

} // namespace miqudm
