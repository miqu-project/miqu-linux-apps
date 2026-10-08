#include "desktop_canvas.hpp"
#include "desk_config.hpp"
#include "desktop_scanner.hpp"
#include "widgets/app_shortcut_widget.hpp"
#include "widgets/clock_widget.hpp"
#include "widgets/system_widget.hpp"
#include <sys/poll.h>
#include <sys/eventfd.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <thread>
#include <iostream>
#include <algorithm>

namespace miqudesk {

namespace fs = std::filesystem;

CanvasRootView::CanvasRootView(DesktopCanvas* canvas) : m_canvas(canvas) {}

void CanvasRootView::draw(cairo_t* cr, const miqu::Rect& bounds) {
    if (!cr || !m_canvas) return;

    for (const auto& container : m_canvas->get_widgets()) {
        if (container) {
            container->draw(cr);
        }
    }
}

bool CanvasRootView::on_mouse_button(int lx, int ly, miqu::MouseButton button, bool pressed, const miqu::Rect& bounds) {
    if (!m_canvas) return false;

    if (pressed) {
        // Traverse widgets in reverse order (topmost first)
        const auto& widgets = m_canvas->get_widgets();
        for (auto it = widgets.rbegin(); it != widgets.rend(); ++it) {
            if (*it && (*it)->contains(lx, ly)) {
                auto widget = *it;
                m_canvas->bring_to_front(widget);
                m_captured_widget = widget;
                if (widget->handle_mouse_button(lx, ly, button, true)) {
                    m_canvas->schedule_redraw();
                    return true;
                }
                break;
            }
        }
    } else {
        // Pointer release
        if (m_captured_widget) {
            auto widget = m_captured_widget;
            m_captured_widget = nullptr;
            if (widget->handle_mouse_button(lx, ly, button, false)) {
                m_canvas->save_config();
                m_canvas->schedule_redraw();
                return true;
            }
        }
    }
    return false;
}

bool CanvasRootView::on_mouse_move(int lx, int ly, const miqu::Rect& bounds) {
    if (!m_canvas) return false;

    if (m_captured_widget) {
        if (m_captured_widget->handle_mouse_move(lx, ly)) {
            m_canvas->schedule_redraw();
            return true;
        }
    } else {
        bool handled = false;
        for (const auto& w : m_canvas->get_widgets()) {
            if (w && w->handle_mouse_move(lx, ly)) {
                handled = true;
            }
        }
        if (handled) {
            m_canvas->schedule_redraw();
            return true;
        }
    }
    return false;
}

DesktopCanvas::DesktopCanvas(miqu::AppEngine* engine) : m_engine(engine) {}

DesktopCanvas::~DesktopCanvas() {
    {
        std::lock_guard<std::mutex> lock(m_timer_mutex);
        m_running = false;
    }
    m_timer_cv.notify_all();

    if (m_reload_event_fd >= 0) {
        uint64_t val = 1;
        write(m_reload_event_fd, &val, sizeof(val));
    }
    if (m_watcher_thread.joinable()) {
        m_watcher_thread.join();
    }
    if (m_timer_thread.joinable()) {
        m_timer_thread.join();
    }
    if (m_inotify_fd >= 0) {
        ::close(m_inotify_fd);
        m_inotify_fd = -1;
    }
    if (m_reload_event_fd >= 0) {
        ::close(m_reload_event_fd);
        m_reload_event_fd = -1;
    }
}

bool DesktopCanvas::init() {
    if (!m_engine) return false;

    m_edit_mode = DeskConfig::get().edit_mode;
    m_root_view = std::make_shared<CanvasRootView>(this);

    // Create desktop layer covering LayerBottom
    m_window = miqu::WindowBuilder::create()
        ->role(miqu::WindowRole::LayerBottom)
        ->title("miqudesk")
        ->appId("miqudesk")
        ->keyboardInteractive(false)
        ->dimBackdrop(false)
        ->closeOnClickOutside(false)
        ->closeOnEscape(false)
        ->contentView(m_root_view)
        ->onClose([this]() {
            {
                std::lock_guard<std::mutex> lock(m_timer_mutex);
                m_running = false;
            }
            m_timer_cv.notify_all();
        })
        ->build();

    if (!m_window) {
        std::cerr << "[miqudesk] Failed to create desktop canvas window." << std::endl;
        return false;
    }

    m_engine->add_theme_change_listener([this]() {
        request_reload();
    });

    setup_timer();
    setup_watcher();
    return true;
}

void DesktopCanvas::add_widget(std::shared_ptr<Widget> widget, int x, int y, int width, int height) {
    m_widgets.push_back(std::make_shared<WidgetContainer>(std::move(widget), x, y, width, height));
}

void DesktopCanvas::bring_to_front(const std::shared_ptr<WidgetContainer>& widget) {
    auto it = std::find(m_widgets.begin(), m_widgets.end(), widget);
    if (it != m_widgets.end() && it != m_widgets.end() - 1) {
        auto w = *it;
        m_widgets.erase(it);
        m_widgets.push_back(w);
    }
}

void DesktopCanvas::tick() {
    for (const auto& w : m_widgets) {
        if (w) {
            w->tick();
        }
    }
    schedule_redraw();
}

void DesktopCanvas::schedule_redraw() {
    if (m_window) {
        m_window->schedule_redraw();
    }
}

void DesktopCanvas::setup_timer() {
    // Spawn timer thread to trigger tick every second on the main loop
    m_timer_thread = std::thread([this]() {
        std::unique_lock<std::mutex> lock(m_timer_mutex);
        while (m_running) {
            if (m_timer_cv.wait_for(lock, std::chrono::seconds(1), [this]() { return !m_running; })) {
                break;
            }
            if (m_engine) {
                m_engine->post([this]() {
                    if (m_running) {
                        tick();
                    }
                });
            }
        }
    });
}

void DesktopCanvas::setup_watcher() {
    m_inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    m_reload_event_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

    auto add_watch_dir = [this](const std::string& dir) {
        if (m_inotify_fd >= 0 && fs::exists(dir)) {
            inotify_add_watch(m_inotify_fd, dir.c_str(),
                IN_CLOSE_WRITE | IN_MOVED_TO | IN_MOVED_FROM | IN_CREATE | IN_DELETE);
            std::cout << "[miqudesk] Hot reload watching directory: " << dir << std::endl;
        }
    };

    for (const auto& d : DeskConfig::get().get_watched_dirs()) {
        add_watch_dir(d);
    }

    m_watcher_thread = std::thread([this]() {
        struct pollfd pfd[2];
        pfd[0].fd = m_inotify_fd;
        pfd[0].events = POLLIN;
        pfd[1].fd = m_reload_event_fd;
        pfd[1].events = POLLIN;

        while (m_running) {
            int nfds = (m_reload_event_fd >= 0) ? 2 : 1;
            int ret = poll(pfd, nfds, 1000);
            if (!m_running) break;
            if (ret <= 0) continue;

            bool trigger_reload = false;

            if (pfd[1].revents & POLLIN) {
                uint64_t val = 0;
                read(m_reload_event_fd, &val, sizeof(val));
                trigger_reload = true;
            }

            if (pfd[0].revents & POLLIN) {
                trigger_reload = true;
            }

            if (trigger_reload && m_running) {
                // Debounce window (150ms) to allow multi-file writes or burst signals to settle
                std::this_thread::sleep_for(std::chrono::milliseconds(150));

                // Drain inotify buffer
                char buffer[4096];
                while (read(m_inotify_fd, buffer, sizeof(buffer)) > 0) {}

                // Drain eventfd buffer
                if (m_reload_event_fd >= 0) {
                    uint64_t val = 0;
                    while (read(m_reload_event_fd, &val, sizeof(val)) > 0) {}
                }

                if (m_running && m_engine) {
                    bool expected = false;
                    if (m_reload_pending.compare_exchange_strong(expected, true)) {
                        m_engine->post([this]() {
                            m_reload_pending.store(false);
                            if (m_running) {
                                reload_config();
                            }
                        });
                    }
                }
            }
        }
    });
}

void DesktopCanvas::reload_shortcuts() {
    const auto& cfg = DeskConfig::get();

    // 1. Remove existing shortcut widgets from m_widgets
    m_widgets.erase(
        std::remove_if(m_widgets.begin(), m_widgets.end(), [](const std::shared_ptr<WidgetContainer>& wc) {
            if (!wc || !wc->get_widget()) return false;
            return wc->get_widget()->get_id().rfind("shortcut_", 0) == 0;
        }),
        m_widgets.end()
    );

    // 2. Scan current desktop shortcuts
    auto shortcuts = DesktopScanner::get_shortcuts();
    int start_x = cfg.shortcuts_start_x;
    int start_y = cfg.shortcuts_start_y;
    int col_spacing = cfg.col_spacing;
    int row_spacing = cfg.row_spacing;
    int items_per_col = cfg.items_per_col;

    for (size_t i = 0; i < shortcuts.size(); ++i) {
        int col = static_cast<int>(i / items_per_col);
        int row = static_cast<int>(i % items_per_col);
        int x = start_x + col * col_spacing;
        int y = start_y + row * row_spacing;
        auto shortcut_w = std::make_shared<AppShortcutWidget>(shortcuts[i]);
        add_widget(shortcut_w, x, y, cfg.shortcut_width, cfg.shortcut_height);
    }

    // 3. Re-apply any saved custom coordinates from desktop.conf
    load_config();
}

void DesktopCanvas::sync_builtin_widgets() {
    const auto& cfg = DeskConfig::get();

    // 1. Clock Widget
    auto clock_it = std::find_if(m_widgets.begin(), m_widgets.end(), [](const std::shared_ptr<WidgetContainer>& wc) {
        return wc && wc->get_widget() && wc->get_widget()->get_id() == "clock";
    });

    if (cfg.clock_enabled) {
        if (clock_it == m_widgets.end()) {
            auto clock_w = std::make_shared<ClockWidget>();
            add_widget(clock_w, cfg.clock_x, cfg.clock_y, cfg.clock_width, cfg.clock_height);
        }
    } else {
        if (clock_it != m_widgets.end()) {
            m_widgets.erase(clock_it);
        }
    }

    // 2. System Widget
    auto sys_it = std::find_if(m_widgets.begin(), m_widgets.end(), [](const std::shared_ptr<WidgetContainer>& wc) {
        return wc && wc->get_widget() && wc->get_widget()->get_id() == "system";
    });

    if (cfg.system_enabled) {
        if (sys_it == m_widgets.end()) {
            auto sys_w = std::make_shared<SystemWidget>();
            add_widget(sys_w, cfg.system_x, cfg.system_y, cfg.system_width, cfg.system_height);
        }
    } else {
        if (sys_it != m_widgets.end()) {
            m_widgets.erase(sys_it);
        }
    }
}

void DesktopCanvas::reload_config() {
    std::cout << "[miqudesk] Hot reload triggered. Reloading configuration, theme, and desktop shortcuts..." << std::endl;
    DeskConfig::get().load();
    sync_builtin_widgets();
    for (const auto& w : m_widgets) {
        if (w) {
            w->on_config_reload();
        }
    }
    reload_shortcuts();
    load_config();
    schedule_redraw();
}

void DesktopCanvas::request_reload() {
    if (m_reload_event_fd >= 0) {
        uint64_t val = 1;
        write(m_reload_event_fd, &val, sizeof(val));
    }
}

static std::string get_config_path() {
    std::string user_cfg_dir = miqu::FsUtils::get_user_config_dir("miqudesk");
    if (!user_cfg_dir.empty()) {
        return user_cfg_dir + "/desktop.conf";
    }
    return "";
}

void DesktopCanvas::load_config() {
    std::string path = get_config_path();
    if (path.empty() || !fs::exists(path)) return;

    std::ifstream file(path);
    if (!file.is_open()) return;

    std::string line;
    std::string current_section;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;

        if (line.front() == '[' && line.back() == ']') {
            current_section = line.substr(1, line.size() - 2);
            continue;
        }

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        // Trim
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        val.erase(0, val.find_first_not_of(" \t"));
        val.erase(val.find_last_not_of(" \t") + 1);

        for (auto& wc : m_widgets) {
            if (wc && wc->get_widget() && wc->get_widget()->get_id() == current_section) {
                try {
                    if (key == "x") wc->set_position(std::stoi(val), wc->get_y());
                    else if (key == "y") wc->set_position(wc->get_x(), std::stoi(val));
                    else if (key == "width") wc->set_size(std::stoi(val), wc->get_height());
                    else if (key == "height") wc->set_size(wc->get_width(), std::stoi(val));
                } catch (...) {}
            }
        }
    }
}

void DesktopCanvas::save_config() {
    std::string path = get_config_path();
    if (path.empty()) return;

    try {
        fs::path p(path);
        if (!fs::exists(p.parent_path())) {
            fs::create_directories(p.parent_path());
        }

        std::ofstream file(path);
        if (!file.is_open()) return;

        file << "# miqudesk saved layout state\n\n";
        for (const auto& wc : m_widgets) {
            if (wc && wc->get_widget()) {
                file << "[" << wc->get_widget()->get_id() << "]\n";
                file << "x = " << wc->get_x() << "\n";
                file << "y = " << wc->get_y() << "\n";
                file << "width = " << wc->get_width() << "\n";
                file << "height = " << wc->get_height() << "\n\n";
            }
        }
    } catch (...) {}
}

} // namespace miqudesk
