/*
=============================================================================
Error Handling Framework Implementation

Structured error handling with stack traces and comprehensive error management.
=============================================================================
*/

#include "error_handling.h"
#include "qcommon.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <execinfo.h>
#include <unistd.h>
#include <pthread.h>

// Global error system instance
error_system_t error_system = {0};

// Error filter list
#define MAX_ERROR_FILTERS 16
static error_filter_t error_filters[MAX_ERROR_FILTERS];
static int num_error_filters = 0;

// Error context pool for efficient allocation
#define MAX_ERROR_CONTEXTS 64
static error_context_t error_context_pool[MAX_ERROR_CONTEXTS];
static qboolean context_used[MAX_ERROR_CONTEXTS] = {qfalse};

// Thread-local exception context
__thread exception_context_t *thread_exception_context = NULL;

// Error string tables
static const char *error_severity_strings[ERROR_SEVERITY_COUNT] = {
    "INFO",
    "WARNING",
    "ERROR",
    "CRITICAL",
    "FATAL"
};

static const char *error_category_strings[ERROR_CATEGORY_COUNT] = {
    "General",
    "Memory",
    "File I/O",
    "Network",
    "Rendering",
    "Audio",
    "Input",
    "Scripting",
    "Security",
    "Validation",
    "System"
};

static const char *error_code_strings[ERROR_CODE_COUNT] = {
    // General errors
    "SUCCESS",
    "UNKNOWN",
    "INVALID_PARAMETER",
    "OUT_OF_MEMORY",
    "NOT_IMPLEMENTED",
    "TIMEOUT",
    "PERMISSION_DENIED",
    "NOT_FOUND",
    "ALREADY_EXISTS",

    // Memory errors (100-199)
    "MEMORY_CORRUPTION",
    "DOUBLE_FREE",
    "INVALID_FREE",
    "MEMORY_LEAK",
    "BUFFER_OVERFLOW",
    "BUFFER_UNDERFLOW",

    // File I/O errors (200-299)
    "FILE_NOT_FOUND",
    "FILE_ACCESS_DENIED",
    "FILE_CORRUPTED",
    "FILE_TOO_LARGE",
    "DISK_FULL",
    "IO_ERROR",

    // Network errors (300-399)
    "NETWORK_UNREACHABLE",
    "CONNECTION_REFUSED",
    "CONNECTION_TIMEOUT",
    "PROTOCOL_ERROR",
    "HOST_NOT_FOUND",

    // Rendering errors (400-499)
    "GPU_NOT_SUPPORTED",
    "SHADER_COMPILE_FAILED",
    "TEXTURE_LOAD_FAILED",
    "RENDER_TARGET_ERROR",
    "PIPELINE_ERROR",

    // Audio errors (500-599)
    "AUDIO_DEVICE_ERROR",
    "AUDIO_FORMAT_UNSUPPORTED",
    "AUDIO_BUFFER_UNDERFLOW",

    // Validation errors (600-699)
    "VALIDATION_FAILED",
    "TYPE_MISMATCH",
    "RANGE_ERROR",
    "FORMAT_ERROR",

    // System errors (700-799)
    "SYSTEM_CALL_FAILED",
    "THREAD_ERROR",
    "SYNCHRONIZATION_ERROR"
};

/*
=============================================================================
Error Handling API Implementation
=============================================================================
*/

qboolean Error_Init(void) {
    if (error_system.initialized) {
        return qtrue;
    }

    memset(&error_system, 0, sizeof(error_system_t));

    // Set default configuration
    error_system.stack_traces_enabled = qtrue;
    error_system.error_logging_enabled = qtrue;
    error_system.fatal_errors_exit = qtrue;
    error_system.max_errors_per_second = 100;
    Q_strncpyz(error_system.log_file, "error_log.txt", sizeof(error_system.log_file));

    // Initialize error context pool
    memset(error_context_pool, 0, sizeof(error_context_pool));
    memset(context_used, 0, sizeof(context_used));

    error_system.initialized = qtrue;

    Com_Printf("Error handling framework initialized with stack trace support\n");

    // Test stack trace capability
    if (Error_CaptureStackTrace(NULL)) {
        Com_Printf("Stack trace capture: ENABLED\n");
    } else {
        Com_Printf("Stack trace capture: DISABLED (backtrace not available)\n");
        error_system.stack_traces_enabled = qfalse;
    }

    return qtrue;
}

