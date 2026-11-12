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
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>

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

static std::vector<uint8_t> build_dns_query(const std::string& domain) {
    std::vector<uint8_t> packet;
    // 2-byte length placeholder for RFC 7858
    packet.push_back(0);
    packet.push_back(0);

    // DNS Header (12 bytes)
    packet.push_back(0x12); packet.push_back(0x34); // ID
    packet.push_back(0x01); packet.push_back(0x00); // Standard query, RD=1
    packet.push_back(0x00); packet.push_back(0x01); // QDCOUNT = 1
    packet.push_back(0x00); packet.push_back(0x00); // ANCOUNT = 0
    packet.push_back(0x00); packet.push_back(0x00); // NSCOUNT = 0
    packet.push_back(0x00); packet.push_back(0x00); // ARCOUNT = 0

    // Question: QNAME
    size_t start = 0;
    while (start < domain.length()) {
        size_t dot = domain.find('.', start);
        if (dot == std::string::npos) dot = domain.length();
        uint8_t len = static_cast<uint8_t>(dot - start);
        if (len > 0) {
            packet.push_back(len);
            for (size_t i = start; i < dot; ++i) {
                packet.push_back(domain[i]);
            }
        }
        start = dot + 1;
    }
    packet.push_back(0); // Root label null terminator

    packet.push_back(0x00); packet.push_back(0x01); // QTYPE = A (1)
    packet.push_back(0x00); packet.push_back(0x01); // QCLASS = IN (1)

    // Fill in 2-byte big-endian length prefix
    uint16_t dns_len = static_cast<uint16_t>(packet.size() - 2);
    packet[0] = static_cast<uint8_t>((dns_len >> 8) & 0xFF);
    packet[1] = static_cast<uint8_t>(dns_len & 0xFF);

    return packet;
}

static std::string parse_first_a_record(const uint8_t* buf, size_t len) {
    if (len < 12) return "";
    uint16_t ancount = (static_cast<uint16_t>(buf[6]) << 8) | buf[7];
    if (ancount == 0) return "";

    size_t offset = 12;
    while (offset < len) {
        uint8_t label_len = buf[offset++];
        if (label_len == 0) break;
        if ((label_len & 0xC0) == 0xC0) {
            offset++;
            break;
        }
        offset += label_len;
    }
    offset += 4; // Skip QTYPE + QCLASS

    for (uint16_t i = 0; i < ancount && offset < len; ++i) {
        if ((buf[offset] & 0xC0) == 0xC0) {
            offset += 2;
        } else {
            while (offset < len && buf[offset] != 0) {
                offset += 1 + buf[offset];
            }
            offset++;
        }
        if (offset + 10 > len) break;

        uint16_t type = (static_cast<uint16_t>(buf[offset]) << 8) | buf[offset + 1];
        uint16_t rdlength = (static_cast<uint16_t>(buf[offset + 8]) << 8) | buf[offset + 9];
        offset += 10;

        if (type == 1 && rdlength == 4 && offset + 4 <= len) {
            char ip_str[INET_ADDRSTRLEN];
            if (inet_ntop(AF_INET, buf + offset, ip_str, sizeof(ip_str))) {
                return std::string(ip_str);
            }
        }
        offset += rdlength;
    }
    return "";
}

