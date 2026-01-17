/*
===============================================================================
Comprehensive Error Handling System - AAA Quality

Professional-grade error handling with:
- Standardized error codes and categories
- Error context and stack tracing
- Recovery mechanisms and fallback strategies
- Comprehensive logging and reporting
- Exception safety patterns (C equivalent)
===============================================================================
*/

#ifndef __Q_ERROR_SYSTEM_H__
#define __Q_ERROR_SYSTEM_H__

#include "q_shared.h"
#include <setjmp.h>

//============================================================================
// Error Categories - Hierarchical Classification
//============================================================================

typedef enum {
    // Core System Errors (0x0000-0x0FFF)
    ERR_CATEGORY_SYSTEM         = 0x0000,
    ERR_CATEGORY_MEMORY         = 0x0100,
    ERR_CATEGORY_FILESYSTEM     = 0x0200,
    ERR_CATEGORY_NETWORK        = 0x0300,
    ERR_CATEGORY_RENDERER       = 0x0400,
    ERR_CATEGORY_AUDIO          = 0x0500,
    ERR_CATEGORY_INPUT          = 0x0600,

    // Game Logic Errors (0x1000-0x1FFF)
    ERR_CATEGORY_GAME           = 0x1000,
    ERR_CATEGORY_SCRIPT         = 0x1100,
    ERR_CATEGORY_PHYSICS        = 0x1200,
    ERR_CATEGORY_AI             = 0x1300,
    ERR_CATEGORY_UI             = 0x1400,

    // External/Integration Errors (0x2000-0x2FFF)
    ERR_CATEGORY_EXTERNAL       = 0x2000,
    ERR_CATEGORY_PLUGIN         = 0x2100,
    ERR_CATEGORY_MOD            = 0x2200,
    ERR_CATEGORY_ASSET          = 0x2300,

    // User/Input Errors (0x3000-0x3FFF)
    ERR_CATEGORY_USER           = 0x3000,
    ERR_CATEGORY_CONFIG         = 0x3100,
    ERR_CATEGORY_VALIDATION     = 0x3200,

    // Recovery Categories (0xF000-0xFFFF)
    ERR_CATEGORY_RECOVERY       = 0xF000,
    ERR_CATEGORY_FALLBACK       = 0xF100,
} error_category_t;

//============================================================================
// Error Codes - Specific Error Conditions
//============================================================================

