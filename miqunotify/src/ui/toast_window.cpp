#include "toast_window.hpp"
#include "../config/notify_config.hpp"
#include <miqutoolkit/core/app_engine.hpp>
#include <algorithm>
#include <chrono>

namespace miqunotify {

ToastWindow::ToastWindow() {
    const auto& cfg = NotifyConfig::get();
    m_stack_layout = std::make_shared<miqu::LinearLayout>(miqu::Orientation::Vertical);
    m_stack_layout->set_divider_spacing(cfg.toast_spacing);
    m_stack_layout->set_padding(0);
}

ToastWindow::~ToastWindow() {
    close();
}

void ToastWindow::ensure_window() {
    if (m_window) return;

    const auto& cfg = NotifyConfig::get();

    uint32_t anchors = 1 | 8; // Top | Right
    if (cfg.position == "top-left") anchors = 1 | 4;
    else if (cfg.position == "bottom-right") anchors = 2 | 8;
    else if (cfg.position == "bottom-left") anchors = 2 | 4;

    miqu::Margin margin{cfg.margin_y, cfg.margin_x, cfg.margin_y, cfg.margin_x};

    m_window = miqu::WindowBuilder::create()
        ->role(miqu::WindowRole::LayerTop)
        ->appId("miqunotify")
        ->layerNamespace("miqunotify")
        ->anchors(anchors)
        ->contentMargin(margin)
        ->keyboardInteractive(false)
        ->closeOnClickOutside(false)
        ->closeOnEscape(false)
        ->transparent(true)
        ->preferredSize(cfg.toast_width, 100)
        ->contentView(m_stack_layout)
        ->build();

    if (m_window) {
        m_window->show();
    }

    // Start timer loop if not running
    if (!m_timer_running) {
        m_timer_running = true;
        m_timer_thread = std::thread([this]() {
            while (m_timer_running) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                if (!m_timer_running) break;

                auto engine = miqu::AppEngine::instance();
                if (engine) {
                    engine->post([this]() {
                        tick_all(50);
                    });
                }
            }
        });
    }
}

void ToastWindow::update_window_geometry() {
    if (!m_window) return;
    const auto& cfg = NotifyConfig::get();

    // Measure total required height
    int total_h = 0;
    for (size_t i = 0; i < m_toasts.size(); ++i) {
        auto sz = m_toasts[i]->measure_size(cfg.toast_width);
        total_h += sz.height;
        if (i + 1 < m_toasts.size()) {
            total_h += cfg.toast_spacing;
        }
    }

    total_h = std::max(50, total_h);
    m_window->set_preferred_size(cfg.toast_width, total_h);
    m_window->set_content_size(cfg.toast_width, total_h);
    m_window->schedule_redraw();
}

void ToastWindow::add_toast(std::shared_ptr<ToastView> toast) {
    if (!toast) return;

    const auto& cfg = NotifyConfig::get();

    // Cap at max_toasts
    while (m_toasts.size() >= static_cast<size_t>(cfg.max_toasts)) {
        remove_toast(m_toasts.front()->get_id());
    }

    m_toasts.push_back(toast);
    m_stack_layout->add_view(toast, miqu::LayoutParams(cfg.toast_width, static_cast<int>(miqu::LayoutDimension::WrapContent)));

    ensure_window();
    update_window_geometry();
}

void ToastWindow::remove_toast(uint32_t id) {
    auto it = std::find_if(m_toasts.begin(), m_toasts.end(), [id](const auto& t) {
        return t->get_id() == id;
    });

    if (it != m_toasts.end()) {
        m_stack_layout->remove_view(*it);
        m_toasts.erase(it);
    }

    if (m_toasts.empty()) {
        close();
    } else {
        update_window_geometry();
    }
}

void ToastWindow::tick_all(int delta_ms) {
    if (m_toasts.empty()) return;

    // Tick each toast (make a copy of list to prevent iterator invalidation on dismiss)
    auto list_copy = m_toasts;
    for (auto& toast : list_copy) {
        toast->tick(delta_ms);
    }
}

void ToastWindow::close() {
    m_timer_running = false;
    if (m_timer_thread.joinable()) {
        m_timer_thread.join();
    }

    if (m_window) {
        m_window->close();
        m_window.reset();
    }

    m_toasts.clear();
    m_stack_layout->clear_views();
}

} // namespace miqunotify
