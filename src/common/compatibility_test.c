/*
=============================================================================
Comprehensive Compatibility Testing Framework Implementation

Cross-platform and cross-hardware validation system for automated
compatibility verification and feature detection.
=============================================================================
*/

#include "compatibility_test.h"
#include "q_shared.h"
#include "qcommon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations
const char* Compatibility_GetCompilerName(compiler_type_t compiler);
#include <sys/utsname.h>
#include <unistd.h>

#ifdef _WIN32
#include <windows.h>
#include <intrin.h>
#endif

#ifdef __linux__
#include <sys/sysinfo.h>
#include <cpuid.h>
#endif

#ifdef __APPLE__
#include <sys/sysctl.h>
#include <mach-o/dyld.h>
#endif

// Global compatibility testing system
compatibility_system_t compatibility_system = {0};

// Platform name strings
static const char* platform_names[PLATFORM_COUNT] = {
    "Unknown", "Windows", "Linux", "macOS", "FreeBSD", "Android", "iOS", "Emscripten"
};

// Architecture name strings
static const char* architecture_names[ARCH_COUNT] = {
    "Unknown", "x86", "x86_64", "ARM", "ARM64", "RISC-V", "PowerPC", "PowerPC64", "s390x"
};

// Compiler name strings
static const char* compiler_names[COMPILER_COUNT] = {
    "Unknown", "GCC", "Clang", "MSVC", "Emscripten"
};

// Hardware category strings
static const char* hardware_category_names[HW_CATEGORY_COUNT] = {
    "CPU", "GPU", "Memory", "Storage", "Network", "Audio", "Input", "Display"
};

// Feature category strings
static const char* feature_category_names[FEATURE_COUNT] = {
    "Rendering", "Compute", "Multimedia", "Networking", "Storage", "Security", "Multithreading", "UI"
};

// Compatibility level strings
static const char* compatibility_level_names[] = {
    "None", "Minimal", "Basic", "Full", "Optimal"
};

/*
=============================================================================
Compatibility Testing API Implementation
=============================================================================
*/

qboolean Compatibility_Init(void) {
    if (compatibility_system.initialized) {
        return qtrue;
    }

    memset(&compatibility_system, 0, sizeof(compatibility_system_t));

    // Allocate feature matrix
    compatibility_system.max_features = 100;
    compatibility_system.feature_matrix = (feature_support_t*)malloc(
        sizeof(feature_support_t) * compatibility_system.max_features);

    if (!compatibility_system.feature_matrix) {
        Com_Printf("Failed to allocate memory for feature matrix\n");
        return qfalse;
    }

    memset(compatibility_system.feature_matrix, 0,
           sizeof(feature_support_t) * compatibility_system.max_features);

    // Allocate test results
    compatibility_system.max_results = 50;
    compatibility_system.test_results = (compatibility_result_t*)malloc(
        sizeof(compatibility_result_t) * compatibility_system.max_results);

    if (!compatibility_system.test_results) {
        free(compatibility_system.feature_matrix);
        Com_Printf("Failed to allocate memory for test results\n");
        return qfalse;
    }

    memset(compatibility_system.test_results, 0,
           sizeof(compatibility_result_t) * compatibility_system.max_results);

    // Set default configuration
    compatibility_system.enable_detailed_logging = qtrue;
    compatibility_system.enable_hardware_profiling = qtrue;
    compatibility_system.enable_feature_detection = qtrue;
    Q_strncpyz(compatibility_system.report_directory, "compatibility_reports", sizeof(compatibility_system.report_directory));
    Q_strncpyz(compatibility_system.baseline_file, "compatibility_baseline.txt", sizeof(compatibility_system.baseline_file));

    // Detect platform and hardware
    if (!Compatibility_DetectPlatform(&compatibility_system.platform_info)) {
        Com_Printf("Warning: Failed to detect platform information\n");
    }

    if (!Compatibility_DetectHardware(&compatibility_system.hardware_caps)) {
        Com_Printf("Warning: Failed to detect hardware capabilities\n");
    }

    // Initialize feature detection
    if (!Compatibility_DetectFeatureSupport()) {
        Com_Printf("Warning: Failed to detect feature support\n");
    }

    compatibility_system.initialized = qtrue;

    Com_Printf("Comprehensive compatibility testing system initialized\n");
    Com_Printf("Platform: %s %s (%s)\n",
               compatibility_system.platform_info.os_name,
               compatibility_system.platform_info.os_version,
               architecture_names[compatibility_system.platform_info.architecture]);
    Com_Printf("Hardware: %s CPU, %s GPU, %u GB RAM\n",
               compatibility_system.hardware_caps.cpu_brand,
               compatibility_system.hardware_caps.gpu_renderer,
               (uint32_t)(compatibility_system.hardware_caps.total_ram_mb / 1024));

    return qtrue;
}

void Compatibility_Shutdown(void) {
    if (!compatibility_system.initialized) {
        return;
    }

    // Save final compatibility report
    char report_path[512];
    Q_snprintf(report_path, sizeof(report_path), "%s/final_compatibility_report.txt",
               compatibility_system.report_directory);
    Compatibility_GenerateCompatibilityReport(report_path);

    // Free resources
    if (compatibility_system.feature_matrix) {
        free(compatibility_system.feature_matrix);
    }

    if (compatibility_system.test_results) {
        free(compatibility_system.test_results);
    }

    compatibility_system.initialized = qfalse;
    Com_Printf("Compatibility testing system shutdown\n");
}

