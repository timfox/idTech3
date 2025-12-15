#pragma once

#include "tr_local.h"

// Advanced Shadow Techniques
// Supports multiple shadow mapping methods and cascaded shadow maps

typedef enum {
    SHADOW_DISABLED = 0,
    SHADOW_STENCIL_VOLUME,    // Traditional stencil shadow volumes
    SHADOW_DEPTH_MAP,         // Basic shadow mapping
    SHADOW_CSM,              // Cascaded Shadow Maps
    SHADOW_VSM,              // Variance Shadow Maps
    SHADOW_PCF,              // Percentage Closer Filtering
    SHADOW_PCSS,             // Percentage Closer Soft Shadows
    SHADOW_MSM,              // Moment Shadow Maps
    SHADOW_RSM               // Reflective Shadow Maps
} shadowTechnique_t;

typedef struct {
    shadowTechnique_t technique;
    qboolean enabled;
    qboolean initialized;

    // Basic shadow mapping
    VkImage depthImage;
    VkImageView depthView;
    VkDeviceMemory depthMemory;
    VkFramebuffer framebuffer;
    VkRenderPass renderPass;

    // CSM (Cascaded Shadow Maps)
    int cascadeCount;
    float cascadeSplits[4];
    VkImage csmDepthImages[4];
    VkImageView csmDepthViews[4];
    VkDeviceMemory csmDepthMemory[4];
    VkFramebuffer csmFramebuffers[4];
    VkDescriptorSet csmDescriptorSets[4];
    mat4_t csmViewProjMatrices[4];

    // VSM (Variance Shadow Maps)
    VkImage vsmImage;
    VkImageView vsmView;
    VkDeviceMemory vsmMemory;

    // Shadow parameters
    float shadowBias;
    float shadowSlopeBias;
    float shadowNear;
    float shadowFar;
    float shadowFilterSize;
    int shadowMapSize;

    // Light parameters
    vec3_t lightDirection;
    vec4_t lightColor;
    float lightIntensity;

    // Pipeline
    VkPipeline shadowPipeline;
    VkPipelineLayout shadowPipelineLayout;
    VkDescriptorSetLayout shadowDescriptorSetLayout;
    VkDescriptorSet shadowDescriptorSet;

} vk_shadow_t;

extern vk_shadow_t vk_shadow;

// Shadow API
void VK_Shadows_Init(void);
void VK_Shadows_Shutdown(void);
void VK_Shadows_BeginFrame(void);
void VK_Shadows_RenderDepth(const refdef_t* refdef);
void VK_Shadows_EndFrame(void);
void VK_Shadows_SetTechnique(shadowTechnique_t technique);
void VK_Shadows_SetLightDirection(const vec3_t direction);
void VK_Shadows_SetLightColor(const vec4_t color);
float VK_Shadows_GetVisibility(const vec3_t position, float ndotl);

// CSM functions
void VK_Shadows_UpdateCSMSplits(const refdef_t* refdef);
void VK_Shadows_RenderCSM(const refdef_t* refdef, int cascadeIndex);

// Advanced shadow functions
float VK_Shadows_SampleVSM(const vec3_t position);
float VK_Shadows_SamplePCF(const vec3_t position, int samples);
float VK_Shadows_SamplePCSS(const vec3_t position);
float VK_Shadows_SampleMSM(const vec3_t position);