extern "C" {
#include "vk_config.h"
#include "vk_config_private.h"
#include "../renderercommon/tr_public.h"
}

// Renderer interface
extern refimport_t ri;

// CVAR extern declarations - these are defined in tr_init.c
extern cvar_t *r_vrs;
extern cvar_t *r_vrs_mode;
extern cvar_t *r_vrs_center_radius;
extern cvar_t *r_vrs_falloff_start;
extern cvar_t *r_vrs_min_rate;
extern cvar_t *r_vrs_max_rate;
extern cvar_t *r_vk_profiling;
extern cvar_t *r_vk_debug_overlay;
extern cvar_t *r_vk_disableScreenMap;
extern cvar_t *r_procDressing;
extern cvar_t *r_materialSystem;
extern cvar_t *r_frameTelemetry;
extern cvar_t *r_bloom;
extern cvar_t *r_dlss;
extern cvar_t *r_dlss_quality;
extern cvar_t *r_dlss_sharpening;
extern cvar_t *r_styleTransfer;
extern cvar_t *r_styleStrength;
extern cvar_t *r_styleLevels;
extern cvar_t *r_styleEdge;
extern cvar_t *r_postprocess_workgroup;
extern cvar_t *r_postprocess_compute;
extern cvar_t *r_postQuality;
extern cvar_t *r_hdr;
extern cvar_t *r_tonemapMode;
extern cvar_t *r_tonemapExposure;
extern cvar_t *r_gamma;
extern cvar_t *r_greyscale;
extern cvar_t *r_dither;
extern cvar_t *r_vk_hotReload;

// Initialize Vulkan CVARs
// Note: CVars are registered in tr_init.c, this function is for any Vulkan-specific initialization
extern "C" void vk_config_init(void) {
    ri.Printf(PRINT_ALL, "Vulkan: Configuration initialized\n");
}

// Shutdown Vulkan CVARs
extern "C" void vk_config_shutdown(void) {
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