typedef enum {
    // System Errors (ERR_CATEGORY_SYSTEM)
    ERR_SUCCESS                 = ERR_CATEGORY_SYSTEM + 0,
    ERR_UNKNOWN                 = ERR_CATEGORY_SYSTEM + 1,
    ERR_NOT_IMPLEMENTED         = ERR_CATEGORY_SYSTEM + 2,
    ERR_INVALID_PARAMETER       = ERR_CATEGORY_SYSTEM + 3,
    ERR_INVALID_STATE           = ERR_CATEGORY_SYSTEM + 4,
    ERR_TIMEOUT                 = ERR_CATEGORY_SYSTEM + 5,
    ERR_RESOURCE_EXHAUSTED      = ERR_CATEGORY_SYSTEM + 6,

    // Memory Errors (ERR_CATEGORY_MEMORY)
    ERR_OUT_OF_MEMORY           = ERR_CATEGORY_MEMORY + 1,
    ERR_MEMORY_CORRUPTION       = ERR_CATEGORY_MEMORY + 2,
    ERR_DOUBLE_FREE             = ERR_CATEGORY_MEMORY + 3,
    ERR_INVALID_FREE            = ERR_CATEGORY_MEMORY + 4,
    ERR_MEMORY_LEAK             = ERR_CATEGORY_MEMORY + 5,

    // Filesystem Errors (ERR_CATEGORY_FILESYSTEM)
    ERR_FILE_NOT_FOUND          = ERR_CATEGORY_FILESYSTEM + 1,
    ERR_FILE_ACCESS_DENIED      = ERR_CATEGORY_FILESYSTEM + 2,
    ERR_FILE_CORRUPTED          = ERR_CATEGORY_FILESYSTEM + 3,
    ERR_DISK_FULL               = ERR_CATEGORY_FILESYSTEM + 4,
    ERR_PATH_TOO_LONG           = ERR_CATEGORY_FILESYSTEM + 5,

    // Network Errors (ERR_CATEGORY_NETWORK)
    ERR_CONNECTION_FAILED       = ERR_CATEGORY_NETWORK + 1,
    ERR_CONNECTION_LOST         = ERR_CATEGORY_NETWORK + 2,
    ERR_PROTOCOL_ERROR          = ERR_CATEGORY_NETWORK + 3,
    ERR_HOST_UNREACHABLE        = ERR_CATEGORY_NETWORK + 4,

    // Renderer Errors (ERR_CATEGORY_RENDERER)
    ERR_GPU_NOT_SUPPORTED       = ERR_CATEGORY_RENDERER + 1,
    ERR_SHADER_COMPILE_FAILED   = ERR_CATEGORY_RENDERER + 2,
    ERR_TEXTURE_LOAD_FAILED     = ERR_CATEGORY_RENDERER + 3,
    ERR_RENDERER_INIT_FAILED    = ERR_CATEGORY_RENDERER + 4,

    // Audio Errors (ERR_CATEGORY_AUDIO)
    ERR_AUDIO_DEVICE_INIT_FAILED = ERR_CATEGORY_AUDIO + 1,
    ERR_AUDIO_FILE_CORRUPTED    = ERR_CATEGORY_AUDIO + 2,
    ERR_AUDIO_BUFFER_UNDERFLOW  = ERR_CATEGORY_AUDIO + 3,

    // Input Errors (ERR_CATEGORY_INPUT)
    ERR_INPUT_DEVICE_NOT_FOUND  = ERR_CATEGORY_INPUT + 1,
    ERR_INPUT_DEVICE_BUSY       = ERR_CATEGORY_INPUT + 2,

    // Game Errors (ERR_CATEGORY_GAME)
    ERR_GAME_STATE_CORRUPTED    = ERR_CATEGORY_GAME + 1,
    ERR_LEVEL_LOAD_FAILED       = ERR_CATEGORY_GAME + 2,
    ERR_ENTITY_SPAWN_FAILED     = ERR_CATEGORY_GAME + 3,

    // Script Errors (ERR_CATEGORY_SCRIPT)
    ERR_SCRIPT_COMPILE_FAILED   = ERR_CATEGORY_SCRIPT + 1,
    ERR_SCRIPT_RUNTIME_ERROR    = ERR_CATEGORY_SCRIPT + 2,

    // Physics Errors (ERR_CATEGORY_PHYSICS)
    ERR_PHYSICS_WORLD_INVALID   = ERR_CATEGORY_PHYSICS + 1,
    ERR_COLLISION_DETECTION_FAILED = ERR_CATEGORY_PHYSICS + 2,

    // AI Errors (ERR_CATEGORY_AI)
    ERR_AI_PATHFINDING_FAILED   = ERR_CATEGORY_AI + 1,
    ERR_AI_STATE_MACHINE_CORRUPTED = ERR_CATEGORY_AI + 2,

    // UI Errors (ERR_CATEGORY_UI)
    ERR_UI_LAYOUT_INVALID       = ERR_CATEGORY_UI + 1,
    ERR_UI_RESOURCE_MISSING     = ERR_CATEGORY_UI + 2,

    // External Errors (ERR_CATEGORY_EXTERNAL)
    ERR_LIBRARY_LOAD_FAILED     = ERR_CATEGORY_EXTERNAL + 1,
    ERR_API_CALL_FAILED         = ERR_CATEGORY_EXTERNAL + 2,

    // User Errors (ERR_CATEGORY_USER)
    ERR_INVALID_COMMAND         = ERR_CATEGORY_USER + 1,
    ERR_PERMISSION_DENIED       = ERR_CATEGORY_USER + 2,

    // Recovery Errors (ERR_CATEGORY_RECOVERY)
    ERR_RECOVERY_FAILED         = ERR_CATEGORY_RECOVERY + 1,
    ERR_FALLBACK_FAILED         = ERR_CATEGORY_RECOVERY + 2,
} error_code_t;

//============================================================================
// Error Severity Levels
//============================================================================

typedef enum {
    ERR_SEVERITY_INFO,          // Informational, no action required
    ERR_SEVERITY_WARNING,       // Warning, may require attention
    ERR_SEVERITY_ERROR,         // Error, operation failed but system can continue
    ERR_SEVERITY_CRITICAL,      // Critical error, system may be unstable
    ERR_SEVERITY_FATAL          // Fatal error, system must terminate
} error_severity_t;

//============================================================================
// Error Context and Tracing
//============================================================================

#define ERROR_STACK_DEPTH 32
#define ERROR_MESSAGE_MAX 1024
#define ERROR_CONTEXT_MAX 256

typedef struct {
    const char *file;
    int line;
    const char *function;
    const char *message;
} error_frame_t;

typedef struct {
    error_code_t code;
    error_category_t category;
    error_severity_t severity;
    char message[ERROR_MESSAGE_MAX];
    char context[ERROR_CONTEXT_MAX];

    // Stack trace
    error_frame_t stack[ERROR_STACK_DEPTH];
    int stack_depth;

    // Additional metadata
    int thread_id;
    int timestamp;
    void *user_data;

    // Recovery information
    qboolean recoverable;
    const char *recovery_hint;
} error_info_t;

//============================================================================
// Error Recovery Mechanisms
//============================================================================

typedef enum {
    RECOVERY_NONE,              // No recovery possible
    RECOVERY_RETRY,             // Retry the operation
    RECOVERY_FALLBACK,          // Use fallback implementation
    RECOVERY_DEGRADE,           // Degrade functionality
    RECOVERY_RESTART,           // Restart subsystem
    RECOVERY_TERMINATE          // Terminate gracefully
} recovery_strategy_t;

typedef struct {
    recovery_strategy_t strategy;
    int max_retries;
    int retry_delay_ms;
    void (*fallback_func)(void);
    void (*cleanup_func)(void);
} recovery_info_t;

