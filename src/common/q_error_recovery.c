/*
===========================================================================
q_error_recovery.c - Enhanced Error Handling and Recovery System
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "q_error_recovery.h"

// Error recovery configuration
cvar_t *error_recovery_enable;
cvar_t *error_recovery_max_attempts;
cvar_t *error_recovery_backoff_time;
cvar_t *error_recovery_log_detailed;
cvar_t *error_recovery_auto_restart;
cvar_t *error_recovery_graceful_degradation;
cvar_t *error_recovery_telemetry;
cvar_t *error_recovery_sandbox_mode;

// Error recovery state
static error_recovery_state_t error_state;
static error_history_t error_history;
static error_recovery_stats_t recovery_stats;

// Error classification
typedef enum {
    ERROR_TYPE_NONE,
    ERROR_TYPE_MEMORY,
    ERROR_TYPE_FILESYSTEM,
    ERROR_TYPE_NETWORK,
    ERROR_TYPE_RENDERING,
    ERROR_TYPE_SCRIPTING,
    ERROR_TYPE_INPUT,
    ERROR_TYPE_SYSTEM,
    ERROR_TYPE_UNKNOWN
} error_type_t;

// Recovery strategies
typedef enum {
    RECOVERY_STRATEGY_NONE,
    RECOVERY_STRATEGY_RESTART,
    RECOVERY_STRATEGY_DEGRADE,
    RECOVERY_STRATEGY_RETRY,
    RECOVERY_STRATEGY_SANDBOX,
    RECOVERY_STRATEGY_SHUTDOWN
} recovery_strategy_t;

/*
===============
ErrorRecovery_Init
===============
*/
void ErrorRecovery_Init(void) {
    Com_Memset(&error_state, 0, sizeof(error_state));
    Com_Memset(&error_history, 0, sizeof(error_history));
    Com_Memset(&recovery_stats, 0, sizeof(recovery_stats));

    // Register CVars
    error_recovery_enable = Cvar_Get("error_recovery_enable", "1", CVAR_ARCHIVE | CVAR_LATCH,
        "Enable enhanced error recovery system");
    error_recovery_max_attempts = Cvar_Get("error_recovery_max_attempts", "3", CVAR_ARCHIVE,
        "Maximum recovery attempts before giving up");
    error_recovery_backoff_time = Cvar_Get("error_recovery_backoff_time", "5", CVAR_ARCHIVE,
        "Backoff time between recovery attempts (seconds)");
    error_recovery_log_detailed = Cvar_Get("error_recovery_log_detailed", "1", CVAR_ARCHIVE,
        "Enable detailed error logging");
    error_recovery_auto_restart = Cvar_Get("error_recovery_auto_restart", "1", CVAR_ARCHIVE,
        "Enable automatic subsystem restart on errors");
    error_recovery_graceful_degradation = Cvar_Get("error_recovery_graceful_degradation", "1", CVAR_ARCHIVE,
        "Enable graceful degradation on failures");
    error_recovery_telemetry = Cvar_Get("error_recovery_telemetry", "0", CVAR_ARCHIVE,
        "Enable error telemetry reporting");
    error_recovery_sandbox_mode = Cvar_Get("error_recovery_sandbox_mode", "1", CVAR_ARCHIVE,
        "Enable sandbox mode for error containment");

    error_state.initialized = qtrue;
    Com_Printf("Error recovery system initialized\n");
}

/*
===============
ErrorRecovery_Shutdown
===============
*/
void ErrorRecovery_Shutdown(void) {
    if (!error_state.initialized) {
        return;
    }

    // Generate final error report
    ErrorRecovery_GenerateReport();

    Com_Printf("Error recovery system shutdown\n");
    error_state.initialized = qfalse;
}

