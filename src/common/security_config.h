/*
=============================================================================
Security Configuration

Security-related constants, macros, and configuration for hardened builds.
=============================================================================
*/

#ifndef __SECURITY_CONFIG_H__
#define __SECURITY_CONFIG_H__

#include "q_shared.h"

// Security feature detection
#ifdef __GNUC__
#define COMPILER_GCC 1
#endif

#ifdef __clang__
#define COMPILER_CLANG 1
#endif

#ifdef _MSC_VER
#define COMPILER_MSVC 1
#endif

// Stack canary configuration
#define SECURITY_STACK_CANARY_SIZE 8

// FORTIFY_SOURCE level
#ifndef _FORTIFY_SOURCE
#define _FORTIFY_SOURCE 2
#endif

// Security assertion macros
#define SECURITY_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "SECURITY VIOLATION: %s at %s:%d\n", message, __FILE__, __LINE__); \
            abort(); \
        } \
    } while(0)

// Bounds checking macros
#define SECURITY_CHECK_BOUNDS(index, size) \
    SECURITY_ASSERT((size_t)(index) < (size_t)(size), "Array index out of bounds")

#define SECURITY_CHECK_BUFFER_SIZE(buffer, requested, available) \
    SECURITY_ASSERT((size_t)(requested) <= (size_t)(available), "Buffer size exceeded")

// Memory protection macros
#define SECURITY_CHECK_NULL(ptr) \
    SECURITY_ASSERT((ptr) != NULL, "NULL pointer dereference")

#define SECURITY_CHECK_ALLOCATION(ptr) \
    SECURITY_ASSERT((ptr) != NULL, "Memory allocation failed")

// String security macros
#define SECURITY_CHECK_STRING_LENGTH(str, max_len) \
    SECURITY_ASSERT(str && strlen(str) < (size_t)(max_len), "String length exceeds maximum")

#define SECURITY_CHECK_STRING_COPY(dest, src, dest_size) \
    SECURITY_ASSERT(dest && src && dest_size > 0, "Invalid string copy parameters"); \
    SECURITY_ASSERT(strlen(src) < dest_size, "String copy would overflow destination")

// Security-aware string functions
static inline size_t security_strlen(const char* str) {
    SECURITY_CHECK_NULL(str);
    return strlen(str);
}

static inline char* security_strcpy(char* dest, const char* src, size_t dest_size) {
    SECURITY_CHECK_STRING_COPY(dest, src, dest_size);
    return strncpy(dest, src, dest_size - 1);
}

static inline char* security_strcat(char* dest, const char* src, size_t dest_size) {
    size_t dest_len = security_strlen(dest);
    size_t src_len = security_strlen(src);
    SECURITY_ASSERT(dest_len + src_len < dest_size, "String concatenation would overflow");
    return strncat(dest, src, dest_size - dest_len - 1);
}

// Memory security functions
static inline void* security_malloc(size_t size) {
    SECURITY_ASSERT(size > 0 && size < (1024 * 1024 * 1024), "Invalid allocation size"); // Max 1GB
    void* ptr = malloc(size);
    SECURITY_CHECK_ALLOCATION(ptr);
    return ptr;
}

static inline void* security_calloc(size_t count, size_t size) {
    SECURITY_ASSERT(count > 0 && size > 0, "Invalid calloc parameters");
    SECURITY_ASSERT(count * size < (1024 * 1024 * 1024), "Invalid allocation size"); // Max 1GB
    void* ptr = calloc(count, size);
    SECURITY_CHECK_ALLOCATION(ptr);
    return ptr;
}

static inline void security_free(void* ptr) {
    if (ptr) {
        free(ptr);
    }
}

// Security configuration constants
#define SECURITY_MAX_PATH_LENGTH 4096
#define SECURITY_MAX_BUFFER_SIZE (1024 * 1024) // 1MB max buffer
#define SECURITY_MAX_STRING_LENGTH 32768       // 32KB max string
#define SECURITY_MAX_ALLOCATION_SIZE (1024 * 1024 * 1024) // 1GB max allocation

// Stack protection validation
#define SECURITY_STACK_COOKIE_VALUE 0xDEADBEEF

