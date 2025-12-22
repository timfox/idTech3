#ifndef __VK_SYNC_H__
#define __VK_SYNC_H__

#include <vulkan/vulkan.h>
#include <stdint.h>
#include "../common/q_shared.h"

// Synchronization primitives management
void vk_create_sync_primitives(void);
void vk_destroy_sync_primitives(void);

// Fence and semaphore utilities
void vk_wait_for_frame_fences(uint32_t frame_index);
qboolean vk_is_frame_complete(uint32_t frame_index);

// Timeline semaphore operations (VK_KHR_timeline_semaphore)
__attribute__((unused)) void vk_timeline_wait(uint64_t value);
__attribute__((unused)) void vk_timeline_signal(uint64_t value);

// Synchronization2 operations (VK_KHR_synchronization2)
__attribute__((unused)) void vk_sync2_pipeline_barrier(const VkDependencyInfo *dependency_info);

// Frame timing and performance counters
void vk_update_frame_timing(void);
float vk_get_frame_time(void);
float vk_get_average_fps(void);

// GPU timing queries (if available)
__attribute__((unused)) void vk_begin_gpu_timing(const char *name);
__attribute__((unused)) void vk_end_gpu_timing(void);
__attribute__((unused)) float vk_get_gpu_timing_result(const char *name);

#endif // __VK_SYNC_H__
