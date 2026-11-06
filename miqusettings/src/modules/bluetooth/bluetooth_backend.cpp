#include "modules/bluetooth/bluetooth_backend.hpp"
#include <sdbus-c++/sdbus-c++.h>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <filesystem>
#include <sstream>
#include <algorithm>
#include <unordered_set>
#include <mutex>
#include <iostream>

namespace miqu {

namespace fs = std::filesystem;

static std::mutex s_dbus_mutex;
static std::unique_ptr<sdbus::IConnection> s_dbus_conn;
static std::unique_ptr<sdbus::IProxy> s_adapter_proxy;
static std::string s_adapter_path = "/org/bluez/hci0";

static void ensure_adapter_proxy() {
    std::lock_guard<std::mutex> lock(s_dbus_mutex);
    if (s_adapter_proxy) return;
    try {
        if (!s_dbus_conn) {
            s_dbus_conn = sdbus::createSystemBusConnection();
            s_dbus_conn->enterEventLoopAsync();
        }
        try {
            auto mgr = sdbus::createProxy(*s_dbus_conn, sdbus::ServiceName{"org.bluez"}, sdbus::ObjectPath{"/"});
            std::map<sdbus::ObjectPath, std::map<std::string, std::map<std::string, sdbus::Variant>>> objects;
            mgr->callMethod("GetManagedObjects")
               .onInterface("org.freedesktop.DBus.ObjectManager")
               .storeResultsTo(objects);
            for (const auto& [path, ifaces] : objects) {
                if (ifaces.find("org.bluez.Adapter1") != ifaces.end()) {
                    s_adapter_path = path;
                    break;
                }
            }
        } catch (...) {}

        s_adapter_proxy = sdbus::createProxy(*s_dbus_conn, sdbus::ServiceName{"org.bluez"}, sdbus::ObjectPath{s_adapter_path});
    } catch (const std::exception& e) {
        std::cerr << "[BluetoothBackend] DBus connection failed: " << e.what() << "\n";
    }
}

struct PipeCloser {
    void operator()(FILE* fp) const {
        if (fp) pclose(fp);
    }
};

std::string BluetoothBackend::run_cmd(const std::string& cmd) {
    std::string result;
    std::unique_ptr<FILE, PipeCloser> pipe(popen(cmd.c_str(), "r"));
    if (!pipe) return result;

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe.get())) {
        result += buffer;
    }
    return result;
}

void BluetoothBackend::run_cmd_async(const std::string& cmd) {
    std::string full_cmd = cmd + " 2>/dev/null &";
    std::system(full_cmd.c_str());
}

bool BluetoothBackend::is_bluetooth_installed() {
    std::error_code ec;
    return fs::exists("/usr/bin/bluetoothctl", ec);
}

bool BluetoothBackend::is_obex_installed() {
    std::error_code ec;
    if (fs::exists("/usr/lib/bluetooth/obexd", ec)) {
        return true;
    }
    std::string which_out = run_cmd("which obexctl 2>/dev/null");
    return !which_out.empty();
}

BluetoothAdapterInfo BluetoothBackend::get_adapter_info() {
    BluetoothAdapterInfo info;
    std::string rf_out = run_cmd("rfkill list bluetooth 2>/dev/null");
    if (rf_out.find("Soft blocked: yes") != std::string::npos) {
        info.soft_blocked = true;
    }

    ensure_adapter_proxy();
    if (s_adapter_proxy) {
        try {
            info.available = true;
            info.powered = s_adapter_proxy->getProperty("Powered").onInterface("org.bluez.Adapter1").get<bool>();
            info.discoverable = s_adapter_proxy->getProperty("Discoverable").onInterface("org.bluez.Adapter1").get<bool>();
            info.discovering = s_adapter_proxy->getProperty("Discovering").onInterface("org.bluez.Adapter1").get<bool>();
            info.mac = s_adapter_proxy->getProperty("Address").onInterface("org.bluez.Adapter1").get<std::string>();
            try {
                info.name = s_adapter_proxy->getProperty("Alias").onInterface("org.bluez.Adapter1").get<std::string>();
            } catch (...) {
                try {
                    info.name = s_adapter_proxy->getProperty("Name").onInterface("org.bluez.Adapter1").get<std::string>();
                } catch (...) {
                    info.name = "Bluetooth Adapter";
                }
            }
            return info;
        } catch (...) {}
    }

    std::string out = run_cmd("bluetoothctl show 2>/dev/null");
    if (out.empty() || out.find("No default controller") != std::string::npos) {
        // If rfkill exists, controller hardware exists but might be uninitialized
        info.available = !rf_out.empty();
        info.powered = false;
        info.name = "Bluetooth Adapter";
        return info;
    }

    info.available = true;
    info.powered = (out.find("Powered: yes") != std::string::npos);
    info.discoverable = (out.find("Discoverable: yes") != std::string::npos);
    info.discovering = (out.find("Discovering: yes") != std::string::npos);

    size_t ctrl_pos = out.find("Controller ");
    if (ctrl_pos != std::string::npos) {
        size_t end_mac = out.find(' ', ctrl_pos + 11);
        if (end_mac != std::string::npos) {
            info.mac = out.substr(ctrl_pos + 11, end_mac - (ctrl_pos + 11));
        }
    }

    size_t alias_pos = out.find("Alias: ");
    if (alias_pos != std::string::npos) {
        size_t end_pos = out.find('\n', alias_pos);
        info.name = out.substr(alias_pos + 7, end_pos - (alias_pos + 7));
    }
    return info;
}

