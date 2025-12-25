/*
=============================================================================
Comprehensive Compatibility Testing Framework

Cross-platform and cross-hardware validation system for automated
compatibility verification and feature detection.
=============================================================================
*/

#ifndef __COMPATIBILITY_TEST_H__
#define __COMPATIBILITY_TEST_H__

#include "q_shared.h"

// Platform compatibility levels
typedef enum {
    COMPATIBILITY_NONE = 0,        // Not compatible
    COMPATIBILITY_MINIMAL,         // Minimal functionality
    COMPATIBILITY_BASIC,           // Basic features work
    COMPATIBILITY_FULL,            // Full functionality
    COMPATIBILITY_OPTIMAL,         // Optimal performance/features
    COMPATIBILITY_COUNT
} compatibility_level_t;

// Hardware categories
typedef enum {
    HW_CATEGORY_CPU = 0,
    HW_CATEGORY_GPU,
    HW_CATEGORY_MEMORY,
    HW_CATEGORY_STORAGE,
    HW_CATEGORY_NETWORK,
    HW_CATEGORY_AUDIO,
    HW_CATEGORY_INPUT,
    HW_CATEGORY_DISPLAY,
    HW_CATEGORY_COUNT
} hardware_category_t;

// Feature categories
typedef enum {
    FEATURE_RENDERING = 0,
    FEATURE_COMPUTE,
    FEATURE_MULTIMEDIA,
    FEATURE_NETWORKING,
    FEATURE_STORAGE,
    FEATURE_SECURITY,
    FEATURE_MULTITHREADING,
    FEATURE_UI,
    FEATURE_COUNT
} feature_category_t;

// Platform information
typedef struct {
    char os_name[64];
    char os_version[32];
    char kernel_version[32];
    char distribution[64];
    platform_type_t platform_type;
    architecture_type_t architecture;
    compiler_type_t compiler;
    qboolean is_64bit;
    qboolean is_big_endian;
    uint32_t cpu_count;
    uint64_t total_memory_mb;
    char locale[16];
    char timezone[32];
} compatibility_platform_info_t;

// Hardware capabilities
typedef struct {
    // CPU information
    char cpu_vendor[32];
    char cpu_brand[64];
    uint32_t cpu_cores;
    uint32_t cpu_threads;
    uint32_t cpu_frequency_mhz;
    qboolean has_sse;
    qboolean has_sse2;
    qboolean has_sse3;
    qboolean has_ssse3;
    qboolean has_sse4_1;
    qboolean has_sse4_2;
    qboolean has_avx;
    qboolean has_avx2;
    qboolean has_avx512;

    // GPU information
    char gpu_vendor[32];
    char gpu_renderer[128];
    char gpu_version[32];
    uint64_t gpu_memory_mb;
    qboolean supports_opengl;
    qboolean supports_vulkan;
    qboolean supports_directx;
    qboolean supports_metal;

    // Memory information
    uint64_t total_ram_mb;
    uint64_t available_ram_mb;
    uint32_t memory_channels;
    qboolean supports_ecc;

    // Storage information
    uint64_t total_storage_mb;
    uint64_t available_storage_mb;
    char storage_type[16]; // SSD, HDD, NVMe, etc.

    // Network information
    qboolean has_network;
    char network_type[16]; // Ethernet, WiFi, etc.
    uint32_t network_speed_mbps;
} hardware_capabilities_t;

// Feature support matrix
typedef struct {
    feature_category_t category;
    char feature_name[64];
    compatibility_level_t compatibility;
    qboolean is_required;
    qboolean is_available;
    char requirement_description[128];
    char limitation_description[256];
} feature_support_t;

// Compatibility test result
typedef struct {
    char test_name[64];
    char platform_description[128];
    compatibility_level_t overall_compatibility;
    uint32_t features_tested;
    uint32_t features_passed;
    uint32_t features_failed;
    uint32_t features_skipped;
    qboolean platform_supported;
    char recommendations[512];
    uint64_t test_duration_ms;
    char error_message[256];
} compatibility_result_t;

