#pragma once

#include "mpd_types.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <memory>
#include <vector>
#include <functional>

namespace miqumusic {

class ArtistsView : public miqu::LinearLayout {
public:
    ArtistsView();
    ~ArtistsView() override = default;

    void load_artists(const std::vector<Song>& all_songs);
    bool is_loaded() const { return m_loaded; }

    void set_on_artist_play(std::function<void(const std::string& first_uri)> cb) {
        m_on_artist_play = std::move(cb);
    }

private:
    std::shared_ptr<miqu::ListView> m_list_view;
    std::vector<std::string> m_first_uris;
    std::function<void(const std::string&)> m_on_artist_play;
    bool m_loaded = false;
};

} // namespace miqumusic
