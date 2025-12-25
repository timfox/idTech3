/*
=============================================================================
Security Configuration Implementation

Security-related functions and validation for hardened builds.
=============================================================================
*/

#include "security_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>

// Security violation tracking
static uint32_t security_violations[SECURITY_VIOLATION_COUNT] = {0};
static security_violation_t last_violations[32]; // Ring buffer
static int last_violation_index = 0;

// Security monitoring
static security_monitor_t security_monitor = {0};

/*
=============================================================================
Security Feature Detection
=============================================================================
*/

void Security_GetFeatures(security_features_t* features) {
    if (!features) return;

    memset(features, 0, sizeof(security_features_t));

    // Detect stack protector
#if defined(__SSP__) || defined(__SSP_STRONG__) || defined(__SSP_ALL__)
    features->stack_protector = qtrue;
#endif

    // Detect AddressSanitizer
#if defined(__SANITIZE_ADDRESS__) || defined(__has_feature) && __has_feature(address_sanitizer)
    features->address_sanitizer = qtrue;
#endif

    // Detect FORTIFY_SOURCE
#if _FORTIFY_SOURCE > 0
    features->fortify_source = qtrue;
#endif

    // Detect PIE
#if defined(__PIE__) || defined(__pie__)
    features->pie = qtrue;
#endif

    // Detect stack clash protection
#if defined(__stack_clash_protection__)
    features->stack_clash_protection = qtrue;
#endif

    // Detect control flow protection
#if defined(__CET__) || defined(__cfi__)
    features->control_flow_protection = qtrue;
#endif

    // ASLR and DEP are OS-level features, assume enabled on modern systems
    features->aslr = qtrue;
    features->dep = qtrue;

    // SafeStack (Clang)
#if defined(__has_feature) && __has_feature(safe_stack)
    features->safe_stack = qtrue;
#endif
}

void Security_PrintFeatures(void) {
    security_features_t features;
    Security_GetFeatures(&features);

    printf("Security Features Status:\n");
    printf("  Stack Protector: %s\n", features.stack_protector ? "YES" : "NO");
    printf("  Address Sanitizer: %s\n", features.address_sanitizer ? "YES" : "NO");
    printf("  FORTIFY_SOURCE: %s\n", features.fortify_source ? "YES" : "NO");
    printf("  Position Independent Executable: %s\n", features.pie ? "YES" : "NO");
    printf("  Stack Clash Protection: %s\n", features.stack_clash_protection ? "YES" : "NO");
    printf("  Control Flow Protection: %s\n", features.control_flow_protection ? "YES" : "NO");
    printf("  Address Space Layout Randomization: %s\n", features.aslr ? "YES" : "NO");
    printf("  Data Execution Prevention: %s\n", features.dep ? "YES" : "NO");
    printf("  SafeStack: %s\n", features.safe_stack ? "YES" : "NO");
}

qboolean Security_ValidateConfiguration(void) {
    security_features_t features;
    Security_GetFeatures(&features);

    // Check that critical security features are enabled
    qboolean valid = qtrue;

    if (!features.stack_protector) {
        printf("WARNING: Stack protector not detected!\n");
        valid = qfalse;
    }

    if (!features.fortify_source) {
        printf("WARNING: FORTIFY_SOURCE not enabled!\n");
        valid = qfalse;
    }

    if (!features.pie) {
        printf("WARNING: Position Independent Executable not enabled!\n");
        valid = qfalse;
    }

    if (valid) {
        printf("Security configuration validation passed.\n");
    } else {
        printf("Security configuration validation failed - some features may not be enabled.\n");
    }

    return valid;
}

/*
=============================================================================
Security Violation Reporting
=============================================================================
*/

