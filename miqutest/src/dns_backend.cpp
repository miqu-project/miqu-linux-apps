#include "dns_backend.hpp"
#include <miqutoolkit/core/string_utils.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <memory>
#include <algorithm>
#include <filesystem>
#include <ctime>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace miqu {

namespace {

struct PipeCloser {
    void operator()(FILE* fp) const {
        if (fp) pclose(fp);
    }
};

std::string run_cmd(const std::string& cmd) {
    std::string result;
    std::unique_ptr<FILE, PipeCloser> pipe(popen(cmd.c_str(), "r"));
    if (!pipe) return result;

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
        result += buffer;
    }
    return result;
}

static float s_last_latency_ms = -1.0f;
static std::string s_last_test_details;

std::string resolve_or_validate_host(const std::string& host) {
    struct in_addr addr4;
    if (inet_aton(host.c_str(), &addr4) != 0) {
        return host;
    }

    struct addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* res = nullptr;

    if (getaddrinfo(host.c_str(), nullptr, &hints, &res) == 0 && res != nullptr) {
        char buf[INET_ADDRSTRLEN];
        auto* sin = reinterpret_cast<struct sockaddr_in*>(res->ai_addr);
        if (inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf))) {
            std::string resolved = buf;
            freeaddrinfo(res);
            return resolved;
        }
        freeaddrinfo(res);
    }
    return "";
}

struct NmcliDnsProps {
    std::string dot_status = "default";
    std::string dnssec_status = "default";
    std::string ipv4_dns;
    std::string ignore_auto_dns = "no";
};

NmcliDnsProps query_connection_props(const std::string& conn) {
    NmcliDnsProps props;
    if (conn.empty()) return props;

    std::string cmd = "nmcli -t -f connection.dns-over-tls,connection.dnssec,ipv4.dns,ipv4.ignore-auto-dns connection show \"" + conn + "\" 2>/dev/null";
    std::string out = run_cmd(cmd);
    std::stringstream ss(out);
    std::string line;

    while (std::getline(ss, line)) {
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = line.substr(0, colon);
        std::string val = StringUtils::trim(line.substr(colon + 1));

        if (key == "connection.dns-over-tls") {
            props.dot_status = val;
        } else if (key == "connection.dnssec") {
            props.dnssec_status = val;
        } else if (key == "ipv4.dns") {
            props.ipv4_dns = val;
        } else if (key == "ipv4.ignore-auto-dns") {
            props.ignore_auto_dns = val;
        }
    }
    return props;
}

void parse_resolvectl_dns_output(const std::string& res_out, std::vector<std::string>& out_servers) {
    std::stringstream ss(res_out);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        std::string rest;
        if (line.rfind("Link ", 0) == 0 || line.rfind("Global:", 0) == 0) {
            size_t colon = line.find(':');
            if (colon != std::string::npos && colon + 1 < line.size()) {
                rest = StringUtils::trim(line.substr(colon + 1));
            }
        } else {
            // Continuation line (e.g. secondary or IPv6 addresses indented under link)
            rest = StringUtils::trim(line);
        }

        std::stringstream r_ss(rest);
        std::string word;
        while (r_ss >> word) {
            if (word.rfind("127.", 0) != 0 &&
                std::find(out_servers.begin(), out_servers.end(), word) == out_servers.end()) {
                out_servers.push_back(word);
            }
        }
    }
}

