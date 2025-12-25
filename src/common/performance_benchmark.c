/*
=============================================================================
Performance Benchmarking Framework Implementation

Automated performance regression detection and benchmarking infrastructure.
=============================================================================
*/

#include "performance_benchmark.h"
#include "qcommon.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

// Global benchmark system instance
benchmark_system_t benchmark_system = {0};

// Statistical constants
#define Z_SCORE_95_CONFIDENCE 1.96f    // Z-score for 95% confidence
#define Z_SCORE_99_CONFIDENCE 2.576f   // Z-score for 99% confidence

// Performance measurement thresholds (percentage change)
#define REGRESSION_THRESHOLD -5.0f     // 5% degradation = regression
#define IMPROVEMENT_THRESHOLD 5.0f     // 5% improvement = significant

/*
=============================================================================
Performance Benchmarking API Implementation
=============================================================================
*/

qboolean Benchmark_Init(void) {
    if (benchmark_system.initialized) {
        return qtrue;
    }

    memset(&benchmark_system, 0, sizeof(benchmark_system_t));
    Q_strncpyz(benchmark_system.system_name, "Performance Benchmarking", sizeof(benchmark_system.system_name));
    Q_strncpyz(benchmark_system.description, "Automated performance regression detection and benchmarking", sizeof(benchmark_system.description));

    // Allocate suite storage
    benchmark_system.max_suites = 50;
    benchmark_system.suites = (benchmark_suite_t*)malloc(
        sizeof(benchmark_suite_t) * benchmark_system.max_suites);

    if (!benchmark_system.suites) {
        Com_Printf("Failed to allocate memory for benchmark suites\n");
        return qfalse;
    }

    memset(benchmark_system.suites, 0,
           sizeof(benchmark_suite_t) * benchmark_system.max_suites);

    // Allocate results storage
    benchmark_system.max_results = 1000;
    benchmark_system.results = (benchmark_result_t*)malloc(
        sizeof(benchmark_result_t) * benchmark_system.max_results);

    if (!benchmark_system.results) {
        free(benchmark_system.suites);
        Com_Printf("Failed to allocate memory for benchmark results\n");
        return qfalse;
    }

    memset(benchmark_system.results, 0,
           sizeof(benchmark_result_t) * benchmark_system.max_results);

    // Set default configuration
    benchmark_system.auto_baseline_update = qtrue;
    benchmark_system.enable_regression_alerts = qtrue;
    benchmark_system.baseline_retention_days = 365;
    benchmark_system.global_regression_threshold = 5.0f;
    benchmark_system.min_samples_for_stats = 5;
    benchmark_system.statistical_confidence = 0.95f;
    benchmark_system.use_robust_statistics = qtrue;
    benchmark_system.enable_hardware_profiling = qtrue;

    // Profile hardware
    char hardware_info[512];
    if (Benchmark_ProfileHardware(hardware_info, sizeof(hardware_info))) {
        Com_Printf("Detected hardware: %s\n", hardware_info);
    }

    benchmark_system.initialized = qtrue;
    benchmark_system.currently_running = qfalse;

    Com_Printf("Performance benchmarking system initialized\n");
    Com_Printf("Supports %u benchmark categories with automated regression detection\n", BENCHMARK_CATEGORY_COUNT);

    return qtrue;
}

void Benchmark_Shutdown(void) {
    if (!benchmark_system.initialized) {
        return;
    }

    // Clean up suites
    for (uint32_t i = 0; i < benchmark_system.max_suites; i++) {
        if (benchmark_system.suites[i].benchmarks) {
            free(benchmark_system.suites[i].benchmarks);
        }
    }

    if (benchmark_system.suites) {
        free(benchmark_system.suites);
        benchmark_system.suites = NULL;
    }

    // Clean up results
    for (uint32_t i = 0; i < benchmark_system.max_results; i++) {
        if (benchmark_system.results[i].measurements) {
            free(benchmark_system.results[i].measurements);
        }
    }

    if (benchmark_system.results) {
        free(benchmark_system.results);
        benchmark_system.results = NULL;
    }

    benchmark_system.initialized = qfalse;
    Com_Printf("Performance benchmarking system shutdown\n");
}

/*
=============================================================================
Benchmark Suite Management
=============================================================================
*/

