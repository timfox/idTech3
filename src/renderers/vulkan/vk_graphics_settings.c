#include "vk_graphics_settings.h"
#include "vk.h"
#include "vk_ibl.h"
#include "vk_shadows.h"
#include "vk_sem.h"
#include "vk_physics.h"
#include "vk_compute_raytracing.h"
#include "tr_local.h"
#include "../../qcommon/qcommon.h"
#include <string.h>

vk_graphics_settings_t vk_graphics_settings;

// CVars for graphics settings
static cvar_t *r_graphicsPreset;
static cvar_t *r_pbr;
static cvar_t *r_ibl;
static cvar_t *r_iblIntensity;
static cvar_t *r_shadows;
static cvar_t *r_shadowQuality;
static cvar_t *r_shadowTechnique;
static cvar_t *r_shadowBias;
static cvar_t *r_shadowMapSize;
static cvar_t *r_sem;
static cvar_t *r_semMatCap;
static cvar_t *r_tessellation;
static cvar_t *r_tessellationLevel;
static cvar_t *r_deferred;
static cvar_t *r_bloom;
static cvar_t *r_bloomIntensity;
static cvar_t *r_tonemap;
static cvar_t *r_tonemapExposure;
static cvar_t *r_msaa;
static cvar_t *r_fxaa;
static cvar_t *r_textureQuality;
static cvar_t *r_anisotropic;
static cvar_t *r_maxAnisotropy;
static cvar_t *r_physics;
static cvar_t *r_physicsWind;
static cvar_t *r_physicsIterations;
static cvar_t *r_computeRT;
static cvar_t *r_computeRTResolution;
static cvar_t *r_computeRTReflections;
static cvar_t *r_computeRTMaxBounces;

void VK_GraphicsSettings_Init(void) {
    memset(&vk_graphics_settings, 0, sizeof(vk_graphics_settings_t));

    // Register CVars
    r_graphicsPreset = ri.Cvar_Get("r_graphicsPreset", "2", CVAR_ARCHIVE | CVAR_LATCH, "Graphics quality preset (0=Low, 1=Medium, 2=High, 3=Ultra, 4=Custom)");
    r_pbr = ri.Cvar_Get("r_pbr", "1", CVAR_ARCHIVE, "Enable PBR rendering");
    r_ibl = ri.Cvar_Get("r_ibl", "1", CVAR_ARCHIVE, "Enable Image-Based Lighting");
    r_iblIntensity = ri.Cvar_Get("r_iblIntensity", "1.0", CVAR_ARCHIVE, "IBL intensity multiplier");
    r_shadows = ri.Cvar_Get("r_shadows", "1", CVAR_ARCHIVE, "Enable shadows");
    r_shadowQuality = ri.Cvar_Get("r_shadowQuality", "2", CVAR_ARCHIVE, "Shadow quality (0=Disabled, 1=Low, 2=Medium, 3=High, 4=Ultra)");
    r_shadowTechnique = ri.Cvar_Get("r_shadowTechnique", "2", CVAR_ARCHIVE, "Shadow technique (0=Disabled, 1=Depth Map, 2=CSM, 3=VSM)");
    r_shadowBias = ri.Cvar_Get("r_shadowBias", "0.005", CVAR_ARCHIVE, "Shadow depth bias");
    r_shadowMapSize = ri.Cvar_Get("r_shadowMapSize", "2048", CVAR_ARCHIVE, "Shadow map resolution");
    r_sem = ri.Cvar_Get("r_sem", "0", CVAR_ARCHIVE, "Enable Spherical Environment Mapping");
    r_semMatCap = ri.Cvar_Get("r_semMatCap", "0", CVAR_ARCHIVE, "SEM material capture index");
    r_tessellation = ri.Cvar_Get("r_tessellation", "0", CVAR_ARCHIVE, "Enable tessellation");
    r_tessellationLevel = ri.Cvar_Get("r_tessellationLevel", "3.0", CVAR_ARCHIVE, "Tessellation level");
    r_deferred = ri.Cvar_Get("r_deferred", "0", CVAR_ARCHIVE, "Enable deferred rendering");
    r_bloom = ri.Cvar_Get("r_bloom", "1", CVAR_ARCHIVE, "Enable bloom post-processing");
    r_bloomIntensity = ri.Cvar_Get("r_bloomIntensity", "0.5", CVAR_ARCHIVE, "Bloom intensity");
    r_tonemap = ri.Cvar_Get("r_tonemap", "1", CVAR_ARCHIVE, "Enable tonemapping");
    r_tonemapExposure = ri.Cvar_Get("r_tonemapExposure", "1.0", CVAR_ARCHIVE, "Tonemap exposure");
    r_msaa = ri.Cvar_Get("r_msaa", "0", CVAR_ARCHIVE, "MSAA samples (0=Disabled, 1=2x, 2=4x, 3=8x)");
    r_fxaa = ri.Cvar_Get("r_fxaa", "0", CVAR_ARCHIVE, "Enable FXAA");
    r_textureQuality = ri.Cvar_Get("r_textureQuality", "2", CVAR_ARCHIVE, "Texture quality (0=Low, 1=Medium, 2=High, 3=Ultra)");
    r_anisotropic = ri.Cvar_Get("r_anisotropic", "1", CVAR_ARCHIVE, "Enable anisotropic filtering");
    r_maxAnisotropy = ri.Cvar_Get("r_maxAnisotropy", "16", CVAR_ARCHIVE, "Maximum anisotropic filtering level");
    r_physics = ri.Cvar_Get("r_physics", "0", CVAR_ARCHIVE, "Enable GPU physics simulation");
    r_physicsWind = ri.Cvar_Get("r_physicsWind", "0", CVAR_ARCHIVE, "Enable wind simulation");
    r_physicsIterations = ri.Cvar_Get("r_physicsIterations", "64", CVAR_ARCHIVE, "Physics simulation iterations per frame");
    r_computeRT = ri.Cvar_Get("r_computeRT", "0", CVAR_ARCHIVE, "Enable compute shader ray tracing (software)");
    r_computeRTResolution = ri.Cvar_Get("r_computeRTResolution", "2048", CVAR_ARCHIVE, "Compute RT resolution");
    r_computeRTReflections = ri.Cvar_Get("r_computeRTReflections", "1", CVAR_ARCHIVE, "Enable reflections in compute RT");
    r_computeRTMaxBounces = ri.Cvar_Get("r_computeRTMaxBounces", "3", CVAR_ARCHIVE, "Maximum ray bounces in compute RT");

    // Load settings from CVars
    VK_GraphicsSettings_LoadFromCvars();

    ri.Printf(PRINT_ALL, "Graphics settings system initialized\n");
}

