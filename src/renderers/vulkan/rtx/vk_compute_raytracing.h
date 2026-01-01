#pragma once

#include "tr_local.h"
#include <vulkan/vulkan.h>

// Compute Shader-Based Ray Tracing System
// Software ray tracing using compute shaders (not hardware-accelerated)
// Can be used as a fallback or alternative to hardware ray tracing

// Scene object types
typedef enum {
    RT_OBJECT_SPHERE = 0,
    RT_OBJECT_PLANE = 1
} rt_object_type_t;

// Scene object structure (matches compute shader layout)
typedef struct {
    vec4_t properties;      // Position+radius for sphere, normal+distance for plane
    vec3_t diffuse;         // Diffuse color
    float specular;         // Specular exponent
    uint32_t id;            // Object ID
    uint32_t objectType;    // RT_OBJECT_SPHERE or RT_OBJECT_PLANE
    int32_t _pad[2];        // Padding for alignment
} rt_scene_object_t;

// Ray tracing state
typedef struct {
    qboolean enabled;
    qboolean initialized;
    VkShaderModule computeShaderModule;
    qboolean computeShaderLoaded;

    // Compute resources
    VkQueue computeQueue;
    VkCommandPool computeCommandPool;
    VkCommandBuffer computeCommandBuffer;
    VkFence computeFence;

    // Storage image for ray traced output
    VkImage storageImage;
    VkImageView storageImageView;
    VkDeviceMemory storageImageMemory;
    VkSampler storageImageSampler;
    int storageImageWidth;
    int storageImageHeight;

    // Compute pipeline
    VkPipeline computePipeline;
    VkPipelineLayout computePipelineLayout;
    VkDescriptorSetLayout computeDescriptorSetLayout;
    VkDescriptorSet computeDescriptorSet;

    // Scene objects storage buffer
    VkBuffer sceneObjectsBuffer;
    VkDeviceMemory sceneObjectsBufferMemory;
    int sceneObjectCount;

    // Uniform buffer for ray tracing parameters
    VkBuffer uniformBuffer;
    VkDeviceMemory uniformBufferMemory;
    void* uniformBufferMapped;

    // Graphics pipeline for displaying result
    VkPipeline graphicsPipeline;
    VkPipelineLayout graphicsPipelineLayout;
    VkDescriptorSetLayout graphicsDescriptorSetLayout;
    VkDescriptorSet graphicsDescriptorSet;

    // Camera parameters
    vec3_t cameraPos;
    vec3_t cameraLookat;
    float cameraFOV;
    float aspectRatio;

    // Lighting
    vec3_t lightPos;
    vec4_t fogColor;

    // Settings
    int resolution;  // Storage image resolution (e.g., 2048)
    qboolean useReflections;
    int maxBounces;
    // Staging area for scene data (uploads via host-visible staging buffer)
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    size_t stagingBufferSize;
    qboolean stagingBufferInitialized;
    // Flag indicating staging data has been prepared for transfer this frame
    qboolean stagingDataDirty;
    // Smoke-test render path flag
    qboolean smokeTestEnabled;
    // GPU timestamp timing
    VkQueryPool computeQueryPool;
    float timestampPeriod; // nanoseconds per timestamp unit
} vk_compute_rt_t;

extern vk_compute_rt_t vk_compute_rt;

// Compute Ray Tracing API
void VK_ComputeRT_Init(void);
void VK_ComputeRT_Shutdown(void);
void VK_ComputeRT_CreateStorageImage(int width, int height);
void VK_ComputeRT_AddSphere(const vec3_t position, float radius, const vec3_t diffuse, float specular);
void VK_ComputeRT_AddPlane(const vec3_t normal, float distance, const vec3_t diffuse, float specular);
void VK_ComputeRT_ClearScene(void);
void VK_ComputeRT_UpdateCamera(const vec3_t position, const vec3_t lookat, float fov);
void VK_ComputeRT_UpdateLight(const vec3_t position);
void VK_ComputeRT_Dispatch(void);
void VK_ComputeRT_RenderFullscreen(void);

// Settings
qboolean VK_ComputeRT_IsEnabled(void);
void VK_ComputeRT_ReloadShader(void);
void VK_ComputeRT_EnableSmokeTest(qboolean enabled);
void VK_ComputeRT_BatchRenderFrame(void);
void VK_ComputeRT_SetEnabled(qboolean enabled);
void VK_ComputeRT_SetResolution(int resolution);
void VK_ComputeRT_SetMaxBounces(int bounces);
void VK_ComputeRT_SetUseReflections(qboolean use);
