#include "modules/wifi/wifi_backend.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace miqu {

namespace fs = std::filesystem;

static std::string run_cmd(const std::string& cmd) {
    std::string result;
    FILE* fp = popen(cmd.c_str(), "r");
    if (!fp) return result;

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), fp)) {
        result += buffer;
    }
    pclose(fp);
    return result;
}

static std::string find_wifi_interface() {
    std::string out = run_cmd("nmcli -t -f DEVICE,TYPE device 2>/dev/null");
    std::stringstream ss(out);
    std::string line;
    while (std::getline(ss, line)) {
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string name = line.substr(0, colon);
            std::string type = line.substr(colon + 1);
            if (type == "wifi") return name;
        }
    }
    return "";
}

bool WifiBackend::is_wifi_available() {
    std::error_code ec;
    if (!fs::exists("/usr/bin/nmcli", ec)) return false;
    return !find_wifi_interface().empty();
}

std::string WifiBackend::resolve_signal_icon(int signal) {
    if (signal >= 75) return "network-wireless-signal-excellent";
    if (signal >= 50) return "network-wireless-signal-good";
    if (signal >= 25) return "network-wireless-signal-ok";
    return "network-wireless-signal-weak";
}

WifiAdapterInfo WifiBackend::get_adapter_info() {
    WifiAdapterInfo info;
    info.iface = find_wifi_interface();
    if (info.iface.empty()) {
        info.available = false;
        info.radio_enabled = false;
        return info;
    }

    info.available = true;

    // Check radio state
    std::string radio_out = run_cmd("nmcli radio wifi 2>/dev/null");
    info.radio_enabled = (radio_out.find("enabled") != std::string::npos);

    // Query device status
    std::string cmd = "nmcli -t -f GENERAL.DEVICE,GENERAL.HWADDR,GENERAL.STATE,GENERAL.CONNECTION,IP4.ADDRESS,IP4.GATEWAY,IP4.DNS device show " + info.iface + " 2>/dev/null";
    std::string dev_out = run_cmd(cmd);

    std::stringstream ss(dev_out);
    std::string line;
    std::string dns_accum;

    while (std::getline(ss, line)) {
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = line.substr(0, colon);
        std::string val = line.substr(colon + 1);

        if (key == "GENERAL.HWADDR") {
            info.mac_address = val;
        } else if (key == "GENERAL.STATE") {
            info.is_connected = (val.find("100") != std::string::npos || val.find("connected") != std::string::npos);
        } else if (key == "GENERAL.CONNECTION") {
            info.connected_ssid = val;
        } else if (key.rfind("IP4.ADDRESS", 0) == 0 && info.ipv4_address.empty()) {
            size_t slash = val.find('/');
            info.ipv4_address = (slash != std::string::npos) ? val.substr(0, slash) : val;
        } else if (key == "IP4.GATEWAY") {
            info.ipv4_gateway = val;
        } else if (key.rfind("IP4.DNS", 0) == 0) {
            if (!dns_accum.empty()) dns_accum += ", ";
            dns_accum += val;
        }
    }
    info.dns_servers = dns_accum;

    // Query active wifi link telemetry if connected
    if (info.is_connected && !info.connected_ssid.empty()) {
        std::string w_cmd = "nmcli -t -f IN-USE,SSID,FREQ,RATE,SIGNAL,SECURITY device wifi list 2>/dev/null";
        std::string w_out = run_cmd(w_cmd);
        std::stringstream wss(w_out);
        std::string wline;

        while (std::getline(wss, wline)) {
            if (wline.rfind("*:", 0) == 0) { // Active in-use
                std::vector<std::string> parts;
                std::stringstream pss(wline);
                std::string p;
                while (std::getline(pss, p, ':')) parts.push_back(p);

                if (parts.size() >= 6) {
                    // parts[0]: "*", parts[1]: SSID, parts[2]: FREQ, parts[3]: RATE, parts[4]: SIGNAL, parts[5]: SECURITY
                    std::string freq = parts[2];
                    if (freq.find("5") != std::string::npos) info.frequency = "5 GHz";
                    else if (freq.find("6") != std::string::npos) info.frequency = "6 GHz";
                    else info.frequency = "2.4 GHz";

                    info.bitrate = parts[3];
                    try { info.signal_percent = std::stoi(parts[4]); } catch (...) {}
                    info.security = parts[5];
                }
                break;
            }
        }
    }

    return info;
}

bool WifiBackend::set_radio_enabled(bool enabled) {
    std::string cmd = enabled ? "nmcli radio wifi on 2>/dev/null" : "nmcli radio wifi off 2>/dev/null";
    return std::system(cmd.c_str()) == 0;
}

