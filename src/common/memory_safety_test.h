/*
=============================================================================
Memory Safety Test Framework

ASan/UBSan validation for comprehensive memory safety testing.
=============================================================================
*/

#ifndef __MEMORY_SAFETY_TEST_H__
#define __MEMORY_SAFETY_TEST_H__

#include "q_shared.h"

// Memory safety test result types
typedef enum {
    SAFETY_RESULT_PASS = 0,     // Test passed without issues
    SAFETY_RESULT_ASAN_ERROR,   // Address Sanitizer error detected
    SAFETY_RESULT_UBSAN_ERROR,  // Undefined Behavior Sanitizer error detected
    SAFETY_RESULT_LEAK_DETECTED,// Memory leak detected
    SAFETY_RESULT_TIMEOUT,      // Test timed out
    SAFETY_RESULT_CRASH,        // Test crashed
    SAFETY_RESULT_INCOMPLETE,   // Test did not complete
    SAFETY_RESULT_COUNT
} memory_safety_result_t;

// ASan error types
typedef enum {
    ASAN_ERROR_NONE = 0,
    ASAN_ERROR_HEAP_OOB,         // Heap out-of-bounds
    ASAN_ERROR_STACK_OOB,        // Stack out-of-bounds
    ASAN_ERROR_GLOBAL_OOB,       // Global out-of-bounds
    ASAN_ERROR_USE_AFTER_FREE,   // Use-after-free
    ASAN_ERROR_USE_AFTER_RETURN, // Use-after-return
    ASAN_ERROR_DOUBLE_FREE,      // Double free
    ASAN_ERROR_INVALID_FREE,     // Invalid free
    ASAN_ERROR_MEMORY_LEAK,      // Memory leak
    ASAN_ERROR_UNKNOWN
} asan_error_type_t;

// UBSan error types
typedef enum {
    UBSAN_ERROR_NONE = 0,
    UBSAN_ERROR_INT_OVERFLOW,    // Integer overflow
    UBSAN_ERROR_INT_DIVIDE_BY_ZERO, // Division by zero
    UBSAN_ERROR_FLOAT_CAST_OVERFLOW, // Float cast overflow
    UBSAN_ERROR_INVALID_BOOL,    // Invalid bool value
    UBSAN_ERROR_MISALIGNED_ACCESS, // Misaligned pointer access
    UBSAN_ERROR_NULLPTR_DEREF,   // Null pointer dereference
    UBSAN_ERROR_INVALID_ENUM,    // Invalid enum value
    UBSAN_ERROR_FUNCTION_TYPE_MISMATCH, // Function type mismatch
    UBSAN_ERROR_UNKNOWN
} ubsan_error_type_t;

// Memory safety test configuration
typedef struct {
    char test_name[64];
    char description[256];
    qboolean enable_asan;        // Enable Address Sanitizer
    qboolean enable_ubsan;       // Enable Undefined Behavior Sanitizer
    qboolean enable_lsan;        // Enable Leak Sanitizer
    qboolean strict_mode;        // Treat warnings as errors
    int timeout_seconds;         // Test timeout
    qboolean isolate_heap;       // Isolate heap operations
    qboolean isolate_stack;      // Isolate stack operations
    char setup_environment[256]; // Environment setup commands
    char* test_code;             // Custom test code to execute
    int test_code_length;
} memory_safety_test_config_t;

// Sanitizer error report
typedef struct {
    char error_type[32];         // "ASan" or "UBSan"
    asan_error_type_t asan_type; // ASan error classification
    ubsan_error_type_t ubsan_type; // UBSan error classification
    char function[128];          // Function where error occurred
    char file[256];              // Source file
    int line;                    // Line number
    char address[32];            // Memory address (hex)
    char description[1024];      // Error description
    char stack_trace[4096];      // Stack trace
    qboolean is_fatal;           // Whether this is a fatal error
    uint64_t timestamp;          // When error was detected
} sanitizer_error_t;

// Memory safety test result
typedef struct {
    char test_name[64];
    memory_safety_result_t result;
    uint64_t start_time;
    uint64_t end_time;
    uint64_t duration_ms;

    // Error details
    sanitizer_error_t* errors;
    uint32_t error_count;
    uint32_t max_errors;

    // Memory statistics
    uint64_t peak_heap_usage;
    uint64_t total_allocations;
    uint64_t total_frees;
    uint64_t memory_leaks;

    // Test metadata
    char platform[32];
    char compiler[32];
    char sanitizer_version[32];
    qboolean asan_enabled;
    qboolean ubsan_enabled;
    qboolean lsan_enabled;
} memory_safety_test_result_t;

// Memory safety test suite
typedef struct {
    char suite_name[64];
    char description[256];
    memory_safety_test_config_t* tests;
    uint32_t num_tests;
    uint32_t max_tests;
    qboolean enable_asan;
    qboolean enable_ubsan;
    qboolean enable_lsan;
    qboolean strict_mode;
    int suite_timeout_seconds;
} memory_safety_test_suite_t;

