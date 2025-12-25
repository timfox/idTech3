/*
=============================================================================
Performance Regression Testing System Implementation

Automated performance regression detection with simplified interface.
=============================================================================
*/

#include "performance_regression.h"
#include "q_shared.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Global regression testing system
regression_system_t regression_system = {0};

// Frame time buffer size
#define MAX_FRAME_TIMES 10000
#define BASELINE_VERSION 1

/*
=============================================================================
Performance Regression API Implementation
=============================================================================
*/

qboolean Regression_Init(const char* baseline_file) {
    if (regression_system.initialized) {
        return qtrue;
    }

    memset(&regression_system, 0, sizeof(regression_system_t));

    // Set baseline file
    if (baseline_file) {
        Q_strncpyz(regression_system.baseline_file, baseline_file, sizeof(regression_system.baseline_file));
    } else {
        Q_strncpyz(regression_system.baseline_file, "performance_baseline.txt", sizeof(regression_system.baseline_file));
    }

    // Allocate test configurations
    regression_system.max_tests = 50;
    regression_system.tests = (regression_test_config_t*)malloc(
        sizeof(regression_test_config_t) * regression_system.max_tests);

    if (!regression_system.tests) {
        Com_Printf("Failed to allocate memory for regression tests\n");
        return qfalse;
    }

    memset(regression_system.tests, 0,
           sizeof(regression_test_config_t) * regression_system.max_tests);

    // Allocate results storage
    regression_system.max_results = 100;
    regression_system.results = (regression_test_result_t*)malloc(
        sizeof(regression_test_result_t) * regression_system.max_results);

    if (!regression_system.results) {
        free(regression_system.tests);
        Com_Printf("Failed to allocate memory for regression results\n");
        return qfalse;
    }

    memset(regression_system.results, 0,
           sizeof(regression_test_result_t) * regression_system.max_results);

    // Allocate frame time buffer
    regression_system.frame_buffer_size = MAX_FRAME_TIMES;
    regression_system.frame_times = (float*)malloc(
        sizeof(float) * regression_system.frame_buffer_size);

    if (!regression_system.frame_times) {
        free(regression_system.tests);
        free(regression_system.results);
        Com_Printf("Failed to allocate memory for frame times\n");
        return qfalse;
    }

    // Set default configuration
    regression_system.auto_update_baseline = qtrue;
    regression_system.confidence_level = 0.95f;
    regression_system.regression_threshold = 5.0f; // 5% change indicates regression
    regression_system.min_samples = 10;

    // Load baseline data
    if (!Regression_LoadBaseline()) {
        Com_Printf("Warning: Could not load performance baseline, starting fresh\n");
    }

    regression_system.initialized = qtrue;
    regression_system.currently_testing = qfalse;

    Com_Printf("Performance regression testing system initialized\n");
    Com_Printf("Baseline file: %s\n", regression_system.baseline_file);

    return qtrue;
}

void Regression_Shutdown(void) {
    if (!regression_system.initialized) {
        return;
    }

    // Save current baseline
    Regression_SaveBaseline();

    // Free resources
    if (regression_system.tests) {
        free(regression_system.tests);
    }

    if (regression_system.results) {
        free(regression_system.results);
    }

    if (regression_system.frame_times) {
        free(regression_system.frame_times);
    }

    regression_system.initialized = qfalse;
    Com_Printf("Performance regression testing system shutdown\n");
}

/*
=============================================================================
Test Configuration
=============================================================================
*/

qboolean Regression_AddTest(const regression_test_config_t* config) {
    if (!regression_system.initialized || !config || regression_system.test_count >= regression_system.max_tests) {
        return qfalse;
    }

    // Check if test already exists
    for (uint32_t i = 0; i < regression_system.test_count; i++) {
        if (Q_stricmp(regression_system.tests[i].test_name, config->test_name) == 0) {
            // Update existing test
            memcpy(&regression_system.tests[i], config, sizeof(regression_test_config_t));
            return qtrue;
        }
    }

    // Add new test
    memcpy(&regression_system.tests[regression_system.test_count++], config, sizeof(regression_test_config_t));
    return qtrue;
}