void Security_ReportViolation(security_violation_type_t type, const char* description,
                             const char* file, const char* function, int line) {
    if (type >= SECURITY_VIOLATION_COUNT) {
        fprintf(stderr, "Invalid security violation type: %d\n", type);
        return;
    }

    // Increment violation counter
    security_violations[type]++;
    security_monitor.total_violations++;
    security_monitor.violations_by_type[type]++;
    security_monitor.last_violation_time = time(NULL);

    // Store violation details
    security_violation_t* violation = &last_violations[last_violation_index];
    violation->type = type;
    violation->file = file ? file : "unknown";
    violation->function = function ? function : "unknown";
    violation->line = line;
    violation->description = description ? description : "No description";
    violation->timestamp = time(NULL);
    violation->address = 0; // Could be populated with actual address if available

    last_violation_index = (last_violation_index + 1) % 32;

    // Print violation immediately
    fprintf(stderr, "SECURITY VIOLATION [%s]: %s at %s:%d in %s()\n",
            type == SECURITY_VIOLATION_BUFFER_OVERFLOW ? "BUFFER_OVERFLOW" :
            type == SECURITY_VIOLATION_NULL_DEREFERENCE ? "NULL_DEREFERENCE" :
            type == SECURITY_VIOLATION_USE_AFTER_FREE ? "USE_AFTER_FREE" :
            type == SECURITY_VIOLATION_DOUBLE_FREE ? "DOUBLE_FREE" :
            type == SECURITY_VIOLATION_INVALID_FREE ? "INVALID_FREE" :
            type == SECURITY_VIOLATION_STACK_CORRUPTION ? "STACK_CORRUPTION" :
            type == SECURITY_VIOLATION_HEAP_CORRUPTION ? "HEAP_CORRUPTION" :
            type == SECURITY_VIOLATION_FORMAT_STRING ? "FORMAT_STRING" :
            type == SECURITY_VIOLATION_INTEGER_OVERFLOW ? "INTEGER_OVERFLOW" :
            "UNKNOWN",
            description, file, line, function);
}

uint32_t Security_GetViolationCount(security_violation_type_t type) {
    if (type >= SECURITY_VIOLATION_COUNT) return 0;
    return security_violations[type];
}

void Security_PrintViolations(void) {
    printf("Security Violations Summary:\n");

    for (int i = 0; i < SECURITY_VIOLATION_COUNT; i++) {
        if (security_violations[i] > 0) {
            printf("  %s: %u\n",
                   i == SECURITY_VIOLATION_BUFFER_OVERFLOW ? "Buffer Overflow" :
                   i == SECURITY_VIOLATION_NULL_DEREFERENCE ? "NULL Dereference" :
                   i == SECURITY_VIOLATION_USE_AFTER_FREE ? "Use After Free" :
                   i == SECURITY_VIOLATION_DOUBLE_FREE ? "Double Free" :
                   i == SECURITY_VIOLATION_INVALID_FREE ? "Invalid Free" :
                   i == SECURITY_VIOLATION_STACK_CORRUPTION ? "Stack Corruption" :
                   i == SECURITY_VIOLATION_HEAP_CORRUPTION ? "Heap Corruption" :
                   i == SECURITY_VIOLATION_FORMAT_STRING ? "Format String" :
                   i == SECURITY_VIOLATION_INTEGER_OVERFLOW ? "Integer Overflow" :
                   "Unknown",
                   security_violations[i]);
        }
    }

    if (security_monitor.total_violations == 0) {
        printf("  No security violations detected.\n");
    }
}

/*
=============================================================================
Security Monitoring
=============================================================================
*/

void Security_EnableMonitoring(qboolean enable) {
    security_monitor.monitoring_enabled = enable;
}

void Security_GetMonitorStats(security_monitor_t* stats) {
    if (stats) {
        memcpy(stats, &security_monitor, sizeof(security_monitor_t));
    }
}

/*
=============================================================================
Secure Random Number Generation
=============================================================================
*/

uint32_t Security_RandomUint32(void) {
    uint32_t result;
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        ssize_t bytes_read = read(fd, &result, sizeof(result));
        close(fd);
        if (bytes_read == sizeof(result)) {
            return result;
        }
    }

    // Fallback to rand() if /dev/urandom fails
    srand((unsigned int)time(NULL));
    return (uint32_t)rand();
}

uint64_t Security_RandomUint64(void) {
    uint64_t result;
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        ssize_t bytes_read = read(fd, &result, sizeof(result));
        close(fd);
        if (bytes_read == sizeof(result)) {
            return result;
        }
    }

    // Fallback to rand() if /dev/urandom fails
    srand((unsigned int)time(NULL));
    return ((uint64_t)rand() << 32) | rand();
}

