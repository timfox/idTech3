/*
=============================================================================
Performance Benchmarking Framework

Automated performance regression detection and benchmarking infrastructure.
=============================================================================
*/

#ifndef __PERFORMANCE_BENCHMARK_H__
#define __PERFORMANCE_BENCHMARK_H__

#include "q_shared.h"

// Benchmark result types
typedef enum {
    BENCHMARK_RESULT_PASS = 0,         // Benchmark completed successfully
    BENCHMARK_RESULT_FAIL,             // Benchmark failed
    BENCHMARK_RESULT_TIMEOUT,          // Benchmark timed out
    BENCHMARK_RESULT_INCONCLUSIVE,     // Results inconclusive
    BENCHMARK_RESULT_REGRESSION,       // Performance regression detected
    BENCHMARK_RESULT_IMPROVEMENT,      // Performance improvement detected
    BENCHMARK_RESULT_COUNT
} benchmark_result_type_t;

// Benchmark categories
typedef enum {
    BENCHMARK_CATEGORY_RENDERING = 0,  // Rendering performance
    BENCHMARK_CATEGORY_MEMORY,          // Memory usage and allocation
    BENCHMARK_CATEGORY_IO,              // File I/O performance
    BENCHMARK_CATEGORY_NETWORK,         // Network performance
    BENCHMARK_CATEGORY_AUDIO,           // Audio processing performance
    BENCHMARK_CATEGORY_PHYSICS,         // Physics simulation performance
    BENCHMARK_CATEGORY_AI,              // AI/pathfinding performance
    BENCHMARK_CATEGORY_SCRIPTING,       // Scripting performance
    BENCHMARK_CATEGORY_GENERAL,         // General system performance
    BENCHMARK_CATEGORY_COUNT
} benchmark_category_t;

// Performance metrics
typedef enum {
    METRIC_FPS = 0,                    // Frames per second
    METRIC_FRAME_TIME,                 // Frame time in milliseconds
    METRIC_CPU_USAGE,                  // CPU usage percentage
    METRIC_MEMORY_USAGE,               // Memory usage in MB
    METRIC_GPU_USAGE,                  // GPU usage percentage
    METRIC_VRAM_USAGE,                 // VRAM usage in MB
    METRIC_LOAD_TIME,                  // Loading time in milliseconds
    METRIC_RENDER_TIME,                // Rendering time in milliseconds
    METRIC_UPDATE_TIME,                // Update time in milliseconds
    METRIC_IO_TIME,                    // I/O operation time
    METRIC_BANDWIDTH,                  // Data transfer bandwidth
    METRIC_LATENCY,                    // Operation latency
    METRIC_THROUGHPUT,                 // Operations per second
    METRIC_COUNT
} performance_metric_t;

// Benchmark configuration
typedef struct {
    char benchmark_id[64];             // Unique benchmark identifier
    char name[128];                    // Human-readable name
    char description[256];             // Detailed description
    benchmark_category_t category;     // Benchmark category

    // Execution parameters
    int warmup_iterations;             // Number of warmup iterations
    int measurement_iterations;        // Number of measurement iterations
    int timeout_seconds;               // Benchmark timeout
    qboolean enable_profiling;         // Enable detailed profiling

    // Quality settings for comparison
    int resolution_width;              // Screen width for rendering tests
    int resolution_height;             // Screen height for rendering tests
    int quality_preset;                // Quality preset (0-4, Potato to Ultra)

    // Test parameters
    char test_map[64];                 // Map to load for testing
    char test_scenario[128];           // Specific test scenario
    int test_duration;                 // Test duration in seconds
    qboolean record_demo;              // Record demo during test

    // Statistical parameters
    float confidence_level;            // Statistical confidence level (0.95 = 95%)
    float relative_tolerance;          // Relative tolerance for regression detection
    float absolute_tolerance;          // Absolute tolerance for regression detection
} benchmark_config_t;

