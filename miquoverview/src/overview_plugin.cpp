#include "overview_state.hpp"
#include "overview_config.hpp"
#include "miquland/plugin.hpp"

using namespace miquoverview;

MIQU_PLUGIN_EXPORT miquland::PluginInfo* miqu_plugin_init(miquland::PluginAPI* api) {
    static miquland::PluginInfo s_info = {
        .name = "miquoverview",
        .author = "Miqu Ecosystem Team",
        .description = "GPU-accelerated live workspace expose and overview plugin",
        .version = "1.2.0",
        .abi_version = miquland::MIQU_PLUGIN_ABI_VERSION,
    };

    OverviewState::get().init(api);

    // Command dispatchers
    api->register_dispatcher("overview_toggle", []() {
        OverviewState::get().toggle();
    });

    api->register_dispatcher("overview_open", []() {
        OverviewState::get().open();
    });

    api->register_dispatcher("overview_close", []() {
        OverviewState::get().close();
    });

    // Input hooks
    api->register_pointer_button_hook([](double lx, double ly, uint32_t button, bool pressed) -> bool {
        return OverviewState::get().handle_pointer_button(lx, ly, button, pressed);
    });

    api->register_pointer_motion_hook([](double lx, double ly) -> bool {
        return OverviewState::get().handle_pointer_motion(lx, ly);
    });

    api->register_pointer_axis_hook([](double lx, double ly, double delta_x, double delta_y) -> bool {
        return OverviewState::get().handle_pointer_axis(lx, ly, delta_x, delta_y);
    });

    api->register_key_hook([](uint32_t keysym, uint32_t modifiers, bool pressed) -> bool {
        return OverviewState::get().handle_key(keysym, modifiers, pressed);
    });

    api->register_touchpad_swipe_hook([](const miquland::SwipeEvent& event) -> bool {
        return OverviewState::get().handle_touchpad_swipe(event);
    });

    api->register_touch_gesture_hook([](const miquland::TouchGestureEvent& event) -> bool {
        return OverviewState::get().handle_touch_gesture(event);
    });

    // Lifecycle hook: eliminate dangling pointers if a window crashes mid-overview
    api->register_view_destroy_hook([](miquland::View* view) {
        OverviewState::get().handle_view_destroyed(view);
    });

    return &s_info;
}

MIQU_PLUGIN_EXPORT void miqu_plugin_exit() {
    if (OverviewState::get().is_active()) {
        OverviewState::get().close();
    }
    OverviewConfig::get().cleanup_watcher();
}