void Error_Shutdown(void) {
    if (!error_system.initialized) {
        return;
    }

    // Log final statistics
    if (error_system.total_errors > 0) {
        Com_Printf("Error handling shutdown - Total errors handled: %llu\n", error_system.total_errors);
        Error_ReportViolations();
    }

    // Clear all filters
    Error_ClearFilters();

    // Reset state
    error_system.initialized = qfalse;
    error_system.current_error = NULL;
    error_system.current_exception = NULL;

    Com_Printf("Error handling framework shutdown\n");
}

/*
=============================================================================
Error Creation and Reporting
=============================================================================
*/

error_context_t* Error_AllocateContext(void) {
    for (int i = 0; i < MAX_ERROR_CONTEXTS; i++) {
        if (!context_used[i]) {
            context_used[i] = qtrue;
            memset(&error_context_pool[i], 0, sizeof(error_context_t));
            return &error_context_pool[i];
        }
    }

    // Fallback: allocate from heap if pool is exhausted
    Com_Printf("Warning: Error context pool exhausted, allocating from heap\n");
    error_context_t *context = (error_context_t *)malloc(sizeof(error_context_t));
    if (context) {
        memset(context, 0, sizeof(error_context_t));
    }
    return context;
}

void Error_FreeContext(error_context_t *context) {
    if (!context) return;

    // Check if it's from our pool
    ptrdiff_t offset = (char*)context - (char*)error_context_pool;
    if (offset >= 0 && offset < sizeof(error_context_pool)) {
        int index = offset / sizeof(error_context_t);
        if (index >= 0 && index < MAX_ERROR_CONTEXTS) {
            context_used[index] = qfalse;
            return;
        }
    }

    // It was allocated from heap
    free(context);
}

error_context_t* Error_Create(error_code_t code, error_severity_t severity,
                             const char *message, const char *file,
                             const char *function, int line) {
    error_context_t *error = Error_AllocateContext();
    if (!error) {
        Com_Printf("ERROR: Failed to allocate error context\n");
        return NULL;
    }

    error->error_code = code;
    error->severity = severity;
    error->category = Error_GetCategoryFromCode(code);

    if (message) {
        Q_strncpyz(error->message, message, sizeof(error->message));
    }

    if (file) {
        Q_strncpyz(error->file, file, sizeof(error->file));
    }

    if (function) {
        Q_strncpyz(error->function, function, sizeof(error->function));
    }

    error->line = line;
    error->timestamp = Sys_Milliseconds();

    // Get thread ID (simplified)
    error->thread_id = (int)(uintptr_t)pthread_self();

    // Determine if recoverable
    error->recoverable = (severity < ERROR_SEVERITY_FATAL);

    // Capture stack trace if enabled
    if (error_system.stack_traces_enabled) {
        Error_CaptureStackTrace(error);
    }

    return error;
}