bool BluetoothBackend::set_powered(bool enabled) {
    if (enabled) {
        run_cmd("rfkill unblock bluetooth 2>/dev/null");
    }
    ensure_adapter_proxy();
    if (s_adapter_proxy) {
        try {
            s_adapter_proxy->setProperty("Powered").onInterface("org.bluez.Adapter1").toValue(enabled);
            return true;
        } catch (const sdbus::Error& e) {
            std::cerr << "[BluetoothBackend] set_powered via DBus failed: " << e.getMessage() << "\n";
        }
    }

    if (enabled) {
        run_cmd_async("rfkill unblock bluetooth && bluetoothctl power on");
    } else {
        run_cmd_async("bluetoothctl power off");
    }
    return true;
}

bool BluetoothBackend::set_discoverable(bool enabled) {
    ensure_adapter_proxy();
    if (s_adapter_proxy) {
        try {
            s_adapter_proxy->setProperty("Discoverable").onInterface("org.bluez.Adapter1").toValue(enabled);
            return true;
        } catch (const sdbus::Error& e) {
            std::cerr << "[BluetoothBackend] set_discoverable via DBus failed: " << e.getMessage() << "\n";
        }
    }

    std::string cmd = enabled ? "bluetoothctl discoverable on" : "bluetoothctl discoverable off";
    run_cmd_async(cmd);
    return true;
}

bool BluetoothBackend::start_scan() {
    ensure_adapter_proxy();
    if (s_adapter_proxy) {
        try {
            s_adapter_proxy->callMethod("StartDiscovery").onInterface("org.bluez.Adapter1");
            return true;
        } catch (const sdbus::Error& e) {
            if (std::string(e.getName()).find("InProgress") != std::string::npos) {
                return true; // Already discovering
            }
            std::cerr << "[BluetoothBackend] StartDiscovery via DBus failed: " << e.getMessage() << "\n";
        }
    }

    run_cmd_async("bluetoothctl scan on");
    return true;
}

bool BluetoothBackend::stop_scan() {
    ensure_adapter_proxy();
    if (s_adapter_proxy) {
        try {
            s_adapter_proxy->callMethod("StopDiscovery").onInterface("org.bluez.Adapter1");
            return true;
        } catch (const sdbus::Error& e) {
            // Harmless if not scanning or already stopped
        }
    }

    run_cmd_async("bluetoothctl scan off");
    return true;
}

bool BluetoothBackend::is_scanning() {
    ensure_adapter_proxy();
    if (s_adapter_proxy) {
        try {
            return s_adapter_proxy->getProperty("Discovering").onInterface("org.bluez.Adapter1").get<bool>();
        } catch (...) {}
    }

    std::string out = run_cmd("bluetoothctl show 2>/dev/null");
    return (out.find("Discovering: yes") != std::string::npos);
}

