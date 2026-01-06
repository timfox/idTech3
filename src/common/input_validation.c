/*
=============================================================================
User Input Validation Implementation

Comprehensive input sanitization and type validation for user-controlled data
=============================================================================
*/

#include "input_validation.h"
#include "qcommon.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

// Dangerous character patterns
static const char *injection_patterns[] = {
    "../", "..\\", "<script", "</script>", "javascript:", "vbscript:",
    "onload=", "onerror=", "onclick=", "onmouseover=", "eval(",
    "document.", "window.", "location.", "cookie", "localStorage",
    "sessionStorage", "XMLHttpRequest", "fetch(", "import(",
    "require(", "exec(", "system(", "popen(", "shell_exec(",
    NULL
};

static const char *scripting_patterns[] = {
    "lua:", "python:", "perl:", "ruby:", "php:", "bash:",
    "sh:", "cmd:", "powershell:", "batch:", NULL
};

// Control characters to filter out
static const char control_chars[] = {
    '\x00', '\x01', '\x02', '\x03', '\x04', '\x05', '\x06', '\x07',
    '\x08', '\x0B', '\x0C', '\x0E', '\x0F', '\x10', '\x11', '\x12',
    '\x13', '\x14', '\x15', '\x16', '\x17', '\x18', '\x19', '\x1A',
    '\x1B', '\x1C', '\x1D', '\x1E', '\x1F', '\x7F', '\x80', '\x81',
    '\x82', '\x83', '\x84', '\x85', '\x86', '\x87', '\x88', '\x89',
    '\x8A', '\x8B', '\x8C', '\x8D', '\x8E', '\x8F', '\x90', '\x91',
    '\x92', '\x93', '\x94', '\x95', '\x96', '\x97', '\x98', '\x99',
    '\x9A', '\x9B', '\x9C', '\x9D', '\x9E', '\x9F'
};

/*
===============
Input_ValidateString

General string validation with type-specific checks
===============
*/
qboolean Input_ValidateString(const char *input, int max_length, input_type_t type) {
    if (!input) {
        return qfalse;
    }

    int length = strlen(input);
    if (length >= max_length) {
        return qfalse;
    }

    // Check for null bytes
    if (memchr(input, '\0', length) != NULL) {
        return qfalse;
    }

    // Type-specific validation
    switch (type) {
        case INPUT_TYPE_CHAT:
            return Input_ValidateChatMessage(input, length) == INPUT_VALID;
        case INPUT_TYPE_COMMAND:
            return Input_ValidateConsoleCommand(input, length) == INPUT_VALID;
        case INPUT_TYPE_PLAYER_NAME:
            return Input_ValidatePlayerName(input, length) == INPUT_VALID;
        case INPUT_TYPE_SERVER_NAME:
            return Input_ValidateServerName(input, length) == INPUT_VALID;
        case INPUT_TYPE_MAP_NAME:
            return Input_ValidateMapName(input, length) == INPUT_VALID;
        case INPUT_TYPE_CVAR_VALUE:
            return Input_ValidateCvarValue(input, length) == INPUT_VALID;
        default:
            break;
    }

    // Basic validation for all types
    for (int i = 0; i < length; i++) {
        if (!Input_IsPrintable(input[i])) {
            return qfalse;
        }
    }

    return qtrue;
}

/*
===============
Input_ValidateChatMessage

Validate chat messages
===============
*/
input_validation_result_t Input_ValidateChatMessage(const char *message, int length) {
    if (!message || length <= 0) {
        return INPUT_NULL_INPUT;
    }

    if (length > MAX_SAY_TEXT) {
        return INPUT_INVALID_LENGTH;
    }

    // Check for injection attempts
    if (Input_DetectInjection(message)) {
        return INPUT_INJECTION_ATTEMPT;
    }

    // Check for scripting attempts
    if (Input_DetectScripting(message)) {
        return INPUT_INJECTION_ATTEMPT;
    }

    // Allow color codes (^0-^9) but limit their frequency
    int color_code_count = 0;
    for (int i = 0; i < length - 1; i++) {
        if (message[i] == '^' && message[i + 1] >= '0' && message[i + 1] <= '9') {
            color_code_count++;
            i++; // Skip the color code number
        }
    }

    if (color_code_count > 10) { // Arbitrary limit to prevent spam
        return INPUT_INVALID_CHARACTERS;
    }

    return INPUT_VALID;
}

