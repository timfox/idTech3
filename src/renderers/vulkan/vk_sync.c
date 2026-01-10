#include "vk_sync.h"
#include "../renderercommon/tr_public.h"
#include "vk.h"

// Renderer interface
extern refimport_t ri;

// Vulkan function pointer extern declarations
extern PFN_vkCreateSemaphore qvkCreateSemaphore;
extern PFN_vkDestroySemaphore qvkDestroySemaphore;
extern PFN_vkCreateFence qvkCreateFence;
extern PFN_vkDestroyFence qvkDestroyFence;
extern PFN_vkWaitForFences qvkWaitForFences;
extern PFN_vkResetFences qvkResetFences;

// Utility functions
// Com_Memcpy and Com_Memset are defined in q_shared.h

// Synchronization primitives creation and destruction
void vk_create_sync_primitives(void) {
    VkSemaphoreCreateInfo desc;
    VkFenceCreateInfo fence_desc;
    uint32_t i;

    desc.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    desc.pNext = NULL;
    desc.flags = 0;

#ifdef USE_UPLOAD_QUEUE
    VK_CHECK(qvkCreateSemaphore(vk.device, &desc, NULL, &vk.image_uploaded2));
#endif

    // Create semaphores and fences for each command buffer
    for (i = 0; i < NUM_COMMAND_BUFFERS; i++) {
        // Image acquired semaphore
        VK_CHECK(qvkCreateSemaphore(vk.device, &desc, NULL, &vk.tess[i].image_acquired));

#ifdef USE_UPLOAD_QUEUE
        // Additional semaphore for upload queue synchronization
        VK_CHECK(qvkCreateSemaphore(vk.device, &desc, NULL, &vk.tess[i].rendering_finished2));
#endif

        // Rendering finished fence
        fence_desc.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_desc.pNext = NULL;
        fence_desc.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Start signaled so first frame can use it

        VK_CHECK(qvkCreateFence(vk.device, &fence_desc, NULL, &vk.tess[i].rendering_finished_fence));
        vk.tess[i].waitForFence = qfalse;
        vk.tess[i].swapchain_image_acquired = qfalse;
    }

#ifdef USE_UPLOAD_QUEUE
    // Auxiliary fence for upload operations
    fence_desc.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_desc.pNext = NULL;
    fence_desc.flags = 0; // Not signaled initially

    VK_CHECK(qvkCreateFence(vk.device, &fence_desc, NULL, &vk.aux_fence));
#endif

    ri.Printf(PRINT_ALL, "Vulkan: Created synchronization primitives\n");
}

// Reset GPU timing for new frame
void vk_reset_gpu_timing(uint32_t frame_index) {
    if (!gpu_timing.initialized || frame_index >= NUM_COMMAND_BUFFERS) {
        return;
    }

    if (gpu_timing.query_pools[frame_index] != VK_NULL_HANDLE && vk.cmd && vk.cmd->command_buffer) {
        // Reset query pool at start of frame
        qvkCmdResetQueryPool(vk.cmd->command_buffer, gpu_timing.query_pools[frame_index], 0, MAX_GPU_TIMING_REGIONS * 2);
    }

    // Reset region tracking
    gpu_timing.region_count[frame_index] = 0;
    gpu_timing.next_query_index[frame_index] = 0;
    Com_Memset(gpu_timing.regions[frame_index], 0, sizeof(gpu_timing.regions[frame_index]));
}

