#pragma once

#include <string>
#include <cstdint>

namespace miqu {

struct FilesConfig {
    int window_width = 1100;
    int window_height = 700;

    std::string default_path = "~";
    bool show_hidden = false;
    bool vim_keys = true;
    std::string terminal_cmd = "alacritty";

    size_t preview_max_text_lines = 250;
    size_t preview_max_file_size_kb = 1024;
    std::string image_preview_quality = "FullOriginal";

    static FilesConfig& get();
    void sync_defaults_from_toolkit();
    void load(const std::string& custom_path = "");
};

} // namespace miqu