void Security_RandomBytes(void* buffer, size_t size) {
    if (!buffer || size == 0) return;

    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        size_t total_read = 0;
        while (total_read < size) {
            ssize_t bytes_read = read(fd, (char*)buffer + total_read, size - total_read);
            if (bytes_read <= 0) break;
            total_read += bytes_read;
        }
        close(fd);
        if (total_read == size) return;
    }

    // Fallback to rand() if /dev/urandom fails
    srand((unsigned int)time(NULL));
    for (size_t i = 0; i < size; i++) {
        ((unsigned char*)buffer)[i] = (unsigned char)rand();
    }
}

/*
=============================================================================
Secure Memory Operations
=============================================================================
*/

void Security_SecureZero(void* buffer, size_t size) {
    if (!buffer || size == 0) return;

    // Use volatile to prevent compiler optimization
    volatile char* ptr = (volatile char*)buffer;
    while (size--) {
        *ptr++ = 0;
    }
}

int Security_SecureCompare(const void* a, const void* b, size_t size) {
    if (!a || !b) return a == b ? 0 : 1;

    const volatile unsigned char* pa = (const volatile unsigned char*)a;
    const volatile unsigned char* pb = (const volatile unsigned char*)b;

    int result = 0;
    while (size--) {
        result |= *pa++ ^ *pb++;
    }

    return result;
}

/*
=============================================================================
Security-Aware File Operations
=============================================================================
*/

FILE* Security_FOpen(const char* filename, const char* mode) {
    SECURITY_CHECK_NULL(filename);
    SECURITY_CHECK_NULL(mode);
    SECURITY_CHECK_STRING_LENGTH(filename, SECURITY_MAX_PATH_LENGTH);

    return fopen(filename, mode);
}

size_t Security_FRead(void* buffer, size_t size, size_t count, FILE* stream) {
    SECURITY_CHECK_NULL(buffer);
    SECURITY_CHECK_NULL(stream);

    size_t total_size = size * count;
    SECURITY_ASSERT(total_size < SECURITY_MAX_BUFFER_SIZE, "Read size too large");

    return fread(buffer, size, count, stream);
}

size_t Security_FWrite(const void* buffer, size_t size, size_t count, FILE* stream) {
    SECURITY_CHECK_NULL(buffer);
    SECURITY_CHECK_NULL(stream);

    size_t total_size = size * count;
    SECURITY_ASSERT(total_size < SECURITY_MAX_BUFFER_SIZE, "Write size too large");

    return fwrite(buffer, size, count, stream);
}

/*
=============================================================================
Security Validation Functions
=============================================================================
*/

qboolean Security_ValidatePointer(const void* ptr) {
    // Basic validation - check if pointer is not NULL and aligned
    if (!ptr) return qfalse;

    // Check alignment (assume at least 4-byte alignment for most systems)
    uintptr_t addr = (uintptr_t)ptr;
    if (addr % 4 != 0) return qfalse;

    return qtrue;
}

qboolean Security_ValidateString(const char* str, size_t max_length) {
    if (!str) return qfalse;

    size_t len = strlen(str);
    if (len >= max_length) return qfalse;

    // Check for null bytes in the middle (not a valid C string)
    for (size_t i = 0; i < len; i++) {
        if (str[i] == '\0' && i < len - 1) return qfalse;
    }

    return qtrue;
}

qboolean Security_ValidateBuffer(const void* buffer, size_t size) {
    if (!buffer) return qfalse;
    if (size == 0 || size > SECURITY_MAX_BUFFER_SIZE) return qfalse;

    // Could add more sophisticated validation here
    return Security_ValidatePointer(buffer);
}

qboolean Security_ValidatePath(const char* path) {
    if (!Security_ValidateString(path, SECURITY_MAX_PATH_LENGTH)) return qfalse;

    // Check for directory traversal attempts
    if (strstr(path, "..") != NULL) return qfalse;
    if (strstr(path, "/../") != NULL) return qfalse;
    if (strstr(path, "\\..\\") != NULL) return qfalse;

    // Check for absolute paths if not allowed
    if (path[0] == '/' || (path[0] && path[1] == ':')) return qfalse;

    return qtrue;
}