/*
=============================================================================
Platform Detection
=============================================================================
*/

qboolean Compatibility_DetectPlatform(compatibility_platform_info_t* info) {
    if (!info) return qfalse;

    memset(info, 0, sizeof(compatibility_platform_info_t));

    // Detect OS and platform type
#ifdef _WIN32
    info->platform_type = PLATFORM_WINDOWS;
    Q_strncpyz(info->os_name, "Windows", sizeof(info->os_name));
    OSVERSIONINFOA osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOA));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
    GetVersionExA(&osvi);
    Q_snprintf(info->os_version, sizeof(info->os_version), "%u.%u",
               osvi.dwMajorVersion, osvi.dwMinorVersion);
    info->cpu_count = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);

#elif defined(__linux__)
    info->platform_type = PLATFORM_LINUX;
    struct utsname uname_info;
    if (uname(&uname_info) == 0) {
        Q_strncpyz(info->os_name, uname_info.sysname, sizeof(info->os_name));
        Q_strncpyz(info->os_version, uname_info.release, sizeof(info->os_version));
        Q_strncpyz(info->kernel_version, uname_info.release, sizeof(info->kernel_version));
    }

    // Try to detect distribution
    FILE* file = fopen("/etc/os-release", "r");
    if (file) {
        char line[256];
        while (fgets(line, sizeof(line), file)) {
            if (strstr(line, "PRETTY_NAME=") == line) {
                char* start = strchr(line, '"');
                char* end = strrchr(line, '"');
                if (start && end && start != end) {
                    start++;
                    *end = '\0';
                    Q_strncpyz(info->distribution, start, sizeof(info->distribution));
                }
                break;
            }
        }
        fclose(file);
    }

    // Get CPU count
    info->cpu_count = get_nprocs();

    // Get memory info
    struct sysinfo sys_info;
    if (sysinfo(&sys_info) == 0) {
        info->total_memory_mb = (sys_info.totalram * sys_info.mem_unit) / (1024 * 1024);
    }

#elif defined(__APPLE__)
    info->platform_type = PLATFORM_MACOS;
    Q_strncpyz(info->os_name, "macOS", sizeof(info->os_name));

    // Get system version
    char version_str[32];
    size_t version_size = sizeof(version_str);
    if (sysctlbyname("kern.osrelease", version_str, &version_size, NULL, 0) == 0) {
        Q_strncpyz(info->kernel_version, version_str, sizeof(info->kernel_version));
    }

    // Get CPU count
    int cpu_count;
    size_t cpu_size = sizeof(cpu_count);
    if (sysctlbyname("hw.ncpu", &cpu_count, &cpu_size, NULL, 0) == 0) {
        info->cpu_count = cpu_count;
    }

    // Get memory
    uint64_t mem_size;
    size_t mem_size_size = sizeof(mem_size);
    if (sysctlbyname("hw.memsize", &mem_size, &mem_size_size, NULL, 0) == 0) {
        info->total_memory_mb = mem_size / (1024 * 1024);
    }

#endif

    // Detect architecture
#ifdef __x86_64__
    info->architecture = ARCH_X86_64;
    info->is_64bit = qtrue;
#elif defined(__i386__)
    info->architecture = ARCH_X86;
    info->is_64bit = qfalse;
#elif defined(__aarch64__)
    info->architecture = ARCH_ARM64;
    info->is_64bit = qtrue;
#elif defined(__arm__)
    info->architecture = ARCH_ARM;
    info->is_64bit = qfalse;
#endif

    // Detect compiler
#ifdef __GNUC__
    #ifdef __clang__
        info->compiler = COMPILER_CLANG;
    #else
        info->compiler = COMPILER_GCC;
    #endif
#elif defined(_MSC_VER)
    info->compiler = COMPILER_MSVC;
#endif

    // Detect endianness
    uint32_t test = 1;
    info->is_big_endian = (*((uint8_t*)&test) == 0);

    return qtrue;
}

