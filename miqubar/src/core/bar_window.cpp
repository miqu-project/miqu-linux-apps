#include "bar_window.hpp"
#include "../config/bar_config.hpp"
#include "miqutoolkit/core/app_engine.hpp"
#include "miqutoolkit/core/config.hpp"
#include "miqutoolkit/view/text_view.hpp"
#include "miqutoolkit/view/tooltip_manager.hpp"
#include <iostream>
#include <cmath>

namespace miqubar {

BarRootView::BarRootView() {
    const auto& cfg = BarConfig::get();
    auto config = miqu::Config::get();

    set_style(cfg.miqubar_outline ? miqu::CardStyle::Outlined : miqu::CardStyle::Filled);
    set_radius(cfg.bar_corner_radius);
    set_elevation(0); // Surface elevation and shadows are handled natively by miquland compositor layer rules
    set_padding(cfg.padding_left, cfg.padding_top, cfg.padding_right, cfg.padding_bottom);

    // 1. Left container (pinned left)
    m_left_box = std::make_shared<miqu::LinearLayout>(miqu::Orientation::Horizontal);
    m_left_box->set_gravity(miqu::Gravity::CenterVertical);
    m_left_box->set_divider_spacing(6);
    m_left_box->set_layout_params(miqu::LayoutParams(
        static_cast<int>(miqu::LayoutDimension::WrapContent),
        static_cast<int>(miqu::LayoutDimension::MatchParent),
        miqu::Gravity::Left | miqu::Gravity::CenterVertical
    ));
    add_view(m_left_box);

    // 2. Center container (centered dock: workspace, start, search, windows)
    m_center_box = std::make_shared<miqu::LinearLayout>(miqu::Orientation::Horizontal);
    m_center_box->set_gravity(miqu::Gravity::CenterVertical);
    m_center_box->set_divider_spacing(4);
    m_center_box->set_layout_params(miqu::LayoutParams(
        static_cast<int>(miqu::LayoutDimension::WrapContent),
        static_cast<int>(miqu::LayoutDimension::MatchParent),
        miqu::Gravity::CenterHorizontal | miqu::Gravity::CenterVertical
    ));

    if (cfg.show_workspace_btn) {
        m_ws_btn = std::make_shared<WorkspaceButtonView>();
        m_ws_btn->set_tooltip("Workspace Overview");
        m_center_box->add_view(m_ws_btn);
    }
    if (cfg.show_start) {
        m_start_btn = std::make_shared<StartButtonView>();
        m_start_btn->set_tooltip("Start Menu");
        m_center_box->add_view(m_start_btn);
    }
    if (cfg.show_search) {
        m_search_btn = std::make_shared<SearchButtonView>();
        m_search_btn->set_tooltip("Search Apps & Files");
        m_center_box->add_view(m_search_btn);
    }
    if (cfg.show_taskbar) {
        m_taskbar = std::make_shared<TaskbarView>();
        m_center_box->add_view(m_taskbar);
    }
    add_view(m_center_box);

    // 3. Right container (system telemetry & tray pinned right)
    m_right_box = std::make_shared<miqu::LinearLayout>(miqu::Orientation::Horizontal);
    m_right_box->set_gravity(miqu::Gravity::CenterVertical);
    m_right_box->set_divider_spacing(2);
    m_right_box->set_layout_params(miqu::LayoutParams(
        static_cast<int>(miqu::LayoutDimension::WrapContent),
        static_cast<int>(miqu::LayoutDimension::MatchParent),
        miqu::Gravity::Right | miqu::Gravity::CenterVertical
    ));

    if (cfg.show_cpu) {
        m_cpu = std::make_shared<CpuModuleView>();
        m_right_box->add_view(m_cpu);
    }
    if (cfg.show_memory) {
        m_mem = std::make_shared<MemoryModuleView>();
        m_right_box->add_view(m_mem);
    }
    if (cfg.show_network) {
        m_network = std::make_shared<NetworkModuleView>([this]() {
            toggle_network();
        });
        m_right_box->add_view(m_network);
    }
    if (cfg.show_bluetooth) {
        m_bluetooth = std::make_shared<BluetoothModuleView>([this]() {
            toggle_bluetooth();
        });
        m_right_box->add_view(m_bluetooth);
    }
    if (cfg.show_volume) {
        m_volume = std::make_shared<VolumeModuleView>([this]() {
            toggle_sound();
        });
        m_right_box->add_view(m_volume);
    }
    if (cfg.show_brightness) {
        m_brightness = std::make_shared<BrightnessModuleView>([this]() {
            toggle_brightness();
        });
        m_right_box->add_view(m_brightness);
    }
    if (cfg.show_battery) {
        m_battery = std::make_shared<BatteryModuleView>();
        m_right_box->add_view(m_battery);
    }
    if (cfg.show_dark_mode) {
        m_dark_mode = std::make_shared<DarkModeModuleView>();
        m_right_box->add_view(m_dark_mode);
    }
    if (cfg.show_notifications) {
        m_notifications = std::make_shared<NotificationModuleView>();
        m_right_box->add_view(m_notifications);
    }
    if (cfg.show_clock) {
        m_clock = std::make_shared<ClockModuleView>([this]() {
            toggle_calendar();
        });
        m_right_box->add_view(m_clock);
    }
    if (cfg.show_power) {
        m_power = std::make_shared<PowerButtonView>([this]() {
            toggle_power();
        });
        m_right_box->add_view(m_power);
    }
    if (cfg.show_peek) {
        m_peek = std::make_shared<PeekButtonView>();
        m_right_box->add_view(m_peek);
    }
    add_view(m_right_box);
}

BarRootView::~BarRootView() {
    miqu::TooltipManager::get()->dismiss();
    if (m_network_win) m_network_win->close();
    if (m_bluetooth_win) m_bluetooth_win->close();
    if (m_sound_win) m_sound_win->close();
    if (m_brightness_win) m_brightness_win->close();
    if (m_calendar_win) m_calendar_win->close();
    if (m_power_win) m_power_win->close();
}

void BarRootView::sync_theme() {
    const auto& cfg = BarConfig::get();
    set_style(cfg.miqubar_outline ? miqu::CardStyle::Outlined : miqu::CardStyle::Filled);
    set_radius(cfg.bar_corner_radius);
    set_elevation(0);
    set_padding(cfg.padding_left, cfg.padding_top, cfg.padding_right, cfg.padding_bottom);
    if (m_start_btn) m_start_btn->sync_style();
    if (m_search_btn) m_search_btn->sync_style();
    if (m_dark_mode) m_dark_mode->sync_theme_state();
    request_redraw();
}

bool BarRootView::on_mouse_move(int lx, int ly, const miqu::Rect& bounds) {
    m_last_mouse_x = lx;
    m_last_mouse_y = ly;
    return miqu::CardView::on_mouse_move(lx, ly, bounds);
}

bool BarRootView::on_scroll(double delta) {
    if (m_ws_btn && m_ws_btn->is_visible() && m_ws_btn->get_bounds().contains(m_last_mouse_x, m_last_mouse_y)) {
        return m_ws_btn->on_scroll(delta);
    }
    if (m_volume && m_volume->is_visible() && m_volume->get_bounds().contains(m_last_mouse_x, m_last_mouse_y)) {
        return m_volume->on_scroll(delta);
    }
    if (m_brightness && m_brightness->is_visible() && m_brightness->get_bounds().contains(m_last_mouse_x, m_last_mouse_y)) {
        return m_brightness->on_scroll(delta);
    }
    return miqu::CardView::on_scroll(delta);
}

void BarRootView::toggle_network() {
    miqu::TooltipManager::get()->dismiss();
    if (m_network_win) {
        m_network_win->close();
        m_network_win = nullptr;
        request_redraw();
        return;
    }
    if (m_bluetooth_win) { m_bluetooth_win->close(); m_bluetooth_win = nullptr; }
    if (m_sound_win) { m_sound_win->close(); m_sound_win = nullptr; }
    if (m_brightness_win) { m_brightness_win->close(); m_brightness_win = nullptr; }
    if (m_calendar_win) { m_calendar_win->close(); m_calendar_win = nullptr; }
    if (m_power_win) { m_power_win->close(); m_power_win = nullptr; }

    auto content = std::make_shared<NetworkPopupView>([this]() {
        if (m_network_win) {
            m_network_win->close();
            m_network_win = nullptr;
            request_redraw();
        }
    });

    const auto& cfg = BarConfig::get();
    bool is_top = (cfg.position == "top");
    uint32_t anchors = (is_top ? 1 : 2) | 8;
    miqu::Margin margin;
    margin.right = 60 + cfg.margin_right;
    if (is_top) margin.top = cfg.height + 8 + cfg.margin_top;
    else margin.bottom = cfg.height + 8 + cfg.margin_bottom;

    m_network_win = miqu::WindowBuilder::create()
        ->role(miqu::WindowRole::LayerOverlay)
        ->layerNamespace("miqubar-network")
        ->anchors(anchors)
        ->preferredSize(340, 320)
        ->contentSize(340, 320)
        ->contentMargin(margin)
        ->transparent(true)
        ->exclusiveZone(-1)
        ->closeOnClickOutside(true)
        ->closeOnEscape(true)
        ->contentView(content)
        ->onClose([this]() {
            m_network_win = nullptr;
            request_redraw();
        })
        ->build();

    if (m_network_win) m_network_win->show();
}

void BarRootView::toggle_bluetooth() {
    miqu::TooltipManager::get()->dismiss();
    if (m_bluetooth_win) {
        m_bluetooth_win->close();
        m_bluetooth_win = nullptr;
        request_redraw();
        return;
    }
    if (m_network_win) { m_network_win->close(); m_network_win = nullptr; }
    if (m_sound_win) { m_sound_win->close(); m_sound_win = nullptr; }
    if (m_brightness_win) { m_brightness_win->close(); m_brightness_win = nullptr; }
    if (m_calendar_win) { m_calendar_win->close(); m_calendar_win = nullptr; }
    if (m_power_win) { m_power_win->close(); m_power_win = nullptr; }

    auto content = std::make_shared<BluetoothPopupView>([this]() {
        if (m_bluetooth_win) {
            m_bluetooth_win->close();
            m_bluetooth_win = nullptr;
            request_redraw();
        }
    });

    const auto& cfg = BarConfig::get();
    bool is_top = (cfg.position == "top");
    uint32_t anchors = (is_top ? 1 : 2) | 8;
    miqu::Margin margin;
    margin.right = 40 + cfg.margin_right;
    if (is_top) margin.top = cfg.height + 8 + cfg.margin_top;
    else margin.bottom = cfg.height + 8 + cfg.margin_bottom;

    m_bluetooth_win = miqu::WindowBuilder::create()
        ->role(miqu::WindowRole::LayerOverlay)
        ->layerNamespace("miqubar-bluetooth")
        ->anchors(anchors)
        ->preferredSize(340, 300)
        ->contentSize(340, 300)
        ->contentMargin(margin)
        ->transparent(true)
        ->exclusiveZone(-1)
        ->closeOnClickOutside(true)
        ->closeOnEscape(true)
        ->contentView(content)
        ->onClose([this]() {
            m_bluetooth_win = nullptr;
            request_redraw();
        })
        ->build();

    if (m_bluetooth_win) m_bluetooth_win->show();
}

void BarRootView::toggle_sound() {
    miqu::TooltipManager::get()->dismiss();
    if (m_sound_win) {
        m_sound_win->close();
        m_sound_win = nullptr;
        request_redraw();
        return;
    }
    if (m_network_win) { m_network_win->close(); m_network_win = nullptr; }
    if (m_bluetooth_win) { m_bluetooth_win->close(); m_bluetooth_win = nullptr; }
    if (m_brightness_win) { m_brightness_win->close(); m_brightness_win = nullptr; }
    if (m_calendar_win) { m_calendar_win->close(); m_calendar_win = nullptr; }
    if (m_power_win) { m_power_win->close(); m_power_win = nullptr; }

    auto content = std::make_shared<SoundControlPopupView>([this]() {
        if (m_sound_win) {
            m_sound_win->close();
            m_sound_win = nullptr;
            request_redraw();
        }
    });

    const auto& cfg = BarConfig::get();
    bool is_top = (cfg.position == "top");
    uint32_t anchors = (is_top ? 1 : 2) | 8;
    miqu::Margin margin;
    margin.right = 24 + cfg.margin_right;
    if (is_top) margin.top = cfg.height + 8 + cfg.margin_top;
    else margin.bottom = cfg.height + 8 + cfg.margin_bottom;

    m_sound_win = miqu::WindowBuilder::create()
        ->role(miqu::WindowRole::LayerOverlay)
        ->layerNamespace("miqubar-sound")
        ->anchors(anchors)
        ->preferredSize(340, 150)
        ->contentSize(340, 150)
        ->contentMargin(margin)
        ->transparent(true)
        ->exclusiveZone(-1)
        ->closeOnClickOutside(true)
        ->closeOnEscape(true)
        ->contentView(content)
        ->onClose([this]() {
            m_sound_win = nullptr;
            request_redraw();
        })
        ->build();

    if (m_sound_win) m_sound_win->show();
}

void BarRootView::toggle_brightness() {
    miqu::TooltipManager::get()->dismiss();
    if (m_brightness_win) {
        m_brightness_win->close();
        m_brightness_win = nullptr;
        request_redraw();
        return;
    }
    if (m_network_win) { m_network_win->close(); m_network_win = nullptr; }
    if (m_bluetooth_win) { m_bluetooth_win->close(); m_bluetooth_win = nullptr; }
    if (m_sound_win) { m_sound_win->close(); m_sound_win = nullptr; }
    if (m_calendar_win) { m_calendar_win->close(); m_calendar_win = nullptr; }
    if (m_power_win) { m_power_win->close(); m_power_win = nullptr; }

    auto content = std::make_shared<BrightnessControlPopupView>([this]() {
        if (m_brightness_win) {
            m_brightness_win->close();
            m_brightness_win = nullptr;
            request_redraw();
        }
    });

    const auto& cfg = BarConfig::get();
    bool is_top = (cfg.position == "top");
    uint32_t anchors = (is_top ? 1 : 2) | 8;
    miqu::Margin margin;
    margin.right = 24 + cfg.margin_right;
    if (is_top) margin.top = cfg.height + 8 + cfg.margin_top;
    else margin.bottom = cfg.height + 8 + cfg.margin_bottom;

    m_brightness_win = miqu::WindowBuilder::create()
        ->role(miqu::WindowRole::LayerOverlay)
        ->layerNamespace("miqubar-brightness")
        ->anchors(anchors)
        ->preferredSize(340, 150)
        ->contentSize(340, 150)
        ->contentMargin(margin)
        ->transparent(true)
        ->exclusiveZone(-1)
        ->closeOnClickOutside(true)
        ->closeOnEscape(true)
        ->contentView(content)
        ->onClose([this]() {
            m_brightness_win = nullptr;
            request_redraw();
        })
        ->build();

    if (m_brightness_win) m_brightness_win->show();
}

void BarRootView::toggle_quick_settings() {
    toggle_sound();
}

void BarRootView::toggle_calendar() {
    miqu::TooltipManager::get()->dismiss();
    if (m_calendar_win) {
        m_calendar_win->close();
        m_calendar_win = nullptr;
        request_redraw();
        return;
    }
    if (m_network_win) { m_network_win->close(); m_network_win = nullptr; }
    if (m_bluetooth_win) { m_bluetooth_win->close(); m_bluetooth_win = nullptr; }
    if (m_sound_win) { m_sound_win->close(); m_sound_win = nullptr; }
    if (m_brightness_win) { m_brightness_win->close(); m_brightness_win = nullptr; }
    if (m_power_win) { m_power_win->close(); m_power_win = nullptr; }

    auto content = std::make_shared<CalendarPopupView>([this]() {
        if (m_calendar_win) {
            m_calendar_win->close();
            m_calendar_win = nullptr;
            request_redraw();
        }
    });

    const auto& cfg = BarConfig::get();
    bool is_top = (cfg.position == "top");
    uint32_t anchors = (is_top ? 1 : 2) | 8;
    miqu::Margin margin;
    margin.right = 12 + cfg.margin_right;
    if (is_top) {
        margin.top = cfg.height + 8 + cfg.margin_top;
    } else {
        margin.bottom = cfg.height + 8 + cfg.margin_bottom;
    }

    m_calendar_win = miqu::WindowBuilder::create()
        ->role(miqu::WindowRole::LayerOverlay)
        ->layerNamespace("miqubar-calendar")
        ->anchors(anchors)
        ->preferredSize(310, 360)
        ->contentSize(310, 360)
        ->contentMargin(margin)
        ->transparent(true)
        ->exclusiveZone(-1)
        ->closeOnClickOutside(true)
        ->closeOnEscape(true)
        ->contentView(content)
        ->onClose([this]() {
            m_calendar_win = nullptr;
            request_redraw();
        })
        ->build();

    if (m_calendar_win) m_calendar_win->show();
}

void BarRootView::toggle_power() {
    miqu::TooltipManager::get()->dismiss();
    if (m_power_win) {
        m_power_win->close();
        m_power_win = nullptr;
        request_redraw();
        return;
    }
    if (m_network_win) { m_network_win->close(); m_network_win = nullptr; }
    if (m_bluetooth_win) { m_bluetooth_win->close(); m_bluetooth_win = nullptr; }
    if (m_sound_win) { m_sound_win->close(); m_sound_win = nullptr; }
    if (m_brightness_win) { m_brightness_win->close(); m_brightness_win = nullptr; }
    if (m_calendar_win) { m_calendar_win->close(); m_calendar_win = nullptr; }

    auto content = std::make_shared<PowerPopupView>([this]() {
        if (m_power_win) {
            m_power_win->close();
            m_power_win = nullptr;
            request_redraw();
        }
    });

    const auto& cfg = BarConfig::get();
    bool is_top = (cfg.position == "top");
    uint32_t anchors = (is_top ? 1 : 2) | 8;
    miqu::Margin margin;
    margin.right = 8 + (cfg.show_peek ? 10 : 0) + cfg.margin_right;
    if (is_top) {
        margin.top = cfg.height + 8 + cfg.margin_top;
    } else {
        margin.bottom = cfg.height + 8 + cfg.margin_bottom;
    }

    m_power_win = miqu::WindowBuilder::create()
        ->role(miqu::WindowRole::LayerOverlay)
        ->layerNamespace("miqubar-power")
        ->anchors(anchors)
        ->preferredSize(260, 275)
        ->contentSize(260, 275)
        ->contentMargin(margin)
        ->transparent(true)
        ->exclusiveZone(-1)
        ->closeOnClickOutside(true)
        ->closeOnEscape(true)
        ->contentView(content)
        ->onClose([this]() {
            m_power_win = nullptr;
            request_redraw();
        })
        ->build();

    if (m_power_win) m_power_win->show();
}

void BarRootView::update_telemetry() {
    if (m_cpu) m_cpu->update_metrics();
    if (m_mem) m_mem->update_metrics();
    if (m_network) m_network->update_metrics();
    if (m_bluetooth) m_bluetooth->update_metrics();
    if (m_battery) m_battery->update_metrics();
    if (m_volume) m_volume->update_metrics();
    if (m_brightness) m_brightness->update_metrics();
    if (m_notifications) m_notifications->update_metrics();
    request_redraw();
}

void BarRootView::update_clock() {
    if (m_clock) m_clock->update_time();
    request_redraw();
}

// ==========================================
// BarWindow
// ==========================================
BarWindow::BarWindow(miqu::AppEngine* engine) : m_engine(engine) {}

BarWindow::~BarWindow() {
    stop_telemetry_timer();
    if (m_window) m_window->close();
}

bool BarWindow::init() {
    if (!m_engine) return false;

    const auto& cfg = BarConfig::get();
    m_root_view = std::make_shared<BarRootView>();

    uint32_t anchors = 2 | 4 | 8; // Bottom | Left | Right
    if (cfg.position == "top") {
        anchors = 1 | 4 | 8; // Top | Left | Right
    }

    miqu::Margin margin;
    margin.top = cfg.margin_top;
    margin.bottom = cfg.margin_bottom;
    margin.left = cfg.margin_left;
    margin.right = cfg.margin_right;

    int effective_exclusive_zone = 0;
    if (cfg.reserve_space) {
        int v_margin = (cfg.position == "top") ? cfg.margin_top : cfg.margin_bottom;
        effective_exclusive_zone = cfg.height + v_margin;
    }

    m_window = miqu::WindowBuilder::create()
        ->role(miqu::WindowRole::LayerTop)
        ->title("miqubar")
        ->appId("miqubar")
        ->layerNamespace("miqubar")
        ->anchors(anchors)
        ->exclusiveZone(effective_exclusive_zone)
        ->preferredSize(0, cfg.height)
        ->contentMargin(margin)
        ->transparent(true)
        ->keyboardInteractive(false)
        ->closeOnClickOutside(false)
        ->closeOnEscape(false)
        ->contentView(m_root_view)
        ->build();

    if (!m_window) {
        std::cerr << "[miqubar] Failed to create bar window." << std::endl;
        return false;
    }

    m_window->show();
    start_telemetry_timer();
    return true;
}

void BarWindow::request_redraw() {
    if (m_window) m_window->schedule_redraw();
}

void BarWindow::sync_theme() {
    if (m_root_view) m_root_view->sync_theme();
    request_redraw();
}

void BarWindow::start_telemetry_timer() {
    m_running = true;
    m_clock_ticks = 0;
    schedule_next_telemetry();
}

void BarWindow::schedule_next_telemetry() {
    auto* engine = miqu::AppEngine::instance();
    if (!engine || !m_running) return;

    m_telemetry_timer_id = engine->post_delayed(500, [this]() {
        m_telemetry_timer_id = 0;
        if (!m_running || !m_window) return;

        m_clock_ticks += 500;
        if (m_root_view) {
            m_root_view->update_clock();
            const auto& cfg = BarConfig::get();
            if (m_clock_ticks >= cfg.telemetry_interval_ms) {
                m_root_view->update_telemetry();
                m_clock_ticks = 0;
            }
        }
        schedule_next_telemetry();
    });
}

void BarWindow::stop_telemetry_timer() {
    m_running = false;
    auto* engine = miqu::AppEngine::instance();
    if (engine && m_telemetry_timer_id != 0) {
        engine->cancel_delayed(m_telemetry_timer_id);
        m_telemetry_timer_id = 0;
    }
}

} // namespace miqubar
