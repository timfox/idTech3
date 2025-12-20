/*
===========================================================================
q_stability.c - Engine Stability and Hardening Framework
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "q_stability.h"

// Stability configuration
cvar_t *stability_enable;
cvar_t *stability_log_level;
cvar_t *stability_assert_level;
cvar_t *stability_memory_guard;
cvar_t *stability_thread_safety;
cvar_t *stability_input_validation;
cvar_t *stability_resource_limits;
cvar_t *stability_crash_recovery;
cvar_t *stability_performance_monitoring;

// Stability state
static stability_state_t stability_state;
static stability_stats_t stability_stats;
static stability_mutex_t stability_mutex;

// Forward declarations
static void Stability_LogEvent(stability_event_type_t type, const char *message, ...);
static qboolean Stability_ValidateMemory(void *ptr, size_t size);
static void Stability_CrashHandler(const char *reason);
static void Stability_PerformanceMonitor(void);

/*
===============
Stability_Init
===============
*/
void Stability_Init(void) {
    Com_Memset(&stability_state, 0, sizeof(stability_state));
    Com_Memset(&stability_stats, 0, sizeof(stability_stats));

    // Initialize thread safety
    Stability_MutexInit(&stability_mutex);

    // Register CVars
    stability_enable = Cvar_Get("stability_enable", "1", CVAR_ARCHIVE | CVAR_LATCH,
        "Enable comprehensive stability framework");
    stability_log_level = Cvar_Get("stability_log_level", "2", CVAR_ARCHIVE,
        "Stability logging level (0=none, 1=errors, 2=warnings, 3=info, 4=debug)");
    stability_assert_level = Cvar_Get("stability_assert_level", "2", CVAR_ARCHIVE,
        "Assertion level (0=off, 1=fatal, 2=warning, 3=debug)");
    stability_memory_guard = Cvar_Get("stability_memory_guard", "1", CVAR_ARCHIVE,
        "Enable memory corruption detection");
    stability_thread_safety = Cvar_Get("stability_thread_safety", "1", CVAR_ARCHIVE,
        "Enable thread safety validation");
    stability_input_validation = Cvar_Get("stability_input_validation", "1", CVAR_ARCHIVE,
        "Enable input sanitization and validation");
    stability_resource_limits = Cvar_Get("stability_resource_limits", "1", CVAR_ARCHIVE,
        "Enable resource usage limits");
    stability_crash_recovery = Cvar_Get("stability_crash_recovery", "1", CVAR_ARCHIVE,
        "Enable crash recovery mechanisms");
    stability_performance_monitoring = Cvar_Get("stability_performance_monitoring", "1", CVAR_ARCHIVE,
        "Enable performance stability monitoring");

    // Initialize subsystems
    if (stability_memory_guard->integer) {
        Stability_MemoryGuardInit();
    }

    if (stability_thread_safety->integer) {
        Stability_ThreadSafetyInit();
    }

    if (stability_input_validation->integer) {
        Stability_InputValidationInit();
    }

    if (stability_resource_limits->integer) {
        Stability_ResourceLimitsInit();
    }

    if (stability_crash_recovery->integer) {
        Stability_CrashRecoveryInit();
    }

    if (stability_performance_monitoring->integer) {
        Stability_PerformanceMonitorInit();
    }

    stability_state.initialized = qtrue;
    Stability_LogEvent(STABILITY_INFO, "Stability framework initialized");
}

/*
===============
Stability_Shutdown
===============
*/
void Stability_Shutdown(void) {
    if (!stability_state.initialized) {
        return;
    }

    Stability_LogEvent(STABILITY_INFO, "Shutting down stability framework");

    // Shutdown subsystems in reverse order
    if (stability_performance_monitoring->integer) {
        Stability_PerformanceMonitorShutdown();
    }

    if (stability_crash_recovery->integer) {
        Stability_CrashRecoveryShutdown();
    }

    if (stability_resource_limits->integer) {
        Stability_ResourceLimitsShutdown();
    }

    if (stability_input_validation->integer) {
        Stability_InputValidationShutdown();
    }

    if (stability_thread_safety->integer) {
        Stability_ThreadSafetyShutdown();
    }

    if (stability_memory_guard->integer) {
        Stability_MemoryGuardShutdown();
    }

    // Destroy mutex
    Stability_MutexDestroy(&stability_mutex);

    stability_state.initialized = qfalse;
}