qboolean Compatibility_DetectHardware(hardware_capabilities_t* caps) {
    if (!caps) return qfalse;

    memset(caps, 0, sizeof(hardware_capabilities_t));

    // CPU detection
#ifdef __x86_64__
    unsigned int regs[4];

    // Get CPU vendor
    __cpuid(0x00000000, regs[0], regs[1], regs[2], regs[3]);
    memcpy(caps->cpu_vendor, &regs[1], 4);
    memcpy(caps->cpu_vendor + 4, &regs[3], 4);
    memcpy(caps->cpu_vendor + 8, &regs[2], 4);

    // Get CPU brand string
    __cpuid(0x80000002, regs[0], regs[1], regs[2], regs[3]);
    memcpy(caps->cpu_brand, regs, 16);
    __cpuid(0x80000003, regs[0], regs[1], regs[2], regs[3]);
    memcpy(caps->cpu_brand + 16, regs, 16);
    __cpuid(0x80000004, regs[0], regs[1], regs[2], regs[3]);
    memcpy(caps->cpu_brand + 32, regs, 16);

    // Get CPU features
    __cpuid(0x00000001, regs[0], regs[1], regs[2], regs[3]);
    caps->has_sse = (regs[3] & (1 << 25)) != 0;
    caps->has_sse2 = (regs[3] & (1 << 26)) != 0;
    caps->has_sse3 = (regs[2] & (1 << 0)) != 0;
    caps->has_ssse3 = (regs[2] & (1 << 9)) != 0;
    caps->has_sse4_1 = (regs[2] & (1 << 19)) != 0;
    caps->has_sse4_2 = (regs[2] & (1 << 20)) != 0;

    // AVX detection
    if (regs[2] & (1 << 28)) { // AVX bit
        caps->has_avx = qtrue;
        // Check for AVX2
        __cpuid(0x00000007, regs[0], regs[1], regs[2], regs[3]);
        caps->has_avx2 = (regs[1] & (1 << 5)) != 0;
    }

    caps->cpu_cores = compatibility_system.platform_info.cpu_count;

#endif

    // Basic memory detection (already done in platform detection)
    caps->total_ram_mb = compatibility_system.platform_info.total_memory_mb;

    // GPU detection - basic placeholder
    Q_strncpyz(caps->gpu_vendor, "Unknown", sizeof(caps->gpu_vendor));
    Q_strncpyz(caps->gpu_renderer, "Unknown", sizeof(caps->gpu_renderer));
    caps->supports_opengl = qtrue; // Assume basic OpenGL support

    return qtrue;
}

const char* Compatibility_GetPlatformName(platform_type_t platform) {
    if (platform >= PLATFORM_COUNT) return "Unknown";
    return platform_names[platform];
}

const char* Compatibility_GetArchitectureName(architecture_type_t arch) {
    if (arch >= ARCH_COUNT) return "Unknown";
    return architecture_names[arch];
}

const char* Compatibility_GetCompilerName(compiler_type_t compiler) {
    if (compiler >= COMPILER_COUNT) return "Unknown";
    return compiler_names[compiler];
}

/*
=============================================================================
Feature Detection
=============================================================================
*/

qboolean Compatibility_DetectFeatureSupport(void) {
    // Add core engine features to check
    Compatibility_AddFeatureCheck(FEATURE_RENDERING, "OpenGL 2.0+", "Basic 3D rendering", qtrue);
    Compatibility_AddFeatureCheck(FEATURE_RENDERING, "Vulkan 1.0+", "Modern graphics API", qfalse);
    Compatibility_AddFeatureCheck(FEATURE_MULTIMEDIA, "OpenAL", "3D audio support", qtrue);
    Compatibility_AddFeatureCheck(FEATURE_MULTITHREADING, "POSIX Threads", "Multi-threaded rendering", qfalse);
    Compatibility_AddFeatureCheck(FEATURE_NETWORKING, "TCP/UDP Sockets", "Multiplayer support", qtrue);
    Compatibility_AddFeatureCheck(FEATURE_STORAGE, "File I/O", "Save/load functionality", qtrue);
    Compatibility_AddFeatureCheck(FEATURE_UI, "SDL Input", "User input handling", qtrue);
    Compatibility_AddFeatureCheck(FEATURE_SECURITY, "Stack Protection", "Buffer overflow protection", qfalse);

    // Platform-specific features
#ifdef _WIN32
    Compatibility_AddFeatureCheck(FEATURE_MULTIMEDIA, "DirectSound", "Windows audio", qfalse);
    Compatibility_AddFeatureCheck(FEATURE_RENDERING, "Direct3D", "Windows graphics", qfalse);
#endif

#ifdef __linux__
    Compatibility_AddFeatureCheck(FEATURE_MULTIMEDIA, "ALSA", "Linux audio", qfalse);
    Compatibility_AddFeatureCheck(FEATURE_MULTIMEDIA, "PulseAudio", "Linux audio", qfalse);
#endif

    // Test each feature
    for (uint32_t i = 0; i < compatibility_system.feature_count; i++) {
        feature_support_t* feature = &compatibility_system.feature_matrix[i];
        qboolean available = qfalse;
        char limitations[256] = "";

        // Basic feature testing
        if (strcmp(feature->feature_name, "OpenGL 2.0+") == 0) {
            available = qtrue; // Assume available for now
        } else if (strcmp(feature->feature_name, "Vulkan 1.0+") == 0) {
#ifdef USE_VULKAN
            available = qtrue;
#else
            Q_strncpyz(limitations, "Vulkan renderer not enabled", sizeof(limitations));
#endif
        } else if (strcmp(feature->feature_name, "OpenAL") == 0) {
            available = qtrue; // Assume available
        } else if (strcmp(feature->feature_name, "TCP/UDP Sockets") == 0) {
            available = qtrue; // Network support
        } else if (strcmp(feature->feature_name, "File I/O") == 0) {
            available = qtrue; // Basic file operations
        } else if (strcmp(feature->feature_name, "SDL Input") == 0) {
            available = qtrue; // SDL input system
        }

        Compatibility_UpdateFeatureStatus(feature->feature_name,
                                        available ? COMPATIBILITY_FULL : COMPATIBILITY_NONE,
                                        available, limitations);
    }

    return qtrue;
}

