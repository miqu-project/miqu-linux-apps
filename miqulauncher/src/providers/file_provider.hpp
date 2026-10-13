#pragma once

#include "model/launcher_item.hpp"
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>

namespace miqu {

struct FileRecord {
    std::string full_path;
    std::string filename;
    std::string lower_filename;
    std::string parent_dir;
    std::string icon_name;
    bool is_dir = false;
};

class FileProvider {
public:
    FileProvider();
    ~FileProvider();

    void ensure_loaded();
    std::vector<LauncherItem> get_items(const std::string& query);
    void activate(const LauncherItem& item);

    static std::string detect_icon(const std::string& path, bool is_dir);
    static std::string url_decode(const std::string& str);

private:
    void index_recent_files();
    void index_user_directories();

    std::vector<FileRecord> m_recent_files;
    std::vector<FileRecord> m_indexed_files;
    std::mutex m_mutex;
    std::thread m_worker;
    std::atomic<bool> m_indexed{false};
    std::atomic<bool> m_stop{false};
};

} // namespace miqu
