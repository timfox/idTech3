/*
=============================================================================
Performance Test Framework Implementation

Automated performance validation and regression testing for CI/CD pipelines.
=============================================================================
*/

#include "perf_test.h"
#include "qcommon.h"
#include <string.h>
#include <stdlib.h>

// Global performance test system instance
perf_test_system_t perf_test_system = {0};

// Sampling system for real-time performance data
static perf_sample_t* perf_samples = NULL;
static uint32_t max_samples = 0;
static uint32_t num_samples = 0;
static qboolean sampling_active = qfalse;
static uint64_t sampling_start_time = 0;

// Benchmark timing variables
static uint64_t frame_start_time = 0;
static uint64_t render_start_time = 0;
static uint64_t load_start_time = 0;

/*
=============================================================================
Internal Helper Functions
=============================================================================
*/

// Calculate percentile from samples
static double CalculatePercentile(const perf_sample_t* samples, uint32_t count, double percentile) {
    if (count == 0) return 0.0;

    // Simple implementation - sort and find percentile
    // In production, would use more efficient algorithm
    double* values = (double*)malloc(count * sizeof(double));
    if (!values) return 0.0;

    for (uint32_t i = 0; i < count; i++) {
        values[i] = samples[i].value;
    }

    // Simple sort (bubble sort for small arrays)
    for (uint32_t i = 0; i < count - 1; i++) {
        for (uint32_t j = 0; j < count - i - 1; j++) {
            if (values[j] > values[j + 1]) {
                double temp = values[j];
                values[j] = values[j + 1];
                values[j + 1] = temp;
            }
        }
    }

    uint32_t index = (uint32_t)((count - 1) * percentile / 100.0);
    double result = values[index];

    free(values);
    return result;
}