void Error_Report(error_context_t *error) {
    if (!error) return;

    // Rate limiting
    uint64_t current_time = Sys_Milliseconds();
    if (current_time - error_system.last_error_time < 1000) { // Within 1 second
        error_system.error_count_this_second++;
        if (error_system.error_count_this_second > error_system.max_errors_per_second) {
            // Too many errors per second, skip reporting but count it
            error_system.total_errors++;
            error_system.errors_by_severity[error->severity]++;
            error_system.errors_by_category[error->category]++;
            return;
        }
    } else {
        error_system.error_count_this_second = 1;
        error_system.last_error_time = current_time;
    }

    // Update statistics
    error_system.total_errors++;
    error_system.errors_by_severity[error->severity]++;
    error_system.errors_by_category[error->category]++;
    error_system.current_error = error;

    // Check filters
    for (int i = 0; i < num_error_filters; i++) {
        if (error_filters[i] && error_filters[i](error)) {
            // Error was filtered out
            return;
        }
    }

    // Format error message
    char formatted_message[1024];
    Com_sprintf(formatted_message, sizeof(formatted_message),
                "[%s] %s:%d in %s(): %s",
                Error_GetSeverityString(error->severity),
                error->file, error->line, error->function, error->message);

    // Log to console
    switch (error->severity) {
        case ERROR_SEVERITY_INFO:
            Com_Printf("INFO: %s\n", formatted_message);
            break;
        case ERROR_SEVERITY_WARNING:
            Com_Printf("WARNING: %s\n", formatted_message);
            break;
        case ERROR_SEVERITY_ERROR:
            Com_Printf("ERROR: %s\n", formatted_message);
            break;
        case ERROR_SEVERITY_CRITICAL:
            Com_Printf("CRITICAL: %s\n", formatted_message);
            break;
        case ERROR_SEVERITY_FATAL:
            Com_Printf("FATAL: %s\n", formatted_message);
            if (error_system.fatal_errors_exit) {
                Com_Error(ERR_FATAL, "Fatal error: %s", error->message);
            }
            break;
    }

    // Print stack trace if available and severity is high enough
    if (error->stack_depth > 0 && error->severity >= ERROR_SEVERITY_ERROR) {
        Error_PrintStackTrace(error);
    }

    // Log to file if enabled
    if (error_system.error_logging_enabled) {
        Error_LogToFile(error, error_system.log_file);
    }

    // Call error handlers
    if (error_system.global_error_handler) {
        error_system.global_error_handler(error);
    }

    if (error_system.category_handlers[error->category]) {
        error_system.category_handlers[error->category](error);
    }

    // Handle exceptions if we're in a try block
    if (thread_exception_context && thread_exception_context->handler_installed) {
        memcpy(&thread_exception_context->error_context, error, sizeof(error_context_t));
        thread_exception_context->error_occurred = qtrue;
        longjmp(thread_exception_context->jump_buffer, 1);
    }
}

void Error_ReportSimple(error_code_t code, const char *message) {
    error_context_t *error = Error_Create(code, ERROR_SEVERITY_ERROR, message,
                                        "unknown", "unknown", 0);
    if (error) {
        Error_Report(error);
        Error_FreeContext(error);
    }
}

void Error_ReportWithContext(error_code_t code, const char *message,
                           error_category_t category, const char *details) {
    error_context_t *error = Error_Create(code, ERROR_SEVERITY_ERROR, message,
                                        "unknown", "unknown", 0);
    if (error) {
        error->category = category;
        if (details) {
            Q_strncpyz(error->details, details, sizeof(error->details));
        }
        Error_Report(error);
        Error_FreeContext(error);
    }
}

/*
=============================================================================
Stack Trace Generation
=============================================================================
*/

qboolean Error_CaptureStackTrace(error_context_t *error) {
    if (!error_system.stack_traces_enabled) {
        return qfalse;
    }

    void *buffer[MAX_STACK_DEPTH];
    int nptrs = backtrace(buffer, MAX_STACK_DEPTH);

    if (nptrs <= 1) { // Skip this function itself
        return qfalse;
    }

    // Convert to our format (skip Error_CaptureStackTrace itself)
    error->stack_depth = 0;
    for (int i = 1; i < nptrs && error->stack_depth < MAX_STACK_DEPTH; i++) {
        stack_frame_t *frame = &error->stack_trace[error->stack_depth];
        frame->address = (uintptr_t)buffer[i];

        // Try to resolve symbol information
        char **strings = backtrace_symbols(buffer, nptrs);
        if (strings && strings[i]) {
            // Parse the backtrace_symbols format: "binary(function+offset) [address]"
            char *symbol = strings[i];

            // Extract function name
            char *paren = strchr(symbol, '(');
            if (paren) {
                *paren = '\0';
                Q_strncpyz(frame->file_name, symbol, sizeof(frame->file_name));

                char *plus = strchr(paren + 1, '+');
                if (plus) {
                    *plus = '\0';
                    Q_strncpyz(frame->function_name, paren + 1, sizeof(frame->function_name));
                }
            }

            free(strings);
        }

        error->stack_depth++;
    }

    return error->stack_depth > 0;
}