// Performance measurement
typedef struct {
    performance_metric_t metric_type;
    char metric_name[32];
    char unit[16];                     // Unit of measurement (FPS, ms, MB, etc.)

    // Current measurement
    float current_value;
    float current_stddev;              // Standard deviation

    // Historical comparison
    float baseline_value;              // Baseline/reference value
    float baseline_stddev;
    float previous_value;              // Previous run value
    float previous_stddev;

    // Statistical analysis
    float percent_change;              // Percentage change from baseline
    float z_score;                     // Statistical significance
    qboolean is_regression;            // Whether this indicates a regression
    qboolean is_improvement;           // Whether this indicates an improvement
    float confidence_interval_low;     // Confidence interval lower bound
    float confidence_interval_high;    // Confidence interval upper bound

    // Trend analysis
    float trend_slope;                 // Rate of change over time
    int trend_direction;               // -1 = degrading, 0 = stable, 1 = improving
} performance_measurement_t;

// Benchmark result
typedef struct {
    char benchmark_id[64];
    char run_id[64];                   // Unique run identifier
    benchmark_result_type_t result;
    uint64_t start_time;
    uint64_t end_time;
    uint64_t duration_ms;

    // Execution details
    char platform[32];
    char hardware_config[128];
    char software_config[128];
    char git_commit[64];
    char git_branch[64];

    // Performance measurements
    performance_measurement_t* measurements;
    uint32_t measurement_count;
    uint32_t max_measurements;

    // Statistical summary
    float overall_score;               // Composite performance score
    qboolean has_regression;           // Whether any regression was detected
    qboolean has_significant_change;   // Whether any significant change occurred
    float confidence_level;            // Statistical confidence in results

    // Error information
    char error_message[256];
    qboolean timed_out;
    qboolean crashed;
} benchmark_result_t;

// Benchmark suite
typedef struct {
    char suite_name[64];
    char description[256];
    benchmark_config_t* benchmarks;
    uint32_t benchmark_count;
    uint32_t max_benchmarks;

    // Suite configuration
    qboolean run_in_parallel;          // Run benchmarks in parallel
    int max_parallel_benchmarks;       // Maximum parallel execution
    qboolean stop_on_failure;          // Stop suite on first failure
    int suite_timeout_minutes;         // Total suite timeout

    // Statistical parameters
    float regression_threshold;        // Threshold for regression detection
    float improvement_threshold;       // Threshold for improvement detection
    float statistical_significance;    // Required statistical significance
} benchmark_suite_t;

// Benchmark system
typedef struct {
    char system_name[64];
    char description[256];

    // Benchmark storage
    benchmark_suite_t* suites;
    uint32_t suite_count;
    uint32_t max_suites;

    // Historical results
    benchmark_result_t* results;
    uint32_t result_count;
    uint32_t max_results;

    // System configuration
    qboolean auto_baseline_update;     // Automatically update baselines
    qboolean enable_regression_alerts; // Enable regression alerts
    int baseline_retention_days;       // How long to keep baseline data
    float global_regression_threshold; // Global regression threshold

    // Statistical configuration
    int min_samples_for_stats;         // Minimum samples for statistical analysis
    float statistical_confidence;      // Default confidence level
    qboolean use_robust_statistics;    // Use robust statistical methods

    // System state
    qboolean initialized;
    qboolean currently_running;
    char current_benchmark[64];

    // Hardware profiling
    qboolean enable_hardware_profiling;
    char cpu_model[64];
    char gpu_model[64];
    int cpu_cores;
    int ram_mb;
    int vram_mb;
} benchmark_system_t;

extern benchmark_system_t benchmark_system;

// Performance Benchmarking API
qboolean Benchmark_Init(void);
void Benchmark_Shutdown(void);

// Benchmark Suite Management
benchmark_suite_t* Benchmark_CreateSuite(const char* name, const char* description);
qboolean Benchmark_AddBenchmarkToSuite(benchmark_suite_t* suite, const benchmark_config_t* config);
qboolean Benchmark_RemoveBenchmarkFromSuite(benchmark_suite_t* suite, const char* benchmark_id);

