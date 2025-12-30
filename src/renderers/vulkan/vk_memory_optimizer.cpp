/*
=============================================================================
Vulkan Advanced Memory Optimizer
=============================================================================
GPU Memory Compression and Intelligent Allocation Strategies
*/

#include "tr_local.h"
#include <algorithm>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cmath>

#ifdef USE_VULKAN

// External Vulkan objects
extern VkDevice vk_device;
extern VkPhysicalDevice vk_physical_device;

// Vulkan function pointers
extern PFN_vkGetPhysicalDeviceMemoryProperties qvkGetPhysicalDeviceMemoryProperties;
extern PFN_vkAllocateMemory qvkAllocateMemory;
extern PFN_vkFreeMemory qvkFreeMemory;
extern PFN_vkBindBufferMemory qvkBindBufferMemory;
extern PFN_vkBindImageMemory qvkBindImageMemory;
extern PFN_vkCreateBuffer qvkCreateBuffer;
extern PFN_vkDestroyBuffer qvkDestroyBuffer;
extern PFN_vkCreateImage qvkCreateImage;
extern PFN_vkDestroyImage qvkDestroyImage;
extern PFN_vkGetBufferMemoryRequirements qvkGetBufferMemoryRequirements;
extern PFN_vkGetImageMemoryRequirements qvkGetImageMemoryRequirements;

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

// Memory allocation strategy
typedef enum {
    ALLOCATION_STRATEGY_FIRST_FIT = 0,
    ALLOCATION_STRATEGY_BEST_FIT = 1,
    ALLOCATION_STRATEGY_WORST_FIT = 2,
    ALLOCATION_STRATEGY_BUDDY_SYSTEM = 3,
    ALLOCATION_STRATEGY_SLAB = 4,
} vk_allocation_strategy_t;

// Memory pool types
typedef enum {
    MEMORY_POOL_TEXTURES = 0,
    MEMORY_POOL_BUFFERS = 1,
    MEMORY_POOL_RENDER_TARGETS = 2,
    MEMORY_POOL_STAGING = 3,
    MEMORY_POOL_COMPUTE = 4,
} vk_memory_pool_type_t;

// Intelligent memory block structure
typedef struct vk_memory_block_s {
    VkDeviceMemory memory;
    VkDeviceSize offset;
    VkDeviceSize size;
    qboolean free;
    uint32_t memory_type_index;
    vk_memory_pool_type_t pool_type;
    const char* debug_name;

    // Usage tracking
    uint64_t last_used_time;
    uint32_t access_pattern; // 0=random, 1=sequential, 2=streaming
    float fragmentation_factor;

    // Compression info
    vk_memory_compression_t compression_type;
    float compression_ratio;
    VkDeviceSize original_size;
    VkDeviceSize compressed_size;

    struct vk_memory_block_s* next;
    struct vk_memory_block_s* prev;
} vk_memory_block_t;

// Memory pool structure
typedef struct {
    vk_memory_pool_type_t type;
    VkDeviceSize total_size;
    VkDeviceSize used_size;
    VkDeviceSize free_size;
    uint32_t block_count;
    vk_memory_block_t* blocks;
    vk_allocation_strategy_t strategy;

    // Performance tracking
    uint64_t allocation_count;
    uint64_t deallocation_count;
    uint64_t failed_allocations;
    double average_fragmentation;

    // Compression statistics
    VkDeviceSize total_compressed_size;
    VkDeviceSize total_original_size;
    float average_compression_ratio;
} vk_memory_pool_t;

// Advanced memory optimizer structure
typedef struct {
    qboolean initialized;
    VkPhysicalDeviceMemoryProperties memory_properties;

    // Memory pools
    vk_memory_pool_t pools[5]; // One for each pool type

    // Global memory statistics
    VkDeviceSize total_vram_used;
    VkDeviceSize total_vram_available;
    float global_fragmentation_ratio;
    uint32_t total_allocations;
    uint32_t total_deallocations;

    // Intelligent allocation hints
    std::unordered_map<VkBuffer, vk_memory_pool_type_t> buffer_pool_hints;
    std::unordered_map<VkImage, vk_memory_pool_type_t> image_pool_hints;

    // Compression cache
    std::unordered_map<uint64_t, VkDeviceMemory> compression_cache;

    // Defragmentation state
    qboolean defragmentation_enabled;
    float defragmentation_threshold;
    uint64_t last_defragmentation_time;

    // Memory pressure management
    qboolean under_memory_pressure;
    VkDeviceSize memory_pressure_threshold;
    std::vector<void*> evictable_resources;

} vk_memory_optimizer_t;

