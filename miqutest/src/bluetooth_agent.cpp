#include "bluetooth_agent.hpp"
#include "bluetooth_backend.hpp"
#include <miqutoolkit/core/app_engine.hpp>
#include <iostream>
#include <algorithm>

namespace miqu {

BluetoothAgent::BluetoothAgent() = default;

BluetoothAgent::~BluetoothAgent() {
    stop();
}

std::string BluetoothAgent::extract_mac_from_path(const sdbus::ObjectPath& path) {
    std::string s = path;
    size_t dev_idx = s.find("dev_");
    if (dev_idx == std::string::npos) return s;
    std::string raw = s.substr(dev_idx + 4);
    std::replace(raw.begin(), raw.end(), '_', ':');
    return raw;
}

static std::mutex s_name_cache_mutex;
static std::unordered_map<std::string, std::string> s_name_cache;

void BluetoothAgent::cache_device_name(const std::string& mac, const std::string& name) {
    if (mac.empty() || name.empty()) return;
    std::lock_guard<std::mutex> lock(s_name_cache_mutex);
    s_name_cache[mac] = name;
}

std::string BluetoothAgent::get_cached_device_name(const std::string& mac) {
    std::lock_guard<std::mutex> lock(s_name_cache_mutex);
    auto it = s_name_cache.find(mac);
    if (it != s_name_cache.end()) return it->second;
    return "";
}

std::string BluetoothAgent::resolve_device_name(const sdbus::ObjectPath& path, const std::string& mac) {
    std::string cached = get_cached_device_name(mac);
    if (!cached.empty()) return cached;

    try {
        if (m_connection) {
            auto dev_proxy = sdbus::createProxy(*m_connection, sdbus::ServiceName{"org.bluez"}, path);
            try {
                std::string alias = dev_proxy->getProperty("Alias").onInterface("org.bluez.Device1").get<std::string>();
                if (!alias.empty()) {
                    cache_device_name(mac, alias);
                    return alias;
                }
            } catch (...) {}
            try {
                std::string name = dev_proxy->getProperty("Name").onInterface("org.bluez.Device1").get<std::string>();
                if (!name.empty()) {
                    cache_device_name(mac, name);
                    return name;
                }
            } catch (...) {}
        }
    } catch (...) {}
    return mac;
}

bool BluetoothAgent::start() {
    if (m_running) return true;

    try {
        m_connection = sdbus::createSystemBusConnection();
        m_agent_object = sdbus::createObject(*m_connection, m_agent_path);

        m_agent_object->addVTable(
            sdbus::registerMethod("Release")
                .implementedAs([this]() {
                    m_running = false;
                }),
            sdbus::registerMethod("RequestPinCode")
                .withInputParamNames("device")
                .withOutputParamNames("pincode")
                .implementedAs([this](sdbus::Result<std::string> result, sdbus::ObjectPath device) {
                    std::string mac = extract_mac_from_path(device);
                    std::string dev_name = resolve_device_name(device, mac);

                    if (m_on_request_pin) {
                        if (auto engine = AppEngine::instance()) {
                            auto res_ptr = std::make_shared<sdbus::Result<std::string>>(std::move(result));
                            engine->post([this, dev_name, mac, res_ptr]() {
                                m_on_request_pin(dev_name, mac, [res_ptr](const std::string& pin) {
                                    if (!pin.empty()) {
                                        res_ptr->returnResults(pin);
                                    } else {
                                        res_ptr->returnError(sdbus::Error(sdbus::Error::Name{"org.bluez.Error.Rejected"}, "PIN entry rejected"));
                                    }
                                });
                            });
                            return;
                        }
                    }
                    // Default to standard 0000 PIN for audio speakers / headsets
                    result.returnResults("0000");
                }),
            sdbus::registerMethod("DisplayPinCode")
                .withInputParamNames("device", "pincode")
                .implementedAs([](sdbus::ObjectPath, std::string) {}),
            sdbus::registerMethod("RequestPasskey")
                .withInputParamNames("device")
                .withOutputParamNames("passkey")
                .implementedAs([](sdbus::Result<uint32_t> result, sdbus::ObjectPath) {
                    result.returnResults(0);
                }),
            sdbus::registerMethod("DisplayPasskey")
                .withInputParamNames("device", "passkey", "entered")
                .implementedAs([this](sdbus::ObjectPath device, uint32_t passkey, uint16_t) {
                    std::string mac = extract_mac_from_path(device);
                    std::string dev_name = resolve_device_name(device, mac);

                    if (auto engine = AppEngine::instance()) {
                        engine->post([this, dev_name, mac, passkey]() {
                            if (m_on_display_passkey) {
                                m_on_display_passkey(dev_name, mac, passkey);
                            }
                        });
                    }
                }),
            sdbus::registerMethod("RequestConfirmation")
                .withInputParamNames("device", "passkey")
                .implementedAs([this](sdbus::Result<> result, sdbus::ObjectPath device, uint32_t passkey) {
                    std::string mac = extract_mac_from_path(device);
                    std::string dev_name = resolve_device_name(device, mac);

                    if (auto engine = AppEngine::instance()) {
                        auto res_ptr = std::make_shared<sdbus::Result<>>(std::move(result));
                        engine->post([this, dev_name, mac, passkey, res_ptr]() {
                            if (m_on_request_confirmation) {
                                m_on_request_confirmation(dev_name, mac, passkey, [res_ptr](bool confirmed) {
                                    if (confirmed) {
                                        res_ptr->returnResults();
                                    } else {
                                        res_ptr->returnError(sdbus::Error(sdbus::Error::Name{"org.bluez.Error.Rejected"}, "Pairing rejected by user"));
                                    }
                                });
                            } else {
                                res_ptr->returnResults();
                            }
                        });
                    } else {
                        result.returnResults();
                    }
                }),
            sdbus::registerMethod("RequestAuthorization")
                .withInputParamNames("device")
                .implementedAs([this](sdbus::Result<> result, sdbus::ObjectPath device) {
                    std::string mac = extract_mac_from_path(device);
                    std::string dev_name = resolve_device_name(device, mac);

                    if (auto engine = AppEngine::instance()) {
                        auto res_ptr = std::make_shared<sdbus::Result<>>(std::move(result));
                        engine->post([this, dev_name, mac, res_ptr]() {
                            if (m_on_request_authorization) {
                                m_on_request_authorization(dev_name, mac, [res_ptr](bool auth) {
                                    if (auth) {
                                        res_ptr->returnResults();
                                    } else {
                                        res_ptr->returnError(sdbus::Error(sdbus::Error::Name{"org.bluez.Error.Rejected"}, "Authorization rejected"));
                                    }
                                });
                            } else {
                                res_ptr->returnResults();
                            }
                        });
                    } else {
                        result.returnResults();
                    }
                }),
            sdbus::registerMethod("AuthorizeService")
                .withInputParamNames("device", "uuid")
                .implementedAs([](sdbus::Result<> result, sdbus::ObjectPath, std::string) {
                    // Automatically approve standard services (A2DP, AVRCP, HFP, PBAP contacts, etc.)
                    result.returnResults();
                }),
            sdbus::registerMethod("Cancel")
                .implementedAs([this]() {
                    if (auto engine = AppEngine::instance()) {
                        engine->post([this]() {
                            if (m_on_cancel) m_on_cancel();
                        });
                    }
                })
        ).forInterface("org.bluez.Agent1");

        m_connection->enterEventLoopAsync();

        m_manager_proxy = sdbus::createProxy(*m_connection, sdbus::ServiceName{"org.bluez"}, sdbus::ObjectPath{"/org/bluez"});
        m_manager_proxy->callMethod("RegisterAgent")
            .onInterface("org.bluez.AgentManager1")
            .withArguments(m_agent_path, "KeyboardDisplay");

        m_manager_proxy->callMethod("RequestDefaultAgent")
            .onInterface("org.bluez.AgentManager1")
            .withArguments(m_agent_path);

        m_running = true;
        return true;
    } catch (const sdbus::Error& e) {
        std::cerr << "[BluetoothAgent] Failed to register agent: " << e.getMessage() << std::endl;
        stop();
        return false;
    }
}

void BluetoothAgent::stop() {
    if (!m_running && !m_connection) return;
    m_running = false;

    try {
        if (m_manager_proxy) {
            m_manager_proxy->callMethod("UnregisterAgent")
                .onInterface("org.bluez.AgentManager1")
                .withArguments(m_agent_path);
        }
    } catch (...) {}

    if (m_connection) {
        m_connection->leaveEventLoop();
    }

    m_manager_proxy.reset();
    m_agent_object.reset();
    m_connection.reset();
}

} // namespace miqu
