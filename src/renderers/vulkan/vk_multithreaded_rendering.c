#include "vk_multithreaded_rendering.h"
#include "vk.h"
#include "tr_local.h"
// Renderer import interface - defined in renderer main file
extern refimport_t ri;
#include "../../common/qcommon.h"
#include "../../common/thread_platform.h"
#include <string.h>
#include <math.h>

vk_multithreaded_rendering_t vk_multithreaded_rendering;

// Work item for dedicated threads
typedef struct {
    void (*work_function)(void* data);
    void* work_data;
    uint64_t submit_time;
} vk_thread_work_item_t;

// Dedicated thread worker function
static THREAD_RETURN THREAD_CALL DedicatedThreadWorker(void* arg) {
    vk_thread_data_t* thread = (vk_thread_data_t*)arg;

    while (!thread->should_exit) {
        // Wait for work
        MUTEX_LOCK(thread->work_mutex);
        while (atomic_load_explicit(&thread->work_available_count, memory_order_relaxed) == 0 &&
               !thread->should_exit) {
            CONDITION_WAIT(thread->work_available, thread->work_mutex);
        }
        MUTEX_UNLOCK(thread->work_mutex);

        if (thread->should_exit) break;

        // Process work items
        while (1) {
            SpinLock_Lock(&thread->queue_lock);

            int available = atomic_load_explicit(&thread->work_available_count, memory_order_relaxed);
            if (available == 0) {
                SpinLock_Unlock(&thread->queue_lock);
                break;
            }

            int head = atomic_load_explicit(&thread->work_queue_head, memory_order_relaxed);
            vk_thread_work_item_t* work = (vk_thread_work_item_t*)thread->work_queue[head];

            atomic_store_explicit(&thread->work_queue_head,
                                (head + 1) % thread->work_queue_size, memory_order_relaxed);
            atomic_fetch_sub_explicit(&thread->work_available_count, 1, memory_order_relaxed);

            SpinLock_Unlock(&thread->queue_lock);

            if (work) {
                // Execute work
                uint64_t start_time = ri.Microseconds() * 1000;
                work->work_function(work->work_data);
                uint64_t end_time = ri.Microseconds() * 1000;

                // Update statistics
                thread->total_work_items_processed++;
                uint64_t execution_time = end_time - start_time;
                thread->total_execution_time_ns += execution_time;
                thread->average_work_time_ms = (float)thread->total_execution_time_ns /
                                             (float)thread->total_work_items_processed / 1000000.0f;
                thread->last_activity_time = end_time;
            }
        }
    }

    return 0;
}

void VK_MultithreadedRendering_Init(void) {
    memset(&vk_multithreaded_rendering, 0, sizeof(vk_multithreaded_rendering_t));

    // Detect number of CPU cores
    int cpuCount = Sys_GetCPUCount();
    vk_multithreaded_rendering.numThreads = Com_Clamp(2, cpuCount - 1, cpuCount / 2); // Use half the cores max
    vk_multithreaded_rendering.maxObjectsPerThread = 128;
    vk_multithreaded_rendering.frustumCullingEnabled = qtrue;
    vk_multithreaded_rendering.parallelCommandBufferGeneration = qtrue;
    vk_multithreaded_rendering.useDedicatedThreads = qtrue;

    // Create thread data structures
    vk_multithreaded_rendering.threadData = (vk_thread_data_t*)ri.Hunk_AllocateTempMemory(
        vk_multithreaded_rendering.numThreads * sizeof(vk_thread_data_t));
    memset(vk_multithreaded_rendering.threadData, 0,
        vk_multithreaded_rendering.numThreads * sizeof(vk_thread_data_t));

    // Create command pools for each thread
    for (int i = 0; i < vk_multithreaded_rendering.numThreads; i++) {
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = vk.queue_family_index;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        VK_CHECK(qvkCreateCommandPool(vk.device, &poolInfo, NULL,
            &vk_multithreaded_rendering.threadData[i].commandPool));

        vk_multithreaded_rendering.threadData[i].threadIndex = i;
        vk_multithreaded_rendering.threadData[i].objectCount = 0;
        vk_multithreaded_rendering.threadData[i].commandBufferCount = 0;
        vk_multithreaded_rendering.threadData[i].active = qfalse;
    }

    // Initialize dedicated threads
    for (int i = 0; i < RENDER_THREAD_MAX; i++) {
        vk_multithreaded_rendering.dedicatedThreads[i] = NULL;
        vk_multithreaded_rendering.dedicatedThreadEnabled[i] = qfalse;
    }

    // Initialize frustum
    memset(&vk_multithreaded_rendering.frustum, 0, sizeof(vk_frustum_t));

    vk_multithreaded_rendering.enabled = qtrue;
    vk_multithreaded_rendering.initialized = qtrue;

    ri.Printf(PRINT_ALL, "Multi-threaded rendering system initialized (%d threads, %d CPUs detected)\n",
        vk_multithreaded_rendering.numThreads, cpuCount);
}

