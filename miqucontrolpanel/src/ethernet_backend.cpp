#include "ethernet_backend.hpp"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <filesystem>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <arpa/inet.h>

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

bool EthernetBackend::is_networkmanager_available() {
    std::error_code ec;
    return fs::exists("/usr/bin/nmcli", ec);
}

std::string EthernetBackend::prefix_to_netmask(int prefix) {
    if (prefix <= 0 || prefix > 32) return "255.255.255.0";
    uint32_t mask = (prefix == 32) ? 0xFFFFFFFF : ~(0xFFFFFFFF >> prefix);
    struct in_addr addr;
    addr.s_addr = htonl(mask);
    return inet_ntoa(addr);
}

int EthernetBackend::netmask_to_prefix(const std::string& netmask) {
    struct in_addr addr;
    if (inet_aton(netmask.c_str(), &addr) == 0) return 24;
    uint32_t mask = ntohl(addr.s_addr);
    int prefix = 0;
    while (mask & 0x80000000) {
        prefix++;
        mask <<= 1;
    }
    return prefix;
}

static void read_sysfs_link_speed(const std::string& iface, int& speed, std::string& duplex) {
    std::string speed_path = "/sys/class/net/" + iface + "/speed";
    std::string duplex_path = "/sys/class/net/" + iface + "/duplex";

    std::ifstream sf(speed_path);
    if (sf.is_open()) {
        int s = 0;
        if (sf >> s && s > 0 && s < 100000) {
            speed = s;
        }
    }

    std::ifstream df(duplex_path);
    if (df.is_open()) {
        std::string d;
        if (df >> d && !d.empty() && d != "unknown") {
            if (d == "full") duplex = "Full Duplex";
            else if (d == "half") duplex = "Half Duplex";
            else duplex = d;
        }
    }
}

static void parse_nmcli_device_props(const std::string& iface, EthernetDevice& dev) {
    std::string cmd = "nmcli -t -f GENERAL.DEVICE,GENERAL.HWADDR,GENERAL.MTU,GENERAL.STATE,GENERAL.CONNECTION,WIRED-PROPERTIES.CARRIER,IP4.ADDRESS,IP4.GATEWAY,IP4.DNS,IP6.ADDRESS device show " + iface + " 2>/dev/null";
    std::string out = run_cmd(cmd);

    std::stringstream ss(out);
    std::string line;
    std::string dns_accum;

    while (std::getline(ss, line)) {
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = line.substr(0, colon);
        std::string val = line.substr(colon + 1);

        if (key == "GENERAL.HWADDR") {
            dev.mac_address = val;
        } else if (key == "GENERAL.MTU") {
            try { dev.mtu = std::stoi(val); } catch (...) {}
        } else if (key == "GENERAL.STATE") {
            if (val.find("100") != std::string::npos || val.find("connected") != std::string::npos) {
                dev.is_connected = true;
            } else if (val.find("20") != std::string::npos || val.find("unavailable") != std::string::npos) {
                dev.has_carrier = false;
            } else if (val.find("30") != std::string::npos || val.find("disconnected") != std::string::npos) {
                dev.is_connected = false;
            }
        } else if (key == "GENERAL.CONNECTION") {
            dev.connection_name = val;
        } else if (key == "WIRED-PROPERTIES.CARRIER") {
            dev.has_carrier = (val == "on");
        } else if (key.rfind("IP4.ADDRESS", 0) == 0 && dev.ipv4_address.empty()) {
            size_t slash = val.find('/');
            if (slash != std::string::npos) {
                dev.ipv4_address = val.substr(0, slash);
                dev.ipv4_prefix = val.substr(slash + 1);
            } else {
                dev.ipv4_address = val;
            }
        } else if (key == "IP4.GATEWAY") {
            dev.ipv4_gateway = val;
        } else if (key.rfind("IP4.DNS", 0) == 0) {
            if (!dns_accum.empty()) dns_accum += ", ";
            dns_accum += val;
        } else if (key.rfind("IP6.ADDRESS", 0) == 0 && dev.ipv6_address.empty()) {
            dev.ipv6_address = val;
        }
    }
    if (!dns_accum.empty()) dev.dns_servers = dns_accum;
}

static void parse_connection_profile(EthernetDevice& dev) {
    std::string con = dev.connection_name.empty() ? "Wired connection 1" : dev.connection_name;
    std::string cmd = "nmcli -t -f connection.id,connection.uuid,ipv4.method,ipv4.addresses,ipv4.gateway,ipv4.dns connection show \"" + con + "\" 2>/dev/null";
    std::string out = run_cmd(cmd);

    std::stringstream ss(out);
    std::string line;
    while (std::getline(ss, line)) {
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = line.substr(0, colon);
        std::string val = line.substr(colon + 1);

        if (key == "connection.id" && dev.connection_name.empty()) {
            dev.connection_name = val;
        } else if (key == "connection.uuid") {
            dev.connection_uuid = val;
        } else if (key == "ipv4.method") {
            dev.ipv4_method = val.empty() ? "auto" : val;
        } else if (key == "ipv4.addresses" && !val.empty() && dev.ipv4_address.empty()) {
            size_t slash = val.find('/');
            if (slash != std::string::npos) {
                dev.ipv4_address = val.substr(0, slash);
                dev.ipv4_prefix = val.substr(slash + 1);
            } else {
                dev.ipv4_address = val;
            }
        } else if (key == "ipv4.gateway" && !val.empty() && dev.ipv4_gateway.empty()) {
            dev.ipv4_gateway = val;
        } else if (key == "ipv4.dns" && !val.empty() && dev.dns_servers.empty()) {
            dev.dns_servers = val;
        }
    }
}