// Memory safety testing system
typedef struct {
    qboolean initialized;
    memory_safety_test_suite_t* current_suite;
    memory_safety_test_result_t* results;
    uint32_t max_results;
    uint32_t num_results;

    // Sanitizer configuration
    qboolean asan_available;
    qboolean ubsan_available;
    qboolean lsan_available;
    char asan_version[32];
    char ubsan_version[32];
    char lsan_version[32];

    // Statistics
    uint32_t total_tests_run;
    uint32_t total_passed;
    uint32_t total_asan_errors;
    uint32_t total_ubsan_errors;
    uint32_t total_leaks_detected;
    uint32_t total_crashes;
} memory_safety_test_system_t;

extern memory_safety_test_system_t memory_safety_test_system;

// Memory Safety Test API
qboolean MemorySafetyTest_Init(void);
void MemorySafetyTest_Shutdown(void);

// Sanitizer Detection and Configuration
qboolean MemorySafetyTest_DetectSanitizers(void);
qboolean MemorySafetyTest_EnableASan(void);
qboolean MemorySafetyTest_EnableUBSan(void);
qboolean MemorySafetyTest_EnableLSan(void);
qboolean MemorySafetyTest_SetStrictMode(qboolean strict);

// Test Suite Management
memory_safety_test_suite_t* MemorySafetyTest_CreateSuite(const char* name, const char* description);
qboolean MemorySafetyTest_AddTestToSuite(memory_safety_test_suite_t* suite,
                                       const memory_safety_test_config_t* config);
qboolean MemorySafetyTest_RunSuite(memory_safety_test_suite_t* suite);

// Individual Test Execution
qboolean MemorySafetyTest_RunTest(const memory_safety_test_config_t* config,
                                memory_safety_test_result_t* result);
qboolean MemorySafetyTest_CancelTest(void);
qboolean MemorySafetyTest_IsTestRunning(void);

// Test Result Management
uint32_t MemorySafetyTest_GetResults(memory_safety_test_result_t** results);
qboolean MemorySafetyTest_SaveResults(const char* filename);
qboolean MemorySafetyTest_LoadResults(const char* filename);

// Error Analysis
asan_error_type_t MemorySafetyTest_ClassifyASanError(const char* error_msg);
ubsan_error_type_t MemorySafetyTest_ClassifyUBSanError(const char* error_msg);
qboolean MemorySafetyTest_ParseSanitizerOutput(const char* output,
                                             sanitizer_error_t* errors,
                                             uint32_t max_errors,
                                             uint32_t* num_errors);

// Built-in Test Functions
qboolean MemorySafetyTest_BufferOverflow(void);
qboolean MemorySafetyTest_UseAfterFree(void);
qboolean MemorySafetyTest_DoubleFree(void);
qboolean MemorySafetyTest_MemoryLeak(void);
qboolean MemorySafetyTest_IntegerOverflow(void);
qboolean MemorySafetyTest_DivisionByZero(void);
qboolean MemorySafetyTest_NullPointerDeref(void);
qboolean MemorySafetyTest_UninitializedVariable(void);
qboolean MemorySafetyTest_TypeConfusion(void);
qboolean MemorySafetyTest_StackOverflow(void);

// Comprehensive Code Path Testing
qboolean MemorySafetyTest_FileOperations(void);
qboolean MemorySafetyTest_NetworkOperations(void);
qboolean MemorySafetyTest_ThreadOperations(void);
qboolean MemorySafetyTest_GraphicsOperations(void);
qboolean MemorySafetyTest_AudioOperations(void);
qboolean MemorySafetyTest_MemoryManagement(void);
qboolean MemorySafetyTest_StringOperations(void);
qboolean MemorySafetyTest_DataStructures(void);

// Automated Test Generation
qboolean MemorySafetyTest_GenerateFuzzTests(memory_safety_test_suite_t* suite);
qboolean MemorySafetyTest_GenerateBoundaryTests(memory_safety_test_suite_t* suite);
qboolean MemorySafetyTest_GenerateConcurrencyTests(memory_safety_test_suite_t* suite);

// CI/CD Integration
qboolean MemorySafetyTest_ExportForCI(const char* output_dir);
qboolean MemorySafetyTest_GenerateReport(const char* output_file, const char* format);

// Utility Functions
const char* MemorySafetyTest_GetResultString(memory_safety_result_t result);
const char* MemorySafetyTest_GetASanErrorString(asan_error_type_t error);
const char* MemorySafetyTest_GetUBSanErrorString(ubsan_error_type_t error);
qboolean MemorySafetyTest_ValidateTestConfig(const memory_safety_test_config_t* config);

// Sanitizer Control Functions
void MemorySafetyTest_PoisonMemory(void* ptr, size_t size);
void MemorySafetyTest_UnpoisonMemory(void* ptr, size_t size);
void MemorySafetyTest_DisableSanitizer(void);
void MemorySafetyTest_EnableSanitizer(void);

// Memory Safety Testing Primitives
void* MemorySafetyTest_AllocateMemory(size_t size);
void MemorySafetyTest_FreeMemory(void* ptr);
void MemorySafetyTest_WriteToMemory(void* ptr, size_t offset, uint8_t value);
uint8_t MemorySafetyTest_ReadFromMemory(void* ptr, size_t offset);

#endif // __MEMORY_SAFETY_TEST_H__
