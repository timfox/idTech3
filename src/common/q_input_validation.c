/*
===========================================================================
q_input_validation.c - Enhanced Input Validation and Sanitization
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "q_input_validation.h"

// Input validation configuration
cvar_t *input_validation_enable;
cvar_t *input_validation_strict_mode;
cvar_t *input_validation_log_suspicious;
cvar_t *input_validation_max_length;
cvar_t *input_validation_filter_unicode;
cvar_t *input_validation_check_null_bytes;
cvar_t *input_validation_validate_paths;
cvar_t *input_validation_sanitize_sql;
cvar_t *input_validation_rate_limit;

// Input validation statistics
static input_validation_stats_t validation_stats;
static input_validation_state_t validation_state;

// Rate limiting
#define RATE_LIMIT_WINDOW 1000  // 1 second
#define RATE_LIMIT_MAX_REQUESTS 10

typedef struct {
    int timestamp;
    int request_count;
} rate_limit_entry_t;

static rate_limit_entry_t rate_limits[MAX_CLIENTS];

/*
===============
InputValidation_Init
===============
*/
void InputValidation_Init(void) {
    Com_Memset(&validation_stats, 0, sizeof(validation_stats));
    Com_Memset(&validation_state, 0, sizeof(validation_state));
    Com_Memset(&rate_limits, 0, sizeof(rate_limits));

    // Register CVars
    input_validation_enable = Cvar_Get("input_validation_enable", "1", CVAR_ARCHIVE | CVAR_LATCH,
        "Enable enhanced input validation and sanitization");
    input_validation_strict_mode = Cvar_Get("input_validation_strict_mode", "0", CVAR_ARCHIVE,
        "Enable strict input validation (reject suspicious input)");
    input_validation_log_suspicious = Cvar_Get("input_validation_log_suspicious", "1", CVAR_ARCHIVE,
        "Log suspicious input for monitoring");
    input_validation_max_length = Cvar_Get("input_validation_max_length", "1024", CVAR_ARCHIVE,
        "Maximum allowed input length");
    input_validation_filter_unicode = Cvar_Get("input_validation_filter_unicode", "1", CVAR_ARCHIVE,
        "Filter potentially dangerous Unicode characters");
    input_validation_check_null_bytes = Cvar_Get("input_validation_check_null_bytes", "1", CVAR_ARCHIVE,
        "Check for null byte injection attempts");
    input_validation_validate_paths = Cvar_Get("input_validation_validate_paths", "1", CVAR_ARCHIVE,
        "Validate file paths for directory traversal");
    input_validation_sanitize_sql = Cvar_Get("input_validation_sanitize_sql", "1", CVAR_ARCHIVE,
        "Sanitize input to prevent SQL injection");
    input_validation_rate_limit = Cvar_Get("input_validation_rate_limit", "1", CVAR_ARCHIVE,
        "Enable rate limiting for input commands");

    validation_state.initialized = qtrue;
    Com_Printf("Input validation system initialized\n");
}

/*
===============
InputValidation_Shutdown
===============
*/
void InputValidation_Shutdown(void) {
    if (!validation_state.initialized) {
        return;
    }

    // Generate final validation report
    InputValidation_GenerateReport();

    Com_Printf("Input validation system shutdown\n");
    validation_state.initialized = qfalse;
}

