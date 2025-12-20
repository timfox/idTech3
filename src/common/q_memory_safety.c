/*
===========================================================================
q_memory_safety.c - Enhanced Memory Safety and Bounds Checking
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "q_memory_safety.h"

// Memory canary values
#define MEMORY_CANARY_VALUE 0xDEADBEEF
#define MEMORY_CANARY_SIZE 4
#define MEMORY_CANARY_COUNT 2  // Before and after allocation

// Memory block header for tracking
typedef struct memory_block_s {
    size_t size;
    uint32_t canary_before;
    const char *file;
    int line;
    uint32_t canary_after;
    qboolean freed;
    uint32_t magic;  // For validation
} memory_block_t;

#define MEMORY_BLOCK_MAGIC 0xCAFEBABE
#define MEMORY_BLOCK_OVERHEAD (sizeof(memory_block_t) + (MEMORY_CANARY_SIZE * MEMORY_CANARY_COUNT))

// Forward declarations for static functions called before definition
static qboolean MemorySafety_ValidateBlock(memory_block_t *block, void *user_ptr);
static void MemorySafety_TrackAllocation(void *ptr, size_t size, const char *file, int line);
static void MemorySafety_UntrackAllocation(void *ptr);


// Memory safety configuration
cvar_t *memory_safety_enable;
cvar_t *memory_bounds_checking;
cvar_t *memory_corruption_detection;
cvar_t *memory_leak_detection;
cvar_t *memory_canary_protection;
cvar_t *memory_double_free_detection;
cvar_t *memory_use_after_free_detection;
cvar_t *memory_buffer_overflow_protection;

// Memory safety statistics
static memory_safety_stats_t memory_stats;
static memory_safety_state_t memory_state;

// Memory allocation tracking
static memory_allocation_t *allocation_list = NULL;
static int allocation_count = 0;
static int max_allocations = 0;

/*
===============
MemorySafety_Init
===============
*/
void MemorySafety_Init(void) {
    Com_Memset(&memory_stats, 0, sizeof(memory_stats));
    Com_Memset(&memory_state, 0, sizeof(memory_state));

    // Register CVars
    memory_safety_enable = Cvar_Get("memory_safety_enable", "1", CVAR_ARCHIVE | CVAR_LATCH);
    memory_bounds_checking = Cvar_Get("memory_bounds_checking", "1", CVAR_ARCHIVE);
    memory_corruption_detection = Cvar_Get("memory_corruption_detection", "1", CVAR_ARCHIVE);
    memory_leak_detection = Cvar_Get("memory_leak_detection", "1", CVAR_ARCHIVE);
    memory_canary_protection = Cvar_Get("memory_canary_protection", "1", CVAR_ARCHIVE);
    memory_double_free_detection = Cvar_Get("memory_double_free_detection", "1", CVAR_ARCHIVE);
    memory_use_after_free_detection = Cvar_Get("memory_use_after_free_detection", "1", CVAR_ARCHIVE);
    memory_buffer_overflow_protection = Cvar_Get("memory_buffer_overflow_protection", "1", CVAR_ARCHIVE);

    memory_state.initialized = qtrue;
    Com_Printf("Memory safety framework initialized\n");
}

/*
===============
MemorySafety_Shutdown
===============
*/
void MemorySafety_Shutdown(void) {
    if (!memory_state.initialized) {
        return;
    }

    // Check for memory leaks
    if (memory_leak_detection->integer) {
        MemorySafety_CheckLeaks();
    }

    // Free all tracked allocations
    memory_allocation_t *current = allocation_list;
    while (current) {
        memory_allocation_t *next = current->next;
        if (!current->freed) {
            Com_Printf(S_COLOR_YELLOW "WARNING: Memory leak detected at %s:%d (%zu bytes)\n",
                current->file, current->line, current->size);
        }
        MemorySafety_Free(current);
        current = next;
    }

    Com_Printf("Memory safety framework shutdown\n");
    memory_state.initialized = qfalse;
}

/*
===============
MemorySafety_Malloc
===============
*/
void *MemorySafety_Malloc(size_t size, const char *file, int line) {
    if (!memory_safety_enable->integer || !memory_state.initialized) {
        return Z_Malloc(size);
    }

    size_t total_size = size + MEMORY_BLOCK_OVERHEAD;
    void *raw_ptr = Z_Malloc(total_size);

    if (!raw_ptr) {
        Com_Error(ERR_FATAL, "MemorySafety_Malloc: Failed to allocate %zu bytes", total_size);
        return NULL;
    }

    // Set up memory block header
    memory_block_t *block = (memory_block_t *)raw_ptr;
    block->size = size;
    block->canary_before = MEMORY_CANARY_VALUE;
    block->file = file;
    block->line = line;
    block->canary_after = MEMORY_CANARY_VALUE;
    block->freed = qfalse;
    block->magic = MEMORY_BLOCK_MAGIC;

    // Set canaries
    uint32_t *canary_after = (uint32_t *)((char *)raw_ptr + sizeof(memory_block_t) + size);
    *canary_after = MEMORY_CANARY_VALUE;

    // Get user pointer
    void *user_ptr = (char *)raw_ptr + sizeof(memory_block_t);

    // Track allocation
    MemorySafety_TrackAllocation(user_ptr, size, file, line);

    memory_stats.total_allocations++;
    memory_stats.current_memory += size;

    if (memory_stats.current_memory > memory_stats.peak_memory) {
        memory_stats.peak_memory = memory_stats.current_memory;
    }

    return user_ptr;
}

