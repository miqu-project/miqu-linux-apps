#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <cstdint>

namespace miqu {

enum class FileCategory {
    Directory,
    Image,
    Video,
    Audio,
    Document,
    CodeText,
    Archive,
    Executable,
    Other
};

struct FileItem {
    std::filesystem::path path;
    std::string name;
    bool is_dir = false;
    bool is_symlink = false;
    bool is_hidden = false;
    uintmax_t size_bytes = 0;
    size_t child_count = 0;
    std::string size_formatted;
    std::string permissions_str;
    std::string mtime_formatted;
    FileCategory category = FileCategory::Other;
    std::string icon_name;
    std::string category_label;

    static FileItem from_path(const std::filesystem::path& path);
    static std::vector<FileItem> scan_directory(const std::filesystem::path& dir, bool show_hidden);
    static std::string format_file_size(uintmax_t bytes);
    static std::string format_permissions(std::filesystem::perms p);
    static std::string format_time(std::filesystem::file_time_type ftime);
    static FileCategory detect_category(const std::filesystem::path& path, bool is_dir);
    static std::string get_icon_for_category(FileCategory cat, const std::string& filename = "");
};

} // namespace miqu
