#include "audio_backend.hpp"
#include "miqutoolkit/core/app_engine.hpp"

#include <iostream>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

namespace miqubar {

AudioBackend& AudioBackend::get() {
    static AudioBackend s_instance;
    return s_instance;
}

AudioBackend::AudioBackend() = default;

AudioBackend::~AudioBackend() {
    shutdown();
}

void AudioBackend::init(miqu::AppEngine* engine) {
    m_engine = engine;

    // 1. Initial synchronous query to populate cache
    m_cached_info = query_sink_info_raw();

    m_running.store(true);

    // 2. Start dispatch thread for async volume/mute execution
    m_dispatch_thread = std::thread(&AudioBackend::dispatch_worker, this);

    // 3. Start pactl subscribe monitor thread if pactl exists
    if (access("/usr/bin/pactl", X_OK) == 0) {
        int pipe_fds[2];
        if (pipe(pipe_fds) == 0) {
            pid_t pid = fork();
            if (pid == 0) {
                // Child process
                close(pipe_fds[0]);
                dup2(pipe_fds[1], STDOUT_FILENO);
                close(pipe_fds[1]);

                int dev_null = open("/dev/null", O_WRONLY);
                if (dev_null >= 0) {
                    dup2(dev_null, STDERR_FILENO);
                    close(dev_null);
                }

                execl("/usr/bin/pactl", "pactl", "subscribe", nullptr);
                _exit(127);
            } else if (pid > 0) {
                // Parent process
                close(pipe_fds[1]);
                m_sub_pid = pid;
                m_pipe_fd = pipe_fds[0];
                m_monitor_thread = std::thread(&AudioBackend::monitor_worker, this);
            } else {
                close(pipe_fds[0]);
                close(pipe_fds[1]);
            }
        }
    }
}

void AudioBackend::shutdown() {
    if (!m_running.load()) return;
    m_running.store(false);

    // Stop dispatch thread
    {
        std::lock_guard<std::mutex> lock(m_dispatch_mutex);
        m_dispatch_cv.notify_all();
    }
    if (m_dispatch_thread.joinable()) {
        m_dispatch_thread.join();
    }

    // Stop monitor process & thread
    if (m_sub_pid > 0) {
        kill(m_sub_pid, SIGTERM);
    }
    if (m_pipe_fd >= 0) {
        close(m_pipe_fd);
        m_pipe_fd = -1;
    }
    if (m_monitor_thread.joinable()) {
        m_monitor_thread.join();
    }
    if (m_sub_pid > 0) {
        waitpid(m_sub_pid, nullptr, 0);
        m_sub_pid = -1;
    }
}

AudioSinkInfo AudioBackend::get_sink_info() {
    std::lock_guard<std::mutex> lock(m_state_mutex);
    return m_cached_info;
}

void AudioBackend::set_volume(int pct) {
    pct = std::clamp(pct, 0, 100);

    AudioSinkInfo updated;
    {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        m_cached_info.volume = pct;
        updated = m_cached_info;
    }

    // Enqueue async hardware update
    {
        std::lock_guard<std::mutex> lock(m_dispatch_mutex);
        m_pending_volume = pct;
        m_dispatch_cv.notify_one();
    }

    // Notify listeners immediately for zero-lag UI response
    notify_listeners_on_main_thread(updated);
}

void AudioBackend::toggle_mute() {
    AudioSinkInfo updated;
    {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        m_cached_info.muted = !m_cached_info.muted;
        updated = m_cached_info;
    }

    {
        std::lock_guard<std::mutex> lock(m_dispatch_mutex);
        m_pending_mute_toggle = true;
        m_dispatch_cv.notify_one();
    }

    notify_listeners_on_main_thread(updated);
}

void AudioBackend::add_listener(void* subscriber, ChangeListener cb) {
    std::lock_guard<std::mutex> lock(m_listeners_mutex);
    m_listeners[subscriber] = std::move(cb);
}

void AudioBackend::remove_listener(void* subscriber) {
    std::lock_guard<std::mutex> lock(m_listeners_mutex);
    m_listeners.erase(subscriber);
}

void AudioBackend::notify_listeners_on_main_thread(const AudioSinkInfo& info) {
    auto dispatch = [this, info]() {
        std::vector<ChangeListener> callbacks;
        {
            std::lock_guard<std::mutex> lock(m_listeners_mutex);
            callbacks.reserve(m_listeners.size());
            for (const auto& [_, cb] : m_listeners) {
                if (cb) callbacks.push_back(cb);
            }
        }
        for (const auto& cb : callbacks) {
            cb(info);
        }
    };

    if (m_engine) {
        m_engine->post(dispatch);
    } else {
        dispatch();
    }
}

AudioSinkInfo AudioBackend::query_sink_info_raw() {
    AudioSinkInfo info;
    FILE* fp = popen("wpctl get-volume @DEFAULT_AUDIO_SINK@ 2>/dev/null", "r");
    if (fp) {
        char buf[64];
        if (fgets(buf, sizeof(buf), fp)) {
            std::string str(buf);
            size_t pos = str.find("Volume:");
            if (pos != std::string::npos) {
                try {
                    float v = std::stof(str.substr(pos + 7));
                    info.volume = std::clamp(static_cast<int>(std::round(v * 100.0f)), 0, 100);
                } catch (...) {}
            }
            info.muted = (str.find("[MUTED]") != std::string::npos);
        }
        pclose(fp);
    }
    return info;
}

void AudioBackend::monitor_worker() {
    if (m_pipe_fd < 0) return;

    FILE* fp = fdopen(m_pipe_fd, "r");
    if (!fp) return;

    char buf[256];
    while (m_running.load()) {
        if (!fgets(buf, sizeof(buf), fp)) {
            break;
        }

        std::string line(buf);
        // Look for sink change events or server changes from PipeWire
        if (line.find("'change' on sink") != std::string::npos ||
            line.find("'change' on server") != std::string::npos ||
            line.find("'new' on sink") != std::string::npos ||
            line.find("'remove' on sink") != std::string::npos) {

            AudioSinkInfo new_info = query_sink_info_raw();
            bool changed = false;
            {
                std::lock_guard<std::mutex> lock(m_state_mutex);
                if (m_cached_info.volume != new_info.volume || m_cached_info.muted != new_info.muted) {
                    m_cached_info = new_info;
                    changed = true;
                }
            }
            if (changed) {
                notify_listeners_on_main_thread(new_info);
            }
        }
    }

    fclose(fp);
}

void AudioBackend::dispatch_worker() {
    while (m_running.load()) {
        int vol_to_set = -1;
        bool do_mute_toggle = false;

        {
            std::unique_lock<std::mutex> lock(m_dispatch_mutex);
            m_dispatch_cv.wait(lock, [this]() {
                return !m_running.load() || m_pending_volume >= 0 || m_pending_mute_toggle;
            });

            if (!m_running.load()) break;

            if (m_pending_volume >= 0) {
                vol_to_set = m_pending_volume;
                m_pending_volume = -1;
            }
            if (m_pending_mute_toggle) {
                do_mute_toggle = true;
                m_pending_mute_toggle = false;
            }
        }

        if (do_mute_toggle) {
            std::system("wpctl set-mute @DEFAULT_AUDIO_SINK@ toggle 2>/dev/null || pactl set-sink-mute @DEFAULT_SINK@ toggle 2>/dev/null");
        }

        if (vol_to_set >= 0) {
            float norm = vol_to_set / 100.0f;
            std::string cmd = "wpctl set-volume -l 1.0 @DEFAULT_AUDIO_SINK@ " + std::to_string(norm) + " 2>/dev/null";
            std::system(cmd.c_str());
        }

        // Small sleep to coalesce subsequent high-frequency slider events
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
}

} // namespace miqubar
