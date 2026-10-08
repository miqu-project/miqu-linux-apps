#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <filesystem>

namespace miqumusic {

enum class PlaybackState {
    Stopped,
    Playing,
    Paused
};

inline std::string format_time(unsigned int seconds) {
    unsigned int mins = seconds / 60;
    unsigned int secs = seconds % 60;
    std::ostringstream oss;
    oss << mins << ":" << std::setw(2) << std::setfill('0') << secs;
    return oss.str();
}

struct Song {
    std::string uri;
    std::string title;
    std::string artist;
    std::string album;
    std::string genre;
    std::string date;
    std::string track;
    unsigned int duration = 0;
    unsigned int id = 0;
    unsigned int pos = 0;

    std::string display_title() const {
        if (!title.empty()) return title;
        if (!uri.empty()) {
            std::filesystem::path p(uri);
            return p.stem().string();
        }
        return "Unknown Track";
    }

    std::string display_artist() const {
        if (!artist.empty()) return artist;
        return "Unknown Artist";
    }

    std::string display_album() const {
        if (!album.empty()) return album;
        return "Unknown Album";
    }

    std::string formatted_duration() const {
        if (duration == 0) return "--:--";
        return format_time(duration);
    }
};

struct MpdStatus {
    bool connected = false;
    PlaybackState state = PlaybackState::Stopped;
    unsigned int elapsed_seconds = 0;
    unsigned int total_seconds = 0;
    float elapsed_fraction = 0.0f;
    int volume = -1; // 0-100 or -1
    bool repeat = false;
    bool random = false;
    bool single = false;
    bool consume = false;
    int song_pos = -1;
    int song_id = -1;
    uint32_t queue_version = 0;
    uint32_t queue_length = 0;
    std::string audio_format;
    unsigned int bitrate = 0;

    std::string formatted_elapsed() const {
        return format_time(elapsed_seconds);
    }

    std::string formatted_total() const {
        return format_time(total_seconds);
    }
};

} // namespace miqumusic
