#include "bluetooth_backend.hpp"
#include "bluetooth_agent.hpp"
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
#include <thread>

namespace miqu {

namespace fs = std::filesystem;

static std::mutex s_dbus_mutex;
static std::unique_ptr<sdbus::IConnection> s_dbus_conn;
static std::unique_ptr<sdbus::IProxy> s_adapter_proxy;
static std::string s_adapter_path = "/org/bluez/hci0";

struct PipeCloser {
    void operator()(FILE* fp) const {
        if (fp) pclose(fp);
    }
};

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
        s_adapter_proxy.reset();
    }
}

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
    std::thread([cmd]() {
        std::string full_cmd = cmd + " 2>/dev/null";
        std::system(full_cmd.c_str());
    }).detach();
}

bool BluetoothBackend::is_bluetooth_installed() {
    std::error_code ec;
    return fs::exists("/usr/bin/bluetoothctl", ec);
}

bool BluetoothBackend::is_bluetooth_available() {
    if (!is_bluetooth_installed()) return false;
    auto info = get_adapter_info();
    return info.available;
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

            size_t slash_pos = s_adapter_path.rfind('/');
            if (slash_pos != std::string::npos) {
                info.iface = s_adapter_path.substr(slash_pos + 1);
            }
            return info;
        } catch (...) {
            std::lock_guard<std::mutex> lock(s_dbus_mutex);
            s_adapter_proxy.reset();
        }
    }

    std::string out = run_cmd("bluetoothctl show 2>/dev/null");
    if (out.empty() || out.find("No default controller") != std::string::npos) {
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
            std::lock_guard<std::mutex> lock(s_dbus_mutex);
            s_adapter_proxy.reset();
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
            std::lock_guard<std::mutex> lock(s_dbus_mutex);
            s_adapter_proxy.reset();
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
                return true;
            }
            std::cerr << "[BluetoothBackend] StartDiscovery via DBus failed: " << e.getMessage() << "\n";
            std::lock_guard<std::mutex> lock(s_dbus_mutex);
            s_adapter_proxy.reset();
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
        } catch (const sdbus::Error&) {}
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
    std::string s = icon;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    if (s.find("audio") != std::string::npos || s.find("headset") != std::string::npos ||
        s.find("headphones") != std::string::npos || s.find("speaker") != std::string::npos) {
        return BluetoothDeviceCategory::Audio;
    }
    if (s.find("mouse") != std::string::npos || s.find("keyboard") != std::string::npos ||
        s.find("tablet") != std::string::npos || s.find("pen") != std::string::npos ||
        s.find("input") != std::string::npos || s.find("gamepad") != std::string::npos ||
        s.find("joystick") != std::string::npos) {
        return BluetoothDeviceCategory::Input;
    }
    if (s.find("phone") != std::string::npos) {
        return BluetoothDeviceCategory::Phone;
    }
    if (s.find("computer") != std::string::npos || s.find("laptop") != std::string::npos) {
        return BluetoothDeviceCategory::Computer;
    }
    return BluetoothDeviceCategory::Other;
}

std::string BluetoothBackend::resolve_icon(const std::string& bluez_icon, BluetoothDeviceCategory cat) {
    std::string s = bluez_icon;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    if (cat == BluetoothDeviceCategory::Audio) {
        if (s.find("speaker") != std::string::npos) return "󰓃";
        return "󰋋";
    }
    if (cat == BluetoothDeviceCategory::Input) {
        if (s.find("keyboard") != std::string::npos) return "󰌌";
        if (s.find("gamepad") != std::string::npos || s.find("joystick") != std::string::npos) return "󰊴";
        if (s.find("tablet") != std::string::npos || s.find("pen") != std::string::npos) return "󰓹";
        return "󰍽";
    }
    if (cat == BluetoothDeviceCategory::Phone) {
        return "󰏲";
    }
    if (cat == BluetoothDeviceCategory::Computer) {
        return "󰌢";
    }
    return "󰂯";
}

