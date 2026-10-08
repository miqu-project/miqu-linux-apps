#include "mpd_client.hpp"
#include <miqutoolkit/core/app_engine.hpp>
#include <mpd/client.h>
#include <iostream>
#include <chrono>

namespace miqumusic {

static Song parse_mpd_song(struct mpd_song* s) {
    Song song;
    if (!s) return song;

    const char* uri = mpd_song_get_uri(s);
    if (uri) song.uri = uri;

    const char* title = mpd_song_get_tag(s, MPD_TAG_TITLE, 0);
    if (title) song.title = title;

    const char* artist = mpd_song_get_tag(s, MPD_TAG_ARTIST, 0);
    if (artist) song.artist = artist;

    const char* album = mpd_song_get_tag(s, MPD_TAG_ALBUM, 0);
    if (album) song.album = album;

    const char* date = mpd_song_get_tag(s, MPD_TAG_DATE, 0);
    if (date) song.date = date;

    const char* genre = mpd_song_get_tag(s, MPD_TAG_GENRE, 0);
    if (genre) song.genre = genre;

    const char* track = mpd_song_get_tag(s, MPD_TAG_TRACK, 0);
    if (track) song.track = track;

    song.duration = mpd_song_get_duration(s);
    song.id = mpd_song_get_id(s);
    song.pos = mpd_song_get_pos(s);
    return song;
}

MpdClient::MpdClient() = default;

MpdClient::~MpdClient() {
    disconnect();
}

struct mpd_connection* MpdClient::open_connection() {
    const char* host = m_host.empty() ? nullptr : m_host.c_str();
    struct mpd_connection* conn = mpd_connection_new(host, m_port, 4000);
    if (!conn) return nullptr;

    if (mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS) {
        std::cerr << "[miqumusic] MPD connection error: "
                  << mpd_connection_get_error_message(conn) << "\n";
        mpd_connection_free(conn);
        return nullptr;
    }
    return conn;
}

bool MpdClient::check_connection_locked() {
    if (!m_cmd_conn) {
        m_cmd_conn = open_connection();
        return m_cmd_conn != nullptr;
    }

    enum mpd_error err = mpd_connection_get_error(m_cmd_conn);
    if (err != MPD_ERROR_SUCCESS) {
        if (err == MPD_ERROR_SERVER) {
            mpd_connection_clear_error(m_cmd_conn);
            return true;
        }
        // Fatal error, reopen
        mpd_connection_free(m_cmd_conn);
        m_cmd_conn = open_connection();
        return m_cmd_conn != nullptr;
    }
    return true;
}

bool MpdClient::connect(const std::string& host, unsigned int port) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_host = host;
    m_port = port;

    if (m_cmd_conn) {
        mpd_connection_free(m_cmd_conn);
        m_cmd_conn = nullptr;
    }
    m_cmd_conn = open_connection();
    return m_cmd_conn != nullptr;
}

void MpdClient::disconnect() {
    stop_idle_listener();

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_cmd_conn) {
        mpd_connection_free(m_cmd_conn);
        m_cmd_conn = nullptr;
    }
}

bool MpdClient::is_connected() const {
    return m_cmd_conn != nullptr;
}

void MpdClient::start_idle_listener() {
    if (m_running.load()) return;
    m_running.store(true);

    m_idle_thread = std::thread([this]() {
        while (m_running.load()) {
            if (!m_idle_conn) {
                m_idle_conn = open_connection();
                if (!m_idle_conn) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
                    continue;
                }
            }

            // Wait for events
            enum mpd_idle events = mpd_run_idle_mask(m_idle_conn,
                static_cast<enum mpd_idle>(MPD_IDLE_PLAYER | MPD_IDLE_MIXER | MPD_IDLE_OPTIONS | MPD_IDLE_QUEUE));

            if (!m_running.load()) break;

            if (events == 0) {
                // Connection lost or error
                if (m_idle_conn) {
                    mpd_connection_free(m_idle_conn);
                    m_idle_conn = nullptr;
                }
                if (auto engine = miqu::AppEngine::instance()) {
                    engine->post([this]() {
                        if (m_on_connection_changed) m_on_connection_changed(false);
                    });
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1500));
                continue;
            }

            // Query updated state
            if (events & (MPD_IDLE_PLAYER | MPD_IDLE_MIXER | MPD_IDLE_OPTIONS)) {
                MpdStatus status = get_status();
                Song song = get_current_song();

                if (auto engine = miqu::AppEngine::instance()) {
                    engine->post([this, status, song]() {
                        m_last_status = status;
                        m_last_song = song;
                        if (m_on_status_changed) m_on_status_changed(status, song);
                    });
                }
            }

            if (events & MPD_IDLE_QUEUE) {
                std::vector<Song> queue = get_queue();

                if (auto engine = miqu::AppEngine::instance()) {
                    engine->post([this, queue]() {
                        if (m_on_queue_changed) m_on_queue_changed(queue);
                    });
                }
            }
        }

        if (m_idle_conn) {
            mpd_connection_free(m_idle_conn);
            m_idle_conn = nullptr;
        }
    });
}

