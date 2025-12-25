/*
=============================================================================
Error Handling Framework

Structured error handling with stack traces and comprehensive error management.
=============================================================================
*/

#ifndef __ERROR_HANDLING_H__
#define __ERROR_HANDLING_H__

#include "q_shared.h"
#include <setjmp.h>

// Error severity levels
typedef enum {
    ERROR_SEVERITY_INFO,           // Informational message
    ERROR_SEVERITY_WARNING,        // Warning that doesn't prevent operation
    ERROR_SEVERITY_ERROR,          // Error that affects operation but is recoverable
    ERROR_SEVERITY_CRITICAL,       // Critical error that requires attention
    ERROR_SEVERITY_FATAL,          // Fatal error that terminates the program
    ERROR_SEVERITY_COUNT
} error_severity_t;

// Error categories
typedef enum {
    ERROR_CATEGORY_GENERAL,        // General/unknown errors
    ERROR_CATEGORY_MEMORY,         // Memory allocation/deallocation errors
    ERROR_CATEGORY_FILE_IO,        // File I/O operations
    ERROR_CATEGORY_NETWORK,        // Network operations
    ERROR_CATEGORY_RENDERING,      // Rendering/graphics operations
    ERROR_CATEGORY_AUDIO,          // Audio operations
    ERROR_CATEGORY_INPUT,          // Input handling
    ERROR_CATEGORY_SCRIPTING,      // Scripting engine errors
    ERROR_CATEGORY_SECURITY,       // Security-related errors
    ERROR_CATEGORY_VALIDATION,     // Data validation errors
    ERROR_CATEGORY_SYSTEM,         // System-level errors
    ERROR_CATEGORY_COUNT
} error_category_t;

// Error codes
typedef enum {
    // General errors
    ERROR_SUCCESS = 0,
    ERROR_UNKNOWN = 1,
    ERROR_INVALID_PARAMETER = 2,
    ERROR_OUT_OF_MEMORY = 3,
    ERROR_NOT_IMPLEMENTED = 4,
    ERROR_TIMEOUT = 5,
    ERROR_PERMISSION_DENIED = 6,
    ERROR_NOT_FOUND = 7,
    ERROR_ALREADY_EXISTS = 8,

    // Memory errors
    ERROR_MEMORY_CORRUPTION = 100,
    ERROR_DOUBLE_FREE = 101,
    ERROR_INVALID_FREE = 102,
    ERROR_MEMORY_LEAK = 103,
    ERROR_BUFFER_OVERFLOW = 104,
    ERROR_BUFFER_UNDERFLOW = 105,

    // File I/O errors
    ERROR_FILE_NOT_FOUND = 200,
    ERROR_FILE_ACCESS_DENIED = 201,
    ERROR_FILE_CORRUPTED = 202,
    ERROR_FILE_TOO_LARGE = 203,
    ERROR_DISK_FULL = 204,
    ERROR_IO_ERROR = 205,

    // Network errors
    ERROR_NETWORK_UNREACHABLE = 300,
    ERROR_CONNECTION_REFUSED = 301,
    ERROR_CONNECTION_TIMEOUT = 302,
    ERROR_PROTOCOL_ERROR = 303,
    ERROR_HOST_NOT_FOUND = 304,

    // Rendering errors
    ERROR_GPU_NOT_SUPPORTED = 400,
    ERROR_SHADER_COMPILE_FAILED = 401,
    ERROR_TEXTURE_LOAD_FAILED = 402,
    ERROR_RENDER_TARGET_ERROR = 403,
    ERROR_PIPELINE_ERROR = 404,

    // Audio errors
    ERROR_AUDIO_DEVICE_ERROR = 500,
    ERROR_AUDIO_FORMAT_UNSUPPORTED = 501,
    ERROR_AUDIO_BUFFER_UNDERFLOW = 502,

    // Validation errors
    ERROR_VALIDATION_FAILED = 600,
    ERROR_TYPE_MISMATCH = 601,
    ERROR_RANGE_ERROR = 602,
    ERROR_FORMAT_ERROR = 603,

    // System errors
    ERROR_SYSTEM_CALL_FAILED = 700,
    ERROR_THREAD_ERROR = 701,
    ERROR_SYNCHRONIZATION_ERROR = 702,

    ERROR_CODE_COUNT
} error_code_t;