static vk_memory_optimizer_t memory_optimizer = {qfalse};

// Initialize memory optimizer
qboolean vk_memory_optimizer_init(void) {
    ri.Printf(PRINT_ALL, "Vulkan: Initializing advanced memory optimizer\n");

    // Get memory properties
    qvkGetPhysicalDeviceMemoryProperties(vk_physical_device, &memory_optimizer.memory_properties);

    // Calculate total VRAM
    memory_optimizer.total_vram_available = 0;
    for (uint32_t i = 0; i < memory_optimizer.memory_properties.memoryHeapCount; i++) {
        if (memory_optimizer.memory_properties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            memory_optimizer.total_vram_available += memory_optimizer.memory_properties.memoryHeaps[i].size;
        }
    }

    memory_optimizer.total_vram_used = 0;
    memory_optimizer.global_fragmentation_ratio = 0.0f;
    memory_optimizer.total_allocations = 0;
    memory_optimizer.total_deallocations = 0;

    // Initialize memory pools
    const char* pool_names[] = {"Textures", "Buffers", "RenderTargets", "Staging", "Compute"};

    for (int i = 0; i < 5; i++) {
        vk_memory_pool_t* pool = &memory_optimizer.pools[i];
        pool->type = static_cast<vk_memory_pool_type_t>(i);
        pool->total_size = memory_optimizer.total_vram_available / 8; // 1/8 of VRAM per pool
        pool->used_size = 0;
        pool->free_size = pool->total_size;
        pool->block_count = 0;
        pool->blocks = nullptr;
        pool->strategy = ALLOCATION_STRATEGY_BEST_FIT;
        pool->allocation_count = 0;
        pool->deallocation_count = 0;
        pool->failed_allocations = 0;
        pool->average_fragmentation = 0.0;
        pool->total_compressed_size = 0;
        pool->total_original_size = 0;
        pool->average_compression_ratio = 1.0f;

        ri.Printf(PRINT_DEVELOPER, "Initialized memory pool %s: %.2f MB\n",
                 pool_names[i], pool->total_size / (1024.0 * 1024.0));
    }

    // Configure optimization settings
    memory_optimizer.defragmentation_enabled = qtrue;
    memory_optimizer.defragmentation_threshold = 0.3f; // 30% fragmentation triggers defrag
    memory_optimizer.last_defragmentation_time = ri.Milliseconds();
    memory_optimizer.under_memory_pressure = qfalse;
    memory_optimizer.memory_pressure_threshold = memory_optimizer.total_vram_available * 90 / 100; // 90% usage

    memory_optimizer.initialized = qtrue;
    ri.Printf(PRINT_ALL, "Vulkan: Advanced memory optimizer initialized (%.2f GB VRAM available)\n",
             memory_optimizer.total_vram_available / (1024.0 * 1024.0 * 1024.0));

    return qtrue;
}

// Shutdown memory optimizer
void vk_memory_optimizer_shutdown(void) {
    if (!memory_optimizer.initialized) {
        return;
    }

    ri.Printf(PRINT_ALL, "Vulkan: Shutting down advanced memory optimizer\n");

    // Free all memory blocks
    for (int i = 0; i < 5; i++) {
        vk_memory_pool_t* pool = &memory_optimizer.pools[i];
        vk_memory_block_t* block = pool->blocks;
        while (block) {
            vk_memory_block_t* next = block->next;
            if (block->memory != VK_NULL_HANDLE) {
                qvkFreeMemory(vk_device, block->memory, nullptr);
            }
            ri.Free(block);
            block = next;
        }
        pool->blocks = nullptr;
    }

    // Clear caches
    memory_optimizer.buffer_pool_hints.clear();
    memory_optimizer.image_pool_hints.clear();
    memory_optimizer.compression_cache.clear();
    memory_optimizer.evictable_resources.clear();

    memory_optimizer.initialized = qfalse;
    ri.Printf(PRINT_ALL, "Vulkan: Advanced memory optimizer shutdown complete\n");
}

