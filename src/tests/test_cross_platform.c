/*
=============================================================================
Cross-Platform Compatibility Test Demo

Demonstrates the automated cross-platform compatibility testing system.
=============================================================================
*/

#include "../src/common/q_shared.h"
#include "../src/common/cross_platform_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char* argv[]) {
    printf("Cross-Platform Compatibility Test Demo\n");
    printf("=======================================\n\n");

    // Initialize the cross-platform test system
    if (!CrossPlatformTest_Init()) {
        printf("Failed to initialize cross-platform test system\n");
        return 1;
    }

    printf("Cross-platform test system initialized successfully\n\n");

    // Detect platform information
    platform_info_t platform;
    if (CrossPlatformTest_DetectPlatform(&platform)) {
        printf("=== Platform Detection ===\n");
        printf("Platform: %s\n", platform.name);
        printf("Architecture: %s (%d-bit)\n", platform.arch.name, platform.arch.bits);
        printf("Compiler: %s %s\n", platform.compiler.name, platform.compiler.version);
        printf("Little Endian: %s\n", platform.arch.little_endian ? "Yes" : "No");
        printf("SIMD Support: %s\n", platform.arch.supports_simd ? "Yes" : "No");
        printf("64-bit Atomics: %s\n\n", platform.arch.supports_atomic64 ? "Yes" : "No");
    }

    // Create a comprehensive test suite
    cross_platform_test_suite_t* suite = CrossPlatformTest_CreateSuite(
        "comprehensive_suite", "Comprehensive Cross-Platform Compatibility Test Suite");

    if (!suite) {
        printf("Failed to create test suite\n");
        CrossPlatformTest_Shutdown();
        return 1;
    }

    printf("Created test suite: %s\n", suite->suite_name);
    printf("Description: %s\n\n", suite->description);

    // Generate platform-specific tests
    printf("Generating platform-specific tests...\n");
    if (CrossPlatformTest_GeneratePlatformTests(suite)) {
        printf("Successfully generated platform tests\n");
    } else {
        printf("Failed to generate platform tests\n");
    }

    // Add some specific tests manually
    cross_platform_test_config_t manual_tests[] = {
        {"basic_functionality", "Test basic C functionality", PLATFORM_UNKNOWN, ARCH_UNKNOWN, qfalse, qfalse, qfalse, qfalse, 10},
        {"memory_management", "Test memory allocation and management", PLATFORM_UNKNOWN, ARCH_UNKNOWN, qfalse, qfalse, qfalse, qfalse, 15},
        {"file_system", "Test file system operations", PLATFORM_UNKNOWN, ARCH_UNKNOWN, qfalse, qfalse, qfalse, qfalse, 20},
        {"time_and_date", "Test time and date functions", PLATFORM_UNKNOWN, ARCH_UNKNOWN, qfalse, qfalse, qfalse, qfalse, 5},
        {"math_precision", "Test mathematical precision", PLATFORM_UNKNOWN, ARCH_UNKNOWN, qfalse, qfalse, qfalse, qfalse, 10},
        {"unicode_support", "Test Unicode string handling", PLATFORM_UNKNOWN, ARCH_UNKNOWN, qfalse, qfalse, qfalse, qfalse, 10}
    };

    for (int i = 0; i < sizeof(manual_tests) / sizeof(manual_tests[0]); i++) {
        if (!CrossPlatformTest_AddTestToSuite(suite, &manual_tests[i])) {
            printf("Failed to add test: %s\n", manual_tests[i].test_name);
        }
    }

    printf("Added %d manual tests to suite\n", sizeof(manual_tests) / sizeof(manual_tests[0]));
    printf("Total tests in suite: %u\n\n", suite->num_tests);

    // Run the test suite
    printf("Running cross-platform compatibility test suite...\n");
    printf("===================================================\n");

    qboolean suite_success = CrossPlatformTest_RunSuite(suite);

    printf("\nTest suite completed: %s\n", suite_success ? "SUCCESS" : "FAILURE");

    // Show test results
    cross_platform_test_result_t* results;
    uint32_t result_count = CrossPlatformTest_GetResults(&results);

    printf("\n=== Test Results Summary ===\n");
    printf("Total Tests Run: %u\n", result_count);

    uint32_t passed = 0, failed = 0, skipped = 0;
    for (uint32_t i = 0; i < result_count; i++) {
        switch (results[i].result) {
            case COMPAT_RESULT_PASS: passed++; break;
            case COMPAT_RESULT_FAIL: failed++; break;
            case COMPAT_RESULT_SKIP: skipped++; break;
            default: break;
        }
    }

    printf("Passed: %u\n", passed);
    printf("Failed: %u\n", failed);
    printf("Skipped: %u\n", skipped);

    if (result_count > 0) {
        float pass_rate = (float)passed / result_count * 100.0f;
        printf("Pass Rate: %.1f%%\n", pass_rate);
    }

    // Show detailed results for first few tests
    printf("\n=== Detailed Results (first 10) ===\n");
    for (uint32_t i = 0; i < result_count && i < 10; i++) {
        const cross_platform_test_result_t* result = &results[i];
        printf("%-20s: %s (%.2fs)",
               result->test_name,
               CrossPlatformTest_GetResultString(result->result),
               result->duration_ms / 1000.0f);

        if (result->error_message[0]) {
            printf(" - %s", result->error_message);
        }
        printf("\n");
    }

    if (result_count > 10) {
        printf("... and %u more results\n", result_count - 10);
    }

    // Validate platform compatibility
    printf("\n=== Platform Compatibility Validation ===\n");

    qboolean requirements_met = CrossPlatformTest_CheckMinimumRequirements();
    printf("Minimum Requirements: %s\n", requirements_met ? "MET" : "NOT MET");

    qboolean platform_valid = CrossPlatformTest_ValidatePlatformCompatibility();
    printf("Platform Compatibility: %s\n", platform_valid ? "VALID" : "INVALID");

    qboolean caps_ok = CrossPlatformTest_TestPlatformCapabilities();
    printf("Platform Capabilities: %s\n", caps_ok ? "OK" : "ISSUES DETECTED");

    qboolean arch_ok = CrossPlatformTest_TestArchitectureFeatures();
    printf("Architecture Features: %s\n", arch_ok ? "OK" : "ISSUES DETECTED");

    qboolean compiler_ok = CrossPlatformTest_TestCompilerFeatures();
    printf("Compiler Features: %s\n", compiler_ok ? "OK" : "ISSUES DETECTED");

    qboolean overall = requirements_met && platform_valid && caps_ok && arch_ok && compiler_ok;
    printf("\nOverall Compatibility: %s\n", overall ? "COMPATIBLE" : "NOT COMPATIBLE");

    // Export results for CI
    printf("\nExporting results for CI...\n");
    if (CrossPlatformTest_ExportForCI("cross_platform_ci_results")) {
        printf("Results exported to cross_platform_ci_results directory\n");
    }

    // Generate a report
    printf("\nGenerating compatibility report...\n");
    if (CrossPlatformTest_GenerateCIReport("compatibility_report.json", "JSON")) {
        printf("Compatibility report generated: compatibility_report.json\n");
    }

    // Clean up
    if (suite) {
        free(suite->tests);
        free(suite);
    }

    CrossPlatformTest_Shutdown();
    printf("\nCross-platform compatibility test demo completed successfully!\n");

    return suite_success ? 0 : 1;
}
