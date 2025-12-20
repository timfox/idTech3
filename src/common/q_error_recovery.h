/*
===========================================================================
q_error_recovery.h - Enhanced Error Handling and Recovery System
===========================================================================
*/

#ifndef __Q_ERROR_RECOVERY_H__
#define __Q_ERROR_RECOVERY_H__

#include "q_shared.h"

// Error history
#define ERROR_HISTORY_SIZE 32

typedef struct {
    int timestamp;
    int error_type;
    qboolean is_fatal;
    char message[256];
    char context[128];
} error_history_entry_t;

typedef struct {
    error_history_entry_t entries[ERROR_HISTORY_SIZE];
    int count;
} error_history_t;

// Recovery result
typedef struct {
    int strategy;
    qboolean success;
    int backoff_time;
} recovery_result_t;

// Error recovery statistics
typedef struct {
    int total_errors;
    int successful_recoveries;
    int failed_recoveries;
    int restart_attempts;
    int degradation_events;
    int sandbox_activations;
    int shutdown_events;
} error_recovery_stats_t;

// Error recovery state
typedef struct {
    qboolean initialized;
    int recovery_attempts;
    int last_recovery_time;
    qboolean sandbox_active;
    qboolean degraded_mode;
} error_recovery_state_t;

// Function declarations
void ErrorRecovery_Init(void);
void ErrorRecovery_Shutdown(void);

// Error handling
recovery_result_t ErrorRecovery_HandleError(int error_type, const char *error_message,
                                          const char *context, qboolean is_fatal);

// Reporting and statistics
void ErrorRecovery_GenerateReport(void);
const error_recovery_stats_t *ErrorRecovery_GetStats(void);

// Enhanced error functions
void Com_Error_Recoverable(int code, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void Com_Printf_Safe(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif // __Q_ERROR_RECOVERY_H__
