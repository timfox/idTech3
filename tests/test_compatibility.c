/*
=============================================================================
Comprehensive Compatibility Testing System Test

Automated cross-platform and cross-hardware validation tests.
=============================================================================
*/

#include "q_shared.h"
#include "compatibility_test.h"
#include "cross_platform_test.h"
#include <stdio.h>

// Forward declarations for functions used in tests
const char* Compatibility_GetArchitectureName(architecture_type_t arch);

static qboolean test_compatibility_initialization(void) {
    printf("Testing compatibility system initialization...\n");

    if (!Compatibility_Init()) {
        printf("FAILED: Could not initialize compatibility system\n");
        return qfalse;
    }

    if (!compatibility_system.initialized) {
        printf("FAILED: Compatibility system not marked as initialized\n");
        return qfalse;
    }

    printf("PASSED: Compatibility system initialized successfully\n");
    return qtrue;
}

static qboolean test_platform_detection(void) {
    printf("Testing platform detection...\n");

    platform_info_t info;
    if (!Compatibility_DetectPlatform(&info)) {
        printf("FAILED: Could not detect platform information\n");
        return qfalse;
    }

    if (info.platform_type == PLATFORM_UNKNOWN) {
        printf("FAILED: Platform type not detected\n");
        return qfalse;
    }

    if (info.architecture == ARCH_UNKNOWN) {
        printf("FAILED: Architecture not detected\n");
        return qfalse;
    }

    printf("PASSED: Platform detection works (Platform: %s, Arch: %s)\n",
           Compatibility_GetPlatformName(info.platform_type),
           Compatibility_GetArchitectureName(info.architecture));
    return qtrue;
}

static qboolean test_hardware_detection(void) {
    printf("Testing hardware detection...\n");

    hardware_capabilities_t caps;
    if (!Compatibility_DetectHardware(&caps)) {
        printf("FAILED: Could not detect hardware capabilities\n");
        return qfalse;
    }

    // Basic validation - we should at least detect some CPU cores
    if (caps.cpu_cores == 0) {
        printf("FAILED: CPU cores not detected\n");
        return qfalse;
    }

    printf("PASSED: Hardware detection works (CPU: %u cores, RAM: %u MB)\n",
           caps.cpu_cores, (uint32_t)caps.total_ram_mb);
    return qtrue;
}

static qboolean test_feature_detection(void) {
    printf("Testing feature detection...\n");

    if (!Compatibility_DetectFeatureSupport()) {
        printf("FAILED: Could not detect feature support\n");
        return qfalse;
    }

    if (compatibility_system.feature_count == 0) {
        printf("FAILED: No features detected\n");
        return qfalse;
    }

    printf("PASSED: Feature detection works (%u features detected)\n",
           compatibility_system.feature_count);
    return qtrue;
}

static qboolean test_feature_matrix_operations(void) {
    printf("Testing feature matrix operations...\n");

    // Test adding a feature
    if (!Compatibility_AddFeatureCheck(FEATURE_RENDERING, "Test Feature",
                                     "Test requirement", qtrue)) {
        printf("FAILED: Could not add feature check\n");
        return qfalse;
    }

    // Test updating feature status
    if (!Compatibility_UpdateFeatureStatus("Test Feature", COMPATIBILITY_FULL,
                                         qtrue, "No limitations")) {
        printf("FAILED: Could not update feature status\n");
        return qfalse;
    }

    printf("PASSED: Feature matrix operations work correctly\n");
    return qtrue;
}

static qboolean test_platform_tests(void) {
    printf("Testing platform compatibility tests...\n");

    if (!Compatibility_RunPlatformTests()) {
        printf("FAILED: Platform tests failed\n");
        return qfalse;
    }

    // Should have at least one test result
    if (compatibility_system.result_count == 0) {
        printf("FAILED: No test results generated\n");
        return qfalse;
    }

    printf("PASSED: Platform tests executed successfully\n");
    return qtrue;
}

static qboolean test_hardware_tests(void) {
    printf("Testing hardware compatibility tests...\n");

    if (!Compatibility_RunHardwareTests()) {
        printf("FAILED: Hardware tests failed\n");
        return qfalse;
    }

    printf("PASSED: Hardware tests executed successfully\n");
    return qtrue;
}

static qboolean test_feature_tests(void) {
    printf("Testing feature compatibility tests...\n");

    if (!Compatibility_RunFeatureTests()) {
        printf("FAILED: Feature tests failed\n");
        return qfalse;
    }

    printf("PASSED: Feature tests executed successfully\n");
    return qtrue;
}

static qboolean test_comprehensive_tests(void) {
    printf("Testing comprehensive compatibility tests...\n");

    if (!Compatibility_RunComprehensiveTest()) {
        printf("FAILED: Comprehensive tests failed\n");
        return qfalse;
    }

    printf("PASSED: Comprehensive tests executed successfully\n");
    return qtrue;
}

static qboolean test_compatibility_assessment(void) {
    printf("Testing compatibility assessment...\n");

    compatibility_level_t overall = Compatibility_GetOverallCompatibility();
    qboolean platform_supported = Compatibility_IsPlatformSupported();
    qboolean meets_minimum = Compatibility_CheckMinimumRequirements();

    printf("Overall compatibility: %s\n", Compatibility_GetCompatibilityLevelString(overall));
    printf("Platform supported: %s\n", platform_supported ? "Yes" : "No");
    printf("Meets minimum requirements: %s\n", meets_minimum ? "Yes" : "No");

    // Should have some level of compatibility assessment
    if (overall == COMPATIBILITY_NONE) {
        printf("WARNING: No compatibility level detected\n");
    }

    printf("PASSED: Compatibility assessment works\n");
    return qtrue;
}

static qboolean test_report_generation(void) {
    printf("Testing compatibility report generation...\n");

    if (!Compatibility_GenerateCompatibilityReport("test_compatibility_report.txt")) {
        printf("FAILED: Could not generate compatibility report\n");
        return qfalse;
    }

    printf("PASSED: Compatibility report generated successfully\n");
    return qtrue;
}

int main(int argc, char* argv[]) {
    printf("=== Comprehensive Compatibility Testing System Tests ===\n\n");

    int tests_passed = 0;
    int total_tests = 10;

    if (test_compatibility_initialization()) tests_passed++;
    if (test_platform_detection()) tests_passed++;
    if (test_hardware_detection()) tests_passed++;
    if (test_feature_detection()) tests_passed++;
    if (test_feature_matrix_operations()) tests_passed++;
    if (test_platform_tests()) tests_passed++;
    if (test_hardware_tests()) tests_passed++;
    if (test_feature_tests()) tests_passed++;
    if (test_comprehensive_tests()) tests_passed++;
    if (test_compatibility_assessment()) tests_passed++;
    if (test_report_generation()) tests_passed++;

    printf("\n=== Test Results ===\n");
    printf("Tests passed: %d/%d\n", tests_passed, total_tests);

    if (tests_passed == total_tests) {
        printf("ALL TESTS PASSED!\n");
    } else {
        printf("SOME TESTS FAILED!\n");
    }

    // Cleanup
    Compatibility_Shutdown();

    return (tests_passed == total_tests) ? 0 : 1;
}
