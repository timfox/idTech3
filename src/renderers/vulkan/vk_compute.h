#ifndef __VK_COMPUTE_H__
#define __VK_COMPUTE_H__

#include <vulkan/vulkan.h>
#include <stdint.h>
#include "q_shared.h"

#define NUM_COMMAND_BUFFERS 2	// number of command buffers / render semaphores / framebuffer sets

// Async compute queue structure (extracted from Vk_Instance)
typedef struct {
    qboolean supported;
    uint32_t queue_family_index;
    VkQueue queue;
    VkCommandPool command_pool;
    VkCommandBuffer *command_buffers;
    VkFence fences[NUM_COMMAND_BUFFERS];
    uint32_t current_buffer_index;
} vk_compute_queue_t;

// Async compute job structure
typedef struct {
    qboolean active;
    VkCommandBuffer command_buffer;
    VkFence fence;
    uint64_t timeline_value;
    qboolean wait_for_graphics;
} vk_async_compute_job_t;

// Async compute function declarations
qboolean vk_submit_async_compute(VkCommandBuffer cmd_buffer, qboolean wait_for_graphics);
void vk_wait_async_compute(void);
void vk_shutdown_async_compute(void);

#endif // __VK_COMPUTE_H__
