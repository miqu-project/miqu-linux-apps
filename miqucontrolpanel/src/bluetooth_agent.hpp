#pragma once

#include <sdbus-c++/sdbus-c++.h>
#include <memory>
#include <string>
#include <functional>
#include <cstdint>

namespace miqu {

class BluetoothAgent {
public:
    using ConfirmCallback = std::function<void(bool confirmed)>;
    using AuthorizationCallback = std::function<void(bool authorized)>;
    using PinCallback = std::function<void(const std::string& pin)>;

    BluetoothAgent();
    ~BluetoothAgent();

    bool start();
    void stop();
    bool is_running() const { return m_running; }

    static void cache_device_name(const std::string& mac, const std::string& name);
    static std::string get_cached_device_name(const std::string& mac);

    void set_on_request_confirmation(std::function<void(const std::string& dev_name, const std::string& mac, uint32_t passkey, ConfirmCallback cb)> cb) {
        m_on_request_confirmation = std::move(cb);
    }

    void set_on_display_passkey(std::function<void(const std::string& dev_name, const std::string& mac, uint32_t passkey)> cb) {
        m_on_display_passkey = std::move(cb);
    }

    void set_on_request_authorization(std::function<void(const std::string& dev_name, const std::string& mac, AuthorizationCallback cb)> cb) {
        m_on_request_authorization = std::move(cb);
    }

    void set_on_request_pin(std::function<void(const std::string& dev_name, const std::string& mac, PinCallback cb)> cb) {
        m_on_request_pin = std::move(cb);
    }

    void set_on_cancel(std::function<void()> cb) {
        m_on_cancel = std::move(cb);
    }

private:
    std::string extract_mac_from_path(const sdbus::ObjectPath& path);
    std::string resolve_device_name(const sdbus::ObjectPath& path, const std::string& mac);

    std::unique_ptr<sdbus::IConnection> m_connection;
    std::unique_ptr<sdbus::IObject> m_agent_object;
    std::unique_ptr<sdbus::IProxy> m_manager_proxy;
    sdbus::ObjectPath m_agent_path{"/org/bluez/miqucontrolpanel/agent"};

    bool m_running = false;

    std::function<void(const std::string&, const std::string&, uint32_t, ConfirmCallback)> m_on_request_confirmation;
    std::function<void(const std::string&, const std::string&, uint32_t)> m_on_display_passkey;
    std::function<void(const std::string&, const std::string&, AuthorizationCallback)> m_on_request_authorization;
    std::function<void(const std::string&, const std::string&, PinCallback)> m_on_request_pin;
    std::function<void()> m_on_cancel;
};

} // namespace miqu