// Compatibility testing system
typedef struct {
    qboolean initialized;
    compatibility_platform_info_t platform_info;
    hardware_capabilities_t hardware_caps;
    feature_support_t* feature_matrix;
    uint32_t feature_count;
    uint32_t max_features;

    // Test results
    compatibility_result_t* test_results;
    uint32_t result_count;
    uint32_t max_results;

    // Configuration
    qboolean enable_detailed_logging;
    qboolean enable_hardware_profiling;
    qboolean enable_feature_detection;
    char report_directory[256];
    char baseline_file[256];
} compatibility_system_t;

extern compatibility_system_t compatibility_system;

// Compatibility Testing API
qboolean Compatibility_Init(void);
void Compatibility_Shutdown(void);

// Platform Detection
qboolean Compatibility_DetectPlatform(compatibility_platform_info_t* info);
qboolean Compatibility_DetectHardware(hardware_capabilities_t* caps);
const char* Compatibility_GetPlatformName(platform_type_t platform);
const char* Compatibility_GetArchitectureName(architecture_type_t arch);
const char* Compatibility_GetCompilerName(compiler_type_t compiler);

// Feature Detection
qboolean Compatibility_DetectFeatureSupport(void);
qboolean Compatibility_AddFeatureCheck(feature_category_t category,
                                     const char* feature_name,
                                     const char* requirement_desc,
                                     qboolean is_required);
qboolean Compatibility_UpdateFeatureStatus(const char* feature_name,
                                         compatibility_level_t level,
                                         qboolean available,
                                         const char* limitations);

// Compatibility Testing
qboolean Compatibility_RunPlatformTests(void);
qboolean Compatibility_RunHardwareTests(void);
qboolean Compatibility_RunFeatureTests(void);
qboolean Compatibility_RunComprehensiveTest(void);

// Result Management
uint32_t Compatibility_GetResults(compatibility_result_t** results);
const compatibility_result_t* Compatibility_GetResult(const char* test_name);
qboolean Compatibility_SaveResults(const char* filename);
qboolean Compatibility_LoadResults(const char* filename);

// Validation and Verification
compatibility_level_t Compatibility_GetOverallCompatibility(void);
qboolean Compatibility_IsPlatformSupported(void);
qboolean Compatibility_IsHardwareAdequate(void);
qboolean Compatibility_CheckMinimumRequirements(void);
qboolean Compatibility_GenerateCompatibilityReport(const char* output_file);

// Utility Functions
qboolean Compatibility_PrintPlatformInfo(void);
qboolean Compatibility_PrintHardwareInfo(void);
qboolean Compatibility_PrintFeatureMatrix(void);
qboolean Compatibility_PrintTestResults(void);
const char* Compatibility_GetCompatibilityLevelString(compatibility_level_t level);
qboolean Compatibility_ValidateConfiguration(void);

// Hardware-specific tests
qboolean Compatibility_TestCPUCapabilities(void);
qboolean Compatibility_TestGPUCapabilities(void);
qboolean Compatibility_TestMemoryCapabilities(void);
qboolean Compatibility_TestStorageCapabilities(void);
qboolean Compatibility_TestNetworkCapabilities(void);

// Platform-specific tests
qboolean Compatibility_TestPlatformAPIs(void);
qboolean Compatibility_TestPlatformLimits(void);
qboolean Compatibility_TestPlatformPerformance(void);

// Automated compatibility verification
qboolean Compatibility_VerifyRendererCompatibility(void);
qboolean Compatibility_VerifyAudioCompatibility(void);
qboolean Compatibility_VerifyInputCompatibility(void);
qboolean Compatibility_VerifyNetworkCompatibility(void);

// Benchmark integration
qboolean Compatibility_RunCompatibilityBenchmark(void);
qboolean Compatibility_CompareAgainstBaseline(void);

// Console commands
void Compatibility_Status_f(void);
void Compatibility_Test_f(void);
void Compatibility_Report_f(void);
void Compatibility_Benchmark_f(void);

#endif // __COMPATIBILITY_TEST_H__