qboolean Regression_RemoveTest(const char* test_name) {
    if (!regression_system.initialized || !test_name) {
        return qfalse;
    }

    for (uint32_t i = 0; i < regression_system.test_count; i++) {
        if (Q_stricmp(regression_system.tests[i].test_name, test_name) == 0) {
            // Shift remaining tests
            for (uint32_t j = i; j < regression_system.test_count - 1; j++) {
                memcpy(&regression_system.tests[j], &regression_system.tests[j + 1],
                       sizeof(regression_test_config_t));
            }
            regression_system.test_count--;
            return qtrue;
        }
    }

    return qfalse;
}

const regression_test_config_t* Regression_GetTest(const char* test_name) {
    if (!regression_system.initialized || !test_name) {
        return NULL;
    }

    for (uint32_t i = 0; i < regression_system.test_count; i++) {
        if (Q_stricmp(regression_system.tests[i].test_name, test_name) == 0) {
            return &regression_system.tests[i];
        }
    }

    return NULL;
}

/*
=============================================================================
Test Execution
=============================================================================
*/

qboolean Regression_RunTest(const char* test_name) {
    if (!regression_system.initialized || regression_system.currently_testing) {
        return qfalse;
    }

    const regression_test_config_t* config = Regression_GetTest(test_name);
    if (!config) {
        Com_Printf("Regression test not found: %s\n", test_name);
        return qfalse;
    }

    // Initialize test state
    Q_strncpyz(regression_system.current_test, test_name, sizeof(regression_system.current_test));
    regression_system.currently_testing = qtrue;
    regression_system.test_start_time = Sys_Milliseconds();
    regression_system.current_frame_count = 0;
    regression_system.frame_count = 0;

    Com_Printf("Starting regression test: %s\n", test_name);
    Com_Printf("Warmup frames: %d, Measurement frames: %d\n",
               config->warmup_frames, config->measurement_frames);

    return qtrue;
}

qboolean Regression_RunAllTests(void) {
    if (!regression_system.initialized || regression_system.currently_testing) {
        return qfalse;
    }

    Com_Printf("Running all regression tests (%u tests)...\n", regression_system.test_count);

    qboolean all_passed = qtrue;

    for (uint32_t i = 0; i < regression_system.test_count; i++) {
        const char* test_name = regression_system.tests[i].test_name;

        if (!Regression_RunTest(test_name)) {
            all_passed = qfalse;
            continue;
        }

        // Wait for test to complete (simplified - in real implementation would wait for completion signal)
        // For now, just run sequentially
        Sys_Sleep(100); // Brief pause between tests
    }

    return all_passed;
}

qboolean Regression_IsTestRunning(void) {
    return regression_system.currently_testing;
}

qboolean Regression_CancelCurrentTest(void) {
    if (!regression_system.currently_testing) {
        return qfalse;
    }

    Com_Printf("Cancelling regression test: %s\n", regression_system.current_test);
    regression_system.currently_testing = qfalse;
    memset(regression_system.current_test, 0, sizeof(regression_system.current_test));

    return qtrue;
}

void Regression_RecordFrameTime(float frame_time_ms) {
    if (!regression_system.currently_testing) {
        return;
    }

    const regression_test_config_t* config = Regression_GetTest(regression_system.current_test);
    if (!config) {
        regression_system.currently_testing = qfalse;
        return;
    }

    regression_system.current_frame_count++;

    // Skip warmup frames
    if (regression_system.current_frame_count <= config->warmup_frames) {
        return;
    }

    // Record frame time
    if (regression_system.frame_count < regression_system.frame_buffer_size) {
        regression_system.frame_times[regression_system.frame_count++] = frame_time_ms;
    }

    // Check if we've collected enough measurement frames
    int measurement_frames = regression_system.current_frame_count - config->warmup_frames;
    if (measurement_frames >= config->measurement_frames) {
        // Test complete, analyze results
        Regression_FinishCurrentTest();
    }
}