/*
===============
Input_ValidateConsoleCommand

Validate console commands
===============
*/
input_validation_result_t Input_ValidateConsoleCommand(const char *command, int length) {
    if (!command || length <= 0) {
        return INPUT_NULL_INPUT;
    }

    if (length > 256) { // Reasonable command length limit
        return INPUT_INVALID_LENGTH;
    }

    // Check for dangerous command patterns
    if (strstr(command, ";") != NULL || strstr(command, "|") != NULL ||
        strstr(command, "&") != NULL || strstr(command, "`") != NULL) {
        return INPUT_INJECTION_ATTEMPT;
    }

    // Check for path traversal
    if (Input_DetectPathTraversal(command)) {
        return INPUT_INJECTION_ATTEMPT;
    }

    // Commands should start with a letter or /
    if (length > 0 && command[0] != '/' && !isalpha(command[0])) {
        return INPUT_FORMAT_ERROR;
    }

    return INPUT_VALID;
}

/*
===============
Input_ValidatePlayerName

Validate player names
===============
*/
input_validation_result_t Input_ValidatePlayerName(const char *name, int length) {
    if (!name || length <= 0) {
        return INPUT_NULL_INPUT;
    }

    if (length > 32) { // Standard name length limit
        return INPUT_INVALID_LENGTH;
    }

    // Check for injection attempts
    if (Input_DetectInjection(name)) {
        return INPUT_INJECTION_ATTEMPT;
    }

    // Allow alphanumeric, spaces, and some special characters
    for (int i = 0; i < length; i++) {
        char c = name[i];
        if (!isalnum(c) && c != ' ' && c != '_' && c != '-' && c != '[' && c != ']' &&
            c != '(' && c != ')' && c != '{' && c != '}' && c != '^') {
            return INPUT_INVALID_CHARACTERS;
        }
    }

    // Name should not be empty or just spaces
    qboolean has_non_space = qfalse;
    for (int i = 0; i < length; i++) {
        if (name[i] != ' ') {
            has_non_space = qtrue;
            break;
        }
    }

    if (!has_non_space) {
        return INPUT_FORMAT_ERROR;
    }

    return INPUT_VALID;
}

/*
===============
Input_ValidateServerName

Validate server names
===============
*/
input_validation_result_t Input_ValidateServerName(const char *name, int length) {
    if (!name || length <= 0) {
        return INPUT_NULL_INPUT;
    }

    if (length > 64) { // Server name length limit
        return INPUT_INVALID_LENGTH;
    }

    // Similar to player names but more restrictive
    for (int i = 0; i < length; i++) {
        char c = name[i];
        if (!isalnum(c) && c != ' ' && c != '_' && c != '-' && c != '.' && c != ':') {
            return INPUT_INVALID_CHARACTERS;
        }
    }

    return INPUT_VALID;
}

/*
===============
Input_ValidateMapName

Validate map names
===============
*/
input_validation_result_t Input_ValidateMapName(const char *name, int length) {
    if (!name || length <= 0) {
        return INPUT_NULL_INPUT;
    }

    if (length > 64) {
        return INPUT_INVALID_LENGTH;
    }

    // Map names should be alphanumeric with underscores and dashes
    for (int i = 0; i < length; i++) {
        char c = name[i];
        if (!isalnum(c) && c != '_' && c != '-') {
            return INPUT_INVALID_CHARACTERS;
        }
    }

    // Check for path traversal
    if (Input_DetectPathTraversal(name)) {
        return INPUT_INJECTION_ATTEMPT;
    }

    return INPUT_VALID;
}

/*
===============
Input_ValidateCvarValue

Validate CVar values
===============
*/
input_validation_result_t Input_ValidateCvarValue(const char *value, int length) {
    if (!value || length <= 0) {
        return INPUT_NULL_INPUT;
    }

    if (length > 256) {
        return INPUT_INVALID_LENGTH;
    }

    // Check for injection attempts
    if (Input_DetectInjection(value)) {
        return INPUT_INJECTION_ATTEMPT;
    }

    // CVar values can be more permissive, but still check for obvious issues
    if (strstr(value, ";") != NULL || strstr(value, "|") != NULL) {
        return INPUT_INJECTION_ATTEMPT;
    }

    return INPUT_VALID;
}

