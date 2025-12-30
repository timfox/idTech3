/*
=============================================================================
Unified Renderer Interface

This header defines a unified interface for all modern renderers (OpenGL2, Vulkan, Metal)
to reduce maintenance burden and ensure feature parity across backends.
=============================================================================
*/

#ifndef __TR_RENDERER_H
#define __TR_RENDERER_H

#include "../renderers/renderercommon/tr_types.h"

// Forward declarations
typedef struct dlight_s dlight_t;
#include "../renderers/renderercommon/tr_public.h"

// Renderer capabilities and feature flags
typedef enum {
    RENDERER_FEATURE_MULTISAMPLE = (1 << 0),
    RENDERER_FEATURE_ANISOTROPY = (1 << 1),
    RENDERER_FEATURE_SHADER_CACHE = (1 << 2),
    RENDERER_FEATURE_COMPUTE_SHADERS = (1 << 3),
    RENDERER_FEATURE_RAYTRACING = (1 << 4),
    RENDERER_FEATURE_BINDLESS_TEXTURES = (1 << 5),
    RENDERER_FEATURE_SHADER_HOTRELOAD = (1 << 6),
    RENDERER_FEATURE_PERFORMANCE_HUD = (1 << 7),
    RENDERER_FEATURE_ADVANCED_MATERIALS = (1 << 8),
    RENDERER_FEATURE_PARTICLE_SYSTEMS = (1 << 9),
    RENDERER_FEATURE_POST_PROCESSING = (1 << 10),
} rendererFeature_t;

// Unified renderer interface
typedef struct rendererInterface_s {
    // Basic identification
    const char* name;
    const char* description;
    uint32_t version;
    uint32_t features; // Bitfield of rendererFeature_t

    // Core rendering functions
    qboolean (*Init)(void);
    void (*Shutdown)(refShutdownCode_t code);
    void (*Reset)(void);

    // Frame management
    void (*BeginFrame)(void);
    void (*EndFrame)(void);
    void (*Present)(void);

    // Scene management
    void (*BeginScene)(const refdef_t* refdef);
    void (*EndScene)(void);
    void (*ClearScene)(void);

    // Entity rendering
    void (*AddEntity)(const refEntity_t* entity);
    void (*AddPolygon)(qhandle_t shader, int numVerts, const polyVert_t* verts);

    // Lighting
    void (*AddLight)(const dlight_t* light);
    void (*SetupLighting)(void);

    // Shader management
    qhandle_t (*RegisterShader)(const char* name);
    void (*RemapShader)(const char* oldShader, const char* newShader, const char* timeOffset);

    // Image management
    qhandle_t (*RegisterImage)(const char* name);
    void (*UpdateImage)(qhandle_t image, const void* data, int x, int y, int width, int height);

    // Model management
    qhandle_t (*RegisterModel)(const char* name);

    // Font management
    qhandle_t (*RegisterFont)(const char* fontName, int pointSize, fontInfo_t* font);

    // Surface rendering
    void (*RenderSurfaces)(void);

    // Post-processing
    void (*BeginPostProcess)(void);
    void (*EndPostProcess)(void);

    // Debug/visualization
    void (*DebugDrawAxis)(void);
    void (*DebugDrawNormals)(void);
    void (*DebugDrawTangents)(void);

    // Performance monitoring
    void (*GetGPUInfo)(char* info, int size);
    void (*GetPerformanceStats)(float* fps, float* frameTime, float* gpuTime);

    // Hot reload support
    qboolean (*ReloadShaders)(void);
    qboolean (*ReloadTextures)(void);

    // Extension/feature queries
    qboolean (*HasFeature)(rendererFeature_t feature);
    const char* (*GetExtensionString)(void);

} rendererInterface_t;

// Global renderer interface - set by the active renderer
extern rendererInterface_t* renderer;

// Renderer factory functions
rendererInterface_t* Renderer_OpenGL2_Create(void);
rendererInterface_t* Renderer_Vulkan_Create(void);
rendererInterface_t* Renderer_Metal_Create(void);

// Common renderer utilities (shared across implementations)
qboolean R_ValidateRendererInterface(const rendererInterface_t* ri);
void R_LogRendererInfo(const rendererInterface_t* ri);
qboolean R_CheckRendererCompatibility(void);

#endif // __TR_RENDERER_H