BluetoothDeviceCategory BluetoothBackend::categorize_icon(const std::string& icon) {
    if (icon.find("audio") != std::string::npos || icon.find("headset") != std::string::npos ||
        icon.find("headphones") != std::string::npos || icon.find("speaker") != std::string::npos) {
        return BluetoothDeviceCategory::Audio;
    }
    if (icon.find("mouse") != std::string::npos || icon.find("keyboard") != std::string::npos ||
        icon.find("tablet") != std::string::npos || icon.find("pen") != std::string::npos ||
        icon.find("input") != std::string::npos) {
        return BluetoothDeviceCategory::Input;
    }
    return BluetoothDeviceCategory::Other;
}

std::string BluetoothBackend::resolve_icon(const std::string& bluez_icon, BluetoothDeviceCategory cat) {
    if (cat == BluetoothDeviceCategory::Audio) {
        if (bluez_icon.find("speaker") != std::string::npos) return "audio-speakers";
        return "audio-headphones";
    }
    if (cat == BluetoothDeviceCategory::Input) {
        if (bluez_icon.find("keyboard") != std::string::npos) return "input-keyboard";
        if (bluez_icon.find("tablet") != std::string::npos || bluez_icon.find("pen") != std::string::npos) {
            return "input-tablet";
        }
        return "input-mouse";
    }
    if (bluez_icon.find("phone") != std::string::npos) return "phone";
    return "bluetooth";
}

BluetoothDeviceItem BluetoothBackend::parse_device_info(
    const std::string& mac,
    const std::string& fallback_name,
    bool is_connected_fallback
) {
    BluetoothDeviceItem item;
    item.id = "dev_" + mac;
    item.mac = mac;
    item.name = fallback_name.empty() ? mac : fallback_name;
    item.is_connected = is_connected_fallback;
    item.is_paired = true;

    std::string info_out = run_cmd("bluetoothctl info " + mac + " 2>/dev/null");
    if (info_out.empty()) {
        item.icon_name = "bluetooth";
        item.category = BluetoothDeviceCategory::Other;
        item.connection_info = item.is_connected ? "Connected" : "Paired · Not Connected";
        return item;
    }

    size_t alias_pos = info_out.find("Alias: ");
    if (alias_pos != std::string::npos) {
        size_t end_pos = info_out.find('\n', alias_pos);
        item.name = info_out.substr(alias_pos + 7, end_pos - (alias_pos + 7));
    }

    item.is_connected = (info_out.find("Connected: yes") != std::string::npos);
    item.is_paired = (info_out.find("Paired: yes") != std::string::npos);

    std::string raw_icon;
    size_t icon_pos = info_out.find("Icon: ");
    if (icon_pos != std::string::npos) {
        size_t end_pos = info_out.find('\n', icon_pos);
        raw_icon = info_out.substr(icon_pos + 6, end_pos - (icon_pos + 6));
    }

    item.category = categorize_icon(raw_icon);
    item.icon_name = resolve_icon(raw_icon, item.category);

    size_t batt_pos = info_out.find("Battery Percentage: ");
    if (batt_pos != std::string::npos) {
        size_t open_paren = info_out.find('(', batt_pos);
        size_t close_paren = info_out.find(')', open_paren);
        if (open_paren != std::string::npos && close_paren != std::string::npos) {
            std::string num = info_out.substr(open_paren + 1, close_paren - open_paren - 1);
            item.battery_percentage = std::atoi(num.c_str());
        }
    }

    if (item.is_connected) {
        item.connection_info = (item.battery_percentage >= 0)
            ? ("Connected · " + std::to_string(item.battery_percentage) + "% Battery")
            : "Connected";
    } else {
        item.connection_info = "Paired · Not Connected";
    }
    return item;
}

std::vector<BluetoothDeviceItem> BluetoothBackend::get_paired_devices() {
    std::vector<BluetoothDeviceItem> list;
    std::string paired_out = run_cmd("bluetoothctl devices Paired 2>/dev/null");
    std::string conn_out = run_cmd("bluetoothctl devices Connected 2>/dev/null");

    std::istringstream stream(paired_out);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.rfind("Device ", 0) != 0) continue;
        size_t p2 = line.find(' ', 7);
        if (p2 == std::string::npos) continue;

        std::string mac = line.substr(7, p2 - 7);
        std::string name = line.substr(p2 + 1);
        bool is_conn = (conn_out.find(mac) != std::string::npos);

        list.push_back(parse_device_info(mac, name, is_conn));
    }
    return list;
}

