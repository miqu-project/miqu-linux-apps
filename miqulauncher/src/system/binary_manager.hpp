#pragma once

#include <vector>
#include <string>

namespace miqu {

struct BinaryInfo {
    std::string name;
    std::string lower_name;
    bool is_terminal = false;
};

class BinaryManager {
public:
    static const std::vector<std::string>& get_system_binaries();
    static const std::vector<BinaryInfo>& get_binary_entries();
    static bool is_terminal_command(const std::string& cmd);
    static void launch_command(const std::string& cmd, bool terminal = false);
};

} // namespace miqu
