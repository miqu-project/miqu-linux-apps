#include "model/file_item.hpp"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

namespace miqu {

namespace fs = std::filesystem;

std::string FileItem::format_file_size(uintmax_t bytes) {
    if (bytes < 1024) {
        return std::to_string(bytes) + " B";
    }
    const char* units[] = {"KB", "MB", "GB", "TB"};
    double size = static_cast<double>(bytes) / 1024.0;
    int unit_idx = 0;
    while (size >= 1024.0 && unit_idx < 3) {
        size /= 1024.0;
        unit_idx++;
    }
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1) << size << " " << units[unit_idx];
    return ss.str();
}

std::string FileItem::format_permissions(fs::perms p) {
    std::string s;
    s.reserve(10);
    s += (p & fs::perms::owner_read) != fs::perms::none ? "r" : "-";
    s += (p & fs::perms::owner_write) != fs::perms::none ? "w" : "-";
    s += (p & fs::perms::owner_exec) != fs::perms::none ? "x" : "-";
    s += (p & fs::perms::group_read) != fs::perms::none ? "r" : "-";
    s += (p & fs::perms::group_write) != fs::perms::none ? "w" : "-";
    s += (p & fs::perms::group_exec) != fs::perms::none ? "x" : "-";
    s += (p & fs::perms::others_read) != fs::perms::none ? "r" : "-";
    s += (p & fs::perms::others_write) != fs::perms::none ? "w" : "-";
    s += (p & fs::perms::others_exec) != fs::perms::none ? "x" : "-";
    return s;
}

std::string FileItem::format_time(fs::file_time_type ftime) {
    try {
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
        );
        std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
        std::tm tm_buf;
        if (localtime_r(&tt, &tm_buf)) {
            char buf[64];
            if (std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm_buf)) {
                return std::string(buf);
            }
        }
    } catch (...) {}
    return "Unknown";
}

FileCategory FileItem::detect_category(const fs::path& path, bool is_dir) {
    if (is_dir) return FileCategory::Directory;

    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".webp" ||
        ext == ".svg" || ext == ".bmp" || ext == ".gif"  || ext == ".ico"  ||
        ext == ".avif" || ext == ".tiff") {
        return FileCategory::Image;
    }

    if (ext == ".mp4" || ext == ".mkv" || ext == ".webm" || ext == ".avi" ||
        ext == ".mov" || ext == ".flv" || ext == ".wmv") {
        return FileCategory::Video;
    }

    if (ext == ".mp3" || ext == ".flac" || ext == ".wav" || ext == ".ogg" ||
        ext == ".m4a" || ext == ".aac"  || ext == ".opus") {
        return FileCategory::Audio;
    }

    if (ext == ".pdf" || ext == ".epub" || ext == ".doc" || ext == ".docx" ||
        ext == ".odt" || ext == ".xls"  || ext == ".xlsx"|| ext == ".ppt"  ||
        ext == ".pptx") {
        return FileCategory::Document;
    }

    if (ext == ".zip" || ext == ".tar" || ext == ".gz"  || ext == ".xz"  ||
        ext == ".bz2" || ext == ".7z"  || ext == ".rar" || ext == ".zst") {
        return FileCategory::Archive;
    }

    if (ext == ".txt" || ext == ".md"   || ext == ".cpp" || ext == ".hpp" ||
        ext == ".c"   || ext == ".h"    || ext == ".py"  || ext == ".rs"   ||
        ext == ".js"  || ext == ".ts"   || ext == ".json"|| ext == ".yaml" ||
        ext == ".yml" || ext == ".toml" || ext == ".sh"  || ext == ".bash" ||
        ext == ".zsh" || ext == ".conf" || ext == ".ini" || ext == ".xml"  ||
        ext == ".html"|| ext == ".css"  || ext == ".scss"|| ext == ".go"   ||
        ext == ".java"|| ext == ".lua"  || ext == ".csv" || ext == ".patch"||
        ext == ".diff") {
        return FileCategory::CodeText;
    }

    std::error_code ec;
    auto perms = fs::status(path, ec).permissions();
    if (!ec && (perms & (fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec)) != fs::perms::none) {
        return FileCategory::Executable;
    }

    return FileCategory::Other;
}

