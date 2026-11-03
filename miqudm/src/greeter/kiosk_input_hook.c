#define _GNU_SOURCE
#define WLR_USE_UNSTABLE 1

#include <dlfcn.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output.h>
#include <wlr/backend/libinput.h>
#include <libinput.h>

static struct wlr_cursor *s_active_cursor = NULL;
static struct wlr_output *s_active_output = NULL;
static struct wlr_input_device *s_touch_devices[16];
static size_t s_touch_device_count = 0;

__attribute__((constructor))
static void init_hook(void) {
    // Prevent child processes (greeter UI and subsequent user sessions) from inheriting LD_PRELOAD
    unsetenv("LD_PRELOAD");
    fprintf(stderr, "[miqudm-kiosk-input] Kiosk input hook loaded in compositor.\n");
}

typedef void (*attach_input_fn)(struct wlr_cursor *, struct wlr_input_device *);
typedef struct wlr_output_layout_output *(*output_layout_add_auto_fn)(struct wlr_output_layout *, struct wlr_output *);
typedef struct wlr_output_layout_output *(*output_layout_add_fn)(struct wlr_output_layout *, struct wlr_output *, int, int);

void wlr_cursor_attach_input_device(struct wlr_cursor *cur, struct wlr_input_device *dev) {
    static attach_input_fn real_attach = NULL;
    if (!real_attach) {
        real_attach = (attach_input_fn)dlsym(RTLD_NEXT, "wlr_cursor_attach_input_device");
    }

    if (cur) {
        s_active_cursor = cur;
    }

    if (real_attach) {
        real_attach(cur, dev);
    }

    if (!dev) return;

    // Track touch devices for output mapping
    if (dev->type == WLR_INPUT_DEVICE_TOUCH || dev->type == WLR_INPUT_DEVICE_TABLET) {
        if (s_touch_device_count < 16) {
            s_touch_devices[s_touch_device_count++] = dev;
        }
        if (cur && s_active_output) {
            wlr_cursor_map_input_to_output(cur, dev, s_active_output);
            fprintf(stderr, "[miqudm-kiosk-input] Mapped touch device '%s' to active output '%s'\n",
                    dev->name ? dev->name : "touch",
                    s_active_output->name ? s_active_output->name : "unnamed");
        }
    }

    // Configure libinput devices (touchpads)
    if (wlr_input_device_is_libinput(dev)) {
        struct libinput_device *handle = wlr_libinput_get_device_handle(dev);
        if (handle) {
            // Enable tap-to-click on all touchpads
            if (libinput_device_config_tap_get_finger_count(handle) > 0) {
                enum libinput_config_status st = libinput_device_config_tap_set_enabled(handle, LIBINPUT_CONFIG_TAP_ENABLED);
                libinput_device_config_tap_set_drag_enabled(handle, LIBINPUT_CONFIG_DRAG_ENABLED);
                fprintf(stderr, "[miqudm-kiosk-input] Enabled tap-to-click on touchpad '%s' (status: %d)\n",
                        dev->name ? dev->name : "touchpad", (int)st);
            }
            // Enable natural scrolling if available
            if (libinput_device_config_scroll_has_natural_scroll(handle)) {
                libinput_device_config_scroll_set_natural_scroll_enabled(handle, 1);
            }
        }
    }
}

struct wlr_output_layout_output *wlr_output_layout_add_auto(struct wlr_output_layout *layout, struct wlr_output *output) {
    static output_layout_add_auto_fn real_add = NULL;
    if (!real_add) {
        real_add = (output_layout_add_auto_fn)dlsym(RTLD_NEXT, "wlr_output_layout_add_auto");
    }

    struct wlr_output_layout_output *res = real_add ? real_add(layout, output) : NULL;

    if (output) {
        s_active_output = output;
        if (s_active_cursor) {
            for (size_t i = 0; i < s_touch_device_count; ++i) {
                if (s_touch_devices[i]) {
                    wlr_cursor_map_input_to_output(s_active_cursor, s_touch_devices[i], output);
                    fprintf(stderr, "[miqudm-kiosk-input] Deferred mapped touch device '%s' to output '%s'\n",
                            s_touch_devices[i]->name ? s_touch_devices[i]->name : "touch",
                            output->name ? output->name : "unnamed");
                }
            }
        }
    }

    return res;
}

struct wlr_output_layout_output *wlr_output_layout_add(struct wlr_output_layout *layout, struct wlr_output *output, int lx, int ly) {
    static output_layout_add_fn real_add = NULL;
    if (!real_add) {
        real_add = (output_layout_add_fn)dlsym(RTLD_NEXT, "wlr_output_layout_add");
    }

    struct wlr_output_layout_output *res = real_add ? real_add(layout, output, lx, ly) : NULL;

    if (output) {
        s_active_output = output;
        if (s_active_cursor) {
            for (size_t i = 0; i < s_touch_device_count; ++i) {
                if (s_touch_devices[i]) {
                    wlr_cursor_map_input_to_output(s_active_cursor, s_touch_devices[i], output);
                }
            }
        }
    }

    return res;
}
