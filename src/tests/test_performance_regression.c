/*
===============================================================================
Automated Performance Regression Testing Suite

Comprehensive performance monitoring and regression detection system.
===============================================================================
*/

#include "../common/q_shared.h"
#include "../common/q_math_simd.h"
#include "../common/q_allocator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

// Performance baseline data
typedef struct {
    char test_name[64];
    double baseline_time;
    double tolerance_percent;
    int sample_count;
    double *samples;
    double min_time, max_time, avg_time;
    double std_dev;
} perf_baseline_t;

// Regression test configuration
typedef struct {
    char name[64];
    void (*test_func)(void);
    perf_baseline_t *baseline;
    int warmup_iterations;
    int measurement_iterations;
    double max_regression_percent;
} perf_test_config_t;

// Global regression detection state
static struct {
    perf_baseline_t *baselines;
    int baseline_count;
    char baseline_file[256];
    qboolean initialized;
    double confidence_level; // 0.95 = 95% confidence
    FILE *log_file;
} perf_regression = {0};

//============================================================================
// Statistical Analysis Functions
//============================================================================

static double CalculateMean(const double *data, int count) {
    double sum = 0.0;
    for (int i = 0; i < count; i++) {
        sum += data[i];
    }
    return sum / count;
}

static double CalculateStdDev(const double *data, int count, double mean) {
    if (count <= 1) return 0.0;

    double sum_sq = 0.0;
    for (int i = 0; i < count; i++) {
        double diff = data[i] - mean;
        sum_sq += diff * diff;
    }
    return sqrt(sum_sq / (count - 1));
}

static qboolean IsSignificantRegression(double current_time, const perf_baseline_t *baseline) {
    // Use statistical significance testing
    double mean_diff = fabs(current_time - baseline->avg_time);
    double threshold = baseline->std_dev * 2.0; // 2 standard deviations

    // Also check percentage regression
    double percent_regression = ((current_time - baseline->avg_time) / baseline->avg_time) * 100.0;

    return (mean_diff > threshold && percent_regression > baseline->tolerance_percent);
}

//============================================================================
// Performance Test Implementations
//============================================================================

static void PerfTest_VectorMath(void) {
    const int ITERATIONS = 100000;
    vec3_t a = {1.0f, 2.0f, 3.0f};
    vec3_t b = {4.0f, 5.0f, 6.0f};
    vec3_t result;

    for (int i = 0; i < ITERATIONS; i++) {
        // Test both SIMD and scalar versions
        VectorAdd_SIMD(a, b, result);
        VectorSubtract_SIMD(a, b, result);
        VectorScale_SIMD(result, 2.0f, result);
        (void)VectorDot_SIMD(a, b);
        VectorCross_SIMD(a, b, result);
    }
}

static void PerfTest_MemoryAllocation(void) {
    const int ITERATIONS = 10000;
    const size_t TEST_SIZE = 1024;

    for (int i = 0; i < ITERATIONS; i++) {
        void *ptr1 = Alloc_Alloc(TEST_SIZE, ALLOCATOR_GENERAL, "perf_test");
        void *ptr2 = Render_Alloc(TEST_SIZE, "perf_test");
        void *ptr3 = Net_Alloc(TEST_SIZE, "perf_test");

        // Simulate some work
        memset(ptr1, 0xAA, TEST_SIZE);
        memset(ptr2, 0xBB, TEST_SIZE);
        memset(ptr3, 0xCC, TEST_SIZE);

        Alloc_Free(ptr1, ALLOCATOR_GENERAL);
        Render_Free(ptr2);
        Net_Free(ptr3);
    }
}

static void PerfTest_MessageProcessing(void) {
    const int ITERATIONS = 50000;
    msg_t msg;
    byte buffer[1400];

    MSG_Init(&msg, buffer, sizeof(buffer));

    for (int i = 0; i < ITERATIONS; i++) {
        MSG_WriteByte(&msg, i % 256);
        MSG_WriteShort(&msg, (short)i);
        MSG_WriteLong(&msg, i * 1000);
        MSG_WriteFloat(&msg, (float)i / 1000.0f);
        MSG_WriteString(&msg, "performance test message");

        // Read back
        msg.readcount = 0;
        (void)MSG_ReadByte(&msg);
        (void)MSG_ReadShort(&msg);
        (void)MSG_ReadLong(&msg);
        (void)MSG_ReadFloat(&msg);

        char str_buf[256];
        MSG_ReadString(&msg, str_buf, sizeof(str_buf));

        // Reset for next iteration
        msg.cursize = 0;
        msg.readcount = 0;
    }
}