qboolean Compatibility_AddFeatureCheck(feature_category_t category,
                                     const char* feature_name,
                                     const char* requirement_desc,
                                     qboolean is_required) {
    if (!compatibility_system.initialized ||
        compatibility_system.feature_count >= compatibility_system.max_features) {
        return qfalse;
    }

    feature_support_t* feature = &compatibility_system.feature_matrix[compatibility_system.feature_count++];
    memset(feature, 0, sizeof(feature_support_t));

    feature->category = category;
    Q_strncpyz(feature->feature_name, feature_name, sizeof(feature->feature_name));
    Q_strncpyz(feature->requirement_description, requirement_desc, sizeof(feature->requirement_description));
    feature->is_required = is_required;
    feature->compatibility = COMPATIBILITY_NONE;
    feature->is_available = qfalse;

    return qtrue;
}

qboolean Compatibility_UpdateFeatureStatus(const char* feature_name,
                                         compatibility_level_t level,
                                         qboolean available,
                                         const char* limitations) {
    for (uint32_t i = 0; i < compatibility_system.feature_count; i++) {
        feature_support_t* feature = &compatibility_system.feature_matrix[i];
        if (strcmp(feature->feature_name, feature_name) == 0) {
            feature->compatibility = level;
            feature->is_available = available;
            if (limitations) {
                Q_strncpyz(feature->limitation_description, limitations,
                          sizeof(feature->limitation_description));
            }
            return qtrue;
        }
    }
    return qfalse;
}

/*
=============================================================================
Compatibility Testing
=============================================================================
*/

qboolean Compatibility_RunPlatformTests(void) {
    Com_Printf("Running platform compatibility tests...\n");

    compatibility_result_t result;
    memset(&result, 0, sizeof(result));
    Q_strncpyz(result.test_name, "platform_compatibility", sizeof(result.test_name));
    Q_snprintf(result.platform_description, sizeof(result.platform_description),
               "%s %s (%s)", compatibility_system.platform_info.os_name,
               compatibility_system.platform_info.os_version,
               architecture_names[compatibility_system.platform_info.architecture]);

    uint64_t start_time = Sys_Milliseconds();

    // Test basic platform APIs
    result.features_tested = 3;
    result.features_passed = 0;

    // Test 1: Basic file operations
    FILE* test_file = fopen("compatibility_test.tmp", "w");
    if (test_file) {
        fprintf(test_file, "test");
        fclose(test_file);
        unlink("compatibility_test.tmp");
        result.features_passed++;
    }

    // Test 2: Memory allocation
    void* test_mem = malloc(1024);
    if (test_mem) {
        free(test_mem);
        result.features_passed++;
    }

    // Test 3: Time functions
    uint64_t current_time = Sys_Milliseconds();
    if (current_time > 0) {
        result.features_passed++;
    }

    result.features_failed = result.features_tested - result.features_passed;
    result.test_duration_ms = Sys_Milliseconds() - start_time;

    // Determine overall compatibility
    if (result.features_passed == result.features_tested) {
        result.overall_compatibility = COMPATIBILITY_FULL;
        result.platform_supported = qtrue;
        Q_strncpyz(result.recommendations, "Platform fully supported", sizeof(result.recommendations));
    } else if (result.features_passed >= result.features_tested / 2) {
        result.overall_compatibility = COMPATIBILITY_BASIC;
        result.platform_supported = qtrue;
        Q_strncpyz(result.recommendations, "Platform supported with limitations", sizeof(result.recommendations));
    } else {
        result.overall_compatibility = COMPATIBILITY_NONE;
        result.platform_supported = qfalse;
        Q_strncpyz(result.recommendations, "Platform not supported", sizeof(result.recommendations));
    }

    // Store result
    if (compatibility_system.result_count < compatibility_system.max_results) {
        memcpy(&compatibility_system.test_results[compatibility_system.result_count++],
               &result, sizeof(result));
    }

    Com_Printf("Platform compatibility test completed: %s\n",
               compatibility_level_names[result.overall_compatibility]);
    return result.platform_supported;
}