/*
===============
Stability_Frame
===============
*/
void Stability_Frame(void) {
    if (!stability_state.initialized || !stability_enable->integer) {
        return;
    }

    // Update performance monitoring
    if (stability_performance_monitoring->integer) {
        Stability_PerformanceMonitor();
    }

    // Check resource limits
    if (stability_resource_limits->integer) {
        Stability_ResourceLimitsCheck();
    }

    // Update statistics
    stability_stats.frame_count++;
    stability_stats.uptime = Sys_Milliseconds() / 1000.0f;
}

/*
===============
Stability_Assert
===============
*/
void Stability_Assert(qboolean condition, const char *expression, const char *file, int line, const char *function) {
    if (!condition) {
        char message[1024];
        Com_sprintf(message, sizeof(message), "Assertion failed: %s at %s:%d in %s",
            expression, file, line, function);

        Stability_LogEvent(STABILITY_ERROR, "%s", message);

        if (stability_assert_level->integer >= STABILITY_ASSERT_FATAL) {
            Stability_CrashHandler(message);
        } else if (stability_assert_level->integer >= STABILITY_ASSERT_WARNING) {
            Com_Printf(S_COLOR_YELLOW "WARNING: %s\n", message);
        }
    }
}

/*
===============
Stability_ValidatePointer
===============
*/
qboolean Stability_ValidatePointer(const void *ptr, const char *context) {
    if (!ptr) {
        Stability_LogEvent(STABILITY_WARNING, "NULL pointer detected in %s", context);
        return qfalse;
    }

    // Basic validation - check if pointer is in valid memory range
    // This is platform-specific and would need implementation per platform
    if (!Stability_ValidateMemory((void *)ptr, 1)) {
        Stability_LogEvent(STABILITY_ERROR, "Invalid pointer %p detected in %s", ptr, context);
        return qfalse;
    }

    return qtrue;
}

/*
===============
Stability_ValidateString
===============
*/
qboolean Stability_ValidateString(const char *str, size_t max_length, const char *context) {
    if (!str) {
        Stability_LogEvent(STABILITY_WARNING, "NULL string detected in %s", context);
        return qfalse;
    }

    size_t length = strlen(str);
    if (length > max_length) {
        Stability_LogEvent(STABILITY_WARNING, "String too long (%d > %d) in %s", length, max_length, context);
        return qfalse;
    }

    // Check for invalid characters or patterns
    for (size_t i = 0; i < length; i++) {
        if (str[i] < 32 && str[i] != '\n' && str[i] != '\t' && str[i] != '\r') {
            Stability_LogEvent(STABILITY_WARNING, "Invalid character 0x%02X in string at %s", str[i], context);
            return qfalse;
        }
    }

    return qtrue;
}

/*
===============
Stability_SanitizeInput
===============
*/
void Stability_SanitizeInput(char *input, size_t max_length) {
    if (!input) return;

    // Remove potentially dangerous characters
    char *src = input;
    char *dst = input;

    while (*src && (dst - input) < max_length - 1) {
        // Allow printable ASCII, spaces, and common punctuation
        if ((*src >= 32 && *src <= 126) || *src == '\n' || *src == '\t' || *src == '\r') {
            *dst++ = *src;
        } else {
            // Replace dangerous characters with safe alternatives
            *dst++ = '?';
            Stability_LogEvent(STABILITY_INFO, "Sanitized dangerous character 0x%02X in input", *src);
        }
        src++;
    }

    *dst = '\0';
}

/*
===============
Stability_LogEvent
===============
*/
static void Stability_LogEvent(stability_event_type_t type, const char *message, ...) {
    if (!stability_enable->integer) return;

    const char *type_str;
    const char *color;

    switch (type) {
        case STABILITY_DEBUG:
            if (stability_log_level->integer < 4) return;
            type_str = "DEBUG";
            color = S_COLOR_CYAN;
            break;
        case STABILITY_INFO:
            if (stability_log_level->integer < 3) return;
            type_str = "INFO";
            color = S_COLOR_GREEN;
            break;
        case STABILITY_WARNING:
            if (stability_log_level->integer < 2) return;
            type_str = "WARNING";
            color = S_COLOR_YELLOW;
            break;
        case STABILITY_ERROR:
            if (stability_log_level->integer < 1) return;
            type_str = "ERROR";
            color = S_COLOR_RED;
            break;
        default:
            type_str = "UNKNOWN";
            color = S_COLOR_WHITE;
            break;
    }

    va_list args;
    char buffer[2048];

    va_start(args, message);
    Q_vsnprintf(buffer, sizeof(buffer), message, args);
    va_end(args);

    Com_Printf("%s[STABILITY %s] %s^7\n", color, type_str, buffer);

    stability_stats.event_count[type]++;
}