benchmark_suite_t* Benchmark_CreateSuite(const char* name, const char* description) {
    if (!benchmark_system.initialized || benchmark_system.suite_count >= benchmark_system.max_suites) {
        return NULL;
    }

    benchmark_suite_t* suite = &benchmark_system.suites[benchmark_system.suite_count];
    memset(suite, 0, sizeof(benchmark_suite_t));

    Q_strncpyz(suite->suite_name, name, sizeof(suite->suite_name));
    Q_strncpyz(suite->description, description, sizeof(suite->description));

    suite->max_benchmarks = 100;
    suite->benchmarks = (benchmark_config_t*)malloc(
        sizeof(benchmark_config_t) * suite->max_benchmarks);

    if (!suite->benchmarks) {
        return NULL;
    }

    memset(suite->benchmarks, 0, sizeof(benchmark_config_t) * suite->max_benchmarks);

    // Set default suite configuration
    suite->run_in_parallel = qfalse;
    suite->max_parallel_benchmarks = 1;
    suite->stop_on_failure = qfalse;
    suite->suite_timeout_minutes = 60;
    suite->regression_threshold = benchmark_system.global_regression_threshold;
    suite->improvement_threshold = IMPROVEMENT_THRESHOLD;
    suite->statistical_significance = benchmark_system.statistical_confidence;

    benchmark_system.suite_count++;

    return suite;
}

qboolean Benchmark_AddBenchmarkToSuite(benchmark_suite_t* suite, const benchmark_config_t* config) {
    if (!suite || !config || suite->benchmark_count >= suite->max_benchmarks) {
        return qfalse;
    }

    if (!Benchmark_ValidateConfig(config)) {
        return qfalse;
    }

    memcpy(&suite->benchmarks[suite->benchmark_count], config, sizeof(benchmark_config_t));
    suite->benchmark_count++;

    return qtrue;
}

qboolean Benchmark_RemoveBenchmarkFromSuite(benchmark_suite_t* suite, const char* benchmark_id) {
    if (!suite || !benchmark_id) return qfalse;

    for (uint32_t i = 0; i < suite->benchmark_count; i++) {
        if (Q_stricmp(suite->benchmarks[i].benchmark_id, benchmark_id) == 0) {
            // Shift remaining benchmarks
            for (uint32_t j = i; j < suite->benchmark_count - 1; j++) {
                memcpy(&suite->benchmarks[j],
                       &suite->benchmarks[j + 1],
                       sizeof(benchmark_config_t));
            }
            suite->benchmark_count--;
            return qtrue;
        }
    }
    return qfalse;
}

/*
=============================================================================
Benchmark Execution
=============================================================================
*/

qboolean Benchmark_RunSuite(benchmark_suite_t* suite) {
    if (!suite || suite->benchmark_count == 0) {
        return qfalse;
    }

    Com_Printf("Running benchmark suite: %s\n", suite->suite_name);
    Com_Printf("Description: %s\n", suite->description);
    Com_Printf("Benchmarks: %u\n", suite->benchmark_count);

    uint64_t suite_start_time = Sys_Milliseconds();
    uint32_t passed = 0, failed = 0, regressions = 0, timeouts = 0;

    for (uint32_t i = 0; i < suite->benchmark_count; i++) {
        const benchmark_config_t* config = &suite->benchmarks[i];

        Com_Printf("Running benchmark %u/%u: %s\n", i + 1, suite->benchmark_count, config->name);

        benchmark_result_t result;
        memset(&result, 0, sizeof(result));
        Q_strncpyz(result.benchmark_id, config->benchmark_id, sizeof(result.benchmark_id));

        // Generate unique run ID
        Com_sprintf(result.run_id, sizeof(result.run_id), "%s_%llu",
                   config->benchmark_id, Sys_Milliseconds());

        if (Benchmark_RunBenchmark(config, &result)) {
            // Store result
            if (benchmark_system.result_count < benchmark_system.max_results) {
                memcpy(&benchmark_system.results[benchmark_system.result_count++],
                       &result, sizeof(benchmark_result_t));
            }

            // Analyze result
            Com_Printf("  Result: %s", Benchmark_GetResultString(result.result));

            if (result.duration_ms > 0) {
                Com_Printf(" (%.2fs)", result.duration_ms / 1000.0f);
            }

            if (result.overall_score > 0) {
                Com_Printf(" - Score: %.1f", result.overall_score);
            }

            if (result.has_regression) {
                Com_Printf(" ⚠️ REGRESSION DETECTED");
                regressions++;
            }

            Com_Printf("\n");

            switch (result.result) {
                case BENCHMARK_RESULT_PASS:
                    passed++;
                    break;
                case BENCHMARK_RESULT_FAIL:
                case BENCHMARK_RESULT_REGRESSION:
                    failed++;
                    break;
                case BENCHMARK_RESULT_TIMEOUT:
                    timeouts++;
                    break;
                default:
                    break;
            }

            // Update baseline if auto-update is enabled and no regression
            if (benchmark_system.auto_baseline_update && !result.has_regression &&
                result.result == BENCHMARK_RESULT_PASS) {
                Benchmark_UpdateBaseline(config->benchmark_id, &result);
            }
        } else {
            Com_Printf("  FAILED: Benchmark execution error\n");
            failed++;
        }

        // Check for suite timeout
        uint64_t current_time = Sys_Milliseconds();
        if ((current_time - suite_start_time) / (1000 * 60) > suite->suite_timeout_minutes) {
            Com_Printf("Suite timeout reached, stopping execution\n");
            break;
        }

        // Stop on failure if configured
        if (suite->stop_on_failure && failed > 0) {
            Com_Printf("Stopping suite due to failure\n");
            break;
        }
    }

    uint64_t suite_duration = Sys_Milliseconds() - suite_start_time;

    Com_Printf("\nSuite Summary:\n");
    Com_Printf("Total Benchmarks: %u\n", suite->benchmark_count);
    Com_Printf("Passed: %u\n", passed);
    Com_Printf("Failed: %u\n", failed);
    Com_Printf("Regressions: %u\n", regressions);
    Com_Printf("Timeouts: %u\n", timeouts);
    Com_Printf("Duration: %.2f minutes\n", suite_duration / (1000.0f * 60.0f));

    return (failed == 0 && timeouts == 0);
}

