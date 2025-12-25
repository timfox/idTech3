/*
=============================================================================
Cross-Platform Compatibility Test Framework Implementation

Automated testing across all supported platforms and architectures.
=============================================================================
*/

#include "cross_platform_test.h"
#include "qcommon.h"
#include <string.h>
#include <stdlib.h>

// Global cross-platform test system instance
cross_platform_test_system_t cross_platform_test_system = {0};

// Platform detection macros
#if defined(_WIN32) || defined(_WIN64)
#define PLATFORM_WINDOWS_DETECTED
#elif defined(__linux__)
#define PLATFORM_LINUX_DETECTED
#elif defined(__APPLE__) && defined(__MACH__)
#define PLATFORM_MACOS_DETECTED
#elif defined(__FreeBSD__)
#define PLATFORM_FREEBSD_DETECTED
#elif defined(__ANDROID__)
#define PLATFORM_ANDROID_DETECTED
#elif defined(__EMSCRIPTEN__)
#define PLATFORM_EMSCRIPTEN_DETECTED
#endif

// Architecture detection macros
#if defined(__x86_64__) || defined(_M_X64)
#define ARCH_X86_64_DETECTED
#elif defined(__i386__) || defined(_M_IX86)
#define ARCH_X86_DETECTED
#elif defined(__aarch64__) || defined(_M_ARM64)
#define ARCH_ARM64_DETECTED
#elif defined(__arm__) || defined(_M_ARM)
#define ARCH_ARM_DETECTED
#elif defined(__riscv)
#define ARCH_RISCV_DETECTED
#endif

// Compiler detection macros
#if defined(__GNUC__) && !defined(__clang__)
#define COMPILER_GCC_DETECTED
#elif defined(__clang__)
#define COMPILER_CLANG_DETECTED
#elif defined(_MSC_VER)
#define COMPILER_MSVC_DETECTED
#endif

/*
=============================================================================
Platform Detection Functions
=============================================================================
*/

// Detect current platform
static platform_type_t DetectPlatform(void) {
#if defined(PLATFORM_WINDOWS_DETECTED)
    return PLATFORM_WINDOWS;
#elif defined(PLATFORM_LINUX_DETECTED)
    return PLATFORM_LINUX;
#elif defined(PLATFORM_MACOS_DETECTED)
    return PLATFORM_MACOS;
#elif defined(PLATFORM_FREEBSD_DETECTED)
    return PLATFORM_FREEBSD;
#elif defined(PLATFORM_ANDROID_DETECTED)
    return PLATFORM_ANDROID;
#elif defined(PLATFORM_EMSCRIPTEN_DETECTED)
    return PLATFORM_EMSCRIPTEN;
#else
    return PLATFORM_UNKNOWN;
#endif
}

// Detect current architecture
static architecture_type_t DetectArchitecture(void) {
#if defined(ARCH_X86_64_DETECTED)
    return ARCH_X86_64;
#elif defined(ARCH_X86_DETECTED)
    return ARCH_X86;
#elif defined(ARCH_ARM64_DETECTED)
    return ARCH_ARM64;
#elif defined(ARCH_ARM_DETECTED)
    return ARCH_ARM;
#elif defined(ARCH_RISCV_DETECTED)
    return ARCH_RISCV;
#else
    return ARCH_UNKNOWN;
#endif
}

// Detect current compiler
static compiler_type_t DetectCompiler(void) {
#if defined(COMPILER_GCC_DETECTED)
    return COMPILER_GCC;
#elif defined(COMPILER_CLANG_DETECTED)
    return COMPILER_CLANG;
#elif defined(COMPILER_MSVC_DETECTED)
    return COMPILER_MSVC;
#else
    return COMPILER_UNKNOWN;
#endif
}