void VK_GraphicsSettings_Shutdown(void) {
    // CVars are managed by the engine, no cleanup needed
    memset(&vk_graphics_settings, 0, sizeof(vk_graphics_settings_t));
}

void VK_GraphicsSettings_LoadFromCvars(void) {
    // Load preset
    vk_graphics_settings.preset = (graphics_preset_t)Com_Clamp(0, 4, r_graphicsPreset->integer);

    // PBR/IBL
    vk_graphics_settings.pbrEnabled = r_pbr->integer != 0;
    vk_graphics_settings.iblEnabled = r_ibl->integer != 0;
    vk_graphics_settings.iblIntensity = r_iblIntensity->value;
    VectorSet(vk_graphics_settings.iblTintColor, 1.0f, 1.0f, 1.0f);

    // Shadows
    vk_graphics_settings.shadowsEnabled = r_shadows->integer != 0;
    vk_graphics_settings.shadowQuality = (shadow_quality_t)Com_Clamp(0, 4, r_shadowQuality->integer);
    vk_graphics_settings.shadowTechnique = (shadowTechnique_t)Com_Clamp(0, 7, r_shadowTechnique->integer);
    vk_graphics_settings.shadowBias = r_shadowBias->value;
    vk_graphics_settings.shadowMapSize = r_shadowMapSize->integer;

    // SEM
    vk_graphics_settings.semEnabled = r_sem->integer != 0;
    vk_graphics_settings.semMatCapIndex = r_semMatCap->integer;
    vk_graphics_settings.semIntensity = 1.0f;

    // Tessellation
    vk_graphics_settings.tessellationEnabled = r_tessellation->integer != 0;
    vk_graphics_settings.tessellationLevel = r_tessellationLevel->value;

    // Deferred
    vk_graphics_settings.deferredEnabled = r_deferred->integer != 0;

    // Post-processing
    vk_graphics_settings.bloomEnabled = r_bloom->integer != 0;
    vk_graphics_settings.bloomIntensity = r_bloomIntensity->value;
    vk_graphics_settings.tonemappingEnabled = r_tonemap->integer != 0;
    vk_graphics_settings.tonemapExposure = r_tonemapExposure->value;

    // Anti-aliasing
    vk_graphics_settings.msaaSamples = Com_Clamp(0, 3, r_msaa->integer);
    vk_graphics_settings.fxaaEnabled = r_fxaa->integer != 0;

    // Texture quality
    vk_graphics_settings.textureQuality = Com_Clamp(0, 3, r_textureQuality->integer);
    vk_graphics_settings.anisotropicFiltering = r_anisotropic->integer != 0;
    vk_graphics_settings.maxAnisotropy = Com_Clamp(1, 16, r_maxAnisotropy->integer);

    // Physics
    vk_graphics_settings.physicsEnabled = r_physics->integer != 0;
    vk_graphics_settings.physicsWindEnabled = r_physicsWind->integer != 0;
    vk_graphics_settings.physicsIterations = Com_Clamp(1, 256, r_physicsIterations->integer);
    VectorSet(vk_graphics_settings.physicsGravity, 0.0f, -9.8f, 0.0f);

    // Compute Ray Tracing
    vk_graphics_settings.computeRTEnabled = r_computeRT->integer != 0;
    vk_graphics_settings.computeRTResolution = Com_Clamp(256, 4096, r_computeRTResolution->integer);
    vk_graphics_settings.computeRTReflections = r_computeRTReflections->integer != 0;
    vk_graphics_settings.computeRTMaxBounces = Com_Clamp(0, 8, r_computeRTMaxBounces->integer);
}

