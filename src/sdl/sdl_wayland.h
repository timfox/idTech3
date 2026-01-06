/*
===========================================================================
SDL Wayland Header

Native Wayland support declarations for the id Tech 3 engine.
===========================================================================
*/

#ifndef __SDL_WAYLAND_H__
#define __SDL_WAYLAND_H__

// Check if Wayland headers are available at compile time
#ifdef __has_include
#  if __has_include(<wayland-client.h>) && __has_include(<xdg-shell-client-protocol.h>)
#    define WAYLAND_HEADERS_AVAILABLE
#  endif
#endif

// Only declare Wayland functions if both SDL Wayland driver and headers are available
#if defined(SDL_VIDEO_DRIVER_WAYLAND) && defined(WAYLAND_HEADERS_AVAILABLE)

// Wayland initialization and window management
qboolean GLimp_InitWayland(void);
void GLimp_CreateWaylandWindow(int width, int height, qboolean fullscreen);
void GLimp_HandleWaylandEvents(void);

// Wayland state queries
qboolean GLimp_Wayland_IsInitialized(void);
qboolean GLimp_Wayland_HasFocus(void);
qboolean GLimp_Wayland_IsMaximized(void);
qboolean GLimp_Wayland_IsFullscreen(void);
qboolean GLimp_Wayland_IsMinimized(void);

// VRR (Variable Refresh Rate) support
qboolean GLimp_Wayland_VRRSupported(void);
qboolean GLimp_Wayland_VRREnabled(void);
void GLimp_Wayland_SetVRREnabled(qboolean enabled);

#else // SDL_VIDEO_DRIVER_WAYLAND && WAYLAND_HEADERS_AVAILABLE

// Stub declarations when Wayland is not available
static inline qboolean GLimp_InitWayland(void) { return qfalse; }
static inline void GLimp_CreateWaylandWindow(int width, int height, qboolean fullscreen) { (void)width; (void)height; (void)fullscreen; }
static inline void GLimp_HandleWaylandEvents(void) {}
static inline qboolean GLimp_Wayland_IsInitialized(void) { return qfalse; }
static inline qboolean GLimp_Wayland_HasFocus(void) { return qfalse; }
static inline qboolean GLimp_Wayland_IsMaximized(void) { return qfalse; }
static inline qboolean GLimp_Wayland_IsFullscreen(void) { return qfalse; }
static inline qboolean GLimp_Wayland_IsMinimized(void) { return qfalse; }
static inline qboolean GLimp_Wayland_VRRSupported(void) { return qfalse; }
static inline qboolean GLimp_Wayland_VRREnabled(void) { return qfalse; }
static inline void GLimp_Wayland_SetVRREnabled(qboolean enabled) { (void)enabled; }

#endif // SDL_VIDEO_DRIVER_WAYLAND && WAYLAND_HEADERS_AVAILABLE

#endif // __SDL_WAYLAND_H__