bool BluetoothBackend::query_devices_dbus(std::vector<BluetoothDevice>& out_paired, std::vector<BluetoothDevice>& out_available) {
    ensure_adapter_proxy();
    std::unique_ptr<sdbus::IProxy> mgr;
    {
        std::lock_guard<std::mutex> lock(s_dbus_mutex);
        if (!s_dbus_conn) return false;
        mgr = sdbus::createProxy(*s_dbus_conn, sdbus::ServiceName{"org.bluez"}, sdbus::ObjectPath{"/"});
    }
    if (!mgr) return false;

    try {
        std::map<sdbus::ObjectPath, std::map<std::string, std::map<std::string, sdbus::Variant>>> objects;
        mgr->callMethod("GetManagedObjects")
           .onInterface("org.freedesktop.DBus.ObjectManager")
           .storeResultsTo(objects);

        for (const auto& [path, ifaces] : objects) {
            auto dev_it = ifaces.find("org.bluez.Device1");
            if (dev_it == ifaces.end()) continue;

            const auto& props = dev_it->second;

            BluetoothDevice dev;
            auto addr_it = props.find("Address");
            if (addr_it != props.end()) {
                try { dev.mac = addr_it->second.get<std::string>(); } catch (...) {}
            }
            if (dev.mac.empty()) continue;

            auto alias_it = props.find("Alias");
            if (alias_it != props.end()) {
                try { dev.name = alias_it->second.get<std::string>(); } catch (...) {}
            }
            if (dev.name.empty()) {
                auto name_it = props.find("Name");
                if (name_it != props.end()) {
                    try { dev.name = name_it->second.get<std::string>(); } catch (...) {}
                }
            }
            if (dev.name.empty()) dev.name = dev.mac;
            BluetoothAgent::cache_device_name(dev.mac, dev.name);

            auto conn_it = props.find("Connected");
            if (conn_it != props.end()) {
                try { dev.is_connected = conn_it->second.get<bool>(); } catch (...) {}
            }

            // Strictly check Paired state (Trusted != Paired)
            auto paired_it = props.find("Paired");
            if (paired_it != props.end()) {
                try { dev.is_paired = paired_it->second.get<bool>(); } catch (...) {}
            } else {
                dev.is_paired = false;
            }

            std::string bluez_icon;
            auto icon_it = props.find("Icon");
            if (icon_it != props.end()) {
                try { bluez_icon = icon_it->second.get<std::string>(); } catch (...) {}
            }

            auto cat = categorize_icon(bluez_icon);
            switch (cat) {
                case BluetoothDeviceCategory::Audio: dev.category = "Audio"; break;
                case BluetoothDeviceCategory::Input: dev.category = "Input"; break;
                case BluetoothDeviceCategory::Phone: dev.category = "Phone"; break;
                case BluetoothDeviceCategory::Computer: dev.category = "Computer"; break;
                default: dev.category = "Other"; break;
            }
            dev.icon = resolve_icon(bluez_icon, cat);

            auto batt_it = ifaces.find("org.bluez.Battery1");
            if (batt_it != ifaces.end()) {
                auto pct_it = batt_it->second.find("Percentage");
                if (pct_it != batt_it->second.end()) {
                    try {
                        uint8_t pct = pct_it->second.get<uint8_t>();
                        dev.battery_percentage = pct;
                        dev.battery = std::to_string(pct) + "%";
                    } catch (...) {
                        try {
                            int pct = pct_it->second.get<int>();
                            dev.battery_percentage = pct;
                            dev.battery = std::to_string(pct) + "%";
                        } catch (...) {}
                    }
                }
            }

            if (dev.is_connected) {
                dev.status = "Connected";
            } else if (dev.is_paired) {
                dev.status = "Paired";
            } else {
                dev.status = "Tap to pair";
            }

            if (dev.is_paired) {
                out_paired.push_back(dev);
            } else {
                out_available.push_back(dev);
            }
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[BluetoothBackend] DBus query devices failed: " << e.what() << "\n";
        return false;
    }
}

BluetoothDevice BluetoothBackend::parse_device_info_cli(
    const std::string& mac,
    const std::string& fallback_name,
    bool is_connected_fallback
) {
    BluetoothDevice item;
    item.mac = mac;
    item.name = fallback_name.empty() ? mac : fallback_name;
    item.is_connected = is_connected_fallback;
    item.is_paired = true;

    std::string info_out = run_cmd("bluetoothctl info " + mac + " 2>/dev/null");
    if (info_out.empty()) {
        item.icon = "󰂯";
        item.status = item.is_connected ? "Connected" : "Paired";
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

    auto cat = categorize_icon(raw_icon);
    switch (cat) {
        case BluetoothDeviceCategory::Audio: item.category = "Audio"; break;
        case BluetoothDeviceCategory::Input: item.category = "Input"; break;
        case BluetoothDeviceCategory::Phone: item.category = "Phone"; break;
        case BluetoothDeviceCategory::Computer: item.category = "Computer"; break;
        default: item.category = "Other"; break;
    }
    item.icon = resolve_icon(raw_icon, cat);

    size_t batt_pos = info_out.find("Battery Percentage: ");
    if (batt_pos != std::string::npos) {
        size_t open_paren = info_out.find('(', batt_pos);
        size_t close_paren = info_out.find(')', open_paren);
        if (open_paren != std::string::npos && close_paren != std::string::npos) {
            std::string num = info_out.substr(open_paren + 1, close_paren - open_paren - 1);
            item.battery_percentage = std::atoi(num.c_str());
            item.battery = num + "%";
        }
    }

    if (item.is_connected) {
        item.status = "Connected";
    } else if (item.is_paired) {
        item.status = "Paired";
    } else {
        item.status = "Tap to pair";
    }
    return item;
}

bool BluetoothBackend::query_devices_cli(std::vector<BluetoothDevice>& out_paired, std::vector<BluetoothDevice>& out_available) {
    std::string paired_out = run_cmd("bluetoothctl devices Paired 2>/dev/null");
    std::string all_out = run_cmd("bluetoothctl devices 2>/dev/null");
    std::string conn_out = run_cmd("bluetoothctl devices Connected 2>/dev/null");

    std::unordered_set<std::string> paired_macs;

    std::istringstream p_stream(paired_out);
    std::string p_line;
    while (std::getline(p_stream, p_line)) {
        if (p_line.rfind("Device ", 0) != 0) continue;
        size_t p2 = p_line.find(' ', 7);
        if (p2 == std::string::npos) continue;

        std::string mac = p_line.substr(7, p2 - 7);
        std::string name = p_line.substr(p2 + 1);
        bool is_conn = (conn_out.find(mac) != std::string::npos);

        BluetoothAgent::cache_device_name(mac, name);
        paired_macs.insert(mac);
        out_paired.push_back(parse_device_info_cli(mac, name, is_conn));
    }

    std::istringstream stream(all_out);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.rfind("Device ", 0) != 0) continue;
        size_t p2 = line.find(' ', 7);
        if (p2 == std::string::npos) continue;

        std::string mac = line.substr(7, p2 - 7);
        if (paired_macs.count(mac)) continue;

        std::string name = line.substr(p2 + 1);
        if (name.empty()) name = mac;
        BluetoothAgent::cache_device_name(mac, name);

        BluetoothDevice dev;
        dev.mac = mac;
        dev.name = name;
        dev.is_paired = false;
        dev.is_connected = false;
        dev.icon = "󰂯";
        dev.status = "Tap to pair";
        out_available.push_back(dev);
    }
    return true;
}

