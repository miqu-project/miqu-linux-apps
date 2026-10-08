#include "artists_view.hpp"
#include <map>

using namespace miqu;

namespace miqumusic {

ArtistsView::ArtistsView() : LinearLayout(Orientation::Vertical) {
    set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));

    m_list_view = ListViewBuilder::create()
        ->itemHeight(50)
        ->spacing(2)
        ->itemCornerRadius(8)
        ->onItemClick([this](size_t index, std::shared_ptr<View>) {
            if (index < m_first_uris.size() && m_on_artist_play) {
                m_on_artist_play(m_first_uris[index]);
            }
        })
        ->build();

    m_list_view->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    add_view(m_list_view);
}

void ArtistsView::load_artists(const std::vector<Song>& all_songs) {
    m_first_uris.clear();

    std::map<std::string, std::vector<Song>> artists_map;
    for (const auto& s : all_songs) {
        artists_map[s.display_artist()].push_back(s);
    }

    m_first_uris.reserve(artists_map.size());
    std::vector<std::shared_ptr<View>> items;
    items.reserve(artists_map.size());

    for (const auto& pair : artists_map) {
        const std::string& artist_name = pair.first;
        const auto& songs = pair.second;

        if (!songs.empty()) {
            m_first_uris.push_back(songs[0].uri);
        } else {
            m_first_uris.push_back("");
        }

        std::string sub = std::to_string(songs.size()) + (songs.size() == 1 ? " track" : " tracks");

        auto item = ListItemViewBuilder::create()
            ->title(artist_name)
            ->subtitle(sub)
            ->trailingText("▶")
            ->build();

        items.push_back(item);
    }

    m_list_view->set_items(std::move(items));
    m_loaded = true;
}

} // namespace miqumusic
