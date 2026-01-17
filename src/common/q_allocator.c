/*
===============================================================================
Custom Memory Allocators Implementation

Specialized allocators for improved memory management and performance.
===============================================================================
*/

#include "q_allocator.h"
#include "q_shared.h"
#include <stdlib.h>
#include <string.h>

// Global allocator instances
allocator_t *g_allocators[ALLOCATOR_COUNT];

//============================================================================
// Pool Allocator Implementation
//============================================================================

static void *PoolAlloc_Alloc(allocator_t *alloc, size_t size, const char *tag) {
    pool_allocator_data_t *data = (pool_allocator_data_t *)alloc->private_data;

    if (size > data->block_size) {
        Com_Printf("PoolAlloc: Requested size %zu exceeds block size %zu\n", size, data->block_size);
        return NULL;
    }

    // Get a free block
    if (data->free_list) {
        void *block = data->free_list;
        data->free_list = *(void **)block;
        alloc->stats.allocation_count++;
        alloc->stats.total_allocated += data->block_size;
        return block;
    }

    // Allocate new chunk
    size_t chunk_size = data->block_size * data->blocks_per_chunk;
    void *chunk = malloc(chunk_size);
    if (!chunk) {
        Com_Printf("PoolAlloc: Failed to allocate chunk of size %zu\n", chunk_size);
        return NULL;
    }

    // Link all blocks in the chunk
    void *current = chunk;
    for (size_t i = 0; i < data->blocks_per_chunk - 1; i++) {
        void *next = (byte *)current + data->block_size;
        *(void **)current = next;
        current = next;
    }
    *(void **)current = data->free_list;
    data->free_list = chunk;

    // Update statistics
    data->chunk_count++;
    alloc->stats.allocation_count++;
    alloc->stats.total_allocated += data->block_size;

    // Return first block
    void *block = data->free_list;
    data->free_list = *(void **)block;
    return block;
}

static void PoolAlloc_Free(allocator_t *alloc, void *ptr) {
    pool_allocator_data_t *data = (pool_allocator_data_t *)alloc->private_data;

    if (!ptr) return;

    // Add to free list
    *(void **)ptr = data->free_list;
    data->free_list = ptr;

    alloc->stats.free_count++;
    alloc->stats.total_allocated -= data->block_size;
}

static void *PoolAlloc_Realloc(allocator_t *alloc, void *ptr, size_t new_size) {
    pool_allocator_data_t *data = (pool_allocator_data_t *)alloc->private_data;

    if (new_size <= data->block_size) {
        // Can reuse same block
        return ptr;
    }

    // Need to allocate new block and copy
    void *new_ptr = PoolAlloc_Alloc(alloc, new_size, "realloc");
    if (new_ptr && ptr) {
        memcpy(new_ptr, ptr, data->block_size);
        PoolAlloc_Free(alloc, ptr);
    }
    return new_ptr;
}

allocator_t *Alloc_CreatePoolAllocator(size_t block_size, size_t blocks_per_chunk, const char *name) {
    allocator_t *alloc = (allocator_t *)malloc(sizeof(allocator_t));
    pool_allocator_data_t *data = (pool_allocator_data_t *)malloc(sizeof(pool_allocator_data_t));

    if (!alloc || !data) {
        free(alloc);
        free(data);
        return NULL;
    }

    // Initialize data
    data->block_size = block_size;
    data->blocks_per_chunk = blocks_per_chunk;
    data->chunk_count = 0;
    data->free_list = NULL;
    data->chunks = NULL;

    // Initialize allocator
    alloc->name = name;
    alloc->type = ALLOCATOR_GENERAL; // Will be set by caller
    alloc->alloc = PoolAlloc_Alloc;
    alloc->free = PoolAlloc_Free;
    alloc->realloc = PoolAlloc_Realloc;
    alloc->get_size = NULL; // Not supported for pool allocator
    alloc->validate = NULL; // Not supported for pool allocator
    alloc->deflate = NULL; // Not supported for pool allocator
    alloc->reset = NULL; // Not supported for pool allocator
    memset(&alloc->stats, 0, sizeof(alloc->stats));
    alloc->private_data = data;

    return alloc;
}