static void Regression_FinishCurrentTest(void) {
    if (!regression_system.currently_testing) {
        return;
    }

    const regression_test_config_t* config = Regression_GetTest(regression_system.current_test);
    if (!config) {
        regression_system.currently_testing = qfalse;
        return;
    }

    // Calculate statistics
    float sum_fps = 0.0f;
    float min_fps = 999.0f;
    float max_fps = 0.0f;
    float sum_frame_time = 0.0f;

    for (uint32_t i = 0; i < regression_system.frame_count; i++) {
        float frame_time = regression_system.frame_times[i];
        float fps = 1000.0f / frame_time;

        sum_fps += fps;
        sum_frame_time += frame_time;

        if (fps < min_fps) min_fps = fps;
        if (fps > max_fps) max_fps = fps;
    }

    performance_measurement_t current = {
        .avg_fps = sum_fps / regression_system.frame_count,
        .min_fps = min_fps,
        .max_fps = max_fps,
        .frame_time_avg = sum_frame_time / regression_system.frame_count,
        .measurement_time = Sys_Milliseconds()
    };

    // Calculate standard deviation for frame time
    float variance = 0.0f;
    for (uint32_t i = 0; i < regression_system.frame_count; i++) {
        float diff = regression_system.frame_times[i] - current.frame_time_avg;
        variance += diff * diff;
    }
    current.frame_time_stddev = sqrtf(variance / regression_system.frame_count);

    // Get baseline measurement
    performance_measurement_t baseline = {0};
    Regression_GetBaseline(regression_system.current_test, &baseline);

    // Create result
    regression_test_result_t result;
    memset(&result, 0, sizeof(result));
    Q_strncpyz(result.test_name, regression_system.current_test, sizeof(result.test_name));
    result.current = current;

    if (baseline.avg_fps > 0) {
        result.baseline = baseline;
        result.fps_change_percentage = ((current.avg_fps - baseline.avg_fps) / baseline.avg_fps) * 100.0f;
        result.frame_time_change_percentage = ((current.frame_time_avg - baseline.frame_time_avg) / baseline.frame_time_avg) * 100.0f;

        // Determine if this is a regression or improvement
        if (fabsf(result.fps_change_percentage) >= config->regression_threshold) {
            if (result.fps_change_percentage < 0) {
                result.is_regression = qtrue;
                Q_snprintf(result.status_message, sizeof(result.status_message),
                          "REGRESSION: FPS decreased by %.1f%% (%.1f -> %.1f)",
                          -result.fps_change_percentage, baseline.avg_fps, current.avg_fps);
            } else {
                result.is_improvement = qtrue;
                Q_snprintf(result.status_message, sizeof(result.status_message),
                          "IMPROVEMENT: FPS increased by %.1f%% (%.1f -> %.1f)",
                          result.fps_change_percentage, baseline.avg_fps, current.avg_fps);
            }
        } else {
            Q_snprintf(result.status_message, sizeof(result.status_message),
                      "STABLE: FPS change within threshold (%.1f%%)",
                      result.fps_change_percentage);
        }

        // Check if test passed (above minimum FPS threshold)
        result.test_passed = (current.avg_fps >= config->fps_threshold);
    } else {
        // No baseline available
        Q_snprintf(result.status_message, sizeof(result.status_message),
                  "BASELINE: First run - FPS %.1f, Frame time %.2f ms",
                  current.avg_fps, current.frame_time_avg);
        result.test_passed = (current.avg_fps >= config->fps_threshold);

        // Auto-update baseline if enabled
        if (regression_system.auto_update_baseline) {
            Regression_UpdateBaseline(regression_system.current_test);
        }
    }

    // Store result
    if (regression_system.result_count < regression_system.max_results) {
        memcpy(&regression_system.results[regression_system.result_count++],
               &result, sizeof(regression_test_result_t));
    }

    // Print result
    Com_Printf("Regression test completed: %s\n", result.test_name);
    Com_Printf("  FPS: %.1f (min: %.1f, max: %.1f)\n",
               current.avg_fps, current.min_fps, current.max_fps);
    Com_Printf("  Frame Time: %.2f ± %.2f ms\n",
               current.frame_time_avg, current.frame_time_stddev);
    Com_Printf("  Status: %s\n", result.status_message);
    Com_Printf("  Result: %s\n", result.test_passed ? "PASSED" : "FAILED");

    // Reset test state
    regression_system.currently_testing = qfalse;
    memset(regression_system.current_test, 0, sizeof(regression_system.current_test));
}