// Determine optimal memory type for allocation
static uint32_t vk_memory_optimizer_find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties) {
    for (uint32_t i = 0; i < memory_optimizer.memory_properties.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) &&
            (memory_optimizer.memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    ri.Printf(PRINT_WARNING, "Vulkan: Failed to find suitable memory type\n");
    return 0;
}

// Determine optimal pool for resource
static vk_memory_pool_type_t vk_memory_optimizer_determine_pool(VkBuffer buffer, VkImage image) {
    if (buffer != VK_NULL_HANDLE) {
        // Check buffer hints
        auto it = memory_optimizer.buffer_pool_hints.find(buffer);
        if (it != memory_optimizer.buffer_pool_hints.end()) {
            return it->second;
        }

        // Determine by buffer usage
        VkBufferCreateInfo buffer_info;
        // Note: In a real implementation, we'd need to store buffer creation info
        return MEMORY_POOL_BUFFERS;

    } else if (image != VK_NULL_HANDLE) {
        // Check image hints
        auto it = memory_optimizer.image_pool_hints.find(image);
        if (it != memory_optimizer.image_pool_hints.end()) {
            return it->second;
        }

        // Determine by image usage (textures, render targets, etc.)
        VkImageCreateInfo image_info;
        // Note: In a real implementation, we'd need to store image creation info
        return MEMORY_POOL_TEXTURES;
    }

    return MEMORY_POOL_BUFFERS;
}

// Intelligent memory allocation with compression support
VkDeviceMemory vk_memory_optimizer_allocate(VkDeviceSize size, uint32_t memory_type_bits,
                                           VkMemoryPropertyFlags properties,
                                           vk_memory_compression_t compression,
                                           const char* debug_name) {
    if (!memory_optimizer.initialized) {
        // Fallback to direct allocation
        VkMemoryAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = size,
            .memoryTypeIndex = vk_memory_optimizer_find_memory_type(memory_type_bits, properties)
        };

        VkDeviceMemory memory;
        if (qvkAllocateMemory(vk_device, &alloc_info, nullptr, &memory) == VK_SUCCESS) {
            return memory;
        }
        return VK_NULL_HANDLE;
    }

    // Apply compression if requested
    VkDeviceSize original_size = size;
    if (compression != MEMORY_COMPRESSION_NONE) {
        size = vk_memory_optimizer_estimate_compressed_size(size, compression);
    }

    // Find best pool for allocation
    vk_memory_pool_type_t best_pool = MEMORY_POOL_BUFFERS;
    vk_memory_pool_t* pool = nullptr;

    // Simple pool selection - in practice, this would be more sophisticated
    for (int i = 0; i < 5; i++) {
        if (memory_optimizer.pools[i].free_size >= size) {
            best_pool = static_cast<vk_memory_pool_type_t>(i);
            pool = &memory_optimizer.pools[i];
            break;
        }
    }

    if (!pool) {
        ri.Printf(PRINT_WARNING, "Vulkan: No suitable memory pool found for allocation of %.2f KB\n", size / 1024.0);
        memory_optimizer.pools[best_pool].failed_allocations++;
        return VK_NULL_HANDLE;
    }

    // Allocate from pool using selected strategy
    VkDeviceMemory memory = vk_memory_optimizer_allocate_from_pool(pool, size, memory_type_bits, properties, debug_name);

    if (memory != VK_NULL_HANDLE) {
        // Update statistics
        pool->used_size += size;
        pool->free_size -= size;
        pool->allocation_count++;
        memory_optimizer.total_allocations++;
        memory_optimizer.total_vram_used += size;

        // Check memory pressure
        vk_memory_optimizer_check_memory_pressure();

        ri.Printf(PRINT_DEVELOPER, "Memory Optimizer: Allocated %.2f KB in pool %d (%s)\n",
                 size / 1024.0, best_pool, debug_name ? debug_name : "unnamed");
    }

    return memory;
}