void VK_MultithreadedRendering_Shutdown(void) {
    if (!vk_multithreaded_rendering.initialized) return;

    // Shutdown dedicated threads first
    for (int i = 0; i < RENDER_THREAD_MAX; i++) {
        VK_MultithreadedRendering_DisableDedicatedThread((vk_render_thread_type_t)i);
    }

    // Free command buffers for regular threads
    for (int i = 0; i < vk_multithreaded_rendering.numThreads; i++) {
        vk_thread_data_t* thread = &vk_multithreaded_rendering.threadData[i];

        if (thread->commandBuffers && thread->commandBufferCount > 0) {
            qvkFreeCommandBuffers(vk.device, thread->commandPool,
                thread->commandBufferCount, thread->commandBuffers);
        }

        if (thread->commandPool != VK_NULL_HANDLE) {
            qvkDestroyCommandPool(vk.device, thread->commandPool, NULL);
        }

        if (thread->objectData) {
            ri.Hunk_FreeTempMemory(thread->objectData);
        }
    }

    if (vk_multithreaded_rendering.threadData) {
        ri.Hunk_FreeTempMemory(vk_multithreaded_rendering.threadData);
    }

    if (vk_multithreaded_rendering.secondaryCommandBuffers) {
        ri.Hunk_FreeTempMemory(vk_multithreaded_rendering.secondaryCommandBuffers);
    }

    memset(&vk_multithreaded_rendering, 0, sizeof(vk_multithreaded_rendering_t));
}

void VK_MultithreadedRendering_CreateThreadPools(int numThreads) {
    if (!vk_multithreaded_rendering.initialized) return;

    vk_multithreaded_rendering.numThreads = Com_Clamp(1, 16, numThreads);
    // Thread pool creation would happen here
    // In a full implementation, would create actual threads
}

void VK_MultithreadedRendering_UpdateFrustum(const mat4_t viewMatrix, const mat4_t projectionMatrix) {
    if (!vk_multithreaded_rendering.frustumCullingEnabled) return;

    // Compute frustum planes from view-projection matrix
    // mat4_t is a flat array [16] - need to check actual layout used in engine
    // For now, use a simplified approach - extract planes directly from combined matrix
    // In a full implementation, would properly multiply matrices and extract planes
    
    // Simplified frustum update - would need proper matrix multiplication
    // This is a placeholder that will be filled in when integrating with actual rendering
    Q_UNUSED(viewMatrix);
    Q_UNUSED(projectionMatrix);
    
    // For now, disable frustum culling until proper matrix handling is implemented
    // The frustum structure is initialized but not computed

    // Normalize planes
    for (int i = 0; i < 6; i++) {
        float length = sqrtf(
            vk_multithreaded_rendering.frustum.planes[i][0] * vk_multithreaded_rendering.frustum.planes[i][0] +
            vk_multithreaded_rendering.frustum.planes[i][1] * vk_multithreaded_rendering.frustum.planes[i][1] +
            vk_multithreaded_rendering.frustum.planes[i][2] * vk_multithreaded_rendering.frustum.planes[i][2]);
        if (length > 0.0f) {
            float invLength = 1.0f / length;
            vk_multithreaded_rendering.frustum.planes[i][0] *= invLength;
            vk_multithreaded_rendering.frustum.planes[i][1] *= invLength;
            vk_multithreaded_rendering.frustum.planes[i][2] *= invLength;
            vk_multithreaded_rendering.frustum.planes[i][3] *= invLength;
        }
    }
}