std::vector<std::string> WifiBackend::get_saved_network_names() {
    std::vector<std::string> list;
    std::string out = run_cmd("nmcli -t -f NAME,TYPE connection show 2>/dev/null");
    std::stringstream ss(out);
    std::string line;

    while (std::getline(ss, line)) {
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string name = line.substr(0, colon);
            std::string type = line.substr(colon + 1);
            if (type.find("wireless") != std::string::npos || type.find("wifi") != std::string::npos) {
                list.push_back(name);
            }
        }
    }
    return list;
}

std::vector<WifiNetwork> WifiBackend::scan_nearby_networks() {
    std::vector<WifiNetwork> results;
    std::unordered_set<std::string> saved_set;
    for (const auto& s : get_saved_network_names()) {
        saved_set.insert(s);
    }

    // Format: IN-USE:SSID:BSSID:MODE:CHAN:FREQ:RATE:SIGNAL:BARS:SECURITY
    std::string cmd = "nmcli -t -f IN-USE,SSID,BSSID,MODE,CHAN,FREQ,RATE,SIGNAL,BARS,SECURITY device wifi list 2>/dev/null";
    std::string out = run_cmd(cmd);

    std::unordered_map<std::string, WifiNetwork> best_by_ssid;
    std::stringstream ss(out);
    std::string line;

    while (std::getline(ss, line)) {
        if (line.empty()) continue;

        bool in_use = (line.rfind("*:", 0) == 0);
        size_t first_colon = line.find(':');
        if (first_colon == std::string::npos) continue;

        std::string rest = line.substr(first_colon + 1);
        std::vector<std::string> tokens;
        std::string token;
        bool escape = false;

        for (char c : rest) {
            if (escape) {
                token += c;
                escape = false;
            } else if (c == '\\') {
                escape = true;
            } else if (c == ':') {
                tokens.push_back(token);
                token.clear();
            } else {
                token += c;
            }
        }
        tokens.push_back(token);

        if (tokens.size() < 9) continue;

        std::string ssid = tokens[0];
        if (ssid.empty()) continue; // Skip hidden SSIDs

        std::string bssid = tokens[1];
        std::string freq_str = tokens[4];
        std::string rate = tokens[5];
        int signal = 0;
        try { signal = std::stoi(tokens[6]); } catch (...) {}
        std::string security = tokens[8];

        std::string freq = "2.4 GHz";
        if (freq_str.find("5") != std::string::npos || freq_str.find("52") != std::string::npos) {
            freq = "5 GHz";
        } else if (freq_str.find("6") != std::string::npos) {
            freq = "6 GHz";
        }

        bool is_saved = (saved_set.find(ssid) != saved_set.end());
        bool is_secured = (!security.empty() && security != "--");

        WifiNetwork net;
        net.ssid = ssid;
        net.bssid = bssid;
        net.signal_percent = signal;
        net.frequency = freq;
        net.security = is_secured ? security : "Open";
        net.rate = rate;
        net.is_connected = in_use;
        net.is_saved = is_saved;
        net.is_secured = is_secured;

        auto it = best_by_ssid.find(ssid);
        if (it == best_by_ssid.end() || in_use || signal > it->second.signal_percent) {
            best_by_ssid[ssid] = net;
        }
    }

    for (auto& pair : best_by_ssid) {
        results.push_back(pair.second);
    }

    // Sort: connected first, then saved first, then signal strength descending
    std::sort(results.begin(), results.end(), [](const WifiNetwork& a, const WifiNetwork& b) {
        if (a.is_connected != b.is_connected) return a.is_connected > b.is_connected;
        if (a.is_saved != b.is_saved) return a.is_saved > b.is_saved;
        return a.signal_percent > b.signal_percent;
    });

    return results;
}

bool WifiBackend::connect_network(const std::string& ssid, const std::string& password) {
    std::string cmd;
    if (password.empty()) {
        cmd = "nmcli device wifi connect \"" + ssid + "\" 2>&1";
    } else {
        cmd = "nmcli device wifi connect \"" + ssid + "\" password \"" + password + "\" 2>&1";
    }
    return std::system(cmd.c_str()) == 0;
}

bool WifiBackend::disconnect_current(const std::string& iface) {
    std::string dev = iface.empty() ? find_wifi_interface() : iface;
    if (dev.empty()) return false;
    std::string cmd = "nmcli device disconnect " + dev + " 2>/dev/null";
    return std::system(cmd.c_str()) == 0;
}

bool WifiBackend::forget_network(const std::string& ssid) {
    std::string cmd = "nmcli connection delete id \"" + ssid + "\" 2>/dev/null";
    return std::system(cmd.c_str()) == 0;
}

void WifiBackend::rescan_async() {
    std::string cmd = "nmcli device wifi rescan 2>/dev/null &";
    std::system(cmd.c_str());
}

} // namespace miqu
