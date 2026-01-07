/*
===========================================================================
SDL Wayland Implementation

Native Wayland support for the id Tech 3 engine.
Provides Wayland surface creation, input handling, and advanced features.
===========================================================================
*/

// Check if Wayland headers are available at compile time
#ifdef __has_include
#  if __has_include(<wayland-client.h>) && __has_include(<xdg-shell-client-protocol.h>)
#    define WAYLAND_HEADERS_AVAILABLE
#  endif
#endif
// Only include Wayland functionality if both SDL Wayland driver and headers are available
#if defined(SDL_VIDEO_DRIVER_WAYLAND) && defined(WAYLAND_HEADERS_AVAILABLE)

#include "../client/client.h"
#include "../renderers/renderercommon/tr_public.h"
#include "sdl_glw.h"
#include <SDL.h>
#include <SDL_syswm.h>

// Wayland headers
#include <wayland-client.h>
#include <wayland-egl.h>
#include <xdg-shell-client-protocol.h>
#include <xdg-decoration-unstable-v1-client-protocol.h>

// Wayland global objects
static struct wl_display *wayland_display = NULL;
static struct wl_registry *wayland_registry = NULL;
static struct wl_compositor *wayland_compositor = NULL;
static struct wl_surface *wayland_surface = NULL;
static struct wl_egl_window *wayland_egl_window = NULL;

// XDG Shell objects
static struct xdg_wm_base *xdg_wm_base = NULL;
static struct xdg_surface *xdg_surface = NULL;
static struct xdg_toplevel *xdg_toplevel = NULL;

// XDG Decoration objects
static struct zxdg_decoration_manager_v1 *xdg_decoration_manager = NULL;
static struct zxdg_toplevel_decoration_v1 *xdg_decoration = NULL;

// Wayland state
static qboolean wayland_initialized = qfalse;
static qboolean wayland_has_focus = qfalse;
static qboolean wayland_is_maximized = qfalse;
static qboolean wayland_is_fullscreen = qfalse;
static qboolean wayland_is_minimized = qfalse;

// VRR (Variable Refresh Rate) support
static qboolean wayland_vrr_supported = qfalse;
static qboolean wayland_vrr_enabled = qfalse;

// Input handling
static struct wl_seat *wayland_seat = NULL;
static struct wl_pointer *wayland_pointer = NULL;
static struct wl_keyboard *wayland_keyboard = NULL;
static struct wl_touch *wayland_touch = NULL;

// Clipboard
static struct wl_data_device_manager *data_device_manager = NULL;
static struct wl_data_device *data_device = NULL;

// Wayland listeners
static struct wl_registry_listener registry_listener;
static struct xdg_surface_listener xdg_surface_listener;
static struct xdg_toplevel_listener xdg_toplevel_listener;
static struct wl_pointer_listener pointer_listener;
static struct wl_keyboard_listener keyboard_listener;
static struct wl_touch_listener touch_listener;

/*
=================
Wayland Registry Handler
=================
*/
static void wayland_registry_handler(void *data, struct wl_registry *registry,
                                   uint32_t name, const char *interface, uint32_t version) {
    if (strcmp(interface, "wl_compositor") == 0) {
        wayland_compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 1);
    } else if (strcmp(interface, "xdg_wm_base") == 0) {
        xdg_wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
    } else if (strcmp(interface, "zxdg_decoration_manager_v1") == 0) {
        xdg_decoration_manager = wl_registry_bind(registry, name, &zxdg_decoration_manager_v1_interface, 1);
    } else if (strcmp(interface, "wl_seat") == 0) {
        wayland_seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
    } else if (strcmp(interface, "wl_data_device_manager") == 0) {
        data_device_manager = wl_registry_bind(registry, name, &wl_data_device_manager_interface, 1);
    }
}

static void wayland_registry_remover(void *data, struct wl_registry *registry, uint32_t name) {
    // Handle global removal if needed
}

