#include "firewall_backend.hpp"
#include <fstream>
#include <sstream>
#include <array>
#include <memory>
#include <cstdlib>
#include <unistd.h>
#include <algorithm>

namespace miqu {

namespace {

struct PipeCloser {
    void operator()(FILE* fp) const { if (fp) pclose(fp); }
};

std::string exec_cmd(const std::string& cmd) {
    std::array<char, 256> buffer;
    std::string result;
    std::unique_ptr<FILE, PipeCloser> pipe(popen(cmd.c_str(), "r"));
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    // Trim trailing whitespace and newlines
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r' || result.back() == ' ')) {
        result.pop_back();
    }
    return result;
}

std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

bool run_pkexec(const std::string& ufw_args) {
    std::string cmd = "pkexec /usr/bin/ufw " + ufw_args + " 2>/dev/null";
    return std::system(cmd.c_str()) == 0;
}

} // namespace

bool FirewallBackend::is_available() {
    return access("/usr/bin/ufw", X_OK) == 0;
}

bool FirewallBackend::is_enabled() {
    std::string conf = read_file("/etc/ufw/ufw.conf");
    if (conf.find("ENABLED=yes") != std::string::npos) {
        return true;
    }
    std::string act = exec_cmd("systemctl is-active ufw 2>/dev/null");
    return (act == "active");
}

std::string FirewallBackend::get_incoming_policy() {
    std::string def = read_file("/etc/default/ufw");
    if (def.find("DEFAULT_INPUT_POLICY=\"DROP\"") != std::string::npos) {
        return "Drop (Deny)";
    }
    if (def.find("DEFAULT_INPUT_POLICY=\"REJECT\"") != std::string::npos) {
        return "Reject";
    }
    if (def.find("DEFAULT_INPUT_POLICY=\"ACCEPT\"") != std::string::npos) {
        return "Allow";
    }
    return "Drop (Deny)";
}

std::string FirewallBackend::get_outgoing_policy() {
    std::string def = read_file("/etc/default/ufw");
    if (def.find("DEFAULT_OUTPUT_POLICY=\"ACCEPT\"") != std::string::npos) {
        return "Allow";
    }
    if (def.find("DEFAULT_OUTPUT_POLICY=\"DROP\"") != std::string::npos) {
        return "Drop (Deny)";
    }
    if (def.find("DEFAULT_OUTPUT_POLICY=\"REJECT\"") != std::string::npos) {
        return "Reject";
    }
    return "Allow";
}

bool FirewallBackend::is_service_allowed(const std::string& service_key) {
    std::string rules = read_file("/etc/ufw/user.rules");
    if (rules.empty()) return false;

    if (service_key == "kdeconnect") {
        return rules.find("1714:1764") != std::string::npos || rules.find("1714") != std::string::npos;
    }
    if (service_key == "ssh") {
        return rules.find("--dport 22 ") != std::string::npos ||
               rules.find("allow tcp 22 ") != std::string::npos ||
               rules.find(" 22 ") != std::string::npos;
    }
    if (service_key == "samba") {
        return rules.find("--dport 445 ") != std::string::npos ||
               rules.find("allow tcp 445 ") != std::string::npos;
    }
    if (service_key == "cups") {
        return rules.find("--dport 631 ") != std::string::npos ||
               rules.find("allow tcp 631 ") != std::string::npos ||
               rules.find("allow udp 631 ") != std::string::npos;
    }
    return false;
}

std::vector<FirewallCustomRule> FirewallBackend::get_custom_rules() {
    std::vector<FirewallCustomRule> list;
    std::string rules = read_file("/etc/ufw/user.rules");
    if (rules.empty()) return list;

    std::stringstream ss(rules);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.rfind("### tuple ###", 0) == 0) {
            std::stringstream lss(line);
            std::string t1, t2, t3, action, proto, port;
            lss >> t1 >> t2 >> t3 >> action >> proto >> port;
            if (port.empty()) continue;

            // Skip default known services
            if (port == "1714:1764" || port == "22" || port == "445" || port == "139" || port == "631") {
                continue;
            }

            FirewallCustomRule r;
            r.name = "Custom Port " + port;
            r.port_proto = port + "/" + proto;
            r.enabled = (action == "allow");

            // Avoid duplicate display entries
            bool exists = false;
            for (const auto& item : list) {
                if (item.port_proto == r.port_proto) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                list.push_back(r);
            }
        }
    }
    return list;
}

bool FirewallBackend::is_logging_enabled() {
    std::string conf = read_file("/etc/ufw/ufw.conf");
    return (conf.find("LOGLEVEL=off") == std::string::npos);
}

FirewallTelemetry FirewallBackend::get_telemetry() {
    FirewallTelemetry t;

    // Active sockets
    std::string ss_out = exec_cmd("ss -t -u -a -H 2>/dev/null | wc -l");
    try {
        if (!ss_out.empty()) t.active_connections = std::stoi(ss_out);
    } catch (...) {}

    // Monitored interfaces
    std::string ifaces = exec_cmd("ip -o link show up 2>/dev/null | awk -F': ' '{print $2}' | grep -v 'lo' | paste -sd, -");
    t.monitored_interfaces = ifaces.empty() ? "None" : ifaces;

    // Blocked requests today
    std::string block_cnt = exec_cmd("journalctl -q -g '\\[UFW BLOCK\\]' --since 'today' --no-pager 2>/dev/null | wc -l");
    try {
        if (!block_cnt.empty()) t.blocked_today = std::stoi(block_cnt);
    } catch (...) {}

    // Last blocked probe event
    std::string last_log = exec_cmd("journalctl -q -g '\\[UFW BLOCK\\]' --since 'today' -n 1 --no-pager 2>/dev/null");
    if (!last_log.empty()) {
        size_t in_pos = last_log.find("IN=");
        size_t src_pos = last_log.find("SRC=");
        size_t proto_pos = last_log.find("PROTO=");

        std::string in_str = (in_pos != std::string::npos) ? last_log.substr(in_pos, last_log.find(' ', in_pos) - in_pos) : "";
        std::string src_str = (src_pos != std::string::npos) ? last_log.substr(src_pos, last_log.find(' ', src_pos) - src_pos) : "";
        std::string proto_str = (proto_pos != std::string::npos) ? last_log.substr(proto_pos, last_log.find(' ', proto_pos) - proto_pos) : "";

        if (!src_str.empty()) {
            t.last_blocked_probe = src_str + " (" + in_str + " " + proto_str + ") -> DROP";
        } else {
            t.last_blocked_probe = last_log.substr(0, std::min<size_t>(60, last_log.size()));
        }
    } else {
        t.last_blocked_probe = "No unauthorized probe events recorded today";
    }

    return t;
}

