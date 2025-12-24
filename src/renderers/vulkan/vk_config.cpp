#include "vk_config.h"
#include "vk_config_private.h"
#include "../renderercommon/tr_public.h"

// Renderer interface
extern refimport_t ri;

// CVAR extern declarations for Vulkan renderer
cvar_t *r_vrs = NULL;
cvar_t *r_vrs_mode = NULL;
cvar_t *r_vrs_center_radius = NULL;
cvar_t *r_vrs_falloff_start = NULL;
cvar_t *r_vrs_min_rate = NULL;
cvar_t *r_vrs_max_rate = NULL;
cvar_t *r_vk_profiling = NULL;
cvar_t *r_vk_debug_overlay = NULL;
cvar_t *r_vk_disableScreenMap = NULL;
cvar_t *r_procDressing = NULL;
cvar_t *r_materialSystem = NULL;
cvar_t *r_frameTelemetry = NULL;
cvar_t *r_bloom = NULL;
cvar_t *r_dlss = NULL;
cvar_t *r_dlss_quality = NULL;
cvar_t *r_dlss_sharpening = NULL;
cvar_t *r_styleTransfer = NULL;
cvar_t *r_styleStrength = NULL;
cvar_t *r_styleLevels = NULL;
cvar_t *r_styleEdge = NULL;
cvar_t *r_postprocess_workgroup = NULL;
cvar_t *r_postprocess_compute = NULL;
cvar_t *r_postQuality = NULL;
cvar_t *r_hdr = NULL;
cvar_t *r_tonemapMode = NULL;
cvar_t *r_tonemapExposure = NULL;
cvar_t *r_gamma = NULL;
cvar_t *r_greyscale = NULL;
cvar_t *r_dither = NULL;
cvar_t *r_vk_hotReload = NULL;