void VK_GraphicsSettings_ApplyPreset(graphics_preset_t preset) {
    vk_graphics_settings.preset = preset;
    vk_graphics_settings.presetModified = qtrue;

    switch (preset) {
        case GRAPHICS_PRESET_LOW:
            vk_graphics_settings.pbrEnabled = qfalse;
            vk_graphics_settings.iblEnabled = qfalse;
            vk_graphics_settings.shadowsEnabled = qfalse;
            vk_graphics_settings.shadowQuality = SHADOW_QUALITY_DISABLED;
            vk_graphics_settings.shadowMapSize = 512;
            vk_graphics_settings.bloomEnabled = qfalse;
            vk_graphics_settings.msaaSamples = 0;
            vk_graphics_settings.textureQuality = 0;
            vk_graphics_settings.anisotropicFiltering = qfalse;
            break;

        case GRAPHICS_PRESET_MEDIUM:
            vk_graphics_settings.pbrEnabled = qtrue;
            vk_graphics_settings.iblEnabled = qfalse;
            vk_graphics_settings.shadowsEnabled = qtrue;
            vk_graphics_settings.shadowQuality = SHADOW_QUALITY_LOW;
            vk_graphics_settings.shadowMapSize = 1024;
            vk_graphics_settings.bloomEnabled = qtrue;
            vk_graphics_settings.msaaSamples = 0;
            vk_graphics_settings.textureQuality = 1;
            vk_graphics_settings.anisotropicFiltering = qtrue;
            vk_graphics_settings.maxAnisotropy = 4;
            break;

        case GRAPHICS_PRESET_HIGH:
            vk_graphics_settings.pbrEnabled = qtrue;
            vk_graphics_settings.iblEnabled = qtrue;
            vk_graphics_settings.shadowsEnabled = qtrue;
            vk_graphics_settings.shadowQuality = SHADOW_QUALITY_MEDIUM;
            vk_graphics_settings.shadowMapSize = 2048;
            vk_graphics_settings.bloomEnabled = qtrue;
            vk_graphics_settings.msaaSamples = 1; // 2x
            vk_graphics_settings.textureQuality = 2;
            vk_graphics_settings.anisotropicFiltering = qtrue;
            vk_graphics_settings.maxAnisotropy = 8;
            break;

        case GRAPHICS_PRESET_ULTRA:
            vk_graphics_settings.pbrEnabled = qtrue;
            vk_graphics_settings.iblEnabled = qtrue;
            vk_graphics_settings.shadowsEnabled = qtrue;
            vk_graphics_settings.shadowQuality = SHADOW_QUALITY_ULTRA;
            vk_graphics_settings.shadowMapSize = 4096;
            vk_graphics_settings.bloomEnabled = qtrue;
            vk_graphics_settings.msaaSamples = 2; // 4x
            vk_graphics_settings.textureQuality = 3;
            vk_graphics_settings.anisotropicFiltering = qtrue;
            vk_graphics_settings.maxAnisotropy = 16;
            break;

        case GRAPHICS_PRESET_CUSTOM:
            // Don't modify settings, user has customized them
            break;
    }

    VK_GraphicsSettings_ApplySettings();
}