void FirewallBackend::sync_item_state(const std::shared_ptr<SettingItem>& item) {
    if (!item) return;
    if (!is_available()) return;

    item->firewall_cfg.enabled = is_enabled();
    item->firewall_cfg.incoming_policy = get_incoming_policy();
    item->firewall_cfg.outgoing_policy = get_outgoing_policy();

    item->firewall_cfg.allow_kdeconnect = is_service_allowed("kdeconnect");
    item->firewall_cfg.allow_ssh = is_service_allowed("ssh");
    item->firewall_cfg.allow_samba = is_service_allowed("samba");
    item->firewall_cfg.allow_cups = is_service_allowed("cups");

    auto custom = get_custom_rules();
    if (!custom.empty()) {
        item->firewall_cfg.custom_rules = custom;
    }

    item->firewall_cfg.monitor_enabled = is_logging_enabled();

    auto telem = get_telemetry();
    item->firewall_cfg.blocked_today = telem.blocked_today;
    item->firewall_cfg.active_connections = telem.active_connections;
    item->firewall_cfg.monitored_interfaces = telem.monitored_interfaces;
    item->firewall_cfg.last_blocked_probe = telem.last_blocked_probe;

    item->subtitle = item->firewall_cfg.enabled ? "Active" : "Disabled";
}

bool FirewallBackend::set_enabled(bool enable) {
    return run_pkexec(enable ? "--force enable" : "disable");
}

bool FirewallBackend::set_default_policy(bool incoming, const std::string& policy_val) {
    std::string pol = "deny";
    if (policy_val.find("Allow") != std::string::npos) {
        pol = "allow";
    } else if (policy_val.find("Reject") != std::string::npos) {
        pol = "reject";
    } else {
        pol = "deny";
    }
    return run_pkexec("default " + pol + (incoming ? " incoming" : " outgoing"));
}

bool FirewallBackend::toggle_service(const std::string& service_key, bool allow) {
    if (service_key == "kdeconnect") {
        if (allow) {
            bool r1 = run_pkexec("allow 1714:1764/udp comment 'KDE Connect'");
            bool r2 = run_pkexec("allow 1714:1764/tcp comment 'KDE Connect'");
            return r1 || r2;
        } else {
            run_pkexec("delete allow 1714:1764/udp");
            run_pkexec("delete allow 1714:1764/tcp");
            return true;
        }
    }
    if (service_key == "ssh") {
        if (allow) {
            return run_pkexec("allow 22/tcp comment 'SSH'");
        } else {
            return run_pkexec("delete allow 22/tcp");
        }
    }
    if (service_key == "samba") {
        if (allow) {
            bool r1 = run_pkexec("allow 445/tcp comment 'Samba'");
            bool r2 = run_pkexec("allow 139/tcp comment 'Samba'");
            return r1 || r2;
        } else {
            run_pkexec("delete allow 445/tcp");
            run_pkexec("delete allow 139/tcp");
            return true;
        }
    }
    if (service_key == "cups") {
        if (allow) {
            bool r1 = run_pkexec("allow 631/tcp comment 'CUPS'");
            bool r2 = run_pkexec("allow 631/udp comment 'CUPS'");
            return r1 || r2;
        } else {
            run_pkexec("delete allow 631/tcp");
            run_pkexec("delete allow 631/udp");
            return true;
        }
    }
    return false;
}

bool FirewallBackend::add_custom_rule(const std::string& name, const std::string& port_proto) {
    std::string arg = "allow " + port_proto;
    if (!name.empty()) {
        arg += " comment '" + name + "'";
    }
    return run_pkexec(arg);
}

bool FirewallBackend::delete_custom_rule(const std::string& port_proto) {
    return run_pkexec("delete allow " + port_proto);
}

bool FirewallBackend::set_logging(bool enable) {
    return run_pkexec(enable ? "logging on" : "logging off");
}

bool FirewallBackend::apply_preset(const std::string& preset_name) {
    if (preset_name == "Home") {
        run_pkexec("default deny incoming");
        run_pkexec("default allow outgoing");
        toggle_service("kdeconnect", true);
        toggle_service("cups", true);
        return true;
    }
    if (preset_name == "Work") {
        run_pkexec("default deny incoming");
        run_pkexec("default allow outgoing");
        toggle_service("ssh", true);
        return true;
    }
    if (preset_name == "Public") {
        run_pkexec("default deny incoming");
        run_pkexec("default allow outgoing");
        toggle_service("kdeconnect", false);
        toggle_service("ssh", false);
        toggle_service("samba", false);
        toggle_service("cups", false);
        return true;
    }
    if (preset_name == "Cellular") {
        run_pkexec("default deny incoming");
        run_pkexec("default reject outgoing");
        return true;
    }
    return false;
}

} // namespace miqu