void MpdClient::stop_idle_listener() {
    if (!m_running.load()) return;
    m_running.store(false);

    // Cancel idle on connection
    if (m_idle_conn) {
        mpd_send_noidle(m_idle_conn);
    }

    if (m_idle_thread.joinable()) {
        m_idle_thread.join();
    }
}

bool MpdClient::play() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!check_connection_locked()) return false;
    return mpd_run_play(m_cmd_conn);
}

bool MpdClient::pause() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!check_connection_locked()) return false;
    return mpd_run_pause(m_cmd_conn, true);
}

bool MpdClient::toggle_pause() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!check_connection_locked()) return false;
    return mpd_run_toggle_pause(m_cmd_conn);
}

bool MpdClient::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!check_connection_locked()) return false;
    return mpd_run_stop(m_cmd_conn);
}

bool MpdClient::next() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!check_connection_locked()) return false;
    return mpd_run_next(m_cmd_conn);
}

bool MpdClient::previous() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!check_connection_locked()) return false;
    return mpd_run_previous(m_cmd_conn);
}

bool MpdClient::seek(unsigned int seconds) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!check_connection_locked()) return false;
    return mpd_run_seek_current(m_cmd_conn, static_cast<float>(seconds), false);
}

bool MpdClient::seek_fraction(float fraction) {
    MpdStatus st = get_status();
    if (st.total_seconds == 0) return false;
    unsigned int target = static_cast<unsigned int>(fraction * st.total_seconds);
    return seek(target);
}

bool MpdClient::play_pos(unsigned int pos) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!check_connection_locked()) return false;
    return mpd_run_play_pos(m_cmd_conn, pos);
}

bool MpdClient::set_volume(int volume) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!check_connection_locked()) return false;
    int vol = std::clamp(volume, 0, 100);
    return mpd_run_set_volume(m_cmd_conn, vol);
}

bool MpdClient::set_repeat(bool repeat) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!check_connection_locked()) return false;
    return mpd_run_repeat(m_cmd_conn, repeat);
}

bool MpdClient::set_random(bool random) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!check_connection_locked()) return false;
    return mpd_run_random(m_cmd_conn, random);
}

bool MpdClient::set_single(bool single) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!check_connection_locked()) return false;
    return mpd_run_single(m_cmd_conn, single);
}

bool MpdClient::set_consume(bool consume) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!check_connection_locked()) return false;
    return mpd_run_consume(m_cmd_conn, consume);
}

bool MpdClient::update_database() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!check_connection_locked()) return false;
    return mpd_run_update(m_cmd_conn, nullptr) > 0;
}