//============================================================================
// Arena Allocator Implementation
//============================================================================

static void *ArenaAlloc_Alloc(allocator_t *alloc, size_t size, const char *tag) {
    arena_allocator_data_t *data = (arena_allocator_data_t *)alloc->private_data;

    // Align to pointer size
    size_t aligned_size = (size + sizeof(void *) - 1) & ~(sizeof(void *) - 1);

    if (data->used + aligned_size > data->size) {
        Com_Printf("ArenaAlloc: Out of memory (requested %zu, used %zu, total %zu)\n",
                   aligned_size, data->used, data->size);
        return NULL;
    }

    void *ptr = data->buffer + data->used;
    data->used += aligned_size;

    alloc->stats.allocation_count++;
    alloc->stats.total_allocated += aligned_size;

    if (alloc->stats.total_allocated > alloc->stats.peak_allocated) {
        alloc->stats.peak_allocated = alloc->stats.total_allocated;
    }

    return ptr;
}

static void ArenaAlloc_Free(allocator_t *alloc, void *ptr) {
    // Arena allocator doesn't support individual frees
    // Memory is freed when arena is reset
    (void)alloc;
    (void)ptr;
}

static void ArenaAlloc_Reset(allocator_t *alloc) {
    arena_allocator_data_t *data = (arena_allocator_data_t *)alloc->private_data;

    data->used = 0;
    alloc->stats.total_allocated = 0;
    alloc->stats.free_count = alloc->stats.allocation_count; // All "freed"
}

allocator_t *Alloc_CreateArenaAllocator(size_t size, qboolean auto_reset, const char *name) {
    allocator_t *alloc = (allocator_t *)malloc(sizeof(allocator_t));
    arena_allocator_data_t *data = (arena_allocator_data_t *)malloc(sizeof(arena_allocator_data_t));

    if (!alloc || !data) {
        free(alloc);
        free(data);
        return NULL;
    }

    // Allocate arena buffer
    data->buffer = (byte *)malloc(size);
    if (!data->buffer) {
        free(alloc);
        free(data);
        return NULL;
    }

    data->size = size;
    data->used = 0;
    data->auto_reset = auto_reset;

    // Initialize allocator
    alloc->name = name;
    alloc->type = ALLOCATOR_TEMPORARY;
    alloc->alloc = ArenaAlloc_Alloc;
    alloc->free = ArenaAlloc_Free;
    alloc->realloc = NULL; // Not supported for arena allocator
    alloc->get_size = NULL;
    alloc->validate = NULL;
    alloc->deflate = NULL;
    alloc->reset = ArenaAlloc_Reset;
    memset(&alloc->stats, 0, sizeof(alloc->stats));
    alloc->private_data = data;

    return alloc;
}

//============================================================================
// Slab Allocator Implementation
//============================================================================

static void *SlabAlloc_Alloc(allocator_t *alloc, size_t size, const char *tag) {
    slab_allocator_data_t *data = (slab_allocator_data_t *)alloc->private_data;

    if (size != data->object_size) {
        Com_Printf("SlabAlloc: Size mismatch (requested %zu, slab size %zu)\n",
                   size, data->object_size);
        return NULL;
    }

    // Check free list first
    if (data->free_list) {
        void *obj = data->free_list;
        data->free_list = *(void **)obj;
        alloc->stats.allocation_count++;
        alloc->stats.total_allocated += data->object_size;
        return obj;
    }

    // Allocate new slab
    size_t slab_size = data->object_size * data->objects_per_slab;
    void *slab = malloc(slab_size);
    if (!slab) {
        Com_Printf("SlabAlloc: Failed to allocate slab of size %zu\n", slab_size);
        return NULL;
    }

    // Add to slabs list
    data->slabs = realloc(data->slabs, sizeof(void *) * (data->slab_count + 1));
    data->slabs[data->slab_count++] = slab;

    // Link objects in free list
    void *current = slab;
    for (size_t i = 0; i < data->objects_per_slab - 1; i++) {
        void *next = (byte *)current + data->object_size;
        *(void **)current = next;
        current = next;
    }
    *(void **)current = data->free_list;
    data->free_list = slab;

    // Return first object
    void *obj = data->free_list;
    data->free_list = *(void **)obj;

    alloc->stats.allocation_count++;
    alloc->stats.total_allocated += data->object_size;

    return obj;
}

