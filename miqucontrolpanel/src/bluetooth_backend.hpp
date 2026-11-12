#pragma once

#include "settings_model.hpp"
#include <vector>
#include <string>
#include <memory>

namespace miqu {

enum class BluetoothDeviceCategory {
    Audio,
    Input,
    Phone,
    Computer,
    Other
};

struct BluetoothAdapterInfo {
    std::string iface = "hci0";
    std::string name = "Bluetooth Adapter";
    std::string mac;
    bool available = false;
    bool powered = false;
    bool discoverable = false;
    bool discovering = false;
    bool soft_blocked = false;
};

class BluetoothBackend {
public:
    static bool is_bluetooth_installed();
    static bool is_bluetooth_available();
    static BluetoothAdapterInfo get_adapter_info();

    static bool set_powered(bool enabled);
    static bool set_discoverable(bool enabled);
    static bool start_scan();
    static bool stop_scan();
    static bool is_scanning();

    static std::vector<BluetoothDevice> get_paired_devices();
    static std::vector<BluetoothDevice> get_available_devices();
    static std::vector<BluetoothDevice> get_all_devices();
    static bool is_device_connected(const std::string& mac);

    static bool connect_device(const std::string& mac);
    static bool disconnect_device(const std::string& mac);
    static bool pair_and_connect(const std::string& mac);
    static bool forget_device(const std::string& mac);

    static void sync_item_state(const std::shared_ptr<SettingItem>& item);
    static BluetoothDeviceCategory categorize_icon(const std::string& icon);
    static std::string resolve_icon(const std::string& bluez_icon, BluetoothDeviceCategory cat);
    static void copy_to_clipboard(const std::string& text);

private:
    static std::string run_cmd(const std::string& cmd);
    static void run_cmd_async(const std::string& cmd);
    static bool query_devices_dbus(std::vector<BluetoothDevice>& out_paired, std::vector<BluetoothDevice>& out_available);
    static bool query_devices_cli(std::vector<BluetoothDevice>& out_paired, std::vector<BluetoothDevice>& out_available);
    static BluetoothDevice parse_device_info_cli(const std::string& mac, const std::string& fallback_name, bool is_connected_fallback);
};

} // namespace miqu