MpdStatus MpdClient::get_status() {
    std::lock_guard<std::mutex> lock(m_mutex);
    MpdStatus res;
    if (!check_connection_locked()) return res;

    struct mpd_status* status = mpd_run_status(m_cmd_conn);
    if (!status) return res;

    res.connected = true;
    switch (mpd_status_get_state(status)) {
        case MPD_STATE_PLAY:
            res.state = PlaybackState::Playing;
            break;
        case MPD_STATE_PAUSE:
            res.state = PlaybackState::Paused;
            break;
        case MPD_STATE_STOP:
        default:
            res.state = PlaybackState::Stopped;
            break;
    }

    res.elapsed_seconds = mpd_status_get_elapsed_time(status);
    res.total_seconds = mpd_status_get_total_time(status);
    if (res.total_seconds > 0) {
        res.elapsed_fraction = static_cast<float>(res.elapsed_seconds) / static_cast<float>(res.total_seconds);
    }

    res.volume = mpd_status_get_volume(status);
    res.repeat = mpd_status_get_repeat(status);
    res.random = mpd_status_get_random(status);
    res.single = (mpd_status_get_single_state(status) != MPD_SINGLE_OFF);
    res.consume = mpd_status_get_consume(status);
    res.song_pos = mpd_status_get_song_pos(status);
    res.song_id = mpd_status_get_song_id(status);
    res.queue_version = mpd_status_get_queue_version(status);
    res.queue_length = mpd_status_get_queue_length(status);
    res.bitrate = mpd_status_get_kbit_rate(status);

    const struct mpd_audio_format* af = mpd_status_get_audio_format(status);
    if (af && af->sample_rate > 0) {
        std::ostringstream oss;
        if (af->sample_rate % 1000 == 0) {
            oss << (af->sample_rate / 1000) << " kHz";
        } else {
            oss << std::fixed << std::setprecision(1) << (af->sample_rate / 1000.0) << " kHz";
        }
        if (af->bits > 0) {
            oss << " • " << static_cast<int>(af->bits) << "-bit";
        }
        if (af->channels == 1) oss << " • Mono";
        else if (af->channels == 2) oss << " • Stereo";
        else if (af->channels > 2) oss << " • " << static_cast<int>(af->channels) << "ch";
        res.audio_format = oss.str();
    }

    mpd_status_free(status);
    return res;
}

Song MpdClient::get_current_song() {
    std::lock_guard<std::mutex> lock(m_mutex);
    Song res;
    if (!check_connection_locked()) return res;

    struct mpd_song* song = mpd_run_current_song(m_cmd_conn);
    if (song) {
        res = parse_mpd_song(song);
        mpd_song_free(song);
    }
    return res;
}

std::vector<Song> MpdClient::get_queue() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<Song> queue;
    if (!check_connection_locked()) return queue;

    if (!mpd_send_list_queue_meta(m_cmd_conn)) {
        return queue;
    }

    struct mpd_song* song = nullptr;
    while ((song = mpd_recv_song(m_cmd_conn)) != nullptr) {
        queue.push_back(parse_mpd_song(song));
        mpd_song_free(song);
    }
    mpd_response_finish(m_cmd_conn);
    return queue;
}

std::vector<uint8_t> MpdClient::fetch_art_data(const std::string& uri) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<uint8_t> data;
    if (uri.empty() || !check_connection_locked()) return data;

    char buf[8192];
    int n = 0;

    // 1. Try embedded picture
    while ((n = mpd_run_readpicture(m_cmd_conn, uri.c_str(), static_cast<unsigned int>(data.size()), buf, sizeof(buf))) > 0) {
        data.insert(data.end(), buf, buf + n);
    }
    if (mpd_connection_get_error(m_cmd_conn) == MPD_ERROR_SERVER) {
        mpd_connection_clear_error(m_cmd_conn);
    }

    // 2. If no embedded picture, try albumart
    if (data.empty()) {
        while ((n = mpd_run_albumart(m_cmd_conn, uri.c_str(), static_cast<unsigned int>(data.size()), buf, sizeof(buf))) > 0) {
            data.insert(data.end(), buf, buf + n);
        }
        if (mpd_connection_get_error(m_cmd_conn) == MPD_ERROR_SERVER) {
            mpd_connection_clear_error(m_cmd_conn);
        }
    }

    return data;
}

std::vector<Song> MpdClient::get_all_songs() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<Song> songs;
    if (!check_connection_locked()) return songs;

    if (!mpd_send_list_all_meta(m_cmd_conn, "")) {
        return songs;
    }

    struct mpd_entity* entity = nullptr;
    while ((entity = mpd_recv_entity(m_cmd_conn)) != nullptr) {
        if (mpd_entity_get_type(entity) == MPD_ENTITY_TYPE_SONG) {
            const struct mpd_song* s = mpd_entity_get_song(entity);
            if (s) {
                songs.push_back(parse_mpd_song(const_cast<struct mpd_song*>(s)));
            }
        }
        mpd_entity_free(entity);
    }
    mpd_response_finish(m_cmd_conn);
    return songs;
}

bool MpdClient::add_and_play(const std::string& uri) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (uri.empty() || !check_connection_locked()) return false;

    int song_id = mpd_run_add_id(m_cmd_conn, uri.c_str());
    if (song_id >= 0) {
        return mpd_run_play_id(m_cmd_conn, static_cast<unsigned int>(song_id));
    }
    return false;
}

} // namespace miqumusic