void vk_destroy_sync_primitives(void) {
    vk_shutdown_gpu_timing();
    uint32_t i;

#ifdef USE_UPLOAD_QUEUE
    qvkDestroySemaphore(vk.device, vk.image_uploaded2, NULL);
#endif

    for (i = 0; i < NUM_COMMAND_BUFFERS; i++) {
        qvkDestroySemaphore(vk.device, vk.tess[i].image_acquired, NULL);
#ifdef USE_UPLOAD_QUEUE
        qvkDestroySemaphore(vk.device, vk.tess[i].rendering_finished2, NULL);
#endif
        qvkDestroyFence(vk.device, vk.tess[i].rendering_finished_fence, NULL);
        vk.tess[i].waitForFence = qfalse;
        vk.tess[i].swapchain_image_acquired = qfalse;
    }

#ifdef USE_UPLOAD_QUEUE
    qvkDestroyFence(vk.device, vk.aux_fence, NULL);
    vk.rendering_finished = VK_NULL_HANDLE;
    vk.image_uploaded = VK_NULL_HANDLE;
#endif
// Destroy timeline semaphore if present
#ifdef VK_KHR_TIMELINE_SEMAPHORE
    if (vk.timeline_semaphore != VK_NULL_HANDLE) {
        qvkDestroySemaphore(vk.device, vk.timeline_semaphore, NULL);
        vk.timeline_semaphore = VK_NULL_HANDLE;
    }
#endif

    ri.Printf(PRINT_ALL, "Vulkan: Destroyed synchronization primitives\n");
}

// Fence and semaphore utilities
void vk_wait_for_frame_fences(uint32_t frame_index) {
    if (frame_index < NUM_COMMAND_BUFFERS && vk.tess[frame_index].waitForFence) {
        qvkWaitForFences(vk.device, 1, &vk.tess[frame_index].rendering_finished_fence, VK_TRUE, UINT64_MAX);
        vk.tess[frame_index].waitForFence = qfalse;
    }
}

qboolean vk_is_frame_complete(uint32_t frame_index) {
    if (frame_index >= NUM_COMMAND_BUFFERS) {
        return qtrue;
    }

    VkResult result = qvkWaitForFences(vk.device, 1, &vk.tess[frame_index].rendering_finished_fence, VK_TRUE, 0);
    return result == VK_SUCCESS;
}

// Timeline semaphore operations (VK_KHR_timeline_semaphore) - framework
__attribute__((unused)) void vk_timeline_wait(uint64_t value) {
#ifdef VK_KHR_TIMELINE_SEMAPHORE
    if (qvkWaitSemaphoresKHR && vk.timeline_semaphore != VK_NULL_HANDLE) {
        VkSemaphoreWaitInfoKHR waitInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &vk.timeline_semaphore,
            .pValues = &value
        };
        VkResult res = qvkWaitSemaphoresKHR(vk.device, &waitInfo, UINT64_MAX);
        if (res != VK_SUCCESS) {
            ri.Printf(PRINT_WARNING, "Vulkan: timeline wait failed: %s\n", vk_result_string(res));
        }
    } else {
        ri.Printf(PRINT_DEVELOPER, "Vulkan: timeline wait not available (no function pointer or semaphore)\n");
    }
#else
    ri.Printf(PRINT_DEVELOPER, "Vulkan: timeline wait requested but VK_KHR_TIMELINE_SEMAPHORE not defined\n");
#endif
}

__attribute__((unused)) void vk_timeline_signal(uint64_t value) {
#ifdef VK_KHR_TIMELINE_SEMAPHORE
    if (qvkSignalSemaphoreKHR && vk.timeline_semaphore != VK_NULL_HANDLE) {
        VkSemaphoreSignalInfoKHR signalInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO_KHR,
            .pNext = nullptr,
            .semaphore = vk.timeline_semaphore,
            .value = value
        };
        VkResult res = qvkSignalSemaphoreKHR(vk.device, &signalInfo);
        if (res != VK_SUCCESS) {
            ri.Printf(PRINT_WARNING, "Vulkan: timeline signal failed: %s\n", vk_result_string(res));
        }
    } else {
        ri.Printf(PRINT_DEVELOPER, "Vulkan: timeline signal not available (no function pointer or semaphore)\n");
    }
#else
    ri.Printf(PRINT_DEVELOPER, "Vulkan: timeline signal requested but VK_KHR_TIMELINE_SEMAPHORE not defined\n");
#endif
}

