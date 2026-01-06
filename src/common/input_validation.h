/*
=============================================================================
User Input Validation Header

Comprehensive input sanitization and type validation for user-controlled data
=============================================================================
*/

#ifndef __INPUT_VALIDATION_H__
#define __INPUT_VALIDATION_H__

#include "q_shared.h"

// Input validation result codes
typedef enum {
    INPUT_VALID = 0,
    INPUT_INVALID_LENGTH,
    INPUT_INVALID_CHARACTERS,
    INPUT_BUFFER_OVERFLOW,
    INPUT_INJECTION_ATTEMPT,
    INPUT_TYPE_MISMATCH,
    INPUT_RANGE_ERROR,
    INPUT_FORMAT_ERROR,
    INPUT_NULL_INPUT,
    INPUT_ENCODING_ERROR,
    INPUT_TOO_FREQUENT
} input_validation_result_t;

// Input validation configuration
typedef struct {
    qboolean enable_validation;
    qboolean strict_mode;
    qboolean allow_unicode;
    qboolean filter_injection;
    int max_input_length;
    int max_name_length;
    int max_chat_length;
    int max_command_length;
    float input_rate_limit;  // inputs per second
} input_validation_config_t;

// Input validation statistics
typedef struct {
    atomic_int_t total_inputs_validated;
    atomic_int_t inputs_rejected;
    atomic_int_t injection_attempts;
    atomic_int_t encoding_errors;
    atomic_int_t rate_limit_hits;
    atomic_int_t length_violations;
    atomic_int_t character_violations;
} input_validation_stats_t;

// Input validation context
typedef struct {
    input_validation_stats_t stats;
    input_validation_config_t config;

    // Rate limiting
    int last_input_time;
    int input_count_this_second;
} input_validation_context_t;

// Input types
typedef enum {
    INPUT_TYPE_CHAT,
    INPUT_TYPE_COMMAND,
    INPUT_TYPE_PLAYER_NAME,
    INPUT_TYPE_SERVER_NAME,
    INPUT_TYPE_MAP_NAME,
    INPUT_TYPE_CVAR_VALUE,
    INPUT_TYPE_VECTOR,
    INPUT_TYPE_COLOR,
    INPUT_TYPE_NUMERIC,
    INPUT_TYPE_BOOLEAN,
    INPUT_TYPE_STRING
} input_type_t;

// Core validation functions
qboolean Input_ValidateString(const char *input, int max_length, input_type_t type);
input_validation_result_t Input_ValidateChatMessage(const char *message, int length);
input_validation_result_t Input_ValidateConsoleCommand(const char *command, int length);
input_validation_result_t Input_ValidatePlayerName(const char *name, int length);
input_validation_result_t Input_ValidateServerName(const char *name, int length);
input_validation_result_t Input_ValidateMapName(const char *name, int length);
input_validation_result_t Input_ValidateCvarValue(const char *value, int length);

// Type-specific validation
qboolean Input_ValidateNumeric(const char *input, float *result, float min_val, float max_val);
qboolean Input_ValidateInteger(const char *input, int *result, int min_val, int max_val);
qboolean Input_ValidateBoolean(const char *input, qboolean *result);
qboolean Input_ValidateVector(const char *input, vec3_t result);
qboolean Input_ValidateColor(const char *input, vec4_t result);

// Sanitization functions
void Input_SanitizeString(char *output, int output_size, const char *input);
void Input_SanitizeChatMessage(char *output, int output_size, const char *input);
void Input_SanitizePlayerName(char *output, int output_size, const char *input);
void Input_RemoveControlCharacters(char *str);
void Input_RemoveColorCodes(char *str);

// Security validation
qboolean Input_DetectInjection(const char *input);
qboolean Input_DetectScripting(const char *input);
qboolean Input_DetectPathTraversal(const char *input);
qboolean Input_ValidateEncoding(const char *input, int length);

// Rate limiting
qboolean Input_CheckRateLimit(input_validation_context_t *ctx, int current_time);

// Bounds and safety checks
qboolean Input_BoundsCheckString(const char *str, int max_length);
qboolean Input_BoundsCheckArray(const void *array, int element_count, int max_elements);

// Utility functions
const char *Input_ValidationResultToString(input_validation_result_t result);
qboolean Input_IsCriticalValidationError(input_validation_result_t result);
void Input_LogValidationError(input_validation_result_t result, const char *input_type, const char *input);

// Context management
void Input_ValidationInit(input_validation_context_t *ctx);
void Input_ValidationShutdown(input_validation_context_t *ctx);
void Input_ValidationResetStats(input_validation_context_t *ctx);
void Input_ValidationSetConfig(input_validation_context_t *ctx, const input_validation_config_t *config);
void Input_ValidationGetStats(const input_validation_context_t *ctx, input_validation_stats_t *stats);

// Character classification
qboolean Input_IsPrintable(char c);
qboolean Input_IsAlphanumeric(char c);
qboolean Input_IsSafeForFilename(char c);
qboolean Input_IsControlCharacter(char c);

#endif // __INPUT_VALIDATION_H__