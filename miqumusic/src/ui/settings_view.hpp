#pragma once

#include "mpd_client.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <memory>
#include <string>
#include <functional>

namespace miqumusic {

class SettingsView {
public:
    static std::shared_ptr<miqu::View> create(
        MpdClient& client,
        const std::string& host,
        unsigned int port,
        std::function<void(bool connected)> on_conn_changed
    );
};

} // namespace miqumusic