// Allocate from specific pool using allocation strategy
static VkDeviceMemory vk_memory_optimizer_allocate_from_pool(vk_memory_pool_t* pool,
                                                            VkDeviceSize size,
                                                            uint32_t memory_type_bits,
                                                            VkMemoryPropertyFlags properties,
                                                            const char* debug_name) {
    switch (pool->strategy) {
        case ALLOCATION_STRATEGY_FIRST_FIT:
            return vk_memory_optimizer_first_fit_allocate(pool, size, memory_type_bits, properties, debug_name);

        case ALLOCATION_STRATEGY_BEST_FIT:
            return vk_memory_optimizer_best_fit_allocate(pool, size, memory_type_bits, properties, debug_name);

        case ALLOCATION_STRATEGY_BUDDY_SYSTEM:
            return vk_memory_optimizer_buddy_allocate(pool, size, memory_type_bits, properties, debug_name);

        default:
            return vk_memory_optimizer_first_fit_allocate(pool, size, memory_type_bits, properties, debug_name);
    }
}

// First-fit allocation strategy
static VkDeviceMemory vk_memory_optimizer_first_fit_allocate(vk_memory_pool_t* pool,
                                                            VkDeviceSize size,
                                                            uint32_t memory_type_bits,
                                                            VkMemoryPropertyFlags properties,
                                                            const char* debug_name) {
    // Look for first free block that fits
    vk_memory_block_t* block = pool->blocks;
    while (block) {
        if (block->free && block->size >= size) {
            // Found suitable block, split if necessary
            return vk_memory_optimizer_split_and_allocate(block, size, memory_type_bits, properties, debug_name);
        }
        block = block->next;
    }

    // No suitable block found, allocate new one
    return vk_memory_optimizer_allocate_new_block(pool, size, memory_type_bits, properties, debug_name);
}

// Best-fit allocation strategy
static VkDeviceMemory vk_memory_optimizer_best_fit_allocate(vk_memory_pool_t* pool,
                                                           VkDeviceSize size,
                                                           uint32_t memory_type_bits,
                                                           VkMemoryPropertyFlags properties,
                                                           const char* debug_name) {
    vk_memory_block_t* best_block = nullptr;
    VkDeviceSize best_size = VK_WHOLE_SIZE;

    // Find smallest block that fits
    vk_memory_block_t* block = pool->blocks;
    while (block) {
        if (block->free && block->size >= size && block->size < best_size) {
            best_block = block;
            best_size = block->size;
        }
        block = block->next;
    }

    if (best_block) {
        return vk_memory_optimizer_split_and_allocate(best_block, size, memory_type_bits, properties, debug_name);
    }

    // No suitable block found, allocate new one
    return vk_memory_optimizer_allocate_new_block(pool, size, memory_type_bits, properties, debug_name);
}

// Split block and allocate
static VkDeviceMemory vk_memory_optimizer_split_and_allocate(vk_memory_block_t* block,
                                                            VkDeviceSize size,
                                                            uint32_t memory_type_bits,
                                                            VkMemoryPropertyFlags properties,
                                                            const char* debug_name) {
    // If block is much larger than needed, split it
    if (block->size > size * 2) {
        VkDeviceSize remaining_size = block->size - size;

        // Create new block for remaining space
        vk_memory_block_t* new_block = static_cast<vk_memory_block_t*>(ri.Malloc(sizeof(vk_memory_block_t)));
        if (new_block) {
            *new_block = *block;
            new_block->offset += size;
            new_block->size = remaining_size;
            new_block->free = qtrue;

            // Insert after current block
            new_block->next = block->next;
            new_block->prev = block;
            if (block->next) block->next->prev = new_block;
            block->next = new_block;
        }
    }

    // Mark block as used
    block->free = qfalse;
    block->last_used_time = ri.Milliseconds();
    block->debug_name = debug_name;

    return block->memory;
}

// Allocate new memory block
static VkDeviceMemory vk_memory_optimizer_allocate_new_block(vk_memory_pool_t* pool,
                                                            VkDeviceSize size,
                                                            uint32_t memory_type_bits,
                                                            VkMemoryPropertyFlags properties,
                                                            const char* debug_name) {
    // Allocate new Vulkan memory
    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = size,
        .memoryTypeIndex = vk_memory_optimizer_find_memory_type(memory_type_bits, properties)
    };

    VkDeviceMemory memory;
    if (qvkAllocateMemory(vk_device, &alloc_info, nullptr, &memory) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }

    // Create new block
    vk_memory_block_t* block = static_cast<vk_memory_block_t*>(ri.Malloc(sizeof(vk_memory_block_t)));
    if (!block) {
        qvkFreeMemory(vk_device, memory, nullptr);
        return VK_NULL_HANDLE;
    }

    block->memory = memory;
    block->offset = 0;
    block->size = size;
    block->free = qfalse;
    block->memory_type_index = alloc_info.memoryTypeIndex;
    block->pool_type = pool->type;
    block->debug_name = debug_name;
    block->last_used_time = ri.Milliseconds();
    block->access_pattern = 0;
    block->fragmentation_factor = 0.0f;
    block->compression_type = MEMORY_COMPRESSION_NONE;
    block->compression_ratio = 1.0f;
    block->original_size = size;
    block->compressed_size = size;

    // Add to pool's block list
    block->next = pool->blocks;
    block->prev = nullptr;
    if (pool->blocks) pool->blocks->prev = block;
    pool->blocks = block;
    pool->block_count++;

    return memory;
}