/*
=================
XDG Surface Handlers
=================
*/
static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
    xdg_surface_ack_configure(xdg_surface, serial);

    // Surface is now configured and ready
    wayland_has_focus = qtrue;
}

static void xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel,
                                 int32_t width, int32_t height, struct wl_array *states) {
    // Handle window state changes
    wayland_is_maximized = qfalse;
    wayland_is_fullscreen = qfalse;
    wayland_is_minimized = qfalse;

    uint32_t *state;
    wl_array_for_each(state, states) {
        switch (*state) {
            case XDG_TOPLEVEL_STATE_MAXIMIZED:
                wayland_is_maximized = qtrue;
                break;
            case XDG_TOPLEVEL_STATE_FULLSCREEN:
                wayland_is_fullscreen = qtrue;
                break;
            case XDG_TOPLEVEL_STATE_ACTIVATED:
                wayland_has_focus = qtrue;
                break;
            case XDG_TOPLEVEL_STATE_MINIMIZED:
                wayland_is_minimized = qtrue;
                break;
        }
    }

    // Update window dimensions if provided
    if (width > 0 && height > 0) {
        // Notify the engine of the new dimensions
        // This would typically trigger a vid_restart or similar
        Com_Printf("Wayland: Window configured to %dx%d\n", width, height);
    }
}

static void xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel) {
    // Handle window close request
    Com_Quit_f();
}

/*
=================
Input Handlers
=================
*/
static void pointer_enter(void *data, struct wl_pointer *pointer, uint32_t serial,
                         struct wl_surface *surface, wl_fixed_t x, wl_fixed_t y) {
    // Pointer entered the surface
}

static void pointer_leave(void *data, struct wl_pointer *pointer, uint32_t serial,
                         struct wl_surface *surface) {
    // Pointer left the surface
}

static void pointer_motion(void *data, struct wl_pointer *pointer, uint32_t time,
                          wl_fixed_t x, wl_fixed_t y) {
    // Handle mouse motion
    int mouse_x = wl_fixed_to_int(x);
    int mouse_y = wl_fixed_to_int(y);

    // Convert to engine coordinates and inject as SDL event
    SDL_Event event;
    event.type = SDL_MOUSEMOTION;
    event.motion.x = mouse_x;
    event.motion.y = mouse_y;
    SDL_PushEvent(&event);
}

static void pointer_button(void *data, struct wl_pointer *pointer, uint32_t serial,
                          uint32_t time, uint32_t button, uint32_t state) {
    // Handle mouse button presses
    SDL_Event event;
    event.type = (state == WL_POINTER_BUTTON_STATE_PRESSED) ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
    event.button.button = button;
    event.button.state = state;
    SDL_PushEvent(&event);
}

static void pointer_axis(void *data, struct wl_pointer *pointer, uint32_t time,
                        uint32_t axis, wl_fixed_t value) {
    // Handle mouse wheel
    if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
        SDL_Event event;
        event.type = SDL_MOUSEWHEEL;
        event.wheel.y = -wl_fixed_to_int(value); // Negative for natural scrolling
        SDL_PushEvent(&event);
    }
}

/*
=================
Keyboard Handlers
=================
*/
static void keyboard_keymap(void *data, struct wl_keyboard *keyboard, uint32_t format,
                           int32_t fd, uint32_t size) {
    // Handle keymap changes - for now, rely on SDL for keyboard handling
    close(fd);
}

static void keyboard_enter(void *data, struct wl_keyboard *keyboard, uint32_t serial,
                          struct wl_surface *surface, struct wl_array *keys) {
    // Keyboard focus entered
}

static void keyboard_leave(void *data, struct wl_keyboard *keyboard, uint32_t serial,
                          struct wl_surface *surface) {
    // Keyboard focus left
}