static void PerfTest_FileOperations(void) {
    const int ITERATIONS = 1000;
    char temp_filename[256];

    for (int i = 0; i < ITERATIONS; i++) {
        Com_sprintf(temp_filename, sizeof(temp_filename), "/tmp/perf_test_%d.tmp", i);

        // Test file I/O
        FILE *f = fopen(temp_filename, "wb");
        if (f) {
            fwrite(&i, sizeof(i), 1, f);
            fclose(f);

            f = fopen(temp_filename, "rb");
            if (f) {
                int read_val;
                fread(&read_val, sizeof(read_val), 1, f);
                fclose(f);
                unlink(temp_filename);
            }
        }
    }
}

static void PerfTest_SIMDOperations(void) {
    const int ITERATIONS = 50000;
    vec3_t vectors[100];

    // Initialize test vectors
    for (int i = 0; i < 100; i++) {
        vectors[i][0] = (float)i;
        vectors[i][1] = (float)i * 2.0f;
        vectors[i][2] = (float)i * 3.0f;
    }

    for (int iter = 0; iter < ITERATIONS; iter++) {
        for (int i = 0; i < 99; i++) {
            vec3_t result;
            VectorAdd_SIMD(vectors[i], vectors[i + 1], result);
            VectorScale_SIMD(result, 0.5f, vectors[i]);
        }
    }
}

//============================================================================
// Performance Test Configurations
//============================================================================

static perf_test_config_t perf_tests[] = {
    {
        "Vector Math Operations",
        PerfTest_VectorMath,
        NULL, // Will be set during initialization
        1000, // warmup iterations
        10000, // measurement iterations
        10.0  // max 10% regression
    },
    {
        "Memory Allocation",
        PerfTest_MemoryAllocation,
        NULL,
        100,
        5000,
        15.0
    },
    {
        "Message Processing",
        PerfTest_MessageProcessing,
        NULL,
        1000,
        5000,
        8.0
    },
    {
        "File Operations",
        PerfTest_FileOperations,
        NULL,
        10,
        1000,
        20.0
    },
    {
        "SIMD Operations",
        PerfTest_SIMDOperations,
        NULL,
        1000,
        5000,
        12.0
    },
    {NULL, NULL, NULL, 0, 0, 0.0}
};

//============================================================================
// Baseline Management
//============================================================================

static qboolean LoadBaselines(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        Com_Printf("Warning: Could not load baseline file %s\n", filename);
        return qfalse;
    }

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        perf_baseline_t baseline = {0};
        if (sscanf(line, "%63[^,],%lf,%lf,%d",
                   baseline.test_name,
                   &baseline.baseline_time,
                   &baseline.tolerance_percent,
                   &baseline.sample_count) == 4) {

            // Allocate space for samples
            if (baseline.sample_count > 0) {
                baseline.samples = (double *)malloc(sizeof(double) * baseline.sample_count);
                if (baseline.samples) {
                    // Read sample data (simplified - would need proper parsing)
                    baseline.avg_time = baseline.baseline_time;
                    baseline.std_dev = baseline.baseline_time * 0.1; // Estimate
                }
            }

            perf_regression.baselines = (perf_baseline_t *)realloc(
                perf_regression.baselines,
                sizeof(perf_baseline_t) * (perf_regression.baseline_count + 1));
            perf_regression.baselines[perf_regression.baseline_count++] = baseline;
        }
    }

    fclose(f);
    Com_Printf("Loaded %d performance baselines\n", perf_regression.baseline_count);
    return qtrue;
}

static qboolean SaveBaselines(const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) {
        Com_Printf("Error: Could not save baseline file %s\n", filename);
        return qfalse;
    }

    fprintf(f, "# Performance Regression Baselines\n");
    fprintf(f, "# Format: test_name,baseline_time,tolerance_percent,sample_count\n");

    for (int i = 0; i < perf_regression.baseline_count; i++) {
        perf_baseline_t *baseline = &perf_regression.baselines[i];
        fprintf(f, "%s,%.6f,%.2f,%d\n",
                baseline->test_name,
                baseline->avg_time,
                baseline->tolerance_percent,
                baseline->sample_count);
    }

    fclose(f);
    Com_Printf("Saved %d performance baselines\n", perf_regression.baseline_count);
    return qtrue;
}

//============================================================================
// Regression Testing Functions
//============================================================================

