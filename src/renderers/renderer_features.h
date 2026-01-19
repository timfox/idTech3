/*
==============================================================================

RENDERER FEATURES CONFIGURATION

Centralized feature toggles and configuration for all renderers.
This prevents "mystery state" by providing clear, consistent controls.

==============================================================================
*/

#ifndef RENDERER_FEATURES_H
#define RENDERER_FEATURES_H

#include "../common/qcommon.h"

//==============================================================================
// FEATURE CATEGORIES
//==============================================================================

typedef enum {
    FEATURE_STABLE,      // Well-tested, production ready
    FEATURE_EXPERIMENTAL,// New features that may have issues
    FEATURE_DEBUG,       // Development/debugging only
    FEATURE_PERFORMANCE  // Performance-sensitive features
} feature_category_t;

typedef enum {
    FEATURE_DISABLED,    // Feature is off
    FEATURE_ENABLED,     // Feature is on
    FEATURE_AUTO         // Automatic based on hardware/capability
} feature_state_t;

//==============================================================================
// FEATURE DEFINITIONS
//==============================================================================

typedef struct renderer_feature_s {
    const char *name;                    // Cvar name
    const char *description;             // Human readable description
    const char *default_value;           // Default value
    feature_category_t category;         // Feature category
    feature_state_t safe_mode_state;     // State in safe mode
    qboolean requires_restart;           // Requires vid_restart
    qboolean hardware_dependent;         // Depends on hardware support
} renderer_feature_t;

//==============================================================================
// COMMON FEATURES (shared across renderers)
//==============================================================================

static const renderer_feature_t common_features[] = {
    // Core rendering features
    {
        "r_vertexLight", "Use vertex lighting instead of per-pixel",
        "0", FEATURE_STABLE, FEATURE_DISABLED, qtrue, qfalse
    },
    {
        "r_vbo", "Use Vertex Buffer Objects for performance",
        "0", FEATURE_STABLE, FEATURE_ENABLED, qtrue, qfalse
    },
    {
        "r_mergeLightmaps", "Merge small lightmaps into atlases",
        "1", FEATURE_STABLE, FEATURE_ENABLED, qtrue, qfalse
    },

    // Quality/performance tradeoffs
    {
        "r_subdivisions", "Distance to subdivide bezier curved surfaces",
        "1", FEATURE_PERFORMANCE, FEATURE_ENABLED, qtrue, qfalse
    },
    {
        "r_lodCurveError", "Level of detail error on curved surface grids",
        "250", FEATURE_PERFORMANCE, FEATURE_ENABLED, qfalse, qfalse
    },
    {
        "r_picmip", "Texture quality reduction (0=full, 1=half, etc)",
        "0", FEATURE_PERFORMANCE, FEATURE_ENABLED, qtrue, qfalse
    },

    // Visual enhancements
    {
        "r_mapGreyScale", "Desaturate world map textures",
        "0", FEATURE_STABLE, FEATURE_DISABLED, qtrue, qfalse
    },
    {
        "r_fullbright", "Render fullbright (no lighting)",
        "0", FEATURE_DEBUG, FEATURE_DISABLED, qfalse, qfalse
    },
    {
        "r_neatsky", "Clean sky rendering",
        "0", FEATURE_STABLE, FEATURE_DISABLED, qtrue, qfalse
    },

    // Debug features
    {
        "r_showtris", "Show triangle outlines",
        "0", FEATURE_DEBUG, FEATURE_DISABLED, qfalse, qfalse
    },
    {
        "r_shownormals", "Show surface normals",
        "0", FEATURE_DEBUG, FEATURE_DISABLED, qfalse, qfalse
    },
    {
        "r_showsky", "Show sky instead of world",
        "0", FEATURE_DEBUG, FEATURE_DISABLED, qfalse, qfalse
    },
};

//==============================================================================
// VULKAN-SPECIFIC FEATURES
//==============================================================================