qboolean VK_MultithreadedRendering_IsObjectVisible(const vec3_t position, float radius) {
    if (!vk_multithreaded_rendering.frustumCullingEnabled) return qtrue;

    // Test sphere against frustum planes
    for (int i = 0; i < 6; i++) {
        float distance = DotProduct(position, vk_multithreaded_rendering.frustum.planes[i]) +
            vk_multithreaded_rendering.frustum.planes[i][3];
        if (distance < -radius) {
            return qfalse;  // Object is outside frustum
        }
    }
    return qtrue;  // Object is inside or intersects frustum
}

void VK_MultithreadedRendering_BeginSecondaryCommandBuffer(VkCommandBuffer commandBuffer,
                                                           VkCommandBufferInheritanceInfo* inheritanceInfo) {
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
    beginInfo.pInheritanceInfo = inheritanceInfo;

    VK_CHECK(qvkBeginCommandBuffer(commandBuffer, &beginInfo));
}

void VK_MultithreadedRendering_EndSecondaryCommandBuffer(VkCommandBuffer commandBuffer) {
    VK_CHECK(qvkEndCommandBuffer(commandBuffer));
}

void VK_MultithreadedRendering_ExecuteSecondaryBuffers(VkCommandBuffer primaryBuffer,
                                                       VkCommandBuffer* secondaryBuffers,
                                                       int count) {
    if (count > 0 && secondaryBuffers) {
        qvkCmdExecuteCommands(primaryBuffer, count, secondaryBuffers);
    }
}

void VK_MultithreadedRendering_AddJob(int threadIndex, void (*jobFunction)(void*), void* jobData) {
    // Add job to thread pool
    // In a full implementation, would use a proper thread pool with job queues
    // For now, this is a placeholder
    Q_UNUSED(threadIndex);
    Q_UNUSED(jobFunction);
    Q_UNUSED(jobData);
}

void VK_MultithreadedRendering_WaitForThreads(void) {
    // Wait for all threads to complete
    // In a full implementation, would synchronize thread pool
}

qboolean VK_MultithreadedRendering_IsEnabled(void) {
    return vk_multithreaded_rendering.enabled && vk_multithreaded_rendering.initialized;
}

void VK_MultithreadedRendering_SetEnabled(qboolean enabled) {
    vk_multithreaded_rendering.enabled = enabled;
}

void VK_MultithreadedRendering_SetFrustumCullingEnabled(qboolean enabled) {
    vk_multithreaded_rendering.frustumCullingEnabled = enabled;
}

int VK_MultithreadedRendering_GetThreadCount(void) {
    return vk_multithreaded_rendering.numThreads;
}

// Dedicated Rendering Threads Implementation