static void keyboard_key(void *data, struct wl_keyboard *keyboard, uint32_t serial,
                        uint32_t time, uint32_t key, uint32_t state) {
    // Convert Wayland key to SDL key and inject
    SDL_Event event;
    event.type = (state == WL_KEYBOARD_KEY_STATE_PRESSED) ? SDL_KEYDOWN : SDL_KEYUP;
    event.key.keysym.sym = (SDL_Keycode)(key + 8); // Wayland key offset
    SDL_PushEvent(&event);
}

static void keyboard_modifiers(void *data, struct wl_keyboard *keyboard, uint32_t serial,
                              uint32_t mods_depressed, uint32_t mods_latched,
                              uint32_t mods_locked, uint32_t group) {
    // Handle modifier key changes
}

/*
=================
Touch Handlers
=================
*/
static void touch_down(void *data, struct wl_touch *touch, uint32_t serial,
                      uint32_t time, struct wl_surface *surface,
                      int32_t id, wl_fixed_t x, wl_fixed_t y) {
    // Handle touch down
    SDL_Event event;
    event.type = SDL_FINGERDOWN;
    event.tfinger.fingerId = id;
    event.tfinger.x = wl_fixed_to_double(x);
    event.tfinger.y = wl_fixed_to_double(y);
    SDL_PushEvent(&event);
}

static void touch_up(void *data, struct wl_touch *touch, uint32_t serial,
                    uint32_t time, int32_t id) {
    // Handle touch up
    SDL_Event event;
    event.type = SDL_FINGERUP;
    event.tfinger.fingerId = id;
    SDL_PushEvent(&event);
}

static void touch_motion(void *data, struct wl_touch *touch, uint32_t time,
                        int32_t id, wl_fixed_t x, wl_fixed_t y) {
    // Handle touch motion
    SDL_Event event;
    event.type = SDL_FINGERMOTION;
    event.tfinger.fingerId = id;
    event.tfinger.x = wl_fixed_to_double(x);
    event.tfinger.y = wl_fixed_to_double(y);
    SDL_PushEvent(&event);
}