static void SlabAlloc_Free(allocator_t *alloc, void *ptr) {
    slab_allocator_data_t *data = (slab_allocator_data_t *)alloc->private_data;

    if (!ptr) return;

    // Add to free list
    *(void **)ptr = data->free_list;
    data->free_list = ptr;

    alloc->stats.free_count++;
    alloc->stats.total_allocated -= data->object_size;
}

allocator_t *Alloc_CreateSlabAllocator(size_t object_size, size_t objects_per_slab, const char *name) {
    allocator_t *alloc = (allocator_t *)malloc(sizeof(allocator_t));
    slab_allocator_data_t *data = (slab_allocator_data_t *)malloc(sizeof(slab_allocator_data_t));

    if (!alloc || !data) {
        free(alloc);
        free(data);
        return NULL;
    }

    data->object_size = object_size;
    data->objects_per_slab = objects_per_slab;
    data->slabs = NULL;
    data->slab_count = 0;
    data->free_list = NULL;

    alloc->name = name;
    alloc->type = ALLOCATOR_GENERAL;
    alloc->alloc = SlabAlloc_Alloc;
    alloc->free = SlabAlloc_Free;
    alloc->realloc = NULL; // Not supported
    alloc->get_size = NULL;
    alloc->validate = NULL;
    alloc->deflate = NULL;
    alloc->reset = NULL;
    memset(&alloc->stats, 0, sizeof(alloc->stats));
    alloc->private_data = data;

    return alloc;
}

//============================================================================
// System Integration
//============================================================================

void Alloc_Init(void) {
    // Create specialized allocators
    g_allocators[ALLOCATOR_RENDERING] = Alloc_CreatePoolAllocator(256, 1024, "rendering");
    g_allocators[ALLOCATOR_NETWORKING] = Alloc_CreatePoolAllocator(1400, 256, "networking"); // MTU-sized
    g_allocators[ALLOCATOR_AUDIO] = Alloc_CreatePoolAllocator(4096, 64, "audio");
    g_allocators[ALLOCATOR_TEMPORARY] = Alloc_CreateArenaAllocator(8 * 1024 * 1024, qtrue, "temporary"); // 8MB
    g_allocators[ALLOCATOR_PERSISTENT] = Alloc_CreateSlabAllocator(64, 1024, "persistent");

    // General allocator uses system malloc
    g_allocators[ALLOCATOR_GENERAL] = NULL; // Uses system allocator

    Com_Printf("Memory allocator system initialized\n");
}

void Alloc_Shutdown(void) {
    for (int i = 0; i < ALLOCATOR_COUNT; i++) {
        if (g_allocators[i]) {
            // Clean up allocator-specific data
            free(g_allocators[i]->private_data);
            free(g_allocators[i]);
            g_allocators[i] = NULL;
        }
    }

    Com_Printf("Memory allocator system shut down\n");
}

allocator_t *Alloc_GetAllocator(allocator_type_t type) {
    if (type < 0 || type >= ALLOCATOR_COUNT) {
        return NULL;
    }
    return g_allocators[type];
}

// Convenience functions
void *Alloc_Alloc(size_t size, allocator_type_t type, const char *tag) {
    allocator_t *alloc = Alloc_GetAllocator(type);
    if (!alloc) {
        // Fall back to system allocator
        return malloc(size);
    }
    return alloc->alloc(alloc, size, tag);
}

void Alloc_Free(void *ptr, allocator_type_t type) {
    allocator_t *alloc = Alloc_GetAllocator(type);
    if (!alloc) {
        free(ptr);
        return;
    }
    alloc->free(alloc, ptr);
}

void *Alloc_Realloc(void *ptr, size_t new_size, allocator_type_t type) {
    allocator_t *alloc = Alloc_GetAllocator(type);
    if (!alloc || !alloc->realloc) {
        // Fall back to system realloc
        return realloc(ptr, new_size);
    }
    return alloc->realloc(alloc, ptr, new_size);
}