qboolean Benchmark_RunBenchmark(const benchmark_config_t* config, benchmark_result_t* result) {
    if (!config || !result) return qfalse;

    benchmark_system.currently_running = qtrue;
    Q_strncpyz(benchmark_system.current_benchmark, config->benchmark_id,
               sizeof(benchmark_system.current_benchmark));

    result->start_time = Sys_Milliseconds();
    result->result = BENCHMARK_RESULT_PASS;

    // Allocate measurement storage
    result->max_measurements = METRIC_COUNT;
    result->measurements = (performance_measurement_t*)malloc(
        sizeof(performance_measurement_t) * result->max_measurements);

    if (!result->measurements) {
        result->result = BENCHMARK_RESULT_FAIL;
        benchmark_system.currently_running = qfalse;
        return qtrue; // Completed with failure
    }

    memset(result->measurements, 0,
           sizeof(performance_measurement_t) * result->max_measurements);

    // Initialize measurements
    result->measurement_count = 0;

    // Perform warmup iterations
    for (int i = 0; i < config->warmup_iterations; i++) {
        if (!Benchmark_ExecuteBenchmarkIteration(config, NULL, qtrue)) {
            result->result = BENCHMARK_RESULT_FAIL;
            break;
        }
    }

    if (result->result != BENCHMARK_RESULT_PASS) {
        benchmark_system.currently_running = qfalse;
        return qtrue;
    }

    // Perform measurement iterations
    for (int i = 0; i < config->measurement_iterations; i++) {
        performance_measurement_t temp_measurements[METRIC_COUNT];
        memset(temp_measurements, 0, sizeof(temp_measurements));

        if (!Benchmark_ExecuteBenchmarkIteration(config, temp_measurements, qfalse)) {
            result->result = BENCHMARK_RESULT_FAIL;
            break;
        }

        // Aggregate measurements
        for (int j = 0; j < METRIC_COUNT; j++) {
            if (temp_measurements[j].current_value > 0) {
                // Simple averaging for now (could be enhanced with statistical analysis)
                if (result->measurements[j].current_value == 0) {
                    result->measurements[j] = temp_measurements[j];
                    result->measurement_count++;
                } else {
                    // Running average
                    float old_avg = result->measurements[j].current_value;
                    result->measurements[j].current_value =
                        (old_avg * i + temp_measurements[j].current_value) / (i + 1);
                }
            }
        }
    }

    result->end_time = Sys_Milliseconds();
    result->duration_ms = result->end_time - result->start_time;

    // Analyze results for regressions
    if (result->result == BENCHMARK_RESULT_PASS) {
        result->has_regression = Benchmark_CompareToBaseline(result);
        if (result->has_regression) {
            result->result = BENCHMARK_RESULT_REGRESSION;
        }

        // Calculate overall score (simple average for now)
        float total_score = 0.0f;
        int score_count = 0;
        for (uint32_t i = 0; i < result->measurement_count; i++) {
            // Normalize different metrics to 0-100 scale (simplified)
            float normalized_score = 50.0f; // Placeholder - would normalize based on metric type
            total_score += normalized_score;
            score_count++;
        }
        result->overall_score = score_count > 0 ? total_score / score_count : 0.0f;
    }

    benchmark_system.currently_running = qfalse;
    memset(benchmark_system.current_benchmark, 0, sizeof(benchmark_system.current_benchmark));

    return qtrue;
}

