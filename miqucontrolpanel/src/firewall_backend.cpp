#include "firewall_backend.hpp"
#include <miqutoolkit/core/string_utils.hpp>
#include <fstream>
#include <sstream>
#include <array>
#include <memory>
#include <cstdlib>
#include <unistd.h>
#include <algorithm>
#include <filesystem>
#include <chrono>

namespace fs = std::filesystem;

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
    return StringUtils::trim(result);
}


bool run_pkexec(const std::string& cmd_str) {
    std::string full_cmd = "pkexec " + cmd_str + " 2>/dev/null";
    return std::system(full_cmd.c_str()) == 0;
}

std::string hex_decode(const std::string& hex) {
    std::string res;
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        try {
            char ch = static_cast<char>(std::stoi(hex.substr(i, 2), nullptr, 16));
            if (ch >= 32 && ch <= 126) res += ch;
        } catch (...) {
            break;
        }
    }
    return res;
}

struct InternalRule {
    int id = 0;
    std::string action = "ALLOW";
    std::string proto = "tcp";
    std::string port;
    std::string name;
    std::string source = "Anywhere";
    std::string raw_line;
};

void parse_rules_file(const std::string& path, std::vector<InternalRule>& out_rules, int& next_id) {
    std::ifstream file(path);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        line = StringUtils::trim(line);
        if (line.empty() || line[0] == '*' || (line[0] == '#' && !StringUtils::starts_with(line, "### tuple ###"))) {
            continue;
        }

        if (StringUtils::starts_with(line, "-A ufw-user-input") || StringUtils::starts_with(line, "-A ufw6-user-input")) {
            InternalRule rule;
            rule.id = next_id++;
            rule.raw_line = line;

            if (line.find("-j ACCEPT") != std::string::npos) {
                rule.action = "ALLOW";
            } else if (line.find("-j DROP") != std::string::npos) {
                rule.action = "DENY";
            } else if (line.find("-j REJECT") != std::string::npos) {
                rule.action = "REJECT";
            } else {
                continue;
            }

            size_t proto_pos = line.find("-p ");
            if (proto_pos != std::string::npos) {
                size_t space_pos = line.find(' ', proto_pos + 3);
                if (space_pos != std::string::npos) {
                    rule.proto = line.substr(proto_pos + 3, space_pos - (proto_pos + 3));
                }
            }

            size_t dport_pos = line.find("--dport ");
            if (dport_pos != std::string::npos) {
                size_t space_pos = line.find(' ', dport_pos + 8);
                rule.port = (space_pos != std::string::npos) ?
                    line.substr(dport_pos + 8, space_pos - (dport_pos + 8)) :
                    line.substr(dport_pos + 8);
            } else {
                size_t dports_pos = line.find("--dports ");
                if (dports_pos != std::string::npos) {
                    size_t space_pos = line.find(' ', dports_pos + 9);
                    rule.port = (space_pos != std::string::npos) ?
                        line.substr(dports_pos + 9, space_pos - (dports_pos + 9)) :
                        line.substr(dports_pos + 9);
                } else {
                    rule.port = "All Traffic";
                }
            }

            size_t s_pos = line.find("-s ");
            if (s_pos != std::string::npos) {
                size_t space_pos = line.find(' ', s_pos + 3);
                rule.source = (space_pos != std::string::npos) ?
                    line.substr(s_pos + 3, space_pos - (s_pos + 3)) :
                    line.substr(s_pos + 3);
            }

            size_t comment_pos = line.find("--comment \"ufw-user-");
            if (comment_pos != std::string::npos) {
                size_t start = comment_pos + 20;
                size_t end = line.find('"', start);
                if (end != std::string::npos) {
                    rule.name = line.substr(start, end - start);
                }
            } else {
                size_t alt_c = line.find("--comment \"");
                if (alt_c != std::string::npos) {
                    size_t start = alt_c + 11;
                    size_t end = line.find('"', start);
                    if (end != std::string::npos) {
                        rule.name = line.substr(start, end - start);
                    }
                }
            }

            out_rules.push_back(rule);
        } else if (StringUtils::starts_with(line, "### tuple ###")) {
            std::string tline = line.substr(13);
            tline = StringUtils::trim(tline);
            std::stringstream ss(tline);
            std::string action, proto, port, dst, sport, src;
            if (ss >> action >> proto >> port >> dst >> sport >> src) {
                std::string comment;
                size_t c_pos = line.find("comment=");
                if (c_pos != std::string::npos) {
                    std::string hex = line.substr(c_pos + 8);
                    size_t sp = hex.find(' ');
                    if (sp != std::string::npos) hex = hex.substr(0, sp);
                    comment = hex_decode(hex);
                }

                // Check if already in out_rules to enrich comment
                bool found = false;
                for (auto& r : out_rules) {
                    if (r.port == port && r.proto == proto) {
                        if (r.name.empty() && !comment.empty()) {
                            r.name = comment;
                        }
                        found = true;
                        break;
                    }
                }
                if (!found && !port.empty() && port != "any") {
                    InternalRule r;
                    r.id = next_id++;
                    r.action = StringUtils::to_upper(action);
                    r.proto = StringUtils::to_lower(proto);
                    r.port = port;
                    r.name = comment;
                    r.source = src;
                    out_rules.push_back(r);
                }
            }
        }
    }
}

