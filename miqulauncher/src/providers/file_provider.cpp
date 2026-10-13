#include "file_provider.hpp"
#include <miqutoolkit/system/app_manager.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <unordered_set>

namespace miqu {

namespace fs = std::filesystem;

static std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

std::string FileProvider::url_decode(const std::string& str) {
    std::string result;
    result.reserve(str.size());
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '%' && i + 2 < str.size()) {
            std::string hex_str = str.substr(i + 1, 2);
            try {
                char c = static_cast<char>(std::stoi(hex_str, nullptr, 16));
                result += c;
                i += 2;
            } catch (...) {
                result += str[i];
            }
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

std::string FileProvider::detect_icon(const std::string& path, bool is_dir) {
    if (is_dir) return "folder";

    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) return "text-x-generic";

    std::string ext = to_lower(path.substr(dot));

    // Images
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".gif" ||
        ext == ".webp" || ext == ".bmp" || ext == ".svg") {
        return "image-x-generic";
    }
    // Audio
    if (ext == ".mp3" || ext == ".flac" || ext == ".wav" || ext == ".ogg" ||
        ext == ".m4a" || ext == ".opus" || ext == ".aac") {
        return "audio-x-generic";
    }
    // Video
    if (ext == ".mp4" || ext == ".mkv" || ext == ".avi" || ext == ".mov" ||
        ext == ".webm" || ext == ".flv") {
        return "video-x-generic";
    }
    // Documents
    if (ext == ".pdf") return "application-pdf";
    if (ext == ".odt" || ext == ".doc" || ext == ".docx" || ext == ".rtf") {
        return "x-office-document";
    }
    if (ext == ".ods" || ext == ".xls" || ext == ".xlsx" || ext == ".csv") {
        return "x-office-spreadsheet";
    }
    if (ext == ".odp" || ext == ".ppt" || ext == ".pptx") {
        return "x-office-presentation";
    }
    // Archives
    if (ext == ".zip" || ext == ".tar" || ext == ".gz" || ext == ".xz" ||
        ext == ".7z" || ext == ".bz2" || ext == ".rar") {
        return "package-x-generic";
    }
    // Code & Scripts
    if (ext == ".cpp" || ext == ".c" || ext == ".hpp" || ext == ".h" ||
        ext == ".py" || ext == ".js" || ext == ".ts" || ext == ".rs" ||
        ext == ".go" || ext == ".sh" || ext == ".bash" || ext == ".json" ||
        ext == ".xml" || ext == ".html" || ext == ".css" || ext == ".conf") {
        return "text-x-script";
    }

    return "text-x-generic";
}

FileProvider::FileProvider() {
    index_recent_files();
    m_worker = std::thread([this]() {
        index_user_directories();
    });
}

FileProvider::~FileProvider() {
    m_stop = true;
    if (m_worker.joinable()) {
        m_worker.join();
    }
}

void FileProvider::ensure_loaded() {
    // If background thread has completed, indexed is true
    // Recent files are already ready from constructor synchronously
}

