#include "airplane_backend.hpp"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <filesystem>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>
#include <linux/rfkill.h>

namespace miqu {

namespace fs = std::filesystem;

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

bool AirplaneBackend::is_available() {
    std::error_code ec;
    return fs::exists("/dev/rfkill", ec) || fs::exists("/sys/class/rfkill", ec);
}

std::vector<RadioDeviceInfo> AirplaneBackend::scan_rfkill_devices() {
    std::vector<RadioDeviceInfo> devices;
    std::error_code ec;

    if (!fs::exists("/sys/class/rfkill", ec)) {
        return devices;
    }

    for (const auto& entry : fs::directory_iterator("/sys/class/rfkill", ec)) {
        if (!entry.is_directory() && !entry.is_symlink()) continue;
        std::string path = entry.path().string();
        RadioDeviceInfo dev;

        std::string filename = entry.path().filename().string();
        if (filename.rfind("rfkill", 0) == 0) {
            try {
                dev.id = std::stoi(filename.substr(6));
            } catch (...) {
                dev.id = -1;
            }
        }

        std::ifstream name_file(path + "/name");
        if (name_file) std::getline(name_file, dev.name);

        std::ifstream type_file(path + "/type");
        if (type_file) std::getline(type_file, dev.type);

        std::ifstream soft_file(path + "/soft");
        int soft_val = 0;
        if (soft_file >> soft_val) dev.soft_blocked = (soft_val != 0);

        std::ifstream hard_file(path + "/hard");
        int hard_val = 0;
        if (hard_file >> hard_val) dev.hard_blocked = (hard_val != 0);

        devices.push_back(std::move(dev));
    }

    std::sort(devices.begin(), devices.end(), [](const RadioDeviceInfo& a, const RadioDeviceInfo& b) {
        return a.id < b.id;
    });

    return devices;
}

AirplaneState AirplaneBackend::get_state() {
    AirplaneState state;
    state.radios = scan_rfkill_devices();

    bool has_wireless = false;
    bool all_wireless_blocked = true;

    for (const auto& r : state.radios) {
        if (r.type == "wlan") {
            has_wireless = true;
            if (!r.soft_blocked) all_wireless_blocked = false;
            if (r.soft_blocked) state.wifi_blocked = true;
        } else if (r.type == "bluetooth") {
            has_wireless = true;
            if (!r.soft_blocked) all_wireless_blocked = false;
            if (r.soft_blocked) state.bluetooth_blocked = true;
        } else if (r.type == "wwan") {
            has_wireless = true;
            if (!r.soft_blocked) all_wireless_blocked = false;
            if (r.soft_blocked) state.wwan_blocked = true;
        }
        if (r.hard_blocked) {
            state.any_hard_blocked = true;
        }
    }

    std::string nm_out = run_cmd("nmcli networking 2>/dev/null");
    nm_out.erase(std::remove_if(nm_out.begin(), nm_out.end(), ::isspace), nm_out.end());
    state.global_networking_enabled = (nm_out.find("enabled") != std::string::npos);

    // Airplane mode is active if global networking is severed or all wireless radios are blocked
    if (!state.global_networking_enabled || (has_wireless && all_wireless_blocked)) {
        state.is_airplane_mode = true;
    } else {
        state.is_airplane_mode = false;
    }

    return state;
}

bool AirplaneBackend::set_rfkill_all(bool block) {
    int fd = open("/dev/rfkill", O_RDWR | O_CLOEXEC);
    if (fd >= 0) {
        struct rfkill_event event = {};
        event.op = RFKILL_OP_CHANGE_ALL;
        event.type = RFKILL_TYPE_ALL;
        event.soft = block ? 1 : 0;
        ssize_t ret = write(fd, &event, sizeof(event));
        (void)ret;
        close(fd);
    }

    std::string cmd = block ? "rfkill block all 2>/dev/null" : "rfkill unblock all 2>/dev/null";
    run_cmd(cmd);
    return true;
}

bool AirplaneBackend::set_networking_enabled(bool enable) {
    std::string cmd = enable ? "nmcli networking on 2>/dev/null" : "nmcli networking off 2>/dev/null";
    run_cmd(cmd);

    std::string radio_cmd = enable ? "nmcli radio all on 2>/dev/null" : "nmcli radio all off 2>/dev/null";
    run_cmd(radio_cmd);

    return true;
}

bool AirplaneBackend::set_bluetooth_powered(bool powered) {
    std::string cmd = powered ? "bluetoothctl power on 2>/dev/null" : "bluetoothctl power off 2>/dev/null";
    run_cmd(cmd);
    return true;
}

bool AirplaneBackend::set_airplane_mode(bool enable) {
    if (enable) {
        // Ultimate Protection: Kill all RF and sever outer network connectivity
        set_rfkill_all(true);
        set_networking_enabled(false);
        set_bluetooth_powered(false);
    } else {
        // Restore connectivity
        set_rfkill_all(false);
        set_networking_enabled(true);
        set_bluetooth_powered(true);
    }
    return true;
}

void AirplaneBackend::sync_item_state(const std::shared_ptr<SettingItem>& item) {
    if (!item) return;

    auto state = get_state();
    item->switch_val = state.is_airplane_mode;

    item->telemetry = {
        {"Isolation Level", state.is_airplane_mode ? "Air-Gapped (Isolated)" : "Standard (Connected)"},
        {"Kernel RFKill", state.is_airplane_mode ? "All Radios Blocked" : "Radios Operational"},
        {"Global Networking", state.global_networking_enabled ? "Enabled (Online)" : "Disabled (Offline)"},
        {"Wi-Fi Radio", state.wifi_blocked ? "Offline / Blocked" : "Active"},
        {"Bluetooth", state.bluetooth_blocked ? "Offline / Blocked" : "Active"},
        {"Hardware Killswitch", state.any_hard_blocked ? "Engaged (Locked)" : "Disengaged (Ready)"}
    };
}

} // namespace miqu