std::string detect_mode_from_dns(const std::vector<std::string>& nameservers,
                                 const NmcliDnsProps& props,
                                 const std::string& saved_mode,
                                 std::string& out_provider,
                                 bool& out_encrypted) {
    // Collect candidate endpoints from both resolv.conf/resolvectl and connection ipv4.dns
    std::vector<std::string> candidates = nameservers;
    if (!props.ipv4_dns.empty()) {
        std::string s = props.ipv4_dns;
        for (char& c : s) {
            if (c == ',') c = ' ';
        }
        std::stringstream ss(s);
        std::string tok;
        while (ss >> tok) {
            candidates.push_back(tok);
        }
    }

    bool has_cf = false;
    bool has_q9 = false;
    bool has_ag = false;
    bool has_mv = false;

    for (const auto& raw_ns : candidates) {
        std::string ns = raw_ns;
        std::string sni;
        size_t hash = ns.find('#');
        if (hash != std::string::npos) {
            sni = ns.substr(hash + 1);
            ns = ns.substr(0, hash);
        }
        ns = StringUtils::trim(ns);
        sni = StringUtils::trim(sni);

        if (ns == "1.1.1.1" || ns == "1.0.0.1" || ns == "2606:4700:4700::1111" ||
            sni.find("cloudflare") != std::string::npos || sni.find("one.one.one.one") != std::string::npos) {
            has_cf = true;
        }
        if (ns == "9.9.9.9" || ns == "149.112.112.112" || ns == "2620:fe::fe" ||
            sni.find("quad9") != std::string::npos) {
            has_q9 = true;
        }
        if (ns == "94.140.14.14" || ns == "94.140.15.15" ||
            sni.find("adguard") != std::string::npos) {
            has_ag = true;
        }
        if (ns == "194.242.2.2" || ns == "194.242.2.3" || ns == "193.19.108.2" ||
            sni.find("mullvad") != std::string::npos) {
            has_mv = true;
        }
    }

    bool dot_is_yes = (props.dot_status == "yes" || props.dot_status == "2");
    bool dot_is_opp = (props.dot_status == "opportunistic" || props.dot_status == "1");

    if (has_cf) {
        out_provider = "Cloudflare 1.1.1.1 (Fast Anycast)";
        out_encrypted = true;
        return "Cloudflare (1.1.1.1)";
    }
    if (has_q9) {
        out_provider = "Quad9 (Threat Intelligence)";
        out_encrypted = true;
        return "Quad9 (Malware Block)";
    }
    if (has_ag) {
        out_provider = "AdGuard DNS (Ad & Tracker Block)";
        out_encrypted = true;
        return "AdGuard (Ad & Tracker Block)";
    }
    if (has_mv) {
        out_provider = "Mullvad (Zero-Log Privacy)";
        out_encrypted = true;
        return "Mullvad (Zero-Log Privacy)";
    }

    // Check saved mode if user previously configured a standard preset or custom endpoint
    if (!saved_mode.empty() && saved_mode != "Off") {
        if (saved_mode == "Mullvad (Zero-Log Privacy)") {
            out_provider = "Mullvad (Zero-Log Privacy)";
            out_encrypted = true;
            return saved_mode;
        }
        if (saved_mode == "Cloudflare (1.1.1.1)") {
            out_provider = "Cloudflare 1.1.1.1 (Fast Anycast)";
            out_encrypted = true;
            return saved_mode;
        }
        if (saved_mode == "Quad9 (Malware Block)") {
            out_provider = "Quad9 (Threat Intelligence)";
            out_encrypted = true;
            return saved_mode;
        }
        if (saved_mode == "AdGuard (Ad & Tracker Block)") {
            out_provider = "AdGuard DNS (Ad & Tracker Block)";
            out_encrypted = true;
            return saved_mode;
        }
        if (saved_mode == "Automatic") {
            out_provider = "Automatic Opportunistic DoT";
            out_encrypted = true;
            return saved_mode;
        }
        if (saved_mode == "Custom Hostname") {
            out_provider = "Custom Endpoint";
            out_encrypted = dot_is_yes;
            return "Custom Hostname";
        }
    }

    if (dot_is_opp) {
        out_provider = "Automatic Opportunistic DoT";
        out_encrypted = true;
        return "Automatic";
    }
    if (props.ignore_auto_dns == "yes" && !props.ipv4_dns.empty()) {
        out_provider = "Custom Endpoint";
        out_encrypted = dot_is_yes;
        return "Custom Hostname";
    }

    out_provider = "Local Gateway / DHCP";
    out_encrypted = false;
    return "Off";
}

} // namespace

bool DnsBackend::is_available() {
    std::error_code ec;
    return fs::exists("/usr/bin/nmcli", ec) || fs::exists("/etc/resolv.conf", ec);
}

std::vector<std::string> DnsBackend::read_resolv_conf_nameservers(const std::string& iface) {
    std::vector<std::string> servers;

    // 1. If active network interface is known, prioritize link-specific DNS from systemd-resolved
    if (!iface.empty()) {
        std::string res_link = run_cmd("resolvectl dns " + iface + " 2>/dev/null");
        parse_resolvectl_dns_output(res_link, servers);
        if (!servers.empty()) {
            return servers;
        }
    }

    // 2. Read /etc/resolv.conf
    bool has_only_local_stub = true;
    std::ifstream file("/etc/resolv.conf");
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            line = StringUtils::trim(line);
            if (line.rfind("nameserver ", 0) == 0) {
                std::string ns = StringUtils::trim(line.substr(11));
                if (!ns.empty()) {
                    servers.push_back(ns);
                    if (ns.rfind("127.0.0.", 0) != 0) {
                        has_only_local_stub = false;
                    }
                }
            }
        }
    }

    // 3. If empty or points only to 127.0.0.53 stub resolver, parse global resolvectl dns
    if (servers.empty() || has_only_local_stub) {
        servers.clear();
        std::string res_out = run_cmd("resolvectl dns 2>/dev/null");
        parse_resolvectl_dns_output(res_out, servers);
    }

    return servers;
}