std::vector<InternalRule> parse_all_rules() {
    std::vector<InternalRule> rules;
    int next_id = 1;
    parse_rules_file("/etc/ufw/user.rules", rules, next_id);
    parse_rules_file("/etc/ufw/user6.rules", rules, next_id);
    return rules;
}

void persist_network_mode(const std::string& mode) {
    const char* home = getenv("HOME");
    if (!home) return;
    std::vector<std::string> dirs = {
        std::string(home) + "/.config/miqucontrolpanel"
    };
    std::error_code ec;
    for (const auto& d : dirs) {
        fs::create_directories(d, ec);
        std::ofstream ofs(d + "/network_mode.txt");
        if (ofs.is_open()) {
            ofs << mode << "\n";
        }
    }
}

} // namespace

bool FirewallBackend::is_available() {
    return access("/usr/bin/ufw", X_OK) == 0 ||
           access("/usr/sbin/ufw", X_OK) == 0 ||
           std::system("which ufw >/dev/null 2>&1") == 0;
}

bool FirewallBackend::is_enabled() {
    std::ifstream conf("/etc/ufw/ufw.conf");
    if (conf.is_open()) {
        std::string line;
        while (std::getline(conf, line)) {
            line = StringUtils::trim(line);
            if (StringUtils::starts_with(line, "ENABLED=")) {
                std::string val = StringUtils::to_lower(line.substr(8));
                return (val == "yes");
            }
        }
    }
    std::string act = exec_cmd("systemctl is-active ufw 2>/dev/null");
    return (act == "active");
}

std::string FirewallBackend::get_incoming_policy() {
    std::ifstream def_conf("/etc/default/ufw");
    if (def_conf.is_open()) {
        std::string line;
        while (std::getline(def_conf, line)) {
            line = StringUtils::trim(line);
            if (StringUtils::starts_with(line, "DEFAULT_INPUT_POLICY=")) {
                std::string val = line.substr(21);
                val = StringUtils::trim(val, "\" \t");
                if (val == "ACCEPT") return "Allow";
                if (val == "REJECT") return "Reject";
                if (val == "DROP") return "Drop (Deny)";
            }
        }
    }
    return "Drop (Deny)";
}

std::string FirewallBackend::get_outgoing_policy() {
    std::ifstream def_conf("/etc/default/ufw");
    if (def_conf.is_open()) {
        std::string line;
        while (std::getline(def_conf, line)) {
            line = StringUtils::trim(line);
            if (StringUtils::starts_with(line, "DEFAULT_OUTPUT_POLICY=")) {
                std::string val = line.substr(22);
                val = StringUtils::trim(val, "\" \t");
                if (val == "DROP") return "Drop (Deny)";
                if (val == "REJECT") return "Reject";
                if (val == "ACCEPT") return "Allow";
            }
        }
    }
    return "Allow";
}

std::string FirewallBackend::get_active_connection() {
    std::string out = exec_cmd("nmcli -t -f NAME,TYPE connection show --active 2>/dev/null | grep -v \":loopback\" | head -n 1 | cut -d: -f1");
    return StringUtils::trim(out);
}

