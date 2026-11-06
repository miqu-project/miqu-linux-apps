#pragma once

#include <string>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>

namespace miqu {
class AppEngine;
}

namespace miqubar {

struct AudioSinkInfo {
    int volume = 70; // 0 to 100
    bool muted = false;
};

class AudioBackend {
public:
    static AudioBackend& get();

    void init(miqu::AppEngine* engine);
    void shutdown();

    AudioSinkInfo get_sink_info();
    void set_volume(int pct);
    void toggle_mute();

    using ChangeListener = std::function<void(const AudioSinkInfo&)>;
    void add_listener(void* subscriber, ChangeListener cb);
    void remove_listener(void* subscriber);

private:
    AudioBackend();
    ~AudioBackend();
    AudioBackend(const AudioBackend&) = delete;
    AudioBackend& operator=(const AudioBackend&) = delete;

    void monitor_worker();
    void dispatch_worker();
    static AudioSinkInfo query_sink_info_raw();
    void notify_listeners_on_main_thread(const AudioSinkInfo& info);

    miqu::AppEngine* m_engine = nullptr;

    std::mutex m_state_mutex;
    AudioSinkInfo m_cached_info;

    std::mutex m_listeners_mutex;
    std::unordered_map<void*, ChangeListener> m_listeners;

    // Monitor thread (pactl subscribe)
    std::atomic<bool> m_running{false};
    std::thread m_monitor_thread;
    pid_t m_sub_pid{-1};
    int m_pipe_fd{-1};

    // Dispatch thread for coalescing slider volume changes
    std::thread m_dispatch_thread;
    std::mutex m_dispatch_mutex;
    std::condition_variable m_dispatch_cv;
    int m_pending_volume{-1};
    bool m_pending_mute_toggle{false};
};

} // namespace miqubar