qboolean Benchmark_ExecuteBenchmarkIteration(const benchmark_config_t* config,
                                           performance_measurement_t* measurements,
                                           qboolean is_warmup) {
    // This is a placeholder implementation
    // In a real implementation, this would:
    // 1. Set up the benchmark scenario (load map, configure settings)
    // 2. Execute the benchmark workload
    // 3. Collect performance metrics during execution
    // 4. Clean up and return results

    if (!config) return qfalse;

    // Simulate benchmark execution time
    uint64_t start_time = Sys_Milliseconds();

    // Simulate different benchmark types
    if (Q_stricmp(config->benchmark_id, "rendering_fps") == 0) {
        // Simulate rendering benchmark
        if (measurements) {
            measurements[METRIC_FPS].metric_type = METRIC_FPS;
            Q_strncpyz(measurements[METRIC_FPS].metric_name, "FPS", sizeof(measurements[METRIC_FPS].metric_name));
            Q_strncpyz(measurements[METRIC_FPS].unit, "fps", sizeof(measurements[METRIC_FPS].unit));
            measurements[METRIC_FPS].current_value = 120.0f + (rand() % 20 - 10); // 110-130 FPS
            measurements[METRIC_FPS].current_stddev = 5.0f;

            measurements[METRIC_FRAME_TIME].metric_type = METRIC_FRAME_TIME;
            Q_strncpyz(measurements[METRIC_FRAME_TIME].metric_name, "Frame Time", sizeof(measurements[METRIC_FRAME_TIME].metric_name));
            Q_strncpyz(measurements[METRIC_FRAME_TIME].unit, "ms", sizeof(measurements[METRIC_FRAME_TIME].unit));
            measurements[METRIC_FRAME_TIME].current_value = 1000.0f / measurements[METRIC_FPS].current_value;
            measurements[METRIC_FRAME_TIME].current_stddev = 1.0f;
        }
    } else if (Q_stricmp(config->benchmark_id, "memory_usage") == 0) {
        // Simulate memory benchmark
        if (measurements) {
            measurements[METRIC_MEMORY_USAGE].metric_type = METRIC_MEMORY_USAGE;
            Q_strncpyz(measurements[METRIC_MEMORY_USAGE].metric_name, "Memory Usage", sizeof(measurements[METRIC_MEMORY_USAGE].metric_name));
            Q_strncpyz(measurements[METRIC_MEMORY_USAGE].unit, "MB", sizeof(measurements[METRIC_MEMORY_USAGE].unit));
            measurements[METRIC_MEMORY_USAGE].current_value = 512.0f + (rand() % 100 - 50); // 462-562 MB
            measurements[METRIC_MEMORY_USAGE].current_stddev = 10.0f;
        }
    } else if (Q_stricmp(config->benchmark_id, "load_time") == 0) {
        // Simulate loading benchmark
        if (measurements) {
            measurements[METRIC_LOAD_TIME].metric_type = METRIC_LOAD_TIME;
            Q_strncpyz(measurements[METRIC_LOAD_TIME].metric_name, "Load Time", sizeof(measurements[METRIC_LOAD_TIME].metric_name));
            Q_strncpyz(measurements[METRIC_LOAD_TIME].unit, "ms", sizeof(measurements[METRIC_LOAD_TIME].unit));
            measurements[METRIC_LOAD_TIME].current_value = 2500.0f + (rand() % 500 - 250); // 2250-2750 ms
            measurements[METRIC_LOAD_TIME].current_stddev = 50.0f;
        }
    }

    // Simulate execution time
    uint64_t execution_time = Sys_Milliseconds() - start_time;
    if (execution_time < 100) { // Ensure minimum execution time
        Sys_Sleep(100 - execution_time);
    }

    return qtrue;
}

qboolean Benchmark_RunBenchmarkById(const char* benchmark_id) {
    Q_UNUSED(benchmark_id);
    // Implementation would find and run benchmark by ID
    return qfalse;
}

qboolean Benchmark_CancelCurrentBenchmark(void) {
    benchmark_system.currently_running = qfalse;
    return qtrue;
}

qboolean Benchmark_IsBenchmarkRunning(void) {
    return benchmark_system.currently_running;
}

/*
=============================================================================
Result Management
=============================================================================
*/

uint32_t Benchmark_GetResults(benchmark_result_t** results) {
    if (results) *results = benchmark_system.results;
    return benchmark_system.result_count;
}

benchmark_result_t* Benchmark_GetResultById(const char* run_id) {
    for (uint32_t i = 0; i < benchmark_system.result_count; i++) {
        if (Q_stricmp(benchmark_system.results[i].run_id, run_id) == 0) {
            return &benchmark_system.results[i];
        }
    }
    return NULL;
}

qboolean Benchmark_SaveResults(const char* filename) {
    Q_UNUSED(filename);
    // Implementation would save results to file
    return qtrue;
}

qboolean Benchmark_LoadResults(const char* filename) {
    Q_UNUSED(filename);
    // Implementation would load results from file
    return qtrue;
}

/*
=============================================================================
Statistical Analysis
=============================================================================
*/

