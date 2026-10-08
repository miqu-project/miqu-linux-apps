#include "art_loader.hpp"
#include "mpd_client.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <iostream>

namespace fs = std::filesystem;

namespace miqumusic {

std::string ArtLoader::hash_string(const std::string& str) {
    size_t h = std::hash<std::string>{}(str);
    std::stringstream ss;
    ss << std::hex << std::setw(16) << std::setfill('0') << h;
    return ss.str();
}

std::string ArtLoader::get_music_dir() {
    const char* home = getenv("HOME");
    std::string home_str = home ? home : "";

    // Check mpd.conf if possible
    fs::path conf_path = fs::path(home_str) / ".config" / "mpd" / "mpd.conf";
    if (fs::exists(conf_path)) {
        std::ifstream file(conf_path);
        std::string line;
        while (std::getline(file, line)) {
            size_t pos = line.find("music_directory");
            if (pos != std::string::npos && line.find('#') > pos) {
                size_t q1 = line.find('"', pos);
                size_t q2 = line.rfind('"');
                if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1) {
                    std::string dir = line.substr(q1 + 1, q2 - q1 - 1);
                    if (dir.rfind("~/", 0) == 0) {
                        dir = home_str + dir.substr(1);
                    }
                    if (fs::exists(dir)) return dir;
                }
            }
        }
    }

    if (!home_str.empty()) {
        fs::path default_music = fs::path(home_str) / "Music";
        if (fs::exists(default_music)) return default_music.string();
    }
    return "";
}

std::string ArtLoader::get_art_for_song(const Song& song, MpdClient& client) {
    if (song.uri.empty()) return "";

    std::string cache_dir = "/tmp/miqumusic_cache/art";
    std::error_code ec;
    fs::create_directories(cache_dir, ec);

    std::string hash = hash_string(song.uri);
    fs::path cached_jpg = fs::path(cache_dir) / (hash + ".jpg");
    fs::path cached_png = fs::path(cache_dir) / (hash + ".png");

    if (fs::exists(cached_jpg, ec) && fs::file_size(cached_jpg, ec) > 0) {
        return cached_jpg.string();
    }
    if (fs::exists(cached_png, ec) && fs::file_size(cached_png, ec) > 0) {
        return cached_png.string();
    }

    // 1. Try MPD protocol
    std::vector<uint8_t> art_data = client.fetch_art_data(song.uri);
    if (!art_data.empty()) {
        std::ofstream out(cached_jpg, std::ios::binary);
        if (out.is_open()) {
            out.write(reinterpret_cast<const char*>(art_data.data()), art_data.size());
            out.close();
            return cached_jpg.string();
        }
    }

    // 2. Check local filesystem if songs reside locally
    std::string music_dir = get_music_dir();
    if (!music_dir.empty()) {
        fs::path song_path = fs::path(music_dir) / song.uri;
        if (fs::exists(song_path, ec)) {
            // Check same folder for cover files
            fs::path parent_dir = song_path.parent_path();
            const std::vector<std::string> cover_names = {
                "cover.jpg", "cover.png", "cover.webp",
                "folder.jpg", "folder.png",
                "album.jpg", "front.jpg"
            };

            for (const auto& name : cover_names) {
                fs::path candidate = parent_dir / name;
                if (fs::exists(candidate, ec) && fs::file_size(candidate, ec) > 0) {
                    return candidate.string();
                }
            }

            // Check matching stem with image extension (e.g. song.webp)
            fs::path stem_webp = song_path;
            stem_webp.replace_extension(".webp");
            if (fs::exists(stem_webp, ec) && fs::file_size(stem_webp, ec) > 0) {
                return stem_webp.string();
            }

            fs::path stem_jpg = song_path;
            stem_jpg.replace_extension(".jpg");
            if (fs::exists(stem_jpg, ec) && fs::file_size(stem_jpg, ec) > 0) {
                return stem_jpg.string();
            }

            fs::path stem_png = song_path;
            stem_png.replace_extension(".png");
            if (fs::exists(stem_png, ec) && fs::file_size(stem_png, ec) > 0) {
                return stem_png.string();
            }

            // 3. Fallback to ffmpeg extraction into cached_jpg
            std::string cmd = "ffmpeg -y -v quiet -i \"" + song_path.string() + "\" -an \"" + cached_jpg.string() + "\" 2>/dev/null";
            int res = std::system(cmd.c_str());
            if (res == 0 && fs::exists(cached_jpg, ec) && fs::file_size(cached_jpg, ec) > 0) {
                return cached_jpg.string();
            }
        }
    }

    return "";
}

void ArtLoader::clear_cache() {
    std::error_code ec;
    fs::remove_all("/tmp/miqumusic_cache", ec);
}

} // namespace miqumusic
