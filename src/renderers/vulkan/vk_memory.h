#ifndef __VK_MEMORY_H__
#define __VK_MEMORY_H__

#include <vulkan/vulkan.h>
#include <stdint.h>
#include "q_shared.h"
#include "tr_common.h"

// Memory management structures (extracted from Vk_Instance)
typedef struct {
    qboolean enabled;
    float fragmentation_threshold; // Trigger defrag when fragmentation exceeds this (0.0-1.0)
    uint32_t defrag_interval_frames; // Defrag every N frames (0 = disabled)
    uint32_t frame_counter;
    VkDeviceSize total_allocated;
    VkDeviceSize total_used;
    VkDeviceSize largest_free_block;
    uint32_t free_block_count;
} vk_memory_defrag_t;

typedef struct {
    qboolean enabled;
    qboolean sparse_binding_supported;
    VkDeviceSize virtual_address_space_size;
    VkDeviceSize allocated_virtual_size;
    uint32_t sparse_binding_count;
} vk_virtual_memory_t;

typedef struct {
    qboolean enabled;
    struct {
        VkBuffer buffers[64]; // Small buffers (< 1MB)
        VkDeviceMemory memory[64];
        uint32_t count;
        uint32_t free_count;
        uint32_t free_indices[64];
    } small_buffers;
    struct {
        VkBuffer buffers[32]; // Medium buffers (1MB - 16MB)
        VkDeviceMemory memory[32];
        uint32_t count;
        uint32_t free_count;
        uint32_t free_indices[32];
    } medium_buffers;
    struct {
        VkBuffer buffers[16]; // Large buffers (> 16MB)
        VkDeviceMemory memory[16];
        uint32_t count;
        uint32_t free_count;
        uint32_t free_indices[16];
    } large_buffers;
} vk_resource_pool_t;

typedef struct {
    qboolean enabled;
    struct {
        image_t *image;
        float priority; // Higher = more important
        float distance; // View distance
        uint32_t requested_mip_level;
    } queue[256];
    uint32_t queue_count;
    VkDeviceSize memory_bandwidth_used;
    VkDeviceSize memory_bandwidth_limit;
} vk_texture_streaming_t;

// ImageChunk is now defined in vk.h

// Memory management function declarations
void vk_allocate_image_chunk(void);
void vk_calculate_fragmentation_metrics(void);
void vk_check_defragmentation(void);
void vk_init_resource_pool(void);
void vk_shutdown_resource_pool(void);
VkBuffer vk_get_buffer_from_pool(VkDeviceSize size);
void vk_return_buffer_to_pool(VkBuffer buffer);
void vk_alloc_staging_buffer(VkDeviceSize size);
void vk_flush_staging_buffer(qboolean final);
void vk_clean_staging_buffer(void);

#endif // __VK_MEMORY_H__
