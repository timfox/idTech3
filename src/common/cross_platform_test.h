/*
=============================================================================
Cross-Platform Compatibility Test Framework

Automated testing across all supported platforms and architectures.
=============================================================================
*/

#ifndef __CROSS_PLATFORM_TEST_H__
#define __CROSS_PLATFORM_TEST_H__

#include "q_shared.h"

// Platform types
typedef enum {
    PLATFORM_UNKNOWN = 0,
    PLATFORM_WINDOWS,
    PLATFORM_LINUX,
    PLATFORM_MACOS,
    PLATFORM_FREEBSD,
    PLATFORM_ANDROID,
    PLATFORM_IOS,
    PLATFORM_EMSCRIPTEN,
    PLATFORM_COUNT
} platform_type_t;


// Compiler types
typedef enum {
    COMPILER_UNKNOWN = 0,
    COMPILER_GCC,
    COMPILER_CLANG,
    COMPILER_MSVC,
    COMPILER_EMSCRIPTEN,
    COMPILER_COUNT
} compiler_type_t;

// Test result types
typedef enum {
    COMPAT_RESULT_PASS = 0,
    COMPAT_RESULT_FAIL,
    COMPAT_RESULT_SKIP,        // Test not applicable for this platform
    COMPAT_RESULT_TIMEOUT,
    COMPAT_RESULT_CRASH,
    COMPAT_RESULT_INCOMPLETE,
    COMPAT_RESULT_COUNT
} compatibility_result_t;

// Platform capabilities
typedef struct {
    qboolean has_vulkan;
    qboolean has_opengl;
    qboolean has_opengles;
    qboolean has_metal;
    qboolean has_directx;
    qboolean has_threads;
    qboolean has_simd;
    qboolean has_atomic_ops;
    qboolean has_tls;
    qboolean has_mmap;
    qboolean has_large_files;
    qboolean has_unicode;
    qboolean has_networking;
    qboolean has_audio;
    int max_threads;
    int page_size;
    uint64_t total_memory;
    uint64_t available_memory;
} platform_capabilities_t;

// Architecture information
typedef struct {
    architecture_type_t type;
    int bits;                    // 32 or 64
    qboolean little_endian;
    qboolean supports_simd;
    qboolean supports_atomic64;
    char name[32];
} architecture_info_t;

// Compiler information
typedef struct {
    compiler_type_t type;
    char name[32];
    char version[32];
    qboolean supports_c11;
    qboolean supports_c23;
    qboolean supports_cpp11;
    qboolean supports_cpp23;
} compiler_info_t;

// Platform information
typedef struct {
    platform_type_t type;
    char name[32];
    char version[32];
    architecture_info_t arch;
    compiler_info_t compiler;
    platform_capabilities_t capabilities;
} platform_info_t;

// Cross-platform test configuration
typedef struct {
    char test_name[64];
    char description[256];
    platform_type_t required_platform;
    architecture_type_t required_arch;
    qboolean requires_graphics;
    qboolean requires_network;
    qboolean requires_audio;
    qboolean requires_large_memory;
    int timeout_seconds;
    char setup_script[256];
    char cleanup_script[256];
} cross_platform_test_config_t;

// Test execution result
typedef struct {
    char test_name[64];
    platform_info_t platform;
    compatibility_result_t result;
    uint64_t start_time;
    uint64_t end_time;
    uint64_t duration_ms;
    char error_message[512];
    char log_output[4096];
    int exit_code;
    qboolean crashed;
    uint64_t memory_peak_usage;
    uint64_t cpu_time_ms;
} cross_platform_test_result_t;

// Test suite configuration
typedef struct {
    char suite_name[64];
    char description[256];
    cross_platform_test_config_t* tests;
    uint32_t num_tests;
    uint32_t max_tests;
    qboolean run_in_parallel;
    int max_parallel_tests;
    qboolean stop_on_failure;
    int suite_timeout_seconds;
} cross_platform_test_suite_t;

// Cross-platform test system
typedef struct {
    qboolean initialized;
    platform_info_t current_platform;
    cross_platform_test_suite_t* current_suite;
    cross_platform_test_result_t* results;
    uint32_t max_results;
    uint32_t num_results;

    // Statistics
    uint32_t total_tests_run;
    uint32_t total_passed;
    uint32_t total_failed;
    uint32_t total_skipped;
    uint32_t total_timeouts;
    uint32_t total_crashes;
} cross_platform_test_system_t;

extern cross_platform_test_system_t cross_platform_test_system;

// Cross-Platform Test API
qboolean CrossPlatformTest_Init(void);
void CrossPlatformTest_Shutdown(void);

// Platform Detection
qboolean CrossPlatformTest_DetectPlatform(platform_info_t* info);
const char* CrossPlatformTest_GetPlatformName(platform_type_t platform);
const char* CrossPlatformTest_GetArchitectureName(architecture_type_t arch);
const char* CrossPlatformTest_GetCompilerName(compiler_type_t compiler);

// Test Suite Management
cross_platform_test_suite_t* CrossPlatformTest_CreateSuite(const char* name, const char* description);
qboolean CrossPlatformTest_AddTestToSuite(cross_platform_test_suite_t* suite, const cross_platform_test_config_t* config);
qboolean CrossPlatformTest_RunSuite(cross_platform_test_suite_t* suite);

// Individual Test Execution
qboolean CrossPlatformTest_RunTest(const cross_platform_test_config_t* config, cross_platform_test_result_t* result);
qboolean CrossPlatformTest_CancelTest(void);
qboolean CrossPlatformTest_IsTestRunning(void);

// Test Result Management
uint32_t CrossPlatformTest_GetResults(cross_platform_test_result_t** results);
qboolean CrossPlatformTest_SaveResults(const char* filename);
qboolean CrossPlatformTest_LoadResults(const char* filename);

// Platform Capability Testing
qboolean CrossPlatformTest_TestPlatformCapabilities(void);
qboolean CrossPlatformTest_TestArchitectureFeatures(void);
qboolean CrossPlatformTest_TestCompilerFeatures(void);

// Automated Test Generation
qboolean CrossPlatformTest_GeneratePlatformTests(cross_platform_test_suite_t* suite);
qboolean CrossPlatformTest_GenerateArchitectureTests(cross_platform_test_suite_t* suite);
qboolean CrossPlatformTest_GenerateCompilerTests(cross_platform_test_suite_t* suite);

// CI/CD Integration
qboolean CrossPlatformTest_ExportForCI(const char* output_dir);
qboolean CrossPlatformTest_GenerateCIReport(const char* output_file, const char* format);

// Utility Functions
qboolean CrossPlatformTest_ValidatePlatformCompatibility(void);
qboolean CrossPlatformTest_CheckMinimumRequirements(void);
const char* CrossPlatformTest_GetResultString(compatibility_result_t result);

// Built-in Test Functions
qboolean CrossPlatformTest_BasicFunctionality(void);
qboolean CrossPlatformTest_MemoryManagement(void);
qboolean CrossPlatformTest_Threading(void);
qboolean CrossPlatformTest_FileSystem(void);
qboolean CrossPlatformTest_NetworkBasic(void);
qboolean CrossPlatformTest_GraphicsAPI(void);
qboolean CrossPlatformTest_AudioAPI(void);
qboolean CrossPlatformTest_LargeFileSupport(void);
qboolean CrossPlatformTest_UnicodeSupport(void);
qboolean CrossPlatformTest_TimeAndDate(void);
qboolean CrossPlatformTest_MathPrecision(void);

#endif // __CROSS_PLATFORM_TEST_H__