/*
=================
GLimp_InitWayland

Initialize Wayland-specific functionality
=================
*/
qboolean GLimp_InitWayland(void) {
    if (wayland_initialized) {
        return qtrue;
    }

    Com_Printf("Initializing Wayland support...\n");

    // Get Wayland display from SDL
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);

    if (!SDL_GetWindowWMInfo(SDL_window, &wmInfo)) {
        Com_Printf(S_COLOR_RED "Failed to get SDL window info for Wayland\n");
        return qfalse;
    }

    if (wmInfo.subsystem != SDL_SYSWM_WAYLAND) {
        Com_Printf(S_COLOR_YELLOW "Not running under Wayland, using SDL fallback\n");
        return qfalse;
    }

    wayland_display = wmInfo.info.wl.display;
    wayland_surface = wmInfo.info.wl.surface;

    if (!wayland_display || !wayland_surface) {
        Com_Printf(S_COLOR_RED "Invalid Wayland display or surface from SDL\n");
        return qfalse;
    }

    // Get registry and bind to protocols
    wayland_registry = wl_display_get_registry(wayland_display);

    registry_listener.global = wayland_registry_handler;
    registry_listener.global_remove = wayland_registry_remover;

    wl_registry_add_listener(wayland_registry, &registry_listener, NULL);
    wl_display_roundtrip(wayland_display);

    if (!wayland_compositor || !xdg_wm_base) {
        Com_Printf(S_COLOR_RED "Required Wayland protocols not available\n");
        return qfalse;
    }

    // Set up XDG shell
    xdg_surface = xdg_wm_base_get_xdg_surface(xdg_wm_base, wayland_surface);
    xdg_toplevel = xdg_surface_get_toplevel(xdg_surface);

    xdg_surface_listener.configure = xdg_surface_configure;
    xdg_toplevel_listener.configure = xdg_toplevel_configure;
    xdg_toplevel_listener.close = xdg_toplevel_close;

    xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, NULL);
    xdg_toplevel_add_listener(xdg_toplevel, &xdg_toplevel_listener, NULL);

    // Set up window decorations if available
    if (xdg_decoration_manager) {
        xdg_decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(
            xdg_decoration_manager, xdg_toplevel);
        if (xdg_decoration) {
            zxdg_toplevel_decoration_v1_set_mode(xdg_decoration,
                ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
        }
    }

    // Set up input handling
    if (wayland_seat) {
        wayland_pointer = wl_seat_get_pointer(wayland_seat);
        wayland_keyboard = wl_seat_get_keyboard(wayland_seat);
        wayland_touch = wl_seat_get_touch(wayland_seat);

        pointer_listener.enter = pointer_enter;
        pointer_listener.leave = pointer_leave;
        pointer_listener.motion = pointer_motion;
        pointer_listener.button = pointer_button;
        pointer_listener.axis = pointer_axis;

        keyboard_listener.keymap = keyboard_keymap;
        keyboard_listener.enter = keyboard_enter;
        keyboard_listener.leave = keyboard_leave;
        keyboard_listener.key = keyboard_key;
        keyboard_listener.modifiers = keyboard_modifiers;

        touch_listener.down = touch_down;
        touch_listener.up = touch_up;
        touch_listener.motion = touch_motion;

        if (wayland_pointer) {
            wl_pointer_add_listener(wayland_pointer, &pointer_listener, NULL);
        }
        if (wayland_keyboard) {
            wl_keyboard_add_listener(wayland_keyboard, &keyboard_listener, NULL);
        }
        if (wayland_touch) {
            wl_touch_add_listener(wayland_touch, &touch_listener, NULL);
        }
    }

    // Commit the surface
    wl_surface_commit(wayland_surface);
    wl_display_roundtrip(wayland_display);

    wayland_initialized = qtrue;
    Com_Printf("Wayland support initialized successfully\n");

    return qtrue;
}

/*
=================
GLimp_CreateWaylandWindow

Create a Wayland window with specified dimensions
=================
*/
void GLimp_CreateWaylandWindow(int width, int height, qboolean fullscreen) {
    if (!wayland_initialized) {
        Com_Printf(S_COLOR_RED "Wayland not initialized\n");
        return;
    }

    // Set window title
    xdg_toplevel_set_title(xdg_toplevel, "id Tech 3");

    // Set app ID for window management
    xdg_toplevel_set_app_id(xdg_toplevel, "io.github.idtech3");

    if (fullscreen) {
        xdg_toplevel_set_fullscreen(xdg_toplevel, NULL);
        wayland_is_fullscreen = qtrue;
    } else {
        xdg_toplevel_unset_fullscreen(xdg_toplevel);
        wayland_is_fullscreen = qfalse;
    }

    // Commit changes
    wl_surface_commit(wayland_surface);
    wl_display_roundtrip(wayland_display);

    Com_Printf("Wayland window created: %dx%d (%s)\n", width, height,
               fullscreen ? "fullscreen" : "windowed");
}

/*
=================
GLimp_HandleWaylandEvents

Process Wayland events
=================
*/
void GLimp_HandleWaylandEvents(void) {
    if (!wayland_initialized || !wayland_display) {
        return;
    }

    // Process Wayland events
    while (wl_display_prepare_read(wayland_display) != 0) {
        wl_display_dispatch_pending(wayland_display);
    }

    if (wl_display_flush(wayland_display) < 0) {
        // Handle display connection lost
        Com_Printf(S_COLOR_RED "Wayland display connection lost\n");
        wayland_initialized = qfalse;
    }
}

/*
=================
Wayland utility functions
=================
*/
qboolean GLimp_Wayland_IsInitialized(void) {
    return wayland_initialized;
}