std::vector<BluetoothDevice> BluetoothBackend::get_paired_devices() {
    std::vector<BluetoothDevice> paired;
    std::vector<BluetoothDevice> available;
    if (!query_devices_dbus(paired, available)) {
        query_devices_cli(paired, available);
    }
    return paired;
}

std::vector<BluetoothDevice> BluetoothBackend::get_available_devices() {
    std::vector<BluetoothDevice> paired;
    std::vector<BluetoothDevice> available;
    if (!query_devices_dbus(paired, available)) {
        query_devices_cli(paired, available);
    }
    return available;
}

std::vector<BluetoothDevice> BluetoothBackend::get_all_devices() {
    std::vector<BluetoothDevice> paired;
    std::vector<BluetoothDevice> available;
    if (!query_devices_dbus(paired, available)) {
        query_devices_cli(paired, available);
    }
    paired.insert(paired.end(), available.begin(), available.end());
    return paired;
}

bool BluetoothBackend::is_device_connected(const std::string& mac) {
    if (mac.empty()) return false;
    ensure_adapter_proxy();
    std::string dev_mac = mac;
    std::replace(dev_mac.begin(), dev_mac.end(), ':', '_');
    std::string path = s_adapter_path + "/dev_" + dev_mac;

    std::unique_ptr<sdbus::IProxy> dev_proxy;
    {
        std::lock_guard<std::mutex> lock(s_dbus_mutex);
        if (s_dbus_conn) {
            dev_proxy = sdbus::createProxy(*s_dbus_conn, sdbus::ServiceName{"org.bluez"}, sdbus::ObjectPath{path});
        }
    }

    if (dev_proxy) {
        try {
            return dev_proxy->getProperty("Connected").onInterface("org.bluez.Device1").get<bool>();
        } catch (...) {}
    }

    std::string info = run_cmd("bluetoothctl info " + mac + " 2>/dev/null");
    return (info.find("Connected: yes") != std::string::npos);
}