static bool perform_dot_resolution(const std::string& server_ip, int port, const std::string& sni,
                                   const std::string& domain, float* out_latency_ms, std::string* out_details) {
    struct timespec start_ts{}, end_ts{};
    clock_gettime(CLOCK_MONOTONIC, &start_ts);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        if (out_details) *out_details = "Socket creation failed";
        return false;
    }

    struct timeval tv{4, 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct sockaddr_in saddr{};
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port > 0 ? port : 853);
    if (inet_pton(AF_INET, server_ip.c_str(), &saddr.sin_addr) <= 0) {
        close(sock);
        if (out_details) *out_details = "Invalid server IP: " + server_ip;
        return false;
    }

    if (connect(sock, reinterpret_cast<struct sockaddr*>(&saddr), sizeof(saddr)) != 0) {
        close(sock);
        if (out_details) *out_details = "TCP connect to " + server_ip + ":853 failed";
        return false;
    }

    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        close(sock);
        if (out_details) *out_details = "Failed to create SSL context";
        return false;
    }
    SSL_CTX_set_default_verify_paths(ctx);

    SSL* ssl = SSL_new(ctx);
    if (!ssl) {
        SSL_CTX_free(ctx);
        close(sock);
        if (out_details) *out_details = "Failed to allocate SSL session";
        return false;
    }
    SSL_set_fd(ssl, sock);
    if (!sni.empty()) {
        SSL_set_tlsext_host_name(ssl, sni.c_str());
    }

    if (SSL_connect(ssl) <= 0) {
        char err_buf[256];
        ERR_error_string_n(ERR_get_error(), err_buf, sizeof(err_buf));
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(sock);
        if (out_details) *out_details = std::string("TLS handshake failed: ") + err_buf;
        return false;
    }

    std::string tls_ver = SSL_get_version(ssl);
    std::string cipher = SSL_get_cipher_name(ssl);
    long verify_res = SSL_get_verify_result(ssl);

    auto query = build_dns_query(domain);
    if (SSL_write(ssl, query.data(), static_cast<int>(query.size())) <= 0) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(sock);
        if (out_details) *out_details = "SSL_write DNS query failed";
        return false;
    }

    uint8_t len_buf[2];
    if (SSL_read(ssl, len_buf, 2) != 2) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(sock);
        if (out_details) *out_details = "Incomplete DNS response length header";
        return false;
    }

    uint16_t resp_len = (static_cast<uint16_t>(len_buf[0]) << 8) | len_buf[1];
    std::vector<uint8_t> resp_buf(resp_len);
    int total_read = 0;
    while (total_read < resp_len) {
        int r = SSL_read(ssl, resp_buf.data() + total_read, resp_len - total_read);
        if (r <= 0) break;
        total_read += r;
    }

    clock_gettime(CLOCK_MONOTONIC, &end_ts);
    float elapsed_ms = static_cast<float>((end_ts.tv_sec - start_ts.tv_sec) * 1000.0 +
                                          (end_ts.tv_nsec - start_ts.tv_nsec) / 1000000.0);

    std::string resolved_ip = parse_first_a_record(resp_buf.data(), total_read);

    SSL_shutdown(ssl);
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    close(sock);

    if (out_latency_ms) *out_latency_ms = elapsed_ms;

    std::string cert_status = (verify_res == X509_V_OK) ? "Cert Valid" : "Cert Unverified";
    std::ostringstream ss;
    ss << tls_ver << " (" << cipher << ") • " << cert_status << " • SNI: " << (sni.empty() ? server_ip : sni)
       << " • Resolved " << domain;
    if (!resolved_ip.empty()) {
        ss << " -> " << resolved_ip;
    }
    if (out_details) *out_details = ss.str();
    return true;
}

static bool perform_standard_dns_resolution(const std::string& server_ip, const std::string& domain,
                                            float* out_latency_ms, std::string* out_details) {
    struct timespec start_ts{}, end_ts{};
    clock_gettime(CLOCK_MONOTONIC, &start_ts);

    struct addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* res = nullptr;

    int err = getaddrinfo(domain.c_str(), nullptr, &hints, &res);
    clock_gettime(CLOCK_MONOTONIC, &end_ts);
    float elapsed_ms = static_cast<float>((end_ts.tv_sec - start_ts.tv_sec) * 1000.0 +
                                          (end_ts.tv_nsec - start_ts.tv_nsec) / 1000000.0);

    if (out_latency_ms) *out_latency_ms = elapsed_ms;
    if (err != 0) {
        if (out_details) *out_details = gai_strerror(err);
        return false;
    }

    std::string resolved_ip;
    if (res && res->ai_family == AF_INET) {
        char buf[INET_ADDRSTRLEN];
        auto* sin = reinterpret_cast<struct sockaddr_in*>(res->ai_addr);
        if (inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf))) {
            resolved_ip = buf;
        }
    }
    if (res) freeaddrinfo(res);

    std::string target = server_ip.empty() ? "Local Resolver" : server_ip;
    if (out_details) {
        *out_details = "Standard DNS (RFC 1035 / Port 53) via " + target + " • Resolved " + domain + " -> " + resolved_ip;
    }
    return true;
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

    // If user explicitly configured Off or if auto-DNS is active with no DoT/custom DNS, it is Off
    if (saved_mode == "Off" || (props.ignore_auto_dns == "no" && props.ipv4_dns.empty() && !dot_is_yes && !dot_is_opp)) {
        out_provider = "Local Gateway / DHCP";
        out_encrypted = false;
        return "Off";
    }

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
    std::string path = std::string(home) + "/.config/miqucontrolpanel/dns_state.conf";
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
    std::string dir = std::string(home) + "/.config/miqucontrolpanel";
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

    bool engine_active = is_dot_engine_active();
    if (engine_active) {
        state.system_resolver = "systemd-resolved.service (Stub 127.0.0.53:53 -> DoT 853)";
    } else if (state.systemd_resolved_active) {
        state.system_resolver = "systemd-resolved.service (Foreign / Bypass)";
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
    bool engine_active = is_dot_engine_active();

    item->private_dns_cfg.selected_option = state.mode;
    item->private_dns_cfg.dot_engine_active = (state.mode != "Off" && engine_active);
    item->switch_val = (state.mode != "Off");
    item->subtitle = state.mode;
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
        if (engine_active) {
            transport_str = "TLSv1.3 (Strict Port 853 / Encrypted via systemd-resolved)";
        } else {
            transport_str = "Plaintext (Port 53; DoT profile enabled, but DoT engine inactive)";
        }
    } else if (dot_is_opp) {
        if (engine_active) {
            transport_str = "Opportunistic Upgrade (Port 853 with Plaintext Fallback)";
        } else {
            transport_str = "Plaintext (Port 53; Opportunistic DoT requires DoT engine)";
        }
    } else if (state.dnscrypt_active) {
        transport_str = "DNSCrypt Encrypted Tunnel";
    } else {
        transport_str = "Plaintext (Standard Port 53 / Unencrypted)";
    }

    std::string protocol_str;
    if (dot_is_yes) {
        protocol_str = engine_active
            ? "DNS-over-TLS (RFC 7858 / Port 853)"
            : "DNS-over-TLS (Engine Setup Required)";
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
    auto state = get_state();

    bool is_dot = (state.mode != "Off" && (state.dot_status == "yes" || state.dot_status == "2" ||
                                           state.dot_status == "opportunistic" || state.dot_status == "1" ||
                                           is_dot_engine_active()));

    std::string target_ip;
    std::string sni;

    if (state.mode == "Cloudflare (1.1.1.1)") {
        target_ip = "1.1.1.1";
        sni = "cloudflare-dns.com";
    } else if (state.mode == "Quad9 (Malware Block)") {
        target_ip = "9.9.9.9";
        sni = "dns.quad9.net";
    } else if (state.mode == "AdGuard (Ad & Tracker Block)") {
        target_ip = "94.140.14.14";
        sni = "dns.adguard-dns.com";
    } else if (state.mode == "Mullvad (Zero-Log Privacy)") {
        target_ip = "194.242.2.2";
        sni = "dns.mullvad.net";
    } else if (state.mode == "Custom Hostname") {
        sni = state.custom_hostname;
        target_ip = resolve_or_validate_host(sni);
    } else if (!state.nameservers.empty()) {
        target_ip = state.nameservers[0];
        if (target_ip.find('#') != std::string::npos) {
            size_t h = target_ip.find('#');
            sni = target_ip.substr(h + 1);
            target_ip = target_ip.substr(0, h);
        }
    }

    bool ok = false;
    float lat = -1.0f;
    std::string details;

    if (is_dot && !target_ip.empty()) {
        ok = perform_dot_resolution(target_ip, 853, sni, test_domain, &lat, &details);
    } else {
        ok = perform_standard_dns_resolution(target_ip, test_domain, &lat, &details);
    }

    s_last_latency_ms = lat;
    s_last_test_details = details;

    if (out_latency_ms) *out_latency_ms = lat;
    if (out_details) *out_details = details;
    return ok;
}