/*
=============================================================================
Result Management
=============================================================================
*/

uint32_t Regression_GetResults(regression_test_result_t** results) {
    if (results) {
        *results = regression_system.results;
    }
    return regression_system.result_count;
}

const regression_test_result_t* Regression_GetResult(const char* test_name) {
    for (uint32_t i = 0; i < regression_system.result_count; i++) {
        if (Q_stricmp(regression_system.results[i].test_name, test_name) == 0) {
            return &regression_system.results[i];
        }
    }
    return NULL;
}

/*
=============================================================================
Baseline Management
=============================================================================
*/

qboolean Regression_UpdateBaseline(const char* test_name) {
    if (!regression_system.initialized) return qfalse;

    const regression_test_result_t* result = Regression_GetResult(test_name);
    if (!result) return qfalse;

    FILE* file = fopen(regression_system.baseline_file, "a");
    if (!file) return qfalse;

    fprintf(file, "TEST=%s\n", test_name);
    fprintf(file, "AVG_FPS=%.2f\n", result->current.avg_fps);
    fprintf(file, "MIN_FPS=%.2f\n", result->current.min_fps);
    fprintf(file, "MAX_FPS=%.2f\n", result->current.max_fps);
    fprintf(file, "FRAME_TIME_AVG=%.4f\n", result->current.frame_time_avg);
    fprintf(file, "FRAME_TIME_STDDEV=%.4f\n", result->current.frame_time_stddev);
    fprintf(file, "TIMESTAMP=%llu\n", (unsigned long long)result->current.measurement_time);
    fprintf(file, "\n");

    fclose(file);
    return qtrue;
}

qboolean Regression_LoadBaseline(void) {
    FILE* file = fopen(regression_system.baseline_file, "r");
    if (!file) return qfalse;

    // This is a simplified implementation - in practice, would parse the baseline file
    // and store baseline measurements for each test
    fclose(file);
    return qtrue;
}

qboolean Regression_SaveBaseline(void) {
    // Save all current baselines
    for (uint32_t i = 0; i < regression_system.test_count; i++) {
        Regression_UpdateBaseline(regression_system.tests[i].test_name);
    }
    return qtrue;
}

qboolean Regression_ResetBaseline(const char* test_name) {
    // Implementation would remove baseline data for specific test
    Com_Printf("Baseline reset not yet implemented for test: %s\n", test_name);
    return qfalse;
}

static qboolean Regression_GetBaseline(const char* test_name, performance_measurement_t* baseline) {
    // Simplified - in real implementation would load from baseline file
    memset(baseline, 0, sizeof(performance_measurement_t));
    return qfalse; // No baseline available
}

/*
=============================================================================
Statistical Analysis
=============================================================================
*/

