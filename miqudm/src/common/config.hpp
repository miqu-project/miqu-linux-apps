#pragma once

#include <string>
#include <memory>

namespace miqudm {

struct GeneralConfig {
    std::string default_session = "miquland";
    bool remember_last_user = true;
    std::string last_user = "";
    std::string greeter_command = "miquland -s /usr/bin/miqudm-greeter";
    std::string greeter_user = "greeter";
    int vt = 1;
    bool numlock = false;
};

struct AutologinConfig {
    bool enabled = false;
    std::string user = "";
    std::string session = "miquland";
};

class Config {
public:
    static Config& get();

    bool load(const std::string& custom_path = "");
    bool save(const std::string& custom_path = "");

    GeneralConfig general;
    AutologinConfig autologin;

    const std::string& get_loaded_path() const { return m_loaded_path; }

private:
    Config() = default;
    ~Config() = default;

    std::string m_loaded_path;
    static std::string find_config_file(const std::string& custom_path);
};

} // namespace miqudm
