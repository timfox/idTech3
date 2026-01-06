/*
===========================================================================
q_memory_safety.h - Enhanced Memory Safety and Bounds Checking
===========================================================================
*/

#ifndef __Q_MEMORY_SAFETY_H__
#define __Q_MEMORY_SAFETY_H__

#include "q_shared.h"

// Memory safety statistics (thread-safe)
typedef struct {
    atomic_int_t total_allocations;
    atomic_int_t total_frees;
    atomic_int_t current_memory;
    atomic_int_t peak_memory;
    atomic_int_t leak_count;
    atomic_int_t leak_size;
    atomic_int_t corruption_detected;
    atomic_int_t bounds_violations;
} memory_safety_stats_t;

// Memory safety state
typedef struct {
    qboolean initialized;
    qboolean bounds_checking_active;
    qboolean corruption_detection_active;
    qboolean leak_detection_active;
} memory_safety_state_t;

// Memory allocation tracking
typedef struct memory_allocation_s {
    void *ptr;
    size_t size;
    const char *file;
    int line;
    qboolean freed;
    int allocation_time;
    struct memory_allocation_s *next;
} memory_allocation_t;

// Function declarations
void MemorySafety_Init(void);
void MemorySafety_RegisterCVars(void);
void MemorySafety_Shutdown(void);

// Memory management with safety
void *MemorySafety_Malloc(size_t size, const char *file, int line);
void MemorySafety_Free(void *ptr);

// Validation functions
qboolean MemorySafety_ValidatePointer(const void *ptr, size_t access_size, const char *context);

// Leak detection
void MemorySafety_CheckLeaks(void);

// Statistics and debugging
const memory_safety_stats_t *MemorySafety_GetStats(void);
void MemorySafety_DumpAllocations(void);

// Safe standard library functions
size_t MemorySafety_Strlcpy(char *dst, const char *src, size_t dstsize);
size_t MemorySafety_Strlcat(char *dst, const char *src, size_t dstsize);
void *MemorySafety_Memcpy(void *dst, const void *src, size_t n);
void *MemorySafety_Memset(void *s, int c, size_t n);

// Convenience macros
#define MEMORY_SAFETY_MALLOC(size) MemorySafety_Malloc(size, __FILE__, __LINE__)
#define MEMORY_SAFETY_FREE(ptr) MemorySafety_Free(ptr)

#define MEMORY_SAFETY_VALIDATE_PTR(ptr, size) MemorySafety_ValidatePointer(ptr, size, #ptr)

#define MEMORY_SAFETY_STRLCPY(dst, src, size) MemorySafety_Strlcpy(dst, src, size)
#define MEMORY_SAFETY_STRLCAT(dst, src, size) MemorySafety_Strlcat(dst, src, size)
#define MEMORY_SAFETY_MEMCPY(dst, src, n) MemorySafety_Memcpy(dst, src, n)
#define MEMORY_SAFETY_MEMSET(s, c, n) MemorySafety_Memset(s, c, n)

#endif // __Q_MEMORY_SAFETY_H__