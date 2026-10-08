#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace miqudesk {

enum class DesktopEntryType {
    Application,
    Directory,
    File
};

struct DesktopShortcut {
    std::string id;
    std::string name;
    std::string exec;
    std::string icon;
    bool terminal = false;
    DesktopEntryType type = DesktopEntryType::Application;
};

class DesktopScanner {
public:
    static std::string get_desktop_dir();
    static std::vector<DesktopShortcut> get_shortcuts();
    static std::string clean_exec(const std::string& raw);
    static std::string get_icon_for_directory(const std::string& name);
    static std::string get_icon_for_file(const std::filesystem::path& path);
};

} // namespace miqudesk
