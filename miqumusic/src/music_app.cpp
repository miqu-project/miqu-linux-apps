#include "music_app.hpp"
#include "ui/settings_view.hpp"
#include <xkbcommon/xkbcommon-keysyms.h>
#include <iostream>
#include <algorithm>
#include <chrono>

using namespace miqu;

namespace miqumusic {

MusicApp::MusicApp(std::shared_ptr<AppEngine> engine, std::string host, unsigned int port)
    : m_engine(std::move(engine)), m_host(std::move(host)), m_port(port) {
    m_client = std::make_shared<MpdClient>();
}

MusicApp::~MusicApp() {
    quit();
}

bool MusicApp::init() {
    if (!m_engine) return false;

    bool initial_connected = m_client->connect(m_host, m_port);
    if (!initial_connected) {
        std::cerr << "[miqumusic] Warning: Could not connect to MPD on startup. Will keep retrying.\n";
    }

    setup_ui();
    setup_mpd_callbacks();

    if (initial_connected) {
        MpdStatus st = m_client->get_status();
        Song song = m_client->get_current_song();
        std::vector<Song> q = m_client->get_queue();
        update_playback_ui(st, song);
        update_queue_ui(q, st.song_pos);
    }

    m_client->start_idle_listener();
    start_ticker();

    m_window = WindowBuilder::create()
        ->title("miqumusic")
        ->appId("miqumusic")
        ->role(WindowRole::Toplevel)
        ->preferredSize(600, 680)
        ->closeOnEscape(false)
        ->contentView(m_nav_view)
        ->onClose([this]() {
            quit();
        })
        ->onKey([this](const KeyPressEvent& ev) {
            handle_key_press(ev);
        })
        ->build();

    if (!m_window) {
        std::cerr << "[miqumusic] Failed to create Wayland window.\n";
        return false;
    }

    m_window->show();
    return true;
}

void MusicApp::setup_ui() {
    m_nav_view = NavigationViewBuilder::create()
        ->autoBack(true)
        ->backOnEscape(true)
        ->build();
    m_nav_view->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));

    auto root_container = std::make_shared<LinearLayout>(Orientation::Vertical);
    root_container->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    root_container->set_padding(0);

    // 1. Top Toolbar
    m_toolbar = ToolbarBuilder::create()
        ->title("Music")
        ->subtitle(m_client->is_connected() ? "MPD • Connected" : "MPD • Offline")
        ->titleAlignment(TitleAlignment::Center)
        ->onBack([this]() {
            m_nav_view->pop();
        })
        ->onRefresh([this]() {
            refresh_all();
        })
        ->onClose([this]() {
            if (m_window) m_window->request_close();
            else quit();
        })
        ->build();
    m_toolbar->set_back_visible(false);
    m_toolbar->set_margin(14, 12, 14, 8);
    root_container->add_view(m_toolbar);

    // 2. Master ViewPager (4 Dedicated Pages)
    m_view_pager = ViewPagerBuilder::create()->build();
    m_view_pager->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        0,
        1.0f
    ));
    m_view_pager->set_margin(14, 0, 14, 0);

    // Page 0: Now Playing
    m_player_view = std::make_shared<PlayerView>();
    auto player_scroll = ScrollViewBuilder::create()
        ->contentView(m_player_view)
        ->scrollbar(true)
        ->build();
    player_scroll->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    m_view_pager->add_page(player_scroll);

    // Page 1: Queue
    m_queue_view = std::make_shared<QueueView>();
    m_view_pager->add_page(m_queue_view);

    // Page 2: Library
    m_library_view = std::make_shared<LibraryView>();
    m_view_pager->add_page(m_library_view);

    // Page 3: Artists
    m_artists_view = std::make_shared<ArtistsView>();
    m_view_pager->add_page(m_artists_view);

    root_container->add_view(m_view_pager);

    // 3. Master Bottom Navigation Bar
    m_bottom_nav = BottomNavigationViewBuilder::create()
        ->addItem("Player", "media-playback-start")
        ->addItem("Queue", "view-list-symbolic")
        ->addItem("Library", "audio-x-generic")
        ->addItem("Artists", "system-users")
        ->selectedIndex(0)
        ->cornerRadius(24)
        ->barHeight(56)
        ->itemWidth(74)
        ->showDivider(false)
        ->build();
    m_bottom_nav->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        56,
        Gravity::CenterHorizontal
    ));
    m_bottom_nav->set_margin(16, 6, 16, 12);
    root_container->add_view(m_bottom_nav);

    m_nav_view->push(root_container, "Music", m_client->is_connected() ? "MPD • Connected" : "MPD • Offline", "root");

    // Action button for settings
    auto btn_settings = m_toolbar->add_action(icons::SETTINGS, [this]() {
        auto settings_v = SettingsView::create(*m_client, m_host, m_port, [this](bool connected) {
            m_toolbar->set_subtitle(connected ? "MPD • Connected" : "MPD • Disconnected");
            if (m_window) m_window->schedule_redraw();
        });
        m_nav_view->push(settings_v, "Settings", "Preferences & Server", "settings");
    });

    m_nav_view->set_on_navigation_listener([this, btn_settings](const NavigationPage& page, bool can_go_back) {
        m_toolbar->set_title(page.title);
        m_toolbar->set_subtitle(page.subtitle);
        m_toolbar->set_back_visible(can_go_back);
        if (btn_settings) {
            btn_settings->set_visibility(can_go_back ? Visibility::Gone : Visibility::Visible);
        }
    });

    // Control wireups
    m_bottom_nav->set_on_item_selected_listener([this](int idx) {
        m_view_pager->set_current_page(idx);
        if (idx == 2 && !m_library_view->is_loaded()) {
            m_library_view->load_songs(m_client->get_all_songs());
        } else if (idx == 3 && !m_artists_view->is_loaded()) {
            m_artists_view->load_artists(m_client->get_all_songs());
        }
    });

    m_view_pager->set_on_page_changed_listener([this](int old_idx, int new_idx) {
        (void)old_idx;
        if (m_bottom_nav && m_bottom_nav->get_selected_index() != new_idx) {
            m_bottom_nav->set_selected_index(new_idx);
        }
        if (new_idx == 2 && !m_library_view->is_loaded()) {
            m_library_view->load_songs(m_client->get_all_songs());
        } else if (new_idx == 3 && !m_artists_view->is_loaded()) {
            m_artists_view->load_artists(m_client->get_all_songs());
        }
    });

    m_player_view->set_on_play_pause([this]() { m_client->toggle_pause(); });
    m_player_view->set_on_prev([this]() { m_client->previous(); });
    m_player_view->set_on_next([this]() { m_client->next(); });
    m_player_view->set_on_toggle_shuffle([this]() {
        std::lock_guard<std::mutex> lock(m_status_mutex);
        m_client->set_random(!m_status.random);
    });
    m_player_view->set_on_toggle_repeat([this]() {
        std::lock_guard<std::mutex> lock(m_status_mutex);
        if (!m_status.repeat && !m_status.single) {
            m_client->set_repeat(true);
            m_client->set_single(false);
        } else if (m_status.repeat && !m_status.single) {
            m_client->set_repeat(true);
            m_client->set_single(true);
        } else {
            m_client->set_repeat(false);
            m_client->set_single(false);
        }
    });
    m_player_view->set_on_seek([this](float fraction) {
        m_client->seek_fraction(fraction);
    });
    m_player_view->set_on_volume([this](int vol) {
        m_client->set_volume(vol);
    });

    m_queue_view->set_on_song_clicked([this](unsigned int pos) {
        m_client->play_pos(pos);
    });

    m_library_view->set_on_song_selected([this](const std::string& uri) {
        m_client->add_and_play(uri);
        refresh_all();
        m_bottom_nav->set_selected_index(0);
        m_view_pager->set_current_page(0);
    });

    m_artists_view->set_on_artist_play([this](const std::string& uri) {
        m_client->add_and_play(uri);
        refresh_all();
        m_bottom_nav->set_selected_index(0);
        m_view_pager->set_current_page(0);
    });
}