/*
===============
Input_ValidateNumeric

Validate and parse numeric input
===============
*/
qboolean Input_ValidateNumeric(const char *input, float *result, float min_val, float max_val) {
    if (!input || !result) {
        return qfalse;
    }

    char *endptr;
    *result = strtof(input, &endptr);

    // Check if conversion succeeded
    if (endptr == input || *endptr != '\0') {
        return qfalse;
    }

    // Check range
    if (*result < min_val || *result > max_val) {
        return qfalse;
    }

    // Check for infinity or NaN
    if (!isfinite(*result)) {
        return qfalse;
    }

    return qtrue;
}

/*
===============
Input_ValidateInteger

Validate and parse integer input
===============
*/
qboolean Input_ValidateInteger(const char *input, int *result, int min_val, int max_val) {
    if (!input || !result) {
        return qfalse;
    }

    char *endptr;
    *result = strtol(input, &endptr, 10);

    // Check if conversion succeeded
    if (endptr == input || *endptr != '\0') {
        return qfalse;
    }

    // Check range
    if (*result < min_val || *result > max_val) {
        return qfalse;
    }

    return qtrue;
}

/*
===============
Input_ValidateBoolean

Validate and parse boolean input
===============
*/
qboolean Input_ValidateBoolean(const char *input, qboolean *result) {
    if (!input || !result) {
        return qfalse;
    }

    // Check various boolean representations
    if (Q_stricmp(input, "1") == 0 || Q_stricmp(input, "true") == 0 ||
        Q_stricmp(input, "yes") == 0 || Q_stricmp(input, "on") == 0) {
        *result = qtrue;
        return qtrue;
    }

    if (Q_stricmp(input, "0") == 0 || Q_stricmp(input, "false") == 0 ||
        Q_stricmp(input, "no") == 0 || Q_stricmp(input, "off") == 0) {
        *result = qfalse;
        return qtrue;
    }

    return qfalse;
}

/*
===============
Input_ValidateVector

Validate and parse vector input (x y z)
===============
*/
qboolean Input_ValidateVector(const char *input, vec3_t result) {
    if (!input || !result) {
        return qfalse;
    }

    float x, y, z;
    if (sscanf(input, "%f %f %f", &x, &y, &z) != 3) {
        return qfalse;
    }

    // Validate each component
    if (!isfinite(x) || !isfinite(y) || !isfinite(z)) {
        return qfalse;
    }

    // Reasonable bounds check (adjust as needed)
    const float MAX_VECTOR_COMPONENT = 1000000.0f;
    if (fabs(x) > MAX_VECTOR_COMPONENT || fabs(y) > MAX_VECTOR_COMPONENT || fabs(z) > MAX_VECTOR_COMPONENT) {
        return qfalse;
    }

    result[0] = x;
    result[1] = y;
    result[2] = z;

    return qtrue;
}

/*
===============
Input_ValidateColor

Validate and parse color input (r g b a)
===============
*/
qboolean Input_ValidateColor(const char *input, vec4_t result) {
    if (!input || !result) {
        return qfalse;
    }

    float r, g, b, a = 1.0f; // Alpha defaults to 1.0
    int parsed = sscanf(input, "%f %f %f %f", &r, &g, &b, &a);

    if (parsed < 3) {
        return qfalse;
    }

    // Validate color components (0.0 to 1.0)
    if (r < 0.0f || r > 1.0f || g < 0.0f || g > 1.0f ||
        b < 0.0f || b > 1.0f || a < 0.0f || a > 1.0f) {
        return qfalse;
    }

    if (!isfinite(r) || !isfinite(g) || !isfinite(b) || !isfinite(a)) {
        return qfalse;
    }

    result[0] = r;
    result[1] = g;
    result[2] = b;
    result[3] = a;

    return qtrue;
}