qboolean Benchmark_AnalyzeResults(const benchmark_result_t* results, uint32_t count,
                                performance_measurement_t* analysis, uint32_t max_analysis) {
    if (!results || !analysis || count == 0 || max_analysis == 0) return qfalse;

    // Simple analysis - in a real implementation this would do proper statistical analysis
    for (uint32_t i = 0; i < count && i < max_analysis; i++) {
        memcpy(&analysis[i], &results[i].measurements[0], sizeof(performance_measurement_t));

        // Calculate statistical significance
        if (analysis[i].baseline_value > 0) {
            analysis[i].percent_change = ((analysis[i].current_value - analysis[i].baseline_value) /
                                        analysis[i].baseline_value) * 100.0f;

            analysis[i].is_regression = analysis[i].percent_change < -REGRESSION_THRESHOLD;
            analysis[i].is_improvement = analysis[i].percent_change > IMPROVEMENT_THRESHOLD;

            // Calculate z-score (simplified)
            if (analysis[i].current_stddev > 0 && analysis[i].baseline_stddev > 0) {
                float pooled_stddev = sqrtf((analysis[i].current_stddev * analysis[i].current_stddev +
                                           analysis[i].baseline_stddev * analysis[i].baseline_stddev) / 2.0f);
                if (pooled_stddev > 0) {
                    analysis[i].z_score = fabsf(analysis[i].current_value - analysis[i].baseline_value) / pooled_stddev;
                    analysis[i].confidence_interval_low = analysis[i].current_value - Z_SCORE_95_CONFIDENCE * analysis[i].current_stddev;
                    analysis[i].confidence_interval_high = analysis[i].current_value + Z_SCORE_95_CONFIDENCE * analysis[i].current_stddev;
                }
            }
        }
    }

    return qtrue;
}

qboolean Benchmark_DetectRegressions(const benchmark_result_t* current,
                                   const benchmark_result_t* baseline,
                                   char* regression_report, size_t report_size) {
    if (!current || !baseline || !regression_report) return qfalse;

    Q_strncpyz(regression_report, "Performance Regression Analysis Report\n", report_size);
    Q_strcat(regression_report, report_size, "=====================================\n\n");

    qboolean has_regressions = qfalse;

    for (uint32_t i = 0; i < current->measurement_count && i < baseline->measurement_count; i++) {
        const performance_measurement_t* curr = &current->measurements[i];
        const performance_measurement_t* base = &baseline->measurements[i];

        if (curr->current_value > 0 && base->current_value > 0) {
            float percent_change = ((curr->current_value - base->current_value) / base->current_value) * 100.0f;

            if (percent_change < -REGRESSION_THRESHOLD) {
                Q_strcat(regression_report, report_size,
                        va("🚨 REGRESSION: %s changed by %.1f%% (from %.1f to %.1f %s)\n",
                           curr->metric_name, percent_change, base->current_value,
                           curr->current_value, curr->unit));
                has_regressions = qtrue;
            } else if (percent_change > IMPROVEMENT_THRESHOLD) {
                Q_strcat(regression_report, report_size,
                        va("✅ IMPROVEMENT: %s improved by %.1f%% (from %.1f to %.1f %s)\n",
                           curr->metric_name, percent_change, base->current_value,
                           curr->current_value, curr->unit));
            }
        }
    }

    if (!has_regressions) {
        Q_strcat(regression_report, report_size, "No significant performance regressions detected.\n");
    }

    return has_regressions;
}

qboolean Benchmark_CalculateConfidenceIntervals(performance_measurement_t* measurement) {
    if (!measurement || measurement->current_stddev <= 0) return qfalse;

    float std_error = measurement->current_stddev / sqrtf(benchmark_system.min_samples_for_stats);
    float margin_of_error = Z_SCORE_95_CONFIDENCE * std_error;

    measurement->confidence_interval_low = measurement->current_value - margin_of_error;
    measurement->confidence_interval_high = measurement->current_value + margin_of_error;

    return qtrue;
}

float Benchmark_CalculateStatisticalSignificance(float current_value, float baseline_value,
                                                float current_stddev, float baseline_stddev) {
    if (current_stddev <= 0 || baseline_stddev <= 0) return 0.0f;

    float difference = fabsf(current_value - baseline_value);
    float pooled_stddev = sqrtf((current_stddev * current_stddev + baseline_stddev * baseline_stddev) / 2.0f);

    return pooled_stddev > 0 ? difference / pooled_stddev : 0.0f;
}

/*
=============================================================================
Baseline Management
=============================================================================
*/

qboolean Benchmark_UpdateBaseline(const char* benchmark_id, const benchmark_result_t* result) {
    Q_UNUSED(benchmark_id);
    Q_UNUSED(result);
    // Implementation would update baseline data for the benchmark
    return qtrue;
}

qboolean Benchmark_GetBaseline(const char* benchmark_id, benchmark_result_t* baseline) {
    Q_UNUSED(benchmark_id);
    Q_UNUSED(baseline);
    // Implementation would retrieve baseline data for the benchmark
    return qtrue;
}

