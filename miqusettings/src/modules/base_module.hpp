#pragma once

#include <string>
#include <memory>
#include <miqutoolkit/view/view.hpp>

namespace miqu {

/**
 * @brief Base interface for all settings domain modules (Bluetooth, Wi-Fi, Display, etc.)
 * Enforces modular architecture and strict boundary isolation as miqusettings grows.
 */
class BaseSettingsModule {
public:
    virtual ~BaseSettingsModule() = default;

    virtual std::string get_id() const = 0;
    virtual std::string get_title() const = 0;
    virtual std::string get_icon() const = 0;

    virtual std::shared_ptr<View> create_detail_view() = 0;
    virtual void refresh() = 0;
};

} // namespace miqu
