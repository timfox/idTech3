/*
===========================================================================
q_input_validation.h - Enhanced Input Validation and Sanitization
===========================================================================
*/

#ifndef __Q_INPUT_VALIDATION_H__
#define __Q_INPUT_VALIDATION_H__

#include "q_shared.h"

// Validation flags
typedef enum {
    VALIDATION_FLAG_NONE = 0,
    VALIDATION_FLAG_TEXT = (1 << 0),          // General text validation
    VALIDATION_FLAG_COMMAND = (1 << 1),       // Command validation
    VALIDATION_FLAG_PATH = (1 << 2),          // Path validation
    VALIDATION_FLAG_UNICODE = (1 << 3),       // Unicode filtering
    VALIDATION_FLAG_CHECK_NULL = (1 << 4),    // Null byte checking
    VALIDATION_FLAG_SANITIZE = (1 << 5),      // Apply sanitization
    VALIDATION_FLAG_SANITIZE_CONTROL = (1 << 6), // Remove control chars
    VALIDATION_FLAG_SANITIZE_QUOTES = (1 << 7),  // Remove quotes
    VALIDATION_FLAG_SANITIZE_PATH = (1 << 8),    // Sanitize paths
    VALIDATION_FLAG_LENGTH_LIMIT = (1 << 9),   // Check length limits
    VALIDATION_FLAG_SQL = (1 << 10)            // SQL injection prevention
} validation_flags_t;

// Validation error codes
typedef enum {
    VALIDATION_ERROR_NONE,
    VALIDATION_ERROR_NULL_INPUT,
    VALIDATION_ERROR_TOO_LONG,
    VALIDATION_ERROR_NULL_BYTE,
    VALIDATION_ERROR_INVALID_PATH,
    VALIDATION_ERROR_SUSPICIOUS_CHARS,
    VALIDATION_ERROR_SQL_INJECTION,
    VALIDATION_ERROR_INVALID_NUMBER,
    VALIDATION_ERROR_INVALID_LENGTH,
    VALIDATION_ERROR_RESERVED_NAME,
    VALIDATION_ERROR_RATE_LIMITED
} validation_error_t;

// Validation result
typedef struct {
    qboolean valid;
    validation_error_t error;
    char message[256];
} validation_result_t;

// Rate limiting result
typedef struct {
    qboolean allowed;
    int retry_after;  // milliseconds
} rate_limit_result_t;

// Input validation statistics
typedef struct {
    int total_validations;
    int passed_validations;
    int failed_validations;
    int rate_limit_hits;
    int suspicious_inputs;
} input_validation_stats_t;

// Input validation state
typedef struct {
    qboolean initialized;
} input_validation_state_t;

// Function declarations
void InputValidation_Init(void);
void InputValidation_Shutdown(void);

// Core validation functions
validation_result_t InputValidation_ValidateString(const char *input, validation_flags_t flags,
                                                 const char *context);
void InputValidation_SanitizeString(char *output, size_t output_size, const char *input,
                                  validation_flags_t flags);

// Command and user info validation
validation_result_t InputValidation_ValidateCommand(int client_id, const char *command,
                                                  const char **args, int arg_count);
validation_result_t InputValidation_ValidateUserInfo(const char *key, const char *value);

// Rate limiting
rate_limit_result_t InputValidation_CheckRateLimit(int client_id, const char *command);

// Statistics and reporting
void InputValidation_GenerateReport(void);
const input_validation_stats_t *InputValidation_GetStats(void);

// Convenience functions
qboolean InputValidation_IsValidPlayerName(const char *name);
qboolean InputValidation_IsValidCommand(const char *command);
qboolean InputValidation_IsValidPath(const char *path);

#endif // __Q_INPUT_VALIDATION_H__
