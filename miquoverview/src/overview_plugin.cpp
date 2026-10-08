#include "overview_state.hpp"
#include "miquland/plugin.hpp"

using namespace miquoverview;

MIQU_PLUGIN_EXPORT miquland::PluginInfo* miqu_plugin_init(miquland::PluginAPI* api) {
    static miquland::PluginInfo s_info = {
        .name = "miquoverview",
        .author = "Miqu Ecosystem Team",
        .description = "GPU-accelerated live workspace expose and overview plugin",
        .version = "1.1.0",
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

    api->register_key_hook([](uint32_t keysym, uint32_t modifiers, bool pressed) -> bool {
        return OverviewState::get().handle_key(keysym, modifiers, pressed);
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
}