std::vector<BluetoothDeviceItem> BluetoothBackend::get_available_devices() {
    std::vector<BluetoothDeviceItem> list;
    std::string all_out = run_cmd("bluetoothctl devices 2>/dev/null");
    std::string paired_out = run_cmd("bluetoothctl devices Paired 2>/dev/null");

    std::unordered_set<std::string> paired_macs;
    std::istringstream p_stream(paired_out);
    std::string p_line;
    while (std::getline(p_stream, p_line)) {
        if (p_line.rfind("Device ", 0) != 0) continue;
        size_t p2 = p_line.find(' ', 7);
        if (p2 != std::string::npos) {
            paired_macs.insert(p_line.substr(7, p2 - 7));
        }
    }

    std::istringstream stream(all_out);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.rfind("Device ", 0) != 0) continue;
        size_t p2 = line.find(' ', 7);
        if (p2 == std::string::npos) continue;

        std::string mac = line.substr(7, p2 - 7);
        if (paired_macs.count(mac)) continue; // Skip already paired devices

        std::string name = line.substr(p2 + 1);
        if (name.empty()) name = mac;

        BluetoothDeviceItem item;
        item.id = "avail_" + mac;
        item.mac = mac;
        item.name = name;
        item.is_paired = false;
        item.is_connected = false;
        item.icon_name = "bluetooth";
        item.category = BluetoothDeviceCategory::Other;
        item.connection_info = "Ready to pair";
        list.push_back(item);
    }
    return list;
}

bool BluetoothBackend::connect_device(const std::string& mac) {
    run_cmd_async("bluetoothctl connect " + mac);
    return true;
}

bool BluetoothBackend::disconnect_device(const std::string& mac) {
    run_cmd_async("bluetoothctl disconnect " + mac);
    return true;
}

bool BluetoothBackend::pair_and_connect(const std::string& mac) {
    std::string dev_mac = mac;
    std::replace(dev_mac.begin(), dev_mac.end(), ':', '_');
    std::string path = "/org/bluez/hci0/dev_" + dev_mac;

    std::string cmd = "bluetoothctl trust " + mac + " 2>/dev/null; "
                      "gdbus call --system --dest org.bluez --object-path " + path + " --method org.bluez.Device1.Pair >/dev/null 2>&1; "
                      "bluetoothctl connect " + mac + " >/dev/null 2>&1";
    run_cmd_async(cmd);
    return true;
}

bool BluetoothBackend::forget_device(const std::string& mac) {
    run_cmd_async("bluetoothctl remove " + mac);
    return true;
}

bool BluetoothBackend::launch_send_file(const std::string& target_mac) {
    std::string mac = target_mac;
    if (mac.empty()) {
        auto paired = get_paired_devices();
        for (const auto& dev : paired) {
            if (dev.category == BluetoothDeviceCategory::Other) {
                mac = dev.mac;
                break;
            }
        }
        if (mac.empty() && !paired.empty()) {
            mac = paired[0].mac;
        }
    }

    if (mac.empty()) {
        return false;
    }

    std::string cmd = "FILE=$(zenity --file-selection --title=\"Select File to Send via Bluetooth\" 2>/dev/null); "
                      "if [ -n \"$FILE\" ] && [ -f \"$FILE\" ]; then "
                      "  python3 -c 'import sys, dbus, os; "
                      "b = dbus.SessionBus(); "
                      "c = dbus.Interface(b.get_object(\"org.bluez.obex\", \"/org/bluez/obex\"), \"org.bluez.obex.Client1\"); "
                      "s = c.CreateSession(\"" + mac + "\", {\"Target\": \"opp\"}); "
                      "o = dbus.Interface(b.get_object(\"org.bluez.obex\", s), \"org.bluez.obex.ObjectPush1\"); "
                      "o.SendFile(os.path.abspath(\"$FILE\"))' 2>/dev/null; "
                      "fi";
    run_cmd_async(cmd);
    return true;
}

std::string BluetoothBackend::get_received_files_directory() {
    const char* home = std::getenv("HOME");
    std::string home_str = home ? home : "";
    return home_str + "/Downloads";
}

bool BluetoothBackend::copy_to_clipboard(const std::string& text) {
    std::string cmd = "printf '%s' \"" + text + "\" | wl-copy 2>/dev/null || xclip -selection clipboard 2>/dev/null";
    run_cmd_async(cmd);
    return true;
}

} // namespace miqu