qboolean Regression_AnalyzeResult(regression_test_result_t* result) {
    if (!result) return qfalse;

    // Basic statistical analysis
    float z_score = 0.0f;

    if (result->baseline.frame_time_stddev > 0) {
        z_score = fabsf(result->current.frame_time_avg - result->baseline.frame_time_avg) /
                 result->baseline.frame_time_stddev;
    }

    // Determine statistical significance
    const float z_threshold = 1.96f; // 95% confidence for 2-tailed test

    if (z_score > z_threshold) {
        // Statistically significant change
        if (result->fps_change_percentage < -regression_system.regression_threshold) {
            result->is_regression = qtrue;
        } else if (result->fps_change_percentage > regression_system.regression_threshold) {
            result->is_improvement = qtrue;
        }
    }

    return qtrue;
}

float Regression_CalculateConfidenceInterval(const performance_measurement_t* measurements,
                                           int count, float confidence_level) {
    if (!measurements || count < 2) return 0.0f;

    // Calculate mean
    float sum = 0.0f;
    for (int i = 0; i < count; i++) {
        sum += measurements[i].avg_fps;
    }
    float mean = sum / count;

    // Calculate standard deviation
    float variance = 0.0f;
    for (int i = 0; i < count; i++) {
        float diff = measurements[i].avg_fps - mean;
        variance += diff * diff;
    }
    variance /= (count - 1);
    float stddev = sqrtf(variance);

    // Calculate confidence interval (t-distribution approximation)
    float t_value = 2.0f; // Approximation for 95% confidence with large sample
    return t_value * (stddev / sqrtf(count));
}

/*
=============================================================================
Utility Functions
=============================================================================
*/

qboolean Regression_ValidateConfiguration(const regression_test_config_t* config) {
    if (!config) return qfalse;

    if (config->measurement_frames <= 0 || config->warmup_frames < 0) {
        return qfalse;
    }

    if (config->fps_threshold < 0 || config->regression_threshold <= 0) {
        return qfalse;
    }

    return qtrue;
}

const char* Regression_GetTestStatus(const regression_test_result_t* result) {
    if (!result) return "INVALID";

    if (!result->test_passed) return "FAILED";
    if (result->is_regression) return "REGRESSION";
    if (result->is_improvement) return "IMPROVEMENT";
    return "PASSED";
}

void Regression_PrintTestSummary(void) {
    Com_Printf("=== Performance Regression Test Summary ===\n");
    Com_Printf("Tests configured: %u\n", regression_system.test_count);
    Com_Printf("Results available: %u\n", regression_system.result_count);

    if (regression_system.result_count > 0) {
        int regressions = 0;
        int improvements = 0;
        int passed = 0;
        int failed = 0;

        for (uint32_t i = 0; i < regression_system.result_count; i++) {
            const regression_test_result_t* result = &regression_system.results[i];
            if (result->is_regression) regressions++;
            else if (result->is_improvement) improvements++;
            else if (result->test_passed) passed++;
            else failed++;
        }

        Com_Printf("Regressions: %d\n", regressions);
        Com_Printf("Improvements: %d\n", improvements);
        Com_Printf("Passed: %d\n", passed);
        Com_Printf("Failed: %d\n", failed);
    }

    Com_Printf("===========================================\n");
}

void Regression_PrintDetailedResults(void) {
    Com_Printf("=== Detailed Regression Test Results ===\n");

    for (uint32_t i = 0; i < regression_system.result_count; i++) {
        const regression_test_result_t* result = &regression_system.results[i];

        Com_Printf("Test: %s\n", result->test_name);
        Com_Printf("  Status: %s\n", Regression_GetTestStatus(result));
        Com_Printf("  Current FPS: %.1f (%.1f - %.1f)\n",
                  result->current.avg_fps, result->current.min_fps, result->current.max_fps);
        Com_Printf("  Frame Time: %.2f ± %.2f ms\n",
                  result->current.frame_time_avg, result->current.frame_time_stddev);

        if (result->baseline.avg_fps > 0) {
            Com_Printf("  Baseline FPS: %.1f\n", result->baseline.avg_fps);
            Com_Printf("  FPS Change: %+.1f%%\n", result->fps_change_percentage);
        }

        Com_Printf("  Message: %s\n\n", result->status_message);
    }

    Com_Printf("=========================================\n");
}