// Synchronization2 operations (VK_KHR_synchronization2) - Vulkan 1.4 core
void vk_sync2_pipeline_barrier(const VkDependencyInfo *dependency_info) {
    if (!qvkCmdPipelineBarrier2KHR) {
        ri.Printf(PRINT_ERROR, "Vulkan: Synchronization2 not available\n");
        return;
    }

    qvkCmdPipelineBarrier2KHR(vk.cmd->command_buffer, dependency_info);
    ri.Printf(PRINT_DEVELOPER, "Vulkan: Synchronization2 pipeline barrier executed\n");
}

// Frame timing and performance counters
void vk_update_frame_timing(void) {
    // Basic frame timing update
    static uint32_t frame_count = 0;
    static uint64_t last_time_ns = 0;

    frame_count++;

    // Compute frame duration using wall-clock time (milliseconds) as a fallback
    uint64_t now_ms = (uint64_t)ri.Milliseconds();
    uint64_t now_ns = now_ms * 1000000ULL;
    double frame_time_ms = 0.0;
    if (last_time_ns != 0) {
        uint64_t delta_ns = now_ns - last_time_ns;
        frame_time_ms = (double)delta_ns / 1000000.0;
    } else {
        frame_time_ms = 16.67; // assume ~60fps for first frame
    }
    last_time_ns = now_ns;

    // Update per-frame timing table in the profiler (circular buffer)
    if (vk.render_profiler.frame_history && vk.render_profiler.max_frames > 0) {
        uint32_t idx = (uint32_t)vk.render_profiler.current_frame_index;
        if (idx >= vk.render_profiler.max_frames) idx = 0;
        vk_frame_profile_t *frame = &vk.render_profiler.frame_history[idx];
        frame->frame_number = vk.render_profiler.total_frames_recorded;
        frame->frame_time_ms = frame_time_ms;
        // CPU time left as 0 for now; GPU time may be filled when/if queries are active
        frame->cpu_time_ms = 0.0;
        frame->gpu_time_ms = 0.0;
        frame->passes = frame->passes; // keep existing pointer
    }

    // Advance circular frame index and total frame counter
    atomic_fetch_add_explicit(&vk.render_profiler.current_frame_index, 1, memory_order_relaxed);
    atomic_fetch_add_explicit(&vk.render_profiler.total_frames_recorded, 1, memory_order_relaxed);
}

float vk_get_frame_time(void) {
    // Return the most recently recorded frame time, if available
    if (vk.render_profiler.frame_history && vk.render_profiler.max_frames > 0) {
        uint32_t idx = (uint32_t)((vk.render_profiler.current_frame_index + vk.render_profiler.max_frames - 1) % vk.render_profiler.max_frames);
        double t = vk.render_profiler.frame_history[idx].frame_time_ms;
        if (t > 0.0) return (float)t;
    }
    return 16.67f;
}

float vk_get_average_fps(void) {
    // Calculate FPS over the available frame history
    if (!vk.render_profiler.frame_history || vk.render_profiler.max_frames == 0) {
        return 60.0f;
    }
    uint32_t max_frames = vk.render_profiler.max_frames;
    uint32_t total = (uint32_t)vk.render_profiler.total_frames_recorded;
    if (total == 0) return 60.0f;
    uint32_t frames_to_consider = total < max_frames ? total : max_frames;
    double sum = 0.0;
    for (uint32_t i = 0; i < frames_to_consider; i++) {
        uint32_t idx = (uint32_t)((vk.render_profiler.current_frame_index + max_frames - 1 - i) % max_frames);
        sum += vk.render_profiler.frame_history[idx].frame_time_ms;
    }
    double avg = (frames_to_consider > 0) ? (sum / (double)frames_to_consider) : 16.67;
    return (float)(1000.0 / avg);
}

// GPU timing queries - implementation
#define MAX_GPU_TIMING_REGIONS 32

typedef struct {
    const char *name;
    uint32_t query_index;
    qboolean active;
} gpu_timing_region_t;

static struct {
    VkQueryPool query_pools[NUM_COMMAND_BUFFERS];
    gpu_timing_region_t regions[NUM_COMMAND_BUFFERS][MAX_GPU_TIMING_REGIONS];
    uint32_t region_count[NUM_COMMAND_BUFFERS];
    uint32_t next_query_index[NUM_COMMAND_BUFFERS];
    qboolean initialized;
    float timestamp_period; // nanoseconds per timestamp unit
} gpu_timing = {0};