qboolean VK_MultithreadedRendering_EnableDedicatedThread(vk_render_thread_type_t threadType) {
    if (threadType >= RENDER_THREAD_MAX || !vk_multithreaded_rendering.initialized) {
        return qfalse;
    }

    if (vk_multithreaded_rendering.dedicatedThreads[threadType]) {
        // Already enabled
        return qtrue;
    }

    // Allocate thread data
    vk_thread_data_t* thread = (vk_thread_data_t*)ri.Hunk_AllocateTempMemory(sizeof(vk_thread_data_t));
    memset(thread, 0, sizeof(vk_thread_data_t));

    thread->threadIndex = threadType;
    thread->threadType = threadType;
    thread->active = qtrue;
    thread->should_exit = qfalse;
    thread->work_queue_size = 256; // Configurable queue size
    thread->work_queue_head = 0;
    thread->work_queue_tail = 0;
    atomic_init(&thread->work_available_count, 0);

    // Initialize synchronization primitives
    SpinLock_Init(&thread->queue_lock);
    MUTEX_INIT(thread->work_mutex);
    CONDITION_INIT(thread->work_available);

    // Allocate work queue
    thread->work_queue = (void**)ri.Hunk_AllocateTempMemory(
        thread->work_queue_size * sizeof(void*));
    memset(thread->work_queue, 0, thread->work_queue_size * sizeof(void*));

    // Create command pool for this thread
    VkCommandPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = vk.queue_family_index,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
    };

    if (qvkCreateCommandPool(vk.device, &poolInfo, NULL, &thread->commandPool) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create command pool for dedicated thread %d\n", threadType);
        ri.Hunk_FreeTempMemory(thread->work_queue);
        ri.Hunk_FreeTempMemory(thread);
        return qfalse;
    }

    // Start the thread
    const char* threadNames[RENDER_THREAD_MAX] = {
        "VK_Geometry",
        "VK_Lighting",
        "VK_Shadows",
        "VK_PostProcess",
        "VK_CommandGen",
        "VK_AssetLoading",
        "VK_Compute"
    };

    if (!Thread_Create(&thread->handle, DedicatedThreadWorker, thread, threadNames[threadType], THREAD_PRIORITY_HIGH)) {
        ri.Printf(PRINT_ERROR, "Failed to create dedicated thread %d\n", threadType);
        qvkDestroyCommandPool(vk.device, thread->commandPool, NULL);
        ri.Hunk_FreeTempMemory(thread->work_queue);
        ri.Hunk_FreeTempMemory(thread);
        return qfalse;
    }

    vk_multithreaded_rendering.dedicatedThreads[threadType] = thread;
    vk_multithreaded_rendering.dedicatedThreadEnabled[threadType] = qtrue;

    ri.Printf(PRINT_ALL, "Enabled dedicated %s thread\n", threadNames[threadType]);
    return qtrue;
}

void VK_MultithreadedRendering_DisableDedicatedThread(vk_render_thread_type_t threadType) {
    if (threadType >= RENDER_THREAD_MAX || !vk_multithreaded_rendering.dedicatedThreads[threadType]) {
        return;
    }

    vk_thread_data_t* thread = vk_multithreaded_rendering.dedicatedThreads[threadType];

    // Signal thread to exit
    MUTEX_LOCK(thread->work_mutex);
    thread->should_exit = qtrue;
    CONDITION_SIGNAL(thread->work_available);
    MUTEX_UNLOCK(thread->work_mutex);

    // Wait for thread to finish
    Thread_Join(thread->handle);

    // Cleanup resources
    if (thread->commandPool != VK_NULL_HANDLE) {
        qvkDestroyCommandPool(vk.device, thread->commandPool, NULL);
    }

    if (thread->work_queue) {
        ri.Hunk_FreeTempMemory(thread->work_queue);
    }

    ri.Hunk_FreeTempMemory(thread);
    vk_multithreaded_rendering.dedicatedThreads[threadType] = NULL;
    vk_multithreaded_rendering.dedicatedThreadEnabled[threadType] = qfalse;
}

qboolean VK_MultithreadedRendering_IsDedicatedThreadEnabled(vk_render_thread_type_t threadType) {
    if (threadType >= RENDER_THREAD_MAX) return qfalse;
    return vk_multithreaded_rendering.dedicatedThreadEnabled[threadType];
}

// Work submission functions
static void SubmitWorkToThread(vk_render_thread_type_t threadType, void* workData,
                              void (*workFunction)(void*)) {
    if (threadType >= RENDER_THREAD_MAX ||
        !vk_multithreaded_rendering.dedicatedThreadEnabled[threadType]) {
        // Execute immediately if no dedicated thread
        workFunction(workData);
        return;
    }

    vk_thread_data_t* thread = vk_multithreaded_rendering.dedicatedThreads[threadType];

    // Create work item
    vk_thread_work_item_t* workItem = (vk_thread_work_item_t*)ri.Hunk_AllocateTempMemory(
        sizeof(vk_thread_work_item_t));
    workItem->work_function = workFunction;
    workItem->work_data = workData;
    workItem->submit_time = ri.Microseconds() * 1000;

    // Add to queue
    SpinLock_Lock(&thread->queue_lock);

    int available_count = atomic_load_explicit(&thread->work_available_count, memory_order_relaxed);
    if (available_count >= thread->work_queue_size) {
        // Queue full - execute immediately as fallback
        SpinLock_Unlock(&thread->queue_lock);
        workFunction(workData);
        ri.Hunk_FreeTempMemory(workItem);
        return;
    }

    int tail = atomic_load_explicit(&thread->work_queue_tail, memory_order_relaxed);
    thread->work_queue[tail] = workItem;
    atomic_store_explicit(&thread->work_queue_tail,
                        (tail + 1) % thread->work_queue_size, memory_order_relaxed);
    atomic_fetch_add_explicit(&thread->work_available_count, 1, memory_order_relaxed);

    SpinLock_Unlock(&thread->queue_lock);

    // Signal thread
    MUTEX_LOCK(thread->work_mutex);
    CONDITION_SIGNAL(thread->work_available);
    MUTEX_UNLOCK(thread->work_mutex);
}