std::vector<EthernetDevice> EthernetBackend::get_all_devices() {
    std::vector<EthernetDevice> list;
    std::string out = run_cmd("nmcli -t -f DEVICE,TYPE device 2>/dev/null");

    std::stringstream ss(out);
    std::string line;
    while (std::getline(ss, line)) {
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string name = line.substr(0, colon);
        std::string type = line.substr(colon + 1);

        if (type == "ethernet") {
            list.push_back(get_device(name));
        }
    }
    return list;
}

EthernetDevice EthernetBackend::get_device(const std::string& iface) {
    EthernetDevice dev;
    std::string target = iface;

    if (target.empty()) {
        auto all = get_all_devices();
        if (!all.empty()) return all.front();
        dev.name = "enp0s31f6";
        dev.is_available = false;
        dev.state_str = "No Ethernet Adapter Found";
        return dev;
    }

    dev.name = target;
    dev.is_available = true;

    parse_nmcli_device_props(target, dev);
    read_sysfs_link_speed(target, dev.speed_mbps, dev.duplex);
    parse_connection_profile(dev);

    // Human readable state string
    if (dev.is_connected) {
        dev.state_str = "Connected";
    } else if (!dev.has_carrier) {
        dev.state_str = "Cable Unplugged";
    } else {
        dev.state_str = "Disconnected";
    }

    return dev;
}

bool EthernetBackend::set_device_enabled(const std::string& iface, bool enabled) {
    std::string cmd;
    if (enabled) {
        cmd = "nmcli device connect " + iface + " 2>/dev/null";
    } else {
        cmd = "nmcli device disconnect " + iface + " 2>/dev/null";
    }
    return std::system(cmd.c_str()) == 0;
}

bool EthernetBackend::configure_ipv4_auto(const std::string& con_name) {
    std::string con = con_name.empty() ? "Wired connection 1" : con_name;
    std::string cmd = "nmcli connection modify \"" + con + "\" ipv4.method auto ipv4.addresses \"\" ipv4.gateway \"\" ipv4.dns \"\" 2>/dev/null && nmcli connection up \"" + con + "\" 2>/dev/null";
    return std::system(cmd.c_str()) == 0;
}

bool EthernetBackend::configure_ipv4_manual(
    const std::string& con_name,
    const std::string& ip_with_prefix,
    const std::string& gateway,
    const std::string& dns
) {
    std::string con = con_name.empty() ? "Wired connection 1" : con_name;
    std::string cmd = "nmcli connection modify \"" + con + "\" ipv4.method manual ipv4.addresses \"" + ip_with_prefix + "\"";
    if (!gateway.empty()) {
        cmd += " ipv4.gateway \"" + gateway + "\"";
    }
    if (!dns.empty()) {
        cmd += " ipv4.dns \"" + dns + "\"";
    }
    cmd += " 2>/dev/null && nmcli connection up \"" + con + "\" 2>/dev/null";
    return std::system(cmd.c_str()) == 0;
}

void EthernetBackend::copy_to_clipboard(const std::string& text) {
    std::string cmd = "printf '%s' \"" + text + "\" | wl-copy 2>/dev/null || printf '%s' \"" + text + "\" | xclip -selection clipboard 2>/dev/null &";
    std::system(cmd.c_str());
}

void EthernetBackend::sync_item_state(const std::shared_ptr<SettingItem>& item) {
    if (!item) return;
    if (!is_networkmanager_available()) return;

    EthernetDevice dev = get_device();
    if (!dev.is_available) return;

    item->eth_profile.name = dev.connection_name.empty() ? dev.name : dev.connection_name;
    item->eth_profile.status = dev.state_str;
    item->eth_profile.ip_address = dev.ipv4_address.empty() ? "None" : dev.ipv4_address;

    int prefix = 24;
    try { prefix = std::stoi(dev.ipv4_prefix); } catch (...) {}
    item->eth_profile.subnet = dev.ipv4_address.empty() ? "None" : prefix_to_netmask(prefix);
    item->eth_profile.gateway = dev.ipv4_gateway.empty() ? "None" : dev.ipv4_gateway;
    item->eth_profile.dns = dev.dns_servers.empty() ? "None" : dev.dns_servers;
    item->eth_profile.mode = (dev.ipv4_method == "manual") ? "Manual (Static IP)" : "Automatic (DHCP)";
    item->eth_profile.iface = dev.name;
    item->eth_profile.mac_address = dev.mac_address;
    item->eth_profile.ipv4_prefix = dev.ipv4_prefix;
    item->eth_profile.ipv6_address = dev.ipv6_address;
    item->eth_profile.speed_mbps = dev.speed_mbps;
    item->eth_profile.duplex = dev.duplex;
    item->eth_profile.has_carrier = dev.has_carrier;
    item->eth_profile.is_enabled = dev.is_enabled;
    item->eth_profile.mtu = dev.mtu;

    // Telemetry
    std::vector<TelemetryEntry> tele;
    tele.push_back({"Interface", dev.name});
    if (!dev.mac_address.empty()) {
        tele.push_back({"MAC Address", dev.mac_address});
    }
    tele.push_back({"Carrier Status", dev.has_carrier ? "Physical Link Active" : "Cable Unplugged"});
    if (dev.has_carrier && dev.speed_mbps > 0) {
        tele.push_back({"Link Speed", std::to_string(dev.speed_mbps) + " Mbps (" + dev.duplex + ")"});
    }
    tele.push_back({"MTU", std::to_string(dev.mtu) + " bytes"});
    if (!dev.ipv6_address.empty()) {
        tele.push_back({"IPv6 Address", dev.ipv6_address});
    }
    tele.push_back({"Connection Profile", dev.connection_name.empty() ? "None" : dev.connection_name});
    item->telemetry = tele;
    item->telemetry_title = "Hardware & link specifications";
}

} // namespace miqu