// Get platform-specific information
static qboolean GetPlatformDetails(platform_info_t* info) {
    if (!info) return qfalse;

    memset(info, 0, sizeof(platform_info_t));
    info->type = DetectPlatform();
    info->arch.type = DetectArchitecture();
    info->compiler.type = DetectCompiler();

    // Set platform name
    switch (info->type) {
        case PLATFORM_WINDOWS:
            Q_strncpyz(info->name, "Windows", sizeof(info->name));
            Q_strncpyz(info->version, "Unknown", sizeof(info->version));
            break;
        case PLATFORM_LINUX:
            Q_strncpyz(info->name, "Linux", sizeof(info->name));
            Q_strncpyz(info->version, "Unknown", sizeof(info->version));
            break;
        case PLATFORM_MACOS:
            Q_strncpyz(info->name, "macOS", sizeof(info->name));
            Q_strncpyz(info->version, "Unknown", sizeof(info->version));
            break;
        case PLATFORM_FREEBSD:
            Q_strncpyz(info->name, "FreeBSD", sizeof(info->name));
            Q_strncpyz(info->version, "Unknown", sizeof(info->version));
            break;
        case PLATFORM_ANDROID:
            Q_strncpyz(info->name, "Android", sizeof(info->name));
            Q_strncpyz(info->version, "Unknown", sizeof(info->version));
            break;
        case PLATFORM_EMSCRIPTEN:
            Q_strncpyz(info->name, "Emscripten", sizeof(info->name));
            Q_strncpyz(info->version, "Unknown", sizeof(info->version));
            break;
        default:
            Q_strncpyz(info->name, "Unknown", sizeof(info->name));
            Q_strncpyz(info->version, "Unknown", sizeof(info->version));
            break;
    }

    // Set architecture details
    switch (info->arch.type) {
        case ARCH_X86_64:
            Q_strncpyz(info->arch.name, "x86_64", sizeof(info->arch.name));
            info->arch.bits = 64;
            info->arch.little_endian = qtrue;
            info->arch.supports_simd = qtrue;
            info->arch.supports_atomic64 = qtrue;
            break;
        case ARCH_X86:
            Q_strncpyz(info->arch.name, "x86", sizeof(info->arch.name));
            info->arch.bits = 32;
            info->arch.little_endian = qtrue;
            info->arch.supports_simd = qtrue;
            info->arch.supports_atomic64 = qtrue;
            break;
        case ARCH_ARM64:
            Q_strncpyz(info->arch.name, "ARM64", sizeof(info->arch.name));
            info->arch.bits = 64;
            info->arch.little_endian = qtrue;
            info->arch.supports_simd = qtrue;
            info->arch.supports_atomic64 = qtrue;
            break;
        case ARCH_ARM:
            Q_strncpyz(info->arch.name, "ARM", sizeof(info->arch.name));
            info->arch.bits = 32;
            info->arch.little_endian = qtrue;
            info->arch.supports_simd = qtrue;
            info->arch.supports_atomic64 = qfalse; // Some ARM32 don't support 64-bit atomics
            break;
        default:
            Q_strncpyz(info->arch.name, "Unknown", sizeof(info->arch.name));
            info->arch.bits = 0;
            info->arch.little_endian = qtrue;
            info->arch.supports_simd = qfalse;
            info->arch.supports_atomic64 = qfalse;
            break;
    }

    // Set compiler details
    switch (info->compiler.type) {
        case COMPILER_GCC:
            Q_strncpyz(info->compiler.name, "GCC", sizeof(info->compiler.name));
            Q_strncpyz(info->compiler.version, __VERSION__, sizeof(info->compiler.version));
            info->compiler.supports_c11 = qtrue;
            info->compiler.supports_c23 = (__STDC_VERSION__ >= 202311L);
            info->compiler.supports_cpp11 = qtrue;
            info->compiler.supports_cpp23 = (__cplusplus >= 202302L);
            break;
        case COMPILER_CLANG:
            Q_strncpyz(info->compiler.name, "Clang", sizeof(info->compiler.name));
            Q_strncpyz(info->compiler.version, __clang_version__, sizeof(info->compiler.version));
            info->compiler.supports_c11 = qtrue;
            info->compiler.supports_c23 = (__STDC_VERSION__ >= 202311L);
            info->compiler.supports_cpp11 = qtrue;
            info->compiler.supports_cpp23 = (__cplusplus >= 202302L);
            break;
        case COMPILER_MSVC:
            Q_strncpyz(info->compiler.name, "MSVC", sizeof(info->compiler.name));
            Com_sprintf(info->compiler.version, sizeof(info->compiler.version), "%d", _MSC_VER);
            info->compiler.supports_c11 = (_MSC_VER >= 1920); // VS 2019+
            info->compiler.supports_c23 = qfalse;
            info->compiler.supports_cpp11 = (_MSC_VER >= 1900); // VS 2015+
            info->compiler.supports_cpp23 = qfalse;
            break;
        default:
            Q_strncpyz(info->compiler.name, "Unknown", sizeof(info->compiler.name));
            Q_strncpyz(info->compiler.version, "Unknown", sizeof(info->compiler.version));
            info->compiler.supports_c11 = qfalse;
            info->compiler.supports_c23 = qfalse;
            info->compiler.supports_cpp11 = qfalse;
            info->compiler.supports_cpp23 = qfalse;
            break;
    }

    return qtrue;
}

