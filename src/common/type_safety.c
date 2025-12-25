/*
=============================================================================
Type Safety Framework Implementation

Comprehensive compile-time and runtime type checking and validation.
=============================================================================
*/

#include "type_safety.h"

// Global type safety statistics
static type_safety_stats_t type_safety_stats = {0};

/*
=============================================================================
Type Safety Framework API Implementation
=============================================================================
*/

qboolean TypeSafety_Init(void) {
    memset(&type_safety_stats, 0, sizeof(type_safety_stats));

    Com_Printf("Type safety framework initialized\n");
    Com_Printf("Runtime type validation and bounds checking enabled\n");

    return qtrue;
}

void TypeSafety_Shutdown(void) {
    // Report final statistics
    Com_Printf("Type safety framework shutdown\n");
    Com_Printf("Total validations performed: %llu\n", type_safety_stats.total_validations);
    Com_Printf("Failed validations: %llu\n", type_safety_stats.failed_validations);

    if (type_safety_stats.failed_validations > 0) {
        Com_Printf("WARNING: %llu type safety violations detected during runtime\n",
                  type_safety_stats.failed_validations);
    }

    memset(&type_safety_stats, 0, sizeof(type_safety_stats));
}

void TypeSafety_GetStats(type_safety_stats_t *stats) {
    if (stats) {
        memcpy(stats, &type_safety_stats, sizeof(type_safety_stats_t));
    }
}

void TypeSafety_ReportViolations(void) {
    Com_Printf("=== Type Safety Violation Report ===\n");
    Com_Printf("Total validations: %llu\n", type_safety_stats.total_validations);
    Com_Printf("Failed validations: %llu\n", type_safety_stats.failed_validations);
    Com_Printf("Null pointer checks: %llu\n", type_safety_stats.null_pointer_checks);
    Com_Printf("Bounds checks: %llu\n", type_safety_stats.bounds_checks);
    Com_Printf("Range checks: %llu\n", type_safety_stats.range_checks);
    Com_Printf("String checks: %llu\n", type_safety_stats.string_checks);
    Com_Printf("Memory checks: %llu\n", type_safety_stats.memory_checks);

    if (type_safety_stats.failed_validations > 0) {
        float failure_rate = (float)type_safety_stats.failed_validations /
                           (float)type_safety_stats.total_validations * 100.0f;
        Com_Printf("Failure rate: %.2f%%\n", failure_rate);
    }

    Com_Printf("===================================\n");
}

void TypeSafety_ResetStats(void) {
    memset(&type_safety_stats, 0, sizeof(type_safety_stats_t));
    Com_Printf("Type safety statistics reset\n");
}

// Internal validation counter functions
static void TypeSafety_RecordValidation(qboolean success) {
    type_safety_stats.total_validations++;
    if (!success) {
        type_safety_stats.failed_validations++;
    }
}

static void TypeSafety_RecordNullPointerCheck(qboolean success) {
    type_safety_stats.null_pointer_checks++;
    TypeSafety_RecordValidation(success);
}

static void TypeSafety_RecordBoundsCheck(qboolean success) {
    type_safety_stats.bounds_checks++;
    TypeSafety_RecordValidation(success);
}

static void TypeSafety_RecordRangeCheck(qboolean success) {
    type_safety_stats.range_checks++;
    TypeSafety_RecordValidation(success);
}

static void TypeSafety_RecordStringCheck(qboolean success) {
    type_safety_stats.string_checks++;
    TypeSafety_RecordValidation(success);
}

static void TypeSafety_RecordMemoryCheck(qboolean success) {
    type_safety_stats.memory_checks++;
    TypeSafety_RecordValidation(success);
}

/*
=============================================================================
Enhanced Runtime Validation Functions
=============================================================================
*/