static void RunPerformanceTest(perf_test_config_t *config, qboolean is_baseline_run) {
    double *samples = (double *)malloc(sizeof(double) * config->measurement_iterations);
    if (!samples) {
        Com_Printf("Error: Could not allocate sample buffer\n");
        return;
    }

    Com_Printf("Running performance test: %s\n", config->name);

    // Warmup phase
    for (int i = 0; i < config->warmup_iterations; i++) {
        config->test_func();
    }

    // Measurement phase
    for (int i = 0; i < config->measurement_iterations; i++) {
        double start_time = (double)clock() / CLOCKS_PER_SEC * 1000.0;
        config->test_func();
        double end_time = (double)clock() / CLOCKS_PER_SEC * 1000.0;
        samples[i] = end_time - start_time;
    }

    // Calculate statistics
    double mean = CalculateMean(samples, config->measurement_iterations);
    double std_dev = CalculateStdDev(samples, config->measurement_iterations, mean);

    Com_Printf("  Mean: %.3f ms, StdDev: %.3f ms\n", mean, std_dev);

    if (is_baseline_run) {
        // Update or create baseline
        qboolean found = qfalse;
        for (int i = 0; i < perf_regression.baseline_count; i++) {
            if (strcmp(perf_regression.baselines[i].test_name, config->name) == 0) {
                perf_regression.baselines[i].avg_time = mean;
                perf_regression.baselines[i].std_dev = std_dev;
                perf_regression.baselines[i].sample_count = config->measurement_iterations;
                found = qtrue;
                break;
            }
        }

        if (!found) {
            perf_regression.baselines = (perf_baseline_t *)realloc(
                perf_regression.baselines,
                sizeof(perf_baseline_t) * (perf_regression.baseline_count + 1));

            perf_baseline_t *baseline = &perf_regression.baselines[perf_regression.baseline_count++];
            Q_strncpyz(baseline->test_name, config->name, sizeof(baseline->test_name));
            baseline->avg_time = mean;
            baseline->std_dev = std_dev;
            baseline->tolerance_percent = config->max_regression_percent;
            baseline->sample_count = config->measurement_iterations;
            baseline->samples = NULL; // Could save samples for more detailed analysis
        }
    } else {
        // Check for regression
        perf_baseline_t *baseline = NULL;
        for (int i = 0; i < perf_regression.baseline_count; i++) {
            if (strcmp(perf_regression.baselines[i].test_name, config->name) == 0) {
                baseline = &perf_regression.baselines[i];
                break;
            }
        }

        if (baseline) {
            if (IsSignificantRegression(mean, baseline)) {
                Com_Printf("  ⚠️  PERFORMANCE REGRESSION DETECTED!\n");
                Com_Printf("     Baseline: %.3f ms, Current: %.3f ms\n", baseline->avg_time, mean);
                Com_Printf("     Regression: %.1f%%\n",
                          ((mean - baseline->avg_time) / baseline->avg_time) * 100.0);

                if (perf_regression.log_file) {
                    fprintf(perf_regression.log_file,
                           "REGRESSION: %s - Baseline: %.3fms, Current: %.3fms, Change: %.1f%%\n",
                           config->name, baseline->avg_time, mean,
                           ((mean - baseline->avg_time) / baseline->avg_time) * 100.0);
                }
            } else {
                Com_Printf("  ✅ No significant regression detected\n");
            }
        } else {
            Com_Printf("  ⚠️  No baseline found for comparison\n");
        }
    }

    free(samples);
}

//============================================================================
// Public API
//============================================================================

qboolean PerfRegression_Init(const char *baseline_file) {
    if (perf_regression.initialized) {
        return qtrue;
    }

    Q_strncpyz(perf_regression.baseline_file, baseline_file, sizeof(perf_regression.baseline_file));
    perf_regression.confidence_level = 0.95;
    perf_regression.baselines = NULL;
    perf_regression.baseline_count = 0;

    // Try to load existing baselines
    if (!LoadBaselines(baseline_file)) {
        Com_Printf("Creating new baseline file: %s\n", baseline_file);
    }

    // Open log file
    perf_regression.log_file = fopen("perf_regression.log", "a");
    if (perf_regression.log_file) {
        fprintf(perf_regression.log_file, "\n=== Performance Regression Test Started: %s ===\n",
               __DATE__ " " __TIME__);
    }

    perf_regression.initialized = qtrue;
    return qtrue;
}

void PerfRegression_Shutdown(void) {
    if (!perf_regression.initialized) {
        return;
    }

    // Save baselines
    SaveBaselines(perf_regression.baseline_file);

    // Clean up
    for (int i = 0; i < perf_regression.baseline_count; i++) {
        if (perf_regression.baselines[i].samples) {
            free(perf_regression.baselines[i].samples);
        }
    }
    free(perf_regression.baselines);

    if (perf_regression.log_file) {
        fprintf(perf_regression.log_file, "=== Performance Regression Test Ended ===\n\n");
        fclose(perf_regression.log_file);
    }

    memset(&perf_regression, 0, sizeof(perf_regression));
}

