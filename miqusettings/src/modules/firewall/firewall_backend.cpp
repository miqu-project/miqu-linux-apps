#include "modules/firewall/firewall_backend.hpp"
#include <miqutoolkit/core/app_engine.hpp>
#include <miqutoolkit/core/string_utils.hpp>
#include <fstream>
#include <sstream>
#include <thread>
#include <cstdlib>
#include <algorithm>
#include <array>
#include <sys/wait.h>
#include <regex>

namespace miqu {

static FirewallState s_state;
static bool s_initialized = false;

static std::string run_cmd_capture(const std::string& cmd) {
    std::array<char, 256> buffer;
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }
    return result;
}

void FirewallBackend::execute_privileged_cmd_async(const std::string& cmd, ActionCallback cb) {
    std::thread([cmd, cb]() {
        std::string full_cmd = (cmd.rfind("pkexec", 0) == 0) ? cmd : ("pkexec " + cmd);
        full_cmd += " 2>&1";

        std::array<char, 256> buffer;
        std::string output;
        int exit_code = -1;

        FILE* pipe = popen(full_cmd.c_str(), "r");
        if (pipe) {
            while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
                output += buffer.data();
            }
            int status = pclose(pipe);
            if (WIFEXITED(status)) {
                exit_code = WEXITSTATUS(status);
            }
        }

        auto engine = AppEngine::instance();
        if (engine) {
            engine->post([exit_code, output, cb]() {
                bool ok = (exit_code == 0);
                if (ok) {
                    FirewallBackend::reload_from_system();
                }
                if (cb) {
                    cb(ok, output);
                }
            });
        }
    }).detach();
}

void FirewallBackend::parse_user_rules() {
    s_state.rules.clear();
    s_state.ssh_enabled = false;
    s_state.plex_enabled = false;
    s_state.samba_enabled = false;
    s_state.kdeconnect_enabled = false;

    std::ifstream rules_file("/etc/ufw/user.rules");
    if (!rules_file.is_open()) return;

    std::string line;
    int rule_id = 1;
    bool in_rules_section = false;

    while (std::getline(rules_file, line)) {
        line = StringUtils::trim(line);
        if (line.empty()) continue;

        if (line == "### RULES ###") {
            in_rules_section = true;
            continue;
        }
        if (line == "### END RULES ###") {
            in_rules_section = false;
            break;
        }

        // 1. Parse UFW tuple lines:
        // ### tuple ### <action> <proto> <dport> <dst> <sport> <src> ...
        if (line.rfind("### tuple ###", 0) == 0) {
            std::string tuple_content = line.substr(13);
            std::istringstream iss(tuple_content);
            std::vector<std::string> tokens;
            std::string tok;
            while (iss >> tok) {
                tokens.push_back(tok);
            }

            if (tokens.size() >= 6) {
                FirewallRuleItem item;
                item.id = rule_id++;
                item.raw_tuple = line;

                // Action
                std::string act = tokens[0];
                if (act.find("allow") != std::string::npos) item.action = "Allow";
                else if (act.find("deny") != std::string::npos) item.action = "Deny";
                else if (act.find("reject") != std::string::npos) item.action = "Reject";
                else {
                    item.action = act;
                    if (!item.action.empty()) item.action[0] = std::toupper(static_cast<unsigned char>(item.action[0]));
                }

                // Protocol
                item.protocol = StringUtils::to_upper(tokens[1]);

                // Port
                item.port = (tokens[2] == "any" || tokens[2] == "0") ? "All" : tokens[2];

                // Source
                std::string src = tokens[5];
                item.source = (src == "0.0.0.0/0" || src == "any") ? "Anywhere" : src;

                // Check quick app matches
                if (item.action == "Allow") {
                    if (item.port == "22" || item.port == "ssh") s_state.ssh_enabled = true;
                    if (item.port == "32400" || item.port == "plex") s_state.plex_enabled = true;
                    if (item.port == "445" || item.port == "139" || item.port == "samba") s_state.samba_enabled = true;
                    if (item.port == "1714:1764") s_state.kdeconnect_enabled = true;
                }

                s_state.rules.push_back(item);
            }
        }
        // 2. Also check standard iptables lines in user.rules if no tuples were present
        else if (in_rules_section && line.rfind("-A ufw-user-input", 0) == 0 && s_state.rules.empty()) {
            FirewallRuleItem item;
            item.id = rule_id++;
            item.action = (line.find("-j ACCEPT") != std::string::npos) ? "Allow" : "Deny";
            item.protocol = "TCP";

            size_t proto_pos = line.find("-p ");
            if (proto_pos != std::string::npos) {
                size_t sp = line.find(' ', proto_pos + 3);
                item.protocol = (sp != std::string::npos)
                    ? StringUtils::to_upper(line.substr(proto_pos + 3, sp - (proto_pos + 3)))
                    : StringUtils::to_upper(line.substr(proto_pos + 3));
            }

            size_t dport_pos = line.find("--dport ");
            if (dport_pos != std::string::npos) {
                size_t sp = line.find(' ', dport_pos + 8);
                item.port = (sp != std::string::npos)
                    ? line.substr(dport_pos + 8, sp - (dport_pos + 8))
                    : line.substr(dport_pos + 8);
            } else {
                item.port = "All";
            }

            size_t s_pos = line.find("-s ");
            if (s_pos != std::string::npos) {
                size_t sp = line.find(' ', s_pos + 3);
                item.source = (sp != std::string::npos)
                    ? line.substr(s_pos + 3, sp - (s_pos + 3))
                    : line.substr(s_pos + 3);
            } else {
                item.source = "Anywhere";
            }

            if (item.action == "Allow") {
                if (item.port == "22" || item.port == "ssh") s_state.ssh_enabled = true;
                if (item.port == "32400" || item.port == "plex") s_state.plex_enabled = true;
                if (item.port == "445" || item.port == "139" || item.port == "samba") s_state.samba_enabled = true;
                if (item.port == "1714:1764") s_state.kdeconnect_enabled = true;
            }

            s_state.rules.push_back(item);
        }
    }
}