// Analyze test results against baseline
static perf_test_result_t AnalyzeResults(const perf_sample_t* samples, uint32_t sample_count,
                                       const perf_baseline_t* baseline, uint64_t test_duration_ms) {
    perf_test_result_t result = {0};
    result.result = PERF_RESULT_SUCCESS;
    result.start_time = sampling_start_time;
    result.end_time = sampling_start_time + test_duration_ms;
    result.sample_count = sample_count;

    // Calculate FPS metrics
    uint32_t fps_samples = 0;
    double fps_sum = 0.0;
    double fps_min = 999.0;
    double fps_max = 0.0;

    // Calculate frame time metrics
    uint32_t frame_time_samples = 0;
    double frame_time_sum = 0.0;
    double frame_time_max = 0.0;

    // Calculate CPU/GPU/Memory metrics
    uint32_t cpu_samples = 0, gpu_samples = 0, mem_samples = 0;
    double cpu_sum = 0.0, gpu_sum = 0.0, mem_sum = 0.0;
    double cpu_peak = 0.0, gpu_peak = 0.0, mem_peak = 0.0;

    // Process all samples
    for (uint32_t i = 0; i < sample_count; i++) {
        const perf_sample_t* sample = &samples[i];

        switch (sample->type) {
            case PERF_METRIC_FPS:
                fps_sum += sample->value;
                fps_samples++;
                if (sample->value < fps_min) fps_min = sample->value;
                if (sample->value > fps_max) fps_max = sample->value;
                break;

            case PERF_METRIC_FRAME_TIME:
                frame_time_sum += sample->value;
                frame_time_samples++;
                if (sample->value > frame_time_max) frame_time_max = sample->value;
                break;

            case PERF_METRIC_CPU_USAGE:
                cpu_sum += sample->value;
                cpu_samples++;
                if (sample->value > cpu_peak) cpu_peak = sample->value;
                break;

            case PERF_METRIC_GPU_USAGE:
                gpu_sum += sample->value;
                gpu_samples++;
                if (sample->value > gpu_peak) gpu_peak = sample->value;
                break;

            case PERF_METRIC_MEMORY_USAGE:
                mem_sum += sample->value;
                mem_samples++;
                if (sample->value > mem_peak) mem_peak = sample->value;
                break;

            default:
                break;
        }
    }

    // Calculate averages
    if (fps_samples > 0) {
        result.avg_fps = fps_sum / fps_samples;
        result.min_fps = fps_min;
        result.max_fps = fps_max;
    }

    if (frame_time_samples > 0) {
        result.avg_frame_time = frame_time_sum / frame_time_samples;
        result.max_frame_time = frame_time_max;
    }

    if (cpu_samples > 0) {
        result.avg_cpu_usage = cpu_sum / cpu_samples;
        result.peak_cpu_usage = cpu_peak;
    }

    if (gpu_samples > 0) {
        result.avg_gpu_usage = gpu_sum / gpu_samples;
        result.peak_gpu_usage = gpu_peak;
    }

    if (mem_samples > 0) {
        result.avg_memory_usage = mem_sum / mem_samples;
        result.peak_memory_usage = mem_peak;
    }

    // Check for regressions if baseline provided
    if (baseline) {
        result.regression_detected = qfalse;
        double total_regression_score = 0.0;
        int regression_factors = 0;

        // Check FPS regression
        if (baseline->baseline_fps_avg > 0 && fps_samples > 0) {
            double fps_change = ((baseline->baseline_fps_avg - result.avg_fps) / baseline->baseline_fps_avg) * 100.0;
            if (fps_change > baseline->regression_threshold_percent) {
                result.regression_detected = qtrue;
                total_regression_score += fps_change;
                regression_factors++;
                Com_sprintf(result.regression_reason, sizeof(result.regression_reason),
                           "FPS regression: %.1f%% decrease (%.1f -> %.1f)",
                           fps_change, baseline->baseline_fps_avg, result.avg_fps);
            }
        }

        // Check frame time regression
        if (baseline->baseline_frame_time_avg > 0 && frame_time_samples > 0) {
            double frame_time_change = ((result.avg_frame_time - baseline->baseline_frame_time_avg) /
                                      baseline->baseline_frame_time_avg) * 100.0;
            if (frame_time_change > baseline->regression_threshold_percent) {
                result.regression_detected = qtrue;
                total_regression_score += frame_time_change;
                regression_factors++;
                Com_sprintf(result.regression_reason, sizeof(result.regression_reason),
                           "Frame time regression: %.1f%% increase (%.2f -> %.2f ms)",
                           frame_time_change, baseline->baseline_frame_time_avg, result.avg_frame_time);
            }
        }

        if (result.regression_detected) {
            result.regression_percentage = total_regression_score / regression_factors;
            result.result = PERF_RESULT_REGRESSION;
        } else {
            // Check for improvements
            double improvement_score = 0.0;
            int improvement_factors = 0;

            if (baseline->baseline_fps_avg > 0 && fps_samples > 0) {
                double fps_improvement = ((result.avg_fps - baseline->baseline_fps_avg) /
                                        baseline->baseline_fps_avg) * 100.0;
                if (fps_improvement > baseline->regression_threshold_percent) {
                    improvement_score += fps_improvement;
                    improvement_factors++;
                }
            }

            if (improvement_factors > 0) {
                result.result = PERF_RESULT_IMPROVEMENT;
            }
        }
    }

    // Store sample data
    if (sample_count > 0 && sample_count <= result.max_samples) {
        result.samples = (perf_sample_t*)malloc(sample_count * sizeof(perf_sample_t));
        if (result.samples) {
            memcpy(result.samples, samples, sample_count * sizeof(perf_sample_t));
            result.num_samples = sample_count;
        }
    }

    return result;
}

/*
=============================================================================
Performance Test API
=============================================================================
*/

