/*
===========================================================================
Memory Safety Utilities

Enhanced string and buffer operations with bounds checking and validation.
===========================================================================
*/

#ifndef __Q_MEMORY_SAFETY_H__
#define __Q_MEMORY_SAFETY_H__

#include "q_shared.h"

// Enhanced string operations with validation
#ifdef __cplusplus
extern "C" {
#endif

// Safe string copy with validation - returns success/failure
qboolean Q_strncpyz_safe(char *dest, const char *src, size_t destsize, const char *context);

// Safe string concatenation with validation
qboolean Q_strcat_safe(char *dest, const char *src, size_t destsize, const char *context);

// Safe sprintf with buffer validation
int Q_snprintf_safe(char *dest, size_t destsize, const char *fmt, ...) __attribute__ ((format (printf, 3, 4)));

// Validate buffer size at compile time where possible
#define Q_STATIC_ASSERT_BUFFER_SIZE(buf, expected_size) \
    _Static_assert(sizeof(buf) >= (expected_size), "Buffer size mismatch")

// Runtime buffer validation macro
#define Q_VALIDATE_BUFFER(buf, required_size, context) \
    do { \
        if (!buf) { \
            Com_Printf("ERROR: NULL buffer in %s at %s:%d\n", context, __FILE__, __LINE__); \
            return qfalse; \
        } \
        if (sizeof(buf) < (required_size)) { \
            Com_Printf("ERROR: Buffer too small (%zu < %zu) in %s at %s:%d\n", \
                      sizeof(buf), (size_t)(required_size), context, __FILE__, __LINE__); \
            return qfalse; \
        } \
    } while(0)

// Enhanced Q_strncpyz that validates the destination buffer size
#define Q_strncpyz_validated(dest, src, context) \
    Q_strncpyz_safe((dest), (src), sizeof(dest), context)

// Enhanced Com_sprintf that validates buffer size
#define Com_sprintf_safe(dest, fmt, ...) \
    Q_snprintf_safe((dest), sizeof(dest), fmt, ##__VA_ARGS__)

// Macro to check for truncation in string operations
#define Q_CHECK_TRUNCATION(dest, src, context) \
    do { \
        size_t src_len = strlen(src); \
        size_t dest_size = sizeof(dest); \
        if (src_len >= dest_size) { \
            Com_Printf("WARNING: String truncation in %s: %zu >= %zu bytes\n", \
                      context, src_len, dest_size); \
        } \
    } while(0)

// Safe memory operations
#define Q_memcpy_safe(dest, src, n, dest_size, context) \
    do { \
        if ((n) > (dest_size)) { \
            Com_Printf("ERROR: memcpy bounds violation in %s: %zu > %zu\n", \
                      context, (size_t)(n), (size_t)(dest_size)); \
            Com_Error(ERR_DROP, "Memory bounds violation"); \
        } \
        memcpy((dest), (src), (n)); \
    } while(0)

#ifdef __cplusplus
}
#endif

#endif // __Q_MEMORY_SAFETY_H__