qboolean Compatibility_RunHardwareTests(void) {
    Com_Printf("Running hardware compatibility tests...\n");

    compatibility_result_t result;
    memset(&result, 0, sizeof(result));
    Q_strncpyz(result.test_name, "hardware_compatibility", sizeof(result.test_name));
    Q_snprintf(result.platform_description, sizeof(result.platform_description),
               "%s CPU, %u cores, %u GB RAM",
               compatibility_system.hardware_caps.cpu_brand,
               compatibility_system.hardware_caps.cpu_cores,
               (uint32_t)(compatibility_system.hardware_caps.total_ram_mb / 1024));

    uint64_t start_time = Sys_Milliseconds();

    // Test hardware capabilities
    result.features_tested = 4;
    result.features_passed = 0;

    // Test 1: CPU core count
    if (compatibility_system.hardware_caps.cpu_cores >= 2) {
        result.features_passed++;
    }

    // Test 2: Memory amount
    if (compatibility_system.hardware_caps.total_ram_mb >= 1024) { // 1GB minimum
        result.features_passed++;
    }

    // Test 3: CPU features (SSE2 minimum)
    if (compatibility_system.hardware_caps.has_sse2) {
        result.features_passed++;
    }

    // Test 4: Basic GPU availability
    if (compatibility_system.hardware_caps.gpu_memory_mb > 0) {
        result.features_passed++;
    }

    result.features_failed = result.features_tested - result.features_passed;
    result.test_duration_ms = Sys_Milliseconds() - start_time;

    // Determine hardware compatibility
    if (result.features_passed == result.features_tested) {
        result.overall_compatibility = COMPATIBILITY_OPTIMAL;
        result.platform_supported = qtrue;
        Q_strncpyz(result.recommendations, "Hardware fully adequate", sizeof(result.recommendations));
    } else if (result.features_passed >= 3) {
        result.overall_compatibility = COMPATIBILITY_FULL;
        result.platform_supported = qtrue;
        Q_strncpyz(result.recommendations, "Hardware meets requirements", sizeof(result.recommendations));
    } else if (result.features_passed >= 2) {
        result.overall_compatibility = COMPATIBILITY_BASIC;
        result.platform_supported = qtrue;
        Q_strncpyz(result.recommendations, "Hardware meets minimum requirements", sizeof(result.recommendations));
    } else {
        result.overall_compatibility = COMPATIBILITY_MINIMAL;
        result.platform_supported = qfalse;
        Q_strncpyz(result.recommendations, "Hardware below minimum requirements", sizeof(result.recommendations));
    }

    // Store result
    if (compatibility_system.result_count < compatibility_system.max_results) {
        memcpy(&compatibility_system.test_results[compatibility_system.result_count++],
               &result, sizeof(result));
    }

    Com_Printf("Hardware compatibility test completed: %s\n",
               compatibility_level_names[result.overall_compatibility]);
    return result.platform_supported;
}

qboolean Compatibility_RunFeatureTests(void) {
    Com_Printf("Running feature compatibility tests...\n");

    compatibility_result_t result;
    memset(&result, 0, sizeof(result));
    Q_strncpyz(result.test_name, "feature_compatibility", sizeof(result.test_name));
    Q_strncpyz(result.platform_description, "Feature availability test", sizeof(result.platform_description));

    uint64_t start_time = Sys_Milliseconds();

    // Count features and their status
    uint32_t required_features = 0;
    uint32_t required_passed = 0;

    for (uint32_t i = 0; i < compatibility_system.feature_count; i++) {
        feature_support_t* feature = &compatibility_system.feature_matrix[i];
        result.features_tested++;

        if (feature->is_available) {
            result.features_passed++;
            if (feature->is_required) {
                required_passed++;
            }
        } else {
            result.features_failed++;
        }

        if (feature->is_required) {
            required_features++;
        }
    }

    result.test_duration_ms = Sys_Milliseconds() - start_time;

    // Determine feature compatibility
    if (required_passed == required_features && result.features_failed == 0) {
        result.overall_compatibility = COMPATIBILITY_FULL;
        result.platform_supported = qtrue;
        Q_strncpyz(result.recommendations, "All features available", sizeof(result.recommendations));
    } else if (required_passed == required_features) {
        result.overall_compatibility = COMPATIBILITY_BASIC;
        result.platform_supported = qtrue;
        Q_snprintf(result.recommendations, sizeof(result.recommendations),
                  "%u optional features unavailable", result.features_failed);
    } else {
        result.overall_compatibility = COMPATIBILITY_MINIMAL;
        result.platform_supported = qfalse;
        Q_snprintf(result.recommendations, sizeof(result.recommendations),
                  "%u required features unavailable", required_features - required_passed);
    }

    // Store result
    if (compatibility_system.result_count < compatibility_system.max_results) {
        memcpy(&compatibility_system.test_results[compatibility_system.result_count++],
               &result, sizeof(result));
    }

    Com_Printf("Feature compatibility test completed: %s (%u/%u features available)\n",
               compatibility_level_names[result.overall_compatibility],
               result.features_passed, result.features_tested);
    return result.platform_supported;
}

qboolean Compatibility_RunComprehensiveTest(void) {
    Com_Printf("Running comprehensive compatibility test suite...\n");

    qboolean platform_ok = Compatibility_RunPlatformTests();
    qboolean hardware_ok = Compatibility_RunHardwareTests();
    qboolean features_ok = Compatibility_RunFeatureTests();

    compatibility_level_t overall = COMPATIBILITY_NONE;

    if (platform_ok && hardware_ok && features_ok) {
        overall = COMPATIBILITY_FULL;
    } else if (platform_ok && (hardware_ok || features_ok)) {
        overall = COMPATIBILITY_BASIC;
    } else if (platform_ok) {
        overall = COMPATIBILITY_MINIMAL;
    }

    Com_Printf("Comprehensive compatibility test completed: %s\n",
               compatibility_level_names[overall]);
    Com_Printf("Platform: %s, Hardware: %s, Features: %s\n",
               platform_ok ? "OK" : "FAIL",
               hardware_ok ? "OK" : "FAIL",
               features_ok ? "OK" : "FAIL");

    return overall >= COMPATIBILITY_BASIC;
}

/*
=============================================================================
Result Management and Reporting
=============================================================================
*/