qboolean PerfTest_Init(void) {
    if (perf_test_system.initialized) {
        return qtrue;
    }

    memset(&perf_test_system, 0, sizeof(perf_test_system_t));

    // Initialize sampling system
    max_samples = 10000; // Store up to 10k samples
    perf_samples = (perf_sample_t*)malloc(max_samples * sizeof(perf_sample_t));
    if (!perf_samples) {
        Com_Printf("Failed to allocate memory for performance samples\n");
        return qfalse;
    }
    memset(perf_samples, 0, max_samples * sizeof(perf_sample_t));

    // Allocate results storage
    perf_test_system.max_results = 100;
    perf_test_system.current_results = (perf_test_result_t*)malloc(
        perf_test_system.max_results * sizeof(perf_test_result_t));
    if (!perf_test_system.current_results) {
        free(perf_samples);
        Com_Printf("Failed to allocate memory for test results\n");
        return qfalse;
    }
    memset(perf_test_system.current_results, 0,
           perf_test_system.max_results * sizeof(perf_test_result_t));

    // Allocate baseline storage
    perf_test_system.max_baselines = 50;
    perf_test_system.baselines = (perf_baseline_t*)malloc(
        perf_test_system.max_baselines * sizeof(perf_baseline_t));
    if (!perf_test_system.baselines) {
        free(perf_test_system.current_results);
        free(perf_samples);
        Com_Printf("Failed to allocate memory for baselines\n");
        return qfalse;
    }
    memset(perf_test_system.baselines, 0,
           perf_test_system.max_baselines * sizeof(perf_baseline_t));

    // Set default CI config
    Com_sprintf(perf_test_system.ci_config.output_directory,
               sizeof(perf_test_system.ci_config.output_directory), "perf_results");
    Com_sprintf(perf_test_system.ci_config.report_format,
               sizeof(perf_test_system.ci_config.report_format), "JSON");
    perf_test_system.ci_config.generate_html_report = qtrue;

    perf_test_system.initialized = qtrue;
    Com_Printf("Performance test system initialized\n");
    return qtrue;
}

void PerfTest_Shutdown(void) {
    if (!perf_test_system.initialized) {
        return;
    }

    // Clean up sampling
    if (perf_samples) {
        free(perf_samples);
        perf_samples = NULL;
    }

    // Clean up results
    if (perf_test_system.current_results) {
        for (uint32_t i = 0; i < perf_test_system.max_results; i++) {
            if (perf_test_system.current_results[i].samples) {
                free(perf_test_system.current_results[i].samples);
            }
        }
        free(perf_test_system.current_results);
        perf_test_system.current_results = NULL;
    }

    // Clean up baselines
    if (perf_test_system.baselines) {
        free(perf_test_system.baselines);
        perf_test_system.baselines = NULL;
    }

    perf_test_system.initialized = qfalse;
    Com_Printf("Performance test system shutdown\n");
}

/*
=============================================================================
Test Suite Management
=============================================================================
*/

perf_test_suite_t* PerfTest_CreateSuite(const char* name, const char* description) {
    if (!perf_test_system.initialized) {
        return NULL;
    }

    perf_test_suite_t* suite = (perf_test_suite_t*)malloc(sizeof(perf_test_suite_t));
    if (!suite) {
        return NULL;
    }

    memset(suite, 0, sizeof(perf_test_suite_t));
    Q_strncpyz(suite->suite_name, name, sizeof(suite->suite_name));
    Q_strncpyz(suite->description, description, sizeof(suite->description));

    suite->max_tests = 20;
    suite->tests = (perf_test_config_t*)malloc(suite->max_tests * sizeof(perf_test_config_t));
    if (!suite->tests) {
        free(suite);
        return NULL;
    }
    memset(suite->tests, 0, suite->max_tests * sizeof(perf_test_config_t));

    // Default suite configuration
    suite->run_in_parallel = qfalse;
    suite->max_parallel_tests = 4;
    suite->stop_on_failure = qfalse;
    suite->timeout_seconds = 300; // 5 minutes

    return suite;
}

qboolean PerfTest_AddTestToSuite(perf_test_suite_t* suite, const perf_test_config_t* config) {
    if (!suite || !config || suite->num_tests >= suite->max_tests) {
        return qfalse;
    }

    memcpy(&suite->tests[suite->num_tests], config, sizeof(perf_test_config_t));
    suite->num_tests++;

    return qtrue;
}