qboolean Error_ResolveSymbols(stack_frame_t *frames, int depth) {
    // Additional symbol resolution could be implemented here
    // For now, we rely on backtrace_symbols
    Q_UNUSED(frames);
    Q_UNUSED(depth);
    return qtrue;
}

void Error_PrintStackTrace(const error_context_t *error) {
    if (!error || error->stack_depth <= 0) {
        return;
    }

    Com_Printf("Stack trace:\n");

    for (int i = 0; i < error->stack_depth; i++) {
        const stack_frame_t *frame = &error->stack_trace[i];

        if (frame->function_name[0]) {
            Com_Printf("  #%d %s", i, frame->function_name);
            if (frame->file_name[0]) {
                Com_Printf(" (%s", frame->file_name);
                if (frame->line_number > 0) {
                    Com_Printf(":%d", frame->line_number);
                }
                Com_Printf(")");
            }
            Com_Printf(" [0x%lx]\n", (unsigned long)frame->address);
        } else {
            Com_Printf("  #%d [0x%lx]\n", i, (unsigned long)frame->address);
        }
    }
}

/*
=============================================================================
Exception Handling
=============================================================================
*/

void Error_Throw(error_code_t code, error_severity_t severity, const char *message,
                const char *file, const char *function, int line) {
    error_context_t *error = Error_Create(code, severity, message, file, function, line);
    if (error) {
        Error_Report(error);
        Error_FreeContext(error);
    }

    // This function should never return
    abort();
}

/*
=============================================================================
Error Recovery and Handling
=============================================================================
*/

qboolean Error_IsRecoverable(const error_context_t *error) {
    if (!error) return qfalse;
    return error->recoverable;
}

qboolean Error_AttemptRecovery(error_context_t *error) {
    if (!error || !Error_IsRecoverable(error)) {
        return qfalse;
    }

    // Attempt recovery based on error type
    switch (error->error_code) {
        case ERROR_OUT_OF_MEMORY:
            // Try garbage collection or memory cleanup
            // This would integrate with the memory management system
            Com_Printf("Attempting memory recovery for error: %s\n", error->message);
            return qfalse; // For now, memory errors are not recoverable

        case ERROR_FILE_NOT_FOUND:
        case ERROR_FILE_ACCESS_DENIED:
            // Could try alternative paths or permissions
            Com_Printf("Attempting file access recovery for error: %s\n", error->message);
            return qfalse; // For now, file errors are not recoverable

        case ERROR_TIMEOUT:
            // Could retry the operation
            Com_Printf("Attempting timeout recovery for error: %s\n", error->message);
            return qfalse; // For now, timeouts are not recoverable

        default:
            // Unknown error type, not recoverable
            return qfalse;
    }
}

void Error_SetRecoveryHint(error_context_t *error, const char *hint) {
    if (error && hint) {
        Q_strncpyz(error->recovery_hint, hint, sizeof(error->recovery_hint));
    }
}

/*
=============================================================================
Error Handler Registration
=============================================================================
*/

void Error_SetGlobalHandler(error_handler_t handler) {
    error_system.global_error_handler = handler;
}