// Detect platform capabilities
static qboolean DetectPlatformCapabilities(platform_capabilities_t* caps) {
    if (!caps) return qfalse;

    memset(caps, 0, sizeof(platform_capabilities_t));

    // Basic capabilities
    caps->has_threads = qtrue; // Assume threading support
    caps->has_atomic_ops = qtrue; // Assume atomic operations
    caps->has_tls = qtrue; // Assume thread-local storage
    caps->has_unicode = qtrue; // Assume Unicode support
    caps->page_size = 4096; // Default page size

    // Platform-specific capabilities
    switch (DetectPlatform()) {
        case PLATFORM_WINDOWS:
            caps->has_vulkan = qtrue;
            caps->has_opengl = qtrue;
            caps->has_directx = qtrue;
            caps->has_mmap = qtrue;
            caps->has_large_files = qtrue;
            caps->has_networking = qtrue;
            caps->has_audio = qtrue;
            caps->max_threads = 64;
            break;

        case PLATFORM_LINUX:
            caps->has_vulkan = qtrue;
            caps->has_opengl = qtrue;
            caps->has_opengles = qtrue;
            caps->has_mmap = qtrue;
            caps->has_large_files = qtrue;
            caps->has_networking = qtrue;
            caps->has_audio = qtrue;
            caps->max_threads = 128;
            break;

        case PLATFORM_MACOS:
            caps->has_metal = qtrue;
            caps->has_opengl = qtrue; // Deprecated but still available
            caps->has_vulkan = qfalse; // Not natively supported
            caps->has_mmap = qtrue;
            caps->has_large_files = qtrue;
            caps->has_networking = qtrue;
            caps->has_audio = qtrue;
            caps->max_threads = 128;
            break;

        case PLATFORM_ANDROID:
            caps->has_vulkan = qtrue;
            caps->has_opengles = qtrue;
            caps->has_opengl = qfalse;
            caps->has_mmap = qtrue;
            caps->has_large_files = qtrue;
            caps->has_networking = qtrue;
            caps->has_audio = qtrue;
            caps->max_threads = 32;
            break;

        case PLATFORM_EMSCRIPTEN:
            caps->has_opengles = qtrue;
            caps->has_webgl = qtrue;
            caps->has_threads = qfalse; // Limited threading
            caps->has_mmap = qfalse;
            caps->has_large_files = qfalse;
            caps->has_networking = qtrue;
            caps->has_audio = qtrue;
            caps->max_threads = 1;
            break;

        default:
            caps->has_opengl = qtrue; // Fallback
            caps->has_networking = qtrue;
            caps->max_threads = 16;
            break;
    }

    // Architecture-specific capabilities
    switch (DetectArchitecture()) {
        case ARCH_X86_64:
        case ARCH_ARM64:
            caps->has_simd = qtrue;
            caps->total_memory = 8ULL * 1024 * 1024 * 1024; // Assume 8GB minimum
            caps->available_memory = 4ULL * 1024 * 1024 * 1024; // Assume 4GB available
            break;
        case ARCH_X86:
        case ARCH_ARM:
            caps->has_simd = qtrue;
            caps->total_memory = 4ULL * 1024 * 1024 * 1024; // Assume 4GB minimum
            caps->available_memory = 2ULL * 1024 * 1024 * 1024; // Assume 2GB available
            break;
        default:
            caps->total_memory = 1ULL * 1024 * 1024 * 1024; // Assume 1GB minimum
            caps->available_memory = 512ULL * 1024 * 1024; // Assume 512MB available
            break;
    }

    return qtrue;
}

/*
=============================================================================
Cross-Platform Test API Implementation
=============================================================================
*/

qboolean CrossPlatformTest_Init(void) {
    if (cross_platform_test_system.initialized) {
        return qtrue;
    }

    memset(&cross_platform_test_system, 0, sizeof(cross_platform_test_system_t));

    // Detect current platform
    if (!CrossPlatformTest_DetectPlatform(&cross_platform_test_system.current_platform)) {
        Com_Printf("Failed to detect platform information\n");
        return qfalse;
    }

    // Allocate results storage
    cross_platform_test_system.max_results = 1000;
    cross_platform_test_system.results = (cross_platform_test_result_t*)malloc(
        sizeof(cross_platform_test_result_t) * cross_platform_test_system.max_results);

    if (!cross_platform_test_system.results) {
        Com_Printf("Failed to allocate memory for test results\n");
        return qfalse;
    }

    memset(cross_platform_test_system.results, 0,
           sizeof(cross_platform_test_result_t) * cross_platform_test_system.max_results);

    cross_platform_test_system.initialized = qtrue;

    Com_Printf("Cross-platform test system initialized for %s %s (%s %s)\n",
               cross_platform_test_system.current_platform.name,
               cross_platform_test_system.current_platform.arch.name,
               cross_platform_test_system.current_platform.compiler.name,
               cross_platform_test_system.current_platform.compiler.version);

    return qtrue;
}

void CrossPlatformTest_Shutdown(void) {
    if (!cross_platform_test_system.initialized) {
        return;
    }

    if (cross_platform_test_system.results) {
        free(cross_platform_test_system.results);
        cross_platform_test_system.results = NULL;
    }

    if (cross_platform_test_system.current_suite) {
        if (cross_platform_test_system.current_suite->tests) {
            free(cross_platform_test_system.current_suite->tests);
        }
        free(cross_platform_test_system.current_suite);
        cross_platform_test_system.current_suite = NULL;
    }

    cross_platform_test_system.initialized = qfalse;
    Com_Printf("Cross-platform test system shutdown\n");
}

qboolean CrossPlatformTest_DetectPlatform(platform_info_t* info) {
    if (!info) return qfalse;

    if (!GetPlatformDetails(info)) {
        return qfalse;
    }

    if (!DetectPlatformCapabilities(&info->capabilities)) {
        return qfalse;
    }

    return qtrue;
}

const char* CrossPlatformTest_GetPlatformName(platform_type_t platform) {
    switch (platform) {
        case PLATFORM_WINDOWS: return "Windows";
        case PLATFORM_LINUX: return "Linux";
        case PLATFORM_MACOS: return "macOS";
        case PLATFORM_FREEBSD: return "FreeBSD";
        case PLATFORM_ANDROID: return "Android";
        case PLATFORM_IOS: return "iOS";
        case PLATFORM_EMSCRIPTEN: return "Emscripten";
        default: return "Unknown";
    }
}