/*
===============
MemorySafety_Free
===============
*/
void MemorySafety_Free(void *ptr) {
    if (!ptr) {
        return;
    }

    if (!memory_safety_enable->integer || !memory_state.initialized) {
        Z_Free(ptr);
        return;
    }

    // Get block header
    memory_block_t *block = (memory_block_t *)((char *)ptr - sizeof(memory_block_t));

    // Validate block
    if (!MemorySafety_ValidateBlock(block, ptr)) {
        Com_Error(ERR_FATAL, "MemorySafety_Free: Invalid memory block");
        return;
    }

    // Check for double free
    if (memory_double_free_detection->integer && block->freed) {
        Com_Error(ERR_FATAL, "MemorySafety_Free: Double free detected at %s:%d",
            block->file, block->line);
        return;
    }

    // Mark as freed
    block->freed = qtrue;

    // Clear the memory to prevent use-after-free
    Com_Memset(ptr, 0xDD, block->size);

    // Remove from tracking
    MemorySafety_UntrackAllocation(ptr);

    memory_stats.total_frees++;
    memory_stats.current_memory -= block->size;

    // Get raw pointer and free
    void *raw_ptr = block;
    Z_Free(raw_ptr);
}

/*
===============
MemorySafety_ValidateBlock
===============
*/
static qboolean MemorySafety_ValidateBlock(memory_block_t *block, void *user_ptr) {
    // Check magic number
    if (block->magic != MEMORY_BLOCK_MAGIC) {
        Com_Printf(S_COLOR_RED "ERROR: Invalid memory block magic (0x%08X)\n", block->magic);
        return qfalse;
    }

    // Check canaries
    if (memory_canary_protection->integer) {
        if (block->canary_before != MEMORY_CANARY_VALUE) {
            Com_Printf(S_COLOR_RED "ERROR: Memory corruption detected (before canary)\n");
            return qfalse;
        }

        uint32_t *canary_after = (uint32_t *)((char *)user_ptr + block->size);
        if (*canary_after != MEMORY_CANARY_VALUE) {
            Com_Printf(S_COLOR_RED "ERROR: Buffer overflow detected (after canary)\n");
            return qfalse;
        }
    }

    return qtrue;
}

/*
===============
MemorySafety_TrackAllocation
===============
*/
static void MemorySafety_TrackAllocation(void *ptr, size_t size, const char *file, int line) {
    memory_allocation_t *allocation = (memory_allocation_t *)Z_Malloc(sizeof(memory_allocation_t));

    allocation->ptr = ptr;
    allocation->size = size;
    allocation->file = file;
    allocation->line = line;
    allocation->freed = qfalse;
    allocation->allocation_time = Sys_Milliseconds();
    allocation->next = allocation_list;

    allocation_list = allocation;
    allocation_count++;

    if (allocation_count > max_allocations) {
        max_allocations = allocation_count;
    }
}

/*
===============
MemorySafety_UntrackAllocation
===============
*/
static void MemorySafety_UntrackAllocation(void *ptr) {
    memory_allocation_t *current = allocation_list;
    memory_allocation_t *prev = NULL;

    while (current) {
        if (current->ptr == ptr) {
            if (prev) {
                prev->next = current->next;
            } else {
                allocation_list = current->next;
            }

            Z_Free(current);
            allocation_count--;
            return;
        }

        prev = current;
        current = current->next;
    }

    Com_Printf(S_COLOR_YELLOW "WARNING: Attempted to untrack unknown allocation\n");
}

/*
===============
MemorySafety_ValidatePointer
===============
*/
qboolean MemorySafety_ValidatePointer(const void *ptr, size_t access_size, const char *context) {
    if (!memory_safety_enable->integer || !memory_bounds_checking->integer) {
        return qtrue;
    }

    if (!ptr) {
        Com_Printf(S_COLOR_RED "ERROR: NULL pointer access in %s\n", context);
        return qfalse;
    }

    // Find the allocation
    memory_allocation_t *allocation = allocation_list;
    while (allocation) {
        if (allocation->ptr == ptr) {
            if (allocation->freed) {
                Com_Printf(S_COLOR_RED "ERROR: Use-after-free detected in %s (%s:%d)\n",
                    context, allocation->file, allocation->line);
                return qfalse;
            }

            // Check bounds
            if (access_size > allocation->size) {
                Com_Printf(S_COLOR_RED "ERROR: Buffer overflow detected in %s (access %zu, allocated %zu)\n",
                    context, access_size, allocation->size);
                return qfalse;
            }

            return qtrue;
        }
        allocation = allocation->next;
    }

    Com_Printf(S_COLOR_YELLOW "WARNING: Access to untracked memory in %s\n", context);
    return qtrue; // Allow but warn
}