/*
=============================================================================
Predefined Test Configurations
=============================================================================
*/

qboolean Regression_AddStandardRenderingTest(const char* map_name, int quality_preset) {
    regression_test_config_t config;

    Q_snprintf(config.test_name, sizeof(config.test_name), "render_%s_q%d",
              map_name ? map_name : "default", quality_preset);

    Q_snprintf(config.description, sizeof(config.description),
              "Rendering performance test on %s at quality level %d",
              map_name ? map_name : "default map", quality_preset);

    config.warmup_frames = 100;      // Skip first 100 frames for warmup
    config.measurement_frames = 500; // Measure 500 frames for statistics
    config.fps_threshold = 30.0f;    // Minimum 30 FPS
    config.regression_threshold = 5.0f; // 5% change indicates regression
    config.enable_profiling = qtrue;

    return Regression_AddTest(&config);
}

qboolean Regression_AddMemoryStressTest(size_t allocation_size, int allocation_count) {
    regression_test_config_t config;

    Q_snprintf(config.test_name, sizeof(config.test_name), "memory_%zukb_x%d",
              (unsigned int)(allocation_size / 1024), allocation_count);

    Q_snprintf(config.description, sizeof(config.description),
              "Memory allocation stress test (%zu KB x %d)",
              allocation_size / 1024, allocation_count);

    config.warmup_frames = 50;
    config.measurement_frames = 200;
    config.fps_threshold = 20.0f;    // Lower threshold for memory tests
    config.regression_threshold = 10.0f; // More lenient for memory tests
    config.enable_profiling = qtrue;

    return Regression_AddTest(&config);
}

qboolean Regression_AddIOBenchmarkTest(const char* test_file, size_t file_size) {
    regression_test_config_t config;

    Q_snprintf(config.test_name, sizeof(config.test_name), "io_%s",
              test_file ? test_file : "benchmark");

    Q_snprintf(config.description, sizeof(config.description),
              "I/O performance test with %zu MB file",
              file_size / (1024 * 1024));

    config.warmup_frames = 10;
    config.measurement_frames = 100;
    config.fps_threshold = 15.0f;    // Lower threshold for I/O tests
    config.regression_threshold = 15.0f; // More lenient for I/O tests
    config.enable_profiling = qtrue;

    return Regression_AddTest(&config);
}

/*
=============================================================================
CI/CD Integration
=============================================================================
*/

qboolean Regression_CheckCIThresholds(void) {
    if (regression_system.result_count == 0) return qtrue;

    qboolean all_passed = qtrue;

    for (uint32_t i = 0; i < regression_system.result_count; i++) {
        const regression_test_result_t* result = &regression_system.results[i];

        // Check for regressions
        if (result->is_regression) {
            all_passed = qfalse;
            Com_Printf("CI FAILURE: Regression detected in test '%s'\n", result->test_name);
        }

        // Check FPS threshold
        if (result->current.avg_fps < Regression_GetTest(result->test_name)->fps_threshold) {
            all_passed = qfalse;
            Com_Printf("CI FAILURE: FPS below threshold in test '%s' (%.1f < %.1f)\n",
                      result->test_name, result->current.avg_fps,
                      Regression_GetTest(result->test_name)->fps_threshold);
        }
    }

    return all_passed;
}

