#pragma once

#include <string>
#include <filesystem>

namespace miqudm {

struct DaemonConfig {
    std::string greeter_command = "cage -s -- /usr/bin/miqudm-greeter";
    std::string greeter_user = "greeter";
    std::string default_session = "miquland";
    bool remember_last_user = true;
    std::string last_user = "";
    int vt = 1;
    bool numlock = false;
};

struct GreeterConfig {
    std::string wallpaper = "";
    bool clock_format_24h = true;
    bool show_power_buttons = true;
    bool show_sessions = true;
    bool allow_user_switching = true;
    int card_radius = 16;
    int avatar_size = 80;
    bool test_mode = false;
};

class ConfigManager {
public:
    static ConfigManager& get();

    DaemonConfig& daemon() { return m_daemon; }
    const DaemonConfig& daemon() const { return m_daemon; }

    GreeterConfig& greeter() { return m_greeter; }
    const GreeterConfig& greeter() const { return m_greeter; }

    void load_daemon_config(const std::string& custom_path = "");
    void save_last_user(const std::string& username);

    void load_greeter_config(const std::string& custom_path = "");

    ConfigManager() = default;

private:
    DaemonConfig m_daemon;
    GreeterConfig m_greeter;
    std::string m_daemon_config_path;
    std::string m_greeter_config_path;
};

} // namespace miqudm
