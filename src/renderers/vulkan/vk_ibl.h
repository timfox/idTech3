#pragma once

#include "tr_local.h"

// Image-Based Lighting (IBL) system for PBR
// Provides environment lighting using precomputed radiance and irradiance maps

typedef struct {
    qboolean enabled;
    qboolean initialized;

    // Environment cubemap
    VkImage envCubemap;
    VkImageView envCubemapView;
    VkDeviceMemory envCubemapMemory;

    // Irradiance cubemap (for diffuse IBL)
    VkImage irradianceCubemap;
    VkImageView irradianceCubemapView;
    VkDeviceMemory irradianceCubemapMemory;

    // Prefiltered radiance cubemap (for specular IBL)
    VkImage radianceCubemap;
    VkImageView radianceCubemapView;
    VkDeviceMemory radianceCubemapMemory;

    // BRDF LUT texture
    VkImage brdfLut;
    VkImageView brdfLutView;
    VkDeviceMemory brdfLutMemory;

    // Descriptors
    VkDescriptorSet descriptorSet;
    VkDescriptorSetLayout descriptorSetLayout;

    // Pipeline for IBL computations
    VkPipeline computePipeline;
    VkPipelineLayout computePipelineLayout;
    VkDescriptorSetLayout computeDescriptorSetLayout;
    VkDescriptorSet computeDescriptorSet;

    // Settings
    float intensity;
    float rotation;
    vec3_t tintColor;
} vk_ibl_t;

extern vk_ibl_t vk_ibl;

// IBL API
void VK_IBL_Init(void);
void VK_IBL_Shutdown(void);
void VK_IBL_LoadEnvironment(const char* cubemapName);
void VK_IBL_GenerateIrradiance(void);
void VK_IBL_GenerateRadiance(void);
void VK_IBL_GenerateBRDFLut(void);
void VK_IBL_UpdateDescriptors(void);
void VK_IBL_RenderEnvironment(qboolean backgroundOnly);

// PBR enhanced functions
void VK_PBR_ApplyIBL(const material_params_t* material, vec3_t viewDir, vec3_t normal,
                     vec3_t albedo, float metallic, float roughness, vec3_t result);
void VK_PBR_ComputeLighting(vec3_t position, vec3_t normal, vec3_t viewDir,
                           vec3_t albedo, float metallic, float roughness,
                           vec3_t emissive, vec3_t result);

// Advanced PBR features
void VK_PBR_ApplyAnisotropy(vec3_t normal, vec3_t tangent, float anisotropy, vec3_t result);
void VK_PBR_ApplySheen(vec3_t sheenColor, float sheen, vec3_t result);
void VK_PBR_ApplySubsurface(vec3_t subsurfaceColor, float subsurface, vec3_t result);
void VK_PBR_ApplyClearcoat(float clearcoat, float clearcoatRoughness, vec3_t result);