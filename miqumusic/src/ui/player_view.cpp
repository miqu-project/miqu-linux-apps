#include "player_view.hpp"
#include "art_loader.hpp"
#include "mpd_client.hpp"
#include <cmath>
#include <algorithm>
#include <thread>
#include <chrono>

using namespace miqu;

namespace miqumusic {

PlayerView::PlayerView() : LinearLayout(Orientation::Vertical) {
    set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    set_padding(4, 2, 4, 0);

    auto config = Config::get();

    // 1. Hero Album Art (280x280 framed & elevated)
    auto art_frame = std::make_shared<FrameLayout>();
    art_frame->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        288
    ));
    art_frame->set_margin(0, 4, 0, 10);

    m_cover_image = ImageViewBuilder::create()
        ->imageResource("audio-x-generic")
        ->targetSize(280)
        ->cornerRadius(20)
        ->fitMode(FitMode::Cover)
        ->build();
    m_cover_image->set_layout_params(LayoutParams(
        280,
        280,
        Gravity::Center
    ));
    art_frame->add_view(m_cover_image);
    add_view(art_frame);

    // 2. Track Title
    m_track_title = TextViewBuilder::create()
        ->text("No track playing")
        ->h1()
        ->bold(true)
        ->textAlignment(TextAlignment::Center)
        ->ellipsize(true)
        ->build();
    m_track_title->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    m_track_title->set_margin(0, 4, 0, 2);
    add_view(m_track_title);

    // 3. Track Artist & Album
    m_track_artist = TextViewBuilder::create()
        ->text("Music Player Daemon")
        ->caption(true)
        ->muted(true)
        ->textAlignment(TextAlignment::Center)
        ->ellipsize(true)
        ->build();
    m_track_artist->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    m_track_artist->set_margin(0, 0, 0, 6);
    add_view(m_track_artist);

    // 4. Audiophile Stream & Format Badge Row
    auto format_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    format_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterHorizontal
    ));
    format_row->set_margin(0, 0, 0, 14);

    m_format_badge = TextViewBuilder::create()
        ->text("")
        ->caption(true)
        ->muted(true)
        ->textAlignment(TextAlignment::Center)
        ->build();
    m_format_badge->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    format_row->add_view(m_format_badge);
    add_view(format_row);

    // 5. "Up Next" Queue Peek Strip
    m_up_next_card = std::make_shared<FrameLayout>();
    m_up_next_card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    m_up_next_card->set_background_color(config->colors.surface_variant);
    m_up_next_card->set_corner_radius(12);
    m_up_next_card->set_padding(12, 7);
    m_up_next_card->set_margin(4, 0, 4, 12);

    auto up_next_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    up_next_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));

    auto up_next_badge = TextViewBuilder::create()
        ->text("⏭ Up Next:")
        ->caption(true)
        ->bold(true)
        ->build();
    up_next_badge->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    up_next_badge->set_margin(0, 0, 8, 0);

    m_up_next_title = TextViewBuilder::create()
        ->text("Queue is empty")
        ->caption(true)
        ->ellipsize(true)
        ->build();
    m_up_next_title->set_layout_params(LayoutParams(
        0,
        static_cast<int>(LayoutDimension::WrapContent),
        1.0f
    ));

    m_up_next_time = TextViewBuilder::create()
        ->text("")
        ->caption(true)
        ->muted(true)
        ->textAlignment(TextAlignment::Right)
        ->build();
    m_up_next_time->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    up_next_row->add_view(up_next_badge);
    up_next_row->add_view(m_up_next_title);
    up_next_row->add_view(m_up_next_time);
    m_up_next_card->add_view(up_next_row);
    add_view(m_up_next_card);

    m_up_next_card->set_on_click_listener([this]() {
        if (m_on_next) m_on_next();
    });

    // Flexible vertical spacer
    auto flex_spacer = std::make_shared<FrameLayout>();
    flex_spacer->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        0,
        1.0f
    ));
    add_view(flex_spacer);

    // 6. Interactive Seek Bar Row
    auto seek_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    seek_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    seek_row->set_margin(0, 4, 0, 8);

    m_seek_elapsed_text = TextViewBuilder::create()
        ->text("0:00")
        ->caption(true)
        ->muted(true)
        ->textAlignment(TextAlignment::Left)
        ->build();
    m_seek_elapsed_text->set_layout_params(LayoutParams(
        46,
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    m_seek_slider = SliderBuilder::create()
        ->progress(0.0f)
        ->trackHeight(7)
        ->thumbRadius(9)
        ->build();
    m_seek_slider->set_layout_params(LayoutParams(
        0,
        static_cast<int>(LayoutDimension::WrapContent),
        1.0f
    ));
    m_seek_slider->set_margin(6, 0, 6, 0);

    m_seek_total_text = TextViewBuilder::create()
        ->text("0:00")
        ->caption(true)
        ->muted(true)
        ->textAlignment(TextAlignment::Right)
        ->build();
    m_seek_total_text->set_layout_params(LayoutParams(
        42,
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    seek_row->add_view(m_seek_elapsed_text);
    seek_row->add_view(m_seek_slider);
    seek_row->add_view(m_seek_total_text);
    add_view(seek_row);

    m_seek_slider->set_on_value_changed_listener([this](float val, bool from_user) {
        if (from_user) {
            m_seeking.store(true);
            if (m_on_seek) m_on_seek(val);
            std::thread([this]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
                m_seeking.store(false);
            }).detach();
        }
    });

    // 7. Transport Controls Row: 🔀  ⏮  ▶  ⏭  🔁
    auto controls_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    controls_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    controls_row->set_gravity(Gravity::CenterVertical);
    controls_row->set_margin(0, 4, 0, 14);

    auto spacer1 = std::make_shared<FrameLayout>();
    spacer1->set_layout_params(LayoutParams(0, 1, 0.6f));

    m_btn_shuffle = ButtonBuilder::create()
        ->text("🔀")
        ->flat(true)
        ->textSize(18)
        ->padding(10, 8)
        ->cornerRadius(10)
        ->onClick([this]() { if (m_on_toggle_shuffle) m_on_toggle_shuffle(); })
        ->build();
    m_btn_shuffle->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));

    auto spacer2 = std::make_shared<FrameLayout>();
    spacer2->set_layout_params(LayoutParams(0, 1, 0.9f));

    m_btn_prev = ButtonBuilder::create()
        ->text("⏮")
        ->flat(true)
        ->textSize(22)
        ->padding(14, 8)
        ->cornerRadius(12)
        ->onClick([this]() { if (m_on_prev) m_on_prev(); })
        ->build();
    m_btn_prev->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));

    auto spacer3 = std::make_shared<FrameLayout>();
    spacer3->set_layout_params(LayoutParams(0, 1, 1.0f));

    m_btn_play = ButtonBuilder::create()
        ->text("▶")
        ->primary(true)
        ->textSize(24)
        ->bold(true)
        ->padding(2, 2)
        ->cornerRadius(32)
        ->onClick([this]() { if (m_on_play_pause) m_on_play_pause(); })
        ->build();
    m_btn_play->set_layout_params(LayoutParams(64, 64, Gravity::CenterVertical));

    auto spacer4 = std::make_shared<FrameLayout>();
    spacer4->set_layout_params(LayoutParams(0, 1, 1.0f));

    m_btn_next = ButtonBuilder::create()
        ->text("⏭")
        ->flat(true)
        ->textSize(22)
        ->padding(14, 8)
        ->cornerRadius(12)
        ->onClick([this]() { if (m_on_next) m_on_next(); })
        ->build();
    m_btn_next->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));

    auto spacer5 = std::make_shared<FrameLayout>();
    spacer5->set_layout_params(LayoutParams(0, 1, 0.9f));

    m_btn_repeat = ButtonBuilder::create()
        ->text("🔁")
        ->flat(true)
        ->textSize(18)
        ->padding(10, 8)
        ->cornerRadius(10)
        ->onClick([this]() { if (m_on_toggle_repeat) m_on_toggle_repeat(); })
        ->build();
    m_btn_repeat->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));

    auto spacer6 = std::make_shared<FrameLayout>();
    spacer6->set_layout_params(LayoutParams(0, 1, 0.6f));

    controls_row->add_view(spacer1);
    controls_row->add_view(m_btn_shuffle);
    controls_row->add_view(spacer2);
    controls_row->add_view(m_btn_prev);
    controls_row->add_view(spacer3);
    controls_row->add_view(m_btn_play);
    controls_row->add_view(spacer4);
    controls_row->add_view(m_btn_next);
    controls_row->add_view(spacer5);
    controls_row->add_view(m_btn_repeat);
    controls_row->add_view(spacer6);
    add_view(controls_row);

    // 8. Centered Volume Row:  🔈  [━━━━━●━━━━━]  🔊
    m_volume_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    m_volume_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    m_volume_row->set_gravity(Gravity::CenterVertical);
    m_volume_row->set_margin(0, 2, 0, 18);

    auto vol_spacer1 = std::make_shared<FrameLayout>();
    vol_spacer1->set_layout_params(LayoutParams(0, 1, 1.0f));

    m_btn_vol_down = ButtonBuilder::create()
        ->text("🔈")
        ->flat(true)
        ->textSize(15)
        ->padding(8, 6)
        ->cornerRadius(8)
        ->build();
    m_btn_vol_down->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));

    m_vol_slider = SliderBuilder::create()
        ->progress(0.8f)
        ->trackHeight(5)
        ->thumbRadius(7)
        ->build();
    m_vol_slider->set_layout_params(LayoutParams(
        220,
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    m_vol_slider->set_margin(8, 0, 8, 0);

    m_btn_vol_up = ButtonBuilder::create()
        ->text("🔊")
        ->flat(true)
        ->textSize(15)
        ->padding(8, 6)
        ->cornerRadius(8)
        ->build();
    m_btn_vol_up->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));

    auto vol_spacer2 = std::make_shared<FrameLayout>();
    vol_spacer2->set_layout_params(LayoutParams(0, 1, 1.0f));

    m_volume_row->add_view(vol_spacer1);
    m_volume_row->add_view(m_btn_vol_down);
    m_volume_row->add_view(m_vol_slider);
    m_volume_row->add_view(m_btn_vol_up);
    m_volume_row->add_view(vol_spacer2);
    add_view(m_volume_row);

    m_vol_slider->set_on_value_changed_listener([this](float val, bool from_user) {
        if (from_user) {
            int v = std::clamp(static_cast<int>(std::round(val * 100.0f)), 0, 100);
            if (m_on_volume) m_on_volume(v);
            m_btn_vol_down->set_text(v == 0 ? "🔇" : "🔈");
        }
    });

    m_btn_vol_down->set_on_click_listener([this]() {
        float cur = m_vol_slider->get_progress();
        if (cur > 0.01f) {
            m_last_vol_before_mute = static_cast<int>(cur * 100.0f);
            if (m_on_volume) m_on_volume(0);
            m_vol_slider->set_value(0.0f);
            m_btn_vol_down->set_text("🔇");
        } else {
            int target = m_last_vol_before_mute > 0 ? m_last_vol_before_mute : 75;
            if (m_on_volume) m_on_volume(target);
            m_vol_slider->set_value(target / 100.0f);
            m_btn_vol_down->set_text("🔈");
        }
    });

    m_btn_vol_up->set_on_click_listener([this]() {
        if (m_on_volume) m_on_volume(100);
        m_vol_slider->set_value(1.0f);
        m_btn_vol_down->set_text("🔈");
    });
}