// Buddy system allocation (simplified)
static VkDeviceMemory vk_memory_optimizer_buddy_allocate(vk_memory_pool_t* pool,
                                                        VkDeviceSize size,
                                                        uint32_t memory_type_bits,
                                                        VkMemoryPropertyFlags properties,
                                                        const char* debug_name) {
    // Find power-of-2 size that fits
    VkDeviceSize alloc_size = 1;
    while (alloc_size < size) alloc_size *= 2;

    // For now, fall back to first-fit - full buddy system implementation would be more complex
    return vk_memory_optimizer_first_fit_allocate(pool, alloc_size, memory_type_bits, properties, debug_name);
}

// Estimate compressed size
static VkDeviceSize vk_memory_optimizer_estimate_compressed_size(VkDeviceSize original_size,
                                                                vk_memory_compression_t compression) {
    float ratio = 1.0f;

    switch (compression) {
        case MEMORY_COMPRESSION_BC1: ratio = 6.0f; break; // DXT1: 8 bytes per 4x4 block (64 bytes -> ~10.67 bytes)
        case MEMORY_COMPRESSION_BC3: ratio = 4.0f; break; // DXT5: 16 bytes per 4x4 block (64 bytes -> 16 bytes)
        case MEMORY_COMPRESSION_BC5: ratio = 2.0f; break; // 3DC Normal: 16 bytes per 4x4 block (64 bytes -> 32 bytes)
        case MEMORY_COMPRESSION_BC6H: ratio = 2.0f; break; // HDR: 16 bytes per 4x4 block (64 bytes -> 32 bytes)
        case MEMORY_COMPRESSION_BC7: ratio = 3.0f; break; // High quality: ~21 bytes per 4x4 block (64 bytes -> ~21.3 bytes)
        case MEMORY_COMPRESSION_ETC2: ratio = 4.0f; break; // Mobile: 8 bytes per 4x4 block (64 bytes -> 16 bytes)
        case MEMORY_COMPRESSION_ASTC: ratio = 6.0f; break; // Adaptive: variable, average ~10.67 bytes per 4x4
        case MEMORY_COMPRESSION_ZSTD: ratio = 2.0f; break; // General purpose: ~2:1 compression
        default: ratio = 1.0f; break;
    }

    return original_size / ratio;
}

// Check memory pressure and trigger cleanup if needed
static void vk_memory_optimizer_check_memory_pressure(void) {
    float usage_ratio = static_cast<float>(memory_optimizer.total_vram_used) /
                       static_cast<float>(memory_optimizer.total_vram_available);

    qboolean was_under_pressure = memory_optimizer.under_memory_pressure;
    memory_optimizer.under_memory_pressure = (usage_ratio > 0.9f); // 90% usage threshold

    if (memory_optimizer.under_memory_pressure && !was_under_pressure) {
        ri.Printf(PRINT_WARNING, "Vulkan: Memory pressure detected (%.1f%% usage), triggering cleanup\n",
                 usage_ratio * 100.0f);
        vk_memory_optimizer_evict_unused_resources();
    }
}

// Evict unused resources under memory pressure
static void vk_memory_optimizer_evict_unused_resources(void) {
    // This would implement LRU eviction of textures, buffers, etc.
    // For now, just log the action
    ri.Printf(PRINT_DEVELOPER, "Memory Optimizer: Evicting unused resources under memory pressure\n");

    // TODO: Implement actual resource eviction
    // - Sort resources by last used time
    // - Evict oldest unused resources
    // - Update memory statistics
}