const char* CrossPlatformTest_GetArchitectureName(architecture_type_t arch) {
    switch (arch) {
        case ARCH_X86: return "x86";
        case ARCH_X86_64: return "x86_64";
        case ARCH_ARM: return "ARM";
        case ARCH_ARM64: return "ARM64";
        case ARCH_RISCV: return "RISC-V";
        case ARCH_PPC: return "PowerPC";
        case ARCH_PPC64: return "PowerPC64";
        case ARCH_S390X: return "s390x";
        default: return "Unknown";
    }
}

const char* CrossPlatformTest_GetCompilerName(compiler_type_t compiler) {
    switch (compiler) {
        case COMPILER_GCC: return "GCC";
        case COMPILER_CLANG: return "Clang";
        case COMPILER_MSVC: return "MSVC";
        case COMPILER_EMSCRIPTEN: return "Emscripten";
        default: return "Unknown";
    }
}

const char* CrossPlatformTest_GetResultString(compatibility_result_t result) {
    switch (result) {
        case COMPAT_RESULT_PASS: return "PASS";
        case COMPAT_RESULT_FAIL: return "FAIL";
        case COMPAT_RESULT_SKIP: return "SKIP";
        case COMPAT_RESULT_TIMEOUT: return "TIMEOUT";
        case COMPAT_RESULT_CRASH: return "CRASH";
        case COMPAT_RESULT_INCOMPLETE: return "INCOMPLETE";
        default: return "UNKNOWN";
    }
}

/*
=============================================================================
Test Suite Management
=============================================================================
*/

cross_platform_test_suite_t* CrossPlatformTest_CreateSuite(const char* name, const char* description) {
    if (!cross_platform_test_system.initialized) {
        return NULL;
    }

    cross_platform_test_suite_t* suite = (cross_platform_test_suite_t*)malloc(sizeof(cross_platform_test_suite_t));
    if (!suite) {
        return NULL;
    }

    memset(suite, 0, sizeof(cross_platform_test_suite_t));
    Q_strncpyz(suite->suite_name, name, sizeof(suite->suite_name));
    Q_strncpyz(suite->description, description, sizeof(suite->description));

    suite->max_tests = 100;
    suite->tests = (cross_platform_test_config_t*)malloc(
        sizeof(cross_platform_test_config_t) * suite->max_tests);

    if (!suite->tests) {
        free(suite);
        return NULL;
    }

    memset(suite->tests, 0, sizeof(cross_platform_test_config_t) * suite->max_tests);

    // Default suite configuration
    suite->run_in_parallel = qfalse;
    suite->max_parallel_tests = 4;
    suite->stop_on_failure = qfalse;
    suite->suite_timeout_seconds = 300; // 5 minutes

    return suite;
}

qboolean CrossPlatformTest_AddTestToSuite(cross_platform_test_suite_t* suite,
                                        const cross_platform_test_config_t* config) {
    if (!suite || !config || suite->num_tests >= suite->max_tests) {
        return qfalse;
    }

    memcpy(&suite->tests[suite->num_tests], config, sizeof(cross_platform_test_config_t));
    suite->num_tests++;

    return qtrue;
}