void PlayerView::update_playback(const MpdStatus& st, const Song& song, const std::vector<Song>& queue, MpdClient& client) {
    // Title & Artist
    if (!song.uri.empty()) {
        m_track_title->set_text(song.display_title());
        m_track_artist->set_text(song.display_artist() + " • " + song.display_album());
    } else {
        m_track_title->set_text("No track playing");
        m_track_artist->set_text("Music Player Daemon");
    }

    // Stream & Audiophile Quality Badge
    std::string badge_str;
    if (!st.audio_format.empty()) {
        badge_str = "🎵 " + st.audio_format;
    }
    if (st.bitrate > 0) {
        if (!badge_str.empty()) badge_str += " • ";
        badge_str += std::to_string(st.bitrate) + " kbps";
    }
    if (!song.date.empty()) {
        if (!badge_str.empty()) badge_str += " • ";
        badge_str += song.date;
    }
    if (!song.genre.empty()) {
        if (!badge_str.empty()) badge_str += " • ";
        badge_str += song.genre;
    }
    m_format_badge->set_text(badge_str);

    // Update "Up Next" Peek
    if (st.song_pos >= 0 && st.song_pos + 1 < static_cast<int>(queue.size())) {
        const auto& next_s = queue[st.song_pos + 1];
        m_up_next_title->set_text(next_s.display_title() + " — " + next_s.display_artist());
        m_up_next_time->set_text(next_s.formatted_duration());
    } else if (st.repeat && !queue.empty()) {
        const auto& first_s = queue[0];
        m_up_next_title->set_text("Loops to: " + first_s.display_title() + " — " + first_s.display_artist());
        m_up_next_time->set_text(first_s.formatted_duration());
    } else {
        m_up_next_title->set_text("End of queue");
        m_up_next_time->set_text("");
    }

    // Cover Art
    if (song.uri != m_current_art_uri) {
        m_current_art_uri = song.uri;
        std::string art = ArtLoader::get_art_for_song(song, client);
        if (!art.empty()) {
            m_cover_image->set_image_resource(art);
        } else {
            m_cover_image->set_image_resource("audio-x-generic");
        }
    }

    // Seek Bar
    if (!m_seeking.load()) {
        m_seek_slider->set_progress(st.elapsed_fraction);
        m_seek_elapsed_text->set_text(st.formatted_elapsed());
    }
    m_seek_total_text->set_text(st.formatted_total());

    // Play/Pause button text & style
    if (st.state == PlaybackState::Playing) {
        m_btn_play->set_text("⏸");
    } else {
        m_btn_play->set_text("▶");
    }

    // Mode toggles
    m_btn_shuffle->set_selected(st.random);
    if (st.repeat && st.single) {
        m_btn_repeat->set_text("🔂");
        m_btn_repeat->set_selected(true);
    } else if (st.repeat) {
        m_btn_repeat->set_text("🔁");
        m_btn_repeat->set_selected(true);
    } else {
        m_btn_repeat->set_text("🔁");
        m_btn_repeat->set_selected(false);
    }

    // Volume control
    if (st.volume >= 0) {
        m_volume_row->set_visibility(Visibility::Visible);
        m_vol_slider->set_value(st.volume / 100.0f);
        m_btn_vol_down->set_text(st.volume == 0 ? "🔇" : "🔈");
    } else {
        m_volume_row->set_visibility(Visibility::Gone);
    }
}

void PlayerView::update_elapsed(unsigned int elapsed, unsigned int total) {
    if (m_seeking.load()) return;
    if (total > 0) {
        float frac = static_cast<float>(elapsed) / static_cast<float>(total);
        m_seek_slider->set_progress(frac);
        m_seek_elapsed_text->set_text(format_time(elapsed));
    }
}

} // namespace miqumusic
