#include "modules/airplane/airplane_backend.hpp"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <sstream>
#include <algorithm>

namespace miqu {

struct PipeCloser {
    void operator()(FILE* fp) const {
        if (fp) pclose(fp);
    }
};

static std::string run_cmd(const std::string& cmd) {
    std::string result;
    std::unique_ptr<FILE, PipeCloser> pipe(popen(cmd.c_str(), "r"));
    if (!pipe) return result;

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe.get())) {
        result += buffer;
    }
    return result;
}

static void parse_rfkill_line(const std::string& line, std::vector<RadioDeviceStatus>& devices) {
    if (line.empty()) return;

    if (line[0] != '\t' && line[0] != ' ') {
        // Device header: "<id>: <name>: <type>"
        size_t c1 = line.find(':');
        size_t c2 = (c1 != std::string::npos) ? line.find(':', c1 + 1) : std::string::npos;
        if (c1 != std::string::npos && c2 != std::string::npos) {
            RadioDeviceStatus dev;
            try { dev.id = std::stoi(line.substr(0, c1)); } catch (...) { dev.id = 0; }
            dev.name = line.substr(c1 + 1, c2 - c1 - 1);
            dev.name.erase(0, dev.name.find_first_not_of(" \t"));
            dev.type = line.substr(c2 + 1);
            dev.type.erase(0, dev.type.find_first_not_of(" \t"));
            devices.push_back(dev);
        }
    } else if (!devices.empty()) {
        if (line.find("Soft blocked: yes") != std::string::npos) {
            devices.back().soft_blocked = true;
        } else if (line.find("Hard blocked: yes") != std::string::npos) {
            devices.back().hard_blocked = true;
        }
    }
}

static void evaluate_radio_types(AirplaneStatus& status) {
    bool has_wlan = false;
    bool wlan_unblocked = false;
    bool has_bt = false;
    bool bt_unblocked = false;

    for (const auto& dev : status.devices) {
        std::string type_lower = dev.type;
        std::transform(type_lower.begin(), type_lower.end(), type_lower.begin(), ::tolower);

        bool is_blocked = dev.soft_blocked || dev.hard_blocked;
        if (is_blocked) status.blocked_radios++;

        if (type_lower.find("wireless lan") != std::string::npos || type_lower.find("wlan") != std::string::npos) {
            has_wlan = true;
            if (!is_blocked) wlan_unblocked = true;
        } else if (type_lower.find("bluetooth") != std::string::npos) {
            has_bt = true;
            if (!is_blocked) bt_unblocked = true;
        } else if (type_lower.find("wwan") != std::string::npos || type_lower.find("cellular") != std::string::npos) {
            status.has_wwan = true;
            if (!is_blocked) status.wwan_enabled = true;
        }
    }

    // Cross-check NetworkManager radio wifi state
    std::string nm_wifi = run_cmd("nmcli radio wifi 2>/dev/null");
    if (nm_wifi.find("disabled") != std::string::npos) {
        wlan_unblocked = false;
    }

    // Cross-check Bluetooth power state via bluetoothctl
    std::string bt_out = run_cmd("bluetoothctl show 2>/dev/null");
    if (bt_out.find("Powered: no") != std::string::npos || bt_out.empty()) {
        bt_unblocked = false;
    }

    status.wifi_enabled = has_wlan && wlan_unblocked;
    status.bluetooth_enabled = has_bt && bt_unblocked;
    status.enabled = (!status.wifi_enabled && !status.bluetooth_enabled);
}

AirplaneStatus AirplaneBackend::get_status() {
    AirplaneStatus status;
    std::string out = run_cmd("rfkill list 2>/dev/null");
    std::stringstream ss(out);
    std::string line;

    while (std::getline(ss, line)) {
        parse_rfkill_line(line, status.devices);
    }

    status.total_radios = static_cast<int>(status.devices.size());
    evaluate_radio_types(status);
    return status;
}

bool AirplaneBackend::is_enabled() {
    return get_status().enabled;
}

bool AirplaneBackend::set_enabled(bool enabled) {
    if (enabled) {
        std::system("nmcli radio all off 2>/dev/null");
        std::system("rfkill block all 2>/dev/null");
        std::system("bluetoothctl power off 2>/dev/null");
    } else {
        std::system("rfkill unblock all 2>/dev/null");
        std::system("nmcli radio all on 2>/dev/null");
        std::system("bluetoothctl power on 2>/dev/null");
    }
    return true;
}

bool AirplaneBackend::toggle() {
    bool current = is_enabled();
    return set_enabled(!current);
}

} // namespace miqu
