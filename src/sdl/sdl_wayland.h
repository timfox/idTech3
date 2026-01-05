/*
===========================================================================
SDL Wayland Header

Native Wayland support declarations for the id Tech 3 engine.
===========================================================================
*/

#ifndef __SDL_WAYLAND_H__
#define __SDL_WAYLAND_H__

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

#endif // __SDL_WAYLAND_H__