// Stack frame information for stack traces
#define MAX_STACK_DEPTH 32
#define MAX_FUNCTION_NAME 128
#define MAX_FILE_NAME 256

typedef struct {
    char function_name[MAX_FUNCTION_NAME];
    char file_name[MAX_FILE_NAME];
    int line_number;
    uintptr_t address;             // Function address for symbol resolution
} stack_frame_t;

// Error context information
typedef struct {
    error_code_t error_code;
    error_severity_t severity;
    error_category_t category;
    char message[512];             // Human-readable error message
    char details[1024];            // Detailed error information

    // Location information
    char file[MAX_FILE_NAME];
    char function[MAX_FUNCTION_NAME];
    int line;

    // Stack trace
    stack_frame_t stack_trace[MAX_STACK_DEPTH];
    int stack_depth;

    // Context data
    uint64_t timestamp;            // When the error occurred
    int thread_id;                 // Thread that generated the error
    char module[64];               // Module/component that generated the error

    // Recovery information
    qboolean recoverable;          // Whether this error can be recovered from
    char recovery_hint[256];       // Suggestion for recovery

    // Chain of errors (for nested error handling)
    struct error_context_t *cause; // Root cause of this error
} error_context_t;

// Error handler function type
typedef void (*error_handler_t)(const error_context_t *error);

// Exception handling context (setjmp/longjmp based)
typedef struct {
    jmp_buf jump_buffer;
    error_context_t error_context;
    qboolean error_occurred;
    qboolean handler_installed;
} exception_context_t;

// Global error handling system
typedef struct {
    // Configuration
    qboolean stack_traces_enabled;
    qboolean error_logging_enabled;
    qboolean fatal_errors_exit;
    int max_errors_per_second;     // Rate limiting
    char log_file[256];

    // Statistics
    uint64_t total_errors;
    uint64_t errors_by_severity[ERROR_SEVERITY_COUNT];
    uint64_t errors_by_category[ERROR_CATEGORY_COUNT];

    // Error handlers
    error_handler_t global_error_handler;
    error_handler_t category_handlers[ERROR_CATEGORY_COUNT];

    // Current error state
    error_context_t *current_error;
    exception_context_t *current_exception;

    // System state
    qboolean initialized;
    uint64_t last_error_time;
    int error_count_this_second;
} error_system_t;

extern error_system_t error_system;

// Error Handling API
qboolean Error_Init(void);
void Error_Shutdown(void);

// Error creation and reporting
error_context_t* Error_Create(error_code_t code, error_severity_t severity,
                             const char *message, const char *file,
                             const char *function, int line);
void Error_Report(error_context_t *error);
void Error_ReportSimple(error_code_t code, const char *message);
void Error_ReportWithContext(error_code_t code, const char *message,
                           error_category_t category, const char *details);

// Stack trace generation
qboolean Error_CaptureStackTrace(error_context_t *error);
qboolean Error_ResolveSymbols(stack_frame_t *frames, int depth);
void Error_PrintStackTrace(const error_context_t *error);

// Error recovery and handling
qboolean Error_IsRecoverable(const error_context_t *error);
qboolean Error_AttemptRecovery(error_context_t *error);
void Error_SetRecoveryHint(error_context_t *error, const char *hint);

// Exception handling (setjmp/longjmp based)
#define TRY \
    do { \
        exception_context_t __exception_ctx; \
        __exception_ctx.error_occurred = qfalse; \
        __exception_ctx.handler_installed = qtrue; \
        if (setjmp(__exception_ctx.jump_buffer) == 0) { \
            error_system.current_exception = &__exception_ctx;

#define CATCH(error_var) \
        } else { \
            error_var = &__exception_ctx.error_context; \
            __exception_ctx.handler_installed = qfalse;

#define END_TRY \
        } \
        if (__exception_ctx.handler_installed) { \
            error_system.current_exception = NULL; \
        } \
    } while(0)

