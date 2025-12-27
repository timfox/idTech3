/*
=============================================================================
God Rays/Light Shafts System
Volumetric light scattering effects for atmospheric lighting
=============================================================================
*/

#pragma once

#include "tr_local.h"
#include "vk.h"

#ifdef USE_VULKAN

#define MAX_LIGHT_SOURCES 16
#define GOD_RAYS_SAMPLES 64

// Light source for god rays
typedef struct {
    vec3_t position;           // World position
    vec3_t screen_pos;         // Screen space position (0-1 range)
    vec3_t color;              // Light color
    float intensity;           // Light intensity
    float radius;              // Screen space radius
    qboolean active;           // Whether this light source is active
    qboolean sun_light;        // Special handling for sun/moon
} god_rays_light_t;

// God rays parameters
typedef struct {
    float density;             // Scattering density
    float weight;              // Weight factor
    float decay;               // Decay factor
    float exposure;            // Exposure adjustment
    int num_samples;           // Number of radial samples
    float max_distance;        // Maximum ray distance
    qboolean enabled;          // Enable/disable god rays
    qboolean auto_lights;      // Automatically detect bright lights
} god_rays_params_t;

// God rays system state
typedef struct {
    qboolean initialized;
    qboolean enabled;

    // Vulkan resources
    VkImage rays_image;
    VkImageView rays_image_view;
    VkDeviceMemory rays_image_memory;
    VkSampler rays_sampler;

    // Pipeline
    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
    VkDescriptorSetLayout descriptor_layout;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet descriptor_set;

    // Compute pipeline for light detection
    VkPipeline light_detect_pipeline;
    VkPipelineLayout light_detect_layout;
    VkDescriptorSet light_detect_set;

    // Current parameters
    god_rays_params_t params;

    // Light sources
    god_rays_light_t lights[MAX_LIGHT_SOURCES];
    int num_lights;

    // Sun/moon position
    vec3_t sun_position;
    vec3_t sun_color;
    float sun_intensity;

} god_rays_system_t;

// External API
void vk_god_rays_init(void);
void vk_god_rays_shutdown(void);
void vk_god_rays_update(void);
void vk_god_rays_render(VkCommandBuffer cmd_buffer);

// Light management
void vk_god_rays_add_light(const vec3_t position, const vec3_t color, float intensity, float radius);
void vk_god_rays_set_sun(const vec3_t position, const vec3_t color, float intensity);
void vk_god_rays_clear_lights(void);

// Parameter control
void vk_god_rays_set_params(const god_rays_params_t *params);
void vk_god_rays_get_params(god_rays_params_t *params);

// CVars
extern cvar_t *r_godRays;
extern cvar_t *r_godRaysDensity;
extern cvar_t *r_godRaysWeight;
extern cvar_t *r_godRaysDecay;
extern cvar_t *r_godRaysExposure;
extern cvar_t *r_godRaysSamples;

#endif // USE_VULKAN