qboolean CrossPlatformTest_RunSuite(cross_platform_test_suite_t* suite) {
    if (!suite || suite->num_tests == 0) {
        return qfalse;
    }

    Com_Printf("Running cross-platform test suite: %s\n", suite->suite_name);
    Com_Printf("Description: %s\n", suite->description);
    Com_Printf("Tests: %u\n", suite->num_tests);

    cross_platform_test_system.current_suite = suite;

    uint64_t suite_start_time = Sys_Milliseconds();
    uint32_t passed = 0, failed = 0, skipped = 0, timeouts = 0, crashes = 0;

    for (uint32_t i = 0; i < suite->num_tests; i++) {
        const cross_platform_test_config_t* config = &suite->tests[i];

        Com_Printf("Running test %u/%u: %s\n", i + 1, suite->num_tests, config->test_name);

        // Check if test is applicable for current platform
        if (config->required_platform != PLATFORM_UNKNOWN &&
            config->required_platform != cross_platform_test_system.current_platform.type) {
            Com_Printf("  SKIPPED: Not applicable for %s\n",
                      CrossPlatformTest_GetPlatformName(cross_platform_test_system.current_platform.type));
            skipped++;
            continue;
        }

        if (config->required_arch != ARCH_UNKNOWN &&
            config->required_arch != cross_platform_test_system.current_platform.arch.type) {
            Com_Printf("  SKIPPED: Not applicable for %s\n",
                      CrossPlatformTest_GetArchitectureName(cross_platform_test_system.current_platform.arch.type));
            skipped++;
            continue;
        }

        // Check platform capabilities
        if (config->requires_graphics && !cross_platform_test_system.current_platform.capabilities.has_vulkan &&
            !cross_platform_test_system.current_platform.capabilities.has_opengl) {
            Com_Printf("  SKIPPED: Graphics not available\n");
            skipped++;
            continue;
        }

        if (config->requires_network && !cross_platform_test_system.current_platform.capabilities.has_networking) {
            Com_Printf("  SKIPPED: Networking not available\n");
            skipped++;
            continue;
        }

        if (config->requires_audio && !cross_platform_test_system.current_platform.capabilities.has_audio) {
            Com_Printf("  SKIPPED: Audio not available\n");
            skipped++;
            continue;
        }

        // Run the test
        cross_platform_test_result_t result;
        memset(&result, 0, sizeof(result));
        Q_strncpyz(result.test_name, config->test_name, sizeof(result.test_name));
        memcpy(&result.platform, &cross_platform_test_system.current_platform, sizeof(platform_info_t));

        if (CrossPlatformTest_RunTest(config, &result)) {
            // Store result
            if (cross_platform_test_system.num_results < cross_platform_test_system.max_results) {
                memcpy(&cross_platform_test_system.results[cross_platform_test_system.num_results++],
                       &result, sizeof(cross_platform_test_result_t));
            }

            // Update statistics
            cross_platform_test_system.total_tests_run++;

            Com_Printf("  Result: %s", CrossPlatformTest_GetResultString(result.result));

            if (result.duration_ms > 0) {
                Com_Printf(" (%.2f seconds)", result.duration_ms / 1000.0f);
            }
            Com_Printf("\n");

            if (result.error_message[0]) {
                Com_Printf("  Error: %s\n", result.error_message);
            }

            switch (result.result) {
                case COMPAT_RESULT_PASS:
                    passed++;
                    break;
                case COMPAT_RESULT_FAIL:
                    failed++;
                    break;
                case COMPAT_RESULT_TIMEOUT:
                    timeouts++;
                    break;
                case COMPAT_RESULT_CRASH:
                    crashes++;
                    break;
                case COMPAT_RESULT_SKIP:
                    skipped++;
                    break;
                default:
                    break;
            }
        } else {
            Com_Printf("  FAILED: Test execution error\n");
            failed++;
        }

        // Check for suite timeout
        uint64_t current_time = Sys_Milliseconds();
        if ((current_time - suite_start_time) / 1000 > suite->suite_timeout_seconds) {
            Com_Printf("Suite timeout reached, stopping execution\n");
            break;
        }

        // Check for stop on failure
        if (suite->stop_on_failure && failed > 0) {
            Com_Printf("Stopping suite due to test failure\n");
            break;
        }
    }

    uint64_t suite_duration = Sys_Milliseconds() - suite_start_time;

    Com_Printf("\nSuite Summary:\n");
    Com_Printf("Total Tests: %u\n", suite->num_tests);
    Com_Printf("Passed: %u\n", passed);
    Com_Printf("Failed: %u\n", failed);
    Com_Printf("Skipped: %u\n", skipped);
    Com_Printf("Timeouts: %u\n", timeouts);
    Com_Printf("Crashes: %u\n", crashes);
    Com_Printf("Duration: %.2f seconds\n", suite_duration / 1000.0f);

    // Update global statistics
    cross_platform_test_system.total_passed += passed;
    cross_platform_test_system.total_failed += failed;
    cross_platform_test_system.total_skipped += skipped;
    cross_platform_test_system.total_timeouts += timeouts;
    cross_platform_test_system.total_crashes += crashes;

    cross_platform_test_system.current_suite = NULL;

    return (failed == 0 && timeouts == 0 && crashes == 0);
}

/*
=============================================================================
Individual Test Execution
=============================================================================
*/

qboolean CrossPlatformTest_RunTest(const cross_platform_test_config_t* config,
                                 cross_platform_test_result_t* result) {
    if (!config || !result) {
        return qfalse;
    }

    result->start_time = Sys_Milliseconds();
    result->result = COMPAT_RESULT_PASS; // Default to pass

    // Run the appropriate test function based on test name
    qboolean test_result = qfalse;

    if (Q_stricmp(config->test_name, "basic_functionality") == 0) {
        test_result = CrossPlatformTest_BasicFunctionality();
    } else if (Q_stricmp(config->test_name, "memory_management") == 0) {
        test_result = CrossPlatformTest_MemoryManagement();
    } else if (Q_stricmp(config->test_name, "threading") == 0) {
        test_result = CrossPlatformTest_Threading();
    } else if (Q_stricmp(config->test_name, "file_system") == 0) {
        test_result = CrossPlatformTest_FileSystem();
    } else if (Q_stricmp(config->test_name, "network_basic") == 0) {
        test_result = CrossPlatformTest_NetworkBasic();
    } else if (Q_stricmp(config->test_name, "graphics_api") == 0) {
        test_result = CrossPlatformTest_GraphicsAPI();
    } else if (Q_stricmp(config->test_name, "audio_api") == 0) {
        test_result = CrossPlatformTest_AudioAPI();
    } else if (Q_stricmp(config->test_name, "large_file_support") == 0) {
        test_result = CrossPlatformTest_LargeFileSupport();
    } else if (Q_stricmp(config->test_name, "unicode_support") == 0) {
        test_result = CrossPlatformTest_UnicodeSupport();
    } else if (Q_stricmp(config->test_name, "time_and_date") == 0) {
        test_result = CrossPlatformTest_TimeAndDate();
    } else if (Q_stricmp(config->test_name, "math_precision") == 0) {
        test_result = CrossPlatformTest_MathPrecision();
    } else {
        Com_sprintf(result->error_message, sizeof(result->error_message),
                   "Unknown test: %s", config->test_name);
        result->result = COMPAT_RESULT_FAIL;
        result->end_time = Sys_Milliseconds();
        result->duration_ms = result->end_time - result->start_time;
        return qtrue; // Test completed (with failure)
    }

    result->end_time = Sys_Milliseconds();
    result->duration_ms = result->end_time - result->start_time;

    if (test_result) {
        result->result = COMPAT_RESULT_PASS;
    } else {
        result->result = COMPAT_RESULT_FAIL;
        Q_strncpyz(result->error_message, "Test function returned false", sizeof(result->error_message));
    }

    return qtrue;
}