/*
===============
InputValidation_ValidateString
===============
*/
validation_result_t InputValidation_ValidateString(const char *input, validation_flags_t flags,
                                                 const char *context) {
    validation_result_t result = { qtrue, VALIDATION_ERROR_NONE, "" };

    if (!input_validation_enable->integer || !validation_state.initialized) {
        return result;
    }

    validation_stats.total_validations++;

    // Check for NULL input
    if (!input) {
        result.valid = qfalse;
        result.error = VALIDATION_ERROR_NULL_INPUT;
        Q_strncpyz(result.message, "NULL input provided", sizeof(result.message));
        goto validation_complete;
    }

    // Check length
    size_t length = strlen(input);
    if (length > input_validation_max_length->integer) {
        result.valid = qfalse;
        result.error = VALIDATION_ERROR_TOO_LONG;
        Com_sprintf(result.message, sizeof(result.message),
            "Input too long (%d > %d)", length, input_validation_max_length->integer);
        goto validation_complete;
    }

    // Check for null bytes
    if (input_validation_check_null_bytes->integer && flags & VALIDATION_FLAG_CHECK_NULL) {
        if (memchr(input, '\0', length) != NULL) {
            result.valid = qfalse;
            result.error = VALIDATION_ERROR_NULL_BYTE;
            Q_strncpyz(result.message, "Null byte detected", sizeof(result.message));
            goto validation_complete;
        }
    }

    // Validate paths
    if (input_validation_validate_paths->integer && flags & VALIDATION_FLAG_PATH) {
        if (!InputValidation_ValidatePath(input)) {
            result.valid = qfalse;
            result.error = VALIDATION_ERROR_INVALID_PATH;
            Q_strncpyz(result.message, "Invalid path detected", sizeof(result.message));
            goto validation_complete;
        }
    }

    // Check for suspicious characters
    if (flags & VALIDATION_FLAG_TEXT) {
        char suspicious_chars[] = {'\n', '\r', '\t', '\v', '\f'};
        for (size_t i = 0; i < sizeof(suspicious_chars); i++) {
            if (strchr(input, suspicious_chars[i]) != NULL) {
                if (input_validation_strict_mode->integer) {
                    result.valid = qfalse;
                    result.error = VALIDATION_ERROR_SUSPICIOUS_CHARS;
                    Com_sprintf(result.message, sizeof(result.message),
                        "Suspicious character 0x%02X detected", suspicious_chars[i]);
                    goto validation_complete;
                } else if (input_validation_log_suspicious->integer) {
                    Com_Printf(S_COLOR_YELLOW "Suspicious character 0x%02X in input: %s\n",
                        suspicious_chars[i], context ? context : "unknown");
                }
            }
        }
    }

    // Unicode filtering
    if (input_validation_filter_unicode->integer && flags & VALIDATION_FLAG_UNICODE) {
        for (size_t i = 0; i < length; i++) {
            // Check for potentially dangerous Unicode ranges
            unsigned char c = (unsigned char)input[i];
            if (c >= 0xC0 && c <= 0xFD) {  // UTF-8 multi-byte start
                // Additional validation could be added here
                if (input_validation_log_suspicious->integer) {
                    Com_Printf(S_COLOR_YELLOW "Unicode character detected in input: %s\n",
                        context ? context : "unknown");
                }
            }
        }
    }

    // SQL injection prevention
    if (input_validation_sanitize_sql->integer && flags & VALIDATION_FLAG_SQL) {
        if (strstr(input, "UNION") || strstr(input, "SELECT") || strstr(input, "INSERT") ||
            strstr(input, "UPDATE") || strstr(input, "DELETE") || strstr(input, "DROP") ||
            strstr(input, "--") || strstr(input, "/*") || strstr(input, "*/")) {

            if (input_validation_strict_mode->integer) {
                result.valid = qfalse;
                result.error = VALIDATION_ERROR_SQL_INJECTION;
                Q_strncpyz(result.message, "Potential SQL injection detected", sizeof(result.message));
                goto validation_complete;
            } else if (input_validation_log_suspicious->integer) {
                Com_Printf(S_COLOR_YELLOW "Potential SQL injection in input: %s\n",
                    context ? context : "unknown");
            }
        }
    }

validation_complete:
    if (!result.valid) {
        validation_stats.failed_validations++;
        if (input_validation_log_suspicious->integer) {
            Com_Printf(S_COLOR_RED "Input validation failed (%s): %s\n",
                context ? context : "unknown", result.message);
        }
    } else {
        validation_stats.passed_validations++;
    }

    return result;
}