void FirewallBackend::parse_journal_logs() {
    s_state.recent_logs.clear();
    s_state.hourly_trend.assign(12, 0.0f);
    s_state.blocked_last_hour = 0;

    // 1. Query journalctl for total blocked packets in the last hour
    std::string count_str = run_cmd_capture(
        "journalctl -k --since \"1 hour ago\" -g \"\\[UFW BLOCK\\]\" --no-pager 2>/dev/null | grep -c \"\\[UFW BLOCK\\]\""
    );
    try {
        if (!count_str.empty()) {
            s_state.blocked_last_hour = std::stoi(count_str);
        }
    } catch (...) {
        s_state.blocked_last_hour = 0;
    }

    // 2. Query recent UFW kernel lines
    std::string raw_logs = run_cmd_capture(
        "journalctl -k -g \"\\[UFW \" -n 25 --no-pager 2>/dev/null"
    );

    if (!raw_logs.empty()) {
        std::istringstream stream(raw_logs);
        std::string log_line;
        std::regex ip_regex("SRC=([0-9a-fA-F\\.:]+)");
        std::regex port_regex("DPT=([0-9]+)");

        while (std::getline(stream, log_line)) {
            log_line = StringUtils::trim(log_line);
            if (log_line.empty() || log_line.find("[UFW") == std::string::npos) continue;

            FirewallLogEntry entry;

            // Timestamp (first 15 chars: e.g. "Nov 07 09:06:32")
            if (log_line.size() >= 15) {
                entry.timestamp = log_line.substr(0, 15);
            }

            // Action
            if (log_line.find("[UFW BLOCK]") != std::string::npos) entry.action = "BLOCK";
            else if (log_line.find("[UFW LIMIT BLOCK]") != std::string::npos) entry.action = "LIMIT";
            else if (log_line.find("[UFW ALLOW]") != std::string::npos) entry.action = "ALLOW";
            else entry.action = "DROP";

            // Source IP
            std::smatch match_ip;
            if (std::regex_search(log_line, match_ip, ip_regex) && match_ip.size() > 1) {
                entry.source_ip = match_ip[1].str();
            } else {
                entry.source_ip = "Unknown";
            }

            // Destination Port
            std::smatch match_port;
            if (std::regex_search(log_line, match_port, port_regex) && match_port.size() > 1) {
                entry.port = match_port[1].str();
            } else {
                entry.port = "0";
            }

            s_state.recent_logs.push_back(entry);
        }
        std::reverse(s_state.recent_logs.begin(), s_state.recent_logs.end());
    }
}

void FirewallBackend::reload_from_system() {
    s_state.status_text = "Firewall (Powered by UFW)";

    // 1. Read /etc/ufw/ufw.conf
    std::ifstream conf("/etc/ufw/ufw.conf");
    if (conf.is_open()) {
        std::string line;
        while (std::getline(conf, line)) {
            line = StringUtils::trim(line);
            if (line.rfind("ENABLED=", 0) == 0) {
                std::string val = line.substr(8);
                s_state.active = (val == "yes" || val == "YES");
            } else if (line.rfind("LOGLEVEL=", 0) == 0) {
                s_state.log_level = line.substr(9);
            }
        }
    } else {
        std::string active_str = run_cmd_capture("systemctl is-active ufw 2>/dev/null");
        s_state.active = (active_str == "active");
    }

    // 2. Read /etc/default/ufw
    std::ifstream def_conf("/etc/default/ufw");
    if (def_conf.is_open()) {
        std::string line;
        while (std::getline(def_conf, line)) {
            line = StringUtils::trim(line);
            if (line.rfind("DEFAULT_INPUT_POLICY=", 0) == 0) {
                std::string val = line.substr(21);
                if (!val.empty() && val.front() == '"') val = val.substr(1);
                if (!val.empty() && val.back() == '"') val.pop_back();
                if (val == "DROP") s_state.incoming_policy = "Deny (Default)";
                else if (val == "REJECT") s_state.incoming_policy = "Reject";
                else if (val == "ACCEPT") s_state.incoming_policy = "Allow";
            } else if (line.rfind("DEFAULT_OUTPUT_POLICY=", 0) == 0) {
                std::string val = line.substr(22);
                if (!val.empty() && val.front() == '"') val = val.substr(1);
                if (!val.empty() && val.back() == '"') val.pop_back();
                if (val == "ACCEPT") s_state.outgoing_policy = "Allow (Default)";
                else if (val == "DROP") s_state.outgoing_policy = "Deny";
            }
        }
    }

    // 3. Read strictly real user rules from /etc/ufw/user.rules
    parse_user_rules();

    // 4. Read strictly real kernel logs from journalctl
    parse_journal_logs();
}

