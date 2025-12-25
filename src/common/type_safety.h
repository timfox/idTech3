/*
=============================================================================
Type Safety Framework

Comprehensive compile-time and runtime type checking and validation.
=============================================================================
*/

#ifndef __TYPE_SAFETY_H__
#define __TYPE_SAFETY_H__

#include "q_shared.h"

// Compile-time type safety checks
#define STATIC_ASSERT_TYPE_SIZE(type, expected_size) \
    static_assert(sizeof(type) == (expected_size), #type " must be " #expected_size " bytes")

#define STATIC_ASSERT_TYPE_ALIGNMENT(type, expected_alignment) \
    static_assert(alignof(type) == (expected_alignment), #type " must be aligned to " #expected_alignment " bytes")

// Type-safe enum validation
#define VALIDATE_ENUM(value, max_value) \
    (((value) >= 0 && (value) < (max_value)) ? (value) : -1)

// Type-safe array bounds checking
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define ARRAY_BOUNDS_CHECK(arr, index) \
    ((index) >= 0 && (index) < ARRAY_SIZE(arr))

// Type-safe pointer validation
#define POINTER_IS_VALID(ptr) ((ptr) != NULL)

#define POINTER_ARRAY_IS_VALID(arr, count) \
    ((arr) != NULL && (count) > 0 && (count) < 1000000) // Reasonable upper bound

// Type-safe string operations
#define STRING_IS_VALID(str) ((str) != NULL && (str)[0] != '\0')

#define STRING_LENGTH_IS_VALID(str, max_len) \
    (STRING_IS_VALID(str) && strlen(str) < (max_len))

// Type-safe numeric validation
#define INT_IS_VALID(value, min_val, max_val) \
    ((value) >= (min_val) && (value) <= (max_val))

#define FLOAT_IS_VALID(value, min_val, max_val) \
    ((value) >= (min_val) && (value) <= (max_val) && !isnan(value) && !isinf(value))

// Type-safe memory size validation
#define MEMORY_SIZE_IS_VALID(size) \
    ((size) > 0 && (size) < (1024 * 1024 * 1024)) // Max 1GB

#define MEMORY_ALIGNMENT_IS_VALID(alignment) \
    ((alignment) > 0 && ((alignment) & (alignment - 1)) == 0) // Power of 2

// Type-safe handle validation
#define HANDLE_IS_VALID(handle) ((handle) != NULL && (handle) != INVALID_HANDLE_VALUE)

// Type-safe color validation
#define COLOR_COMPONENT_IS_VALID(component) \
    FLOAT_IS_VALID(component, 0.0f, 1.0f)

#define COLOR_RGBA_IS_VALID(r, g, b, a) \
    (COLOR_COMPONENT_IS_VALID(r) && COLOR_COMPONENT_IS_VALID(g) && \
     COLOR_COMPONENT_IS_VALID(b) && COLOR_COMPONENT_IS_VALID(a))

// Type-safe vector validation
#define VECTOR_COMPONENT_IS_VALID(component) \
    (FLOAT_IS_VALID(component, -1000000.0f, 1000000.0f) && !isnan(component))

#define VECTOR3_IS_VALID(v) \
    (VECTOR_COMPONENT_IS_VALID((v)[0]) && VECTOR_COMPONENT_IS_VALID((v)[1]) && VECTOR_COMPONENT_IS_VALID((v)[2]))

#define VECTOR4_IS_VALID(v) \
    (VECTOR3_IS_VALID(v) && VECTOR_COMPONENT_IS_VALID((v)[3]))

// Type-safe matrix validation
#define MATRIX4X4_IS_VALID(m) \
    (VECTOR4_IS_VALID(&(m)[0]) && VECTOR4_IS_VALID(&(m)[4]) && \
     VECTOR4_IS_VALID(&(m)[8]) && VECTOR4_IS_VALID(&(m)[12]))

// Runtime type validation functions
static inline qboolean TypeSafety_ValidateInt(int value, int min_val, int max_val, const char *context) {
    if (!INT_IS_VALID(value, min_val, max_val)) {
        Com_Printf("Type safety violation in %s: int value %d out of range [%d, %d]\n",
                  context, value, min_val, max_val);
        return qfalse;
    }
    return qtrue;
}

static inline qboolean TypeSafety_ValidateFloat(float value, float min_val, float max_val, const char *context) {
    if (!FLOAT_IS_VALID(value, min_val, max_val)) {
        Com_Printf("Type safety violation in %s: float value %f out of range [%f, %f]\n",
                  context, value, min_val, max_val);
        return qfalse;
    }
    return qtrue;
}

static inline qboolean TypeSafety_ValidatePointer(const void *ptr, const char *context) {
    if (!POINTER_IS_VALID(ptr)) {
        Com_Printf("Type safety violation in %s: NULL pointer\n", context);
        return qfalse;
    }
    return qtrue;
}

static inline qboolean TypeSafety_ValidateString(const char *str, size_t max_len, const char *context) {
    if (!STRING_IS_VALID(str)) {
        Com_Printf("Type safety violation in %s: invalid string\n", context);
        return qfalse;
    }
    if (strlen(str) >= max_len) {
        Com_Printf("Type safety violation in %s: string too long (%zu >= %zu)\n",
                  context, strlen(str), max_len);
        return qfalse;
    }
    return qtrue;
}

static inline qboolean TypeSafety_ValidateArrayBounds(const void *array, size_t element_size,
                                                     size_t element_count, size_t max_elements,
                                                     const char *context) {
    if (!POINTER_IS_VALID(array)) {
        Com_Printf("Type safety violation in %s: NULL array\n", context);
        return qfalse;
    }
    if (element_count > max_elements) {
        Com_Printf("Type safety violation in %s: array too large (%zu > %zu)\n",
                  context, element_count, max_elements);
        return qfalse;
    }
    if (element_size == 0) {
        Com_Printf("Type safety violation in %s: zero element size\n", context);
        return qfalse;
    }
    return qtrue;
}

static inline qboolean TypeSafety_ValidateMemorySize(size_t size, size_t max_size, const char *context) {
    if (!MEMORY_SIZE_IS_VALID(size)) {
        Com_Printf("Type safety violation in %s: invalid memory size %zu\n", context, size);
        return qfalse;
    }
    if (size > max_size) {
        Com_Printf("Type safety violation in %s: memory size too large (%zu > %zu)\n",
                  context, size, max_size);
        return qfalse;
    }
    return qtrue;
}

// Type-safe wrapper macros for common operations
#define SAFE_INT_CLAMP(value, min_val, max_val) \
    (TypeSafety_ValidateInt(value, min_val, max_val, __func__) ? \
     Q_ClampInt(value, min_val, max_val) : 0)

#define SAFE_FLOAT_CLAMP(value, min_val, max_val) \
    (TypeSafety_ValidateFloat(value, min_val, max_val, __func__) ? \
     Q_ClampFloat(value, min_val, max_val) : 0.0f)

#define SAFE_STRNCPY(dest, dest_size, src) \
    (TypeSafety_ValidateString(src, dest_size, __func__) ? \
     Q_strncpyz_safe(dest, dest_size, src) : qfalse)

#define SAFE_STRCAT(dest, dest_size, src) \
    (TypeSafety_ValidateString(src, dest_size - strlen(dest), __func__) ? \
     Q_strcat_safe(dest, dest_size, src) : qfalse)

// Type-safe enum conversion with validation
#define SAFE_ENUM_CAST(value, enum_type, max_value) \
    ((enum_type)VALIDATE_ENUM((int)(value), (max_value)))

// Type-safe array access with bounds checking
#define SAFE_ARRAY_ACCESS(array, index, default_value) \
    (ARRAY_BOUNDS_CHECK(array, index) ? (array)[index] : (default_value))

// Debug mode type safety assertions (only in debug builds)
#ifdef _DEBUG
#define TYPE_SAFETY_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            Com_Printf("TYPE SAFETY ASSERTION FAILED: %s\n", message); \
            assert(0); \
        } \
    } while(0)