qboolean PerfTest_RunSuite(perf_test_suite_t* suite) {
    if (!suite || suite->num_tests == 0) {
        return qfalse;
    }

    Com_Printf("Running performance test suite: %s\n", suite->suite_name);
    Com_Printf("Description: %s\n", suite->description);
    Com_Printf("Tests: %u\n", suite->num_tests);

    uint64_t suite_start_time = PerfTest_GetTimestamp();
    uint32_t passed = 0, failed = 0, regressions = 0;

    for (uint32_t i = 0; i < suite->num_tests; i++) {
        const perf_test_config_t* config = &suite->tests[i];

        Com_Printf("Running test %u/%u: %s\n", i + 1, suite->num_tests, config->name);

        perf_test_result_t result;
        if (PerfTest_RunTest(config, &result)) {
            // Store result
            if (perf_test_system.max_results > 0) {
                memcpy(&perf_test_system.current_results[perf_test_system.max_results - 1],
                       &result, sizeof(perf_test_result_t));
            }

            // Update statistics
            perf_test_system.total_tests_run++;

            switch (result.result) {
                case PERF_RESULT_SUCCESS:
                case PERF_RESULT_IMPROVEMENT:
                    passed++;
                    break;
                case PERF_RESULT_FAILURE:
                    failed++;
                    break;
                case PERF_RESULT_REGRESSION:
                    regressions++;
                    perf_test_system.total_regressions_detected++;
                    break;
                default:
                    failed++;
                    break;
            }

            Com_Printf("  Result: %s\n", PerfTest_GetResultString(result.result));
            Com_Printf("  FPS: %.1f avg, %.1f min, %.1f max\n",
                      result.avg_fps, result.min_fps, result.max_fps);
            Com_Printf("  Frame Time: %.2f ms avg, %.2f ms max\n",
                      result.avg_frame_time, result.max_frame_time);

            if (result.regression_detected) {
                Com_Printf("  REGRESSION: %s\n", result.regression_reason);
            }
        } else {
            Com_Printf("  FAILED: Test execution error\n");
            failed++;
        }

        // Check for suite timeout
        uint64_t current_time = PerfTest_GetTimestamp();
        if ((current_time - suite_start_time) / 1000000 > suite->timeout_seconds) {
            Com_Printf("Suite timeout reached, stopping execution\n");
            break;
        }

        // Check for stop on failure
        if (suite->stop_on_failure && failed > 0) {
            Com_Printf("Stopping suite due to test failure\n");
            break;
        }
    }

    uint64_t suite_end_time = PerfTest_GetTimestamp();
    uint64_t suite_duration = (suite_end_time - suite_start_time) / 1000000;

    Com_Printf("\nSuite Summary:\n");
    Com_Printf("Total Tests: %u\n", suite->num_tests);
    Com_Printf("Passed: %u\n", passed);
    Com_Printf("Failed: %u\n", failed);
    Com_Printf("Regressions: %u\n", regressions);
    Com_Printf("Duration: %llu seconds\n", (unsigned long long)suite_duration);

    perf_test_system.total_test_time_ms += suite_duration * 1000;

    return (failed == 0 && regressions == 0);
}

perf_test_result_t* PerfTest_GetSuiteResults(perf_test_suite_t* suite, uint32_t* count) {
    Q_UNUSED(suite); // For now, return global results
    if (count) *count = perf_test_system.max_results;
    return perf_test_system.current_results;
}

/*
=============================================================================
Individual Test Execution
=============================================================================
*/

