#pragma once

#include "mpd_types.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <memory>
#include <vector>
#include <functional>
#include <atomic>

namespace miqumusic {

class PlayerView : public miqu::LinearLayout {
public:
    PlayerView();
    ~PlayerView() override = default;

    void update_playback(const MpdStatus& st, const Song& song, const std::vector<Song>& queue, class MpdClient& client);
    void update_elapsed(unsigned int elapsed, unsigned int total);

    void set_on_play_pause(std::function<void()> cb) { m_on_play_pause = std::move(cb); }
    void set_on_prev(std::function<void()> cb) { m_on_prev = std::move(cb); }
    void set_on_next(std::function<void()> cb) { m_on_next = std::move(cb); }
    void set_on_toggle_shuffle(std::function<void()> cb) { m_on_toggle_shuffle = std::move(cb); }
    void set_on_toggle_repeat(std::function<void()> cb) { m_on_toggle_repeat = std::move(cb); }
    void set_on_seek(std::function<void(float fraction)> cb) { m_on_seek = std::move(cb); }
    void set_on_volume(std::function<void(int vol)> cb) { m_on_volume = std::move(cb); }

    bool is_user_seeking() const { return m_seeking.load(); }
    void set_user_seeking(bool s) { m_seeking.store(s); }

private:
    std::shared_ptr<miqu::ImageView> m_cover_image;
    std::shared_ptr<miqu::TextView> m_track_title;
    std::shared_ptr<miqu::TextView> m_track_artist;
    std::shared_ptr<miqu::TextView> m_format_badge;

    std::shared_ptr<miqu::FrameLayout> m_up_next_card;
    std::shared_ptr<miqu::TextView> m_up_next_title;
    std::shared_ptr<miqu::TextView> m_up_next_time;

    std::shared_ptr<miqu::TextView> m_seek_elapsed_text;
    std::shared_ptr<miqu::Slider> m_seek_slider;
    std::shared_ptr<miqu::TextView> m_seek_total_text;

    std::shared_ptr<miqu::Button> m_btn_shuffle;
    std::shared_ptr<miqu::Button> m_btn_prev;
    std::shared_ptr<miqu::Button> m_btn_play;
    std::shared_ptr<miqu::Button> m_btn_next;
    std::shared_ptr<miqu::Button> m_btn_repeat;

    std::shared_ptr<miqu::LinearLayout> m_volume_row;
    std::shared_ptr<miqu::Button> m_btn_vol_down;
    std::shared_ptr<miqu::Slider> m_vol_slider;
    std::shared_ptr<miqu::Button> m_btn_vol_up;

    std::string m_current_art_uri;
    std::atomic<bool> m_seeking{false};
    int m_last_vol_before_mute = 80;

    std::function<void()> m_on_play_pause;
    std::function<void()> m_on_prev;
    std::function<void()> m_on_next;
    std::function<void()> m_on_toggle_shuffle;
    std::function<void()> m_on_toggle_repeat;
    std::function<void(float)> m_on_seek;
    std::function<void(int)> m_on_volume;
};

} // namespace miqumusic