// Control flow protection
#if defined(__GNUC__) && defined(__x86_64__)
#define SECURITY_HAS_CET 1
#endif

// Address space layout randomization
#define SECURITY_ASLR_ENABLED 1

// Data Execution Prevention
#define SECURITY_DEP_ENABLED 1

// Security feature status reporting
typedef struct {
    qboolean stack_protector;
    qboolean address_sanitizer;
    qboolean fortify_source;
    qboolean relro;
    qboolean pie;
    qboolean stack_clash_protection;
    qboolean control_flow_protection;
    qboolean aslr;
    qboolean dep;
    qboolean safe_stack;
} security_features_t;

void Security_GetFeatures(security_features_t* features);
void Security_PrintFeatures(void);
qboolean Security_ValidateConfiguration(void);

// Security violation reporting
typedef enum {
    SECURITY_VIOLATION_BUFFER_OVERFLOW,
    SECURITY_VIOLATION_NULL_DEREFERENCE,
    SECURITY_VIOLATION_USE_AFTER_FREE,
    SECURITY_VIOLATION_DOUBLE_FREE,
    SECURITY_VIOLATION_INVALID_FREE,
    SECURITY_VIOLATION_STACK_CORRUPTION,
    SECURITY_VIOLATION_HEAP_CORRUPTION,
    SECURITY_VIOLATION_FORMAT_STRING,
    SECURITY_VIOLATION_INTEGER_OVERFLOW,
    SECURITY_VIOLATION_COUNT
} security_violation_type_t;

typedef struct {
    security_violation_type_t type;
    const char* file;
    int line;
    const char* function;
    const char* description;
    uint64_t timestamp;
    uintptr_t address; // Memory address where violation occurred
} security_violation_t;

void Security_ReportViolation(security_violation_type_t type, const char* description,
                             const char* file, const char* function, int line);
uint32_t Security_GetViolationCount(security_violation_type_t type);
void Security_PrintViolations(void);

// Security monitoring
typedef struct {
    uint64_t total_violations;
    uint64_t violations_by_type[SECURITY_VIOLATION_COUNT];
    uint64_t last_violation_time;
    qboolean monitoring_enabled;
} security_monitor_t;

void Security_EnableMonitoring(qboolean enable);
void Security_GetMonitorStats(security_monitor_t* stats);

// Secure random number generation
uint32_t Security_RandomUint32(void);
uint64_t Security_RandomUint64(void);
void Security_RandomBytes(void* buffer, size_t size);

// Secure memory operations
void Security_SecureZero(void* buffer, size_t size);
int Security_SecureCompare(const void* a, const void* b, size_t size);

// Security-aware file operations
FILE* Security_FOpen(const char* filename, const char* mode);
size_t Security_FRead(void* buffer, size_t size, size_t count, FILE* stream);
size_t Security_FWrite(const void* buffer, size_t size, size_t count, FILE* stream);

// Security validation functions
qboolean Security_ValidatePointer(const void* ptr);
qboolean Security_ValidateString(const char* str, size_t max_length);
qboolean Security_ValidateBuffer(const void* buffer, size_t size);
qboolean Security_ValidatePath(const char* path);

// Compiler security attribute macros
#if defined(__GNUC__) || defined(__clang__)
#define SECURITY_NORETURN __attribute__((noreturn))
#define SECURITY_FORMAT(type, index, first) __attribute__((format(type, index, first)))
#define SECURITY_NONNULL(...) __attribute__((nonnull(__VA_ARGS__)))
#define SECURITY_MALLOC __attribute__((malloc))
#define SECURITY_ALLOCSZ(size_index) __attribute__((alloc_size(size_index)))
#define SECURITY_ALLOCSZ_PRODUCT(size1, size2) __attribute__((alloc_size(size1, size2)))
#else
#define SECURITY_NORETURN
#define SECURITY_FORMAT(type, index, first)
#define SECURITY_NONNULL(...)
#define SECURITY_MALLOC
#define SECURITY_ALLOCSZ(size_index)
#define SECURITY_ALLOCSZ_PRODUCT(size1, size2)
#endif

#endif // __SECURITY_CONFIG_H__
