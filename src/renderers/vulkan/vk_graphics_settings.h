#pragma once

#include "tr_local.h"

// Graphics Settings System
// Provides comprehensive in-game configuration for all rendering features

// Graphics quality presets
typedef enum {
    GRAPHICS_PRESET_LOW = 0,
    GRAPHICS_PRESET_MEDIUM,
    GRAPHICS_PRESET_HIGH,
    GRAPHICS_PRESET_ULTRA,
    GRAPHICS_PRESET_CUSTOM
} graphics_preset_t;

// Shadow quality levels
typedef enum {
    SHADOW_QUALITY_DISABLED = 0,
    SHADOW_QUALITY_LOW,      // 512x512, basic depth map
    SHADOW_QUALITY_MEDIUM,   // 1024x1024, PCF
    SHADOW_QUALITY_HIGH,     // 2048x2048, CSM
    SHADOW_QUALITY_ULTRA     // 4096x4096, CSM + VSM
} shadow_quality_t;

// Graphics settings structure
typedef struct {
    // Quality preset
    graphics_preset_t preset;
    qboolean presetModified;

    // PBR/IBL Settings
    qboolean pbrEnabled;
    qboolean iblEnabled;
    float iblIntensity;
    float iblRotation;
    vec3_t iblTintColor;

    // Shadow Settings
    qboolean shadowsEnabled;
    shadow_quality_t shadowQuality;
    shadowTechnique_t shadowTechnique;
    float shadowBias;
    float shadowSlopeBias;
    float shadowFilterSize;
    int shadowMapSize;
    qboolean shadowSoftening;  // PCF/PCSS

    // SEM (Spherical Environment Mapping) Settings
    qboolean semEnabled;
    int semMatCapIndex;
    float semIntensity;
    qboolean semUseNormalMap;

    // Tessellation Settings
    qboolean tessellationEnabled;
    float tessellationLevel;
    float tessellationAlpha;

    // Deferred Rendering
    qboolean deferredEnabled;
    int deferredBufferSize;

    // Post-processing
    qboolean bloomEnabled;
    float bloomIntensity;
    float bloomThreshold;
    qboolean tonemappingEnabled;
    float tonemapExposure;

    // Anti-aliasing
    int msaaSamples;
    qboolean fxaaEnabled;

    // Texture Quality
    int textureQuality;  // 0-3 (low to ultra)
    qboolean anisotropicFiltering;
    int maxAnisotropy;

    // Advanced Features
    qboolean rayTracingEnabled;
    qboolean meshShadersEnabled;
    qboolean gpuCullingEnabled;

    // Physics Simulation
    qboolean physicsEnabled;
    qboolean physicsWindEnabled;
    int physicsIterations;
    float physicsGravity[3];
} vk_graphics_settings_t;

extern vk_graphics_settings_t vk_graphics_settings;

// Graphics Settings API
void VK_GraphicsSettings_Init(void);
void VK_GraphicsSettings_Shutdown(void);
void VK_GraphicsSettings_LoadFromCvars(void);
void VK_GraphicsSettings_ApplyPreset(graphics_preset_t preset);
void VK_GraphicsSettings_ApplySettings(void);
void VK_GraphicsSettings_ResetToDefaults(void);

// Individual setting getters/setters
qboolean VK_GraphicsSettings_GetPBREnabled(void);
void VK_GraphicsSettings_SetPBREnabled(qboolean enabled);
qboolean VK_GraphicsSettings_GetIBLEnabled(void);
void VK_GraphicsSettings_SetIBLEnabled(qboolean enabled);
qboolean VK_GraphicsSettings_GetShadowsEnabled(void);
void VK_GraphicsSettings_SetShadowsEnabled(qboolean enabled);
shadow_quality_t VK_GraphicsSettings_GetShadowQuality(void);
void VK_GraphicsSettings_SetShadowQuality(shadow_quality_t quality);
qboolean VK_GraphicsSettings_GetSEMEnabled(void);
void VK_GraphicsSettings_SetSEMEnabled(qboolean enabled);
qboolean VK_GraphicsSettings_GetTessellationEnabled(void);
void VK_GraphicsSettings_SetTessellationEnabled(qboolean enabled);

// UI Integration
void VK_GraphicsSettings_DrawUI(void);
const char* VK_GraphicsSettings_GetPresetName(graphics_preset_t preset);
const char* VK_GraphicsSettings_GetShadowQualityName(shadow_quality_t quality);