void Error_SetCategoryHandler(error_category_t category, error_handler_t handler) {
    if (category < ERROR_CATEGORY_COUNT) {
        error_system.category_handlers[category] = handler;
    }
}

void Error_RemoveHandler(error_handler_t handler) {
    if (error_system.global_error_handler == handler) {
        error_system.global_error_handler = NULL;
    }

    for (int i = 0; i < ERROR_CATEGORY_COUNT; i++) {
        if (error_system.category_handlers[i] == handler) {
            error_system.category_handlers[i] = NULL;
        }
    }
}

/*
=============================================================================
Error Querying and Management
=============================================================================
*/

uint64_t Error_GetTotalCount(void) {
    return error_system.total_errors;
}

uint64_t Error_GetCountBySeverity(error_severity_t severity) {
    if (severity >= ERROR_SEVERITY_COUNT) return 0;
    return error_system.errors_by_severity[severity];
}

uint64_t Error_GetCountByCategory(error_category_t category) {
    if (category >= ERROR_CATEGORY_COUNT) return 0;
    return error_system.errors_by_category[category];
}

const error_context_t* Error_GetLastError(void) {
    return error_system.current_error;
}

qboolean Error_HasPendingErrors(void) {
    return error_system.current_error != NULL;
}

/*
=============================================================================
Error Logging and Serialization
=============================================================================
*/

qboolean Error_LogToFile(const error_context_t *error, const char *filename) {
    if (!error || !filename) return qfalse;

    FILE *file = fopen(filename, "a");
    if (!file) return qfalse;

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    fprintf(file, "[%s] [%s] %s:%d in %s(): %s\n",
            time_str,
            Error_GetSeverityString(error->severity),
            error->file, error->line, error->function,
            error->message);

    if (error->details[0]) {
        fprintf(file, "Details: %s\n", error->details);
    }

    if (error->stack_depth > 0) {
        fprintf(file, "Stack trace:\n");
        for (int i = 0; i < error->stack_depth; i++) {
            const stack_frame_t *frame = &error->stack_trace[i];
            fprintf(file, "  #%d %s [0x%lx]\n", i,
                    frame->function_name[0] ? frame->function_name : "???",
                    (unsigned long)frame->address);
        }
    }

    fprintf(file, "\n");
    fclose(file);
    return qtrue;
}

qboolean Error_SerializeToJSON(const error_context_t *error, char *buffer, size_t buffer_size) {
    if (!error || !buffer || buffer_size == 0) return qfalse;

    int written = Q_snprintf(buffer, buffer_size,
        "{\n"
        "  \"error_code\": %d,\n"
        "  \"severity\": \"%s\",\n"
        "  \"category\": \"%s\",\n"
        "  \"message\": \"%s\",\n"
        "  \"file\": \"%s\",\n"
        "  \"function\": \"%s\",\n"
        "  \"line\": %d,\n"
        "  \"timestamp\": %llu,\n"
        "  \"thread_id\": %d,\n"
        "  \"recoverable\": %s",
        error->error_code,
        Error_GetSeverityString(error->severity),
        Error_GetCategoryString(error->category),
        error->message,
        error->file,
        error->function,
        error->line,
        error->timestamp,
        error->thread_id,
        error->recoverable ? "true" : "false");

    if (written < 0 || (size_t)written >= buffer_size - 50) return qfalse;

    // Add stack trace if available
    if (error->stack_depth > 0) {
        Q_strcat(buffer, buffer_size, ",\n  \"stack_trace\": [\n");
        for (int i = 0; i < error->stack_depth; i++) {
            const stack_frame_t *frame = &error->stack_trace[i];
            char frame_str[256];
            Q_snprintf(frame_str, sizeof(frame_str),
                "    {\"frame\": %d, \"function\": \"%s\", \"address\": \"0x%lx\"}%s\n",
                i, frame->function_name, (unsigned long)frame->address,
                (i < error->stack_depth - 1) ? "," : "");
            Q_strcat(buffer, buffer_size, frame_str);
        }
        Q_strcat(buffer, buffer_size, "  ]\n");
    } else {
        Q_strcat(buffer, buffer_size, "\n");
    }

    Q_strcat(buffer, buffer_size, "}\n");
    return qtrue;
}

