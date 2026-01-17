/*
===============================================================================
Custom Memory Allocators for id Tech 3

Specialized allocators for different subsystems to improve performance
and memory usage patterns.
===============================================================================
*/

#ifndef __Q_ALLOCATOR_H__
#define __Q_ALLOCATOR_H__

#include "q_shared.h"

// Allocator types
typedef enum {
    ALLOCATOR_GENERAL,      // General-purpose allocator
    ALLOCATOR_RENDERING,    // Optimized for rendering (frequent small allocations)
    ALLOCATOR_NETWORKING,   // Optimized for networking (frequent message buffers)
    ALLOCATOR_AUDIO,        // Optimized for audio (streaming buffers)
    ALLOCATOR_TEMPORARY,    // Temporary allocations (short-lived)
    ALLOCATOR_PERSISTENT,   // Long-lived persistent data
    ALLOCATOR_COUNT
} allocator_type_t;

// Memory statistics
typedef struct {
    size_t total_allocated;
    size_t peak_allocated;
    size_t allocation_count;
    size_t free_count;
    size_t fragmentation_ratio; // 0-100, higher means more fragmented
    double average_allocation_time;
    double average_free_time;
} allocator_stats_t;

// Allocator interface
typedef struct allocator_s {
    const char *name;
    allocator_type_t type;

    // Core functions
    void *(*alloc)(struct allocator_s *alloc, size_t size, const char *tag);
    void (*free)(struct allocator_s *alloc, void *ptr);
    void *(*realloc)(struct allocator_s *alloc, void *ptr, size_t new_size);

    // Utility functions
    size_t (*get_size)(struct allocator_s *alloc, void *ptr);
    qboolean (*validate)(struct allocator_s *alloc, void *ptr);
    void (*deflate)(struct allocator_s *alloc); // Compact memory
    void (*reset)(struct allocator_s *alloc); // Reset for new frame/level

    // Statistics
    allocator_stats_t stats;

    // Private data
    void *private_data;
} allocator_t;

// Pool allocator for frequent small allocations (rendering, networking)
typedef struct {
    size_t block_size;
    size_t blocks_per_chunk;
    size_t chunk_count;
    void **free_list;
    void *chunks;
} pool_allocator_data_t;

// Arena allocator for temporary allocations
typedef struct {
    byte *buffer;
    size_t size;
    size_t used;
    qboolean auto_reset;
} arena_allocator_data_t;

// Slab allocator for objects of similar size
typedef struct {
    size_t object_size;
    size_t objects_per_slab;
    void **slabs;
    size_t slab_count;
    void *free_list;
} slab_allocator_data_t;

// Buddy allocator for power-of-two sizes
typedef struct {
    byte *memory;
    size_t total_size;
    size_t min_block_size;
    void *free_lists[32]; // For different block sizes
} buddy_allocator_data_t;

// Global allocator manager
extern allocator_t *g_allocators[ALLOCATOR_COUNT];

// Initialize allocator system
void Alloc_Init(void);
void Alloc_Shutdown(void);

// Get allocator for specific use case
allocator_t *Alloc_GetAllocator(allocator_type_t type);

// Create specialized allocators
allocator_t *Alloc_CreatePoolAllocator(size_t block_size, size_t blocks_per_chunk, const char *name);
allocator_t *Alloc_CreateArenaAllocator(size_t size, qboolean auto_reset, const char *name);
allocator_t *Alloc_CreateSlabAllocator(size_t object_size, size_t objects_per_slab, const char *name);
allocator_t *Alloc_CreateBuddyAllocator(size_t total_size, size_t min_block_size, const char *name);

// Convenience functions
void *Alloc_Alloc(size_t size, allocator_type_t type, const char *tag);
void Alloc_Free(void *ptr, allocator_type_t type);
void *Alloc_Realloc(void *ptr, size_t new_size, allocator_type_t type);

// Rendering-specific allocation functions
void *Render_Alloc(size_t size, const char *tag);
void Render_Free(void *ptr);
void *Render_AllocTemp(size_t size, const char *tag); // Frame-scoped

// Networking-specific allocation functions
void *Net_Alloc(size_t size, const char *tag);
void Net_Free(void *ptr);
void *Net_AllocMessage(size_t size); // Optimized for message buffers

// Audio-specific allocation functions
void *Audio_Alloc(size_t size, const char *tag);
void Audio_Free(void *ptr);
void *Audio_AllocStream(size_t size); // Streaming buffer

// Statistics and debugging
void Alloc_PrintStats(void);
void Alloc_PrintLeaks(void);
void Alloc_ValidateAll(void);

// Memory pressure handling
void Alloc_HandleMemoryPressure(void);

// Performance monitoring
double Alloc_GetFragmentationRatio(allocator_type_t type);
size_t Alloc_GetMemoryUsage(allocator_type_t type);

// Legacy compatibility (maps to appropriate allocators)
#define Z_Malloc(size) Alloc_Alloc(size, ALLOCATOR_GENERAL, "legacy")
#define Z_Free(ptr) Alloc_Free(ptr, ALLOCATOR_GENERAL)
#define Hunk_AllocateTempMemory(size) Alloc_Alloc(size, ALLOCATOR_TEMPORARY, "hunk_temp")
#define Hunk_FreeTempMemory(ptr) Alloc_Free(ptr, ALLOCATOR_TEMPORARY)

#endif // __Q_ALLOCATOR_H__