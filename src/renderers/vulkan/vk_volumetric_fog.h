/*
=============================================================================
Volumetric Fog System
Advanced volumetric fog rendering with height-based density and lighting
=============================================================================
*/

#pragma once

#include "tr_local.h"
#include "vk.h"

#ifdef USE_VULKAN

// Volumetric fog parameters
typedef struct {
    float density;              // Base fog density
    float height;               // Fog layer height
    float falloff;              // Height-based falloff factor
    float scattering;           // Scattering coefficient
    float absorption;           // Absorption coefficient
    int numSamples;             // Number of ray marching samples
    float noiseScale;           // 3D noise texture scale
    float noiseSpeed;           // Animation speed for noise
    vec3_t lightColor;          // Light color for volumetric lighting
    float lightIntensity;       // Light intensity multiplier
    qboolean enabled;           // Enable/disable volumetric fog
    qboolean heightFog;         // Enable height-based fog
    qboolean animated;          // Enable noise animation
} volumetric_fog_params_t;

// Volumetric fog system state
typedef struct {
    qboolean initialized;
    qboolean enabled;

    // Vulkan resources
    VkImage fogImage;
    VkImageView fogImageView;
    VkDeviceMemory fogImageMemory;
    VkSampler fogSampler;

    VkImage noiseTexture;
    VkImageView noiseTextureView;
    VkDeviceMemory noiseTextureMemory;
    VkSampler noiseSampler;

    // Pipeline
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSet;

    // Current parameters
    volumetric_fog_params_t params;

    // Noise texture data (for generation)
    uint8_t *noiseData;
    int noiseSize;

} volumetric_fog_system_t;

// External API
void vk_volumetric_fog_init(void);
void vk_volumetric_fog_shutdown(void);
void vk_volumetric_fog_update(void);
void vk_volumetric_fog_render(void);
void vk_volumetric_fog_set_params(const volumetric_fog_params_t *params);
void vk_volumetric_fog_get_params(volumetric_fog_params_t *params);

// CVars
extern cvar_t *r_volumetricFog;
extern cvar_t *r_volumetricFogDensity;
extern cvar_t *r_volumetricFogHeight;
extern cvar_t *r_volumetricFogFalloff;
extern cvar_t *r_volumetricFogSamples;
extern cvar_t *r_volumetricFogScattering;
extern cvar_t *r_volumetricFogAbsorption;

#endif // USE_VULKAN