qboolean Error_SerializeToXML(const error_context_t *error, char *buffer, size_t buffer_size) {
    if (!error || !buffer || buffer_size == 0) return qfalse;

    int written = Q_snprintf(buffer, buffer_size,
        "<error>\n"
        "  <code>%d</code>\n"
        "  <severity>%s</severity>\n"
        "  <category>%s</category>\n"
        "  <message>%s</message>\n"
        "  <location>\n"
        "    <file>%s</file>\n"
        "    <function>%s</function>\n"
        "    <line>%d</line>\n"
        "  </location>\n"
        "  <timestamp>%llu</timestamp>\n"
        "  <thread_id>%d</thread_id>\n"
        "  <recoverable>%s</recoverable>\n",
        error->error_code,
        Error_GetSeverityString(error->severity),
        Error_GetCategoryString(error->category),
        error->message,
        error->file,
        error->function,
        error->line,
        error->timestamp,
        error->thread_id,
        error->recoverable ? "true" : "false");

    if (written < 0 || (size_t)written >= buffer_size - 100) return qfalse;

    // Add stack trace if available
    if (error->stack_depth > 0) {
        Q_strcat(buffer, buffer_size, "  <stack_trace>\n");
        for (int i = 0; i < error->stack_depth; i++) {
            const stack_frame_t *frame = &error->stack_trace[i];
            char frame_str[128];
            Q_snprintf(frame_str, sizeof(frame_str),
                "    <frame number=\"%d\" function=\"%s\" address=\"0x%lx\" />\n",
                i, frame->function_name, (unsigned long)frame->address);
            Q_strcat(buffer, buffer_size, frame_str);
        }
        Q_strcat(buffer, buffer_size, "  </stack_trace>\n");
    }

    Q_strcat(buffer, buffer_size, "</error>\n");
    return qtrue;
}

/*
=============================================================================
Error Filtering and Suppression
=============================================================================
*/

void Error_AddFilter(error_filter_t filter) {
    if (num_error_filters < MAX_ERROR_FILTERS) {
        error_filters[num_error_filters++] = filter;
    }
}

void Error_RemoveFilter(error_filter_t filter) {
    for (int i = 0; i < num_error_filters; i++) {
        if (error_filters[i] == filter) {
            // Shift remaining filters
            for (int j = i; j < num_error_filters - 1; j++) {
                error_filters[j] = error_filters[j + 1];
            }
            error_filters[--num_error_filters] = NULL;
            break;
        }
    }
}

void Error_ClearFilters(void) {
    memset(error_filters, 0, sizeof(error_filters));
    num_error_filters = 0;
}

/*
=============================================================================
Utility Functions
=============================================================================
*/

const char* Error_GetSeverityString(error_severity_t severity) {
    if (severity >= ERROR_SEVERITY_COUNT) return "UNKNOWN";
    return error_severity_strings[severity];
}

const char* Error_GetCategoryString(error_category_t category) {
    if (category >= ERROR_CATEGORY_COUNT) return "Unknown";
    return error_category_strings[category];
}

const char* Error_GetCodeString(error_code_t code) {
    if (code >= ERROR_CODE_COUNT) return "UNKNOWN_ERROR";
    return error_code_strings[code];
}

qboolean Error_IsSystemError(error_code_t code) {
    return code >= ERROR_SYSTEM_CALL_FAILED && code <= ERROR_SYNCHRONIZATION_ERROR;
}

qboolean Error_IsNetworkError(error_code_t code) {
    return code >= ERROR_NETWORK_UNREACHABLE && code <= ERROR_HOST_NOT_FOUND;
}