/*
===============
Input_SanitizeString

Sanitize a string by removing dangerous characters
===============
*/
void Input_SanitizeString(char *output, int output_size, const char *input) {
    if (!output || output_size <= 0 || !input) {
        return;
    }

    int output_pos = 0;
    int input_len = strlen(input);

    for (int i = 0; i < input_len && output_pos < output_size - 1; i++) {
        char c = input[i];

        // Skip control characters
        if (Input_IsControlCharacter(c)) {
            continue;
        }

        // Skip potentially dangerous characters
        if (c == '<' || c == '>' || c == '"' || c == '\'') {
            continue;
        }

        output[output_pos++] = c;
    }

    output[output_pos] = '\0';
}

/*
===============
Input_SanitizeChatMessage

Sanitize chat messages specifically
===============
*/
void Input_SanitizeChatMessage(char *output, int output_size, const char *input) {
    Input_SanitizeString(output, output_size, input);

    // Additional chat-specific sanitization
    Input_RemoveColorCodes(output);

    // Limit repeated characters
    int len = strlen(output);
    int repeat_count = 1;
    char last_char = '\0';

    for (int i = 0; i < len; i++) {
        if (output[i] == last_char && isalnum(last_char)) {
            repeat_count++;
            if (repeat_count > 3) { // Allow max 3 repeats
                // Replace with single character
                memmove(&output[i], &output[i + 1], len - i);
                len--;
                i--;
            }
        } else {
            repeat_count = 1;
        }
        last_char = output[i];
    }
    output[len] = '\0';
}

/*
===============
Input_SanitizePlayerName

Sanitize player names
===============
*/
void Input_SanitizePlayerName(char *output, int output_size, const char *input) {
    if (!output || output_size <= 0 || !input) {
        return;
    }

    int output_pos = 0;
    int input_len = strlen(input);

    for (int i = 0; i < input_len && output_pos < output_size - 1; i++) {
        char c = input[i];

        // Allow alphanumeric, spaces, and safe special characters
        if (isalnum(c) || c == ' ' || c == '_' || c == '-' || c == '[' || c == ']' ||
            c == '(' || c == ')' || c == '{' || c == '}') {
            output[output_pos++] = c;
        }
    }

    output[output_pos] = '\0';

    // Trim leading/trailing spaces
    char *start = output;
    while (*start == ' ') start++;

    char *end = output + strlen(output) - 1;
    while (end > start && *end == ' ') end--;

    if (start != output) {
        memmove(output, start, end - start + 1);
    }
    output[end - start + 1] = '\0';
}

