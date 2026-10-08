#pragma once

#include "mpd_types.hpp"
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>

struct mpd_connection;

namespace miqumusic {

class MpdClient {
public:
    MpdClient();
    ~MpdClient();

    bool connect(const std::string& host = "", unsigned int port = 0);
    void disconnect();
    bool is_connected() const;

    void start_idle_listener();
    void stop_idle_listener();

    // Transport controls
    bool play();
    bool pause();
    bool toggle_pause();
    bool stop();
    bool next();
    bool previous();
    bool seek(unsigned int seconds);
    bool seek_fraction(float fraction);
    bool play_pos(unsigned int pos);

    // Audio & playback modes
    bool set_volume(int volume);
    bool set_repeat(bool repeat);
    bool set_random(bool random);
    bool set_single(bool single);
    bool set_consume(bool consume);

    // State getters
    MpdStatus get_status();
    Song get_current_song();
    std::vector<Song> get_queue();
    std::vector<Song> get_all_songs();

    // Queue modification
    bool add_and_play(const std::string& uri);
    bool update_database();

    // Album art / embedded picture retrieval via MPD protocol
    std::vector<uint8_t> fetch_art_data(const std::string& uri);

    // Callback registrations (invoked on the UI thread via AppEngine::post)
    void set_on_status_changed(std::function<void(const MpdStatus&, const Song&)> cb) {
        m_on_status_changed = std::move(cb);
    }
    void set_on_queue_changed(std::function<void(const std::vector<Song>&)> cb) {
        m_on_queue_changed = std::move(cb);
    }
    void set_on_connection_changed(std::function<void(bool connected)> cb) {
        m_on_connection_changed = std::move(cb);
    }

private:
    struct mpd_connection* open_connection();
    bool check_connection_locked();

    std::string m_host;
    unsigned int m_port = 0;

    std::mutex m_mutex;
    struct mpd_connection* m_cmd_conn = nullptr;
    struct mpd_connection* m_idle_conn = nullptr;

    std::atomic<bool> m_running{false};
    std::thread m_idle_thread;

    std::function<void(const MpdStatus&, const Song&)> m_on_status_changed;
    std::function<void(const std::vector<Song>&)> m_on_queue_changed;
    std::function<void(bool connected)> m_on_connection_changed;

    MpdStatus m_last_status;
    Song m_last_song;
};

} // namespace miqumusic
