#pragma once

#include "modules/bluetooth/bluetooth_types.hpp"
#include <vector>
#include <string>

namespace miqu {

class BluetoothBackend {
public:
    static bool is_bluetooth_installed();
    static bool is_obex_installed();
    static BluetoothAdapterInfo get_adapter_info();
    
    static bool set_powered(bool enabled);
    static bool set_discoverable(bool enabled);
    static bool start_scan();
    static bool stop_scan();
    static bool is_scanning();

    static std::vector<BluetoothDeviceItem> get_paired_devices();
    static std::vector<BluetoothDeviceItem> get_available_devices();

    static bool connect_device(const std::string& mac);
    static bool disconnect_device(const std::string& mac);
    static bool pair_and_connect(const std::string& mac);
    static bool forget_device(const std::string& mac);

    static bool launch_send_file(const std::string& target_mac = "");
    static std::string get_received_files_directory();
    static bool copy_to_clipboard(const std::string& text);

private:
    static std::string run_cmd(const std::string& cmd);
    static void run_cmd_async(const std::string& cmd);
    static BluetoothDeviceCategory categorize_icon(const std::string& icon);
    static std::string resolve_icon(const std::string& bluez_icon, BluetoothDeviceCategory cat);
    static BluetoothDeviceItem parse_device_info(const std::string& mac, const std::string& fallback_name, bool is_connected_fallback);
};

} // namespace miqu
