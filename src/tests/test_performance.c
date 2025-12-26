/*
=============================================================================
Performance Test Demo

Demonstrates the automated performance validation and regression testing system.
=============================================================================
*/

#include "../src/common/q_shared.h"
#include "../src/common/perf_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char* argv[]) {
    printf("Performance Test System Demo\n");
    printf("===========================\n\n");

    // Initialize the performance test system
    if (!PerfTest_Init()) {
        printf("Failed to initialize performance test system\n");
        return 1;
    }

    printf("Performance test system initialized successfully\n\n");

    // Create a test suite
    perf_test_suite_t* suite = PerfTest_CreateSuite("demo_suite", "Demonstration test suite");
    if (!suite) {
        printf("Failed to create test suite\n");
        PerfTest_Shutdown();
        return 1;
    }

    printf("Created test suite: %s\n", suite->suite_name);
    printf("Description: %s\n\n", suite->description);

    // Add some performance tests to the suite
    perf_test_config_t test1;
    memset(&test1, 0, sizeof(test1));
    Q_strncpyz(test1.name, "basic_rendering", sizeof(test1.name));
    Q_strncpyz(test1.description, "Basic rendering performance test", sizeof(test1.description));
    test1.duration_seconds = 10;
    test1.warmup_seconds = 2;
    test1.sample_interval_ms = 100;

    perf_test_config_t test2;
    memset(&test2, 0, sizeof(test2));
    Q_strncpyz(test2.name, "memory_allocation", sizeof(test2.name));
    Q_strncpyz(test2.description, "Memory allocation performance test", sizeof(test2.description));
    test2.duration_seconds = 5;
    test2.warmup_seconds = 1;
    test2.sample_interval_ms = 50;

    perf_test_config_t test3;
    memset(&test3, 0, sizeof(test3));
    Q_strncpyz(test3.name, "asset_loading", sizeof(test3.name));
    Q_strncpyz(test3.description, "Asset loading performance test", sizeof(test3.description));
    test3.duration_seconds = 8;
    test3.warmup_seconds = 3;
    test3.sample_interval_ms = 200;

    if (PerfTest_AddTestToSuite(suite, &test1) &&
        PerfTest_AddTestToSuite(suite, &test2) &&
        PerfTest_AddTestToSuite(suite, &test3)) {
        printf("Added 3 tests to suite:\n");
        printf("  - %s (%d seconds)\n", test1.name, test1.duration_seconds);
        printf("  - %s (%d seconds)\n", test2.name, test2.duration_seconds);
        printf("  - %s (%d seconds)\n", test3.name, test3.duration_seconds);
    } else {
        printf("Failed to add tests to suite\n");
    }

    printf("\nRunning test suite...\n");
    printf("===================\n");

    // Run the test suite
    qboolean suite_success = PerfTest_RunSuite(suite);

    printf("\nTest suite completed: %s\n", suite_success ? "SUCCESS" : "FAILURE");

    // Set baselines for the tests (normally done after establishing good performance)
    printf("\nSetting performance baselines...\n");
    PerfTest_SetBaseline("basic_rendering", &(perf_test_result_t){
        .avg_fps = 60.0,
        .min_fps = 58.0,
        .avg_frame_time = 16.67,
        .max_frame_time = 18.0,
        .avg_cpu_usage = 45.0,
        .avg_memory_usage = 512.0
    });

    PerfTest_SetBaseline("memory_allocation", &(perf_test_result_t){
        .avg_fps = 120.0,
        .min_fps = 115.0,
        .avg_frame_time = 8.33,
        .max_frame_time = 9.0,
        .avg_cpu_usage = 30.0,
        .avg_memory_usage = 256.0
    });

    printf("Baselines set for demo tests\n");

    // Generate a report
    printf("\nGenerating performance report...\n");
    uint32_t result_count;
    perf_test_result_t* results = PerfTest_GetSuiteResults(suite, &result_count);

    if (PerfTest_GenerateReport(results, result_count, "perf_demo_report.json", "JSON")) {
        printf("Performance report generated: perf_demo_report.json\n");
    }

    // Export for CI
    printf("\nExporting results for CI...\n");
    if (PerfTest_ExportForCI(results, result_count, "ci_results")) {
        printf("Results exported to ci_results directory\n");
    }

    // Show final statistics
    printf("\nFinal Statistics:\n");
    printf("================\n");
    uint64_t total_tests, regressions, test_time;
    PerfTest_GetStats(&total_tests, NULL, &test_time);
    printf("Total tests executed: %llu\n", (unsigned long long)total_tests);
    printf("Total execution time: %.2f seconds\n", test_time / 1000.0f);

    // List baselines
    printf("\nPerformance Baselines:\n");
    printf("=====================\n");
    for (uint32_t i = 0; i < perf_test_system.num_baselines; i++) {
        const perf_baseline_t* baseline = &perf_test_system.baselines[i];
        printf("%s:\n", baseline->test_name);
        printf("  FPS: %.1f avg, %.1f min\n", baseline->baseline_fps_avg, baseline->baseline_fps_min);
        printf("  Frame Time: %.2f ms avg, %.2f ms max\n",
               baseline->baseline_frame_time_avg, baseline->baseline_frame_time_max);
        printf("  Regression threshold: %.1f%%\n", baseline->regression_threshold_percent);
    }

    // Clean up
    if (suite) {
        free(suite->tests);
        free(suite);
    }

    PerfTest_Shutdown();
    printf("\nPerformance test demo completed successfully!\n");

    return 0;
}