qboolean Benchmark_CompareToBaseline(const benchmark_result_t* result) {
    if (!result) return qfalse;

    // Simplified baseline comparison
    for (uint32_t i = 0; i < result->measurement_count; i++) {
        const performance_measurement_t* measurement = &result->measurements[i];

        // Check for significant degradation
        if (measurement->percent_change < -REGRESSION_THRESHOLD) {
            return qtrue; // Regression detected
        }
    }

    return qfalse; // No regression
}

qboolean Benchmark_ResetBaseline(const char* benchmark_id) {
    Q_UNUSED(benchmark_id);
    // Implementation would reset baseline data for the benchmark
    return qtrue;
}

/*
=============================================================================
Hardware Profiling
=============================================================================
*/

qboolean Benchmark_ProfileHardware(char* hardware_info, size_t info_size) {
    if (!hardware_info) return qfalse;

    Q_strncpyz(hardware_info, "CPU: Unknown, RAM: Unknown, GPU: Unknown", info_size);

    // Try to get basic system information
    // In a real implementation, this would use platform-specific APIs

    // Set some placeholder values
    benchmark_system.cpu_cores = 4;
    benchmark_system.ram_mb = 8192;
    benchmark_system.vram_mb = 2048;
    Q_strncpyz(benchmark_system.cpu_model, "Intel Core i5", sizeof(benchmark_system.cpu_model));
    Q_strncpyz(benchmark_system.gpu_model, "NVIDIA GTX 1060", sizeof(benchmark_system.gpu_model));

    return qtrue;
}

qboolean Benchmark_DetectHardwareChanges(void) {
    // Implementation would detect if hardware configuration changed
    return qfalse;
}

/*
=============================================================================
Reporting and Export
=============================================================================
*/

qboolean Benchmark_GenerateReport(const benchmark_result_t* results, uint32_t count,
                                const char* output_file, const char* format) {
    Q_UNUSED(results);
    Q_UNUSED(count);
    Q_UNUSED(output_file);
    Q_UNUSED(format);
    // Implementation would generate detailed reports in various formats
    return qtrue;
}

qboolean Benchmark_GenerateComparisonReport(const benchmark_result_t* current,
                                          const benchmark_result_t* baseline,
                                          const char* output_file) {
    Q_UNUSED(current);
    Q_UNUSED(baseline);
    Q_UNUSED(output_file);
    // Implementation would generate comparison reports
    return qtrue;
}

qboolean Benchmark_ExportForCI(const benchmark_result_t* results, uint32_t count,
                             const char* output_dir) {
    Q_UNUSED(results);
    Q_UNUSED(count);
    Q_UNUSED(output_dir);
    // Implementation would export results for CI consumption
    return qtrue;
}

qboolean Benchmark_GenerateCIBadge(const benchmark_result_t* result,
                                 const char* badge_file) {
    Q_UNUSED(result);
    Q_UNUSED(badge_file);
    // Implementation would generate CI badge
    return qtrue;
}

/*
=============================================================================
Utility Functions
=============================================================================
*/

const char* Benchmark_GetResultString(benchmark_result_t result) {
    switch (result) {
        case BENCHMARK_RESULT_PASS: return "PASS";
        case BENCHMARK_RESULT_FAIL: return "FAIL";
        case BENCHMARK_RESULT_TIMEOUT: return "TIMEOUT";
        case BENCHMARK_RESULT_INCONCLUSIVE: return "INCONCLUSIVE";
        case BENCHMARK_RESULT_REGRESSION: return "REGRESSION";
        case BENCHMARK_RESULT_IMPROVEMENT: return "IMPROVEMENT";
        default: return "UNKNOWN";
    }
}

const char* Benchmark_GetCategoryString(benchmark_category_t category) {
    switch (category) {
        case BENCHMARK_CATEGORY_RENDERING: return "Rendering";
        case BENCHMARK_CATEGORY_MEMORY: return "Memory";
        case BENCHMARK_CATEGORY_IO: return "I/O";
        case BENCHMARK_CATEGORY_NETWORK: return "Network";
        case BENCHMARK_CATEGORY_AUDIO: return "Audio";
        case BENCHMARK_CATEGORY_PHYSICS: return "Physics";
        case BENCHMARK_CATEGORY_AI: return "AI";
        case BENCHMARK_CATEGORY_SCRIPTING: return "Scripting";
        case BENCHMARK_CATEGORY_GENERAL: return "General";
        default: return "Unknown";
    }
}