compatibility_level_t Compatibility_GetOverallCompatibility(void) {
    if (compatibility_system.result_count == 0) {
        return COMPATIBILITY_NONE;
    }

    compatibility_level_t worst = COMPATIBILITY_OPTIMAL;

    for (uint32_t i = 0; i < compatibility_system.result_count; i++) {
        compatibility_level_t level = compatibility_system.test_results[i].overall_compatibility;
        if (level < worst) {
            worst = level;
        }
    }

    return worst;
}

qboolean Compatibility_IsPlatformSupported(void) {
    for (uint32_t i = 0; i < compatibility_system.result_count; i++) {
        if (!compatibility_system.test_results[i].platform_supported) {
            return qfalse;
        }
    }
    return qtrue;
}

qboolean Compatibility_CheckMinimumRequirements(void) {
    // Check CPU cores
    if (compatibility_system.hardware_caps.cpu_cores < 2) {
        return qfalse;
    }

    // Check memory
    if (compatibility_system.hardware_caps.total_ram_mb < 1024) { // 1GB
        return qfalse;
    }

    // Check required features
    for (uint32_t i = 0; i < compatibility_system.feature_count; i++) {
        feature_support_t* feature = &compatibility_system.feature_matrix[i];
        if (feature->is_required && !feature->is_available) {
            return qfalse;
        }
    }

    return qtrue;
}

qboolean Compatibility_GenerateCompatibilityReport(const char* output_file) {
    FILE* file = fopen(output_file, "w");
    if (!file) return qfalse;

    fprintf(file, "=============================================================================\n");
    fprintf(file, "COMPREHENSIVE COMPATIBILITY REPORT\n");
    fprintf(file, "Generated: %llu\n", (unsigned long long)Sys_Milliseconds());
    fprintf(file, "=============================================================================\n\n");

    // Platform Information
    fprintf(file, "PLATFORM INFORMATION\n");
    fprintf(file, "--------------------\n");
    fprintf(file, "OS: %s %s\n", compatibility_system.platform_info.os_name,
            compatibility_system.platform_info.os_version);
    fprintf(file, "Kernel: %s\n", compatibility_system.platform_info.kernel_version);
    fprintf(file, "Distribution: %s\n", compatibility_system.platform_info.distribution);
    fprintf(file, "Architecture: %s (%s)\n",
            Compatibility_GetArchitectureName(compatibility_system.platform_info.architecture),
            compatibility_system.platform_info.is_64bit ? "64-bit" : "32-bit");
    fprintf(file, "Compiler: %s\n", Compatibility_GetCompilerName(compatibility_system.platform_info.compiler));
    fprintf(file, "CPU Cores: %u\n", compatibility_system.platform_info.cpu_count);
    fprintf(file, "Total Memory: %u MB\n\n", (uint32_t)compatibility_system.platform_info.total_memory_mb);

    // Hardware Capabilities
    fprintf(file, "HARDWARE CAPABILITIES\n");
    fprintf(file, "---------------------\n");
    fprintf(file, "CPU: %s (%u cores)\n", compatibility_system.hardware_caps.cpu_brand,
            compatibility_system.hardware_caps.cpu_cores);
    fprintf(file, "GPU: %s (%u MB)\n", compatibility_system.hardware_caps.gpu_renderer,
            (uint32_t)compatibility_system.hardware_caps.gpu_memory_mb);
    fprintf(file, "RAM: %u MB\n", (uint32_t)compatibility_system.hardware_caps.total_ram_mb);
    fprintf(file, "CPU Features: SSE=%s, SSE2=%s, AVX=%s\n",
            compatibility_system.hardware_caps.has_sse ? "Yes" : "No",
            compatibility_system.hardware_caps.has_sse2 ? "Yes" : "No",
            compatibility_system.hardware_caps.has_avx ? "Yes" : "No");
    fprintf(file, "\n");

    // Feature Support Matrix
    fprintf(file, "FEATURE SUPPORT MATRIX\n");
    fprintf(file, "----------------------\n");
    fprintf(file, "%-20s %-12s %-8s %-8s %s\n", "Feature", "Category", "Required", "Available", "Limitations");
    fprintf(file, "%-20s %-12s %-8s %-8s %s\n", "--------------------", "------------", "--------", "--------", "-----------");

    for (uint32_t i = 0; i < compatibility_system.feature_count; i++) {
        feature_support_t* feature = &compatibility_system.feature_matrix[i];
        fprintf(file, "%-20s %-12s %-8s %-8s %s\n",
                feature->feature_name,
                feature_category_names[feature->category],
                feature->is_required ? "Yes" : "No",
                feature->is_available ? "Yes" : "No",
                feature->limitation_description);
    }
    fprintf(file, "\n");

    // Test Results
    fprintf(file, "COMPATIBILITY TEST RESULTS\n");
    fprintf(file, "--------------------------\n");

    for (uint32_t i = 0; i < compatibility_system.result_count; i++) {
        compatibility_result_t* result = &compatibility_system.test_results[i];
        fprintf(file, "Test: %s\n", result->test_name);
        fprintf(file, "Platform: %s\n", result->platform_description);
        fprintf(file, "Compatibility: %s\n", compatibility_level_names[result->overall_compatibility]);
        fprintf(file, "Features: %u tested, %u passed, %u failed\n",
                result->features_tested, result->features_passed, result->features_failed);
        fprintf(file, "Duration: %llu ms\n", (unsigned long long)result->test_duration_ms);
        fprintf(file, "Supported: %s\n", result->platform_supported ? "Yes" : "No");
        fprintf(file, "Recommendations: %s\n", result->recommendations);
        fprintf(file, "\n");
    }

    // Overall Assessment
    fprintf(file, "OVERALL COMPATIBILITY ASSESSMENT\n");
    fprintf(file, "--------------------------------\n");
    fprintf(file, "Overall Compatibility: %s\n", compatibility_level_names[Compatibility_GetOverallCompatibility()]);
    fprintf(file, "Platform Supported: %s\n", Compatibility_IsPlatformSupported() ? "Yes" : "No");
    fprintf(file, "Minimum Requirements Met: %s\n", Compatibility_CheckMinimumRequirements() ? "Yes" : "No");

    if (Compatibility_CheckMinimumRequirements()) {
        fprintf(file, "\nCONCLUSION: This platform meets the minimum requirements for running the application.\n");
    } else {
        fprintf(file, "\nCONCLUSION: This platform does not meet the minimum requirements.\n");
        fprintf(file, "Consider upgrading hardware or using a different platform.\n");
    }

    fclose(file);
    return qtrue;
}

