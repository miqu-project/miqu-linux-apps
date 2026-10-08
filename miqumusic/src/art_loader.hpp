#pragma once

#include "mpd_types.hpp"
#include <string>
#include <memory>

namespace miqumusic {

class MpdClient;

class ArtLoader {
public:
    static std::string get_art_for_song(const Song& song, MpdClient& client);
    static void clear_cache();

private:
    static std::string get_music_dir();
    static std::string hash_string(const std::string& str);
};

} // namespace miqumusic