// Benchmark Execution
qboolean Benchmark_RunSuite(benchmark_suite_t* suite);
qboolean Benchmark_RunBenchmark(const benchmark_config_t* config, benchmark_result_t* result);
qboolean Benchmark_RunBenchmarkById(const char* benchmark_id);
qboolean Benchmark_CancelCurrentBenchmark(void);
qboolean Benchmark_IsBenchmarkRunning(void);

// Result Management
uint32_t Benchmark_GetResults(benchmark_result_t** results);
benchmark_result_t* Benchmark_GetResultById(const char* run_id);
qboolean Benchmark_SaveResults(const char* filename);
qboolean Benchmark_LoadResults(const char* filename);

// Statistical Analysis
qboolean Benchmark_AnalyzeResults(const benchmark_result_t* results, uint32_t count,
                                performance_measurement_t* analysis, uint32_t max_analysis);
qboolean Benchmark_DetectRegressions(const benchmark_result_t* current,
                                   const benchmark_result_t* baseline,
                                   char* regression_report, size_t report_size);
qboolean Benchmark_CalculateConfidenceIntervals(performance_measurement_t* measurement);
float Benchmark_CalculateStatisticalSignificance(float current_value, float baseline_value,
                                                float current_stddev, float baseline_stddev);

// Baseline Management
qboolean Benchmark_UpdateBaseline(const char* benchmark_id, const benchmark_result_t* result);
qboolean Benchmark_GetBaseline(const char* benchmark_id, benchmark_result_t* baseline);
qboolean Benchmark_CompareToBaseline(const benchmark_result_t* result);
qboolean Benchmark_ResetBaseline(const char* benchmark_id);

// Hardware Profiling
qboolean Benchmark_ProfileHardware(char* hardware_info, size_t info_size);
qboolean Benchmark_DetectHardwareChanges(void);

// Reporting and Export
qboolean Benchmark_GenerateReport(const benchmark_result_t* results, uint32_t count,
                                const char* output_file, const char* format);
qboolean Benchmark_GenerateComparisonReport(const benchmark_result_t* current,
                                          const benchmark_result_t* baseline,
                                          const char* output_file);
qboolean Benchmark_ExportForCI(const benchmark_result_t* results, uint32_t count,
                             const char* output_dir);
qboolean Benchmark_GenerateCIBadge(const benchmark_result_t* result,
                                 const char* badge_file);

// Utility Functions
const char* Benchmark_GetResultString(benchmark_result_type_t result);
const char* Benchmark_GetCategoryString(benchmark_category_t category);
const char* Benchmark_GetMetricString(performance_metric_t metric);
qboolean Benchmark_ValidateConfig(const benchmark_config_t* config);
qboolean Benchmark_IsResultSignificant(const performance_measurement_t* measurement);
qboolean Benchmark_ShouldAlertOnResult(const benchmark_result_t* result);

// Built-in Benchmark Templates
qboolean Benchmark_AddRenderingBenchmark(benchmark_suite_t* suite,
                                       const char* map_name,
                                       int quality_preset);
qboolean Benchmark_AddMemoryBenchmark(benchmark_suite_t* suite,
                                    int allocation_size,
                                    int allocation_count);
qboolean Benchmark_AddIOBenchmark(benchmark_suite_t* suite,
                                const char* test_file,
                                int file_size_mb);
qboolean Benchmark_AddNetworkBenchmark(benchmark_suite_t* suite,
                                     int packet_size,
                                     int packet_count);

// CI/CD Integration
qboolean Benchmark_CheckCILimits(const benchmark_result_t* results, uint32_t count);
qboolean Benchmark_GetPerformanceStatus(const benchmark_result_t* results, uint32_t count,
                                      char* status, size_t status_size);
qboolean Benchmark_GeneratePerformanceSummary(const benchmark_result_t* results, uint32_t count,
                                            char* summary, size_t summary_size);

#endif // __PERFORMANCE_BENCHMARK_H__