std::string FirewallBackend::get_active_preset() {
    const char* home = getenv("HOME");
    std::string home_str = home ? home : "/tmp";
    std::vector<std::string> paths = {
        home_str + "/.config/miqucontrolpanel/network_mode.txt"
    };

    for (const auto& p : paths) {
        std::ifstream f(p);
        if (f.is_open()) {
            std::string saved;
            if (std::getline(f, saved)) {
                saved = StringUtils::to_lower(StringUtils::trim(saved));
                if (saved == "home") return "Home";
                if (saved == "work") return "Work";
                if (saved == "cellular" || saved == "hotspot") return "Cellular";
                if (saved == "public" || saved == "lockdown") return "Public";
            }
        }
    }

    if (get_incoming_policy() == "Reject") {
        return "Public";
    }

    std::string conn = get_active_connection();
    if (!conn.empty()) {
        std::string metered = exec_cmd("nmcli -t -f connection.metered connection show \"" + conn + "\" 2>/dev/null | cut -d: -f2");
        if (StringUtils::to_lower(StringUtils::trim(metered)) == "yes") {
            return "Cellular";
        }
    }

    return "Home";
}

bool FirewallBackend::is_service_allowed(const std::string& service_key) {
    auto rules = parse_all_rules();
    for (const auto& r : rules) {
        if (r.action != "ALLOW") continue;

        if (service_key == "kdeconnect") {
            if (r.port == "1714:1764" || r.port == "1714") return true;
        } else if (service_key == "ssh") {
            if (r.port == "22" || r.port == "ssh") return true;
        } else if (service_key == "samba") {
            if (r.port == "445" || r.port == "139" || r.port == "samba") return true;
        } else if (service_key == "cups") {
            if (r.port == "631" || r.port == "cups" || r.port == "ipp") return true;
        }
    }
    return false;
}

std::vector<FirewallCustomRule> FirewallBackend::get_custom_rules() {
    std::vector<FirewallCustomRule> result;
    auto rules = parse_all_rules();

    for (const auto& r : rules) {
        bool is_predefined = (r.port == "1714:1764" || r.port == "1714" ||
                              r.port == "22" || r.port == "ssh" ||
                              r.port == "445" || r.port == "139" || r.port == "samba" ||
                              r.port == "631" || r.port == "cups" || r.port == "ipp" ||
                              r.port == "All Traffic");
        if (is_predefined) continue;

        FirewallCustomRule cr;
        cr.id = r.id;
        cr.name = r.name.empty() ? ("Custom Port " + r.port) : r.name;
        cr.port = r.port;
        cr.protocol = r.proto;
        cr.port_proto = r.port + "/" + r.proto;
        cr.action = r.action;
        cr.source = r.source;
        cr.enabled = (r.action == "ALLOW");

        bool exists = false;
        for (const auto& item : result) {
            if (item.port_proto == cr.port_proto && item.action == cr.action) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            result.push_back(cr);
        }
    }

    return result;
}

bool FirewallBackend::is_logging_enabled() {
    std::ifstream conf("/etc/ufw/ufw.conf");
    if (conf.is_open()) {
        std::string line;
        while (std::getline(conf, line)) {
            line = StringUtils::trim(line);
            if (StringUtils::starts_with(line, "LOGLEVEL=")) {
                std::string val = StringUtils::to_lower(line.substr(9));
                return (val != "off");
            }
        }
    }
    return true;
}

