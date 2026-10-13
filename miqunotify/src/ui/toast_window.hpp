#pragma once

#include "toast_view.hpp"
#include <miqutoolkit/core/window.hpp>
#include <miqutoolkit/view/linear_layout.hpp>
#include <vector>
#include <memory>
#include <atomic>
#include <thread>

namespace miqunotify {

class ToastWindow {
public:
    ToastWindow();
    ~ToastWindow();

    void add_toast(std::shared_ptr<ToastView> toast);
    void remove_toast(uint32_t id);
    void tick_all(int delta_ms);
    bool has_toasts() const { return !m_toasts.empty(); }

    void close();

private:
    void ensure_window();
    void update_window_geometry();

    std::shared_ptr<miqu::Window> m_window;
    std::shared_ptr<miqu::LinearLayout> m_stack_layout;
    std::vector<std::shared_ptr<ToastView>> m_toasts;

    std::thread m_timer_thread;
    std::atomic<bool> m_timer_running{false};
};

} // namespace miqunotify