#define THROW(code, message) \
    Error_Throw(code, ERROR_SEVERITY_ERROR, message, __FILE__, __FUNCTION__, __LINE__)

#define THROW_FATAL(code, message) \
    Error_Throw(code, ERROR_SEVERITY_FATAL, message, __FILE__, __FUNCTION__, __LINE__)

void Error_Throw(error_code_t code, error_severity_t severity, const char *message,
                const char *file, const char *function, int line) __attribute__((noreturn));

// Error handler registration
void Error_SetGlobalHandler(error_handler_t handler);
void Error_SetCategoryHandler(error_category_t category, error_handler_t handler);
void Error_RemoveHandler(error_handler_t handler);

// Error querying and management
uint64_t Error_GetTotalCount(void);
uint64_t Error_GetCountBySeverity(error_severity_t severity);
uint64_t Error_GetCountByCategory(error_category_t category);
const error_context_t* Error_GetLastError(void);
qboolean Error_HasPendingErrors(void);

// Error logging and serialization
qboolean Error_LogToFile(const error_context_t *error, const char *filename);
qboolean Error_SerializeToJSON(const error_context_t *error, char *buffer, size_t buffer_size);
qboolean Error_SerializeToXML(const error_context_t *error, char *buffer, size_t buffer_size);

// Error filtering and suppression
typedef qboolean (*error_filter_t)(const error_context_t *error);
void Error_AddFilter(error_filter_t filter);
void Error_RemoveFilter(error_filter_t filter);
void Error_ClearFilters(void);

// Utility functions
const char* Error_GetSeverityString(error_severity_t severity);
const char* Error_GetCategoryString(error_category_t category);
const char* Error_GetCodeString(error_code_t code);
qboolean Error_IsSystemError(error_code_t code);
qboolean Error_IsNetworkError(error_code_t code);
qboolean Error_IsFileError(error_code_t code);

// Error context management
error_context_t* Error_AllocateContext(void);
void Error_FreeContext(error_context_t *context);
void Error_CopyContext(error_context_t *dest, const error_context_t *src);

// Performance and statistics
typedef struct {
    uint64_t total_errors_handled;
    uint64_t stack_traces_captured;
    uint64_t exceptions_thrown;
    uint64_t exceptions_caught;
    uint64_t errors_filtered;
    uint64_t errors_logged;
    double avg_error_handling_time_ms;
    uint64_t max_concurrent_errors;
} error_statistics_t;

void Error_GetStatistics(error_statistics_t *stats);
void Error_ResetStatistics(void);

// Integration helpers for common operations
#define ERROR_CHECK(condition, code, message) \
    do { \
        if (!(condition)) { \
            Error_ReportSimple(code, message); \
        } \
    } while(0)

#define ERROR_CHECK_RETURN(condition, code, message, return_value) \
    do { \
        if (!(condition)) { \
            Error_ReportSimple(code, message); \
            return return_value; \
        } \
    } while(0)

#define ERROR_CHECK_GOTO(condition, code, message, label) \
    do { \
        if (!(condition)) { \
            Error_ReportSimple(code, message); \
            goto label; \
        } \
    } while(0)

// Memory error helpers
#define MEMORY_ERROR_CHECK(ptr, operation) \
    ERROR_CHECK((ptr) != NULL, ERROR_OUT_OF_MEMORY, "Memory allocation failed in " operation)

#define MEMORY_ERROR_CHECK_RETURN(ptr, operation, return_value) \
    ERROR_CHECK_RETURN((ptr) != NULL, ERROR_OUT_OF_MEMORY, "Memory allocation failed in " operation, return_value)

// File I/O error helpers
#define FILE_ERROR_CHECK(result, filename, operation) \
    ERROR_CHECK(result, ERROR_IO_ERROR, va("File operation '%s' failed on '%s'", operation, filename))

#define FILE_ERROR_CHECK_RETURN(result, filename, operation, return_value) \
    ERROR_CHECK_RETURN(result, ERROR_IO_ERROR, va("File operation '%s' failed on '%s'", operation, filename), return_value)

#endif // __ERROR_HANDLING_H__