qboolean PerfTest_RunTest(const perf_test_config_t* config, perf_test_result_t* result) {
    if (!config || !result || perf_test_system.test_running) {
        return qfalse;
    }

    Com_Printf("Starting performance test: %s\n", config->name);

    // Initialize result
    memset(result, 0, sizeof(perf_test_result_t));
    Q_strncpyz(result->test_name, config->name, sizeof(result->test_name));

    // Set test metadata
    result->start_time = PerfTest_GetTimestamp();
    Com_sprintf(result->platform, sizeof(result->platform), "Linux"); // Would detect platform
    Com_sprintf(result->hardware_config, sizeof(result->hardware_config), "Unknown"); // Would detect hardware

    // Load test configuration
    if (config->map_name[0]) {
        Com_Printf("Loading map: %s\n", config->map_name);
        // Would load the specified map
    }

    // Start sampling
    PerfTest_StartSampling();
    num_samples = 0; // Reset sample counter

    // Warmup period
    if (config->warmup_seconds > 0) {
        Com_Printf("Warmup period: %d seconds\n", config->warmup_seconds);
        uint64_t warmup_end = result->start_time + (config->warmup_seconds * 1000000ULL);
        while (PerfTest_GetTimestamp() < warmup_end) {
            // Run game loop for warmup
            // In real implementation, would integrate with main game loop
            Thread_Sleep(16); // ~60 FPS
        }
    }

    // Test execution period
    uint64_t test_start = PerfTest_GetTimestamp();
    uint64_t test_end = test_start + (config->duration_seconds * 1000000ULL);

    Com_Printf("Running test for %d seconds\n", config->duration_seconds);

    while (PerfTest_GetTimestamp() < test_end && !perf_test_system.abort_requested) {
        // Run game loop and collect samples
        uint64_t frame_start = PerfTest_GetTimestamp();

        // Simulate frame processing
        PerfTest_BenchmarkBeginFrame();

        // Add performance samples (would be collected from actual game systems)
        PerfTest_AddSample(PERF_METRIC_FPS, "fps", 60.0 + (rand() % 20 - 10)); // Simulate FPS variation
        PerfTest_AddSample(PERF_METRIC_FRAME_TIME, "frame_time", 16.67 + (rand() % 5)); // Simulate frame time
        PerfTest_AddSample(PERF_METRIC_CPU_USAGE, "cpu", 45.0 + (rand() % 30)); // Simulate CPU usage
        PerfTest_AddSample(PERF_METRIC_MEMORY_USAGE, "memory", 512.0 + (rand() % 128)); // Simulate memory usage

        PerfTest_BenchmarkEndFrame();

        // Sleep to simulate frame timing
        Thread_Sleep(16); // ~60 FPS
    }

    // Stop sampling
    PerfTest_StopSampling();

    uint64_t actual_test_duration = PerfTest_GetTimestamp() - test_start;
    result->end_time = result->start_time + actual_test_duration;

    // Analyze results
    perf_baseline_t* baseline = PerfTest_GetBaseline(config->name);
    *result = AnalyzeResults(perf_samples, num_samples, baseline, actual_test_duration / 1000);

    // Copy test name back
    Q_strncpyz(result->test_name, config->name, sizeof(result->test_name));

    perf_test_system.test_running = qfalse;

    Com_Printf("Test completed: %s\n", PerfTest_GetResultString(result->result));
    return qtrue;
}

qboolean PerfTest_CancelTest(void) {
    perf_test_system.abort_requested = qtrue;
    return qtrue;
}

qboolean PerfTest_IsTestRunning(void) {
    return perf_test_system.test_running;
}

/*
=============================================================================
Baseline Management
=============================================================================
*/

qboolean PerfTest_LoadBaselines(const char* baseline_file) {
    // Implementation would load baseline data from JSON/XML file
    Q_UNUSED(baseline_file);
    return qtrue;
}

qboolean PerfTest_SaveBaselines(const char* baseline_file) {
    // Implementation would save baseline data to JSON/XML file
    Q_UNUSED(baseline_file);
    return qtrue;
}

qboolean PerfTest_SetBaseline(const char* test_name, const perf_test_result_t* result) {
    if (!test_name || !result) return qfalse;

    perf_baseline_t* baseline = PerfTest_GetBaseline(test_name);
    if (!baseline) {
        if (perf_test_system.num_baselines >= perf_test_system.max_baselines) {
            return qfalse;
        }
        baseline = &perf_test_system.baselines[perf_test_system.num_baselines++];
    }

    memset(baseline, 0, sizeof(perf_baseline_t));
    Q_strncpyz(baseline->test_name, test_name, sizeof(baseline->test_name));
    Com_sprintf(baseline->baseline_version, sizeof(baseline->baseline_version), "current");
    baseline->baseline_timestamp = PerfTest_GetTimestamp();

    // Set baseline metrics
    baseline->baseline_fps_avg = result->avg_fps;
    baseline->baseline_fps_min = result->min_fps;
    baseline->baseline_frame_time_avg = result->avg_frame_time;
    baseline->baseline_frame_time_max = result->max_frame_time;
    baseline->baseline_cpu_avg = result->avg_cpu_usage;
    baseline->baseline_memory_avg = result->avg_memory_usage;
    baseline->baseline_gpu_avg = result->avg_gpu_usage;

    // Set default tolerances
    baseline->fps_tolerance_percent = 5.0;
    baseline->frame_time_tolerance_percent = 10.0;
    baseline->cpu_tolerance_percent = 15.0;
    baseline->memory_tolerance_percent = 10.0;
    baseline->gpu_tolerance_percent = 15.0;
    baseline->regression_threshold_percent = 5.0;
    baseline->consecutive_failures_required = 2;

    return qtrue;
}

