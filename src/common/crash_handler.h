/*
===========================================================================
Crash Handler - Comprehensive crash diagnostics and recovery

Features:
- Signal/exception handling with stack traces
- Log ring buffer capture (last N KB of logs)
- Build ID for crash correlation
- Minidump generation (platform-specific)
- Safe mode flag for recovery boot
===========================================================================
*/

#ifndef __CRASH_HANDLER_H__
#define __CRASH_HANDLER_H__

#include "q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

// Ring buffer size for crash log capture (4KB default)
#define CRASH_LOG_RING_SIZE     4096

// Maximum stack frames to capture
#define CRASH_MAX_STACK_FRAMES  32

// Crash report file (base name, will be timestamped)
#define CRASH_REPORT_BASENAME   "logs/crash_report_"

// Ensure logs directory exists
#define CRASH_LOG_DIR           "logs"

// Safe mode flag file
#define SAFE_MODE_FLAG_FILE     "safe_mode.flag"

// Build identification (set at compile time)
#ifndef BUILD_ID
#define BUILD_ID "dev-unknown"
#endif

#ifndef BUILD_DATE
#define BUILD_DATE __DATE__ " " __TIME__
#endif

// Crash info structure
typedef struct {
    int             signal_num;         // Signal number (Unix) or exception code (Windows)
    const char      *signal_name;       // Human-readable signal name
    void            *fault_address;     // Address that caused fault (if available)
    void            *stack_frames[CRASH_MAX_STACK_FRAMES];
    int             stack_frame_count;
    char            log_ring_buffer[CRASH_LOG_RING_SIZE];
    int             log_ring_pos;       // Current write position in ring buffer
    const char      *build_id;
    const char      *build_date;
    int             uptime_seconds;     // Time since engine start
} crash_info_t;

// Initialize crash handler (call early in main)
void Crash_Init(void);

// Shutdown crash handler
void Crash_Shutdown(void);

// Add message to the log ring buffer (called from Com_Printf)
void Crash_LogMessage(const char *msg);

// Check if we should boot in safe mode (previous crash detected)
qboolean Crash_ShouldBootSafeMode(void);

// Clear the safe mode flag (call after successful boot)
void Crash_ClearSafeModeFlag(void);

// Manually trigger crash report (for testing or soft errors)
void Crash_GenerateReport(const char *reason);

// Get current crash info (for debugging)
const crash_info_t *Crash_GetInfo(void);

// Register additional crash callback
typedef void (*crash_callback_t)(const crash_info_t *info);
void Crash_RegisterCallback(crash_callback_t callback);

// Mod loading crash debugging
void Crash_SetModLoadingContext(const char *modName, const char *operation);
void Crash_ClearModLoadingContext(void);
void Crash_ReportModLoad(const char *modName, const char *error);

// Enhanced reliability structures
typedef enum {
    RECOVERY_NONE,        // No recovery attempt
    RECOVERY_RESTART,     // Restart the engine
    RECOVERY_ROLLBACK,    // Rollback to saved state
    RECOVERY_DEGRADED     // Enter degraded mode
} recovery_strategy_t;

typedef enum {
    RECOVERY_FAILED,
    RECOVERY_SUCCESSFUL,
    RECOVERY_PARTIAL
} recovery_result_t;

typedef struct {
    qboolean auto_save_enabled;
    recovery_strategy_t recovery_strategy;
    int last_save_time;
    int uptime_seconds;
    int reliability_score;  // 0-100
} crash_reliability_metrics_t;

// Enhanced reliability functions
void Crash_EnableAutoSave(qboolean enable, const char *save_path);
void Crash_SetSaveInterval(int interval_ms);
void Crash_SetRecoveryStrategy(recovery_strategy_t strategy);
recovery_result_t Crash_AttemptRecovery(void);
void Crash_Update(void);
void Crash_GetReliabilityMetrics(crash_reliability_metrics_t *metrics);
qboolean Crash_ValidateSystemHealth(void);

#ifdef __cplusplus
}
#endif

#endif // __CRASH_HANDLER_H__