static const renderer_feature_t vulkan_features[] = {
    // Core Vulkan features
    {
        "r_vulkan_validation", "Enable Vulkan validation layers",
        "0", FEATURE_DEBUG, FEATURE_DISABLED, qtrue, qfalse
    },
    {
        "r_vkDevice", "Vulkan device index (-1 = auto)",
        "-1", FEATURE_STABLE, FEATURE_ENABLED, qtrue, qtrue
    },
    {
        "r_vk_icd", "Vulkan ICD to use",
        "", FEATURE_STABLE, FEATURE_ENABLED, qtrue, qfalse
    },

    // Advanced Vulkan features
    {
        "r_vk_dynamicRendering", "Use Vulkan dynamic rendering",
        "1", FEATURE_STABLE, FEATURE_ENABLED, qtrue, qfalse
    },
    {
        "r_vk_asyncShaderCompile", "Async shader compilation",
        "1", FEATURE_PERFORMANCE, FEATURE_ENABLED, qtrue, qfalse
    },
    {
        "r_vk_bindlessTextures", "Use bindless textures",
        "0", FEATURE_EXPERIMENTAL, FEATURE_DISABLED, qtrue, qtrue
    },

    // RTX features (hardware dependent)
    {
        "r_vk_enableRTX", "Enable RTX ray tracing",
        "1", FEATURE_EXPERIMENTAL, FEATURE_DISABLED, qtrue, qtrue
    },
    {
        "r_vk_raytracing", "Enable ray tracing pipeline",
        "0", FEATURE_EXPERIMENTAL, FEATURE_DISABLED, qtrue, qtrue
    },

    // Variable Rate Shading (VRS)
    {
        "r_vrs", "Enable Variable Rate Shading",
        "0", FEATURE_EXPERIMENTAL, FEATURE_DISABLED, qtrue, qtrue
    },
    {
        "r_vrs_mode", "VRS shading rate pattern",
        "0", FEATURE_EXPERIMENTAL, FEATURE_DISABLED, qfalse, qtrue
    },

    // Debug/Development features
    {
        "r_vk_profiling", "Enable Vulkan profiling",
        "0", FEATURE_DEBUG, FEATURE_DISABLED, qfalse, qfalse
    },
    {
        "r_vk_debug_overlay", "Show Vulkan debug overlay",
        "0", FEATURE_DEBUG, FEATURE_DISABLED, qfalse, qfalse
    },
    {
        "r_vk_renderdoc", "Enable RenderDoc integration",
        "0", FEATURE_DEBUG, FEATURE_DISABLED, qtrue, qfalse
    },
    {
        "r_vk_hotReload", "Hot reload shaders",
        "0", FEATURE_DEBUG, FEATURE_DISABLED, qfalse, qfalse
    },
};

//==============================================================================
// OPENGL-SPECIFIC FEATURES
//==============================================================================

static const renderer_feature_t opengl_features[] = {
    // Core OpenGL features
    {
        "r_overBrightBits", "Overbright rendering bits",
        "1", FEATURE_STABLE, FEATURE_ENABLED, qtrue, qfalse
    },
    {
        "r_mapOverBrightBits", "Map overbright bits",
        "2", FEATURE_STABLE, FEATURE_ENABLED, qtrue, qfalse
    },
    {
        "r_intensity", "Global lighting intensity",
        "1", FEATURE_STABLE, FEATURE_ENABLED, qtrue, qfalse
    },

    // Texture features
    {
        "r_simpleMipMaps", "Use simple mipmapping",
        "1", FEATURE_STABLE, FEATURE_ENABLED, qtrue, qfalse
    },
    {
        "r_roundImagesDown", "Round texture dimensions down",
        "1", FEATURE_PERFORMANCE, FEATURE_ENABLED, qtrue, qfalse
    },
    {
        "r_colorMipLevels", "Color code mip levels",
        "0", FEATURE_DEBUG, FEATURE_DISABLED, qfalse, qfalse
    },
    {
        "r_detailtextures", "Enable detail textures",
        "1", FEATURE_STABLE, FEATURE_ENABLED, qtrue, qfalse
    },

    // Advanced OpenGL features
    {
        "r_ext_multisample", "Multisample anti-aliasing",
        "0", FEATURE_STABLE, FEATURE_DISABLED, qtrue, qtrue
    },
    {
        "r_ext_supersample", "Supersample anti-aliasing",
        "0", FEATURE_PERFORMANCE, FEATURE_DISABLED, qtrue, qtrue
    },
    {
        "r_hdr", "High dynamic range rendering",
        "0", FEATURE_STABLE, FEATURE_DISABLED, qtrue, qfalse
    },
    {
        "r_bloom", "Bloom post-processing effect",
        "0", FEATURE_STABLE, FEATURE_DISABLED, qtrue, qfalse
    },
};

//==============================================================================
// SAFE MODE CONFIGURATION
//==============================================================================

typedef struct safe_mode_config_s {
    qboolean enabled;                    // Is safe mode active?
    const char *reason;                  // Why safe mode was activated
    qboolean disable_experimental;       // Disable experimental features
    qboolean disable_debug;              // Disable debug features
    qboolean prefer_compatibility;       // Prefer compatibility over performance
} safe_mode_config_t;

// Global safe mode state
extern safe_mode_config_t g_safe_mode;

//==============================================================================
// FUNCTION PROTOTYPES
//==============================================================================

// Feature management
void R_InitFeatures(void);
void R_RegisterFeatureCvars(const renderer_feature_t *features, int count);
void R_ApplySafeMode(void);
void R_LogFeatureStatus(void);

// Safe mode management
qboolean R_DetectSafeMode(void);
void R_SetSafeMode(qboolean enable, const char *reason);

// Feature queries
feature_state_t R_GetFeatureState(const char *feature_name);
qboolean R_IsFeatureEnabled(const char *feature_name);
qboolean R_IsExperimentalFeature(const char *feature_name);

// Startup logging
void R_LogRendererInfo(const char *renderer_name, const char *version);
void R_LogFeatureSummary(void);
void R_LogFallbackInfo(const char *requested, const char *fallback, const char *reason);

#endif // RENDERER_FEATURES_H