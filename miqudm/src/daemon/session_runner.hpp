#pragma once

#include "types.hpp"
#include "pam_service.hpp"
#include <string>
#include <vector>
#include <memory>
#include <sys/types.h>

namespace miqudm {

class SessionRunner {
public:
    static pid_t launch_user_session(const UserInfo& user,
                                     const SessionInfo& session,
                                     std::unique_ptr<PamService> pam,
                                     int vt_number);
};

} // namespace miqudm