std::string DnsBackend::get_active_connection(std::string* out_device) {
    std::string out = run_cmd("nmcli -t -f NAME,TYPE,DEVICE connection show --active 2>/dev/null");
    std::stringstream ss(out);
    std::string line;
    std::string fallback_conn;
    std::string fallback_dev;

    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        std::vector<std::string> parts;
        std::stringstream line_ss(line);
        std::string part;
        while (std::getline(line_ss, part, ':')) {
            parts.push_back(part);
        }
        if (parts.size() < 3) continue;

        const std::string& name = parts[0];
        const std::string& type = parts[1];
        const std::string& dev = parts[2];

        if (type == "loopback" || dev == "lo") continue;

        if (type.find("wireless") != std::string::npos || type.find("ethernet") != std::string::npos) {
            if (out_device) *out_device = dev;
            return name;
        }

        if (fallback_conn.empty()) {
            fallback_conn = name;
            fallback_dev = dev;
        }
    }

    if (out_device) *out_device = fallback_dev;
    return fallback_conn;
}

void DnsBackend::load_saved_state(std::string& out_mode, std::string& out_custom_host) {
    const char* home = getenv("HOME");
    if (!home) return;
    std::string path = std::string(home) + "/.config/miqutest/dns_state.conf";
    std::ifstream ifs(path);
    if (!ifs.is_open()) return;

    std::string line;
    while (std::getline(ifs, line)) {
        line = StringUtils::trim(line);
        if (StringUtils::starts_with(line, "mode=")) {
            out_mode = StringUtils::trim(line.substr(5));
        } else if (StringUtils::starts_with(line, "custom_hostname=")) {
            out_custom_host = StringUtils::trim(line.substr(16));
        }
    }
}

void DnsBackend::save_state(const std::string& mode, const std::string& custom_host) {
    const char* home = getenv("HOME");
    if (!home) return;
    std::string dir = std::string(home) + "/.config/miqutest";
    std::error_code ec;
    fs::create_directories(dir, ec);

    std::ofstream ofs(dir + "/dns_state.conf");
    if (ofs.is_open()) {
        ofs << "mode=" << mode << "\n";
        ofs << "custom_hostname=" << custom_host << "\n";
    }
}

DnsState DnsBackend::get_state() {
    DnsState state;
    state.active_connection = get_active_connection(&state.active_device);
    state.nameservers = read_resolv_conf_nameservers(state.active_device);

    std::string resolved_act = StringUtils::trim(run_cmd("systemctl is-active systemd-resolved 2>/dev/null"));
    state.systemd_resolved_active = (resolved_act == "active");

    std::string dnscrypt_act = StringUtils::trim(run_cmd("systemctl is-active dnscrypt-proxy 2>/dev/null"));
    state.dnscrypt_active = (dnscrypt_act == "active");

    if (state.systemd_resolved_active) {
        state.system_resolver = "systemd-resolved.service";
    } else if (state.dnscrypt_active) {
        state.system_resolver = "dnscrypt-proxy.service";
    } else {
        state.system_resolver = "NetworkManager (Direct resolv.conf)";
    }

    std::string saved_mode;
    load_saved_state(saved_mode, state.custom_hostname);

    NmcliDnsProps props = query_connection_props(state.active_connection);
    state.dot_status = props.dot_status;
    state.dnssec_status = props.dnssec_status;

    state.mode = detect_mode_from_dns(state.nameservers, props, saved_mode,
                                      state.active_provider, state.encrypted);

    state.last_latency_ms = s_last_latency_ms;
    state.last_test_details = s_last_test_details;
    return state;
}