// Perform defragmentation
void vk_memory_optimizer_defragment(void) {
    if (!memory_optimizer.defragmentation_enabled) return;

    uint64_t current_time = ri.Milliseconds();
    if (current_time - memory_optimizer.last_defragmentation_time < 10000) return; // Throttle to every 10 seconds

    ri.Printf(PRINT_DEVELOPER, "Memory Optimizer: Performing defragmentation\n");

    // Calculate global fragmentation
    VkDeviceSize total_free = 0;
    VkDeviceSize largest_free = 0;

    for (int i = 0; i < 5; i++) {
        vk_memory_pool_t* pool = &memory_optimizer.pools[i];
        vk_memory_block_t* block = pool->blocks;
        VkDeviceSize pool_largest_free = 0;

        while (block) {
            if (block->free) {
                total_free += block->size;
                if (block->size > pool_largest_free) pool_largest_free = block->size;
            }
            block = block->next;
        }

        if (pool_largest_free > largest_free) largest_free = pool_largest_free;

        // Update pool fragmentation
        if (pool->total_size > 0) {
            pool->average_fragmentation = 1.0 - static_cast<double>(pool_largest_free) / pool->total_size;
        }
    }

    // Calculate global fragmentation
    if (memory_optimizer.total_vram_available > 0) {
        VkDeviceSize total_used = memory_optimizer.total_vram_available - total_free;
        memory_optimizer.global_fragmentation_ratio = total_used > 0 ?
            1.0f - static_cast<float>(largest_free) / total_used : 0.0f;
    }

    // Trigger defragmentation if fragmentation is high
    if (memory_optimizer.global_fragmentation_ratio > memory_optimizer.defragmentation_threshold) {
        ri.Printf(PRINT_ALL, "Memory Optimizer: High fragmentation detected (%.1f%%), defragmenting\n",
                 memory_optimizer.global_fragmentation_ratio * 100.0f);

        // TODO: Implement actual defragmentation
        // - Identify movable resources
        // - Move resources to consolidate free space
        // - Update memory mappings
    }

    memory_optimizer.last_defragmentation_time = current_time;
}

// Get memory statistics
void vk_memory_optimizer_get_stats(VkDeviceSize* total_used, VkDeviceSize* total_available,
                                  float* fragmentation_ratio, uint32_t* total_allocations,
                                  float* compression_ratio, uint32_t* pool_usage[5]) {
    if (total_used) *total_used = memory_optimizer.total_vram_used;
    if (total_available) *total_available = memory_optimizer.total_vram_available;
    if (fragmentation_ratio) *fragmentation_ratio = memory_optimizer.global_fragmentation_ratio;
    if (total_allocations) *total_allocations = memory_optimizer.total_allocations;

    // Calculate average compression ratio
    VkDeviceSize total_compressed = 0;
    VkDeviceSize total_original = 0;
    for (int i = 0; i < 5; i++) {
        total_compressed += memory_optimizer.pools[i].total_compressed_size;
        total_original += memory_optimizer.pools[i].total_original_size;
    }

    if (compression_ratio && total_original > 0) {
        *compression_ratio = static_cast<float>(total_original) / static_cast<float>(total_compressed);
    }

    if (pool_usage) {
        for (int i = 0; i < 5; i++) {
            pool_usage[i] = reinterpret_cast<uint32_t*>(&memory_optimizer.pools[i].used_size);
        }
    }
}

// Set allocation hints for better pool selection
void vk_memory_optimizer_set_buffer_hint(VkBuffer buffer, vk_memory_pool_type_t pool_type) {
    memory_optimizer.buffer_pool_hints[buffer] = pool_type;
}

void vk_memory_optimizer_set_image_hint(VkImage image, vk_memory_pool_type_t pool_type) {
    memory_optimizer.image_pool_hints[image] = pool_type;
}

// Memory compression functions
qboolean vk_memory_optimizer_compress_texture(VkImage image, vk_memory_compression_t compression) {
    // TODO: Implement texture compression
    // - Create compressed version of texture
    // - Update memory allocation
    // - Update image view
    ri.Printf(PRINT_DEVELOPER, "Memory Optimizer: Texture compression not yet implemented\n");
    return qfalse;
}

qboolean vk_memory_optimizer_decompress_texture(VkImage image) {
    // TODO: Implement texture decompression
    ri.Printf(PRINT_DEVELOPER, "Memory Optimizer: Texture decompression not yet implemented\n");
    return qfalse;
}

#endif // USE_VULKAN