// Subsystem-specific functions
void *Render_Alloc(size_t size, const char *tag) {
    return Alloc_Alloc(size, ALLOCATOR_RENDERING, tag);
}

void Render_Free(void *ptr) {
    Alloc_Free(ptr, ALLOCATOR_RENDERING);
}

void *Render_AllocTemp(size_t size, const char *tag) {
    return Alloc_Alloc(size, ALLOCATOR_TEMPORARY, tag);
}

void *Net_Alloc(size_t size, const char *tag) {
    return Alloc_Alloc(size, ALLOCATOR_NETWORKING, tag);
}

void Net_Free(void *ptr) {
    Alloc_Free(ptr, ALLOCATOR_NETWORKING);
}

void *Net_AllocMessage(size_t size) {
    return Alloc_Alloc(size, ALLOCATOR_NETWORKING, "message");
}

void *Audio_Alloc(size_t size, const char *tag) {
    return Alloc_Alloc(size, ALLOCATOR_AUDIO, tag);
}

void Audio_Free(void *ptr) {
    Alloc_Free(ptr, ALLOCATOR_AUDIO);
}

void *Audio_AllocStream(size_t size) {
    return Alloc_Alloc(size, ALLOCATOR_AUDIO, "stream");
}

// Statistics and debugging
void Alloc_PrintStats(void) {
    Com_Printf("Memory Allocator Statistics:\n");
    Com_Printf("===========================\n");

    for (int i = 0; i < ALLOCATOR_COUNT; i++) {
        allocator_t *alloc = g_allocators[i];
        if (!alloc) continue;

        Com_Printf("%s:\n", alloc->name);
        Com_Printf("  Allocated: %zu bytes (peak: %zu)\n", alloc->stats.total_allocated, alloc->stats.peak_allocated);
        Com_Printf("  Operations: %zu alloc, %zu free\n", alloc->stats.allocation_count, alloc->stats.free_count);
        Com_Printf("  Fragmentation: %zu%%\n", alloc->stats.fragmentation_ratio);
        Com_Printf("\n");
    }
}

void Alloc_PrintLeaks(void) {
    Com_Printf("Memory Leak Detection:\n");
    Com_Printf("=====================\n");

    qboolean has_leaks = qfalse;
    for (int i = 0; i < ALLOCATOR_COUNT; i++) {
        allocator_t *alloc = g_allocators[i];
        if (!alloc) continue;

        size_t leaks = alloc->stats.allocation_count - alloc->stats.free_count;
        if (leaks > 0) {
            Com_Printf("LEAK: %s has %zu unfreed allocations (%zu bytes)\n",
                      alloc->name, leaks, alloc->stats.total_allocated);
            has_leaks = qtrue;
        }
    }

    if (!has_leaks) {
        Com_Printf("No memory leaks detected\n");
    }
}

void Alloc_ValidateAll(void) {
    for (int i = 0; i < ALLOCATOR_COUNT; i++) {
        allocator_t *alloc = g_allocators[i];
        if (alloc && alloc->validate) {
            // Validate allocator integrity
            // Implementation depends on specific allocator
        }
    }
}

void Alloc_HandleMemoryPressure(void) {
    // Compact or free temporary allocations
    allocator_t *temp_alloc = Alloc_GetAllocator(ALLOCATOR_TEMPORARY);
    if (temp_alloc && temp_alloc->reset) {
        temp_alloc->reset(temp_alloc);
    }

    // Deflate other allocators if possible
    for (int i = 0; i < ALLOCATOR_COUNT; i++) {
        allocator_t *alloc = g_allocators[i];
        if (alloc && alloc->deflate) {
            alloc->deflate(alloc);
        }
    }
}

double Alloc_GetFragmentationRatio(allocator_type_t type) {
    allocator_t *alloc = Alloc_GetAllocator(type);
    if (!alloc) return 0.0;
    return alloc->stats.fragmentation_ratio / 100.0;
}

size_t Alloc_GetMemoryUsage(allocator_type_t type) {
    allocator_t *alloc = Alloc_GetAllocator(type);
    if (!alloc) return 0;
    return alloc->stats.total_allocated;
}