void FileProvider::index_recent_files() {
    const char* home = getenv("HOME");
    if (!home) return;

    std::string xbel = std::string(home) + "/.local/share/recently-used.xbel";
    std::error_code ec;
    if (!fs::exists(xbel, ec)) return;

    std::ifstream file(xbel);
    if (!file.is_open()) return;

    std::unordered_set<std::string> seen;
    std::vector<FileRecord> recents;
    std::string line;

    while (std::getline(file, line)) {
        size_t href_pos = line.find("<bookmark href=\"file://");
        if (href_pos == std::string::npos) continue;

        size_t start = href_pos + 23; // length of '<bookmark href="file://'
        size_t end = line.find('"', start);
        if (end == std::string::npos) continue;

        std::string raw_url = line.substr(start, end - start);
        std::string decoded = url_decode(raw_url);

        if (seen.find(decoded) != seen.end()) continue;
        seen.insert(decoded);

        if (fs::exists(decoded, ec)) {
            bool is_dir = fs::is_directory(decoded, ec);
            fs::path p(decoded);
            std::string filename = p.filename().string();
            std::string parent = p.parent_path().string();

            FileRecord rec;
            rec.full_path = decoded;
            rec.filename = filename;
            rec.lower_filename = to_lower(filename);
            rec.parent_dir = parent;
            rec.is_dir = is_dir;
            rec.icon_name = detect_icon(decoded, is_dir);

            recents.push_back(std::move(rec));
            if (recents.size() >= 40) break;
        }
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_recent_files = std::move(recents);
}

void FileProvider::index_user_directories() {
    const char* home = getenv("HOME");
    if (!home) {
        m_indexed = true;
        return;
    }

    std::string home_str(home);
    std::vector<std::string> target_dirs = {
        home_str + "/Desktop",
        home_str + "/Documents",
        home_str + "/Downloads",
        home_str + "/Pictures",
        home_str + "/Music",
        home_str + "/Videos",
        home_str + "/Projects",
        home_str + "/git",
        home_str
    };

    std::unordered_set<std::string> seen;
    std::vector<FileRecord> indexed;
    indexed.reserve(10000);

    const size_t MAX_FILES = 12000;

    for (const auto& base_dir : target_dirs) {
        if (m_stop) break;
        std::error_code ec;
        if (!fs::exists(base_dir, ec)) continue;

        int max_depth = (base_dir == home_str) ? 1 : 3;

        try {
            for (auto it = fs::recursive_directory_iterator(base_dir, fs::directory_options::skip_permission_denied, ec);
                 it != fs::recursive_directory_iterator(); ++it) {
                if (m_stop || indexed.size() >= MAX_FILES) break;

                if (it.depth() > max_depth) {
                    it.pop();
                    continue;
                }

                const auto& entry = *it;
                std::string path_str = entry.path().string();
                std::string filename = entry.path().filename().string();

                // Skip hidden files and common cache/build trees
                if (filename.empty() || filename[0] == '.') {
                    if (entry.is_directory(ec)) it.pop();
                    continue;
                }
                if (filename == "node_modules" || filename == "build" || filename == "target" ||
                    filename == "__pycache__" || filename == ".git" || filename == "cache") {
                    if (entry.is_directory(ec)) it.pop();
                    continue;
                }

                if (seen.find(path_str) != seen.end()) continue;
                seen.insert(path_str);

                bool is_dir = entry.is_directory(ec);
                FileRecord rec;
                rec.full_path = path_str;
                rec.filename = filename;
                rec.lower_filename = to_lower(filename);
                rec.parent_dir = entry.path().parent_path().string();
                rec.is_dir = is_dir;
                rec.icon_name = detect_icon(path_str, is_dir);

                indexed.push_back(std::move(rec));
            }
        } catch (...) {
            // Ignore directory iteration errors
        }
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_indexed_files = std::move(indexed);
    m_indexed = true;
}

static int compute_file_score(const FileRecord& file, const std::string& query, const std::string& lower_query) {
    if (lower_query.empty()) return 0;

    // 1. Exact match
    if (file.lower_filename == lower_query) {
        return 9000;
    }

    // 2. Prefix match
    if (file.lower_filename.rfind(lower_query, 0) == 0) {
        return 6000 + (100 - static_cast<int>(std::min<size_t>(100, file.lower_filename.size())));
    }

    // 3. Word boundary match
    size_t wb = file.lower_filename.find(' ' + lower_query);
    if (wb == std::string::npos) wb = file.lower_filename.find('-' + lower_query);
    if (wb == std::string::npos) wb = file.lower_filename.find('_' + lower_query);
    if (wb != std::string::npos) {
        return 4500;
    }

    // 4. Substring match
    size_t sub = file.lower_filename.find(lower_query);
    if (sub != std::string::npos) {
        return 3000 - static_cast<int>(std::min<size_t>(20, sub)) * 50;
    }

    // 5. Fuzzy subsequence match for short queries
    if (query.size() >= 3 && query.size() <= 8) {
        size_t qi = 0;
        for (char c : file.lower_filename) {
            if (c == lower_query[qi]) {
                qi++;
                if (qi == lower_query.size()) {
                    return 1500;
                }
            }
        }
    }

    return 0;
}

std::vector<LauncherItem> FileProvider::get_items(const std::string& query) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<LauncherItem> items;

    if (query.empty()) {
        items.reserve(m_recent_files.size());
        for (const auto& rec : m_recent_files) {
            LauncherItem item;
            item.id = "file:" + rec.full_path;
            item.title = rec.filename;
            item.subtitle = rec.parent_dir;
            item.icon_name = rec.icon_name;
            item.badge = rec.is_dir ? "Folder" : "File";
            item.category = ItemCategory::File;
            item.exec_cmd = rec.full_path;
            item.return_value = rec.full_path;
            items.push_back(std::move(item));
        }
        return items;
    }

    std::string lower_query = to_lower(query);
    std::vector<std::pair<int, const FileRecord*>> scored;
    scored.reserve(256);

    // Search recent files first with bonus
    for (const auto& rec : m_recent_files) {
        int s = compute_file_score(rec, query, lower_query);
        if (s > 0) {
            scored.push_back({s + 500, &rec});
        }
    }

    // Search indexed directory files
    for (const auto& rec : m_indexed_files) {
        int s = compute_file_score(rec, query, lower_query);
        if (s > 0) {
            scored.push_back({s, &rec});
        }
    }

    // Sort by score descending
    std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
        return a.first > b.first;
    });

    std::unordered_set<std::string> seen;
    items.reserve(std::min<size_t>(scored.size(), 20));

    for (const auto& pair : scored) {
        const auto* rec = pair.second;
        if (seen.find(rec->full_path) != seen.end()) continue;
        seen.insert(rec->full_path);

        LauncherItem item;
        item.id = "file:" + rec->full_path;
        item.title = rec->filename;
        item.subtitle = rec->parent_dir;
        item.icon_name = rec->icon_name;
        item.badge = rec->is_dir ? "Folder" : "File";
        item.category = ItemCategory::File;
        item.exec_cmd = rec->full_path;
        item.return_value = rec->full_path;
        items.push_back(std::move(item));

        if (items.size() >= 20) break;
    }

    return items;
}

void FileProvider::activate(const LauncherItem& item) {
    if (item.exec_cmd.empty()) return;
    std::string cmd = "xdg-open \"" + item.exec_cmd + "\"";
    AppManager::launch_command(cmd);
}

} // namespace miqu
