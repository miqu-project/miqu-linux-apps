#pragma once

#include "mpd_types.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <memory>
#include <vector>
#include <functional>

namespace miqumusic {

class LibraryView : public miqu::LinearLayout {
public:
    LibraryView();
    ~LibraryView() override = default;

    void load_songs(const std::vector<Song>& songs);
    bool is_loaded() const { return m_loaded; }

    void set_on_song_selected(std::function<void(const std::string& uri)> cb) {
        m_on_song_selected = std::move(cb);
    }

private:
    std::shared_ptr<miqu::ListView> m_list_view;
    std::vector<std::string> m_song_uris;
    std::function<void(const std::string&)> m_on_song_selected;
    bool m_loaded = false;
};

} // namespace miqumusic
