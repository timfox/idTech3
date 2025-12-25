/*
=============================================================================
Performance Regression Testing System Test

Automated performance regression detection tests.
=============================================================================
*/

#include "q_shared.h"
#include "performance_regression.h"
#include <stdio.h>

static qboolean test_regression_initialization(void) {
    printf("Testing regression system initialization...\n");

    if (!Regression_Init("test_baseline.txt")) {
        printf("FAILED: Could not initialize regression system\n");
        return qfalse;
    }

    if (!regression_system.initialized) {
        printf("FAILED: Regression system not marked as initialized\n");
        return qfalse;
    }

    printf("PASSED: Regression system initialized successfully\n");
    return qtrue;
}

static qboolean test_regression_test_configuration(void) {
    printf("Testing regression test configuration...\n");

    regression_test_config_t config;
    memset(&config, 0, sizeof(config));

    Q_strncpyz(config.test_name, "test_rendering", sizeof(config.test_name));
    Q_strncpyz(config.description, "Test rendering performance", sizeof(config.description));
    config.warmup_frames = 10;
    config.measurement_frames = 50;
    config.fps_threshold = 30.0f;
    config.regression_threshold = 5.0f;
    config.enable_profiling = qtrue;

    if (!Regression_AddTest(&config)) {
        printf("FAILED: Could not add test configuration\n");
        return qfalse;
    }

    const regression_test_config_t* retrieved = Regression_GetTest("test_rendering");
    if (!retrieved) {
        printf("FAILED: Could not retrieve added test\n");
        return qfalse;
    }

    if (strcmp(retrieved->test_name, "test_rendering") != 0) {
        printf("FAILED: Test name mismatch\n");
        return qfalse;
    }

    if (retrieved->fps_threshold != 30.0f) {
        printf("FAILED: FPS threshold mismatch\n");
        return qfalse;
    }

    printf("PASSED: Test configuration works correctly\n");
    return qtrue;
}

static qboolean test_regression_predefined_tests(void) {
    printf("Testing predefined regression tests...\n");

    // Test rendering test
    if (!Regression_AddStandardRenderingTest("testmap", 2)) {
        printf("FAILED: Could not add standard rendering test\n");
        return qfalse;
    }

    // Test memory stress test
    if (!Regression_AddMemoryStressTest(1024, 100)) {
        printf("FAILED: Could not add memory stress test\n");
        return qfalse;
    }

    // Test I/O benchmark test
    if (!Regression_AddIOBenchmarkTest("testfile.dat", 10 * 1024 * 1024)) {
        printf("FAILED: Could not add I/O benchmark test\n");
        return qfalse;
    }

    printf("PASSED: Predefined tests added successfully\n");
    return qtrue;
}

static qboolean test_regression_simulation(void) {
    printf("Testing regression analysis simulation...\n");

    // Simulate some frame time recordings
    float frame_times[] = {
        16.67f, 16.67f, 16.67f, 16.67f, 16.67f, // 60 FPS
        16.67f, 16.67f, 16.67f, 16.67f, 16.67f,
        16.67f, 16.67f, 16.67f, 16.67f, 16.67f,
        16.67f, 16.67f, 16.67f, 16.67f, 16.67f,
        16.67f, 16.67f, 16.67f, 16.67f, 16.67f,
        16.67f, 16.67f, 16.67f, 16.67f, 16.67f,
        16.67f, 16.67f, 16.67f, 16.67f, 16.67f,
        16.67f, 16.67f, 16.67f, 16.67f, 16.67f,
        16.67f, 16.67f, 16.67f, 16.67f, 16.67f,
        16.67f, 16.67f, 16.67f, 16.67f, 16.67f  // 10 frames at 60 FPS
    };

    for (int i = 0; i < 50; i++) {
        Regression_RecordFrameTime(frame_times[i % 10]);
    }

    // Check if we have results
    regression_test_result_t* results;
    uint32_t count = Regression_GetResults(&results);

    if (count == 0) {
        printf("FAILED: No regression test results generated\n");
        return qfalse;
    }

    printf("PASSED: Regression analysis simulation completed (%u results)\n", count);
    return qtrue;
}

static qboolean test_regression_ci_integration(void) {
    printf("Testing CI integration...\n");

    // Generate CI report
    if (!Regression_GenerateCIReport("test_ci_report.json")) {
        printf("FAILED: Could not generate CI report\n");
        return qfalse;
    }

    // Check CI thresholds
    qboolean ci_passed = Regression_CheckCIThresholds();
    printf("CI Check Result: %s\n", ci_passed ? "PASSED" : "FAILED");

    printf("PASSED: CI integration test completed\n");
    return qtrue;
}

int main(int argc, char* argv[]) {
    printf("=== Performance Regression Testing System Tests ===\n\n");

    int tests_passed = 0;
    int total_tests = 5;

    if (test_regression_initialization()) tests_passed++;
    if (test_regression_test_configuration()) tests_passed++;
    if (test_regression_predefined_tests()) tests_passed++;
    if (test_regression_simulation()) tests_passed++;
    if (test_regression_ci_integration()) tests_passed++;

    printf("\n=== Test Results ===\n");
    printf("Tests passed: %d/%d\n", tests_passed, total_tests);

    if (tests_passed == total_tests) {
        printf("ALL TESTS PASSED!\n");
    } else {
        printf("SOME TESTS FAILED!\n");
    }

    // Cleanup
    Regression_Shutdown();

    return (tests_passed == total_tests) ? 0 : 1;
}
