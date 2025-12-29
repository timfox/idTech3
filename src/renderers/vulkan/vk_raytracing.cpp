/*
=============================================================================
Vulkan Ray Tracing Implementation - Main Renderer Interface

Stub implementation that delegates to RTX renderer for full functionality.
=============================================================================
*/

#include "tr_local.h"

// Forward declarations for RTX renderer functions
extern void RTX_vk_rt_init(void);
extern void RTX_vk_rt_shutdown(void);
extern void RTX_vk_rt_trace_rays(uint32_t width, uint32_t height);

extern "C" void vk_rt_init(void) {
    // Initialize ray tracing in RTX renderer if available
    // For now, this is a stub - full implementation is in RTX renderer
    ri.Printf(PRINT_ALL, "Vulkan ray tracing initialized (stub)\n");
}

extern "C" void vk_rt_shutdown(void) {
    // Shutdown ray tracing in RTX renderer if available
    // For now, this is a stub - full implementation is in RTX renderer
    ri.Printf(PRINT_ALL, "Vulkan ray tracing shutdown (stub)\n");
}

extern "C" void vk_rt_trace_rays(uint32_t width, uint32_t height) {
    // Perform ray tracing using RTX renderer if available
    // For now, this is a stub - full implementation is in RTX renderer
    ri.Printf(PRINT_DEVELOPER, "Ray tracing %dx%d (stub)\n", width, height);
}