qboolean Regression_GenerateCIReport(const char* output_file) {
    FILE* file = fopen(output_file, "w");
    if (!file) return qfalse;

    fprintf(file, "{\n");
    fprintf(file, "  \"timestamp\": %llu,\n", (unsigned long long)Sys_Milliseconds());
    fprintf(file, "  \"test_count\": %u,\n", regression_system.test_count);
    fprintf(file, "  \"result_count\": %u,\n", regression_system.result_count);
    fprintf(file, "  \"has_regressions\": %s,\n", Regression_HasRegressions() ? "true" : "false");
    fprintf(file, "  \"regression_count\": %d,\n", Regression_GetRegressionCount());
    fprintf(file, "  \"all_passed\": %s,\n", Regression_CheckCIThresholds() ? "true" : "false");
    fprintf(file, "  \"results\": [\n");

    for (uint32_t i = 0; i < regression_system.result_count; i++) {
        const regression_test_result_t* result = &regression_system.results[i];

        fprintf(file, "    {\n");
        fprintf(file, "      \"test_name\": \"%s\",\n", result->test_name);
        fprintf(file, "      \"status\": \"%s\",\n", Regression_GetTestStatus(result));
        fprintf(file, "      \"passed\": %s,\n", result->test_passed ? "true" : "false");
        fprintf(file, "      \"is_regression\": %s,\n", result->is_regression ? "true" : "false");
        fprintf(file, "      \"is_improvement\": %s,\n", result->is_improvement ? "true" : "false");
        fprintf(file, "      \"fps_current\": %.2f,\n", result->current.avg_fps);
        fprintf(file, "      \"fps_change_percent\": %.2f,\n", result->fps_change_percentage);
        fprintf(file, "      \"message\": \"%s\"\n", result->status_message);
        fprintf(file, "    }%s\n", (i < regression_system.result_count - 1) ? "," : "");
    }

    fprintf(file, "  ]\n");
    fprintf(file, "}\n");

    fclose(file);
    return qtrue;
}

qboolean Regression_HasRegressions(void) {
    for (uint32_t i = 0; i < regression_system.result_count; i++) {
        if (regression_system.results[i].is_regression) {
            return qtrue;
        }
    }
    return qfalse;
}

int Regression_GetRegressionCount(void) {
    int count = 0;
    for (uint32_t i = 0; i < regression_system.result_count; i++) {
        if (regression_system.results[i].is_regression) {
            count++;
        }
    }
    return count;
}

/*
=============================================================================
Real-time Monitoring
=============================================================================
*/

void Regression_UpdateRealTimeStats(void) {
    // Update real-time performance statistics
    // This would integrate with the rendering loop to track current performance
}

void Regression_GetRealTimeStats(float* avg_fps, float* min_fps, float* max_fps) {
    if (avg_fps) *avg_fps = 0.0f;
    if (min_fps) *min_fps = 0.0f;
    if (max_fps) *max_fps = 0.0f;

    // Implementation would calculate real-time stats from recent frame times
}

/*
=============================================================================
File I/O Functions (Simplified)
=============================================================================
*/

qboolean Regression_SaveResults(const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) return qfalse;

    fprintf(file, "# Performance Regression Test Results\n");
    fprintf(file, "# Generated: %llu\n", (unsigned long long)Sys_Milliseconds());
    fprintf(file, "\n");

    for (uint32_t i = 0; i < regression_system.result_count; i++) {
        const regression_test_result_t* result = &regression_system.results[i];

        fprintf(file, "[RESULT_%u]\n", i);
        fprintf(file, "test_name=%s\n", result->test_name);
        fprintf(file, "test_passed=%d\n", result->test_passed ? 1 : 0);
        fprintf(file, "is_regression=%d\n", result->is_regression ? 1 : 0);
        fprintf(file, "is_improvement=%d\n", result->is_improvement ? 1 : 0);
        fprintf(file, "fps_current=%.2f\n", result->current.avg_fps);
        fprintf(file, "fps_change_percent=%.2f\n", result->fps_change_percentage);
        fprintf(file, "status_message=%s\n", result->status_message);
        fprintf(file, "\n");
    }

    fclose(file);
    return qtrue;
}

qboolean Regression_LoadResults(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) return qfalse;

    // Simplified loading - would parse the results file in full implementation
    fclose(file);
    return qtrue;
}