bool BluetoothBackend::connect_device(const std::string& mac) {
    ensure_adapter_proxy();
    std::string dev_mac = mac;
    std::replace(dev_mac.begin(), dev_mac.end(), ':', '_');
    std::string path = s_adapter_path + "/dev_" + dev_mac;

    std::unique_ptr<sdbus::IProxy> dev_proxy;
    {
        std::lock_guard<std::mutex> lock(s_dbus_mutex);
        if (s_dbus_conn) {
            dev_proxy = sdbus::createProxy(*s_dbus_conn, sdbus::ServiceName{"org.bluez"}, sdbus::ObjectPath{path});
        }
    }

    if (dev_proxy) {
        try {
            dev_proxy->callMethod("Connect").onInterface("org.bluez.Device1");
            return true;
        } catch (const sdbus::Error& e) {
            std::cerr << "[BluetoothBackend] DBus connect failed: " << e.getMessage() << ", trying bluetoothctl...\n";
        }
    }

    std::string out = run_cmd("bluetoothctl connect " + mac + " 2>&1");
    return (out.find("Connection successful") != std::string::npos || is_device_connected(mac));
}

bool BluetoothBackend::disconnect_device(const std::string& mac) {
    ensure_adapter_proxy();
    std::string dev_mac = mac;
    std::replace(dev_mac.begin(), dev_mac.end(), ':', '_');
    std::string path = s_adapter_path + "/dev_" + dev_mac;

    std::unique_ptr<sdbus::IProxy> dev_proxy;
    {
        std::lock_guard<std::mutex> lock(s_dbus_mutex);
        if (s_dbus_conn) {
            dev_proxy = sdbus::createProxy(*s_dbus_conn, sdbus::ServiceName{"org.bluez"}, sdbus::ObjectPath{path});
        }
    }

    if (dev_proxy) {
        try {
            dev_proxy->callMethod("Disconnect").onInterface("org.bluez.Device1");
            return true;
        } catch (const sdbus::Error& e) {
            std::cerr << "[BluetoothBackend] DBus disconnect failed: " << e.getMessage() << "\n";
        }
    }

    run_cmd("bluetoothctl disconnect " + mac + " 2>/dev/null");
    return true;
}