void MusicApp::setup_mpd_callbacks() {
    m_client->set_on_status_changed([this](const MpdStatus& st, const Song& song) {
        update_playback_ui(st, song);
        std::vector<Song> q = m_client->get_queue();
        update_queue_ui(q, st.song_pos);
    });

    m_client->set_on_queue_changed([this](const std::vector<Song>& q) {
        int pos = -1;
        {
            std::lock_guard<std::mutex> lock(m_status_mutex);
            pos = m_status.song_pos;
        }
        update_queue_ui(q, pos);
    });

    m_client->set_on_connection_changed([this](bool connected) {
        m_toolbar->set_subtitle(connected ? "MPD • Connected" : "MPD • Offline");
        if (m_window) m_window->schedule_redraw();
    });
}

void MusicApp::start_ticker() {
    m_running.store(true);
    m_ticker_thread = std::thread([this]() {
        while (m_running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            if (!m_running.load()) break;

            bool is_playing = false;
            unsigned int total = 0;
            {
                std::lock_guard<std::mutex> lock(m_status_mutex);
                is_playing = (m_status.state == PlaybackState::Playing);
                total = m_status.total_seconds;
            }

            if (is_playing && total > 0 && m_player_view && !m_player_view->is_user_seeking()) {
                if (m_engine) {
                    m_engine->post([this]() {
                        std::lock_guard<std::mutex> lock(m_status_mutex);
                        if (m_status.state != PlaybackState::Playing) return;
                        if (m_status.elapsed_seconds < m_status.total_seconds) {
                            m_status.elapsed_seconds++;
                            if (m_player_view) {
                                m_player_view->update_elapsed(m_status.elapsed_seconds, m_status.total_seconds);
                            }
                            if (m_window) m_window->schedule_redraw();
                        }
                    });
                }
            }
        }
    });
}