void DnsBackend::sync_item_state(const std::shared_ptr<SettingItem>& item) {
    if (!item) return;

    auto state = get_state();

    item->private_dns_cfg.selected_option = state.mode;
    item->private_dns_cfg.dot_engine_active = state.systemd_resolved_active;
    if (!state.custom_hostname.empty()) {
        item->private_dns_cfg.custom_hostname = state.custom_hostname;
    }

    std::string ns_str;
    for (size_t i = 0; i < state.nameservers.size(); ++i) {
        if (i > 0) ns_str += ", ";
        ns_str += state.nameservers[i];
    }
    if (ns_str.empty()) ns_str = "(None assigned)";

    std::string latency_str;
    if (state.last_latency_ms >= 0.0f) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << state.last_latency_ms << " ms";
        if (!state.last_test_details.empty()) {
            ss << " (" << state.last_test_details << ")";
        }
        latency_str = ss.str();
    } else {
        latency_str = "Not verified yet (Click Test Resolution)";
    }

    bool dot_is_yes = (state.dot_status == "yes" || state.dot_status == "2");
    bool dot_is_opp = (state.dot_status == "opportunistic" || state.dot_status == "1");

    std::string transport_str;
    if (dot_is_yes) {
        if (state.systemd_resolved_active) {
            transport_str = "TLSv1.3 (Strict Port 853 / Encrypted via systemd-resolved)";
        } else {
            transport_str = "Plaintext (Port 53; DoT enabled in profile, systemd-resolved inactive)";
        }
    } else if (dot_is_opp) {
        if (state.systemd_resolved_active) {
            transport_str = "Opportunistic Upgrade (Port 853 with Plaintext Fallback)";
        } else {
            transport_str = "Plaintext (Port 53; Opportunistic DoT requires systemd-resolved)";
        }
    } else if (state.dnscrypt_active) {
        transport_str = "DNSCrypt Encrypted Tunnel";
    } else {
        transport_str = "Plaintext (Standard Port 53 / Unencrypted)";
    }

    std::string protocol_str;
    if (dot_is_yes) {
        protocol_str = "DNS-over-TLS (RFC 7858 / Port 853)";
    } else if (dot_is_opp) {
        protocol_str = "Opportunistic DoT (RFC 7858)";
    } else if (state.dnscrypt_active) {
        protocol_str = "DNSCrypt / DoH (Port 443 / 853)";
    } else {
        protocol_str = "Standard DNS (RFC 1035 / Port 53)";
    }

    bool dnssec_is_yes = (state.dnssec_status == "yes" || state.dnssec_status == "2");
    bool dnssec_is_opp = (state.dnssec_status == "allow-downgrade" || state.dnssec_status == "1");

    std::string dnssec_str;
    if (dnssec_is_yes) {
        dnssec_str = "Enabled (Strict cryptographic verification)";
    } else if (dnssec_is_opp) {
        dnssec_str = "Opportunistic (Allow downgrade)";
    } else {
        dnssec_str = "Disabled (No validation)";
    }

    item->telemetry = {
        {"Protocol Standard", protocol_str},
        {"Active Resolver", ns_str},
        {"Transport Security", transport_str},
        {"System Resolver", state.system_resolver},
        {"DNSSEC Validation", dnssec_str},
        {"Resolution Latency", latency_str}
    };
}

bool DnsBackend::apply_preset(const std::string& preset_name, const std::string& custom_host) {
    std::string iface;
    std::string conn = get_active_connection(&iface);
    if (conn.empty()) return false;

    std::string cmd;

    if (preset_name == "Off") {
        cmd = "nmcli connection modify \"" + conn + "\" "
              "ipv4.dns \"\" ipv4.ignore-auto-dns no "
              "connection.dns-over-tls default connection.dnssec default 2>/dev/null";
    } else if (preset_name == "Automatic") {
        cmd = "nmcli connection modify \"" + conn + "\" "
              "ipv4.dns \"\" ipv4.ignore-auto-dns no "
              "connection.dns-over-tls opportunistic connection.dnssec allow-downgrade 2>/dev/null";
    } else if (preset_name == "Cloudflare (1.1.1.1)") {
        cmd = "nmcli connection modify \"" + conn + "\" "
              "ipv4.dns \"1.1.1.1#cloudflare-dns.com 1.0.0.1#cloudflare-dns.com\" ipv4.ignore-auto-dns yes "
              "connection.dns-over-tls yes connection.dnssec yes 2>/dev/null";
    } else if (preset_name == "Quad9 (Malware Block)") {
        cmd = "nmcli connection modify \"" + conn + "\" "
              "ipv4.dns \"9.9.9.9#dns.quad9.net 149.112.112.112#dns.quad9.net\" ipv4.ignore-auto-dns yes "
              "connection.dns-over-tls yes connection.dnssec yes 2>/dev/null";
    } else if (preset_name == "AdGuard (Ad & Tracker Block)") {
        cmd = "nmcli connection modify \"" + conn + "\" "
              "ipv4.dns \"94.140.14.14#dns.adguard-dns.com 94.140.15.15#dns.adguard-dns.com\" ipv4.ignore-auto-dns yes "
              "connection.dns-over-tls yes connection.dnssec yes 2>/dev/null";
    } else if (preset_name == "Mullvad (Zero-Log Privacy)") {
        cmd = "nmcli connection modify \"" + conn + "\" "
              "ipv4.dns \"194.242.2.2#dns.mullvad.net 193.19.108.2#dns.mullvad.net\" ipv4.ignore-auto-dns yes "
              "connection.dns-over-tls yes connection.dnssec yes 2>/dev/null";
    } else if (preset_name == "Custom Hostname") {
        std::string host = StringUtils::trim(custom_host);
        if (host.empty()) return false;

        std::string ip = resolve_or_validate_host(host);
        if (ip.empty()) return false;

        std::string dns_entry = (ip != host) ? (ip + "#" + host) : ip;
        cmd = "nmcli connection modify \"" + conn + "\" "
              "ipv4.dns \"" + dns_entry + "\" ipv4.ignore-auto-dns yes "
              "connection.dns-over-tls yes connection.dnssec yes 2>/dev/null";
    } else {
        return false;
    }

    int ret = std::system(cmd.c_str());
    if (ret != 0) return false;

    std::string up_cmd = "nmcli connection up \"" + conn + "\" 2>/dev/null";
    std::system(up_cmd.c_str());

    save_state(preset_name, custom_host);
    return true;
}

