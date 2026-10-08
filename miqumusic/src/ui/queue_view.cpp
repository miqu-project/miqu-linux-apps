#include "queue_view.hpp"

using namespace miqu;

namespace miqumusic {

QueueView::QueueView() : LinearLayout(Orientation::Vertical) {
    set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));

    m_list_view = ListViewBuilder::create()
        ->itemHeight(46)
        ->spacing(2)
        ->itemCornerRadius(8)
        ->onItemClick([this](size_t index, std::shared_ptr<View>) {
            if (index < m_song_positions.size() && m_on_song_clicked) {
                m_on_song_clicked(m_song_positions[index]);
            }
        })
        ->build();

    m_list_view->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    add_view(m_list_view);
}

void QueueView::update_queue(const std::vector<Song>& queue, int current_pos) {
    m_song_positions.clear();
    m_song_positions.reserve(queue.size());

    std::vector<std::shared_ptr<View>> items;
    items.reserve(queue.size());

    int active_idx = -1;

    for (size_t i = 0; i < queue.size(); ++i) {
        const auto& s = queue[i];
        bool is_active = (static_cast<int>(s.pos) == current_pos);
        if (is_active) {
            active_idx = static_cast<int>(i);
        }
        m_song_positions.push_back(s.pos);

        auto item = ListItemViewBuilder::create()
            ->title(s.display_title())
            ->subtitle(s.display_artist())
            ->trailingText(s.formatted_duration())
            ->highlightSubtitle(is_active)
            ->highlightTrailing(is_active)
            ->build();

        items.push_back(item);
    }

    m_list_view->set_items(std::move(items));
    if (active_idx >= 0) {
        m_list_view->set_selected_index(active_idx);
    }
}

} // namespace miqumusic