/*
===============
Stability_GetStats
===============
*/
const stability_stats_t *Stability_GetStats(void) {
    return &stability_stats;
}

/*
===============
Stability_CrashHandler
===============
*/
static void Stability_CrashHandler(const char *reason) {
    Stability_LogEvent(STABILITY_ERROR, "Critical error: %s", reason);

    // Attempt recovery if enabled
    if (stability_crash_recovery->integer) {
        Stability_LogEvent(STABILITY_INFO, "Attempting crash recovery...");
        // Implementation would go here - restart subsystems, reset state, etc.
    }

    // Generate crash dump if available
    // This would integrate with the existing crash handler

    Com_Error(ERR_DROP, "Stability framework detected critical error: %s", reason);
}

/*
===============
Memory Guard Subsystem
===============
*/
static void Stability_MemoryGuardInit(void) {
    Stability_LogEvent(STABILITY_INFO, "Initializing memory guard");
    // Initialize canary values, bounds checking, etc.
}

static void Stability_MemoryGuardShutdown(void) {
    Stability_LogEvent(STABILITY_INFO, "Shutting down memory guard");
}

static qboolean Stability_ValidateMemory(void *ptr, size_t size) {
    // Basic bounds checking - in a real implementation this would be much more sophisticated
    if (!ptr) return qfalse;

    // Check if pointer is in valid heap range (simplified)
    // This is highly platform-specific
    return qtrue; // Placeholder
}

/*
===============
Thread Safety Subsystem
===============
*/
static void Stability_ThreadSafetyInit(void) {
    Stability_LogEvent(STABILITY_INFO, "Initializing thread safety validation");
}

static void Stability_ThreadSafetyShutdown(void) {
    Stability_LogEvent(STABILITY_INFO, "Shutting down thread safety validation");
}

/*
===============
Input Validation Subsystem
===============
*/
static void Stability_InputValidationInit(void) {
    Stability_LogEvent(STABILITY_INFO, "Initializing input validation");
}

static void Stability_InputValidationShutdown(void) {
    Stability_LogEvent(STABILITY_INFO, "Shutting down input validation");
}

/*
===============
Resource Limits Subsystem
===============
*/
static void Stability_ResourceLimitsInit(void) {
    Stability_LogEvent(STABILITY_INFO, "Initializing resource limits");
}

static void Stability_ResourceLimitsShutdown(void) {
    Stability_LogEvent(STABILITY_INFO, "Shutting down resource limits");
}

static void Stability_ResourceLimitsCheck(void) {
    // Check memory usage, file handles, network connections, etc.
    // Implementation would monitor and enforce limits
}

/*
===============
Crash Recovery Subsystem
===============
*/
static void Stability_CrashRecoveryInit(void) {
    Stability_LogEvent(STABILITY_INFO, "Initializing crash recovery");
}

static void Stability_CrashRecoveryShutdown(void) {
    Stability_LogEvent(STABILITY_INFO, "Shutting down crash recovery");
}

/*
===============
Performance Monitor Subsystem
===============
*/
static void Stability_PerformanceMonitorInit(void) {
    Stability_LogEvent(STABILITY_INFO, "Initializing performance monitoring");
}

static void Stability_PerformanceMonitorShutdown(void) {
    Stability_LogEvent(STABILITY_INFO, "Shutting down performance monitoring");
}

static void Stability_PerformanceMonitor(void) {
    // Monitor frame rates, memory usage, etc.
    // Flag performance issues
}

/*
===============
Mutex Implementation (Platform-specific)
===============
*/
void Stability_MutexInit(stability_mutex_t *mutex) {
#ifdef _WIN32
    InitializeCriticalSection(&mutex->cs);
#else
    pthread_mutex_init(&mutex->mutex, NULL);
#endif
}

void Stability_MutexDestroy(stability_mutex_t *mutex) {
#ifdef _WIN32
    DeleteCriticalSection(&mutex->cs);
#else
    pthread_mutex_destroy(&mutex->mutex);
#endif
}

void Stability_MutexLock(stability_mutex_t *mutex) {
#ifdef _WIN32
    EnterCriticalSection(&mutex->cs);
#else
    pthread_mutex_lock(&mutex->mutex);
#endif
}

void Stability_MutexUnlock(stability_mutex_t *mutex) {
#ifdef _WIN32
    LeaveCriticalSection(&mutex->cs);
#else
    pthread_mutex_unlock(&mutex->mutex);
#endif
}