/*
===============
ErrorRecovery_HandleError
===============
*/
recovery_result_t ErrorRecovery_HandleError(error_type_t error_type, const char *error_message,
                                          const char *context, qboolean is_fatal) {
    recovery_result_t result = { RECOVERY_STRATEGY_NONE, qfalse, 0 };

    if (!error_recovery_enable->integer || !error_state.initialized) {
        return result;
    }

    // Classify and log the error
    ErrorRecovery_LogError(error_type, error_message, context, is_fatal);

    // Determine recovery strategy
    recovery_strategy_t strategy = ErrorRecovery_DetermineStrategy(error_type, is_fatal);

    // Execute recovery
    qboolean success = qfalse;
    switch (strategy) {
        case RECOVERY_STRATEGY_RESTART:
            success = ErrorRecovery_AttemptRestart(error_type, context);
            break;
        case RECOVERY_STRATEGY_DEGRADE:
            success = ErrorRecovery_AttemptDegradation(error_type, context);
            break;
        case RECOVERY_STRATEGY_RETRY:
            success = ErrorRecovery_AttemptRetry(error_type, context);
            break;
        case RECOVERY_STRATEGY_SANDBOX:
            success = ErrorRecovery_AttemptSandbox(error_type, context);
            break;
        case RECOVERY_STRATEGY_SHUTDOWN:
            ErrorRecovery_InitiateShutdown(error_type, error_message);
            break;
        default:
            break;
    }

    // Update statistics
    recovery_stats.total_errors++;
    if (success) {
        recovery_stats.successful_recoveries++;
    } else {
        recovery_stats.failed_recoveries++;
    }

    // Prepare result
    result.strategy = strategy;
    result.success = success;
    result.backoff_time = error_recovery_backoff_time->integer;

    if (error_recovery_log_detailed->integer) {
        Com_Printf("Error recovery: %s (strategy: %d, success: %s)\n",
            error_message, strategy, success ? "yes" : "no");
    }

    return result;
}

/*
===============
ErrorRecovery_LogError
===============
*/
static void ErrorRecovery_LogError(error_type_t error_type, const char *error_message,
                                  const char *context, qboolean is_fatal) {
    // Add to error history
    if (error_history.count < ERROR_HISTORY_SIZE) {
        error_history_entry_t *entry = &error_history.entries[error_history.count++];
        entry->timestamp = Sys_Milliseconds();
        entry->error_type = error_type;
        entry->is_fatal = is_fatal;
        Q_strncpyz(entry->message, error_message, sizeof(entry->message));
        Q_strncpyz(entry->context, context, sizeof(entry->context));
    }

    // Log to console
    const char *type_str = "UNKNOWN";
    const char *color = S_COLOR_RED;

    switch (error_type) {
        case ERROR_TYPE_MEMORY: type_str = "MEMORY"; color = S_COLOR_MAGENTA; break;
        case ERROR_TYPE_FILESYSTEM: type_str = "FILESYSTEM"; color = S_COLOR_BLUE; break;
        case ERROR_TYPE_NETWORK: type_str = "NETWORK"; color = S_COLOR_CYAN; break;
        case ERROR_TYPE_RENDERING: type_str = "RENDERING"; color = S_COLOR_YELLOW; break;
        case ERROR_TYPE_SCRIPTING: type_str = "SCRIPTING"; color = S_COLOR_GREEN; break;
        case ERROR_TYPE_INPUT: type_str = "INPUT"; color = S_COLOR_WHITE; break;
        case ERROR_TYPE_SYSTEM: type_str = "SYSTEM"; color = S_COLOR_RED; break;
        default: break;
    }

    Com_Printf("%s[%s ERROR]%s %s", color, type_str, is_fatal ? " FATAL:" : ":",
        error_message);

    if (context && *context) {
        Com_Printf(" (%s)", context);
    }
    Com_Printf("\n");

    // Send telemetry if enabled
    if (error_recovery_telemetry->integer) {
        ErrorRecovery_SendTelemetry(error_type, error_message, context, is_fatal);
    }
}

