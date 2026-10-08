#include "library_view.hpp"

using namespace miqu;

namespace miqumusic {

LibraryView::LibraryView() : LinearLayout(Orientation::Vertical) {
    set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));

    m_list_view = ListViewBuilder::create()
        ->itemHeight(46)
        ->spacing(2)
        ->itemCornerRadius(8)
        ->onItemClick([this](size_t index, std::shared_ptr<View>) {
            if (index < m_song_uris.size() && m_on_song_selected) {
                m_on_song_selected(m_song_uris[index]);
            }
        })
        ->build();

    m_list_view->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    add_view(m_list_view);
}

void LibraryView::load_songs(const std::vector<Song>& songs) {
    m_song_uris.clear();
    m_song_uris.reserve(songs.size());

    std::vector<std::shared_ptr<View>> items;
    items.reserve(songs.size());

    for (const auto& s : songs) {
        m_song_uris.push_back(s.uri);

        auto item = ListItemViewBuilder::create()
            ->title(s.display_title())
            ->subtitle(s.display_artist())
            ->trailingText(s.formatted_duration())
            ->build();

        items.push_back(item);
    }

    m_list_view->set_items(std::move(items));
    m_loaded = true;
}

} // namespace miqumusic