FirewallTelemetry FirewallBackend::get_telemetry() {
    FirewallTelemetry t;

    // Active sockets
    std::string ss_out = exec_cmd("ss -t -u -a -H 2>/dev/null | wc -l");
    try {
        if (!ss_out.empty()) t.active_connections = std::stoi(ss_out);
    } catch (...) {}

    // Monitored interfaces from /proc/net/dev excluding loopback
    std::ifstream net_dev("/proc/net/dev");
    if (net_dev.is_open()) {
        std::string line;
        std::vector<std::string> ifaces;
        while (std::getline(net_dev, line)) {
            size_t col = line.find(':');
            if (col == std::string::npos) continue;
            std::string iface = StringUtils::trim(line.substr(0, col));
            if (iface != "lo" && !iface.empty()) {
                ifaces.push_back(iface);
            }
        }
        if (!ifaces.empty()) {
            std::string joined;
            for (size_t i = 0; i < ifaces.size(); ++i) {
                if (i > 0) joined += ", ";
                joined += ifaces[i];
            }
            t.monitored_interfaces = joined;
        }
    }
    if (t.monitored_interfaces.empty() || t.monitored_interfaces == "None") {
        std::string if_cmd = exec_cmd("ip -o link show up 2>/dev/null | awk -F': ' '{print $2}' | grep -v 'lo' | paste -sd, -");
        t.monitored_interfaces = if_cmd.empty() ? "None" : if_cmd;
    }

    // Blocked requests today via kernel buffer (fast & clean)
    std::string block_cnt = exec_cmd("journalctl -k -q -g \"UFW BLOCK\" --since today --no-pager 2>/dev/null | wc -l");
    try {
        if (!block_cnt.empty()) t.blocked_today = std::stoi(block_cnt);
    } catch (...) {}

    // Last blocked probe event
    std::string last_log = exec_cmd("journalctl -k -q -g \"UFW BLOCK\" -n 1 --no-pager 2>/dev/null");
    if (!last_log.empty()) {
        size_t in_pos = last_log.find("IN=");
        size_t src_pos = last_log.find("SRC=");
        size_t proto_pos = last_log.find("PROTO=");
        size_t dpt_pos = last_log.find("DPT=");

        std::string in_str = (in_pos != std::string::npos) ? last_log.substr(in_pos, last_log.find(' ', in_pos) - in_pos) : "";
        std::string src_str = (src_pos != std::string::npos) ? last_log.substr(src_pos, last_log.find(' ', src_pos) - src_pos) : "";
        std::string proto_str = (proto_pos != std::string::npos) ? last_log.substr(proto_pos, last_log.find(' ', proto_pos) - proto_pos) : "";
        std::string dpt_str = (dpt_pos != std::string::npos) ? last_log.substr(dpt_pos, last_log.find(' ', dpt_pos) - dpt_pos) : "";

        if (!src_str.empty()) {
            t.last_blocked_probe = src_str + (!dpt_str.empty() ? (":" + dpt_str.substr(4)) : "") +
                                   " (" + in_str + " " + proto_str + ") -> DROP";
        } else {
            t.last_blocked_probe = last_log.substr(0, std::min<size_t>(60, last_log.size()));
        }
    } else {
        t.last_blocked_probe = "No unauthorized probe events recorded today";
    }

    // Recent blocks
    std::string recent_raw = exec_cmd("journalctl -k -q -g \"UFW BLOCK\" -n 4 --no-pager 2>/dev/null");
    if (!recent_raw.empty()) {
        std::istringstream iss(recent_raw);
        std::string entry;
        while (std::getline(iss, entry)) {
            entry = StringUtils::trim(entry);
            if (!entry.empty()) t.recent_blocks.push_back(entry);
        }
    }

    return t;
}

void FirewallBackend::sync_item_state(const std::shared_ptr<SettingItem>& item) {
    if (!item) return;
    if (!is_available()) return;

    item->firewall_cfg.enabled = is_enabled();
    item->firewall_cfg.incoming_policy = get_incoming_policy();
    item->firewall_cfg.outgoing_policy = get_outgoing_policy();
    item->firewall_cfg.selected_preset = get_active_preset();

    item->firewall_cfg.allow_kdeconnect = is_service_allowed("kdeconnect");
    item->firewall_cfg.allow_ssh = is_service_allowed("ssh");
    item->firewall_cfg.allow_samba = is_service_allowed("samba");
    item->firewall_cfg.allow_cups = is_service_allowed("cups");

    // Unconditionally update custom rules so deletions immediately reflect in the UI
    item->firewall_cfg.custom_rules = get_custom_rules();

    item->firewall_cfg.monitor_enabled = is_logging_enabled();

    auto telem = get_telemetry();
    item->firewall_cfg.blocked_today = telem.blocked_today;
    item->firewall_cfg.active_connections = telem.active_connections;
    item->firewall_cfg.monitored_interfaces = telem.monitored_interfaces;
    item->firewall_cfg.last_blocked_probe = telem.last_blocked_probe;
    item->firewall_cfg.recent_blocks = telem.recent_blocks;
}

bool FirewallBackend::set_enabled(bool enable) {
    return run_pkexec(enable ? "ufw --force enable" : "ufw --force disable");
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
    return run_pkexec("ufw default " + pol + (incoming ? " incoming" : " outgoing"));
}

