#pragma once

#include "widget_container.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <vector>
#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace miqudesk {

class CanvasRootView : public miqu::View {
public:
    CanvasRootView(class DesktopCanvas* canvas);

    void draw(cairo_t* cr, const miqu::Rect& bounds) override;
    bool on_mouse_button(int lx, int ly, miqu::MouseButton button, bool pressed, const miqu::Rect& bounds) override;
    bool on_mouse_move(int lx, int ly, const miqu::Rect& bounds) override;

private:
    class DesktopCanvas* m_canvas;
    std::shared_ptr<WidgetContainer> m_captured_widget;
};

class DesktopCanvas {
public:
    DesktopCanvas(miqu::AppEngine* engine);
    ~DesktopCanvas();

    bool init();
    void add_widget(std::shared_ptr<Widget> widget, int x, int y, int width, int height);
    void bring_to_front(const std::shared_ptr<WidgetContainer>& widget);

    void tick();
    void schedule_redraw();

    bool is_edit_mode() const { return m_edit_mode; }
    void set_edit_mode(bool edit) { m_edit_mode = edit; schedule_redraw(); }
    void toggle_edit_mode() { set_edit_mode(!m_edit_mode); }

    const std::vector<std::shared_ptr<WidgetContainer>>& get_widgets() const { return m_widgets; }

    void load_config();
    void save_config();
    void reload_config();
    void reload_shortcuts();
    void sync_builtin_widgets();
    void request_reload();

private:
    void setup_timer();
    void setup_watcher();

    miqu::AppEngine* m_engine = nullptr;
    std::shared_ptr<miqu::Window> m_window;
    std::shared_ptr<CanvasRootView> m_root_view;

    std::vector<std::shared_ptr<WidgetContainer>> m_widgets;
    bool m_edit_mode = false;

    int m_inotify_fd = -1;
    int m_reload_event_fd = -1;
    std::thread m_timer_thread;
    std::thread m_watcher_thread;
    std::mutex m_timer_mutex;
    std::condition_variable m_timer_cv;
    bool m_running = true;
    std::atomic<bool> m_reload_pending{false};
};

} // namespace miqudesk