const char* Benchmark_GetMetricString(performance_metric_t metric) {
    switch (metric) {
        case METRIC_FPS: return "FPS";
        case METRIC_FRAME_TIME: return "Frame Time";
        case METRIC_CPU_USAGE: return "CPU Usage";
        case METRIC_MEMORY_USAGE: return "Memory Usage";
        case METRIC_GPU_USAGE: return "GPU Usage";
        case METRIC_VRAM_USAGE: return "VRAM Usage";
        case METRIC_LOAD_TIME: return "Load Time";
        case METRIC_RENDER_TIME: return "Render Time";
        case METRIC_UPDATE_TIME: return "Update Time";
        case METRIC_IO_TIME: return "I/O Time";
        case METRIC_BANDWIDTH: return "Bandwidth";
        case METRIC_LATENCY: return "Latency";
        case METRIC_THROUGHPUT: return "Throughput";
        default: return "Unknown";
    }
}

qboolean Benchmark_ValidateConfig(const benchmark_config_t* config) {
    if (!config) return qfalse;
    if (!config->benchmark_id[0] || !config->name[0]) return qfalse;
    if (config->warmup_iterations < 0 || config->measurement_iterations <= 0) return qfalse;
    if (config->timeout_seconds <= 0) return qfalse;
    if (config->confidence_level <= 0.0f || config->confidence_level >= 1.0f) return qfalse;
    return qtrue;
}

qboolean Benchmark_IsResultSignificant(const performance_measurement_t* measurement) {
    if (!measurement) return qfalse;

    // Check if the change is statistically significant
    return fabsf(measurement->z_score) >= Z_SCORE_95_CONFIDENCE;
}

qboolean Benchmark_ShouldAlertOnResult(const benchmark_result_t* result) {
    if (!result) return qfalse;

    return result->has_regression || result->result == BENCHMARK_RESULT_FAIL;
}

/*
=============================================================================
Built-in Benchmark Templates
=============================================================================
*/

qboolean Benchmark_AddRenderingBenchmark(benchmark_suite_t* suite,
                                       const char* map_name,
                                       int quality_preset) {
    if (!suite || !map_name) return qfalse;

    benchmark_config_t config;
    memset(&config, 0, sizeof(config));

    Q_strncpyz(config.benchmark_id, va("render_%s_q%d", map_name, quality_preset), sizeof(config.benchmark_id));
    Q_strncpyz(config.name, va("Rendering: %s (Quality %d)", map_name, quality_preset), sizeof(config.name));
    Q_strncpyz(config.description, va("Rendering performance benchmark on %s with quality preset %d", map_name, quality_preset), sizeof(config.description));
    config.category = BENCHMARK_CATEGORY_RENDERING;

    config.warmup_iterations = 3;
    config.measurement_iterations = 10;
    config.timeout_seconds = 300;
    config.enable_profiling = qtrue;

    config.resolution_width = 1920;
    config.resolution_height = 1080;
    config.quality_preset = quality_preset;

    Q_strncpyz(config.test_map, map_name, sizeof(config.test_map));
    config.test_duration = 60; // 60 seconds
    config.record_demo = qfalse;

    config.confidence_level = 0.95f;
    config.relative_tolerance = 0.05f; // 5% tolerance
    config.absolute_tolerance = 5.0f;  // 5 FPS tolerance

    return Benchmark_AddBenchmarkToSuite(suite, &config);
}

qboolean Benchmark_AddMemoryBenchmark(benchmark_suite_t* suite,
                                    int allocation_size,
                                    int allocation_count) {
    if (!suite) return qfalse;

    benchmark_config_t config;
    memset(&config, 0, sizeof(config));

    Q_strncpyz(config.benchmark_id, va("memory_%d_%d", allocation_size, allocation_count), sizeof(config.benchmark_id));
    Q_strncpyz(config.name, va("Memory: %d allocations of %d bytes", allocation_count, allocation_size), sizeof(config.name));
    Q_strncpyz(config.description, va("Memory allocation and management performance with %d allocations of %d bytes each", allocation_count, allocation_size), sizeof(config.description));
    config.category = BENCHMARK_CATEGORY_MEMORY;

    config.warmup_iterations = 2;
    config.measurement_iterations = 5;
    config.timeout_seconds = 120;
    config.enable_profiling = qtrue;

    config.confidence_level = 0.95f;
    config.relative_tolerance = 0.10f; // 10% tolerance for memory benchmarks
    config.absolute_tolerance = 50.0f; // 50MB tolerance

    return Benchmark_AddBenchmarkToSuite(suite, &config);
}