// Initialize Vulkan CVARs
void vk_config_init(void) {
    ri.Printf(PRINT_ALL, "Vulkan: Initializing configuration variables\n");

    // VRS (Variable Rate Shading) CVARs
    r_vrs = ri.Cvar_Get("r_vrs", "0", CVAR_ARCHIVE | CVAR_LATCH);
    ri.Cvar_SetDescription(r_vrs, "Enable Variable Rate Shading for performance optimization");

    r_vrs_mode = ri.Cvar_Get("r_vrs_mode", "0", CVAR_ARCHIVE | CVAR_LATCH);
    ri.Cvar_SetDescription(r_vrs_mode, "VRS shading rate mode (0=per-draw, 1=per-primitive, 2=per-vertex)");

    r_vrs_center_radius = ri.Cvar_Get("r_vrs_center_radius", "0.5", CVAR_ARCHIVE);
    ri.Cvar_SetDescription(r_vrs_center_radius, "VRS center region radius (0.0-1.0)");

    r_vrs_falloff_start = ri.Cvar_Get("r_vrs_falloff_start", "0.3", CVAR_ARCHIVE);
    ri.Cvar_SetDescription(r_vrs_falloff_start, "VRS falloff start distance (0.0-1.0)");

    r_vrs_min_rate = ri.Cvar_Get("r_vrs_min_rate", "1", CVAR_ARCHIVE);
    ri.Cvar_SetDescription(r_vrs_min_rate, "Minimum VRS shading rate (1, 2, 4, 8, 16)");

    r_vrs_max_rate = ri.Cvar_Get("r_vrs_max_rate", "16", CVAR_ARCHIVE);
    ri.Cvar_SetDescription(r_vrs_max_rate, "Maximum VRS shading rate (1, 2, 4, 8, 16)");

    // Performance and debugging CVARs
    r_vk_profiling = ri.Cvar_Get("r_vk_profiling", "0", CVAR_ARCHIVE);
    ri.Cvar_SetDescription(r_vk_profiling, "Enable Vulkan performance profiling and statistics");

    r_vk_debug_overlay = ri.Cvar_Get("r_vk_debug_overlay", "0", CVAR_ARCHIVE);
    ri.Cvar_SetDescription(r_vk_debug_overlay, "Show Vulkan debug overlay with performance metrics");

    // Rendering features
    r_vk_disableScreenMap = ri.Cvar_Get("r_vk_disableScreenMap", "0", CVAR_ARCHIVE | CVAR_LATCH);
    ri.Cvar_SetDescription(r_vk_disableScreenMap, "Disable screen map rendering for debugging");

    r_procDressing = ri.Cvar_Get("r_procDressing", "1", CVAR_ARCHIVE | CVAR_LATCH);
    ri.Cvar_SetDescription(r_procDressing, "Enable procedural dressing (decals, etc.)");

    r_materialSystem = ri.Cvar_Get("r_materialSystem", "1", CVAR_ARCHIVE | CVAR_LATCH);
    ri.Cvar_SetDescription(r_materialSystem, "Enable advanced material system");

    r_frameTelemetry = ri.Cvar_Get("r_frameTelemetry", "0", CVAR_ARCHIVE);
    ri.Cvar_SetDescription(r_frameTelemetry, "Enable frame telemetry for performance analysis");

    // Post-processing CVARs
    r_bloom = ri.Cvar_Get("r_bloom", "1", CVAR_ARCHIVE);
    ri.Cvar_SetDescription(r_bloom, "Enable bloom post-processing effect");

    r_dlss = ri.Cvar_Get("r_dlss", "0", CVAR_ARCHIVE | CVAR_LATCH);
    ri.Cvar_SetDescription(r_dlss, "Enable NVIDIA DLSS upscaling");

    r_dlss_quality = ri.Cvar_Get("r_dlss_quality", "3", CVAR_ARCHIVE);
    ri.Cvar_SetDescription(r_dlss_quality, "DLSS quality preset (1=Ultra Performance, 2=Performance, 3=Balanced, 4=Quality, 5=Ultra Quality)");

    r_dlss_sharpening = ri.Cvar_Get("r_dlss_sharpening", "0.5", CVAR_ARCHIVE);
    ri.Cvar_SetDescription(r_dlss_sharpening, "DLSS sharpening strength (0.0-1.0)");

    // Style transfer CVARs
    r_styleTransfer = ri.Cvar_Get("r_styleTransfer", "0", CVAR_ARCHIVE | CVAR_LATCH);
    ri.Cvar_SetDescription(r_styleTransfer, "Enable neural style transfer post-processing");

    r_styleStrength = ri.Cvar_Get("r_styleStrength", "1.0", CVAR_ARCHIVE);
    ri.Cvar_SetDescription(r_styleStrength, "Style transfer strength (0.0-2.0)");

    r_styleLevels = ri.Cvar_Get("r_styleLevels", "8", CVAR_ARCHIVE);
    ri.Cvar_SetDescription(r_styleLevels, "Style transfer detail levels");

    r_styleEdge = ri.Cvar_Get("r_styleEdge", "1.0", CVAR_ARCHIVE);
    ri.Cvar_SetDescription(r_styleEdge, "Style transfer edge enhancement");

    // Compute shader workgroup sizes
    r_postprocess_workgroup = ri.Cvar_Get("r_postprocess_workgroup", "8", CVAR_ARCHIVE | CVAR_LATCH);
    ri.Cvar_SetDescription(r_postprocess_workgroup, "Post-processing compute shader workgroup size");

    r_postprocess_compute = ri.Cvar_Get("r_postprocess_compute", "1", CVAR_ARCHIVE | CVAR_LATCH);
    ri.Cvar_SetDescription(r_postprocess_compute, "Use compute shaders for post-processing");

    // HDR and tonemapping
    r_postQuality = ri.Cvar_Get("r_postQuality", "2", CVAR_ARCHIVE);
    ri.Cvar_SetDescription(r_postQuality, "Post-processing quality level (0=off, 1=low, 2=medium, 3=high, 4=ultra)");

    r_hdr = ri.Cvar_Get("r_hdr", "1", CVAR_ARCHIVE | CVAR_LATCH);
    ri.Cvar_SetDescription(r_hdr, "Enable HDR rendering");

    r_tonemapMode = ri.Cvar_Get("r_tonemapMode", "1", CVAR_ARCHIVE);
    ri.Cvar_SetDescription(r_tonemapMode, "HDR tonemapping mode (0=off, 1=ACES, 2=Reinhard, 3=Uncharted2, 4=Filmic)");

    r_tonemapExposure = ri.Cvar_Get("r_tonemapExposure", "1.0", CVAR_ARCHIVE);
    ri.Cvar_SetDescription(r_tonemapExposure, "Tonemapping exposure adjustment");

    // Color correction
    r_gamma = ri.Cvar_Get("r_gamma", "1.0", CVAR_ARCHIVE);
    ri.Cvar_SetDescription(r_gamma, "Gamma correction value");

    r_greyscale = ri.Cvar_Get("r_greyscale", "0", CVAR_ARCHIVE);
    ri.Cvar_SetDescription(r_greyscale, "Convert to greyscale (0=off, 1=luminance, 2=average, 3=max, 4=min)");

    r_dither = ri.Cvar_Get("r_dither", "1", CVAR_ARCHIVE);
    ri.Cvar_SetDescription(r_dither, "Enable dithering for banding reduction");

    // Development features
    r_vk_hotReload = ri.Cvar_Get("r_vk_hotReload", "0", CVAR_ARCHIVE | CVAR_LATCH);
    ri.Cvar_SetDescription(r_vk_hotReload, "Enable shader hot reload - automatically reload shaders when files change");
}