bool DnsBackend::is_dot_engine_active() {
    std::string resolved_act = StringUtils::trim(run_cmd("systemctl is-active systemd-resolved 2>/dev/null"));
    if (resolved_act != "active") return false;

    std::ifstream nm_conf("/etc/NetworkManager/conf.d/dns.conf");
    if (!nm_conf.is_open()) return false;
    std::string content((std::istreambuf_iterator<char>(nm_conf)), std::istreambuf_iterator<char>());
    if (content.find("dns=systemd-resolved") == std::string::npos) return false;

    std::error_code ec;
    if (fs::is_symlink("/etc/resolv.conf", ec)) {
        auto target = fs::read_symlink("/etc/resolv.conf", ec);
        if (target.string().find("stub-resolv.conf") != std::string::npos) {
            return true;
        }
    }

    std::ifstream resolv("/etc/resolv.conf");
    if (resolv.is_open()) {
        std::string line;
        while (std::getline(resolv, line)) {
            if (line.find("127.0.0.53") != std::string::npos) return true;
        }
    }
    return false;
}

bool DnsBackend::enable_dot_engine(bool enable) {
    std::string script;
    if (enable) {
        script = "mkdir -p /etc/NetworkManager/conf.d && "
                 "printf '[main]\\ndns=systemd-resolved\\n' > /etc/NetworkManager/conf.d/dns.conf && "
                 "mkdir -p /etc/systemd/resolved.conf.d && "
                 "printf '[Resolve]\\nDNSOverTLS=yes\\nDNSSEC=allow-downgrade\\n' > /etc/systemd/resolved.conf.d/dot.conf && "
                 "systemctl enable --now systemd-resolved && "
                 "systemctl restart systemd-resolved && "
                 "ln -sf /run/systemd/resolve/stub-resolv.conf /etc/resolv.conf && "
                 "systemctl restart NetworkManager";
    } else {
        script = "rm -f /etc/NetworkManager/conf.d/dns.conf && "
                 "rm -f /etc/systemd/resolved.conf.d/dot.conf && "
                 "systemctl disable --now systemd-resolved 2>/dev/null && "
                 "rm -f /etc/resolv.conf && "
                 "systemctl restart NetworkManager";
    }
    std::string full_cmd = "pkexec bash -c \"" + script + "\" 2>/dev/null";
    return (std::system(full_cmd.c_str()) == 0);
}

} // namespace miqu
