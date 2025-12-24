#ifndef __VK_MEMORY_H__
#define __VK_MEMORY_H__

#include <vulkan/vulkan.h>
#include <stdint.h>
#include "q_shared.h"
#include "tr_common.h"

// Forward declarations for imGUI types if needed
#ifdef USE_CIMGUI
// We avoid including cimgui.h in the header to prevent type conflicts
// in non-Vulkan files that include this header via vk.h.
#endif

#ifdef __cplusplus
extern "C" {
#endif

// GPU Memory allocation tracking entry
typedef struct vk_memory_allocation_s {
    VkDeviceMemory memory;
    VkDeviceSize size;
    uint32_t memory_type;
    const char *resource_name;
    const char *allocation_site;  // File and line where allocated
    uint32_t allocation_id;      // Unique ID for tracking
    qboolean is_freed;
} vk_memory_allocation_t;

// VRAM usage statistics
typedef struct {
    qboolean enabled;
    VkDeviceSize total_vram;           // Total GPU memory
    VkDeviceSize used_vram;            // Currently allocated GPU memory
    VkDeviceSize available_vram;       // Available GPU memory
    VkDeviceSize max_used_vram;        // Peak memory usage
    VkDeviceSize active_memory_usage;  // CURRENTLY USED
    VkDeviceSize peak_memory_usage;    // PEAK USED

    // Allocation statistics
    atomic_uint_t total_allocations;        // Total allocation calls
    atomic_uint_t current_allocations;      // Current active allocations
    atomic_uint_t freed_allocations;        // Total deallocation calls
    atomic_uint_t leaked_allocations;       // Detected memory leaks
    atomic_uint_t memory_leaks_detected;    // FOR HUD COMPATIBILITY

    // Resource type breakdown
    VkDeviceSize image_memory;         // Memory used by images
    VkDeviceSize buffer_memory;        // Memory used by buffers
    VkDeviceSize other_memory;         // Memory used by other resources

    // Memory type usage (for each Vulkan memory type)
    VkDeviceSize memory_type_usage[VK_MAX_MEMORY_TYPES];
} vk_vram_stats_t;

// Memory leak detection
#define VK_MAX_MEMORY_ALLOCATIONS 8192
typedef struct {
    vk_memory_allocation_t allocations[VK_MAX_MEMORY_ALLOCATIONS];
    atomic_uint_t allocation_count;
    atomic_uint_t next_allocation_id;
    qboolean leak_detection_enabled;
} vk_memory_tracker_t;

// Legacy memory stats (keep for compatibility)
typedef struct {
    atomic_uint_t allocations;
    atomic_uint_t frees;
    atomic_uint_t current_allocations;
    atomic_uint64_t total_allocated_bytes;
    atomic_uint64_t total_freed_bytes;
} vk_memory_stats_t;

extern vk_memory_stats_t vk_memory_stats;
extern vk_vram_stats_t vk_vram_stats;
extern vk_memory_tracker_t vk_memory_tracker;

// Memory allocation entry for defragmentation tracking
typedef struct vk_memory_block_s {
    VkImage image;              // Associated image (VK_NULL_HANDLE for free blocks)
    VkDeviceSize offset;        // Offset within the memory chunk
    VkDeviceSize size;          // Size of this block
    VkDeviceSize alignment;     // Alignment requirement
    qboolean is_free;           // Whether this block is free
    uint32_t allocation_id;     // Unique ID for tracking
    const char *resource_name;  // Name for debugging
} vk_memory_block_t;

// Memory chunk information for defragmentation
typedef struct vk_memory_chunk_s {
    VkDeviceMemory memory;      // Vulkan memory handle
    VkDeviceSize size;          // Total size of chunk
    uint32_t memory_type;       // Memory type index
    vk_memory_block_t *blocks;  // Array of memory blocks
    uint32_t block_count;       // Number of blocks
    uint32_t max_blocks;        // Maximum blocks (for array management)
    VkDeviceSize used_size;     // Currently used size
    VkDeviceSize free_size;     // Currently free size
} vk_memory_chunk_t;

// Memory management structures (extracted from Vk_Instance)
typedef struct {
    qboolean enabled;
    float fragmentation_threshold; // Trigger defrag when fragmentation exceeds this (0.0-1.0)
    uint32_t defrag_interval_frames; // Defrag every N frames (0 = disabled)
    atomic_uint_t frame_counter;

    // Statistics
    atomic_uint64_t total_allocated;
    atomic_uint64_t total_used;
    atomic_uint64_t largest_free_block;
    atomic_uint_t free_block_count;

    // Defragmentation state
    qboolean defrag_in_progress;
    atomic_uint_t chunks_to_defrag;     // Number of chunks that need defragmentation
    atomic_uint_t defrag_operations;    // Total defrag operations performed

    // Vulkan memory defragmentation support
    qboolean vk_defrag_supported;  // Whether VK_EXT_memory_defragmentation is available
    PFN_vkCreateDeferredOperationKHR vkCreateDeferredOperationKHR;
    PFN_vkDeferredOperationJoinKHR vkDeferredOperationJoinKHR;
    PFN_vkGetDeferredOperationResultKHR vkGetDeferredOperationResultKHR;
    PFN_vkDestroyDeferredOperationKHR vkDestroyDeferredOperationKHR;
} vk_memory_defrag_t;

typedef struct {
    qboolean enabled;
    qboolean sparse_binding_supported;
    VkDeviceSize virtual_address_space_size;
    VkDeviceSize allocated_virtual_size;
    uint32_t sparse_binding_count;
} vk_virtual_memory_t;

// Lock-free Memory Allocators

// Lock-free fixed-size allocator for small allocations
typedef struct vk_lock_free_allocator_s {
    void *memory_pool;                    // Base memory pool
    VkDeviceSize pool_size;               // Total pool size
    VkDeviceSize block_size;              // Size of each block
    uint32_t total_blocks;                // Total number of blocks
    atomic_uint_t free_blocks;            // Number of free blocks (atomic)

    // Lock-free free list using atomic operations
    volatile uintptr_t *free_list;        // Array of free block pointers (atomic)
    atomic_uint_t free_list_head;         // Head of free list (atomic)
    atomic_uint_t free_list_tail;         // Tail of free list (atomic)

    // Statistics
    atomic_uint64_t allocations;          // Total allocations (atomic)
    atomic_uint64_t deallocations;        // Total deallocations (atomic)
    atomic_uint64_t contended_allocs;     // Contended allocation attempts (atomic)

    // Thread safety
    qboolean is_thread_safe;              // Whether this allocator is thread-safe
    const char *debug_name;               // Debug name for tracking
} vk_lock_free_allocator_t;

// Lock-free memory manager for coordinating multiple allocators
typedef struct vk_lock_free_memory_manager_s {
    qboolean enabled;
    qboolean initialized;

    // Multiple allocator pools for different sizes
    vk_lock_free_allocator_t small_allocator;    // 16-64 bytes
    vk_lock_free_allocator_t medium_allocator;   // 64-256 bytes
    vk_lock_free_allocator_t large_allocator;    // 256-1024 bytes

    // Statistics
    atomic_uint64_t total_allocations;
    atomic_uint64_t total_deallocations;
    atomic_uint64_t cache_hits;
    atomic_uint64_t cache_misses;
    atomic_uint64_t lock_contention_events;

    // Performance monitoring
    atomic_uint64_t allocation_time_ns;
    atomic_uint64_t deallocation_time_ns;

    // Configuration
    VkDeviceSize small_pool_size;         // Default 4MB
    VkDeviceSize medium_pool_size;        // Default 8MB
    VkDeviceSize large_pool_size;         // Default 16MB

    uint32_t small_block_size;            // Default 64 bytes
    uint32_t medium_block_size;           // Default 256 bytes
    uint32_t large_block_size;            // Default 1024 bytes
} vk_lock_free_memory_manager_t;

// Arena Allocators for Scoped Memory Management

// Memory arena for scoped allocation
typedef struct vk_memory_arena_s {
    void *memory;              // Base memory block
    VkDeviceSize size;         // Total size of arena
    atomic_uint64_t used;      // Currently used bytes
    atomic_uint64_t peak_used; // Peak usage for statistics
    VkDeviceSize alignment;    // Default alignment

    // Allocation tracking for debugging
    atomic_uint_t allocation_count; // Number of allocations
    qboolean is_active;        // Whether arena is active
    const char *name;          // Debug name

    // Parent arena for hierarchical arenas
    struct vk_memory_arena_s *parent;
} vk_memory_arena_t;

// Arena manager for coordinating multiple arenas
typedef struct vk_arena_manager_s {
    qboolean enabled;
    qboolean initialized;

    // Pre-allocated arenas for common use cases
    vk_memory_arena_t frame_arena;        // Reset every frame
    vk_memory_arena_t render_arena;       // Reset between scenes
    vk_memory_arena_t asset_arena;        // Reset on level change
    vk_memory_arena_t persistent_arena;   // Long-lived allocations

    // Dynamic arena pool
    vk_memory_arena_t *dynamic_arenas;
    uint32_t max_dynamic_arenas;
    atomic_uint_t active_dynamic_arenas;

    // Statistics
    atomic_uint64_t total_allocated_bytes;
    atomic_uint64_t total_freed_bytes;
    atomic_uint64_t peak_memory_usage;
    atomic_uint64_t arena_resets;
    atomic_uint64_t allocation_operations;

    // Configuration
    VkDeviceSize frame_arena_size;        // Default 16MB per frame
    VkDeviceSize render_arena_size;       // Default 32MB per scene
    VkDeviceSize asset_arena_size;        // Default 64MB per level
    VkDeviceSize persistent_arena_size;   // Default 128MB persistent
    VkDeviceSize dynamic_arena_size;      // Default 8MB per dynamic arena

    // Frame counter for timing
    atomic_uint64_t current_frame;
} vk_arena_manager_t;

// Memory Advisor - Intelligent Memory Layout Optimization

// Memory access pattern tracking
typedef struct vk_memory_access_s {
    void *memory_address;         // Memory location accessed
    VkDeviceSize access_offset;   // Offset within larger block
    uint64_t access_time;         // Frame/timestamp of access
    uint32_t access_count;        // Number of times accessed
    uint32_t access_pattern;      // Sequential, random, strided
    const char *resource_name;    // Name for debugging
} vk_memory_access_t;

// Memory layout optimization suggestion
typedef enum {
    LAYOUT_OPTIMIZATION_NONE = 0,
    LAYOUT_OPTIMIZATION_DATA_LOCALITY,     // Group frequently accessed data
    LAYOUT_OPTIMIZATION_CACHE_ALIGNMENT,   // Align for cache lines
    LAYOUT_OPTIMIZATION_PRELOADING,       // Prefetch optimization
    LAYOUT_OPTIMIZATION_COMPRESSION,      // Memory compression
    LAYOUT_OPTIMIZATION_REORDERING        // Reorder for better access patterns
} vk_layout_optimization_type_t;

// Optimization recommendation
typedef struct vk_layout_recommendation_s {
    vk_layout_optimization_type_t optimization_type;
    void *target_memory;          // Memory block to optimize
    VkDeviceSize target_size;     // Size of block to optimize
    float expected_improvement;   // Expected performance gain (0.0-1.0)
    uint32_t priority;            // Priority (higher = more important)
    const char *description;      // Human-readable description
    qboolean can_auto_apply;      // Whether this can be applied automatically
} vk_layout_recommendation_t;

// Memory access pattern analyzer
typedef struct vk_access_pattern_analyzer_s {
    vk_memory_access_t *access_log;       // Circular buffer of access patterns
    uint32_t max_access_entries;          // Size of access log
    uint32_t current_access_index;        // Current position in log
    uint32_t total_access_count;          // Total accesses recorded

    // Pattern analysis
    uint64_t sequential_accesses;         // Sequential memory accesses
    uint64_t random_accesses;            // Random memory accesses
    uint64_t strided_accesses;           // Strided access patterns
    uint64_t cache_misses;               // Estimated cache misses

    // Hot/cold data identification
    void **hot_addresses;                // Frequently accessed addresses
    uint32_t max_hot_addresses;
    uint32_t hot_address_count;

    void **cold_addresses;               // Rarely accessed addresses
    uint32_t max_cold_addresses;
    uint32_t cold_address_count;
} vk_access_pattern_analyzer_t;

// Memory layout advisor
typedef struct vk_memory_advisor_s {
    qboolean enabled;
    qboolean initialized;
    qboolean auto_optimization;           // Whether to apply optimizations automatically

    // Pattern analysis
    vk_access_pattern_analyzer_t analyzer;

    // Optimization recommendations
    vk_layout_recommendation_t *recommendations;
    uint32_t max_recommendations;
    atomic_uint_t recommendation_count;

    // Performance metrics
    float cache_hit_rate;                // Estimated cache hit rate
    atomic_uint64_t total_memory_accesses; // Total memory accesses tracked
    uint64_t cache_miss_penalty_ns;      // Estimated cache miss cost

    // Configuration
    uint32_t analysis_window_frames;     // Frames to analyze patterns over
    uint32_t min_accesses_for_hot;       // Minimum accesses to be considered "hot"
    float optimization_threshold;        // Minimum improvement to apply optimization
    qboolean enable_pattern_learning;    // Whether to learn from access patterns

    // Statistics
    atomic_uint64_t optimizations_applied;      // Number of optimizations applied
    atomic_uint64_t performance_improvements;   // Estimated performance gains
    atomic_uint64_t analysis_time_ns;           // Time spent analyzing patterns
} vk_memory_advisor_t;

// Cache-Conscious Data Structures

// Cache line size (typically 64 bytes on modern x86/ARM)
#define CACHE_LINE_SIZE 64
#define CACHE_LINE_MASK (CACHE_LINE_SIZE - 1)

// Cache-aligned allocation
#define CACHE_ALIGNED __attribute__((aligned(CACHE_LINE_SIZE)))

// Cache-conscious dynamic array (cache-friendly vector)
typedef struct vk_cache_array_s {
    void *data;                    // Array data (cache-aligned)
    VkDeviceSize element_size;     // Size of each element
    uint32_t capacity;            // Allocated capacity
    atomic_uint_t size;           // Current size
    uint32_t growth_factor;       // Growth factor for reallocation
    const char *debug_name;       // Debug name
} vk_cache_array_t;

// Cache-conscious hash map with open addressing
typedef struct vk_cache_hash_map_s {
    void *keys;                   // Key array (cache-aligned)
    void *values;                 // Value array (cache-aligned)
    uint8_t *metadata;            // Metadata for each slot (cache-aligned)
    VkDeviceSize key_size;        // Size of keys
    VkDeviceSize value_size;      // Size of values
    uint32_t capacity;            // Total capacity (power of 2)
    atomic_uint_t size;           // Current size
    uint32_t max_load_factor;     // Maximum load factor percentage
    uint32_t (*hash_func)(const void *key, VkDeviceSize key_size); // Hash function
    qboolean (*equals_func)(const void *a, const void *b, VkDeviceSize size); // Equality function
    const char *debug_name;       // Debug name
} vk_cache_hash_map_t;

// Cache-conscious queue (ring buffer optimized for cache)
typedef struct vk_cache_queue_s {
    void *buffer;                 // Ring buffer (cache-aligned)
    VkDeviceSize element_size;    // Size of each element
    uint32_t capacity;            // Buffer capacity (power of 2 for efficiency)
    atomic_uint_t head;           // Read position
    atomic_uint_t tail;           // Write position
    uint32_t mask;                // Capacity mask for efficient modulo
    const char *debug_name;       // Debug name
} vk_cache_queue_t;

// Cache-conscious spatial hash for 2D/3D spatial queries
typedef struct vk_spatial_hash_s {
    vk_cache_array_t **buckets;   // Array of cache-aligned bucket arrays
    uint32_t num_buckets;         // Number of hash buckets
    float cell_size;              // Size of each spatial cell
    uint32_t dimensions;          // 2D or 3D
    VkDeviceSize element_size;    // Size of stored elements
    const char *debug_name;       // Debug name
} vk_spatial_hash_t;

// Cache-conscious entity-component storage
typedef struct vk_entity_storage_s {
    // Entity IDs (dense array for cache efficiency)
    uint32_t *entity_ids;         // Entity IDs (cache-aligned)
    uint32_t entity_capacity;     // Maximum entities
    atomic_uint_t entity_count;   // Current entity count

    // Component arrays (one per component type)
    void **component_arrays;      // Array of component data arrays
    VkDeviceSize *component_sizes; // Size of each component type
    uint32_t max_component_types; // Maximum component types
    uint32_t num_component_types; // Current component types

    // Free list for entity recycling
    uint32_t *free_list;          // Free entity indices
    atomic_uint_t free_count;     // Number of free entities

    const char *debug_name;       // Debug name
} vk_entity_storage_t;

// Cache-conscious render queue (optimized for rendering pipeline)
typedef struct vk_render_queue_s {
    // Command batches (grouped by state to minimize changes)
    vk_cache_array_t command_batches;

    // Material groups (sorted by material for state batching)
    struct {
        uint32_t material_id;
        uint32_t start_batch;
        uint32_t batch_count;
    } *material_groups;

    atomic_uint_t num_material_groups;
    uint32_t material_group_capacity;

    // Statistics
    atomic_uint_t total_commands;
    atomic_uint_t total_draw_calls;
    atomic_uint_t state_changes;

    const char *debug_name;
} vk_render_queue_t;

// Cache-conscious particle system storage
typedef struct vk_particle_storage_s {
    // Particle data (SOA layout for cache efficiency)
    float *positions_x;           // X positions
    float *positions_y;           // Y positions
    float *positions_z;           // Z positions

    float *velocities_x;          // X velocities
    float *velocities_y;          // Y velocities
    float *velocities_z;          // Z velocities

    float *lifetimes;             // Particle lifetimes
    uint32_t *types;              // Particle types
    uint32_t *flags;              // Particle flags

    uint32_t capacity;            // Maximum particles
    atomic_uint_t count;          // Current particles
    atomic_uint_t alive_count;    // Living particles

    // Free list for dead particles
    uint32_t *free_list;
    atomic_uint_t free_count;

    const char *debug_name;
} vk_particle_storage_t;

// Cache-conscious data structures manager
typedef struct vk_cache_structures_manager_s {
    qboolean enabled;
    qboolean initialized;

    // Pre-allocated pools for common data structures
    vk_cache_array_t temp_array_pool[16];     // Temporary arrays
    vk_cache_hash_map_t temp_hash_pool[8];    // Temporary hash maps
    vk_cache_queue_t temp_queue_pool[8];      // Temporary queues

    atomic_uint_t temp_array_count;
    atomic_uint_t temp_hash_count;
    atomic_uint_t temp_queue_count;

    // Performance statistics
    atomic_uint64_t cache_misses_avoided;
    atomic_uint64_t prefetch_operations;
    atomic_uint64_t false_sharing_avoided;
    atomic_uint64_t data_locality_improvements;

    // Memory usage
    atomic_uint64_t total_allocated;
    atomic_uint64_t cache_aligned_allocated;

    const char *debug_name;
} vk_cache_structures_manager_t;

// Render Graph Profiler - Detailed Performance Analysis

// GPU timestamp query for measuring pass execution times
typedef struct vk_timestamp_query_s {
    VkQueryPool query_pool;        // Vulkan query pool
    uint32_t query_count;          // Number of queries in pool
    uint32_t current_query;        // Current query index
    uint64_t *timestamps;          // Timestamp results buffer
    qboolean available;            // Whether results are available
} vk_timestamp_query_t;

// Pipeline statistics for detailed GPU counters
typedef struct vk_pipeline_stats_s {
    VkQueryPool query_pool;        // Pipeline statistics query pool
    VkQueryPipelineStatisticFlags flags; // Enabled statistics flags
    uint32_t query_count;          // Number of statistic queries
    uint64_t *statistics;          // Statistics results buffer
    qboolean available;            // Whether results are available
} vk_pipeline_stats_t;

// Render pass performance data
typedef struct vk_render_pass_profile_s {
    const char *name;              // Pass name for identification
    uint32_t pass_id;              // Unique pass identifier
    uint64_t start_time;           // CPU start time
    uint64_t end_time;             // CPU end time
    uint64_t gpu_start_time;       // GPU start timestamp
    uint64_t gpu_end_time;         // GPU end timestamp

    // GPU pipeline statistics
    uint64_t vertex_invocations;   // Vertex shader invocations
    uint64_t primitive_invocations; // Primitive assembly invocations
    uint64_t fragment_invocations; // Fragment shader invocations
    uint64_t compute_invocations;  // Compute shader invocations
    uint64_t geometry_invocations; // Geometry shader invocations
    uint64_t tess_control_invocations; // Tessellation control invocations
    uint64_t tess_evaluation_invocations; // Tessellation evaluation invocations

    // Memory operations
    uint64_t memory_reads;         // Memory read operations
    uint64_t memory_writes;        // Memory write operations

    // Draw call statistics
    uint32_t draw_calls;           // Number of draw calls
    uint32_t dispatch_calls;       // Number of compute dispatches
    uint32_t vertices_submitted;   // Vertices processed
    uint32_t primitives_generated; // Primitives generated

    // Frame information
    uint64_t frame_number;         // Frame this pass was recorded in
    double frame_time;             // Total frame time for context

    // Bottleneck analysis
    float bottleneck_score;        // 0.0-1.0 bottleneck severity
    const char *bottleneck_type;   // Type of bottleneck detected
    const char *optimization_hint; // Suggested optimization
} vk_render_pass_profile_t;

// Frame-level performance data
typedef struct vk_frame_profile_s {
    uint64_t frame_number;         // Frame identifier
    double frame_time_ms;          // Total frame time in milliseconds
    double cpu_time_ms;            // CPU time spent on rendering
    double gpu_time_ms;            // GPU time spent on rendering

    // Per-pass breakdown
    vk_render_pass_profile_t *passes;
    atomic_uint_t pass_count;
    uint32_t max_passes;

    // System-wide statistics
    float cpu_utilization;         // CPU utilization percentage
    float gpu_utilization;         // GPU utilization percentage
    VkDeviceSize memory_used;      // Memory usage during frame

    // Bottleneck analysis
    const char *primary_bottleneck; // Main performance bottleneck
    float bottleneck_severity;     // 0.0-1.0 severity score
    const char *performance_rating; // Overall performance rating

    // Trend analysis
    double avg_frame_time_10;      // 10-frame moving average
    double avg_frame_time_60;      // 60-frame moving average
    qboolean performance_stable;   // Whether performance is stable
} vk_frame_profile_t;

// Performance bottleneck types
typedef enum {
    BOTTLENECK_NONE = 0,
    BOTTLENECK_CPU_BOUND,          // CPU limited (high CPU time)
    BOTTLENECK_GPU_VERTEX,         // Vertex processing bottleneck
    BOTTLENECK_GPU_FRAGMENT,       // Fragment processing bottleneck
    BOTTLENECK_GPU_MEMORY,         // Memory bandwidth bottleneck
    BOTTLENECK_GPU_COMPUTE,        // Compute shader bottleneck
    BOTTLENECK_MEMORY_PRESSURE,    // High memory usage causing stalls
    BOTTLENECK_COMMAND_BUFFER,     // Command buffer submission overhead
    BOTTLENECK_SYNCHRONIZATION,    // GPU-CPU synchronization overhead
    BOTTLENECK_SHADER_COMPILATION, // Shader compilation stalls
    BOTTLENECK_TEXTURE_SAMPLING,   // Texture sampling bottleneck
    BOTTLENECK_DRAW_CALL_BATCHING, // Too many small draw calls
} vk_bottleneck_type_t;

// Performance trend analysis
typedef struct vk_performance_trend_s {
    atomic_uint_t sample_count;    // Number of samples in analysis
    double avg_frame_time;         // Average frame time
    double min_frame_time;         // Best frame time
    double max_frame_time;         // Worst frame time
    double std_deviation;          // Frame time variance
    double trend_slope;            // Performance trend direction
    qboolean degrading;            // Whether performance is degrading
    qboolean improving;            // Whether performance is improving
    const char *trend_description; // Human-readable trend description
} vk_performance_trend_t;

// Render graph profiler main structure
typedef struct vk_render_profiler_s {
    qboolean enabled;
    qboolean initialized;
    qboolean detailed_profiling;    // Enable detailed GPU queries

    // GPU query objects
    vk_timestamp_query_t timestamp_queries;
    vk_pipeline_stats_t pipeline_stats;

    // Profiling data storage
    vk_frame_profile_t *frame_history;
    uint32_t max_frames;
    atomic_uint_t current_frame_index;
    atomic_uint_t total_frames_recorded;

    // Real-time profiling data
    vk_render_pass_profile_t *current_passes;
    atomic_uint_t current_pass_count;
    atomic_uint_t max_passes_per_frame;

    // Performance analysis
    vk_performance_trend_t performance_trend;
    vk_bottleneck_type_t current_bottleneck;
    float bottleneck_severity;

    // Configuration
    atomic_uint_t frames_to_analyze; // Frames to keep in history
    qboolean enable_trend_analysis; // Enable performance trend detection
    qboolean auto_detect_bottlenecks; // Automatically detect bottlenecks
    float bottleneck_threshold;    // Threshold for bottleneck detection

    // GPU timing calibration
    uint64_t gpu_timestamp_period; // GPU timestamp period in nanoseconds
    qboolean timestamps_calibrated;

    // Statistics
    atomic_uint64_t total_profiling_time_ns; // Time spent profiling
    atomic_uint64_t profiling_overhead_percent; // Profiling overhead percentage
    atomic_uint64_t frames_analyzed;        // Total frames analyzed

    const char *debug_name;
} vk_render_profiler_t;

// Memory Bandwidth Profiler - Cache Miss Analysis and Access Pattern Optimization

// Cache performance metrics
typedef struct vk_cache_performance_s {
    atomic_uint64_t cache_hits;              // Total cache hits
    atomic_uint64_t cache_misses;            // Total cache misses
    atomic_uint64_t cache_accesses;          // Total cache accesses
    float hit_rate;                   // Cache hit rate (0.0-1.0)
    float miss_rate;                  // Cache miss rate (0.0-1.0)

    // Cache level specific metrics
    atomic_uint64_t l1_hits, l1_misses;
    atomic_uint64_t l2_hits, l2_misses;
    atomic_uint64_t l3_hits, l3_misses;

    // Performance impact
    atomic_uint64_t cycles_lost_to_cache_misses;
    atomic_uint64_t memory_stalls;
    VkDeviceSize bytes_transferred;
} vk_cache_performance_t;

// Memory access pattern analysis
typedef struct vk_memory_access_pattern_s {
    const char *resource_name;         // Name of the resource being accessed
    void *base_address;               // Base address of the memory region
    VkDeviceSize region_size;         // Size of the memory region

    // Access pattern metrics
    atomic_uint64_t sequential_accesses;     // Sequential memory accesses
    atomic_uint64_t random_accesses;         // Random memory accesses
    atomic_uint64_t total_accesses;          // Total memory accesses

    // Stride analysis
    int32_t common_stride;            // Most common access stride
    atomic_uint64_t stride_accesses;         // Accesses with common stride

    // Temporal locality
    atomic_uint64_t temporal_reuse;          // Accesses to recently used data
    atomic_uint64_t temporal_misses;         // Accesses to stale data

    // Spatial locality
    atomic_uint64_t spatial_hits;            // Accesses to nearby data
    atomic_uint64_t spatial_misses;          // Accesses to distant data

    // Prefetching analysis
    atomic_uint64_t prefetch_opportunities;  // Potential prefetch opportunities
    atomic_uint64_t prefetch_misses;         // Missed prefetch opportunities
    float prefetch_efficiency;        // Prefetch effectiveness (0.0-1.0)
} vk_memory_access_pattern_t;

// Memory bandwidth usage tracking
typedef struct vk_memory_bandwidth_s {
    atomic_uint64_t total_bytes_read;     // Total bytes read from memory
    atomic_uint64_t total_bytes_written;  // Total bytes written to memory
    atomic_uint64_t peak_bandwidth;       // Peak bandwidth usage
    atomic_uint64_t sustained_bandwidth;  // Sustained bandwidth usage

    // Bandwidth by memory type
    VkDeviceSize host_memory_bandwidth;    // System RAM bandwidth
    VkDeviceSize device_memory_bandwidth;  // GPU memory bandwidth
    VkDeviceSize shared_memory_bandwidth;  // Shared memory bandwidth

    // Bandwidth over time (sliding window)
    VkDeviceSize bandwidth_history[60];    // Last 60 frames/samples
    atomic_uint_t bandwidth_samples;

    // Bandwidth utilization
    float bandwidth_utilization;       // Percentage of available bandwidth used
    VkDeviceSize available_bandwidth;  // Theoretical maximum bandwidth

    // Bottlenecks
    qboolean bandwidth_limited;        // Whether bandwidth is a limiting factor
    float bandwidth_pressure;          // Bandwidth pressure (0.0-1.0)
} vk_memory_bandwidth_t;

// Memory layout optimization recommendations
typedef struct vk_layout_optimization_s {
    const char *structure_name;        // Name of the data structure
    const char *optimization_type;     // Type of optimization recommended

    // Current layout metrics
    atomic_uint_t current_size;         // Current memory footprint
    atomic_uint_t current_alignment;    // Current alignment

    // Optimization metrics
    atomic_uint_t optimized_size;       // Optimized memory footprint
    atomic_uint_t optimized_alignment;  // Optimized alignment
    float improvement_factor;          // Performance improvement factor

    // Cache efficiency improvements
    float cache_hit_improvement;       // Expected cache hit rate improvement
    float bandwidth_reduction;         // Expected bandwidth reduction

    // Implementation details
    const char *implementation_hint;   // How to implement the optimization
    qboolean auto_applicable;          // Whether this can be auto-applied
} vk_layout_optimization_t;

// Data structure access profiling
typedef struct vk_data_structure_profile_s {
    const char *structure_name;        // Name of the data structure
    const char *structure_type;        // Type (array, hash_map, etc.)
    VkDeviceSize element_size;         // Size of each element
    uint32_t element_count;           // Number of elements

    // Access patterns
    vk_memory_access_pattern_t access_pattern;

    // Performance metrics
    uint64_t read_operations;          // Total read operations
    uint64_t write_operations;         // Total write operations
    uint64_t total_operations;         // Total operations

    // Cache performance
    vk_cache_performance_t cache_performance;

    // Memory layout analysis
    qboolean cache_aligned;            // Whether structure is cache-aligned
    qboolean optimally_packed;         // Whether structure is optimally packed
    VkDeviceSize wasted_space;         // Bytes wasted due to padding/alignment

    // Optimization recommendations
    vk_layout_optimization_t *optimizations;
    uint32_t optimization_count;
} vk_data_structure_profile_t;

// Memory Bandwidth Profiler main structure
typedef struct vk_memory_bandwidth_profiler_s {
    qboolean enabled;
    qboolean initialized;
    qboolean detailed_analysis;        // Enable detailed access pattern analysis

    // Core profiling data
    vk_memory_bandwidth_t bandwidth_stats;
    vk_cache_performance_t global_cache_stats;

    // Data structure profiling
    vk_data_structure_profile_t *structure_profiles;
    uint32_t max_structures;
    uint32_t active_structures;

    // Memory access tracking
    vk_memory_access_pattern_t *access_patterns;
    uint32_t max_access_patterns;
    uint32_t active_access_patterns;

    // Performance analysis
    uint64_t total_memory_operations;
    uint64_t total_cache_misses;
    float average_cache_hit_rate;

    // Bandwidth monitoring
    VkDeviceSize bandwidth_sample_interval;  // Sampling interval in nanoseconds
    uint64_t last_bandwidth_sample;

    // Optimization recommendations
    vk_layout_optimization_t *layout_recommendations;
    uint32_t max_recommendations;
    uint32_t active_recommendations;

    // Configuration
    qboolean auto_apply_optimizations; // Automatically apply safe optimizations
    float cache_miss_threshold;        // Threshold for cache miss alerts
    float bandwidth_threshold;         // Threshold for bandwidth alerts

    // Hardware characteristics
    uint32_t cache_line_size;          // CPU cache line size
    uint32_t l1_cache_size;            // L1 cache size
    uint32_t l2_cache_size;            // L2 cache size
    uint32_t l3_cache_size;            // L3 cache size (if available)

    VkDeviceSize memory_bandwidth_limit; // Maximum memory bandwidth

    const char *debug_name;
} vk_memory_bandwidth_profiler_t;

// Parallel Processing Profiler - Thread utilization and synchronization overhead tracking

// Thread performance metrics
typedef struct vk_thread_metrics_s {
    uint32_t thread_id;              // Unique thread identifier
    const char *thread_name;         // Human-readable thread name
    uint64_t start_time;             // Thread start time
    uint64_t end_time;               // Thread end time (if completed)
    uint64_t total_active_time;      // Total time spent actively working
    uint64_t total_wait_time;        // Total time spent waiting/synchronizing
    uint64_t context_switches;       // Number of context switches
    uint64_t work_items_processed;   // Number of work items completed

    // Performance counters
    float utilization_percentage;    // Thread utilization (0.0-1.0)
    float efficiency_rating;         // Work efficiency rating
    uint64_t last_active_timestamp;  // Last time thread was active

    // Work distribution
    uint32_t submitted_work;         // Work items submitted to this thread
    uint32_t completed_work;         // Work items completed by this thread
    uint32_t failed_work;            // Work items that failed

    qboolean is_active;              // Whether thread is currently running
} vk_thread_metrics_t;

// Synchronization operation tracking
typedef struct vk_sync_operation_s {
    const char *operation_name;      // Name of sync operation (barrier, lock, etc.)
    const char *operation_type;      // Type: mutex, semaphore, barrier, etc.
    uint64_t total_wait_time;        // Total time spent waiting
    uint64_t operation_count;        // Number of times this operation was used
    uint64_t max_wait_time;          // Maximum single wait time
    uint64_t min_wait_time;          // Minimum single wait time

    // Contention analysis
    uint32_t contention_count;       // Number of times contention occurred
    float average_contention;        // Average contention level
    uint32_t max_contention;         // Maximum contention level

    // Thread impact
    uint32_t affected_threads;       // Number of threads affected
    uint64_t total_thread_wait_time; // Total wait time across all threads

    // Performance impact
    float overhead_percentage;       // Percentage of total time spent in this sync
    qboolean is_bottleneck;          // Whether this is a significant bottleneck
} vk_sync_operation_t;

// Work distribution analysis
typedef struct vk_work_distribution_s {
    const char *work_name;           // Name of the work unit
    uint32_t total_work_items;       // Total work items in this batch
    uint32_t completed_work_items;   // Work items completed
    uint32_t pending_work_items;     // Work items still pending

    // Timing
    uint64_t submission_time;        // When work was submitted
    uint64_t completion_time;        // When work completed
    uint64_t total_processing_time;  // Total time spent processing

    // Thread distribution
    uint32_t thread_count;           // Number of threads working on this
    uint32_t *thread_assignments;    // Which threads worked on which items
    uint32_t max_items_per_thread;   // Maximum items handled by any thread
    uint32_t min_items_per_thread;   // Minimum items handled by any thread

    // Load balancing metrics
    float load_balance_factor;       // How evenly work is distributed (0.0-1.0)
    qboolean has_load_imbalance;     // Whether there's significant imbalance
    const char *balance_issue;       // Description of load balancing issue

    // Efficiency metrics
    float parallel_efficiency;       // Parallel processing efficiency (0.0-1.0)
    float speedup_factor;            // Actual vs theoretical speedup
    uint64_t serial_overhead;        // Overhead from parallelization
} vk_work_distribution_t;

// Parallel processing efficiency metrics
typedef struct vk_parallel_efficiency_s {
    float overall_efficiency;        // Overall parallel efficiency (0.0-1.0)
    float scalability_factor;        // How well it scales with thread count
    float synchronization_overhead;  // Overhead from synchronization (0.0-1.0)
    float load_imbalance_factor;     // Load imbalance penalty (0.0-1.0)

    // Thread utilization statistics
    float avg_thread_utilization;    // Average thread utilization
    float max_thread_utilization;    // Highest thread utilization
    float min_thread_utilization;    // Lowest thread utilization
    float utilization_variance;      // Variance in thread utilization

    // Work distribution statistics
    uint32_t total_work_items;       // Total work items processed
    uint32_t avg_work_per_thread;    // Average work items per thread
    float work_distribution_efficiency; // Work distribution efficiency

    // Bottleneck analysis
    const char *primary_bottleneck;  // Main performance bottleneck
    float bottleneck_severity;       // Bottleneck severity (0.0-1.0)
    const char *optimization_hint;   // Suggested optimization

    // Performance predictions
    float predicted_efficiency_2x;  // Predicted efficiency with 2x threads
    float predicted_efficiency_4x;  // Predicted efficiency with 4x threads
    float optimal_thread_count;     // Estimated optimal thread count
} vk_parallel_efficiency_t;

// Parallel Processing Profiler main structure
typedef struct vk_parallel_profiler_s {
    qboolean enabled;
    qboolean initialized;
    qboolean detailed_tracking;      // Enable detailed thread tracking

    // Thread tracking
    vk_thread_metrics_t *thread_metrics;
    uint32_t max_threads;
    uint32_t active_threads;

    // Synchronization tracking
    vk_sync_operation_t *sync_operations;
    uint32_t max_sync_operations;
    uint32_t active_sync_operations;

    // Work distribution tracking
    vk_work_distribution_t *work_distributions;
    uint32_t max_work_distributions;
    uint32_t active_work_distributions;

    // Efficiency analysis
    vk_parallel_efficiency_t efficiency_metrics;

    // Sampling configuration
    uint64_t sample_interval_ns;     // How often to sample thread state
    uint64_t last_sample_time;
    uint32_t samples_per_second;     // Target sampling rate

    // Performance thresholds
    float low_utilization_threshold; // Below this is considered low utilization
    float high_contention_threshold; // Above this is high contention
    float load_imbalance_threshold;  // Above this indicates load imbalance

    // Statistics
    uint64_t total_sync_wait_time;   // Total time spent in synchronization
    uint64_t total_active_time;      // Total active processing time
    uint64_t total_idle_time;        // Total idle time
    uint32_t total_context_switches; // Total context switches across all threads

    // Hardware characteristics
    uint32_t logical_cores;          // Number of logical CPU cores
    uint32_t physical_cores;         // Number of physical CPU cores
    uint32_t threads_per_core;       // Hyperthreading factor

    const char *debug_name;
} vk_parallel_profiler_t;

// Shader Performance Analysis - Instruction count, register usage, and optimization suggestions

// Shader instruction analysis
typedef struct vk_shader_instruction_s {
    const char *opcode_name;            // SPIR-V opcode name
    uint32_t opcode;                    // SPIR-V opcode number
    uint32_t count;                     // Number of times this instruction appears
    uint32_t word_count;                // Total words used by this instruction type

    // Performance impact
    float cycles_per_instruction;       // Estimated cycles per instruction
    float alu_intensity;                // ALU utilization factor (0.0-1.0)
    qboolean is_memory_operation;       // Whether this is a memory operation
    qboolean is_branch_operation;       // Whether this is a branch operation
} vk_shader_instruction_t;

// Shader register usage analysis
typedef struct vk_shader_register_usage_s {
    // Input registers
    uint32_t input_attributes;          // Number of input attributes
    uint32_t input_varyings;            // Number of input varyings
    uint32_t uniform_buffers;           // Number of uniform buffers
    uint32_t storage_buffers;           // Number of storage buffers
    uint32_t sampled_images;            // Number of sampled images
    uint32_t storage_images;            // Number of storage images
    uint32_t samplers;                  // Number of samplers

    // Output registers
    uint32_t output_attributes;         // Number of output attributes
    uint32_t output_varyings;           // Number of output varyings

    // Temporary registers
    uint32_t temp_registers;            // Number of temporary registers used
    uint32_t max_register_pressure;     // Maximum register pressure

    // Special registers
    uint32_t push_constants;            // Push constant usage
    uint32_t spec_constants;            // Specialization constants
} vk_shader_register_usage_t;

// Shader performance metrics
typedef struct vk_shader_performance_metrics_s {
    const char *shader_name;            // Name of the shader
    VkShaderStageFlagBits stage;        // Shader stage (vertex, fragment, etc.)

    // Instruction counts
    uint32_t total_instructions;        // Total SPIR-V instructions
    uint32_t total_words;               // Total SPIR-V words
    uint32_t arithmetic_instructions;   // ALU instructions
    uint32_t memory_instructions;       // Memory access instructions
    uint32_t texture_instructions;      // Texture sampling instructions
    uint32_t control_flow_instructions; // Branch/jump instructions

    // Performance estimates
    float estimated_cycles;             // Estimated execution cycles
    float alu_utilization;              // ALU utilization percentage
    float memory_bandwidth_usage;       // Memory bandwidth usage estimate
    float texture_bandwidth_usage;      // Texture bandwidth usage estimate

    // Complexity metrics
    float cyclomatic_complexity;        // Control flow complexity
    uint32_t max_nesting_depth;         // Maximum control flow nesting
    uint32_t basic_blocks;              // Number of basic blocks

    // Resource usage
    vk_shader_register_usage_t registers;

    // Bottleneck analysis
    const char *primary_bottleneck;     // Main performance bottleneck
    float bottleneck_severity;          // Bottleneck severity (0.0-1.0)
    qboolean is_memory_bound;           // Memory bandwidth limited
    qboolean is_alu_bound;              // ALU limited
    qboolean is_texture_bound;          // Texture sampling limited
} vk_shader_performance_metrics_t;

// Shader optimization suggestions
typedef struct vk_shader_optimization_s {
    const char *shader_name;            // Shader being optimized
    const char *optimization_type;      // Type of optimization

    // Current vs optimized metrics
    uint32_t current_instructions;      // Current instruction count
    uint32_t optimized_instructions;    // Optimized instruction count
    float current_cycles;               // Current estimated cycles
    float optimized_cycles;             // Optimized estimated cycles

    // Improvement metrics
    float performance_improvement;      // Performance improvement factor
    float instruction_reduction;        // Instruction count reduction percentage
    VkDeviceSize bandwidth_reduction;   // Bandwidth reduction

    // Implementation details
    const char *implementation_hint;    // How to implement the optimization
    const char *code_example;           // Example code change
    qboolean auto_applicable;           // Can be applied automatically

    // Prerequisites
    const char *hardware_requirements;  // Required hardware features
    qboolean breaks_compatibility;      // Whether this breaks compatibility
} vk_shader_optimization_t;

// Shader Performance Analyzer main structure
typedef struct vk_shader_performance_analyzer_s {
    qboolean enabled;
    qboolean initialized;
    qboolean detailed_analysis;         // Enable detailed SPIR-V analysis

    // Shader database
    vk_shader_performance_metrics_t *shader_metrics;
    uint32_t max_shaders;
    uint32_t active_shaders;

    // Instruction analysis
    vk_shader_instruction_t *instruction_stats;
    uint32_t max_instruction_types;
    uint32_t active_instruction_types;

    // Optimization suggestions
    vk_shader_optimization_t *optimizations;
    uint32_t max_optimizations;
    uint32_t active_optimizations;

    // Analysis configuration
    qboolean analyze_vertex_shaders;    // Analyze vertex shaders
    qboolean analyze_fragment_shaders;  // Analyze fragment shaders
    qboolean analyze_compute_shaders;   // Analyze compute shaders
    qboolean analyze_geometry_shaders;  // Analyze geometry shaders

    // Performance thresholds
    uint32_t high_instruction_threshold; // Instructions considered high
    float high_alu_threshold;           // ALU utilization considered high
    float high_memory_threshold;        // Memory usage considered high

    // Hardware characteristics
    uint32_t max_texture_units;         // Maximum texture units
    uint32_t max_uniform_buffers;       // Maximum uniform buffers
    VkDeviceSize max_memory_bandwidth;  // Maximum memory bandwidth

    // Statistics
    uint64_t total_shaders_analyzed;    // Total shaders processed
    uint64_t total_instructions_analyzed; // Total instructions analyzed
    uint64_t analysis_time_ns;          // Time spent analyzing

    const char *debug_name;
} vk_shader_performance_analyzer_t;

// Asset Loading Profiler - Streaming performance and I/O bottleneck identification

// Asset loading operation tracking
typedef struct vk_asset_load_operation_s {
    const char *asset_name;            // Name/path of the asset
    const char *asset_type;            // Type: texture, model, sound, etc.
    uint64_t start_time;               // When loading started
    uint64_t end_time;                 // When loading completed
    VkDeviceSize expected_size;        // Expected size from metadata
    VkDeviceSize actual_size;          // Actual loaded size
    qboolean success;                  // Whether loading succeeded
    qboolean was_cached;               // Whether loaded from cache

    // Performance metrics
    uint64_t load_time_ns;             // Total loading time
    float load_bandwidth_mbps;         // Loading bandwidth in MB/s
    qboolean is_slow_load;             // Whether this is considered slow

    // Queue and threading info
    atomic_uint_t queue_position;           // Position in loading queue
    atomic_uint_t thread_id;                // Thread that performed the load
    qboolean was_blocking;             // Whether this was a blocking load
} vk_asset_load_operation_t;

// I/O operation performance tracking
typedef struct vk_io_operation_s {
    const char *operation_name;        // Name of the I/O operation
    const char *file_path;             // File being accessed
    uint64_t start_time;               // Operation start time
    uint64_t end_time;                 // Operation end time
    VkDeviceSize bytes_transferred;    // Bytes read/written
    qboolean is_read_operation;        // Read vs write operation

    // Performance metrics
    uint64_t operation_time_ns;        // Total operation time
    float bandwidth_mbps;              // Transfer bandwidth
    float latency_ms;                  // Operation latency
    qboolean is_bottleneck;            // Whether this operation is a bottleneck

    // System impact
    atomic_uint_t blocking_threads;         // Number of threads blocked by this op
    uint64_t total_block_time;         // Total time threads were blocked
} vk_io_operation_t;

// Streaming performance tracking
typedef struct vk_streaming_operation_s {
    const char *asset_name;            // Asset being streamed
    atomic_uint_t mip_level;                // Mip level being loaded
    uint64_t request_time;             // When streaming was requested
    uint64_t load_start_time;          // When loading actually started
    uint64_t completion_time;          // When streaming completed
    VkDeviceSize bytes_requested;      // Bytes requested to load
    VkDeviceSize bytes_loaded;         // Bytes actually loaded
    qboolean is_high_priority;         // Whether this was high priority
    qboolean was_cancelled;            // Whether streaming was cancelled

    // Performance metrics
    uint64_t queue_wait_time;          // Time spent waiting in queue
    uint64_t load_time;                // Actual loading time
    uint64_t total_time;               // Total time from request to completion
    float effective_bandwidth;         // Effective streaming bandwidth
    qboolean missed_deadline;          // Whether streaming missed its deadline
} vk_streaming_operation_t;

// Asset loading queue performance
typedef struct vk_asset_queue_performance_s {
    atomic_uint_t current_queue_length;     // Current number of assets in queue
    atomic_uint_t max_queue_length;         // Maximum queue length observed
    atomic_uint_t total_assets_processed;   // Total assets processed
    atomic_uint_t assets_cancelled;         // Assets cancelled due to priority changes

    // Timing statistics
    uint64_t avg_queue_wait_time;      // Average time assets spend in queue
    uint64_t max_queue_wait_time;      // Maximum queue wait time
    uint64_t total_queue_time;         // Total time spent waiting in queues

    // Throughput metrics
    float assets_per_second;           // Asset loading throughput
    VkDeviceSize bytes_per_second;     // Data loading throughput
    float queue_utilization;           // How busy the loading queue is
} vk_asset_queue_performance_t;

// Asset loading bottleneck analysis
typedef struct vk_asset_loading_bottlenecks_s {
    // I/O bottlenecks
    qboolean io_bottleneck;            // I/O subsystem is bottleneck
    float io_utilization;              // I/O subsystem utilization
    uint64_t avg_io_latency;           // Average I/O latency
    atomic_uint_t concurrent_io_ops;        // Number of concurrent I/O operations

    // CPU bottlenecks
    qboolean cpu_bottleneck;           // CPU decompression/parsing is bottleneck
    float cpu_utilization;             // CPU utilization during loading
    atomic_uint_t active_loading_threads;   // Number of active loading threads

    // Memory bottlenecks
    qboolean memory_bottleneck;        // Memory allocation/copy is bottleneck
    VkDeviceSize memory_pressure;      // Memory pressure during loading
    atomic_uint_t allocation_failures;      // Number of allocation failures

    // Streaming bottlenecks
    qboolean streaming_bottleneck;     // Streaming system is bottleneck
    atomic_uint_t streaming_queue_length;   // Current streaming queue length
    float streaming_efficiency;        // Streaming system efficiency

    // Recommendations
    const char *primary_bottleneck;    // Main bottleneck type
    const char *optimization_hint;     // Suggested optimization
    float expected_improvement;        // Expected performance improvement
} vk_asset_loading_bottlenecks_t;

// Asset Loading Profiler main structure
typedef struct vk_asset_loading_profiler_s {
    qboolean enabled;
    qboolean initialized;
    qboolean detailed_tracking;        // Enable detailed I/O tracking

    // Asset loading tracking
    vk_asset_load_operation_t *load_operations;
    uint32_t max_load_operations;
    atomic_uint_t active_load_operations;

    // I/O operation tracking
    vk_io_operation_t *io_operations;
    uint32_t max_io_operations;
    atomic_uint_t active_io_operations;

    // Streaming operation tracking
    vk_streaming_operation_t *streaming_operations;
    uint32_t max_streaming_operations;
    atomic_uint_t active_streaming_operations;

    // Queue performance
    vk_asset_queue_performance_t queue_performance;

    // Bottleneck analysis
    vk_asset_loading_bottlenecks_t bottlenecks;

    // Performance statistics
    atomic_uint64_t total_assets_loaded;      // Total assets successfully loaded
    atomic_uint64_t total_load_time_ns;       // Total time spent loading assets
    VkDeviceSize total_bytes_loaded;   // Total bytes loaded
    atomic_uint_t failed_loads;             // Number of failed loads

    // Sampling
    uint64_t sample_interval_ns;       // Performance sampling interval
    uint64_t last_sample_time;

    // Configuration
    VkDeviceSize slow_load_threshold_bytes; // Size threshold for slow load detection
    uint64_t slow_load_threshold_ns;   // Time threshold for slow load detection
    uint32_t max_concurrent_loads;     // Maximum concurrent loading operations

    // Asset type statistics
    atomic_uint_t texture_loads;            // Number of texture loads
    atomic_uint_t model_loads;              // Number of model loads
    atomic_uint_t sound_loads;              // Number of sound loads
    atomic_uint_t other_loads;              // Number of other asset loads

    const char *debug_name;
} vk_asset_loading_profiler_t;

// Performance HUD - Real-time overlay with bottleneck highlighting and recommendations

#ifdef USE_CIMGUI
// Performance HUD display configuration
typedef struct vk_performance_hud_config_s {
    qboolean enabled;
    qboolean show_vram_stats;
    qboolean show_memory_pools;
    qboolean show_render_profiler;
    qboolean show_memory_bandwidth;
    qboolean show_parallel_processing;
    qboolean show_shader_analysis;
    qboolean show_asset_loading;
    qboolean show_bottlenecks;
    qboolean show_recommendations;
    float update_interval;           // Update interval in seconds
    float window_alpha;              // Window transparency
    int window_flags;                // ImGui window flags
    float position_x, position_y;    // Window position
    float size_x, size_y;            // Window size

    // Bottleneck highlighting
    qboolean highlight_bottlenecks;
    float bottleneck_threshold_high;
    float bottleneck_threshold_medium;
    float bottleneck_threshold_low;

    // Color scheme (RGBA float arrays)
    float color_normal[4];
    float color_warning[4];
    float color_critical[4];
    float color_good[4];
    float color_background[4];
} vk_performance_hud_config_t;

// Performance HUD bottleneck summary
typedef struct vk_performance_hud_bottlenecks_s {
    // Overall system health
    float overall_performance_score; // 0.0-1.0, 1.0 = optimal
    const char *primary_bottleneck;
    float primary_bottleneck_severity;

    // Subsystem scores
    float vram_score;
    float memory_score;
    float render_score;
    float shader_score;
    float asset_score;
    float io_score;

    // Critical issues count
    int critical_issues;
    int warning_issues;
    int info_issues;

    // Top recommendations
    const char *top_recommendations[5];
    float recommendation_priorities[5];
} vk_performance_hud_bottlenecks_t;

// Performance HUD main structure
typedef struct vk_performance_hud_s {
    atomic_uint_t initialized;
    atomic_uint_t enabled;
    atomic_uint_t visible;

    // Configuration
    vk_performance_hud_config_t config;

    // Timing
    atomic_uint64_t last_update_time;
    float frame_time_accumulator;
    atomic_uint_t frame_count;
    float fps_current;
    float fps_average;
    float frame_time_min;
    float frame_time_max;
    float frame_time_avg;

    // Bottleneck analysis
    vk_performance_hud_bottlenecks_t bottlenecks;

    // Display state
    qboolean show_demo_window;
    qboolean show_metrics_window;
    qboolean show_profiler_window;
    char filter_text[256];

    // ImGui state
    qboolean imgui_frame_started;

    const char *debug_name;
} vk_performance_hud_t;
#endif

// Automated Performance Regression Detection - CI-based performance gates with historical comparison

// Performance metric snapshot
typedef struct vk_performance_metric_s {
    const char *name;                  // Metric name (e.g., "Frame Time", "VRAM Usage")
    float value;                       // Measured value
    const char *unit;                  // Unit (ms, MB, %, etc.)
    qboolean lower_is_better;          // Whether lower values are improved performance
} vk_performance_metric_t;

// Performance snapshot for a specific scenario
typedef struct vk_performance_snapshot_s {
    char scenario_name[64];            // Scenario name (e.g., "Main Menu", "Map Load")
    uint64_t timestamp;                // Capture timestamp
    char build_version[64];            // Engine build version
    char commit_hash[64];              // Git commit hash

    // Metrics
    vk_performance_metric_t *metrics;
    uint32_t metric_count;
    uint32_t max_metrics;

    // Aggregated scores
    float overall_performance_score;
} vk_performance_snapshot_t;

// Performance regression analysis
typedef struct vk_performance_regression_s {
    const char *metric_name;
    float baseline_value;
    float current_value;
    float difference_percentage;
    qboolean is_regression;
    float severity;                    // 0.0 - 1.0
} vk_performance_regression_t;

// Performance Regression Detector main structure
typedef struct vk_performance_regression_detector_s {
    qboolean enabled;
    qboolean initialized;

    // Current snapshot
    vk_performance_snapshot_t current_snapshot;

    // Baseline snapshots (from file)
    vk_performance_snapshot_t *baselines;
    atomic_uint_t baseline_count;
    uint32_t max_baselines;

    // Regression analysis results
    vk_performance_regression_t *regressions;
    atomic_uint_t regression_count;
    uint32_t max_regressions;

    // Configuration
    float regression_threshold;        // Percentage threshold for regression (default 5%)
    qboolean fail_on_regression;       // Whether to fail CI/gates on regression
    char baseline_file_path[256];      // Path to baseline JSON/CSV file

    // Statistics
    atomic_uint_t total_regressions_detected;
    float max_regression_percentage;

    const char *debug_name;
} vk_performance_regression_detector_t;

// Heatmap Visualization - Performance data visualization for optimization focus areas

// Heatmap layer types
typedef enum vk_heatmap_layer_type_e {
    VK_HEATMAP_LAYER_OVERDRAW,         // Fragment shader complexity / overdraw
    VK_HEATMAP_LAYER_GEOMETRY,         // Triangle/vertex density
    VK_HEATMAP_LAYER_MEMORY_ACCESS,    // Memory access hotspots
    VK_HEATMAP_LAYER_SHADING_RATE,     // Variable rate shading / shading intensity
    VK_HEATMAP_LAYER_COMPUTE_WORK,     // Compute shader work distribution
    VK_HEATMAP_LAYER_COUNT
} vk_heatmap_layer_type_t;

// Heatmap sample
typedef struct vk_heatmap_sample_s {
    float x, y;                        // Normalized screen coordinates (0.0-1.0)
    float intensity;                   // Sample intensity (0.0-1.0)
    uint32_t layer_type;               // Layer this sample belongs to
} vk_heatmap_sample_t;

// Heatmap layer data
typedef struct vk_heatmap_layer_s {
    uint32_t *data;                    // 2D grid of intensity values
    atomic_uint_t width, height;            // Grid dimensions
    atomic_uint_t sample_count;             // Number of samples in this frame
    float max_intensity;               // Current maximum intensity for normalization

    // Vulkan resources for visualization
    VkImage texture;
    VkImageView texture_view;
    VkDeviceMemory texture_memory;
    VkDescriptorSet descriptor_set;

    qboolean needs_update;             // Whether texture needs regeneration
} vk_heatmap_layer_t;

// Heatmap Visualizer main structure
typedef struct vk_heatmap_visualizer_s {
    qboolean enabled;
    qboolean initialized;
    atomic_uint_t current_mode;             // Current layer being displayed

    // Layers
    vk_heatmap_layer_t layers[VK_HEATMAP_LAYER_COUNT];

    // Configuration
    float global_opacity;              // Opacity of the heatmap overlay
    float intensity_scale;             // Scale factor for intensities
    qboolean accumulation_mode;        // Whether to accumulate samples over time
    float decay_rate;                  // Decay rate for accumulation (0.0-1.0)

    // Color mapping
    float gradient_colors[5][4];       // Color gradient for heatmap (cold to hot) RGBA arrays

    // Sampling
    vk_heatmap_sample_t *samples;      // Buffer for temporary samples
    atomic_uint_t active_samples;
    uint32_t max_samples;

    const char *debug_name;
} vk_heatmap_visualizer_t;

// Render profiler API functions
extern qboolean vk_init_render_profiler(void);
extern void vk_shutdown_render_profiler(void);
extern void vk_profile_pass_start(const char *pass_name, uint32_t pass_id);
extern void vk_profile_pass_end(const char *pass_name, uint32_t draw_calls, uint32_t vertices);
extern void vk_profile_frame_end(void);
extern void vk_print_render_profiler_stats(void);
extern void vk_set_detailed_profiling(qboolean enabled);

// Memory Bandwidth Profiler API
extern qboolean vk_init_memory_bandwidth_profiler(void);
extern void vk_shutdown_memory_bandwidth_profiler(void);
extern void vk_record_memory_access(void *address, VkDeviceSize size, const char *resource_name, qboolean is_write);
extern void vk_sample_memory_bandwidth(void);
extern void vk_analyze_memory_access_patterns(void);
extern void vk_print_memory_bandwidth_stats(void);
extern void vk_print_cache_performance_stats(void);
extern void vk_print_layout_optimization_recommendations(void);
extern void vk_apply_memory_optimizations(void);
extern void vk_set_bandwidth_profiling_enabled(qboolean enabled);

// Parallel Processing Profiler API
extern qboolean vk_init_parallel_profiler(void);
extern void vk_shutdown_parallel_profiler(void);
extern void vk_profile_thread_start(const char *thread_name, uint32_t thread_id);
extern void vk_profile_thread_end(uint32_t thread_id);
extern void vk_profile_sync_operation(const char *operation_name, uint64_t wait_time_ns);
extern void vk_profile_lock_acquire(const char *lock_name, uint32_t thread_id);
extern void vk_profile_lock_release(const char *lock_name, uint32_t thread_id);
extern void vk_profile_work_submit(const char *work_name, uint32_t thread_count);
extern void vk_profile_work_complete(const char *work_name);
extern void vk_sample_thread_utilization(void);
extern void vk_print_parallel_stats(void);
extern void vk_print_thread_utilization(void);
extern void vk_print_synchronization_overhead(void);
extern void vk_print_parallel_efficiency(void);
extern void vk_set_parallel_profiling_enabled(qboolean enabled);

// Shader Performance Analysis API
extern qboolean vk_init_shader_performance_analyzer(void);
extern void vk_shutdown_shader_performance_analyzer(void);
extern void vk_analyze_shader_performance(const char *shader_name, const uint32_t *spirv_code, size_t code_size, VkShaderStageFlagBits stage);
extern void vk_print_shader_performance_stats(void);
extern void vk_print_shader_optimization_suggestions(void);
extern void vk_print_shader_instruction_analysis(void);
extern void vk_print_shader_register_usage(void);
extern void vk_set_shader_analysis_enabled(qboolean enabled);

// Asset Loading Profiler API
extern qboolean vk_init_asset_loading_profiler(void);
extern void vk_shutdown_asset_loading_profiler(void);
extern void vk_profile_asset_load_start(const char *asset_name, const char *asset_type, VkDeviceSize expected_size);
extern void vk_profile_asset_load_end(const char *asset_name, VkDeviceSize actual_size, qboolean success);
extern void vk_profile_io_operation(const char *operation_name, VkDeviceSize bytes_transferred, uint64_t operation_time_ns);
extern void vk_profile_streaming_request(const char *asset_name, uint32_t mip_level, qboolean is_high_priority);
extern void vk_profile_streaming_complete(const char *asset_name, uint32_t mip_level, VkDeviceSize bytes_loaded);
extern void vk_sample_asset_loading_performance(void);
extern void vk_print_asset_loading_stats(void);
extern void vk_print_io_performance_stats(void);
extern void vk_print_streaming_stats(void);
extern void vk_print_asset_loading_bottlenecks(void);
extern void vk_set_asset_loading_profiling_enabled(qboolean enabled);

// Performance HUD API
extern qboolean vk_init_performance_hud(void);
extern void vk_shutdown_performance_hud(void);
extern void vk_render_performance_hud(void);
extern void vk_toggle_performance_hud(void);
extern void vk_set_performance_hud_enabled(qboolean enabled);
extern qboolean vk_is_performance_hud_enabled(void);

// Automated Performance Regression Detection API
extern qboolean vk_init_performance_regression_detector(void);
extern void vk_shutdown_performance_regression_detector(void);
extern void vk_capture_performance_snapshot(const char *scenario_name);
extern void vk_save_performance_baseline(const char *scenario_name);
extern void vk_compare_against_baseline(const char *scenario_name);
extern void vk_print_performance_regression_report(void);
extern void vk_export_performance_metrics(const char *file_path);
extern void vk_set_performance_regression_threshold(float threshold_percentage);
extern qboolean vk_did_performance_regress(void);

// Heatmap Visualization API
extern qboolean vk_init_heatmap_visualizer(void);
extern void vk_shutdown_heatmap_visualizer(void);
extern void vk_record_heatmap_sample(float x, float y, float intensity, uint32_t layer_type);
extern void vk_generate_heatmap_texture(uint32_t layer_type);
extern void vk_render_heatmap_overlay(void);
extern void vk_clear_heatmap(uint32_t layer_type);
extern void vk_set_heatmap_enabled(qboolean enabled);
extern void vk_set_heatmap_mode(uint32_t mode);
extern void vk_print_heatmap_stats(void);

// Performance Presets API
typedef enum vk_performance_preset_e {
    VK_PERF_PRESET_POTATO = 0,
    VK_PERF_PRESET_LOW,
    VK_PERF_PRESET_MEDIUM,
    VK_PERF_PRESET_HIGH,
    VK_PERF_PRESET_ULTRA,
    VK_PERF_PRESET_COUNT
} vk_performance_preset_t;

extern void vk_apply_performance_preset(vk_performance_preset_t preset);
extern const char *vk_get_performance_preset_name(vk_performance_preset_t preset);
extern vk_performance_preset_t vk_get_current_performance_preset(void);
extern void vk_print_performance_presets_info(void);

// Memory pool allocation entry for tracking
typedef struct vk_pool_allocation_s {
    void *user_ptr;             // User data pointer for identification
    VkDeviceSize size;          // Allocation size
    VkDeviceSize alignment;     // Memory alignment
    uint32_t pool_level;        // Which pool level this belongs to
    uint32_t allocation_index;  // Index within the pool
    uint64_t last_used_frame;   // Frame counter when last used
    qboolean is_active;         // Whether this allocation is in use
    const char *debug_name;     // Debug name for tracking
} vk_pool_allocation_t;

// Hierarchical memory pool level
typedef struct vk_memory_pool_level_s {
    VkDeviceSize min_size;      // Minimum allocation size for this level
    VkDeviceSize max_size;      // Maximum allocation size for this level
    VkDeviceSize block_size;    // Size of each memory block in this pool

    // Dynamic scaling
    uint32_t initial_blocks;    // Initial number of blocks
    uint32_t max_blocks;        // Maximum number of blocks allowed
    uint32_t min_blocks;        // Minimum number of blocks to keep
    uint32_t current_blocks;    // Current number of allocated blocks
    uint32_t free_blocks;       // Number of free blocks

    // Memory blocks
    VkDeviceMemory *memory_blocks;
    VkDeviceSize *memory_sizes;
    uint32_t *allocation_counts; // Allocations per block
    void **mapped_pointers;     // For persistently mapped memory

    // Allocation tracking
    vk_pool_allocation_t *allocations;
    uint32_t max_allocations;
    uint32_t allocation_count;

    // Free list for quick allocation
    uint32_t *free_indices;
    uint32_t free_count;

    // Statistics
    VkDeviceSize total_allocated;
    VkDeviceSize total_used;
    uint64_t allocation_operations;
    uint64_t deallocation_operations;
    uint64_t scaling_operations;

    // Cleanup parameters
    uint32_t frames_since_last_use; // For cleanup decisions
    qboolean allow_cleanup;     // Whether this level can be cleaned up
} vk_memory_pool_level_t;

// Memory pressure levels
typedef enum {
    MEMORY_PRESSURE_LOW = 0,
    MEMORY_PRESSURE_MEDIUM,
    MEMORY_PRESSURE_HIGH,
    MEMORY_PRESSURE_CRITICAL
} vk_memory_pressure_t;

// Hierarchical memory pool system
typedef struct {
    qboolean enabled;
    qboolean initialized;

    // Pool hierarchy (from smallest to largest)
    vk_memory_pool_level_t *pool_levels;
    uint32_t num_pool_levels;

    // Memory pressure management
    vk_memory_pressure_t current_pressure;
    VkDeviceSize total_memory_used;
    VkDeviceSize total_memory_allocated;
    VkDeviceSize memory_pressure_threshold_low;
    VkDeviceSize memory_pressure_threshold_medium;
    VkDeviceSize memory_pressure_threshold_high;

    // Automatic scaling parameters
    float scale_up_threshold;   // Usage ratio to trigger scaling up
    float scale_down_threshold; // Usage ratio to trigger scaling down
    uint32_t min_blocks_per_level;
    uint32_t max_blocks_per_level;

    // Cleanup parameters
    uint32_t cleanup_interval_frames;
    uint32_t max_cleanup_age_frames; // Max age before cleanup
    uint64_t current_frame;

    // Statistics
    uint64_t total_allocations;
    uint64_t total_deallocations;
    uint64_t total_scaling_operations;
    uint64_t total_cleanup_operations;
    uint64_t cache_hits;
    uint64_t cache_misses;

    // Legacy compatibility (will be phased out)
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
qboolean vk_allocate_image_chunk(void);
void vk_init_memory_defragmentation(void);
void vk_calculate_fragmentation_metrics(void);
qboolean vk_perform_defragmentation(void);
void vk_check_defragmentation(void);

// Hierarchical Memory Pool System
qboolean vk_init_memory_pool_system(void);
void *vk_pool_allocate(VkDeviceSize size, VkDeviceSize alignment, const char *debug_name);
void vk_pool_deallocate(void *ptr);
void vk_update_memory_pool_system(void);
void vk_print_pool_statistics(void);
void vk_shutdown_memory_pool_system(void);

// Lock-Free Memory Allocators
qboolean vk_init_lock_free_memory_manager(void);
void *vk_lock_free_alloc(VkDeviceSize size, const char *debug_name);
qboolean vk_lock_free_free(void *ptr);
void vk_print_lock_free_stats(void);
void vk_shutdown_lock_free_memory_manager(void);

// Arena Allocators
qboolean vk_init_arena_manager(void);
void vk_reset_frame_arena(void);
void vk_reset_render_arena(void);
void vk_reset_asset_arena(void);
void *vk_frame_alloc(VkDeviceSize size);
void *vk_render_alloc(VkDeviceSize size);
void *vk_asset_alloc(VkDeviceSize size);
void *vk_persistent_alloc(VkDeviceSize size);
vk_memory_arena_t *vk_create_dynamic_arena(VkDeviceSize size, const char *name);
void vk_destroy_dynamic_arena(vk_memory_arena_t *arena);
void *vk_arena_alloc_from(vk_memory_arena_t *arena, VkDeviceSize size);
void vk_print_arena_stats(void);
void vk_shutdown_arena_manager(void);

// Memory Advisor
qboolean vk_init_memory_advisor(void);
void vk_record_memory_advisor_access(void *address, VkDeviceSize offset, const char *resource_name);
void vk_update_memory_advisor(void);
void vk_print_memory_advisor_stats(void);
void vk_set_memory_advisor_auto_optimization(qboolean enabled);
void vk_force_memory_analysis(void);
void vk_shutdown_memory_advisor(void);

// Cache-Conscious Data Structures
qboolean vk_init_cache_structures_manager(void);
qboolean vk_cache_array_init(vk_cache_array_t *array, VkDeviceSize element_size, uint32_t initial_capacity, const char *debug_name);
qboolean vk_cache_array_resize(vk_cache_array_t *array, uint32_t new_capacity);
qboolean vk_cache_array_push(vk_cache_array_t *array, const void *element);
qboolean vk_cache_array_pop(vk_cache_array_t *array, void *element);
void *vk_cache_array_get(vk_cache_array_t *array, uint32_t index);
void vk_cache_array_clear(vk_cache_array_t *array);
void vk_cache_array_destroy(vk_cache_array_t *array);
qboolean vk_cache_hash_map_init(vk_cache_hash_map_t *map, VkDeviceSize key_size, VkDeviceSize value_size, uint32_t initial_capacity, uint32_t (*hash_func)(const void*, VkDeviceSize), qboolean (*equals_func)(const void*, const void*, VkDeviceSize), const char *debug_name);
qboolean vk_cache_queue_init(vk_cache_queue_t *queue, VkDeviceSize element_size, uint32_t capacity, const char *debug_name);
void vk_print_cache_structures_stats(void);
void vk_shutdown_cache_structures_manager(void);
void vk_init_resource_pool(void);
void vk_shutdown_resource_pool(void);
VkBuffer vk_get_buffer_from_pool(VkDeviceSize size);
void vk_return_buffer_to_pool(VkBuffer buffer);
void vk_alloc_staging_buffer(VkDeviceSize size);
void vk_flush_staging_buffer(qboolean final);
void vk_clean_staging_buffer(void);

#ifdef __cplusplus
}
#endif

#endif // __VK_MEMORY_H__
