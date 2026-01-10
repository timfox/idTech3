/*
===========================================================================
Vulkan Renderer Stubs

Stub implementations for functions expected by shared renderer code paths
but not directly used in Vulkan renderer, or for optional subsystems that
are work-in-progress.

These stubs allow the renderer to compile and function without requiring
full implementations of all optional features.
===========================================================================
*/

#include "tr_local.h"

// Stub global GL state expected by shared renderer code paths.
// These are OpenGL-specific state structures that are not used in Vulkan
// but are referenced by shared code. Keeping them as stubs for compatibility.
glstatic_t gls = {0};
glstate_t glState = {0};

// Screenshot/video capture helpers - stubbed for now
// Note: Screenshot functionality should be implemented via Vulkan image
// capture and encoding. These stubs prevent link errors but don't provide
// actual functionality. Future implementation should use vk.cmd->command_buffer
// to copy framebuffer to host-visible image and encode to file format.
void RB_TakeScreenshot(int x, int y, int width, int height, const char *fileName) {
    ri.Printf(PRINT_WARNING, "RB_TakeScreenshot: Screenshot capture not yet implemented in Vulkan renderer\n");
    (void)x; (void)y; (void)width; (void)height; (void)fileName;
}

void RB_TakeScreenshotJPEG(int x, int y, int width, int height, const char *fileName) {
    ri.Printf(PRINT_WARNING, "RB_TakeScreenshotJPEG: JPEG screenshot capture not yet implemented in Vulkan renderer\n");
    (void)x; (void)y; (void)width; (void)height; (void)fileName;
}

void RB_TakeScreenshotBMP(int x, int y, int width, int height, const char *fileName, int clipboard) {
    ri.Printf(PRINT_WARNING, "RB_TakeScreenshotBMP: BMP screenshot capture not yet implemented in Vulkan renderer\n");
    (void)x; (void)y; (void)width; (void)height; (void)fileName; (void)clipboard;
}

const void *RB_TakeVideoFrameCmd(const void *data) {
    ri.Printf(PRINT_WARNING, "RB_TakeVideoFrameCmd: Video frame capture not yet implemented in Vulkan renderer\n");
    (void)data;
    return NULL;
}

// Stubs for CVARs that are expected by the renderer but defined in the engine
cvar_t *r_texturebits;
cvar_t *r_defaultImage;
cvar_t *r_ambientScale;
cvar_t *r_lodscale;
cvar_t *r_fbo;
cvar_t *r_hdr;
cvar_t *r_postQuality;
cvar_t *r_textureLodBias;
cvar_t *r_ext_texture_filter_anisotropic;
cvar_t *r_ext_max_anisotropy;
cvar_t *r_cullDistance;
cvar_t *r_nocurves;
cvar_t *r_presentBits;
cvar_t *r_glint_intensity;
cvar_t *r_glint_scale;
cvar_t *r_greyscale;
cvar_t *r_bloom_modulate;
cvar_t *r_bloom_threshold_mode;
cvar_t *r_glint;

// Ray tracing CVARs
cvar_t *r_rt_denoiseSpatialAlpha;
cvar_t *r_rt_denoiseVarianceAlpha;
cvar_t *r_rt_denoiseIterations;
cvar_t *r_rt_denoise;
cvar_t *r_rt_temporal;
cvar_t *r_rt_temporalAlpha;
cvar_t *r_rt_blasCompaction;
cvar_t *r_rt_blasReuse;
cvar_t *r_rt_denoiseMode;
cvar_t *r_rt_outputScale;
cvar_t *r_rt_shadowRays;
cvar_t *r_rt_adaptiveSampling;
cvar_t *r_rt_gi;
cvar_t *r_rt_giBounces;
cvar_t *r_rt_giIntensity;

// ---------------------------------------------------------------------------
// Optional subsystems are compiled from their respective translation units.
// (No stubs needed here.)
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Optional subsystems - implementations in their respective files
// ---------------------------------------------------------------------------
// Note: These subsystems are implemented in:
// - vk_volumetric_fog.c - Volumetric fog system
// - vk_decals.c - Decal system
// - vk_god_rays.c - God rays/light shafts system
// - vk_terrain.c - Terrain rendering system
// - vk_surface_sprites.c - Surface sprites system
//
// The functions are declared here for forward compatibility, but the
// actual implementations are in the respective .c files.

// Forward declarations - implementations in vk_volumetric_fog.c
extern void vk_volumetric_fog_init(void);
extern void vk_volumetric_fog_shutdown(void);
extern void vk_volumetric_fog_update(void);
extern void vk_volumetric_fog_render(VkCommandBuffer cmdBuffer);

// Forward declarations - implementations in vk_decals.c
extern void vk_decals_init(void);
extern void vk_decals_shutdown(void);
extern void vk_decals_update(void);
extern void vk_decals_render(void);

// Forward declarations - implementations in vk_god_rays.c
extern void vk_god_rays_init(void);
extern void vk_god_rays_shutdown(void);
extern void vk_god_rays_update(void);
extern void vk_god_rays_render(VkCommandBuffer cmd_buffer);

// Forward declarations - implementations in vk_terrain.c
extern void vk_terrain_init(void);
extern void vk_terrain_shutdown(void);
extern void vk_terrain_update(void);
extern void vk_terrain_render(void);

// Forward declarations - implementations in vk_surface_sprites.c
extern void vk_surface_sprites_init(void);
extern void vk_surface_sprites_shutdown(void);
extern void vk_surface_sprites_update(void);
extern void vk_surface_sprites_render(void);

// Global timing
vk_gpu_timing_t vk_gpu_timing;

// Utility functions
float ByteToFloat( byte b ) {
    return b / 255.0f;
}

float sRGBtoRGB( float f ) {
    if ( f <= 0.04045f ) {
        return f / 12.92f;
    } else {
        return powf( ( f + 0.055f ) / 1.055f, 2.4f );
    }
}

byte FloatToByte( float f ) {
    return (byte)( f * 255.0f );
}

// System timing function - should be provided by engine
// This stub prevents link errors but doesn't provide real timing
int Sys_Milliseconds( void ) {
    // Note: This should be provided by the engine's system layer
    // For now, return 0 as a stub. Real implementation should query
    // system clock (e.g., clock_gettime on Linux, QueryPerformanceCounter on Windows)
    static int stub_counter = 0;
    return stub_counter++; // Incrementing stub to avoid constant 0
}

// Performance counter reset - should be provided by engine or profiling system
void Perf_ResetFrameCounters( void ) {
    // Note: This should reset frame performance counters if profiling is enabled
    // Stub implementation - real version would reset draw call counts, etc.
}

// Performance draw call counter - should be provided by engine or profiling system
void Perf_CountDrawCall( void ) {
    // Note: This should increment draw call counter for performance profiling
    // Stub implementation - real version would increment profiling counters
}