FirewallState& FirewallBackend::get_state() {
    if (!s_initialized) {
        s_initialized = true;
        reload_from_system();
    }
    return s_state;
}

void FirewallBackend::set_active(bool active, ActionCallback cb) {
    std::string cmd = active ? "ufw --force enable" : "ufw disable";
    execute_privileged_cmd_async(cmd, cb);
}

void FirewallBackend::set_incoming_policy(const std::string& policy, ActionCallback cb) {
    std::string ufw_pol = "deny";
    if (policy.find("Allow") != std::string::npos) ufw_pol = "allow";
    else if (policy.find("Reject") != std::string::npos) ufw_pol = "reject";

    std::string cmd = "ufw default " + ufw_pol + " incoming";
    execute_privileged_cmd_async(cmd, cb);
}

void FirewallBackend::set_outgoing_policy(const std::string& policy, ActionCallback cb) {
    std::string ufw_pol = "allow";
    if (policy.find("Deny") != std::string::npos) ufw_pol = "deny";

    std::string cmd = "ufw default " + ufw_pol + " outgoing";
    execute_privileged_cmd_async(cmd, cb);
}

void FirewallBackend::set_profile(FirewallProfile profile, ActionCallback cb) {
    s_state.active_profile = profile;
    std::string cmd;
    switch (profile) {
        case FirewallProfile::Home:
            cmd = "ufw default deny incoming && ufw default allow outgoing";
            break;
        case FirewallProfile::Work:
            cmd = "ufw default reject incoming && ufw default allow outgoing";
            break;
        case FirewallProfile::Cellular:
            cmd = "ufw default deny incoming && ufw default allow outgoing";
            break;
        case FirewallProfile::Public:
            cmd = "ufw default reject incoming && ufw default allow outgoing";
            break;
        case FirewallProfile::Custom:
            return;
    }
    execute_privileged_cmd_async(cmd, cb);
}

void FirewallBackend::set_app_toggle(const std::string& app_name, bool enabled, ActionCallback cb) {
    std::string cmd;
    if (app_name == "SSH") {
        cmd = enabled ? "ufw allow 22/tcp" : "ufw delete allow 22/tcp";
    } else if (app_name == "Plex") {
        cmd = enabled ? "ufw allow 32400/tcp" : "ufw delete allow 32400/tcp";
    } else if (app_name == "Samba") {
        cmd = enabled
            ? "ufw allow 445/tcp && ufw allow 139/tcp"
            : "ufw delete allow 445/tcp && ufw delete allow 139/tcp";
    } else if (app_name == "KDE Connect") {
        cmd = enabled
            ? "ufw allow 1714:1764/udp && ufw allow 1714:1764/tcp"
            : "ufw delete allow 1714:1764/udp && ufw delete allow 1714:1764/tcp";
    }
    if (!cmd.empty()) {
        execute_privileged_cmd_async(cmd, cb);
    }
}

void FirewallBackend::add_rule(const FirewallRuleItem& rule, ActionCallback cb) {
    std::string act = (rule.action == "Allow") ? "allow" : "deny";
    std::string proto = (rule.protocol == "UDP") ? "udp" : "tcp";
    std::string cmd;
    if (rule.source.empty() || rule.source == "Anywhere") {
        cmd = "ufw " + act + " " + rule.port + "/" + proto;
    } else {
        cmd = "ufw " + act + " from " + rule.source + " to any port " + rule.port + " proto " + proto;
    }
    execute_privileged_cmd_async(cmd, cb);
}

void FirewallBackend::delete_rule(int rule_num, ActionCallback cb) {
    std::string cmd = "ufw --force delete " + std::to_string(rule_num);
    execute_privileged_cmd_async(cmd, cb);
}

void FirewallBackend::set_log_level(const std::string& level, ActionCallback cb) {
    std::string cmd = "ufw logging " + level;
    execute_privileged_cmd_async(cmd, cb);
}

} // namespace miqu