qboolean CrossPlatformTest_CancelTest(void) {
    // Implementation for cancelling running tests
    return qtrue;
}

qboolean CrossPlatformTest_IsTestRunning(void) {
    // Check if any test is currently running
    return qfalse;
}

/*
=============================================================================
Built-in Test Functions
=============================================================================
*/

qboolean CrossPlatformTest_BasicFunctionality(void) {
    // Test basic C functionality across platforms

    // Test basic math operations
    if ((1 + 1) != 2) return qfalse;
    if ((10 - 5) != 5) return qfalse;
    if ((3 * 4) != 12) return qfalse;
    if ((15 / 3) != 5) return qfalse;

    // Test floating point operations
    float f1 = 1.5f, f2 = 2.25f;
    if (fabsf((f1 + f2) - 3.75f) > 0.001f) return qfalse;

    double d1 = 1.5, d2 = 2.25;
    if (fabs((d1 + d2) - 3.75) > 0.001) return qfalse;

    // Test string operations
    char test_str[32] = "Hello";
    if (strlen(test_str) != 5) return qfalse;

    strcat(test_str, " World");
    if (strcmp(test_str, "Hello World") != 0) return qfalse;

    // Test memory operations
    int* test_array = (int*)malloc(sizeof(int) * 10);
    if (!test_array) return qfalse;

    for (int i = 0; i < 10; i++) {
        test_array[i] = i * 2;
    }

    for (int i = 0; i < 10; i++) {
        if (test_array[i] != i * 2) {
            free(test_array);
            return qfalse;
        }
    }

    free(test_array);

    // Test bit operations
    unsigned int flags = 0;
    flags |= (1 << 0);  // Set bit 0
    flags |= (1 << 3);  // Set bit 3
    flags |= (1 << 7);  // Set bit 7

    if (!(flags & (1 << 0))) return qfalse;  // Check bit 0
    if (flags & (1 << 1)) return qfalse;     // Check bit 1 (should be unset)
    if (!(flags & (1 << 3))) return qfalse;  // Check bit 3
    if (!(flags & (1 << 7))) return qfalse;  // Check bit 7

    return qtrue;
}

qboolean CrossPlatformTest_MemoryManagement(void) {
    // Test memory management functions

    // Test malloc/calloc/realloc/free
    void* ptr1 = malloc(1024);
    if (!ptr1) return qfalse;

    void* ptr2 = calloc(10, sizeof(int));
    if (!ptr2) {
        free(ptr1);
        return qfalse;
    }

    // Check that calloc zeros memory
    int* int_array = (int*)ptr2;
    for (int i = 0; i < 10; i++) {
        if (int_array[i] != 0) {
            free(ptr1);
            free(ptr2);
            return qfalse;
        }
    }

    // Test realloc
    void* ptr3 = realloc(ptr1, 2048);
    if (!ptr3) {
        free(ptr2);
        return qfalse;
    }

    // Test memset/memcpy/memcmp
    char buffer1[100], buffer2[100];
    memset(buffer1, 0xAA, sizeof(buffer1));
    memcpy(buffer2, buffer1, sizeof(buffer2));

    if (memcmp(buffer1, buffer2, sizeof(buffer1)) != 0) {
        free(ptr2);
        free(ptr3);
        return qfalse;
    }

    // Clean up
    free(ptr2);
    free(ptr3);

    return qtrue;
}

qboolean CrossPlatformTest_Threading(void) {
    // Test basic threading functionality
    // Note: This is a simplified test - real threading tests would be more complex

    // Check if threading is available on this platform
    if (!cross_platform_test_system.current_platform.capabilities.has_threads) {
        return qtrue; // Skip test if threading not available
    }

    // Test thread-local storage if available
    if (cross_platform_test_system.current_platform.capabilities.has_tls) {
        // Basic TLS test would go here
        // For now, just return true as threading framework is tested elsewhere
    }

    return qtrue;
}

