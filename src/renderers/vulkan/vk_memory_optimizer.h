/*
=============================================================================
Vulkan Advanced Memory Optimizer Header
=============================================================================
GPU Memory Compression and Intelligent Allocation Strategies
*/

#pragma once

#include "tr_local.h"

#ifdef USE_VULKAN

#ifdef __cplusplus
extern "C" {
#endif

// Memory compression types
typedef enum {
    MEMORY_COMPRESSION_NONE = 0,
    MEMORY_COMPRESSION_BC1 = 1,     // DXT1 - 6:1 compression
    MEMORY_COMPRESSION_BC3 = 2,     // DXT5 - 4:1 compression
    MEMORY_COMPRESSION_BC5 = 3,     // 3DC Normal - 2:1 compression
    MEMORY_COMPRESSION_BC6H = 4,    // HDR - 2:1 compression
    MEMORY_COMPRESSION_BC7 = 5,     // High quality - 3:1 compression
    MEMORY_COMPRESSION_ETC2 = 6,    // Mobile - 4:1 compression
    MEMORY_COMPRESSION_ASTC = 7,    // Adaptive - variable compression
    MEMORY_COMPRESSION_ZSTD = 8,    // Generic data compression
} vk_memory_compression_t;

// Memory pool types
typedef enum {
    MEMORY_POOL_TEXTURES = 0,
    MEMORY_POOL_BUFFERS = 1,
    MEMORY_POOL_RENDER_TARGETS = 2,
    MEMORY_POOL_STAGING = 3,
    MEMORY_POOL_COMPUTE = 4,
} vk_memory_pool_type_t;

// Memory optimizer functions
qboolean vk_memory_optimizer_init(void);
void vk_memory_optimizer_shutdown(void);
VkDeviceMemory vk_memory_optimizer_allocate(VkDeviceSize size, uint32_t memory_type_bits,
                                           VkMemoryPropertyFlags properties,
                                           vk_memory_compression_t compression,
                                           const char* debug_name);
void vk_memory_optimizer_defragment(void);
void vk_memory_optimizer_get_stats(VkDeviceSize* total_used, VkDeviceSize* total_available,
                                  float* fragmentation_ratio, uint32_t* total_allocations,
                                  float* compression_ratio, uint32_t* pool_usage[5]);

// Allocation hints
void vk_memory_optimizer_set_buffer_hint(VkBuffer buffer, vk_memory_pool_type_t pool_type);
void vk_memory_optimizer_set_image_hint(VkImage image, vk_memory_pool_type_t pool_type);

// Memory compression functions
qboolean vk_memory_optimizer_compress_texture(VkImage image, vk_memory_compression_t compression);
qboolean vk_memory_optimizer_decompress_texture(VkImage image);

#ifdef __cplusplus
}
#endif

#endif // USE_VULKAN