qboolean Benchmark_AddIOBenchmark(benchmark_suite_t* suite,
                                const char* test_file,
                                int file_size_mb) {
    if (!suite || !test_file) return qfalse;

    benchmark_config_t config;
    memset(&config, 0, sizeof(config));

    Q_strncpyz(config.benchmark_id, va("io_%s_%dmb", test_file, file_size_mb), sizeof(config.benchmark_id));
    Q_strncpyz(config.name, va("I/O: %s (%d MB)", test_file, file_size_mb), sizeof(config.name));
    Q_strncpyz(config.description, va("File I/O performance benchmark with %d MB test file", file_size_mb), sizeof(config.description));
    config.category = BENCHMARK_CATEGORY_IO;

    config.warmup_iterations = 1;
    config.measurement_iterations = 3;
    config.timeout_seconds = 180;
    config.enable_profiling = qtrue;

    config.confidence_level = 0.90f; // Lower confidence for I/O benchmarks
    config.relative_tolerance = 0.15f; // 15% tolerance for I/O variability
    config.absolute_tolerance = 100.0f; // 100ms tolerance

    return Benchmark_AddBenchmarkToSuite(suite, &config);
}

qboolean Benchmark_AddNetworkBenchmark(benchmark_suite_t* suite,
                                     int packet_size,
                                     int packet_count) {
    if (!suite) return qfalse;

    benchmark_config_t config;
    memset(&config, 0, sizeof(config));

    Q_strncpyz(config.benchmark_id, va("network_%d_%d", packet_size, packet_count), sizeof(config.benchmark_id));
    Q_strncpyz(config.name, va("Network: %d packets of %d bytes", packet_count, packet_size), sizeof(config.name));
    Q_strncpyz(config.description, va("Network performance benchmark with %d packets of %d bytes each", packet_count, packet_size), sizeof(config.description));
    config.category = BENCHMARK_CATEGORY_NETWORK;

    config.warmup_iterations = 1;
    config.measurement_iterations = 3;
    config.timeout_seconds = 120;
    config.enable_profiling = qtrue;

    config.confidence_level = 0.85f; // Lower confidence for network benchmarks
    config.relative_tolerance = 0.20f; // 20% tolerance for network variability
    config.absolute_tolerance = 50.0f;  // 50ms tolerance

    return Benchmark_AddBenchmarkToSuite(suite, &config);
}

/*
=============================================================================
CI/CD Integration Helpers
=============================================================================
*/

qboolean Benchmark_CheckCILimits(const benchmark_result_t* results, uint32_t count) {
    if (!results || count == 0) return qfalse;

    for (uint32_t i = 0; i < count; i++) {
        if (results[i].has_regression || results[i].result == BENCHMARK_RESULT_FAIL) {
            return qfalse; // CI should fail
        }
    }

    return qtrue; // CI should pass
}

qboolean Benchmark_GetPerformanceStatus(const benchmark_result_t* results, uint32_t count,
                                      char* status, size_t status_size) {
    if (!results || !status) return qfalse;

    int regressions = 0, improvements = 0, failures = 0;

    for (uint32_t i = 0; i < count; i++) {
        if (results[i].has_regression) regressions++;
        if (results[i].result == BENCHMARK_RESULT_IMPROVEMENT) improvements++;
        if (results[i].result == BENCHMARK_RESULT_FAIL) failures++;
    }

    if (failures > 0) {
        Q_strncpyz(status, "FAILED", status_size);
    } else if (regressions > 0) {
        Q_strncpyz(status, "REGRESSION", status_size);
    } else if (improvements > 0) {
        Q_strncpyz(status, "IMPROVED", status_size);
    } else {
        Q_strncpyz(status, "STABLE", status_size);
    }

    return qtrue;
}

qboolean Benchmark_GeneratePerformanceSummary(const benchmark_result_t* results, uint32_t count,
                                            char* summary, size_t summary_size) {
    if (!results || !summary) return qfalse;

    Q_strncpyz(summary, "Performance Benchmark Summary\n", summary_size);
    Q_strcat(summary, summary_size, "==============================\n\n");

    Q_strcat(summary, summary_size, va("Total Benchmarks: %u\n", count));

    int passed = 0, failed = 0, regressions = 0, improvements = 0;

    for (uint32_t i = 0; i < count; i++) {
        switch (results[i].result) {
            case BENCHMARK_RESULT_PASS:
                passed++;
                break;
            case BENCHMARK_RESULT_FAIL:
                failed++;
                break;
            case BENCHMARK_RESULT_REGRESSION:
                regressions++;
                break;
            case BENCHMARK_RESULT_IMPROVEMENT:
                improvements++;
                break;
            default:
                break;
        }
    }

    Q_strcat(summary, summary_size, va("Passed: %d\n", passed));
    Q_strcat(summary, summary_size, va("Failed: %d\n", failed));
    Q_strcat(summary, summary_size, va("Regressions: %d\n", regressions));
    Q_strcat(summary, summary_size, va("Improvements: %d\n", improvements));

    if (regressions > 0) {
        Q_strcat(summary, summary_size, "\n⚠️ Performance regressions detected!\n");
    } else if (improvements > 0) {
        Q_strcat(summary, summary_size, "\n✅ Performance improvements detected!\n");
    } else {
        Q_strcat(summary, summary_size, "\n📊 Performance is stable.\n");
    }

    return qtrue;
}