qboolean Error_IsFileError(error_code_t code) {
    return code >= ERROR_FILE_NOT_FOUND && code <= ERROR_IO_ERROR;
}

error_category_t Error_GetCategoryFromCode(error_code_t code) {
    if (code >= ERROR_MEMORY_CORRUPTION && code <= ERROR_BUFFER_UNDERFLOW) {
        return ERROR_CATEGORY_MEMORY;
    } else if (code >= ERROR_FILE_NOT_FOUND && code <= ERROR_IO_ERROR) {
        return ERROR_CATEGORY_FILE_IO;
    } else if (code >= ERROR_NETWORK_UNREACHABLE && code <= ERROR_HOST_NOT_FOUND) {
        return ERROR_CATEGORY_NETWORK;
    } else if (code >= ERROR_GPU_NOT_SUPPORTED && code <= ERROR_PIPELINE_ERROR) {
        return ERROR_CATEGORY_RENDERING;
    } else if (code >= ERROR_AUDIO_DEVICE_ERROR && code <= ERROR_AUDIO_BUFFER_UNDERFLOW) {
        return ERROR_CATEGORY_AUDIO;
    } else if (code >= ERROR_VALIDATION_FAILED && code <= ERROR_FORMAT_ERROR) {
        return ERROR_CATEGORY_VALIDATION;
    } else if (code >= ERROR_SYSTEM_CALL_FAILED && code <= ERROR_SYNCHRONIZATION_ERROR) {
        return ERROR_CATEGORY_SYSTEM;
    }

    return ERROR_CATEGORY_GENERAL;
}

void Error_CopyContext(error_context_t *dest, const error_context_t *src) {
    if (!dest || !src) return;
    memcpy(dest, src, sizeof(error_context_t));
}

/*
=============================================================================
Performance and Statistics
=============================================================================
*/

void Error_GetStatistics(error_statistics_t *stats) {
    if (!stats) return;

    memset(stats, 0, sizeof(error_statistics_t));
    stats->total_errors_handled = error_system.total_errors;

    // These would be tracked with performance counters in a full implementation
    stats->stack_traces_captured = 0; // Would need to track this
    stats->exceptions_thrown = 0;     // Would need to track this
    stats->exceptions_caught = 0;     // Would need to track this
    stats->errors_filtered = 0;       // Would need to track this
    stats->errors_logged = 0;         // Would need to track this
    stats->avg_error_handling_time_ms = 0.0; // Would need to measure this
    stats->max_concurrent_errors = 0; // Would need to track this
}

void Error_ResetStatistics(void) {
    memset(error_system.errors_by_severity, 0, sizeof(error_system.errors_by_severity));
    memset(error_system.errors_by_category, 0, sizeof(error_system.errors_by_category));
    error_system.total_errors = 0;
    error_system.error_count_this_second = 0;
    error_system.last_error_time = 0;
}

// Report error statistics summary
void Error_ReportViolations(void) {
    Com_Printf("=== Error Statistics Summary ===\n");
    Com_Printf("Total errors handled: %llu\n", error_system.total_errors);

    Com_Printf("\nErrors by severity:\n");
    for (int i = 0; i < ERROR_SEVERITY_COUNT; i++) {
        if (error_system.errors_by_severity[i] > 0) {
            Com_Printf("  %s: %llu\n",
                      Error_GetSeverityString(i),
                      error_system.errors_by_severity[i]);
        }
    }

    Com_Printf("\nErrors by category:\n");
    for (int i = 0; i < ERROR_CATEGORY_COUNT; i++) {
        if (error_system.errors_by_category[i] > 0) {
            Com_Printf("  %s: %llu\n",
                      Error_GetCategoryString(i),
                      error_system.errors_by_category[i]);
        }
    }

    if (error_system.total_errors > 100) {
        Com_Printf("\nWARNING: High error count detected (%llu total)\n",
                  error_system.total_errors);
    }

    Com_Printf("================================\n");
}
