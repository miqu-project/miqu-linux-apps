#pragma once

#include "mpd_client.hpp"
#include "mpd_types.hpp"
#include "ui/player_view.hpp"
#include "ui/queue_view.hpp"
#include "ui/library_view.hpp"
#include "ui/artists_view.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>

namespace miqumusic {

class MusicApp {
public:
    MusicApp(std::shared_ptr<miqu::AppEngine> engine, std::string host, unsigned int port);
    ~MusicApp();

    bool init();
    int run();
    void quit();

private:
    void setup_ui();
    void setup_mpd_callbacks();
    void start_ticker();
    void stop_ticker();

    void update_playback_ui(const MpdStatus& st, const Song& song);
    void update_queue_ui(const std::vector<Song>& queue, int current_pos);
    void refresh_all();

    void handle_key_press(const miqu::KeyPressEvent& ev);

    std::shared_ptr<miqu::AppEngine> m_engine;
    std::shared_ptr<miqu::Window> m_window;
    std::shared_ptr<MpdClient> m_client;
    std::string m_host;
    unsigned int m_port = 0;

    std::shared_ptr<miqu::Toolbar> m_toolbar;
    std::shared_ptr<miqu::NavigationView> m_nav_view;
    std::shared_ptr<miqu::BottomNavigationView> m_bottom_nav;
    std::shared_ptr<miqu::ViewPager> m_view_pager;

    std::shared_ptr<PlayerView> m_player_view;
    std::shared_ptr<QueueView> m_queue_view;
    std::shared_ptr<LibraryView> m_library_view;
    std::shared_ptr<ArtistsView> m_artists_view;

    std::mutex m_status_mutex;
    MpdStatus m_status;
    Song m_current_song;
    std::vector<Song> m_queue;

    std::atomic<bool> m_running{false};
    std::thread m_ticker_thread;
};

} // namespace miqumusic