/*
===============
MemorySafety_CheckLeaks
===============
*/
void MemorySafety_CheckLeaks(void) {
    int leak_count = 0;
    size_t leak_size = 0;

    memory_allocation_t *current = allocation_list;
    while (current) {
        if (!current->freed) {
            Com_Printf(S_COLOR_YELLOW "MEMORY LEAK: %zu bytes allocated at %s:%d\n",
                current->size, current->file, current->line);
            leak_count++;
            leak_size += current->size;
        }
        current = current->next;
    }

    if (leak_count > 0) {
        Com_Printf(S_COLOR_RED "MEMORY LEAKS DETECTED: %d leaks, %zu bytes total\n",
            leak_count, leak_size);
        memory_stats.leak_count = leak_count;
        memory_stats.leak_size = leak_size;
    } else {
        Com_Printf(S_COLOR_GREEN "No memory leaks detected\n");
    }
}

/*
===============
MemorySafety_GetStats
===============
*/
const memory_safety_stats_t *MemorySafety_GetStats(void) {
    return &memory_stats;
}

/*
===============
MemorySafety_DumpAllocations
===============
*/
void MemorySafety_DumpAllocations(void) {
    Com_Printf("=== MEMORY ALLOCATIONS DUMP ===\n");

    memory_allocation_t *current = allocation_list;
    int count = 0;

    while (current && count < 50) {  // Limit output
        const char *status = current->freed ? "FREED" : "ACTIVE";
        Com_Printf("%s: %zu bytes at %s:%d (%d ms ago)\n",
            status, current->size, current->file, current->line,
            Sys_Milliseconds() - current->allocation_time);
        current = current->next;
        count++;
    }

    if (allocation_count > 50) {
        Com_Printf("... and %d more allocations\n", allocation_count - 50);
    }

    Com_Printf("Total allocations: %d, Current memory: %d bytes\n",
        allocation_count, memory_stats.current_memory);
}

/*
===============
Enhanced Memory Functions with Safety
===============
*/

// Safe string copy with bounds checking
size_t MemorySafety_Strlcpy(char *dst, const char *src, size_t dstsize) {
    if (!dst || !src || dstsize == 0) {
        return 0;
    }

    // Validate pointers
    if (!MemorySafety_ValidatePointer(dst, dstsize, "strlcpy_dst") ||
        !MemorySafety_ValidatePointer(src, strlen(src) + 1, "strlcpy_src")) {
        return 0;
    }

    size_t srclen = strlen(src);
    size_t copylen = srclen;

    if (copylen >= dstsize) {
        copylen = dstsize - 1;
    }

    memcpy(dst, src, copylen);
    dst[copylen] = '\0';

    return srclen;
}

// Safe string concatenation
size_t MemorySafety_Strlcat(char *dst, const char *src, size_t dstsize) {
    if (!dst || !src || dstsize == 0) {
        return 0;
    }

    // Validate pointers
    if (!MemorySafety_ValidatePointer(dst, dstsize, "strlcat_dst") ||
        !MemorySafety_ValidatePointer(src, strlen(src) + 1, "strlcat_src")) {
        return 0;
    }

    size_t dstlen = strlen(dst);
    size_t srclen = strlen(src);
    size_t total_len = dstlen + srclen;

    if (dstlen >= dstsize) {
        return srclen + dstsize;  // dst is already full
    }

    size_t copylen = dstsize - dstlen - 1;
    if (copylen > srclen) {
        copylen = srclen;
    }

    memcpy(dst + dstlen, src, copylen);
    dst[dstlen + copylen] = '\0';

    return total_len;
}

// Safe memory copy
void *MemorySafety_Memcpy(void *dst, const void *src, size_t n) {
    if (!dst || !src || n == 0) {
        return dst;
    }

    // Validate pointers
    if (!MemorySafety_ValidatePointer(dst, n, "memcpy_dst") ||
        !MemorySafety_ValidatePointer(src, n, "memcpy_src")) {
        return NULL;
    }

    return memcpy(dst, src, n);
}

// Safe memory set
void *MemorySafety_Memset(void *s, int c, size_t n) {
    if (!s || n == 0) {
        return s;
    }

    // Validate pointer
    if (!MemorySafety_ValidatePointer(s, n, "memset")) {
        return NULL;
    }

    return memset(s, c, n);
}