perf_baseline_t* PerfTest_GetBaseline(const char* test_name) {
    for (uint32_t i = 0; i < perf_test_system.num_baselines; i++) {
        if (strcmp(perf_test_system.baselines[i].test_name, test_name) == 0) {
            return &perf_test_system.baselines[i];
        }
    }
    return NULL;
}

qboolean PerfTest_CheckRegression(const perf_test_result_t* result, const perf_baseline_t* baseline) {
    if (!result || !baseline) return qfalse;

    // Simple regression check - in practice would be more sophisticated
    double fps_change = ((baseline->baseline_fps_avg - result->avg_fps) /
                        baseline->baseline_fps_avg) * 100.0;

    return (fps_change > baseline->regression_threshold_percent);
}

/*
=============================================================================
Result Analysis and Reporting
=============================================================================
*/

qboolean PerfTest_CompareResults(const perf_test_result_t* result1, const perf_test_result_t* result2,
                                double* improvement_percent, char* comparison_desc, size_t desc_size) {
    if (!result1 || !result2 || !improvement_percent || !comparison_desc) {
        return qfalse;
    }

    double fps_diff = result2->avg_fps - result1->avg_fps;
    *improvement_percent = (fps_diff / result1->avg_fps) * 100.0;

    Com_sprintf(comparison_desc, desc_size,
               "FPS: %.1f -> %.1f (%.1f%% %s)",
               result1->avg_fps, result2->avg_fps, fabs(*improvement_percent),
               (*improvement_percent > 0) ? "improvement" : "regression");

    return qtrue;
}

qboolean PerfTest_GenerateReport(const perf_test_result_t* results, uint32_t count,
                               const char* output_file, const char* format) {
    // Implementation would generate reports in various formats (JSON, XML, JUnit, etc.)
    Q_UNUSED(results);
    Q_UNUSED(count);
    Q_UNUSED(output_file);
    Q_UNUSED(format);
    return qtrue;
}

/*
=============================================================================
CI/CD Integration
=============================================================================
*/

void PerfTest_SetCIConfig(const perf_ci_config_t* config) {
    if (config) {
        memcpy(&perf_test_system.ci_config, config, sizeof(perf_ci_config_t));
    }
}

qboolean PerfTest_ExportForCI(const perf_test_result_t* results, uint32_t count,
                            const char* output_dir) {
    // Export results for CI consumption
    Q_UNUSED(results);
    Q_UNUSED(count);
    Q_UNUSED(output_dir);
    return qtrue;
}

qboolean PerfTest_UploadResults(const char* results_file) {
    // Upload results to external service
    Q_UNUSED(results_file);
    return qtrue;
}

/*
=============================================================================
Real-time Sampling
=============================================================================
*/

void PerfTest_StartSampling(void) {
    sampling_active = qtrue;
    sampling_start_time = PerfTest_GetTimestamp();
    num_samples = 0;
}

void PerfTest_StopSampling(void) {
    sampling_active = qfalse;
}

qboolean PerfTest_AddSample(perf_metric_type_t type, const char* name, double value) {
    if (!sampling_active || num_samples >= max_samples) {
        return qfalse;
    }

    perf_sample_t* sample = &perf_samples[num_samples++];
    sample->timestamp = PerfTest_GetTimestamp();
    sample->type = type;
    Q_strncpyz(sample->name, name, sizeof(sample->name));
    sample->value = value;
    sample->min_value = sample->max_value = sample->avg_value = value;

    return qtrue;
}