/*
===============
InputValidation_SanitizeString
===============
*/
void InputValidation_SanitizeString(char *output, size_t output_size, const char *input,
                                  validation_flags_t flags) {
    if (!output || !input || output_size == 0) {
        return;
    }

    size_t input_len = strlen(input);
    size_t copy_len = input_len < output_size - 1 ? input_len : output_size - 1;

    // Copy and sanitize
    for (size_t i = 0; i < copy_len; i++) {
        char c = input[i];

        // Replace dangerous characters
        if (flags & VALIDATION_FLAG_SANITIZE_CONTROL) {
            if (c < 32 && c != '\n' && c != '\t' && c != '\r') {
                c = '?';
            }
        }

        if (flags & VALIDATION_FLAG_SANITIZE_QUOTES) {
            if (c == '"' || c == '\'') {
                c = '_';
            }
        }

        if (flags & VALIDATION_FLAG_SANITIZE_PATH) {
            if (c == '/' || c == '\\' || c == '..' || c == ':') {
                c = '_';
            }
        }

        output[i] = c;
    }

    output[copy_len] = '\0';
}

/*
===============
InputValidation_ValidatePath
===============
*/
static qboolean InputValidation_ValidatePath(const char *path) {
    if (!path || !*path) {
        return qfalse;
    }

    // Check for directory traversal attempts
    if (strstr(path, "..") != NULL) {
        return qfalse;
    }

    // Check for absolute paths
    if (path[0] == '/' || path[0] == '\\') {
        return qfalse;
    }

    // Check for Windows drive letters
    if (strlen(path) >= 3 && path[1] == ':' && (path[2] == '\\' || path[2] == '/')) {
        return qfalse;
    }

    // Check for suspicious characters
    const char *invalid_chars = "<>|?*\"";
    for (size_t i = 0; invalid_chars[i]; i++) {
        if (strchr(path, invalid_chars[i]) != NULL) {
            return qfalse;
        }
    }

    return qtrue;
}

/*
===============
InputValidation_CheckRateLimit
===============
*/
rate_limit_result_t InputValidation_CheckRateLimit(int client_id, const char *command) {
    rate_limit_result_t result = { qtrue, 0 };

    if (!input_validation_rate_limit->integer) {
        return result;
    }

    if (client_id < 0 || client_id >= MAX_CLIENTS) {
        result.allowed = qfalse;
        return result;
    }

    int current_time = Sys_Milliseconds();
    rate_limit_entry_t *entry = &rate_limits[client_id];

    // Reset counter if window has passed
    if (current_time - entry->timestamp > RATE_LIMIT_WINDOW) {
        entry->request_count = 0;
        entry->timestamp = current_time;
    }

    entry->request_count++;

    if (entry->request_count > RATE_LIMIT_MAX_REQUESTS) {
        result.allowed = qfalse;
        result.retry_after = RATE_LIMIT_WINDOW - (current_time - entry->timestamp);

        validation_stats.rate_limit_hits++;
        Com_Printf(S_COLOR_YELLOW "Rate limit exceeded for client %d\n", client_id);
    }

    return result;
}

/*
===============
InputValidation_ValidateCommand
===============
*/
validation_result_t InputValidation_ValidateCommand(int client_id, const char *command,
                                                  const char **args, int arg_count) {
    validation_result_t result = { qtrue, VALIDATION_ERROR_NONE, "" };

    // Check rate limiting
    rate_limit_result_t rate_result = InputValidation_CheckRateLimit(client_id, command);
    if (!rate_result.allowed) {
        result.valid = qfalse;
        result.error = VALIDATION_ERROR_RATE_LIMITED;
        Com_sprintf(result.message, sizeof(result.message),
            "Rate limited, retry after %d ms", rate_result.retry_after);
        return result;
    }

    // Validate command name
    result = InputValidation_ValidateString(command, VALIDATION_FLAG_COMMAND, "command_name");
    if (!result.valid) {
        return result;
    }

    // Validate arguments
    for (int i = 0; i < arg_count && args[i]; i++) {
        char context[64];
        Com_sprintf(context, sizeof(context), "command_arg_%d", i);

        result = InputValidation_ValidateString(args[i],
            VALIDATION_FLAG_COMMAND | VALIDATION_FLAG_SANITIZE, context);
        if (!result.valid) {
            return result;
        }
    }

    return result;
}