void VK_GraphicsSettings_ApplySettings(void) {
    // Apply PBR/IBL settings
    if (vk_ibl.initialized) {
        vk_ibl.enabled = vk_graphics_settings.iblEnabled;
        vk_ibl.intensity = vk_graphics_settings.iblIntensity;
        VectorCopy(vk_graphics_settings.iblTintColor, vk_ibl.tintColor);
    }

    // Apply shadow settings
    if (vk_shadow.initialized) {
        vk_shadow.enabled = vk_graphics_settings.shadowsEnabled;
        vk_shadow.shadowMapSize = vk_graphics_settings.shadowMapSize;
        vk_shadow.shadowBias = vk_graphics_settings.shadowBias;
        
        if (vk_shadow.technique != vk_graphics_settings.shadowTechnique) {
            VK_Shadows_SetTechnique(vk_graphics_settings.shadowTechnique);
        }
    }

    // Apply SEM settings
    if (vk_sem.initialized) {
        vk_sem.enabled = vk_graphics_settings.semEnabled;
        VK_SEM_SetMatCapIndex(vk_graphics_settings.semMatCapIndex);
        VK_SEM_SetIntensity(vk_graphics_settings.semIntensity);
    }

    // Apply Physics settings
    if (vk_physics.initialized) {
        VK_Physics_SetEnabled(vk_graphics_settings.physicsEnabled);
        VK_Physics_SetWindEnabled(vk_graphics_settings.physicsWindEnabled);
        VK_Physics_SetSimulationIterations(vk_graphics_settings.physicsIterations);
        VK_Physics_SetGravity(vk_graphics_settings.physicsGravity);
    }

    // Apply Compute Ray Tracing settings
    if (vk_compute_rt.initialized) {
        VK_ComputeRT_SetEnabled(vk_graphics_settings.computeRTEnabled);
        VK_ComputeRT_SetResolution(vk_graphics_settings.computeRTResolution);
        VK_ComputeRT_SetUseReflections(vk_graphics_settings.computeRTReflections);
        VK_ComputeRT_SetMaxBounces(vk_graphics_settings.computeRTMaxBounces);
    }

    // Update CVars to match settings
    if (r_pbr) r_pbr->integer = vk_graphics_settings.pbrEnabled ? 1 : 0;
    if (r_ibl) r_ibl->integer = vk_graphics_settings.iblEnabled ? 1 : 0;
    if (r_shadows) r_shadows->integer = vk_graphics_settings.shadowsEnabled ? 1 : 0;
    if (r_shadowQuality) r_shadowQuality->integer = vk_graphics_settings.shadowQuality;
    if (r_shadowMapSize) r_shadowMapSize->integer = vk_graphics_settings.shadowMapSize;
    if (r_sem) r_sem->integer = vk_graphics_settings.semEnabled ? 1 : 0;
    if (r_tessellation) r_tessellation->integer = vk_graphics_settings.tessellationEnabled ? 1 : 0;
    if (r_bloom) r_bloom->integer = vk_graphics_settings.bloomEnabled ? 1 : 0;
    if (r_msaa) r_msaa->integer = vk_graphics_settings.msaaSamples;
    if (r_physics) r_physics->integer = vk_graphics_settings.physicsEnabled ? 1 : 0;
    if (r_physicsWind) r_physicsWind->integer = vk_graphics_settings.physicsWindEnabled ? 1 : 0;
    if (r_physicsIterations) r_physicsIterations->integer = vk_graphics_settings.physicsIterations;
    if (r_computeRT) r_computeRT->integer = vk_graphics_settings.computeRTEnabled ? 1 : 0;
    if (r_computeRTResolution) r_computeRTResolution->integer = vk_graphics_settings.computeRTResolution;
    if (r_computeRTReflections) r_computeRTReflections->integer = vk_graphics_settings.computeRTReflections ? 1 : 0;
    if (r_computeRTMaxBounces) r_computeRTMaxBounces->integer = vk_graphics_settings.computeRTMaxBounces;
}

void VK_GraphicsSettings_ResetToDefaults(void) {
    VK_GraphicsSettings_ApplyPreset(GRAPHICS_PRESET_HIGH);
}

// Getters/Setters
qboolean VK_GraphicsSettings_GetPBREnabled(void) {
    return vk_graphics_settings.pbrEnabled;
}

