#pragma once

#include "mpd_types.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <memory>
#include <vector>
#include <functional>

namespace miqumusic {

class QueueView : public miqu::LinearLayout {
public:
    QueueView();
    ~QueueView() override = default;

    void update_queue(const std::vector<Song>& queue, int current_pos);

    void set_on_song_clicked(std::function<void(unsigned int pos)> cb) {
        m_on_song_clicked = std::move(cb);
    }

private:
    std::shared_ptr<miqu::ListView> m_list_view;
    std::vector<unsigned int> m_song_positions;
    std::function<void(unsigned int)> m_on_song_clicked;
};

} // namespace miqumusic