/*
===============
InputValidation_ValidateUserInfo
===============
*/
validation_result_t InputValidation_ValidateUserInfo(const char *key, const char *value) {
    validation_result_t result = { qtrue, VALIDATION_ERROR_NONE, "" };

    // Validate key
    result = InputValidation_ValidateString(key, VALIDATION_FLAG_TEXT, "userinfo_key");
    if (!result.valid) {
        return result;
    }

    // Validate value based on key
    if (Q_stricmp(key, "name") == 0) {
        // Player name validation
        result = InputValidation_ValidateString(value,
            VALIDATION_FLAG_TEXT | VALIDATION_FLAG_SANITIZE | VALIDATION_FLAG_LENGTH_LIMIT, "player_name");

        if (result.valid) {
            // Check for reserved names
            const char *reserved[] = {"console", "server", "admin", NULL};
            for (int i = 0; reserved[i]; i++) {
                if (Q_stricmp(value, reserved[i]) == 0) {
                    result.valid = qfalse;
                    result.error = VALIDATION_ERROR_RESERVED_NAME;
                    Q_strncpyz(result.message, "Reserved name not allowed", sizeof(result.message));
                    break;
                }
            }

            // Check length limits
            if (strlen(value) < 1 || strlen(value) > 32) {
                result.valid = qfalse;
                result.error = VALIDATION_ERROR_INVALID_LENGTH;
                Q_strncpyz(result.message, "Name must be 1-32 characters", sizeof(result.message));
            }
        }

    } else if (Q_stricmp(key, "model") == 0 || Q_stricmp(key, "headmodel") == 0) {
        // Model validation
        result = InputValidation_ValidateString(value, VALIDATION_FLAG_PATH, "model_path");

    } else if (Q_stricmp(key, "rate") == 0 || Q_stricmp(key, "snaps") == 0) {
        // Numeric validation
        char *endptr;
        long num = strtol(value, &endptr, 10);
        if (*endptr != '\0' || num < 0 || num > 100000) {
            result.valid = qfalse;
            result.error = VALIDATION_ERROR_INVALID_NUMBER;
            Q_strncpyz(result.message, "Invalid numeric value", sizeof(result.message));
        }

    } else {
        // Generic string validation
        result = InputValidation_ValidateString(value, VALIDATION_FLAG_TEXT | VALIDATION_FLAG_SANITIZE, "userinfo_value");
    }

    return result;
}

/*
===============
InputValidation_GenerateReport
===============
*/
void InputValidation_GenerateReport(void) {
    Com_Printf("=== INPUT VALIDATION REPORT ===\n");
    Com_Printf("Total validations: %d\n", validation_stats.total_validations);
    Com_Printf("Passed validations: %d\n", validation_stats.passed_validations);
    Com_Printf("Failed validations: %d\n", validation_stats.failed_validations);
    Com_Printf("Rate limit hits: %d\n", validation_stats.rate_limit_hits);
    Com_Printf("Suspicious inputs detected: %d\n", validation_stats.suspicious_inputs);

    if (validation_stats.total_validations > 0) {
        float pass_rate = (validation_stats.passed_validations * 100.0f) / validation_stats.total_validations;
        Com_Printf("Validation pass rate: %.1f%%\n", pass_rate);
    }

    Com_Printf("================================\n");
}

/*
===============
InputValidation_GetStats
===============
*/
const input_validation_stats_t *InputValidation_GetStats(void) {
    return &validation_stats;
}

/*
===============
Convenience Functions
===============
*/

// Quick validation for common cases
qboolean InputValidation_IsValidPlayerName(const char *name) {
    validation_result_t result = InputValidation_ValidateUserInfo("name", name);
    return result.valid;
}

qboolean InputValidation_IsValidCommand(const char *command) {
    validation_result_t result = InputValidation_ValidateString(command, VALIDATION_FLAG_COMMAND, "command");
    return result.valid;
}

qboolean InputValidation_IsValidPath(const char *path) {
    return InputValidation_ValidatePath(path);
}