// Forward declarations
extern PFN_vkCreateQueryPool qvkCreateQueryPool;
extern PFN_vkDestroyQueryPool qvkDestroyQueryPool;
extern PFN_vkResetQueryPool qvkResetQueryPool;
extern PFN_vkCmdResetQueryPool qvkCmdResetQueryPool;
extern PFN_vkCmdWriteTimestamp qvkCmdWriteTimestamp;
extern PFN_vkGetQueryPoolResults qvkGetQueryPoolResults;
extern PFN_vkCmdResetQueryPool qvkCmdResetQueryPool;

// Initialize GPU timing query pools
static void vk_init_gpu_timing(void) {
    if (gpu_timing.initialized) {
        return;
    }

    VkQueryPoolCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .queryType = VK_QUERY_TYPE_TIMESTAMP,
        .queryCount = MAX_GPU_TIMING_REGIONS * 2, // begin + end for each region
        .pipelineStatistics = 0
    };

    for (uint32_t i = 0; i < NUM_COMMAND_BUFFERS; i++) {
        VkResult result = qvkCreateQueryPool(vk.device, &createInfo, NULL, &gpu_timing.query_pools[i]);
        if (result != VK_SUCCESS) {
            ri.Printf(PRINT_WARNING, "vk_init_gpu_timing: Failed to create query pool %u: %s\n", i, vk_result_string(result));
            gpu_timing.query_pools[i] = VK_NULL_HANDLE;
            continue;
        }
        gpu_timing.region_count[i] = 0;
        gpu_timing.next_query_index[i] = 0;
    }

    // Get timestamp period from physical device properties
    VkPhysicalDeviceProperties props;
    extern PFN_vkGetPhysicalDeviceProperties qvkGetPhysicalDeviceProperties;
    qvkGetPhysicalDeviceProperties(vk.physical_device, &props);
    gpu_timing.timestamp_period = props.limits.timestampPeriod;
    gpu_timing.initialized = qtrue;

    ri.Printf(PRINT_DEVELOPER, "vk_init_gpu_timing: Initialized GPU timing (period: %f ns)\n", gpu_timing.timestamp_period);
}

// Cleanup GPU timing query pools
static void vk_shutdown_gpu_timing(void) {
    if (!gpu_timing.initialized) {
        return;
    }

    for (uint32_t i = 0; i < NUM_COMMAND_BUFFERS; i++) {
        if (gpu_timing.query_pools[i] != VK_NULL_HANDLE) {
            qvkDestroyQueryPool(vk.device, gpu_timing.query_pools[i], NULL);
            gpu_timing.query_pools[i] = VK_NULL_HANDLE;
        }
    }

    Com_Memset(&gpu_timing, 0, sizeof(gpu_timing));
}

void vk_begin_gpu_timing(const char *name) {
    if (!gpu_timing.initialized) {
        vk_init_gpu_timing();
    }

    if (!name || !vk.cmd || !vk.cmd->command_buffer) {
        return;
    }

    uint32_t frame_index = vk.cmd->swapchain_image_index % NUM_COMMAND_BUFFERS;
    if (frame_index >= NUM_COMMAND_BUFFERS || gpu_timing.query_pools[frame_index] == VK_NULL_HANDLE) {
        return;
    }

    if (gpu_timing.region_count[frame_index] >= MAX_GPU_TIMING_REGIONS) {
        ri.Printf(PRINT_WARNING, "vk_begin_gpu_timing: Too many timing regions (max %d)\n", MAX_GPU_TIMING_REGIONS);
        return;
    }

    // Find or create region
    gpu_timing_region_t *region = NULL;
    for (uint32_t i = 0; i < gpu_timing.region_count[frame_index]; i++) {
        if (gpu_timing.regions[frame_index][i].name == name && !gpu_timing.regions[frame_index][i].active) {
            region = &gpu_timing.regions[frame_index][i];
            break;
        }
    }

    if (!region) {
        region = &gpu_timing.regions[frame_index][gpu_timing.region_count[frame_index]++];
        region->name = name;
        region->query_index = gpu_timing.next_query_index[frame_index];
        gpu_timing.next_query_index[frame_index] += 2; // begin + end
    }

    region->active = qtrue;

    // Record begin timestamp
    qvkCmdWriteTimestamp(vk.cmd->command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         gpu_timing.query_pools[frame_index], region->query_index);
}