void VK_MultithreadedRendering_SubmitGeometryWork(void* workData) {
    SubmitWorkToThread(RENDER_THREAD_GEOMETRY, workData, (void (*)(void*))workData); // Placeholder
}

void VK_MultithreadedRendering_SubmitLightingWork(void* workData) {
    SubmitWorkToThread(RENDER_THREAD_LIGHTING, workData, (void (*)(void*))workData); // Placeholder
}

void VK_MultithreadedRendering_SubmitShadowWork(void* workData) {
    SubmitWorkToThread(RENDER_THREAD_SHADOWS, workData, (void (*)(void*))workData); // Placeholder
}

void VK_MultithreadedRendering_SubmitPostProcessWork(void* workData) {
    SubmitWorkToThread(RENDER_THREAD_POST_PROCESS, workData, (void (*)(void*))workData); // Placeholder
}

void VK_MultithreadedRendering_SubmitCommandGenWork(void* workData) {
    SubmitWorkToThread(RENDER_THREAD_COMMAND_GEN, workData, (void (*)(void*))workData); // Placeholder
}

void VK_MultithreadedRendering_SubmitAssetLoadingWork(void* workData) {
    SubmitWorkToThread(RENDER_THREAD_ASSET_LOADING, workData, (void (*)(void*))workData); // Placeholder
}

void VK_MultithreadedRendering_SubmitComputeWork(void* workData) {
    SubmitWorkToThread(RENDER_THREAD_COMPUTE, workData, (void (*)(void*))workData); // Placeholder
}

void VK_MultithreadedRendering_WaitForAllThreads(void) {
    for (int i = 0; i < RENDER_THREAD_MAX; i++) {
        if (vk_multithreaded_rendering.dedicatedThreadEnabled[i]) {
            VK_MultithreadedRendering_WaitForThread((vk_render_thread_type_t)i);
        }
    }
}

void VK_MultithreadedRendering_WaitForThread(vk_render_thread_type_t threadType) {
    if (threadType >= RENDER_THREAD_MAX ||
        !vk_multithreaded_rendering.dedicatedThreadEnabled[threadType]) {
        return;
    }

    vk_thread_data_t* thread = vk_multithreaded_rendering.dedicatedThreads[threadType];

    // Wait until work queue is empty
    while (atomic_load_explicit(&thread->work_available_count, memory_order_relaxed) > 0) {
        Thread_Yield();
    }
}

void VK_MultithreadedRendering_GetThreadStats(vk_render_thread_type_t threadType,
                                             uint64_t* processedItems,
                                             float* avgTimeMs,
                                             uint64_t* waitTimeNs) {
    if (threadType >= RENDER_THREAD_MAX ||
        !vk_multithreaded_rendering.dedicatedThreadEnabled[threadType]) {
        if (processedItems) *processedItems = 0;
        if (avgTimeMs) *avgTimeMs = 0.0f;
        if (waitTimeNs) *waitTimeNs = 0;
        return;
    }

    vk_thread_data_t* thread = vk_multithreaded_rendering.dedicatedThreads[threadType];
    if (processedItems) *processedItems = thread->total_work_items_processed;
    if (avgTimeMs) *avgTimeMs = thread->average_work_time_ms;
    if (waitTimeNs) *waitTimeNs = thread->total_execution_time_ns;
}
