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
    add_view(m_right_box);
}

BarRootView::~BarRootView() {
    miqu::TooltipManager::get()->dismiss();
    if (m_popup) m_popup->dismiss();
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

bool BarRootView::on_scroll(double) {
    // Zero scroll response across miqubar to prevent accidental touch/trackpad triggers
    return false;
}

void BarRootView::show_flyout(PopupType type, std::shared_ptr<miqu::View> anchor, std::shared_ptr<miqu::View> content, int width, int height) {
    miqu::TooltipManager::get()->dismiss();
    if (m_popup && m_popup->is_showing()) {
        bool was_same = (m_active_popup_type == type);
        m_popup->dismiss();
        m_popup = nullptr;
        m_active_popup_type = PopupType::None;
        request_redraw();
        if (was_same) return;
    }

    std::string ns_suffix = "flyout";
    switch (type) {
        case PopupType::Network: ns_suffix = "network"; break;
        case PopupType::Bluetooth: ns_suffix = "bluetooth"; break;
        case PopupType::Sound: ns_suffix = "sound"; break;
        case PopupType::Brightness: ns_suffix = "brightness"; break;
        case PopupType::Calendar: ns_suffix = "calendar"; break;
        case PopupType::Power: ns_suffix = "power"; break;
        default: break;
    }

    m_active_popup_type = type;
    m_popup = miqu::PopupWindowBuilder::create()
        ->content(content)
        ->size(width, height)
        ->layerNamespace("miqubar-" + ns_suffix)
        ->onDismiss([this]() {
            m_popup = nullptr;
            m_active_popup_type = PopupType::None;
            request_redraw();
        })
        ->build();

    m_popup->show_as_dropdown(anchor ? anchor.get() : this, miqu::PopupGravity::BottomEnd);
}

void BarRootView::toggle_network() {
    auto content = std::make_shared<NetworkPopupView>([this]() {
        if (m_popup) m_popup->dismiss();
    });
    show_flyout(PopupType::Network, m_network, content, 340, 320);
}

void BarRootView::toggle_bluetooth() {
    auto content = std::make_shared<BluetoothPopupView>([this]() {
        if (m_popup) m_popup->dismiss();
    });
    show_flyout(PopupType::Bluetooth, m_bluetooth, content, 340, 340);
}

void BarRootView::toggle_sound() {
    auto content = std::make_shared<SoundControlPopupView>([this]() {
        if (m_popup) m_popup->dismiss();
    });
    show_flyout(PopupType::Sound, m_volume, content, 340, 150);
}

void BarRootView::toggle_brightness() {
    auto content = std::make_shared<BrightnessControlPopupView>([this]() {
        if (m_popup) m_popup->dismiss();
    });
    show_flyout(PopupType::Brightness, m_brightness, content, 340, 150);
}

void BarRootView::toggle_quick_settings() {
    toggle_sound();
}

void BarRootView::toggle_calendar() {
    auto content = std::make_shared<CalendarPopupView>([this]() {
        if (m_popup) m_popup->dismiss();
    });
    show_flyout(PopupType::Calendar, m_clock, content, 310, 360);
}

void BarRootView::toggle_power() {
    auto content = std::make_shared<PowerPopupView>([this]() {
        if (m_popup) m_popup->dismiss();
    });
    show_flyout(PopupType::Power, m_power, content, 260, 275);
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