qboolean GLimp_Wayland_HasFocus(void) {
    return wayland_has_focus;
}

qboolean GLimp_Wayland_IsMaximized(void) {
    return wayland_is_maximized;
}

qboolean GLimp_Wayland_IsFullscreen(void) {
    return wayland_is_fullscreen;
}

qboolean GLimp_Wayland_IsMinimized(void) {
    return wayland_is_minimized;
}

qboolean GLimp_Wayland_VRRSupported(void) {
    return wayland_vrr_supported;
}

qboolean GLimp_Wayland_VRREnabled(void) {
    return wayland_vrr_enabled;
}

void GLimp_Wayland_SetVRREnabled(qboolean enabled) {
    if (wayland_vrr_supported) {
        wayland_vrr_enabled = enabled;
        Com_Printf("Wayland VRR %s\n", enabled ? "enabled" : "disabled");
    }
}

#else // SDL_VIDEO_DRIVER_WAYLAND && WAYLAND_HEADERS_AVAILABLE

// Stub implementations when Wayland is not available
#include "../client/client.h"
#include "../renderers/renderercommon/tr_public.h"
#include "sdl_glw.h"

// Function prototypes for stub implementations
static qboolean GLimp_Wayland_Init(void);
static void GLimp_Wayland_Shutdown(void);
static qboolean GLimp_Wayland_CreateWindow(int width, int height);
static void GLimp_Wayland_DestroyWindow(void);
static void GLimp_Wayland_SetWindowSize(int width, int height);
static void GLimp_Wayland_SetWindowTitle(const char *title);
static void GLimp_Wayland_PumpEvents(void);
static qboolean GLimp_Wayland_IsVariableRefreshRateSupported(void);
static void GLimp_Wayland_SetVariableRefreshRate(qboolean enabled);
static qboolean GLimp_Wayland_IsMaximized(void);
static qboolean GLimp_Wayland_IsFullscreen(void);
static qboolean GLimp_Wayland_IsMinimized(void);
static qboolean GLimp_Wayland_VRRSupported(void);
static qboolean GLimp_Wayland_VRREnabled(void);
static void GLimp_Wayland_SetVRREnabled(qboolean enabled);

__attribute__((unused)) qboolean GLimp_Wayland_Init(void) { return qfalse; }
__attribute__((unused)) void GLimp_Wayland_Shutdown(void) {}
__attribute__((unused)) qboolean GLimp_Wayland_CreateWindow(int width, int height) { (void)width; (void)height; return qfalse; }
__attribute__((unused)) void GLimp_Wayland_DestroyWindow(void) {}
__attribute__((unused)) void GLimp_Wayland_SetWindowSize(int width, int height) { (void)width; (void)height; }
__attribute__((unused)) void GLimp_Wayland_SetWindowTitle(const char *title) { (void)title; }
__attribute__((unused)) void GLimp_Wayland_PumpEvents(void) {}
__attribute__((unused)) qboolean GLimp_Wayland_IsVariableRefreshRateSupported(void) { return qfalse; }
__attribute__((unused)) void GLimp_Wayland_SetVariableRefreshRate(qboolean enabled) { (void)enabled; }
__attribute__((unused)) qboolean GLimp_Wayland_IsMaximized(void) { return qfalse; }
__attribute__((unused)) qboolean GLimp_Wayland_IsFullscreen(void) { return qfalse; }
__attribute__((unused)) qboolean GLimp_Wayland_IsMinimized(void) { return qfalse; }
__attribute__((unused)) qboolean GLimp_Wayland_VRRSupported(void) { return qfalse; }
__attribute__((unused)) qboolean GLimp_Wayland_VRREnabled(void) { return qfalse; }
__attribute__((unused)) void GLimp_Wayland_SetVRREnabled(qboolean enabled) { (void)enabled; }

#endif // SDL_VIDEO_DRIVER_WAYLAND && WAYLAND_HEADERS_AVAILABLE