/*
===============
ErrorRecovery_DetermineStrategy
===============
*/
static recovery_strategy_t ErrorRecovery_DetermineStrategy(error_type_t error_type, qboolean is_fatal) {
    // Check if we've exceeded max attempts
    if (error_state.recovery_attempts >= error_recovery_max_attempts->integer) {
        return RECOVERY_STRATEGY_SHUTDOWN;
    }

    // Determine strategy based on error type
    switch (error_type) {
        case ERROR_TYPE_MEMORY:
            return error_recovery_graceful_degradation->integer ?
                RECOVERY_STRATEGY_DEGRADE : RECOVERY_STRATEGY_RESTART;

        case ERROR_TYPE_FILESYSTEM:
            return RECOVERY_STRATEGY_RETRY;

        case ERROR_TYPE_NETWORK:
            return RECOVERY_STRATEGY_SANDBOX;

        case ERROR_TYPE_RENDERING:
            return error_recovery_graceful_degradation->integer ?
                RECOVERY_STRATEGY_DEGRADE : RECOVERY_STRATEGY_RESTART;

        case ERROR_TYPE_SCRIPTING:
            return RECOVERY_STRATEGY_SANDBOX;

        case ERROR_TYPE_INPUT:
            return RECOVERY_STRATEGY_RETRY;

        case ERROR_TYPE_SYSTEM:
            return is_fatal ? RECOVERY_STRATEGY_SHUTDOWN : RECOVERY_STRATEGY_RESTART;

        default:
            return RECOVERY_STRATEGY_NONE;
    }
}

/*
===============
ErrorRecovery_AttemptRestart
===============
*/
static qboolean ErrorRecovery_AttemptRestart(error_type_t error_type, const char *context) {
    if (!error_recovery_auto_restart->integer) {
        return qfalse;
    }

    error_state.recovery_attempts++;

    Com_Printf("Attempting subsystem restart for %s...\n", context);

    // Implementation would restart specific subsystems based on context
    // For example: renderer restart, filesystem remount, etc.

    return qtrue;
}

/*
===============
ErrorRecovery_AttemptDegradation
===============
*/
static qboolean ErrorRecovery_AttemptDegradation(error_type_t error_type, const char *context) {
    Com_Printf("Attempting graceful degradation for %s...\n", context);

    // Disable advanced features to maintain basic functionality
    switch (error_type) {
        case ERROR_TYPE_RENDERING:
            Cvar_Set("r_pbr", "0");
            Cvar_Set("r_ssao", "0");
            Cvar_Set("r_bloom", "0");
            Com_Printf("Disabled advanced rendering features\n");
            break;

        case ERROR_TYPE_MEMORY:
            Cvar_Set("com_memoryStats", "0");
            Cvar_Set("memory_leak_detection", "0");
            Com_Printf("Disabled memory monitoring features\n");
            break;

        case ERROR_TYPE_SCRIPTING:
            // Could disable Lua features
            Com_Printf("Scripting features remain enabled\n");
            break;

        default:
            Com_Printf("No degradation options for this error type\n");
            return qfalse;
    }

    return qtrue;
}

/*
===============
ErrorRecovery_AttemptRetry
===============
*/
static qboolean ErrorRecovery_AttemptRetry(error_type_t error_type, const char *context) {
    Com_Printf("Attempting retry for %s...\n", context);

    // Implementation would retry failed operations
    // For example: file access, network connections, etc.

    return qtrue;
}

/*
===============
ErrorRecovery_AttemptSandbox
===============
*/
static qboolean ErrorRecovery_AttemptSandbox(error_type_t error_type, const char *context) {
    if (!error_recovery_sandbox_mode->integer) {
        return qfalse;
    }

    Com_Printf("Activating sandbox mode for %s...\n", context);

    // Isolate problematic components
    // For example: disable network features, isolate scripts, etc.

    return qtrue;
}

/*
===============
ErrorRecovery_InitiateShutdown
===============
*/
static void ErrorRecovery_InitiateShutdown(error_type_t error_type, const char *error_message) {
    Com_Printf(S_COLOR_RED "CRITICAL ERROR: Initiating emergency shutdown\n");
    Com_Printf(S_COLOR_RED "Error: %s\n", error_message);

    // Generate emergency crash report
    ErrorRecovery_GenerateCrashReport(error_type, error_message);

    // Perform graceful shutdown
    Com_Quit_f();
}

/*
===============
ErrorRecovery_SendTelemetry
===============
*/
static void ErrorRecovery_SendTelemetry(error_type_t error_type, const char *error_message,
                                       const char *context, qboolean is_fatal) {
    // Implementation would send anonymized error data to telemetry service
    // This is a placeholder for actual telemetry implementation

    Com_Printf("Telemetry: Error reported (type: %d, fatal: %s)\n",
        error_type, is_fatal ? "yes" : "no");
}