bool FirewallBackend::toggle_service(const std::string& service_key, bool allow) {
    if (service_key == "kdeconnect") {
        if (allow) {
            return run_pkexec("sh -c \"ufw allow 1714:1764/udp comment 'KDE Connect' && ufw allow 1714:1764/tcp comment 'KDE Connect'\"");
        } else {
            return run_pkexec("sh -c \"ufw --force delete allow 1714:1764/udp ; ufw --force delete allow 1714:1764/tcp\"");
        }
    }
    if (service_key == "ssh") {
        if (allow) {
            return run_pkexec("ufw allow 22/tcp comment 'SSH'");
        } else {
            return run_pkexec("ufw --force delete allow 22/tcp");
        }
    }
    if (service_key == "samba") {
        if (allow) {
            return run_pkexec("sh -c \"ufw allow 445/tcp comment 'Samba' && ufw allow 139/tcp comment 'Samba'\"");
        } else {
            return run_pkexec("sh -c \"ufw --force delete allow 445/tcp ; ufw --force delete allow 139/tcp\"");
        }
    }
    if (service_key == "cups") {
        if (allow) {
            return run_pkexec("sh -c \"ufw allow 631/tcp comment 'CUPS' && ufw allow 631/udp comment 'CUPS'\"");
        } else {
            return run_pkexec("sh -c \"ufw --force delete allow 631/tcp ; ufw --force delete allow 631/udp\"");
        }
    }
    return false;
}

bool FirewallBackend::add_custom_rule(const std::string& name, const std::string& port_proto, const std::string& action) {
    std::string act = "allow";
    if (action == "DENY" || action == "deny") act = "deny";
    else if (action == "REJECT" || action == "reject") act = "reject";

    std::string arg = "ufw " + act + " " + port_proto;
    if (!name.empty()) {
        arg += " comment '" + name + "'";
    }
    return run_pkexec(arg);
}

bool FirewallBackend::delete_custom_rule(const FirewallCustomRule& rule) {
    if (rule.id > 0) {
        if (run_pkexec("ufw --force delete " + std::to_string(rule.id))) {
            return true;
        }
    }
    std::string act = (rule.action == "DENY") ? "deny" : ((rule.action == "REJECT") ? "reject" : "allow");
    std::string target = rule.port_proto.empty() ? rule.port : rule.port_proto;
    if (target.empty()) return false;
    return run_pkexec("ufw --force delete " + act + " " + target);
}

bool FirewallBackend::set_logging(bool enable) {
    return run_pkexec(enable ? "ufw logging on" : "ufw logging off");
}

bool FirewallBackend::apply_preset(const std::string& preset_name) {
    std::string conn = get_active_connection();

    if (preset_name == "Home") {
        persist_network_mode("home");
        if (!conn.empty()) {
            std::system(("nmcli connection modify \"" + conn + "\" connection.metered no 2>/dev/null").c_str());
        }
        std::string script = "sh -c \""
            "ufw default deny incoming && "
            "ufw default allow outgoing && "
            "ufw --force enable && "
            "ufw allow 1714:1764/udp comment 'KDE Connect' && "
            "ufw allow 1714:1764/tcp comment 'KDE Connect' && "
            "ufw allow 631/tcp comment 'CUPS' && "
            "ufw allow 631/udp comment 'CUPS'"
            "\"";
        return run_pkexec(script);
    }
    if (preset_name == "Work") {
        persist_network_mode("work");
        std::string script = "sh -c \""
            "ufw default deny incoming && "
            "ufw default allow outgoing && "
            "ufw --force enable && "
            "ufw allow 22/tcp comment 'SSH'"
            "\"";
        return run_pkexec(script);
    }
    if (preset_name == "Public") {
        persist_network_mode("public");
        if (!conn.empty()) {
            std::system(("nmcli connection modify \"" + conn + "\" connection.metered no 2>/dev/null").c_str());
        }
        std::string script = "sh -c \""
            "ufw default reject incoming && "
            "ufw default allow outgoing && "
            "ufw --force enable ; "
            "ufw --force delete allow 1714:1764/udp ; "
            "ufw --force delete allow 1714:1764/tcp ; "
            "ufw --force delete allow 22/tcp ; "
            "ufw --force delete allow 445/tcp ; "
            "ufw --force delete allow 139/tcp ; "
            "ufw --force delete allow 631/tcp ; "
            "ufw --force delete allow 631/udp"
            "\"";
        return run_pkexec(script);
    }
    if (preset_name == "Cellular") {
        persist_network_mode("cellular");
        if (!conn.empty()) {
            std::system(("nmcli connection modify \"" + conn + "\" connection.metered yes 2>/dev/null").c_str());
        }
        std::string script = "sh -c \""
            "ufw default deny incoming && "
            "ufw default reject outgoing && "
            "ufw --force enable"
            "\"";
        return run_pkexec(script);
    }
    return false;
}

} // namespace miqu