// Enhanced pointer validation with type information
qboolean TypeSafety_ValidatePointerEx(const void *ptr, const char *ptr_name,
                                    const char *context, const char *file, int line) {
    TypeSafety_RecordNullPointerCheck(POINTER_IS_VALID(ptr));

    if (!POINTER_IS_VALID(ptr)) {
        Com_Printf("TYPE SAFETY VIOLATION at %s:%d in %s()\n", file, line, context);
        Com_Printf("  NULL pointer: %s\n", ptr_name);
        return qfalse;
    }
    return qtrue;
}

// Enhanced array bounds validation
qboolean TypeSafety_ValidateArrayBoundsEx(const void *array, size_t element_size,
                                        size_t element_count, size_t max_elements,
                                        const char *array_name, const char *context,
                                        const char *file, int line) {
    qboolean valid = TypeSafety_ValidateArrayBounds(array, element_size, element_count,
                                                  max_elements, context);
    TypeSafety_RecordBoundsCheck(valid);

    if (!valid) {
        Com_Printf("TYPE SAFETY VIOLATION at %s:%d in %s()\n", file, line, context);
        Com_Printf("  Array bounds violation: %s\n", array_name);
        Com_Printf("  Requested: %zu elements, Max allowed: %zu\n", element_count, max_elements);
    }

    return valid;
}

// Enhanced string validation
qboolean TypeSafety_ValidateStringEx(const char *str, size_t max_len,
                                   const char *str_name, const char *context,
                                   const char *file, int line) {
    qboolean valid = TypeSafety_ValidateString(str, max_len, context);
    TypeSafety_RecordStringCheck(valid);

    if (!valid) {
        Com_Printf("TYPE SAFETY VIOLATION at %s:%d in %s()\n", file, line, context);
        Com_Printf("  String validation failure: %s\n", str_name);
        if (str) {
            Com_Printf("  Length: %zu, Max allowed: %zu\n", strlen(str), max_len);
        } else {
            Com_Printf("  NULL string\n");
        }
    }

    return valid;
}

// Enhanced numeric validation
qboolean TypeSafety_ValidateIntEx(int value, int min_val, int max_val,
                                const char *value_name, const char *context,
                                const char *file, int line) {
    qboolean valid = TypeSafety_ValidateInt(value, min_val, max_val, context);
    TypeSafety_RecordRangeCheck(valid);

    if (!valid) {
        Com_Printf("TYPE SAFETY VIOLATION at %s:%d in %s()\n", file, line, context);
        Com_Printf("  Integer range violation: %s = %d\n", value_name, value);
        Com_Printf("  Valid range: [%d, %d]\n", min_val, max_val);
    }

    return valid;
}

qboolean TypeSafety_ValidateFloatEx(float value, float min_val, float max_val,
                                  const char *value_name, const char *context,
                                  const char *file, int line) {
    qboolean valid = TypeSafety_ValidateFloat(value, min_val, max_val, context);
    TypeSafety_RecordRangeCheck(valid);

    if (!valid) {
        Com_Printf("TYPE SAFETY VIOLATION at %s:%d in %s()\n", file, line, context);
        Com_Printf("  Float range violation: %s = %f\n", value_name, value);
        Com_Printf("  Valid range: [%f, %f]\n", min_val, max_val);
    }

    return valid;
}

// Enhanced memory validation
qboolean TypeSafety_ValidateMemorySizeEx(size_t size, size_t max_size,
                                       const char *size_name, const char *context,
                                       const char *file, int line) {
    qboolean valid = TypeSafety_ValidateMemorySize(size, max_size, context);
    TypeSafety_RecordMemoryCheck(valid);

    if (!valid) {
        Com_Printf("TYPE SAFETY VIOLATION at %s:%d in %s()\n", file, line, context);
        Com_Printf("  Memory size violation: %s = %zu bytes\n", size_name, size);
        Com_Printf("  Max allowed: %zu bytes\n", max_size);
    }

    return valid;
}

/*
=============================================================================
Type Safety Testing Functions
=============================================================================
*/

