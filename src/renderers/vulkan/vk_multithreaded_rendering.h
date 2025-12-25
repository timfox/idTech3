#pragma once

#include "tr_local.h"
#include "../../common/thread_platform.h"

// Dedicated Rendering Threads System
// Manages specialized threads for different rendering phases:
// - Geometry processing and culling
// - Lighting calculations
// - Shadow map generation
// - Post-processing effects
// - Command buffer generation
// - Asset loading and streaming
// - General compute operations

// Forward declarations
struct refEntity_t;
struct drawSurf_t;

// Rendering thread types
typedef enum {
    RENDER_THREAD_GEOMETRY = 0,  // Geometry processing and culling
    RENDER_THREAD_LIGHTING,      // Lighting calculations
    RENDER_THREAD_SHADOWS,       // Shadow map generation
    RENDER_THREAD_POST_PROCESS,  // Post-processing effects
    RENDER_THREAD_COMMAND_GEN,   // Command buffer generation
    RENDER_THREAD_ASSET_LOADING, // Asset streaming and loading
    RENDER_THREAD_COMPUTE,       // General compute operations
    RENDER_THREAD_MAX
} vk_render_thread_type_t;

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
    vk_render_thread_type_t threadType;
    qboolean active;

    // Dedicated thread management
    thread_handle_t handle;
    condition_t work_available;
    mutex_t work_mutex;
    qboolean should_exit;
    spinlock_t queue_lock;

    // Work queue for dedicated threads
    void** work_queue;
    int work_queue_size;
    atomic_int_t work_queue_head;
    atomic_int_t work_queue_tail;
    atomic_int_t work_available_count;

    // Performance tracking
    uint64_t total_work_items_processed;
    uint64_t total_execution_time_ns;
    float average_work_time_ms;
    uint64_t last_activity_time;
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
    qboolean useDedicatedThreads;

    // Dedicated threads (one per type)
    vk_thread_data_t* dedicatedThreads[RENDER_THREAD_MAX];
    qboolean dedicatedThreadEnabled[RENDER_THREAD_MAX];

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

    // Performance monitoring
    uint64_t frame_start_time;
    uint64_t thread_wait_time;
    uint64_t command_gen_time;
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

// Dedicated Rendering Threads API
qboolean VK_MultithreadedRendering_EnableDedicatedThread(vk_render_thread_type_t threadType);
void VK_MultithreadedRendering_DisableDedicatedThread(vk_render_thread_type_t threadType);
qboolean VK_MultithreadedRendering_IsDedicatedThreadEnabled(vk_render_thread_type_t threadType);

// Work submission to dedicated threads
void VK_MultithreadedRendering_SubmitGeometryWork(void* workData);
void VK_MultithreadedRendering_SubmitLightingWork(void* workData);
void VK_MultithreadedRendering_SubmitShadowWork(void* workData);
void VK_MultithreadedRendering_SubmitPostProcessWork(void* workData);
void VK_MultithreadedRendering_SubmitCommandGenWork(void* workData);
void VK_MultithreadedRendering_SubmitAssetLoadingWork(void* workData);
void VK_MultithreadedRendering_SubmitComputeWork(void* workData);

// Thread synchronization
void VK_MultithreadedRendering_WaitForAllThreads(void);
void VK_MultithreadedRendering_WaitForThread(vk_render_thread_type_t threadType);

// Performance monitoring
void VK_MultithreadedRendering_GetThreadStats(vk_render_thread_type_t threadType,
                                             uint64_t* processedItems,
                                             float* avgTimeMs,
                                             uint64_t* waitTimeNs);