void vk_end_gpu_timing(void) {
    if (!gpu_timing.initialized || !vk.cmd || !vk.cmd->command_buffer) {
        return;
    }

    uint32_t frame_index = vk.cmd->swapchain_image_index % NUM_COMMAND_BUFFERS;
    if (frame_index >= NUM_COMMAND_BUFFERS || gpu_timing.query_pools[frame_index] == VK_NULL_HANDLE) {
        return;
    }

    // Find active region (most recently started)
    gpu_timing_region_t *region = NULL;
    for (uint32_t i = gpu_timing.region_count[frame_index]; i > 0; i--) {
        if (gpu_timing.regions[frame_index][i - 1].active) {
            region = &gpu_timing.regions[frame_index][i - 1];
            break;
        }
    }

    if (!region) {
        return;
    }

    // Record end timestamp
    qvkCmdWriteTimestamp(vk.cmd->command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                         gpu_timing.query_pools[frame_index], region->query_index + 1);
    region->active = qfalse;
}

float vk_get_gpu_timing_result(const char *name) {
    if (!gpu_timing.initialized || !name) {
        return 0.0f;
    }

    // Use the frame that just completed (previous frame)
    uint32_t frame_index = (vk.cmd->swapchain_image_index + NUM_COMMAND_BUFFERS - 1) % NUM_COMMAND_BUFFERS;
    if (frame_index >= NUM_COMMAND_BUFFERS || gpu_timing.query_pools[frame_index] == VK_NULL_HANDLE) {
        return 0.0f;
    }

    // Find region by name
    gpu_timing_region_t *region = NULL;
    for (uint32_t i = 0; i < gpu_timing.region_count[frame_index]; i++) {
        if (gpu_timing.regions[frame_index][i].name == name) {
            region = &gpu_timing.regions[frame_index][i];
            break;
        }
    }

    if (!region) {
        return 0.0f;
    }

    // Retrieve timestamps
    uint64_t timestamps[2];
    VkResult result = qvkGetQueryPoolResults(vk.device, gpu_timing.query_pools[frame_index],
                                              region->query_index, 2,
                                              sizeof(uint64_t) * 2, timestamps,
                                              sizeof(uint64_t),
                                              VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

    if (result != VK_SUCCESS) {
        return 0.0f;
    }

    // Calculate delta and convert to milliseconds
    uint64_t delta = timestamps[1] - timestamps[0];
    float delta_ms = (float)((double)delta * gpu_timing.timestamp_period / 1e6);
    return delta_ms;
}
// Store GPU timing in current frame's profile
void vk_update_gpu_timing_ns(uint64_t gpu_ns) {
#ifdef VK_RENDERER_DEBUG_TIMING
    if (gpu_ns == 0) {
        ri.Printf(PRINT_DEVELOPER, "VK timing: ignoring zero gpu_ns\n");
        return;
    }
#endif
#ifdef VK_RENDERER_DEBUG_TIMING
    ri.Printf(PRINT_DEVELOPER, "VK timing: updating gpu_ns=%llu\n", (unsigned long long)gpu_ns);
#endif
    if (!vk.render_profiler.frame_history || vk.render_profiler.max_frames == 0) {
        return;
    }
    uint32_t idx = (uint32_t)((vk.render_profiler.current_frame_index + vk.render_profiler.max_frames - 1) % vk.render_profiler.max_frames);
    if (idx >= vk.render_profiler.max_frames) return;
    vk_frame_profile_t* frame = &vk.render_profiler.frame_history[idx];
    frame->gpu_time_ms = (float)((double)gpu_ns / 1e6);
}
