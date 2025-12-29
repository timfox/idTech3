/*
=============================================================================
Vulkan Raymarching System

Advanced raymarching implementation for:
- Distance field rendering
- Volumetric effects (fog, clouds)
- Procedural geometry
- Screen space raymarching effects
=============================================================================
*/

#ifndef VK_RAYMARCHING_H
#define VK_RAYMARCHING_H

#ifdef USE_VULKAN

#include "vk.h"

// Raymarching configuration
typedef struct {
    int maxSteps;
    float maxDistance;
    float epsilon;
    float volumetricDensity;
    qboolean enableVolumetric;
    vec3_t lightDirection;
    vec4_t lightColor;
    float ambientIntensity;
} raymarchingConfig_t;

// Distance field functions
typedef struct {
    vec3_t position;
    float radius;
    int type; // 0 = sphere, 1 = box, 2 = torus
    vec3_t dimensions;
} distanceField_t;

// Raymarching pipeline
typedef struct {
    VkPipeline pipeline;
    VkPipelineLayout layout;
    VkDescriptorSetLayout descriptorLayout;
    VkDescriptorSet descriptorSet;
    VkShaderModule computeShader;
} raymarchingPipeline_t;

// Function declarations
qboolean VK_Raymarching_Init(void);
void VK_Raymarching_Shutdown(void);
void VK_Raymarching_Render(VkCommandBuffer commandBuffer, VkImageView inputImage, VkImageView outputImage);
void VK_Raymarching_UpdateConfig(void);

// Distance field management
void VK_Raymarching_AddDistanceField(const distanceField_t* field);
void VK_Raymarching_ClearDistanceFields(void);
void VK_Raymarching_AddDemoFields(void);

// Volumetric raymarching
void VK_Raymarching_RenderVolumetric(VkCommandBuffer commandBuffer, VkImageView depthImage, VkImageView outputImage);

#endif // USE_VULKAN

#endif // VK_RAYMARCHING_H