// Test all type safety validations
qboolean TypeSafety_RunValidationTests(void) {
    Com_Printf("Running type safety validation tests...\n");

    int test_count = 0;
    int pass_count = 0;

    // Test pointer validation
    test_count++;
    if (TypeSafety_ValidatePointer(NULL, "test_null_ptr")) {
        Com_Printf("FAILED: NULL pointer validation\n");
    } else {
        pass_count++;
    }

    test_count++;
    if (!TypeSafety_ValidatePointer(&test_count, "test_valid_ptr")) {
        Com_Printf("FAILED: Valid pointer validation\n");
    } else {
        pass_count++;
    }

    // Test string validation
    test_count++;
    if (TypeSafety_ValidateString(NULL, 100, "test_null_str")) {
        Com_Printf("FAILED: NULL string validation\n");
    } else {
        pass_count++;
    }

    test_count++;
    if (TypeSafety_ValidateString("", 100, "test_empty_str")) {
        Com_Printf("FAILED: Empty string validation\n");
    } else {
        pass_count++;
    }

    test_count++;
    char long_string[2000] = {0};
    memset(long_string, 'a', 1999);
    if (TypeSafety_ValidateString(long_string, 100, "test_long_str")) {
        Com_Printf("FAILED: Long string validation\n");
    } else {
        pass_count++;
    }

    test_count++;
    if (!TypeSafety_ValidateString("valid", 100, "test_valid_str")) {
        Com_Printf("FAILED: Valid string validation\n");
    } else {
        pass_count++;
    }

    // Test integer validation
    test_count++;
    if (TypeSafety_ValidateInt(50, 0, 100, "test_int_in_range")) {
        pass_count++;
    } else {
        Com_Printf("FAILED: In-range integer validation\n");
    }

    test_count++;
    if (TypeSafety_ValidateInt(150, 0, 100, "test_int_out_range")) {
        Com_Printf("FAILED: Out-of-range integer validation\n");
    } else {
        pass_count++;
    }

    // Test float validation
    test_count++;
    if (TypeSafety_ValidateFloat(1.5f, 0.0f, 2.0f, "test_float_in_range")) {
        pass_count++;
    } else {
        Com_Printf("FAILED: In-range float validation\n");
    }

    test_count++;
    if (TypeSafety_ValidateFloat(NAN, 0.0f, 2.0f, "test_float_nan")) {
        Com_Printf("FAILED: NaN float validation\n");
    } else {
        pass_count++;
    }

    // Test memory size validation
    test_count++;
    if (TypeSafety_ValidateMemorySize(1024, 2048, "test_mem_size_valid")) {
        pass_count++;
    } else {
        Com_Printf("FAILED: Valid memory size validation\n");
    }

    test_count++;
    if (TypeSafety_ValidateMemorySize(2 * 1024 * 1024 * 1024ULL, 1024, "test_mem_size_invalid")) {
        Com_Printf("FAILED: Invalid memory size validation\n");
    } else {
        pass_count++;
    }

    Com_Printf("Type safety validation tests: %d/%d passed\n", pass_count, test_count);

    if (pass_count == test_count) {
        Com_Printf("All type safety validation tests PASSED\n");
        return qtrue;
    } else {
        Com_Printf("Some type safety validation tests FAILED\n");
        return qfalse;
    }
}