void MusicApp::stop_ticker() {
    m_running.store(false);
    if (m_ticker_thread.joinable()) {
        m_ticker_thread.join();
    }
}

void MusicApp::update_playback_ui(const MpdStatus& st, const Song& song) {
    std::vector<Song> q;
    {
        std::lock_guard<std::mutex> lock(m_status_mutex);
        m_status = st;
        m_current_song = song;
        q = m_queue;
    }

    if (m_toolbar && m_nav_view && m_nav_view->get_depth() <= 1) {
        m_toolbar->set_subtitle(st.connected ? "MPD • Connected" : "MPD • Disconnected");
    }

    if (m_player_view) {
        m_player_view->update_playback(st, song, q, *m_client);
    }
    if (m_window) m_window->schedule_redraw();
}

void MusicApp::update_queue_ui(const std::vector<Song>& queue, int current_pos) {
    {
        std::lock_guard<std::mutex> lock(m_status_mutex);
        m_queue = queue;
    }

    if (m_queue_view) {
        m_queue_view->update_queue(queue, current_pos);
    }
    if (m_window) m_window->schedule_redraw();
}

void MusicApp::refresh_all() {
    MpdStatus st = m_client->get_status();
    Song song = m_client->get_current_song();
    std::vector<Song> q = m_client->get_queue();
    update_playback_ui(st, song);
    update_queue_ui(q, st.song_pos);

    if (m_bottom_nav) {
        if (m_bottom_nav->get_selected_index() == 2 && m_library_view) {
            m_library_view->load_songs(m_client->get_all_songs());
        } else if (m_bottom_nav->get_selected_index() == 3 && m_artists_view) {
            m_artists_view->load_artists(m_client->get_all_songs());
        }
    }
}

void MusicApp::handle_key_press(const KeyPressEvent& ev) {
    if (!ev.pressed) return;
    switch (ev.keysym) {
        case XKB_KEY_Escape:
            if (m_nav_view && m_nav_view->can_go_back()) {
                m_nav_view->pop();
            } else {
                quit();
            }
            break;
        case XKB_KEY_space:
            m_client->toggle_pause();
            break;
        case XKB_KEY_n:
        case XKB_KEY_N:
            m_client->next();
            break;
        case XKB_KEY_p:
        case XKB_KEY_P:
            m_client->previous();
            break;
        case XKB_KEY_Right: {
            std::lock_guard<std::mutex> lock(m_status_mutex);
            if (m_status.total_seconds > 0) {
                unsigned int target = std::min(m_status.total_seconds, m_status.elapsed_seconds + 5);
                m_client->seek(target);
            }
            break;
        }
        case XKB_KEY_Left: {
            std::lock_guard<std::mutex> lock(m_status_mutex);
            if (m_status.total_seconds > 0) {
                unsigned int target = (m_status.elapsed_seconds > 5) ? (m_status.elapsed_seconds - 5) : 0;
                m_client->seek(target);
            }
            break;
        }
        case XKB_KEY_Up: {
            std::lock_guard<std::mutex> lock(m_status_mutex);
            if (m_status.volume >= 0) {
                m_client->set_volume(std::min(100, m_status.volume + 5));
            }
            break;
        }
        case XKB_KEY_Down: {
            std::lock_guard<std::mutex> lock(m_status_mutex);
            if (m_status.volume >= 0) {
                m_client->set_volume(std::max(0, m_status.volume - 5));
            }
            break;
        }
        case XKB_KEY_1:
            if (m_bottom_nav) m_bottom_nav->set_selected_index(0);
            if (m_view_pager) m_view_pager->set_current_page(0);
            break;
        case XKB_KEY_2:
            if (m_bottom_nav) m_bottom_nav->set_selected_index(1);
            if (m_view_pager) m_view_pager->set_current_page(1);
            break;
        case XKB_KEY_3:
            if (m_bottom_nav) m_bottom_nav->set_selected_index(2);
            if (m_view_pager) m_view_pager->set_current_page(2);
            if (m_library_view && !m_library_view->is_loaded()) {
                m_library_view->load_songs(m_client->get_all_songs());
            }
            break;
        case XKB_KEY_4:
            if (m_bottom_nav) m_bottom_nav->set_selected_index(3);
            if (m_view_pager) m_view_pager->set_current_page(3);
            if (m_artists_view && !m_artists_view->is_loaded()) {
                m_artists_view->load_artists(m_client->get_all_songs());
            }
            break;
        default:
            break;
    }
}

int MusicApp::run() {
    if (!m_engine) return 1;
    return m_engine->enter_loop();
}

void MusicApp::quit() {
    stop_ticker();
    if (m_client) {
        m_client->stop_idle_listener();
    }
    if (m_engine) {
        m_engine->quit(0);
    }
}

} // namespace miqumusic
