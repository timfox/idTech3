#pragma once

#include "tr_local.h"

// Multi-threaded Command Buffer Generation System
// Uses secondary command buffers and thread pools to parallelize command buffer recording
// Improves performance for scenes with many objects

// Forward declarations
struct refEntity_t;
struct drawSurf_t;

// Object data for multi-threaded rendering
typedef struct {
    vec3_t position;
    vec3_t rotation;
    vec3_t scale;
    mat4_t modelMatrix;
    qboolean visible;
    int entityIndex;
    float deltaTime;
} vk_thread_object_data_t;

// Thread-specific data
typedef struct {
    VkCommandPool commandPool;
    VkCommandBuffer* commandBuffers;  // Array of secondary command buffers
    int commandBufferCount;
    vk_thread_object_data_t* objectData;
    int objectCount;
    int threadIndex;
} vk_thread_data_t;

// Frustum for culling
typedef struct {
    vec4_t planes[6];  // Left, right, bottom, top, near, far
    vec3_t corners[8]; // Frustum corners
} vk_frustum_t;

// Multi-threaded rendering state
typedef struct {
    qboolean enabled;
    qboolean initialized;

    // Thread management
    int numThreads;
    vk_thread_data_t* threadData;
    qboolean useThreadPool;

    // Secondary command buffers
    VkCommandBuffer* secondaryCommandBuffers;
    int secondaryCommandBufferCount;

    // Frustum culling
    vk_frustum_t frustum;
    qboolean frustumCullingEnabled;

    // Settings
    int maxObjectsPerThread;
    int totalObjects;
    qboolean parallelCommandBufferGeneration;
} vk_multithreaded_rendering_t;

extern vk_multithreaded_rendering_t vk_multithreaded_rendering;

// Multi-threaded Rendering API
void VK_MultithreadedRendering_Init(void);
void VK_MultithreadedRendering_Shutdown(void);
void VK_MultithreadedRendering_CreateThreadPools(int numThreads);
void VK_MultithreadedRendering_UpdateFrustum(const mat4_t viewMatrix, const mat4_t projectionMatrix);
qboolean VK_MultithreadedRendering_IsObjectVisible(const vec3_t position, float radius);
void VK_MultithreadedRendering_BeginSecondaryCommandBuffer(VkCommandBuffer commandBuffer, 
                                                           VkCommandBufferInheritanceInfo* inheritanceInfo);
void VK_MultithreadedRendering_EndSecondaryCommandBuffer(VkCommandBuffer commandBuffer);
void VK_MultithreadedRendering_ExecuteSecondaryBuffers(VkCommandBuffer primaryBuffer, 
                                                         VkCommandBuffer* secondaryBuffers, 
                                                         int count);

// Thread pool management
void VK_MultithreadedRendering_AddJob(int threadIndex, void (*jobFunction)(void*), void* jobData);
void VK_MultithreadedRendering_WaitForThreads(void);

// Settings
qboolean VK_MultithreadedRendering_IsEnabled(void);
void VK_MultithreadedRendering_SetEnabled(qboolean enabled);
void VK_MultithreadedRendering_SetFrustumCullingEnabled(qboolean enabled);
int VK_MultithreadedRendering_GetThreadCount(void);