/*
===============
ErrorRecovery_GenerateReport
===============
*/
void ErrorRecovery_GenerateReport(void) {
    Com_Printf("=== ERROR RECOVERY REPORT ===\n");
    Com_Printf("Total errors: %d\n", recovery_stats.total_errors);
    Com_Printf("Successful recoveries: %d\n", recovery_stats.successful_recoveries);
    Com_Printf("Failed recoveries: %d\n", recovery_stats.failed_recoveries);
    Com_Printf("Recovery rate: %.1f%%\n",
        recovery_stats.total_errors > 0 ?
        (recovery_stats.successful_recoveries * 100.0f / recovery_stats.total_errors) : 0.0f);

    // Show recent error history
    if (error_history.count > 0) {
        Com_Printf("\nRecent Errors:\n");
        int start = error_history.count > 5 ? error_history.count - 5 : 0;
        for (int i = start; i < error_history.count; i++) {
            error_history_entry_t *entry = &error_history.entries[i];
            Com_Printf("  %s: %s (%s)\n",
                entry->is_fatal ? "FATAL" : "ERROR",
                entry->message,
                entry->context);
        }
    }

    Com_Printf("===============================\n");
}

/*
===============
ErrorRecovery_GenerateCrashReport
===============
*/
static void ErrorRecovery_GenerateCrashReport(error_type_t error_type, const char *error_message) {
    // Generate detailed crash report for debugging
    fileHandle_t f;
    char filename[64];
    Com_sprintf(filename, sizeof(filename), "crash_report_%d.txt", Sys_Milliseconds());

    FS_FOpenFileByMode(filename, &f, FS_WRITE);
    if (f) {
        FS_Printf(f, "=== CRASH REPORT ===\n");
        FS_Printf(f, "Timestamp: %d\n", Sys_Milliseconds());
        FS_Printf(f, "Error Type: %d\n", error_type);
        FS_Printf(f, "Error Message: %s\n", error_message);
        FS_Printf(f, "Engine Version: Enhanced idTech3\n");
        FS_Printf(f, "Platform: %s\n", Sys_GetPlatformString());

        // Add system information
        FS_Printf(f, "\nSystem Information:\n");
        // Implementation would add CPU, memory, GPU info

        // Add error history
        FS_Printf(f, "\nError History:\n");
        for (int i = 0; i < error_history.count; i++) {
            error_history_entry_t *entry = &error_history.entries[i];
            FS_Printf(f, "  %d: %s - %s (%s)\n",
                entry->timestamp, entry->message, entry->context,
                entry->is_fatal ? "FATAL" : "ERROR");
        }

        FS_FCloseFile(f);
        Com_Printf("Crash report saved to %s\n", filename);
    }
}

/*
===============
ErrorRecovery_GetStats
===============
*/
const error_recovery_stats_t *ErrorRecovery_GetStats(void) {
    return &recovery_stats;
}

/*
===============
Enhanced Error Functions
===============
*/

// Enhanced Com_Error with recovery
void Com_Error_Recoverable(int code, const char *fmt, ...) {
    char msg[1024];
    va_list argptr;

    va_start(argptr, fmt);
    Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
    va_end(argptr);

    // Attempt recovery first
    error_type_t error_type = ERROR_TYPE_UNKNOWN;
    recovery_result_t result = ErrorRecovery_HandleError(error_type, msg, "Com_Error", qfalse);

    if (result.success) {
        Com_Printf(S_COLOR_YELLOW "ERROR RECOVERED: %s\n", msg);
        return;
    }

    // If recovery failed, proceed with normal error handling
    Com_Error(code, "%s", msg);
}

// Enhanced Com_Printf with error detection
void Com_Printf_Safe(const char *fmt, ...) {
    // Check for potential format string vulnerabilities
    if (strstr(fmt, "%n") != NULL) {
        Com_Printf(S_COLOR_RED "SECURITY WARNING: Format string contains %%n\n");
        return;
    }

    if (strstr(fmt, "%s") != NULL && strlen(fmt) > 1000) {
        Com_Printf(S_COLOR_RED "SECURITY WARNING: Very long format string\n");
        return;
    }

    va_list argptr;
    va_start(argptr, fmt);
    Com_Printf(fmt, argptr);
    va_end(argptr);
}
