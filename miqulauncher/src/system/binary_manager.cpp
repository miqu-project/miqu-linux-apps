#include "binary_manager.hpp"
#include <filesystem>
#include <sstream>
#include <set>
#include <unordered_set>
#include <algorithm>
#include <unistd.h>
#include <cstdlib>

namespace miqu {

namespace fs = std::filesystem;

static std::vector<std::string> s_binaries;
static std::vector<BinaryInfo> s_binary_entries;
static bool s_loaded = false;

static std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

static void ensure_binaries_loaded() {
    if (s_loaded) return;

    const char* path_env = getenv("PATH");
    std::string path_str = path_env ? path_env : "/usr/local/bin:/usr/bin:/bin";

    std::unordered_set<std::string> seen;
    std::stringstream ss(path_str);
    std::string dir;

    while (std::getline(ss, dir, ':')) {
        if (dir.empty()) continue;
        std::error_code ec;
        if (!fs::is_directory(dir, ec)) continue;

        for (const auto& entry : fs::directory_iterator(dir, fs::directory_options::skip_permission_denied, ec)) {
            if (ec) break;
            if (entry.is_regular_file(ec) || entry.is_symlink(ec)) {
                std::string name = entry.path().filename().string();
                if (!name.empty() && name[0] != '.' && seen.find(name) == seen.end()) {
                    if (::access(entry.path().c_str(), X_OK) == 0) {
                        seen.insert(name);
                        BinaryInfo info;
                        info.name = name;
                        info.lower_name = to_lower(name);
                        info.is_terminal = BinaryManager::is_terminal_command(name);
                        s_binary_entries.push_back(std::move(info));
                    }
                }
            }
        }
    }

    std::sort(s_binary_entries.begin(), s_binary_entries.end(), [](const BinaryInfo& a, const BinaryInfo& b) {
        return a.name < b.name;
    });

    s_binaries.clear();
    s_binaries.reserve(s_binary_entries.size());
    for (const auto& b : s_binary_entries) {
        s_binaries.push_back(b.name);
    }

    s_loaded = true;
}

const std::vector<std::string>& BinaryManager::get_system_binaries() {
    ensure_binaries_loaded();
    return s_binaries;
}

const std::vector<BinaryInfo>& BinaryManager::get_binary_entries() {
    ensure_binaries_loaded();
    return s_binary_entries;
}

bool BinaryManager::is_terminal_command(const std::string& cmd) {
    if (cmd.empty()) return false;

    std::string binary = cmd;
    size_t space = binary.find_first_of(" \t");
    if (space != std::string::npos) {
        binary = binary.substr(0, space);
    }
    size_t slash = binary.find_last_of('/');
    if (slash != std::string::npos) {
        binary = binary.substr(slash + 1);
    }

    static const std::set<std::string> tui_tools = {
        "top", "htop", "btop", "atop", "iotop", "iftop", "nethogs",
        "vim", "nvim", "vi", "nano", "emacs", "micro", "helix", "neovim",
        "less", "more", "man", "tail", "journalctl", "dmesg",
        "bash", "sh", "zsh", "fish", "tmux", "screen", "ssh", "scp", "sftp",
        "ping", "traceroute", "mtr", "curl", "wget", "nc", "netcat", "nmap",
        "yazi", "ranger", "lf", "nnn", "fzf", "lazygit", "gitui", "tig",
        "nmtui", "alsamixer", "pulsemixer", "cmatrix", "neofetch", "fastfetch",
        "cal", "bc", "gdb", "lldb", "strace", "lsof", "watch"
    };

    return tui_tools.find(binary) != tui_tools.end();
}

void BinaryManager::launch_command(const std::string& cmd, bool terminal) {
    if (cmd.empty()) return;

    std::string full_cmd = cmd;
    bool needs_terminal = terminal || is_terminal_command(cmd);

    if (needs_terminal) {
        const char* env_term = getenv("TERMINAL");
        std::string term = (env_term && *env_term) ? env_term : "kitty || foot || alacritty || wezterm || weston-terminal || xterm";
        full_cmd = term + " -e " + cmd;
    }

    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        execl("/bin/sh", "sh", "-c", full_cmd.c_str(), nullptr);
        _exit(1);
    }
}

} // namespace miqu