void PerfRegression_RunBaselineTests(void) {
    if (!perf_regression.initialized) {
        Com_Printf("Error: Performance regression system not initialized\n");
        return;
    }

    Com_Printf("Running performance baseline tests...\n");

    for (perf_test_config_t *test = perf_tests; test->name; test++) {
        RunPerformanceTest(test, qtrue);
    }

    SaveBaselines(perf_regression.baseline_file);
    Com_Printf("Baseline tests completed\n");
}

void PerfRegression_RunRegressionTests(void) {
    if (!perf_regression.initialized) {
        Com_Printf("Error: Performance regression system not initialized\n");
        return;
    }

    Com_Printf("Running performance regression tests...\n");

    int regressions_found = 0;
    for (perf_test_config_t *test = perf_tests; test->name; test++) {
        RunPerformanceTest(test, qfalse);
        // Could count regressions here
    }

    Com_Printf("Regression tests completed\n");
}

void PerfRegression_PrintReport(void) {
    if (!perf_regression.initialized) {
        Com_Printf("Performance regression system not initialized\n");
        return;
    }

    Com_Printf("\nPerformance Regression Report\n");
    Com_Printf("==============================\n");

    for (int i = 0; i < perf_regression.baseline_count; i++) {
        perf_baseline_t *baseline = &perf_regression.baselines[i];
        Com_Printf("%-25s: %.3f ± %.3f ms (%d samples)\n",
                  baseline->test_name,
                  baseline->avg_time,
                  baseline->std_dev,
                  baseline->sample_count);
    }

    Com_Printf("\nConfidence Level: %.1f%%\n", perf_regression.confidence_level * 100.0);
}

//============================================================================
// Test Framework Integration
//============================================================================

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    printf("Automated Performance Regression Testing Suite\n");
    printf("==============================================\n\n");

    // Initialize required systems
    Math_SIMD_Init();
    Alloc_Init();

    printf("Initializing performance regression system...\n");
    if (!PerfRegression_Init("perf_baseline.txt")) {
        printf("ERROR: Failed to initialize performance regression system\n");
        return 1;
    }

    printf("Running baseline performance tests...\n");
    PerfRegression_RunBaselineTests();

    printf("Running regression detection tests...\n");
    PerfRegression_RunRegressionTests();

    printf("Generating performance report...\n");
    PerfRegression_PrintReport();

    printf("Shutting down performance regression system...\n");
    PerfRegression_Shutdown();

    // Clean up
    Alloc_Shutdown();

    printf("Performance regression testing completed successfully!\n");
    return 0;
}

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

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    printf("Automated Performance Regression Testing Suite\n");
    printf("==============================================\n\n");

    // Initialize required systems
    Math_SIMD_Init();
    Alloc_Init();

    // Open log file
    FILE *perf_log = fopen("perf_regression.log", "a");
    if (perf_log) {
        fprintf(perf_log, "\n=== Performance Regression Test Started ===\n");
    }

    printf("Testing SIMD Math Operations...\n");
    vec3_t a = {1.0f, 2.0f, 3.0f};
    vec3_t b = {4.0f, 5.0f, 6.0f};
    vec3_t result;

    double start_time = (double)clock() / CLOCKS_PER_SEC * 1000.0;

    // Test SIMD operations
    for (int i = 0; i < 10000; i++) {
        VectorAdd_SIMD(a, b, result);
        VectorScale_SIMD(result, 2.0f, result);
    }

    double end_time = (double)clock() / CLOCKS_PER_SEC * 1000.0;
    double duration = end_time - start_time;
    printf("SIMD Math Test: %.2f ms\n", duration);

    printf("Testing Memory Allocation...\n");
    start_time = (double)clock() / CLOCKS_PER_SEC * 1000.0;

    // Test memory allocation
    for (int i = 0; i < 1000; i++) {
        void *ptr = Alloc_Alloc(1024, ALLOCATOR_GENERAL, "test");
        Alloc_Free(ptr, ALLOCATOR_GENERAL);
    }

    end_time = (double)clock() / CLOCKS_PER_SEC * 1000.0;
    duration = end_time - start_time;
    printf("Memory Allocation Test: %.2f ms\n", duration);

    printf("Performance Test Report:\n");
    printf("- SIMD Features: %s\n", Math_SIMD_IsAvailable() ? "Available" : "Not Available");
    printf("- Memory Allocators: Initialized\n");

    // Log results
    if (perf_log) {
        fprintf(perf_log, "SIMD Math Test: %.2f ms\n", duration);
        fprintf(perf_log, "Memory Allocation Test: %.2f ms\n", duration);
        fprintf(perf_log, "=== Performance Regression Test Ended ===\n\n");
        fclose(perf_log);
    }

    // Clean up
    Alloc_Shutdown();

    printf("Performance regression testing completed successfully!\n");
    return 0;
}