bool BluetoothBackend::pair_and_connect(const std::string& mac) {
    ensure_adapter_proxy();
    std::string dev_mac = mac;
    std::replace(dev_mac.begin(), dev_mac.end(), ':', '_');
    std::string path = s_adapter_path + "/dev_" + dev_mac;

    std::unique_ptr<sdbus::IProxy> dev_proxy;
    {
        std::lock_guard<std::mutex> lock(s_dbus_mutex);
        if (s_dbus_conn) {
            dev_proxy = sdbus::createProxy(*s_dbus_conn, sdbus::ServiceName{"org.bluez"}, sdbus::ObjectPath{path});
        }
    }

    if (dev_proxy) {
        try {
            dev_proxy->callMethod("Pair").onInterface("org.bluez.Device1");
            try {
                dev_proxy->setProperty("Trusted").onInterface("org.bluez.Device1").toValue(true);
            } catch (...) {}
            dev_proxy->callMethod("Connect").onInterface("org.bluez.Device1");
            return true;
        } catch (const sdbus::Error& e) {
            std::cerr << "[BluetoothBackend] DBus Pair/Connect failed: " << e.getMessage() << ", trying bluetoothctl...\n";
        }
    }

    std::string cmd = "bluetoothctl trust " + mac + " 2>/dev/null; "
                      "bluetoothctl pair " + mac + " 2>/dev/null; "
                      "bluetoothctl connect " + mac + " 2>/dev/null";
    run_cmd_async(cmd);
    return true;
}

bool BluetoothBackend::forget_device(const std::string& mac) {
    ensure_adapter_proxy();
    std::string dev_mac = mac;
    std::replace(dev_mac.begin(), dev_mac.end(), ':', '_');
    std::string path = s_adapter_path + "/dev_" + dev_mac;

    std::unique_ptr<sdbus::IProxy> adapter_proxy;
    {
        std::lock_guard<std::mutex> lock(s_dbus_mutex);
        if (s_dbus_conn) {
            adapter_proxy = sdbus::createProxy(*s_dbus_conn, sdbus::ServiceName{"org.bluez"}, sdbus::ObjectPath{s_adapter_path});
        }
    }

    if (adapter_proxy) {
        try {
            adapter_proxy->callMethod("RemoveDevice")
                .onInterface("org.bluez.Adapter1")
                .withArguments(sdbus::ObjectPath{path});
            return true;
        } catch (const sdbus::Error& e) {
            std::cerr << "[BluetoothBackend] DBus RemoveDevice failed: " << e.getMessage() << "\n";
        }
    }

    run_cmd("bluetoothctl remove " + mac + " 2>/dev/null");
    return true;
}

void BluetoothBackend::copy_to_clipboard(const std::string& text) {
    std::string cmd = "printf '%s' \"" + text + "\" | wl-copy 2>/dev/null || printf '%s' \"" + text + "\" | xclip -selection clipboard 2>/dev/null &";
    std::system(cmd.c_str());
}

void BluetoothBackend::sync_item_state(const std::shared_ptr<SettingItem>& item) {
    if (!item) return;
    if (!is_bluetooth_installed() || !is_bluetooth_available()) return;

    BluetoothAdapterInfo adapter = get_adapter_info();
    item->switch_val = adapter.powered;

    if (adapter.powered) {
        item->bt_devices = get_all_devices();
    } else {
        item->bt_devices.clear();
    }

    std::vector<TelemetryEntry> tele;
    if (!adapter.iface.empty()) {
        std::string ctrl_val = adapter.iface;
        if (!adapter.name.empty()) {
            ctrl_val += " (" + adapter.name + ")";
        }
        tele.push_back({"Controller", ctrl_val});
    }
    if (!adapter.mac.empty()) {
        tele.push_back({"BD_ADDR", adapter.mac});
    }
    tele.push_back({"Power State", adapter.powered ? "Powered On" : "Powered Off"});
    tele.push_back({"Discoverable", adapter.discoverable ? "Yes (Visible to nearby devices)" : "No"});
    tele.push_back({"Discovery Status", adapter.discovering ? "Scanning Active" : "Idle"});

    std::string connected_names;
    int conn_count = 0;
    for (const auto& dev : item->bt_devices) {
        if (dev.is_connected) {
            conn_count++;
            if (!connected_names.empty()) connected_names += ", ";
            connected_names += dev.name;
            if (!dev.battery.empty()) {
                connected_names += " (" + dev.battery + ")";
            }
        }
    }
    if (conn_count > 0) {
        tele.push_back({"Connected Device" + std::string(conn_count > 1 ? "s" : ""), connected_names});
    }

    item->telemetry = tele;
}

} // namespace miqu