bool DnsBackend::test_resolution(const std::string& domain, float* out_latency_ms, std::string* out_details) {
    std::string test_domain = domain.empty() ? "cloudflare.com" : domain;

    struct timespec start{}, end{};
    clock_gettime(CLOCK_MONOTONIC, &start);

    struct addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* res = nullptr;

    int err = getaddrinfo(test_domain.c_str(), nullptr, &hints, &res);
    clock_gettime(CLOCK_MONOTONIC, &end);

    float elapsed_ms = static_cast<float>((end.tv_sec - start.tv_sec) * 1000.0 +
                                          (end.tv_nsec - start.tv_nsec) / 1000000.0);

    if (out_latency_ms) {
        *out_latency_ms = elapsed_ms;
    }

    if (err != 0) {
        if (out_details) {
            *out_details = gai_strerror(err);
        }
        return false;
    }

    std::string resolved_ip;
    char ip_buf[INET6_ADDRSTRLEN];
    for (struct addrinfo* p = res; p != nullptr; p = p->ai_next) {
        void* addr = nullptr;
        if (p->ai_family == AF_INET) {
            addr = &reinterpret_cast<struct sockaddr_in*>(p->ai_addr)->sin_addr;
        } else if (p->ai_family == AF_INET6) {
            addr = &reinterpret_cast<struct sockaddr_in6*>(p->ai_addr)->sin6_addr;
        }
        if (addr && inet_ntop(p->ai_family, addr, ip_buf, sizeof(ip_buf))) {
            resolved_ip = ip_buf;
            break;
        }
    }

    if (res) {
        freeaddrinfo(res);
    }

    s_last_latency_ms = elapsed_ms;
    s_last_test_details = "Resolved " + test_domain + " -> " + resolved_ip;

    if (out_details) {
        *out_details = s_last_test_details;
    }
    return true;
}

bool DnsBackend::is_dot_engine_active() {
    std::string resolved_act = StringUtils::trim(run_cmd("systemctl is-active systemd-resolved 2>/dev/null"));
    return (resolved_act == "active");
}

bool DnsBackend::enable_dot_engine(bool enable) {
    std::string script;
    if (enable) {
        script = "mkdir -p /etc/NetworkManager/conf.d && "
                 "printf '[main]\\ndns=systemd-resolved\\n' > /etc/NetworkManager/conf.d/dns.conf && "
                 "systemctl enable --now systemd-resolved && "
                 "ln -sf /run/systemd/resolve/stub-resolv.conf /etc/resolv.conf && "
                 "systemctl restart NetworkManager";
    } else {
        script = "rm -f /etc/NetworkManager/conf.d/dns.conf && "
                 "systemctl disable --now systemd-resolved 2>/dev/null && "
                 "rm -f /etc/resolv.conf && "
                 "systemctl restart NetworkManager";
    }
    std::string full_cmd = "pkexec bash -c \"" + script + "\" 2>/dev/null";
    return (std::system(full_cmd.c_str()) == 0);
}

} // namespace miqu
