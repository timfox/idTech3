#pragma once

#include "tr_local.h"

// Spherical Environment Mapping (SEM) with Material Capture support
// Provides stylized rendering using pre-baked material captures stored in array textures

typedef struct {
    qboolean enabled;
    qboolean initialized;

    // Material capture texture array
    VkImage matCapArrayImage;
    VkImageView matCapArrayView;
    VkDeviceMemory matCapArrayMemory;
    VkSampler matCapArraySampler;
    int matCapLayerCount;  // Number of mat caps in the array

    // Descriptors
    VkDescriptorSet descriptorSet;
    VkDescriptorSetLayout descriptorSetLayout;

    // Uniform buffer for mat cap selection
    VkBuffer uniformBuffer;
    VkDeviceMemory uniformBufferMemory;
    void* uniformBufferMapped;

    // Current mat cap index
    int currentMatCapIndex;

    // Settings
    float intensity;
    qboolean useNormalMap;
} vk_sem_t;

extern vk_sem_t vk_sem;

// SEM API
void VK_SEM_Init(void);
void VK_SEM_Shutdown(void);
void VK_SEM_LoadMatCapArray(const char* filename);
void VK_SEM_SetMatCapIndex(int index);
int VK_SEM_GetMatCapCount(void);
void VK_SEM_UpdateUniforms(const float* viewMatrix, const float* modelMatrix);
void VK_SEM_BindDescriptors(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout);

// Utility functions
qboolean VK_SEM_IsEnabled(void);
void VK_SEM_SetIntensity(float intensity);
void VK_SEM_SetUseNormalMap(qboolean useNormalMap);