// Benchmark type safety overhead
qboolean TypeSafety_RunPerformanceBenchmark(void) {
    Com_Printf("Benchmarking type safety overhead...\n");

    const int iterations = 1000000;
    uint64_t start_time, end_time;

    // Benchmark pointer validation
    start_time = Sys_Milliseconds();
    for (int i = 0; i < iterations; i++) {
        volatile void *ptr = (void *)&i;
        volatile qboolean result = POINTER_IS_VALID(ptr);
        (void)result; // Prevent optimization
    }
    end_time = Sys_Milliseconds();
    Com_Printf("Pointer validation: %llu ms for %d iterations\n", end_time - start_time, iterations);

    // Benchmark string validation
    start_time = Sys_Milliseconds();
    for (int i = 0; i < iterations; i++) {
        volatile const char *str = "test";
        volatile qboolean result = STRING_IS_VALID(str);
        (void)result; // Prevent optimization
    }
    end_time = Sys_Milliseconds();
    Com_Printf("String validation: %llu ms for %d iterations\n", end_time - start_time, iterations);

    // Benchmark integer validation
    start_time = Sys_Milliseconds();
    for (int i = 0; i < iterations; i++) {
        volatile int value = i % 100;
        volatile qboolean result = INT_IS_VALID(value, 0, 99);
        (void)result; // Prevent optimization
    }
    end_time = Sys_Milliseconds();
    Com_Printf("Integer validation: %llu ms for %d iterations\n", end_time - start_time, iterations);

    Com_Printf("Type safety performance benchmarking completed\n");
    return qtrue;
}

/*
=============================================================================
Type Safety Integration Functions
=============================================================================
*/

// Integrate with existing validation systems
qboolean TypeSafety_ValidateQSharedTypes(void) {
    Com_Printf("Validating q_shared.h type definitions...\n");

    // Validate vector sizes (compile-time assertions should catch these)
    STATIC_ASSERT_TYPE_SIZE(vec3_t, 12);
    STATIC_ASSERT_TYPE_SIZE(color4ub_t, 4);
    STATIC_ASSERT_TYPE_SIZE(floatint_t, 4);

    // Test runtime vector validation
    vec3_t test_vec = {1.0f, 2.0f, 3.0f};
    if (!VECTOR3_IS_VALID(test_vec)) {
        Com_Printf("FAILED: Vector validation\n");
        return qfalse;
    }

    // Test color validation
    if (!COLOR_RGBA_IS_VALID(1.0f, 0.5f, 0.0f, 1.0f)) {
        Com_Printf("FAILED: Color validation\n");
        return qfalse;
    }

    Com_Printf("q_shared.h type validation passed\n");
    return qtrue;
}

// Validate Vulkan renderer types
qboolean TypeSafety_ValidateVulkanTypes(void) {
    Com_Printf("Validating Vulkan renderer type definitions...\n");

    // Test image dimension validation
    if (!Vk_ValidateImageDimensions(1920, 1080, 1)) {
        Com_Printf("FAILED: Image dimension validation\n");
        return qfalse;
    }

    if (Vk_ValidateImageDimensions(0, 1080, 1)) {
        Com_Printf("FAILED: Invalid dimension validation\n");
        return qfalse;
    }

    Com_Printf("Vulkan renderer type validation passed\n");
    return qtrue;
}

// Comprehensive type safety audit
qboolean TypeSafety_RunFullAudit(void) {
    Com_Printf("Running comprehensive type safety audit...\n");

    qboolean all_passed = qtrue;

    if (!TypeSafety_RunValidationTests()) {
        Com_Printf("VALIDATION TESTS: FAILED\n");
        all_passed = qfalse;
    }

    if (!TypeSafety_RunPerformanceBenchmark()) {
        Com_Printf("PERFORMANCE BENCHMARK: FAILED\n");
        all_passed = qfalse;
    }

    if (!TypeSafety_ValidateQSharedTypes()) {
        Com_Printf("Q_SHARED TYPES: FAILED\n");
        all_passed = qfalse;
    }

    if (!TypeSafety_ValidateVulkanTypes()) {
        Com_Printf("VULKAN TYPES: FAILED\n");
        all_passed = qfalse;
    }

    TypeSafety_ReportViolations();

    if (all_passed) {
        Com_Printf("Comprehensive type safety audit: PASSED\n");
    } else {
        Com_Printf("Comprehensive type safety audit: FAILED\n");
    }

    return all_passed;
}