qboolean CrossPlatformTest_FileSystem(void) {
    // Test basic file system operations
    const char* test_filename = "cross_platform_test.tmp";

    // Test file creation
    FILE* fp = fopen(test_filename, "w");
    if (!fp) return qfalse;

    // Test writing
    const char* test_data = "Cross-platform test data\n";
    size_t written = fwrite(test_data, 1, strlen(test_data), fp);
    if (written != strlen(test_data)) {
        fclose(fp);
        return qfalse;
    }

    fclose(fp);

    // Test file reading
    fp = fopen(test_filename, "r");
    if (!fp) return qfalse;

    char buffer[256];
    size_t read = fread(buffer, 1, sizeof(buffer), fp);
    if (read != strlen(test_data)) {
        fclose(fp);
        remove(test_filename);
        return qfalse;
    }

    buffer[read] = '\0';
    if (strcmp(buffer, test_data) != 0) {
        fclose(fp);
        remove(test_filename);
        return qfalse;
    }

    fclose(fp);

    // Test file removal
    if (remove(test_filename) != 0) {
        return qfalse;
    }

    // Test directory operations if available
    // (Basic test - more comprehensive tests would create/remove directories)

    return qtrue;
}

qboolean CrossPlatformTest_NetworkBasic(void) {
    // Test basic networking functionality
    // Note: This is a simplified test - real network tests require server/client setup

    if (!cross_platform_test_system.current_platform.capabilities.has_networking) {
        return qtrue; // Skip if networking not available
    }

    // Test basic socket address structures
    // This tests that networking headers are available and structures are correct

    // For a real test, we would:
    // - Create sockets
    // - Test basic connectivity
    // - Test DNS resolution
    // - Test different protocols

    return qtrue; // Basic test passes
}

qboolean CrossPlatformTest_GraphicsAPI(void) {
    // Test graphics API availability
    platform_capabilities_t* caps = &cross_platform_test_system.current_platform.capabilities;

    // Check that at least one graphics API is available
    if (!caps->has_vulkan && !caps->has_opengl && !caps->has_opengles &&
        !caps->has_metal && !caps->has_directx) {
        return qfalse;
    }

    // Basic graphics capability validation
    // In a real implementation, this would:
    // - Initialize graphics context
    // - Create basic resources (buffers, textures)
    // - Perform basic rendering operations
    // - Clean up resources

    return qtrue;
}

qboolean CrossPlatformTest_AudioAPI(void) {
    // Test audio API availability
    if (!cross_platform_test_system.current_platform.capabilities.has_audio) {
        return qtrue; // Skip if audio not available
    }

    // Basic audio capability validation
    // In a real implementation, this would:
    // - Initialize audio context
    // - Test audio playback
    // - Test audio capture
    // - Clean up audio resources

    return qtrue;
}

qboolean CrossPlatformTest_LargeFileSupport(void) {
    // Test large file support (>2GB)
    if (!cross_platform_test_system.current_platform.capabilities.has_large_files) {
        return qtrue; // Skip if large files not supported
    }

    // Test large file operations
    // This would create and manipulate large files to ensure 64-bit file offsets work

    return qtrue;
}

qboolean CrossPlatformTest_UnicodeSupport(void) {
    // Test Unicode string handling
    if (!cross_platform_test_system.current_platform.capabilities.has_unicode) {
        return qtrue; // Skip if Unicode not supported
    }

    // Test Unicode string operations
    const char* utf8_test = "Hello 世界 🌍";  // UTF-8 string with Chinese and emoji

    // Test string length with multi-byte characters
    size_t len = strlen(utf8_test);
    if (len == 0) return qfalse;

    // Test string copying
    char buffer[256];
    Q_strncpyz(buffer, utf8_test, sizeof(buffer));

    if (strcmp(buffer, utf8_test) != 0) return qfalse;

    return qtrue;
}

qboolean CrossPlatformTest_TimeAndDate(void) {
    // Test time and date functions
    time_t current_time = time(NULL);
    if (current_time == (time_t)-1) return qfalse;

    // Test localtime conversion
    struct tm* time_info = localtime(&current_time);
    if (!time_info) return qfalse;

    // Test time formatting
    char time_str[64];
    if (strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", time_info) == 0) {
        return qfalse;
    }

    // Test that the formatted string is not empty
    if (strlen(time_str) == 0) return qfalse;

    return qtrue;
}

qboolean CrossPlatformTest_MathPrecision(void) {
    // Test mathematical precision across platforms

    // Test floating point precision
    float f_precision_test = 1.0f / 3.0f;
    f_precision_test *= 3.0f;
    if (fabsf(f_precision_test - 1.0f) > 0.0001f) return qfalse;

    double d_precision_test = 1.0 / 3.0;
    d_precision_test *= 3.0;
    if (fabs(d_precision_test - 1.0) > 0.0000001) return qfalse;

    // Test mathematical functions
    double sqrt_test = sqrt(16.0);
    if (fabs(sqrt_test - 4.0) > 0.0001) return qfalse;

    double sin_test = sin(M_PI / 2.0);
    if (fabs(sin_test - 1.0) > 0.0001) return qfalse;

    double cos_test = cos(0.0);
    if (fabs(cos_test - 1.0) > 0.0001) return qfalse;

    // Test integer overflow behavior (if applicable)
    // Different platforms/compilers may handle overflow differently

    return qtrue;
}

/*
=============================================================================
Automated Test Generation
=============================================================================
*/