#else
#define TYPE_SAFETY_ASSERT(condition, message) ((void)0)
#endif

// Type-safe function parameter validation macros
#define VALIDATE_PARAM_NOT_NULL(param) \
    TYPE_SAFETY_ASSERT(POINTER_IS_VALID(param), #param " cannot be NULL")

#define VALIDATE_PARAM_INT_RANGE(param, min_val, max_val) \
    TYPE_SAFETY_ASSERT(INT_IS_VALID(param, min_val, max_val), #param " out of valid range")

#define VALIDATE_PARAM_FLOAT_RANGE(param, min_val, max_val) \
    TYPE_SAFETY_ASSERT(FLOAT_IS_VALID(param, min_val, max_val), #param " out of valid range")

#define VALIDATE_PARAM_STRING(param, max_len) \
    TYPE_SAFETY_ASSERT(STRING_LENGTH_IS_VALID(param, max_len), #param " invalid string")

#define VALIDATE_PARAM_MEMORY_SIZE(param, max_size) \
    TYPE_SAFETY_ASSERT(MEMORY_SIZE_IS_VALID(param) && param <= max_size, #param " invalid memory size")

// Type-safe return value validation
#define VALIDATE_RETURN_INT(value, min_val, max_val) \
    TYPE_SAFETY_ASSERT(INT_IS_VALID(value, min_val, max_val), "return value out of valid range")

#define VALIDATE_RETURN_FLOAT(value, min_val, max_val) \
    TYPE_SAFETY_ASSERT(FLOAT_IS_VALID(value, min_val, max_val), "return value out of valid range")

#define VALIDATE_RETURN_POINTER(value) \
    TYPE_SAFETY_ASSERT(POINTER_IS_VALID(value), "return value cannot be NULL")

// Type safety initialization and shutdown
qboolean TypeSafety_Init(void);
void TypeSafety_Shutdown(void);

// Type safety statistics and reporting
typedef struct {
    uint64_t total_validations;
    uint64_t failed_validations;
    uint64_t null_pointer_checks;
    uint64_t bounds_checks;
    uint64_t range_checks;
    uint64_t string_checks;
    uint64_t memory_checks;
} type_safety_stats_t;

void TypeSafety_GetStats(type_safety_stats_t *stats);
void TypeSafety_ReportViolations(void);
void TypeSafety_ResetStats(void);

#endif // __TYPE_SAFETY_H__
