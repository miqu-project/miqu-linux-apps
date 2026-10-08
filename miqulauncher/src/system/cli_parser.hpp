#pragma once

#include "model/launcher_mode.hpp"

namespace miqu {

class CliParser {
public:
    enum class ParseResult {
        Success,
        ExitSuccess, // e.g. -h or -v
        Error
    };

    static ParseResult parse(int argc, char* argv[], LauncherConfig& out_config);
    static void print_help(const char* prog_name);
    static void print_version();
};

} // namespace miqu