perf_sample_t* PerfTest_GetSamples(uint32_t* count) {
    if (count) *count = num_samples;
    return perf_samples;
}

/*
=============================================================================
Benchmark Functions
=============================================================================
*/

void PerfTest_BenchmarkBeginFrame(void) {
    frame_start_time = PerfTest_GetTimestamp();
}

void PerfTest_BenchmarkEndFrame(void) {
    if (frame_start_time > 0) {
        uint64_t frame_time_ns = PerfTest_GetTimestamp() - frame_start_time;
        double frame_time_ms = frame_time_ns / 1000000.0;
        PerfTest_AddSample(PERF_METRIC_FRAME_TIME, "frame_time", frame_time_ms);

        // Calculate FPS
        double fps = 1000.0 / frame_time_ms;
        PerfTest_AddSample(PERF_METRIC_FPS, "fps", fps);
    }
}

void PerfTest_BenchmarkLoadBegin(const char* asset_name) {
    Q_UNUSED(asset_name);
    load_start_time = PerfTest_GetTimestamp();
}

void PerfTest_BenchmarkLoadEnd(const char* asset_name) {
    if (load_start_time > 0) {
        uint64_t load_time_ns = PerfTest_GetTimestamp() - load_start_time;
        double load_time_ms = load_time_ns / 1000000.0;
        PerfTest_AddSample(PERF_METRIC_LOAD_TIME, asset_name, load_time_ms);
    }
}

void PerfTest_BenchmarkRenderBegin(void) {
    render_start_time = PerfTest_GetTimestamp();
}

void PerfTest_BenchmarkRenderEnd(void) {
    if (render_start_time > 0) {
        uint64_t render_time_ns = PerfTest_GetTimestamp() - render_start_time;
        double render_time_ms = render_time_ns / 1000000.0;
        PerfTest_AddSample(PERF_METRIC_RENDER_TIME, "render_time", render_time_ms);
    }
}

/*
=============================================================================
Utility Functions
=============================================================================
*/

uint64_t PerfTest_GetTimestamp(void) {
    return Sys_Milliseconds() * 1000ULL; // Convert to microseconds
}

const char* PerfTest_GetResultString(perf_test_result_t result) {
    switch (result) {
        case PERF_RESULT_SUCCESS: return "SUCCESS";
        case PERF_RESULT_FAILURE: return "FAILURE";
        case PERF_RESULT_REGRESSION: return "REGRESSION";
        case PERF_RESULT_IMPROVEMENT: return "IMPROVEMENT";
        case PERF_RESULT_INCONCLUSIVE: return "INCONCLUSIVE";
        case PERF_RESULT_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

const char* PerfTest_GetMetricString(perf_metric_type_t metric) {
    switch (metric) {
        case PERF_METRIC_FPS: return "FPS";
        case PERF_METRIC_FRAME_TIME: return "Frame Time";
        case PERF_METRIC_CPU_USAGE: return "CPU Usage";
        case PERF_METRIC_MEMORY_USAGE: return "Memory Usage";
        case PERF_METRIC_GPU_USAGE: return "GPU Usage";
        case PERF_METRIC_LOAD_TIME: return "Load Time";
        case PERF_METRIC_RENDER_TIME: return "Render Time";
        case PERF_METRIC_NETWORK_LATENCY: return "Network Latency";
        case PERF_METRIC_DISK_IO: return "Disk I/O";
        case PERF_METRIC_CUSTOM: return "Custom";
        default: return "Unknown";
    }
}

qboolean PerfTest_ValidateConfig(const perf_test_config_t* config) {
    if (!config) return qfalse;

    if (config->duration_seconds <= 0 || config->duration_seconds > 3600) return qfalse;
    if (config->warmup_seconds < 0 || config->warmup_seconds > 300) return qfalse;
    if (config->sample_interval_ms < 10 || config->sample_interval_ms > 10000) return qfalse;

    return qtrue;
}
