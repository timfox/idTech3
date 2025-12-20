/*
===========================================================================
q_stability.h - Engine Stability and Hardening Framework
===========================================================================
*/

#ifndef __Q_STABILITY_H__
#define __Q_STABILITY_H__

#include "q_shared.h"

// Stability event types
typedef enum {
    STABILITY_DEBUG,
    STABILITY_INFO,
    STABILITY_WARNING,
    STABILITY_ERROR,
    STABILITY_EVENT_COUNT
} stability_event_type_t;

// Assertion levels
typedef enum {
    STABILITY_ASSERT_OFF,
    STABILITY_ASSERT_WARNING,
    STABILITY_ASSERT_FATAL,
    STABILITY_ASSERT_DEBUG
} stability_assert_level_t;

// Stability statistics
typedef struct {
    int frame_count;
    float uptime;
    int event_count[STABILITY_EVENT_COUNT];
    int memory_allocations;
    int memory_frees;
    int thread_operations;
    int validation_checks;
    int recovery_attempts;
    int crash_count;
} stability_stats_t;

// Stability state
typedef struct {
    qboolean initialized;
    qboolean memory_guard_active;
    qboolean thread_safety_active;
    qboolean input_validation_active;
    qboolean resource_limits_active;
    qboolean crash_recovery_active;
    qboolean performance_monitoring_active;
} stability_state_t;

// Thread safety mutex (platform-specific)
#ifdef _WIN32
#include <windows.h>
typedef struct {
    CRITICAL_SECTION cs;
} stability_mutex_t;
#else
#include <pthread.h>
typedef struct {
    pthread_mutex_t mutex;
} stability_mutex_t;
#endif

// Function declarations
void Stability_Init(void);
void Stability_Shutdown(void);
void Stability_Frame(void);

// Assertion and validation
void Stability_Assert(qboolean condition, const char *expression, const char *file, int line, const char *function);
qboolean Stability_ValidatePointer(const void *ptr, const char *context);
qboolean Stability_ValidateString(const char *str, size_t max_length, const char *context);
void Stability_SanitizeInput(char *input, size_t max_length);

// Statistics and monitoring
const stability_stats_t *Stability_GetStats(void);

// Thread safety
void Stability_MutexInit(stability_mutex_t *mutex);
void Stability_MutexDestroy(stability_mutex_t *mutex);
void Stability_MutexLock(stability_mutex_t *mutex);
void Stability_MutexUnlock(stability_mutex_t *mutex);

// Convenience macros
#define STABILITY_ASSERT(condition) Stability_Assert(condition, #condition, __FILE__, __LINE__, __FUNCTION__)
#define STABILITY_VALIDATE_PTR(ptr) Stability_ValidatePointer(ptr, #ptr)
#define STABILITY_VALIDATE_STR(str, max_len) Stability_ValidateString(str, max_len, #str)

// Subsystem initialization (implemented in q_stability.c)
// These are internal functions called by Stability_Init/Shutdown

#endif // __Q_STABILITY_H__