std::string FileItem::get_icon_for_category(FileCategory cat, const std::string& filename) {
    switch (cat) {
        case FileCategory::Directory:
            if (filename == "Downloads") return "folder-download";
            if (filename == "Documents") return "folder-documents";
            if (filename == "Pictures") return "folder-pictures";
            if (filename == "Music") return "folder-music";
            if (filename == "Videos") return "folder-videos";
            return "folder";
        case FileCategory::Image:
            return "image-x-generic";
        case FileCategory::Video:
            return "video-x-generic";
        case FileCategory::Audio:
            return "audio-x-generic";
        case FileCategory::Document:
            return "x-office-document";
        case FileCategory::CodeText:
            return "text-x-generic";
        case FileCategory::Archive:
            return "package-x-generic";
        case FileCategory::Executable:
            return "application-x-executable";
        case FileCategory::Other:
        default:
            return "text-x-generic";
    }
}

FileItem FileItem::from_path(const fs::path& p) {
    FileItem item;
    item.path = p;
    item.name = p.filename().string();
    if (item.name.empty()) {
        item.name = p.string();
    }
    item.is_hidden = (!item.name.empty() && item.name[0] == '.');

    std::error_code ec;
    fs::file_status status = fs::symlink_status(p, ec);
    if (ec) {
        item.size_formatted = "Unknown";
        return item;
    }

    item.is_symlink = fs::is_symlink(status);
    item.is_dir = fs::is_directory(p, ec);
    item.category = detect_category(p, item.is_dir);
    item.icon_name = get_icon_for_category(item.category, item.name);

    switch (item.category) {
        case FileCategory::Directory: item.category_label = "Folder"; break;
        case FileCategory::Image:     item.category_label = "Image"; break;
        case FileCategory::Video:     item.category_label = "Video"; break;
        case FileCategory::Audio:     item.category_label = "Audio"; break;
        case FileCategory::Document:  item.category_label = "Document"; break;
        case FileCategory::CodeText:  item.category_label = "Code/Text"; break;
        case FileCategory::Archive:   item.category_label = "Archive"; break;
        case FileCategory::Executable:item.category_label = "Executable"; break;
        case FileCategory::Other:
        default:                      item.category_label = "File"; break;
    }

    // Permissions
    item.permissions_str = (item.is_dir ? "d" : "-") + format_permissions(status.permissions());

    // Modification time
    auto mtime = fs::last_write_time(p, ec);
    if (!ec) {
        item.mtime_formatted = format_time(mtime);
    }

    // Size / Count
    if (item.is_dir) {
        size_t count = 0;
        for (fs::directory_iterator it(p, fs::directory_options::skip_permission_denied, ec), end;
             !ec && it != end; ++it) {
            count++;
        }
        item.child_count = count;
        item.size_formatted = std::to_string(count) + (count == 1 ? " item" : " items");
    } else {
        item.size_bytes = fs::file_size(p, ec);
        if (!ec) {
            item.size_formatted = format_file_size(item.size_bytes);
        } else {
            item.size_formatted = "-";
        }
    }

    return item;
}

std::vector<FileItem> FileItem::scan_directory(const fs::path& dir, bool show_hidden) {
    std::vector<FileItem> items;
    std::error_code ec;

    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
        return items;
    }

    for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end;
         !ec && it != end; ++it) {
        std::string fname = it->path().filename().string();
        if (!show_hidden && !fname.empty() && fname[0] == '.') {
            continue;
        }
        items.push_back(FileItem::from_path(it->path()));
    }

    // Sort: Folders first, then alphabetically case-insensitive
    std::sort(items.begin(), items.end(), [](const FileItem& a, const FileItem& b) {
        if (a.is_dir != b.is_dir) {
            return a.is_dir > b.is_dir; // Dirs first
        }
        std::string a_lower = a.name;
        std::string b_lower = b.name;
        std::transform(a_lower.begin(), a_lower.end(), a_lower.begin(), ::tolower);
        std::transform(b_lower.begin(), b_lower.end(), b_lower.begin(), ::tolower);
        return a_lower < b_lower;
    });

    return items;
}

} // namespace miqu