void VK_GraphicsSettings_SetPBREnabled(qboolean enabled) {
    vk_graphics_settings.pbrEnabled = enabled;
    vk_graphics_settings.preset = GRAPHICS_PRESET_CUSTOM;
    VK_GraphicsSettings_ApplySettings();
}

qboolean VK_GraphicsSettings_GetIBLEnabled(void) {
    return vk_graphics_settings.iblEnabled;
}

void VK_GraphicsSettings_SetIBLEnabled(qboolean enabled) {
    vk_graphics_settings.iblEnabled = enabled;
    vk_graphics_settings.preset = GRAPHICS_PRESET_CUSTOM;
    VK_GraphicsSettings_ApplySettings();
}

qboolean VK_GraphicsSettings_GetShadowsEnabled(void) {
    return vk_graphics_settings.shadowsEnabled;
}

void VK_GraphicsSettings_SetShadowsEnabled(qboolean enabled) {
    vk_graphics_settings.shadowsEnabled = enabled;
    vk_graphics_settings.preset = GRAPHICS_PRESET_CUSTOM;
    VK_GraphicsSettings_ApplySettings();
}

shadow_quality_t VK_GraphicsSettings_GetShadowQuality(void) {
    return vk_graphics_settings.shadowQuality;
}

void VK_GraphicsSettings_SetShadowQuality(shadow_quality_t quality) {
    vk_graphics_settings.shadowQuality = quality;
    vk_graphics_settings.preset = GRAPHICS_PRESET_CUSTOM;
    
    // Map quality to map size and technique
    switch (quality) {
        case SHADOW_QUALITY_DISABLED:
            vk_graphics_settings.shadowsEnabled = qfalse;
            break;
        case SHADOW_QUALITY_LOW:
            vk_graphics_settings.shadowMapSize = 512;
            vk_graphics_settings.shadowTechnique = SHADOW_DEPTH_MAP;
            break;
        case SHADOW_QUALITY_MEDIUM:
            vk_graphics_settings.shadowMapSize = 1024;
            vk_graphics_settings.shadowTechnique = SHADOW_PCF;
            break;
        case SHADOW_QUALITY_HIGH:
            vk_graphics_settings.shadowMapSize = 2048;
            vk_graphics_settings.shadowTechnique = SHADOW_CSM;
            break;
        case SHADOW_QUALITY_ULTRA:
            vk_graphics_settings.shadowMapSize = 4096;
            vk_graphics_settings.shadowTechnique = SHADOW_CSM;
            break;
    }
    
    VK_GraphicsSettings_ApplySettings();
}

qboolean VK_GraphicsSettings_GetSEMEnabled(void) {
    return vk_graphics_settings.semEnabled;
}

void VK_GraphicsSettings_SetSEMEnabled(qboolean enabled) {
    vk_graphics_settings.semEnabled = enabled;
    vk_graphics_settings.preset = GRAPHICS_PRESET_CUSTOM;
    VK_GraphicsSettings_ApplySettings();
}

qboolean VK_GraphicsSettings_GetTessellationEnabled(void) {
    return vk_graphics_settings.tessellationEnabled;
}

void VK_GraphicsSettings_SetTessellationEnabled(qboolean enabled) {
    vk_graphics_settings.tessellationEnabled = enabled;
    vk_graphics_settings.preset = GRAPHICS_PRESET_CUSTOM;
    VK_GraphicsSettings_ApplySettings();
}

const char* VK_GraphicsSettings_GetPresetName(graphics_preset_t preset) {
    switch (preset) {
        case GRAPHICS_PRESET_LOW: return "Low";
        case GRAPHICS_PRESET_MEDIUM: return "Medium";
        case GRAPHICS_PRESET_HIGH: return "High";
        case GRAPHICS_PRESET_ULTRA: return "Ultra";
        case GRAPHICS_PRESET_CUSTOM: return "Custom";
        default: return "Unknown";
    }
}

const char* VK_GraphicsSettings_GetShadowQualityName(shadow_quality_t quality) {
    switch (quality) {
        case SHADOW_QUALITY_DISABLED: return "Disabled";
        case SHADOW_QUALITY_LOW: return "Low";
        case SHADOW_QUALITY_MEDIUM: return "Medium";
        case SHADOW_QUALITY_HIGH: return "High";
        case SHADOW_QUALITY_ULTRA: return "Ultra";
        default: return "Unknown";
    }
}

void VK_GraphicsSettings_DrawUI(void) {
    // UI rendering would be integrated with ImGui or similar
    // This is a placeholder for the UI system
    // In a full implementation, this would draw sliders, checkboxes, etc.
}