//============================================================================
// Exception Safety Patterns (C Equivalent)
//============================================================================

typedef struct {
    void *resource;
    void (*cleanup)(void *resource);
    qboolean armed;
} scoped_resource_t;

// RAII-like resource management
#define SCOPED_RESOURCE(name, resource, cleanup_func) \
    scoped_resource_t name = {resource, cleanup_func, qtrue}

#define SCOPED_RESOURCE_DISARM(name) \
    (name).armed = qfalse

#define SCOPED_RESOURCE_CLEANUP(name) \
    if ((name).armed && (name).cleanup) { \
        (name).cleanup((name).resource); \
        (name).armed = qfalse; \
    }

//============================================================================
// Error Handling Macros
//============================================================================

// Basic error reporting
#define ERR_REPORT(code, msg) \
    Error_Report(code, msg, __FILE__, __LINE__, __func__)

#define ERR_REPORT_CTX(code, msg, ctx) \
    Error_ReportWithContext(code, msg, ctx, __FILE__, __LINE__, __func__)

// Error with recovery
#define ERR_RECOVERABLE(code, msg, recovery) \
    Error_ReportRecoverable(code, msg, recovery, __FILE__, __LINE__, __func__)

// Validation errors
#define ERR_VALIDATE(condition, code, msg) \
    do { \
        if (!(condition)) { \
            ERR_REPORT(code, msg); \
        } \
    } while(0)

// Resource error handling
#define ERR_CHECK_ALLOC(ptr, msg) \
    ERR_VALIDATE(ptr != NULL, ERR_OUT_OF_MEMORY, msg)

#define ERR_CHECK_FILE(fp, filename) \
    ERR_VALIDATE(fp != NULL, ERR_FILE_ACCESS_DENIED, va("Failed to open file: %s", filename))

//============================================================================
// Core Error Handling API
//============================================================================

// Error reporting functions
void Error_Init(void);
void Error_Shutdown(void);

void Error_Report(error_code_t code, const char *message,
                  const char *file, int line, const char *function);
void Error_ReportWithContext(error_code_t code, const char *message, const char *context,
                            const char *file, int line, const char *function);
void Error_ReportRecoverable(error_code_t code, const char *message,
                           const recovery_info_t *recovery,
                           const char *file, int line, const char *function);

// Error querying and handling
const error_info_t *Error_GetLast(void);
qboolean Error_HasPending(void);
void Error_Clear(void);

// Error recovery
qboolean Error_AttemptRecovery(const error_info_t *error);
recovery_strategy_t Error_SuggestRecovery(error_code_t code);

// Error formatting
const char *Error_CodeToString(error_code_t code);
const char *Error_CategoryToString(error_category_t category);
const char *Error_SeverityToString(error_severity_t severity);

// Error logging integration
void Error_LogToFile(const char *filename);
void Error_EnableConsoleOutput(qboolean enable);
void Error_SetLogLevel(error_severity_t min_level);

//============================================================================
// Advanced Error Handling Features
//============================================================================

// Error context management
void Error_PushContext(const char *context);
void Error_PopContext(void);
const char *Error_GetContextStack(void);

// Error statistics and monitoring
typedef struct {
    int total_errors;
    int errors_by_category[16];
    int errors_by_severity[5];
    int recovery_attempts;
    int recovery_successes;
} error_stats_t;

void Error_GetStats(error_stats_t *stats);
void Error_ResetStats(void);

// Error filtering and suppression
typedef qboolean (*error_filter_func_t)(const error_info_t *error);

void Error_SetFilter(error_filter_func_t filter);
void Error_SuppressCategory(error_category_t category, qboolean suppress);
void Error_SuppressCode(error_code_t code, qboolean suppress);

//============================================================================
// Exception Safety Utilities
//============================================================================

// Resource management helpers
void *Resource_Allocate(size_t size, const char *context);
void Resource_Free(void *ptr, const char *context);
FILE *File_OpenSafe(const char *filename, const char *mode, const char *context);
void File_CloseSafe(FILE *fp, const char *context);

// Transaction-like operations
typedef struct {
    void (*commit)(void *data);
    void (*rollback)(void *data);
    void *data;
} transaction_t;

void Transaction_Begin(transaction_t *tx);
qboolean Transaction_End(transaction_t *tx, qboolean success);

//============================================================================
// Validation Framework
//============================================================================

typedef qboolean (*validator_func_t)(const void *data, error_info_t *error);

void Validation_AddRule(const char *name, validator_func_t validator);
qboolean Validation_RunAll(const void *data, error_info_t *error);
qboolean Validation_RunRule(const char *name, const void *data, error_info_t *error);

// Common validators
qboolean Validate_NotNull(const void *ptr, error_info_t *error);
qboolean Validate_StringLength(const char *str, size_t min_len, size_t max_len, error_info_t *error);
qboolean Validate_Range(int value, int min_val, int max_val, error_info_t *error);
qboolean Validate_Path(const char *path, error_info_t *error);

#endif // __Q_ERROR_SYSTEM_H__