// Shutdown Vulkan CVARs
void vk_config_shutdown(void) {
    // CVARs are automatically managed by the engine, no explicit cleanup needed
    ri.Printf(PRINT_ALL, "Vulkan: Configuration variables shut down\n");
}

// Validate configuration values
qboolean vk_config_validate(void) {
    qboolean valid = qtrue;

    // Validate VRS settings
    if (r_vrs_min_rate && r_vrs_min_rate->integer <= 0) {
        ri.Printf(PRINT_WARNING, "Vulkan: r_vrs_min_rate must be > 0, resetting to 1\n");
        ri.Cvar_Set("r_vrs_min_rate", "1");
        valid = qfalse;
    }

    if (r_vrs_max_rate && r_vrs_max_rate->integer <= 0) {
        ri.Printf(PRINT_WARNING, "Vulkan: r_vrs_max_rate must be > 0, resetting to 16\n");
        ri.Cvar_Set("r_vrs_max_rate", "16");
        valid = qfalse;
    }

    if (r_vrs_min_rate && r_vrs_max_rate && r_vrs_min_rate->integer > r_vrs_max_rate->integer) {
        ri.Printf(PRINT_WARNING, "Vulkan: r_vrs_min_rate > r_vrs_max_rate, swapping values\n");
        int temp = r_vrs_min_rate->integer;
        ri.Cvar_Set("r_vrs_min_rate", va("%d", r_vrs_max_rate->integer));
        ri.Cvar_Set("r_vrs_max_rate", va("%d", temp));
        valid = qfalse;
    }

    // Validate DLSS quality
    if (r_dlss_quality && (r_dlss_quality->integer < 1 || r_dlss_quality->integer > 5)) {
        ri.Printf(PRINT_WARNING, "Vulkan: r_dlss_quality out of range (1-5), resetting to 3\n");
        ri.Cvar_Set("r_dlss_quality", "3");
        valid = qfalse;
    }

    // Validate post-processing quality
    if (r_postQuality && (r_postQuality->integer < 0 || r_postQuality->integer > 4)) {
        ri.Printf(PRINT_WARNING, "Vulkan: r_postQuality out of range (0-4), resetting to 2\n");
        ri.Cvar_Set("r_postQuality", "2");
        valid = qfalse;
    }

    return valid;
}

// Check if advanced features are enabled
qboolean vk_config_has_advanced_features(void) {
    return ((r_vrs && r_vrs->integer) ||
           (r_dlss && r_dlss->integer) ||
           (r_styleTransfer && r_styleTransfer->integer) ||
           (r_hdr && r_hdr->integer)) ? qtrue : qfalse;
}