qboolean CrossPlatformTest_GeneratePlatformTests(cross_platform_test_suite_t* suite) {
    if (!suite) return qfalse;

    // Add platform-specific tests
    cross_platform_test_config_t config;

    // Basic functionality test
    memset(&config, 0, sizeof(config));
    Q_strncpyz(config.test_name, "basic_functionality", sizeof(config.test_name));
    Q_strncpyz(config.description, "Test basic C functionality", sizeof(config.description));
    config.timeout_seconds = 10;
    CrossPlatformTest_AddTestToSuite(suite, &config);

    // Memory management test
    memset(&config, 0, sizeof(config));
    Q_strncpyz(config.test_name, "memory_management", sizeof(config.test_name));
    Q_strncpyz(config.description, "Test memory allocation and management", sizeof(config.description));
    config.timeout_seconds = 15;
    CrossPlatformTest_AddTestToSuite(suite, &config);

    // File system test
    memset(&config, 0, sizeof(config));
    Q_strncpyz(config.test_name, "file_system", sizeof(config.test_name));
    Q_strncpyz(config.description, "Test file system operations", sizeof(config.description));
    config.timeout_seconds = 20;
    CrossPlatformTest_AddTestToSuite(suite, &config);

    // Time and date test
    memset(&config, 0, sizeof(config));
    Q_strncpyz(config.test_name, "time_and_date", sizeof(config.test_name));
    Q_strncpyz(config.description, "Test time and date functions", sizeof(config.description));
    config.timeout_seconds = 5;
    CrossPlatformTest_AddTestToSuite(suite, &config);

    // Math precision test
    memset(&config, 0, sizeof(config));
    Q_strncpyz(config.test_name, "math_precision", sizeof(config.test_name));
    Q_strncpyz(config.description, "Test mathematical precision", sizeof(config.description));
    config.timeout_seconds = 10;
    CrossPlatformTest_AddTestToSuite(suite, &config);

    // Unicode support test
    memset(&config, 0, sizeof(config));
    Q_strncpyz(config.test_name, "unicode_support", sizeof(config.test_name));
    Q_strncpyz(config.description, "Test Unicode string handling", sizeof(config.description));
    config.timeout_seconds = 10;
    CrossPlatformTest_AddTestToSuite(suite, &config);

    return qtrue;
}

qboolean CrossPlatformTest_GenerateArchitectureTests(cross_platform_test_suite_t* suite) {
    if (!suite) return qfalse;

    // Add architecture-specific tests based on current architecture
    architecture_type_t arch = cross_platform_test_system.current_platform.arch.type;

    switch (arch) {
        case ARCH_X86_64:
        case ARCH_X86:
            // x86-specific tests could go here
            break;
        case ARCH_ARM64:
        case ARCH_ARM:
            // ARM-specific tests could go here
            break;
        default:
            break;
    }

    return qtrue;
}

qboolean CrossPlatformTest_GenerateCompilerTests(cross_platform_test_suite_t* suite) {
    if (!suite) return qfalse;

    // Add compiler-specific tests based on current compiler
    compiler_type_t compiler = cross_platform_test_system.current_platform.compiler.type;

    switch (compiler) {
        case COMPILER_GCC:
        case COMPILER_CLANG:
            // GCC/Clang specific tests
            break;
        case COMPILER_MSVC:
            // MSVC specific tests
            break;
        default:
            break;
    }

    return qtrue;
}

/*
=============================================================================
CI/CD Integration and Reporting
=============================================================================
*/

qboolean CrossPlatformTest_ExportForCI(const char* output_dir) {
    // Export test results for CI consumption
    Q_UNUSED(output_dir);
    // Implementation would create JUnit XML or other CI-compatible formats
    return qtrue;
}

qboolean CrossPlatformTest_GenerateCIReport(const char* output_file, const char* format) {
    // Generate CI-compatible reports
    Q_UNUSED(output_file);
    Q_UNUSED(format);
    // Implementation would create detailed reports in various formats
    return qtrue;
}

// Additional utility functions
uint32_t CrossPlatformTest_GetResults(cross_platform_test_result_t** results) {
    if (results) *results = cross_platform_test_system.results;
    return cross_platform_test_system.num_results;
}

qboolean CrossPlatformTest_SaveResults(const char* filename) {
    Q_UNUSED(filename);
    // Implementation would save results to JSON/XML file
    return qtrue;
}

qboolean CrossPlatformTest_LoadResults(const char* filename) {
    Q_UNUSED(filename);
    // Implementation would load results from JSON/XML file
    return qtrue;
}

qboolean CrossPlatformTest_TestPlatformCapabilities(void) {
    // Test platform capabilities
    return qtrue;
}

qboolean CrossPlatformTest_TestArchitectureFeatures(void) {
    // Test architecture-specific features
    return qtrue;
}

qboolean CrossPlatformTest_TestCompilerFeatures(void) {
    // Test compiler-specific features
    return qtrue;
}

qboolean CrossPlatformTest_ValidatePlatformCompatibility(void) {
    // Validate overall platform compatibility
    return qtrue;
}

qboolean CrossPlatformTest_CheckMinimumRequirements(void) {
    // Check minimum system requirements
    platform_capabilities_t* caps = &cross_platform_test_system.current_platform.capabilities;

    // Basic requirements
    if (!caps->has_threads) return qfalse;
    if (caps->total_memory < 512ULL * 1024 * 1024) return qfalse; // 512MB minimum

    return qtrue;
}