/*
===============
Input_RemoveControlCharacters

Remove control characters from string
===============
*/
void Input_RemoveControlCharacters(char *str) {
    if (!str) return;

    char *src = str;
    char *dst = str;

    while (*src) {
        if (!Input_IsControlCharacter(*src)) {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';
}

/*
===============
Input_RemoveColorCodes

Remove Quake-style color codes (^0-^9)
===============
*/
void Input_RemoveColorCodes(char *str) {
    if (!str) return;

    char *src = str;
    char *dst = str;

    while (*src) {
        if (*src == '^' && src[1] >= '0' && src[1] <= '9') {
            src += 2; // Skip the color code
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

/*
===============
Input_DetectInjection

Detect potential code injection attempts
===============
*/
qboolean Input_DetectInjection(const char *input) {
    if (!input) return qfalse;

    for (int i = 0; injection_patterns[i] != NULL; i++) {
        if (strstr(input, injection_patterns[i]) != NULL) {
            return qtrue;
        }
    }

    return qfalse;
}

/*
===============
Input_DetectScripting

Detect scripting attempts
===============
*/
qboolean Input_DetectScripting(const char *input) {
    if (!input) return qfalse;

    for (int i = 0; scripting_patterns[i] != NULL; i++) {
        if (strstr(input, scripting_patterns[i]) != NULL) {
            return qtrue;
        }
    }

    return qfalse;
}

/*
===============
Input_DetectPathTraversal

Detect path traversal attempts
===============
*/
qboolean Input_DetectPathTraversal(const char *input) {
    if (!input) return qfalse;

    // Check for directory traversal patterns
    if (strstr(input, "../") != NULL || strstr(input, "..\\") != NULL) {
        return qtrue;
    }

    // Check for absolute paths
    if (input[0] == '/' || input[0] == '\\' || strstr(input, ":\\") != NULL) {
        return qtrue;
    }

    return qfalse;
}

/*
===============
Input_ValidateEncoding

Basic UTF-8 validation
===============
*/
qboolean Input_ValidateEncoding(const char *input, int length) {
    if (!input || length <= 0) return qtrue; // Empty input is valid

    int i = 0;
    while (i < length) {
        unsigned char c = (unsigned char)input[i];

        if (c <= 0x7F) {
            // ASCII character
            i++;
        } else if ((c & 0xE0) == 0xC0) {
            // 2-byte sequence
            if (i + 1 >= length) return qfalse;
            if ((input[i + 1] & 0xC0) != 0x80) return qfalse;
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            // 3-byte sequence
            if (i + 2 >= length) return qfalse;
            if ((input[i + 1] & 0xC0) != 0x80) return qfalse;
            if ((input[i + 2] & 0xC0) != 0x80) return qfalse;
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            // 4-byte sequence
            if (i + 3 >= length) return qfalse;
            if ((input[i + 1] & 0xC0) != 0x80) return qfalse;
            if ((input[i + 2] & 0xC0) != 0x80) return qfalse;
            if ((input[i + 3] & 0xC0) != 0x80) return qfalse;
            i += 4;
        } else {
            // Invalid UTF-8
            return qfalse;
        }
    }

    return qtrue;
}

/*
===============
Input_CheckRateLimit

Check input rate limiting
===============
*/
qboolean Input_CheckRateLimit(input_validation_context_t *ctx, int current_time) {
    if (!ctx) return qtrue;

    // Check if we're in a new second
    if (current_time - ctx->last_input_time >= 1000) {
        ctx->input_count_this_second = 0;
        ctx->last_input_time = current_time;
    }

    ctx->input_count_this_second++;

    if (ctx->input_count_this_second > (int)(ctx->config.input_rate_limit)) {
        atomic_fetch_add(&ctx->stats.rate_limit_hits, 1);
        return qfalse;
    }

    return qtrue;
}

/*
===============
Input_BoundsCheckString

Bounds checking for strings
===============
*/
qboolean Input_BoundsCheckString(const char *str, int max_length) {
    if (!str) return qfalse;

    int len = strlen(str);
    return len < max_length;
}

/*
===============
Input_BoundsCheckArray

Bounds checking for arrays
===============
*/
qboolean Input_BoundsCheckArray(const void *array, int element_count, int max_elements) {
    if (!array || element_count < 0) return qfalse;
    return element_count <= max_elements;
}

/*
===============
Input_ValidationResultToString

Convert result to string
===============
*/
const char *Input_ValidationResultToString(input_validation_result_t result) {
    switch (result) {
        case INPUT_VALID: return "Valid";
        case INPUT_INVALID_LENGTH: return "Invalid Length";
        case INPUT_INVALID_CHARACTERS: return "Invalid Characters";
        case INPUT_BUFFER_OVERFLOW: return "Buffer Overflow";
        case INPUT_INJECTION_ATTEMPT: return "Injection Attempt";
        case INPUT_TYPE_MISMATCH: return "Type Mismatch";
        case INPUT_RANGE_ERROR: return "Range Error";
        case INPUT_FORMAT_ERROR: return "Format Error";
        case INPUT_NULL_INPUT: return "Null Input";
        case INPUT_ENCODING_ERROR: return "Encoding Error";
        case INPUT_TOO_FREQUENT: return "Too Frequent";
        default: return "Unknown Error";
    }
}

/*
===============
Input_IsCriticalValidationError

Check if error is critical
===============
*/
qboolean Input_IsCriticalValidationError(input_validation_result_t result) {
    switch (result) {
        case INPUT_BUFFER_OVERFLOW:
        case INPUT_INJECTION_ATTEMPT:
        case INPUT_INVALID_LENGTH:
            return qtrue;
        default:
            return qfalse;
    }
}

/*
===============
Input_LogValidationError

Log validation errors
===============
*/
void Input_LogValidationError(input_validation_result_t result, const char *input_type, const char *input) {
    if (!input_type || !input) return;

    const char *result_str = Input_ValidationResultToString(result);
    Com_Printf("Input validation failed: %s in %s input\n", result_str, input_type);
}

/*
===============
Input_ValidationInit

Initialize validation context
===============
*/
void Input_ValidationInit(input_validation_context_t *ctx) {
    if (!ctx) return;

    memset(ctx, 0, sizeof(*ctx));

    // Set default configuration
    ctx->config.enable_validation = qtrue;
    ctx->config.strict_mode = qfalse;
    ctx->config.allow_unicode = qtrue;
    ctx->config.filter_injection = qtrue;
    ctx->config.max_input_length = 1024;
    ctx->config.max_name_length = 32;
    ctx->config.max_chat_length = 150;
    ctx->config.max_command_length = 256;
    ctx->config.input_rate_limit = 10.0f; // 10 inputs per second

    // Initialize stats
    atomic_init(&ctx->stats.total_inputs_validated, 0);
    atomic_init(&ctx->stats.inputs_rejected, 0);
    atomic_init(&ctx->stats.injection_attempts, 0);
    atomic_init(&ctx->stats.encoding_errors, 0);
    atomic_init(&ctx->stats.rate_limit_hits, 0);
    atomic_init(&ctx->stats.length_violations, 0);
    atomic_init(&ctx->stats.character_violations, 0);
}

/*
===============
Input_ValidationShutdown

Shutdown validation context
===============
*/
void Input_ValidationShutdown(input_validation_context_t *ctx) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
}

/*
===============
Input_ValidationResetStats

Reset validation statistics
===============
*/
void Input_ValidationResetStats(input_validation_context_t *ctx) {
    if (!ctx) return;

    atomic_store(&ctx->stats.total_inputs_validated, 0);
    atomic_store(&ctx->stats.inputs_rejected, 0);
    atomic_store(&ctx->stats.injection_attempts, 0);
    atomic_store(&ctx->stats.encoding_errors, 0);
    atomic_store(&ctx->stats.rate_limit_hits, 0);
    atomic_store(&ctx->stats.length_violations, 0);
    atomic_store(&ctx->stats.character_violations, 0);
}

/*
===============
Input_ValidationSetConfig

Set validation configuration
===============
*/
void Input_ValidationSetConfig(input_validation_context_t *ctx, const input_validation_config_t *config) {
    if (!ctx || !config) return;
    ctx->config = *config;
}

/*
===============
Input_ValidationGetStats

Get validation statistics
===============
*/
void Input_ValidationGetStats(const input_validation_context_t *ctx, input_validation_stats_t *stats) {
    if (!ctx || !stats) return;

    stats->total_inputs_validated = atomic_load(&ctx->stats.total_inputs_validated);
    stats->inputs_rejected = atomic_load(&ctx->stats.inputs_rejected);
    stats->injection_attempts = atomic_load(&ctx->stats.injection_attempts);
    stats->encoding_errors = atomic_load(&ctx->stats.encoding_errors);
    stats->rate_limit_hits = atomic_load(&ctx->stats.rate_limit_hits);
    stats->length_violations = atomic_load(&ctx->stats.length_violations);
    stats->character_violations = atomic_load(&ctx->stats.character_violations);
}

/*
===============
Input_IsPrintable

Check if character is printable
===============
*/
qboolean Input_IsPrintable(char c) {
    return (c >= 32 && c <= 126) || c == '\t' || c == '\n' || c == '\r';
}

/*
===============
Input_IsAlphanumeric

Check if character is alphanumeric
===============
*/
qboolean Input_IsAlphanumeric(char c) {
    return isalnum((unsigned char)c);
}

/*
===============
Input_IsSafeForFilename

Check if character is safe for filenames
===============
*/
qboolean Input_IsSafeForFilename(char c) {
    return isalnum(c) || c == '_' || c == '-' || c == '.';
}

/*
===============
Input_IsControlCharacter

Check if character is a control character
===============
*/
qboolean Input_IsControlCharacter(char c) {
    for (size_t i = 0; i < sizeof(control_chars); i++) {
        if (c == control_chars[i]) {
            return qtrue;
        }
    }
    return qfalse;
}