/*
=============================================================================
Utility Functions
=============================================================================
*/

qboolean Compatibility_PrintPlatformInfo(void) {
    Com_Printf("=== Platform Information ===\n");
    Com_Printf("OS: %s %s\n", compatibility_system.platform_info.os_name,
               compatibility_system.platform_info.os_version);
    Com_Printf("Architecture: %s (%s)\n",
               Compatibility_GetArchitectureName(compatibility_system.platform_info.architecture),
               compatibility_system.platform_info.is_64bit ? "64-bit" : "32-bit");
    Com_Printf("Compiler: %s\n", Compatibility_GetCompilerName(compatibility_system.platform_info.compiler));
    Com_Printf("CPU Cores: %u\n", compatibility_system.platform_info.cpu_count);
    Com_Printf("Memory: %u MB\n", (uint32_t)compatibility_system.platform_info.total_memory_mb);
    Com_Printf("===========================\n");
    return qtrue;
}

qboolean Compatibility_PrintHardwareInfo(void) {
    Com_Printf("=== Hardware Information ===\n");
    Com_Printf("CPU: %s (%u cores)\n", compatibility_system.hardware_caps.cpu_brand,
               compatibility_system.hardware_caps.cpu_cores);
    Com_Printf("GPU: %s (%u MB VRAM)\n", compatibility_system.hardware_caps.gpu_renderer,
               (uint32_t)compatibility_system.hardware_caps.gpu_memory_mb);
    Com_Printf("RAM: %u MB\n", (uint32_t)compatibility_system.hardware_caps.total_ram_mb);
    Com_Printf("CPU Features: SSE=%s SSE2=%s AVX=%s\n",
               compatibility_system.hardware_caps.has_sse ? "Y" : "N",
               compatibility_system.hardware_caps.has_sse2 ? "Y" : "N",
               compatibility_system.hardware_caps.has_avx ? "Y" : "N");
    Com_Printf("==========================\n");
    return qtrue;
}

qboolean Compatibility_PrintFeatureMatrix(void) {
    Com_Printf("=== Feature Support Matrix ===\n");
    Com_Printf("%-20s %-10s %-8s %-8s %s\n", "Feature", "Category", "Required", "Available", "Status");
    Com_Printf("%-20s %-10s %-8s %-8s %s\n", "--------------------", "----------", "--------", "--------", "------");

    for (uint32_t i = 0; i < compatibility_system.feature_count; i++) {
        feature_support_t* feature = &compatibility_system.feature_matrix[i];
        Com_Printf("%-20s %-10s %-8s %-8s %s\n",
                  feature->feature_name,
                  feature_category_names[feature->category],
                  feature->is_required ? "Yes" : "No",
                  feature->is_available ? "Yes" : "No",
                  compatibility_level_names[feature->compatibility]);
    }
    Com_Printf("===============================\n");
    return qtrue;
}

qboolean Compatibility_PrintTestResults(void) {
    Com_Printf("=== Compatibility Test Results ===\n");

    for (uint32_t i = 0; i < compatibility_system.result_count; i++) {
        compatibility_result_t* result = &compatibility_system.test_results[i];
        Com_Printf("Test: %s (%s)\n", result->test_name, result->platform_description);
        Com_Printf("  Compatibility: %s\n", compatibility_level_names[result->overall_compatibility]);
        Com_Printf("  Features: %u/%u passed\n", result->features_passed, result->features_tested);
        Com_Printf("  Duration: %llu ms\n", (unsigned long long)result->test_duration_ms);
        Com_Printf("  Status: %s\n", result->platform_supported ? "SUPPORTED" : "NOT SUPPORTED");
        if (result->recommendations[0]) {
            Com_Printf("  Recommendations: %s\n", result->recommendations);
        }
        Com_Printf("\n");
    }

    Com_Printf("Overall Compatibility: %s\n", compatibility_level_names[Compatibility_GetOverallCompatibility()]);
    Com_Printf("==================================\n");
    return qtrue;
}

