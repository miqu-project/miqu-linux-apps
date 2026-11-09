#include "wifi_backend.hpp"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <filesystem>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

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
    if (signal >= 75) return "󰤨";
    if (signal >= 50) return "󰤥";
    if (signal >= 25) return "󰤢";
    if (signal >= 1)  return "󰤟";
    return "󰤮";
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
        std::string w_cmd = "nmcli -t -f IN-USE,SSID,CHAN,FREQ,RATE,SIGNAL,SECURITY device wifi list --rescan no 2>/dev/null";
        std::string w_out = run_cmd(w_cmd);
        std::stringstream wss(w_out);
        std::string wline;

        while (std::getline(wss, wline)) {
            if (wline.rfind("*:", 0) == 0) {
                std::vector<std::string> parts;
                std::stringstream pss(wline);
                std::string p;
                while (std::getline(pss, p, ':')) parts.push_back(p);

                // parts: [0]=*, [1]=SSID, [2]=CHAN, [3]=FREQ, [4]=RATE, [5]=SIGNAL, [6]=SECURITY
                if (parts.size() >= 7) {
                    info.channel = parts[2];
                    std::string freq_str = parts[3];
                    int mhz = 0;
                    try { mhz = std::stoi(freq_str); } catch (...) {}
                    if (mhz >= 5925) info.frequency = "6 GHz";
                    else if (mhz >= 4900) info.frequency = "5 GHz";
                    else info.frequency = "2.4 GHz";

                    info.bitrate = parts[4];
                    try { info.signal_percent = std::stoi(parts[5]); } catch (...) {}
                    info.security = parts[6];
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

std::vector<WifiNetwork> WifiBackend::scan_networks() {
    std::vector<WifiNetwork> results;
    std::unordered_set<std::string> saved_set;
    for (const auto& s : get_saved_network_names()) {
        saved_set.insert(s);
    }

    // Format: IN-USE:SSID:BSSID:MODE:CHAN:FREQ:RATE:SIGNAL:BARS:SECURITY
    std::string cmd = "nmcli -t -f IN-USE,SSID,BSSID,MODE,CHAN,FREQ,RATE,SIGNAL,BARS,SECURITY device wifi list --rescan no 2>/dev/null";
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
        std::string channel = tokens[3];
        std::string freq_str = tokens[4];
        std::string rate = tokens[5];
        int signal = 0;
        try { signal = std::stoi(tokens[6]); } catch (...) {}
        std::string security = tokens[8];

        int mhz = 0;
        try { mhz = std::stoi(freq_str); } catch (...) {}
        std::string freq = "2.4 GHz";
        if (mhz >= 5925) freq = "6 GHz";
        else if (mhz >= 4900) freq = "5 GHz";

        bool is_saved = (saved_set.find(ssid) != saved_set.end());
        bool is_secured = (!security.empty() && security != "--");
        std::string sec_clean = is_secured ? security : "Open";

        // Build 3rd line details: Security • Frequency (Channel) • Signal • Rate • BSSID
        std::string details = sec_clean;
        std::string freq_chan = freq;
        if (!channel.empty()) freq_chan += " (Ch " + channel + ")";
        if (!freq_chan.empty()) details += " • " + freq_chan;
        if (signal > 0) details += " • " + std::to_string(signal) + "%";
        if (!rate.empty()) details += " • " + rate;
        if (!bssid.empty()) details += " • " + bssid;

        WifiNetwork net;
        net.ssid = ssid;
        net.status = in_use ? "Connected" : (is_saved ? "Saved" : "Connect");
        net.icon = resolve_signal_icon(signal);
        net.secured = is_secured;
        net.bssid = bssid;
        net.signal_percent = signal;
        net.frequency = freq;
        net.channel = channel;
        net.security = sec_clean;
        net.rate = rate;
        net.details = details;

        auto it = best_by_ssid.find(ssid);
        if (it == best_by_ssid.end() || in_use || signal > it->second.signal_percent) {
            best_by_ssid[ssid] = net;
        }
    }

    for (auto& pair : best_by_ssid) {
        results.push_back(pair.second);
    }

    // Sort: Connected first, then Saved first, then signal strength descending
    std::sort(results.begin(), results.end(), [](const WifiNetwork& a, const WifiNetwork& b) {
        bool a_conn = (a.status == "Connected");
        bool b_conn = (b.status == "Connected");
        if (a_conn != b_conn) return a_conn > b_conn;

        bool a_saved = (a.status == "Saved");
        bool b_saved = (b.status == "Saved");
        if (a_saved != b_saved) return a_saved > b_saved;

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

void WifiBackend::sync_item_state(const std::shared_ptr<SettingItem>& item) {
    if (!item) return;
    if (!is_wifi_available()) return;

    WifiAdapterInfo info = get_adapter_info();
    item->switch_val = info.radio_enabled;

    if (info.radio_enabled) {
        item->wifi_networks = scan_networks();
    } else {
        item->wifi_networks.clear();
    }

    // Dynamic Interface Specifications (Telemetry)
    std::vector<TelemetryEntry> tele;
    if (!info.iface.empty()) {
        tele.push_back({"Interface", info.iface});
    }
    if (!info.mac_address.empty()) {
        tele.push_back({"MAC Address", info.mac_address});
    }
    if (info.is_connected && !info.connected_ssid.empty()) {
        tele.push_back({"Connected SSID", info.connected_ssid});
        if (!info.ipv4_address.empty()) tele.push_back({"IPv4 Address", info.ipv4_address});
        if (!info.ipv4_gateway.empty()) tele.push_back({"IPv4 Gateway", info.ipv4_gateway});
        if (!info.dns_servers.empty())  tele.push_back({"DNS Servers", info.dns_servers});
        if (!info.frequency.empty()) {
            std::string freq_val = info.frequency;
            if (!info.channel.empty()) freq_val += " (Channel " + info.channel + ")";
            tele.push_back({"Frequency Band", freq_val});
        }
        if (!info.bitrate.empty()) tele.push_back({"Link Speed", info.bitrate});
        if (info.signal_percent > 0) tele.push_back({"Signal Strength", std::to_string(info.signal_percent) + "%"});
        if (!info.security.empty()) tele.push_back({"Security Protocol", info.security});
    } else {
        tele.push_back({"Connection Status", info.radio_enabled ? "Disconnected" : "Wi-Fi Radio Disabled"});
    }
    item->telemetry = tele;
}

} // namespace miqu
