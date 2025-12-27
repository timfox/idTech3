/*
=============================================================================
Pixel Buffer Objects (PBO) System
Async texture upload system for improved performance
=============================================================================
*/

#pragma once

#include "tr_local.h"
#include "vk.h"

#ifdef USE_VULKAN

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_PBO_BUFFERS 8
#define MAX_PBO_SIZE (8 * 1024 * 1024)  // 8MB per buffer

// PBO upload job
typedef struct {
    qboolean active;
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkDeviceSize size;
    void *mapped_data;
    qboolean mapped;

    // Upload parameters
    VkImage dst_image;
    VkImageLayout final_layout;
    VkBufferImageCopy region;

    // Completion callback
    void (*completion_callback)(void *user_data);
    void *user_data;
} pbo_upload_job_t;

// PBO system state
typedef struct {
    qboolean initialized;
    qboolean enabled;

    // Staging buffers
    VkBuffer staging_buffers[MAX_PBO_BUFFERS];
    VkDeviceMemory staging_memory[MAX_PBO_BUFFERS];
    VkDeviceSize buffer_sizes[MAX_PBO_BUFFERS];

    // Upload jobs
    pbo_upload_job_t upload_jobs[MAX_PBO_BUFFERS];
    int next_buffer_index;

    // Transfer queue (if available)
    VkQueue transfer_queue;
    uint32_t transfer_queue_family;

    // Command pool and buffers for transfers
    VkCommandPool transfer_command_pool;
    VkCommandBuffer transfer_command_buffers[MAX_PBO_BUFFERS];

    // Synchronization
    VkSemaphore transfer_semaphores[MAX_PBO_BUFFERS];
    VkFence transfer_fences[MAX_PBO_BUFFERS];

    // Statistics
    uint64_t total_uploads;
    uint64_t total_bytes_uploaded;
    float average_upload_time_ms;

} pbo_system_t;

// External API
void vk_pbo_init(void);
void vk_pbo_shutdown(void);

// Texture upload functions
qboolean vk_pbo_upload_texture_async(const void *data, VkDeviceSize size,
                                   VkImage dst_image, VkImageLayout final_layout,
                                   const VkBufferImageCopy *region,
                                   void (*completion_callback)(void *user_data),
                                   void *user_data);

qboolean vk_pbo_upload_texture_sync(const void *data, VkDeviceSize size,
                                  VkImage dst_image, VkImageLayout final_layout,
                                  const VkBufferImageCopy *region);

// Utility functions
qboolean vk_pbo_is_available(void);
void vk_pbo_wait_all_uploads(void);
void vk_pbo_update(void);

// Statistics
void vk_pbo_get_stats(uint64_t *total_uploads, uint64_t *total_bytes, float *avg_time);

// CVars
extern cvar_t *r_pbo;
extern cvar_t *r_pboBuffers;
extern cvar_t *r_pboAsync;

#ifdef __cplusplus
}
#endif

#endif // USE_VULKAN