const char* Compatibility_GetCompatibilityLevelString(compatibility_level_t level) {
    if (level >= COMPATIBILITY_COUNT) return "Unknown";
    return compatibility_level_names[level];
}

/*
=============================================================================
Hardware-Specific Tests
=============================================================================
*/

qboolean Compatibility_TestCPUCapabilities(void) {
    // Basic CPU capability tests
    return compatibility_system.hardware_caps.cpu_cores >= 1 &&
           compatibility_system.hardware_caps.has_sse2; // Minimum SSE2 requirement
}

qboolean Compatibility_TestGPUCapabilities(void) {
    // Basic GPU capability tests
    return compatibility_system.hardware_caps.gpu_memory_mb >= 128; // Minimum 128MB VRAM
}

qboolean Compatibility_TestMemoryCapabilities(void) {
    // Memory capability tests
    return compatibility_system.hardware_caps.total_ram_mb >= 512; // Minimum 512MB RAM
}

/*
=============================================================================
Console Commands
=============================================================================
*/

void Compatibility_Status_f(void) {
    if (!compatibility_system.initialized) {
        Com_Printf("Compatibility testing system not initialized\n");
        return;
    }

    Com_Printf("=== Compatibility Testing System Status ===\n");
    Com_Printf("Initialized: Yes\n");
    Com_Printf("Features tracked: %u\n", compatibility_system.feature_count);
    Com_Printf("Test results: %u\n", compatibility_system.result_count);
    Com_Printf("Platform: %s\n", Compatibility_GetPlatformName(compatibility_system.platform_info.platform_type));
    Com_Printf("Architecture: %s\n", Compatibility_GetArchitectureName(compatibility_system.platform_info.architecture));
    Com_Printf("Overall compatibility: %s\n", Compatibility_GetCompatibilityLevelString(Compatibility_GetOverallCompatibility()));
    Com_Printf("Platform supported: %s\n", Compatibility_IsPlatformSupported() ? "Yes" : "No");
    Com_Printf("Minimum requirements met: %s\n", Compatibility_CheckMinimumRequirements() ? "Yes" : "No");
    Com_Printf("===========================================\n");
}

void Compatibility_Test_f(void) {
    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: compatibility test <type>\n");
        Com_Printf("Types: platform, hardware, features, comprehensive\n");
        return;
    }

    const char* test_type = Cmd_Argv(1);
    qboolean result = qfalse;

    if (Q_stricmp(test_type, "platform") == 0) {
        result = Compatibility_RunPlatformTests();
    } else if (Q_stricmp(test_type, "hardware") == 0) {
        result = Compatibility_RunHardwareTests();
    } else if (Q_stricmp(test_type, "features") == 0) {
        result = Compatibility_RunFeatureTests();
    } else if (Q_stricmp(test_type, "comprehensive") == 0) {
        result = Compatibility_RunComprehensiveTest();
    } else {
        Com_Printf("Unknown test type: %s\n", test_type);
        return;
    }

    Com_Printf("Test result: %s\n", result ? "PASSED" : "FAILED");
}

void Compatibility_Report_f(void) {
    char report_file[256];
    Q_snprintf(report_file, sizeof(report_file), "%s/compatibility_report_%llu.txt",
               compatibility_system.report_directory, (unsigned long long)Sys_Milliseconds());

    if (Compatibility_GenerateCompatibilityReport(report_file)) {
        Com_Printf("Compatibility report generated: %s\n", report_file);
    } else {
        Com_Printf("Failed to generate compatibility report\n");
    }
}

void Compatibility_Benchmark_f(void) {
    Com_Printf("Running compatibility benchmark...\n");

    uint64_t start_time = Sys_Milliseconds();
    qboolean result = Compatibility_RunComprehensiveTest();
    uint64_t duration = Sys_Milliseconds() - start_time;

    Com_Printf("Compatibility benchmark completed in %llu ms\n", (unsigned long long)duration);
    Com_Printf("Result: %s\n", result ? "COMPATIBLE" : "INCOMPATIBLE");
}

/*
=============================================================================
Stub Functions for Result Management
=============================================================================
*/

uint32_t Compatibility_GetResults(compatibility_result_t** results) {
    if (results) {
        *results = compatibility_system.test_results;
    }
    return compatibility_system.result_count;
}

const compatibility_result_t* Compatibility_GetResult(const char* test_name) {
    for (uint32_t i = 0; i < compatibility_system.result_count; i++) {
        if (Q_stricmp(compatibility_system.test_results[i].test_name, test_name) == 0) {
            return &compatibility_system.test_results[i];
        }
    }
    return NULL;
}

qboolean Compatibility_SaveResults(const char* filename) {
    // Simplified implementation - would save detailed results in full version
    FILE* file = fopen(filename, "w");
    if (!file) return qfalse;

    fprintf(file, "# Compatibility Test Results\n");
    fprintf(file, "# Generated: %llu\n", (unsigned long long)Sys_Milliseconds());
    fclose(file);

    return qtrue;
}

qboolean Compatibility_LoadResults(const char* filename) {
    // Simplified implementation
    return qtrue;
}
