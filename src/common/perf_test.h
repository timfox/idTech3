/*
=============================================================================
Performance Test Framework

Automated performance validation and regression testing for CI/CD pipelines.
=============================================================================
*/

#ifndef __PERF_TEST_H__
#define __PERF_TEST_H__

#include "q_shared.h"
#include "thread_platform.h"

// Performance test result types
typedef enum {
    PERF_RESULT_SUCCESS = 0,    // Test passed
    PERF_RESULT_FAILURE,        // Test failed
    PERF_RESULT_REGRESSION,     // Performance regression detected
    PERF_RESULT_IMPROVEMENT,    // Performance improvement detected
    PERF_RESULT_INCONCLUSIVE,   // Test results inconclusive
    PERF_RESULT_ERROR           // Test execution error
} perf_test_result_t;

// Performance metric types
typedef enum {
    PERF_METRIC_FPS = 0,        // Frames per second
    PERF_METRIC_FRAME_TIME,     // Frame time in milliseconds
    PERF_METRIC_CPU_USAGE,      // CPU usage percentage
    PERF_METRIC_MEMORY_USAGE,   // Memory usage in MB
    PERF_METRIC_GPU_USAGE,      // GPU usage percentage
    PERF_METRIC_LOAD_TIME,      // Asset/model load time
    PERF_METRIC_RENDER_TIME,    // Rendering time
    PERF_METRIC_NETWORK_LATENCY,// Network latency
    PERF_METRIC_DISK_IO,        // Disk I/O operations
    PERF_METRIC_CUSTOM          // Custom metric
} perf_metric_type_t;

// Performance test configuration
typedef struct {
    char name[64];              // Test name
    char description[256];      // Test description
    int duration_seconds;       // Test duration
    int warmup_seconds;         // Warmup time before measurements
    int sample_interval_ms;     // How often to sample metrics
    qboolean save_screenshots;  // Save screenshots during test
    qboolean record_video;      // Record video during test
    char map_name[64];          // Map to load for test
    char config_file[256];      // Custom config file
} perf_test_config_t;

// Performance measurement sample
typedef struct {
    uint64_t timestamp;         // Sample timestamp
    perf_metric_type_t type;    // Metric type
    char name[32];              // Metric name
    double value;               // Measured value
    double min_value;           // Minimum value in sample window
    double max_value;           // Maximum value in sample window
    double avg_value;           // Average value in sample window
} perf_sample_t;

// Performance test result
typedef struct {
    char test_name[64];         // Test name
    perf_test_result_t result;  // Overall result
    uint64_t start_time;        // Test start timestamp
    uint64_t end_time;          // Test end timestamp
    uint32_t sample_count;      // Number of samples collected

    // Metric summaries
    double avg_fps;             // Average FPS
    double min_fps;             // Minimum FPS
    double max_fps;             // Maximum FPS
    double avg_frame_time;      // Average frame time
    double max_frame_time;      // Maximum frame time (99th percentile)

    double avg_cpu_usage;       // Average CPU usage
    double peak_cpu_usage;      // Peak CPU usage

    double avg_memory_usage;    // Average memory usage (MB)
    double peak_memory_usage;   // Peak memory usage (MB)

    double avg_gpu_usage;       // Average GPU usage
    double peak_gpu_usage;      // Peak GPU usage

    // Custom metrics
    perf_sample_t* samples;     // All collected samples
    uint32_t max_samples;
    uint32_t num_samples;

    // Regression analysis
    qboolean regression_detected;
    double regression_percentage;
    char regression_reason[256];

    // Test metadata
    char branch_name[64];       // Git branch
    char commit_hash[41];       // Git commit hash
    char build_number[32];      // CI build number
    char platform[32];          // Platform (Windows, Linux, etc.)
    char hardware_config[256];  // Hardware configuration
} perf_test_result_t;

// Performance baseline data
typedef struct {
    char test_name[64];         // Test name
    char baseline_version[32];  // Baseline version/commit
    uint64_t baseline_timestamp;// When baseline was established

    // Baseline metrics
    double baseline_fps_avg;
    double baseline_fps_min;
    double baseline_frame_time_avg;
    double baseline_frame_time_max;
    double baseline_cpu_avg;
    double baseline_memory_avg;
    double baseline_gpu_avg;

    // Tolerance settings
    double fps_tolerance_percent;      // Allowable FPS variation
    double frame_time_tolerance_percent; // Allowable frame time variation
    double cpu_tolerance_percent;      // Allowable CPU usage variation
    double memory_tolerance_percent;   // Allowable memory usage variation
    double gpu_tolerance_percent;      // Allowable GPU usage variation

    // Regression thresholds
    double regression_threshold_percent; // Minimum change to consider regression
    int consecutive_failures_required;   // Consecutive failures needed for regression
} perf_baseline_t;

// Performance test suite
typedef struct {
    char suite_name[64];        // Suite name
    char description[256];      // Suite description
    perf_test_config_t* tests;  // Array of tests
    uint32_t num_tests;         // Number of tests
    uint32_t max_tests;         // Maximum capacity

    // Suite configuration
    qboolean run_in_parallel;   // Run tests in parallel
    int max_parallel_tests;     // Maximum parallel test count
    qboolean stop_on_failure;   // Stop suite on first failure
    int timeout_seconds;        // Suite timeout
} perf_test_suite_t;

// CI/CD integration
typedef struct {
    char ci_system[32];         // CI system (GitHub Actions, Jenkins, etc.)
    char output_directory[256]; // Output directory for reports
    char report_format[16];     // Report format (JSON, XML, JUnit, etc.)
    qboolean generate_html_report; // Generate HTML report
    qboolean upload_results;    // Upload results to external service
    char upload_url[256];       // Upload URL
    char upload_token[128];     // Upload authentication token

    // Notification settings
    qboolean notify_on_regression; // Send notifications on regressions
    char notification_email[128];  // Email for notifications
    char slack_webhook[256];       // Slack webhook URL
} perf_ci_config_t;

// Performance test system
typedef struct {
    qboolean initialized;
    perf_test_suite_t* current_suite;
    perf_test_result_t* current_results;
    uint32_t max_results;

    // Baseline management
    perf_baseline_t* baselines;
    uint32_t num_baselines;
    uint32_t max_baselines;

    // CI/CD integration
    perf_ci_config_t ci_config;

    // Test execution
    thread_handle_t test_thread;
    qboolean test_running;
    qboolean abort_requested;

    // Statistics
    uint32_t total_tests_run;
    uint32_t total_regressions_detected;
    uint32_t total_improvements_detected;
    uint64_t total_test_time_ms;
} perf_test_system_t;

extern perf_test_system_t perf_test_system;

// Performance Test API
qboolean PerfTest_Init(void);
void PerfTest_Shutdown(void);

// Test Suite Management
perf_test_suite_t* PerfTest_CreateSuite(const char* name, const char* description);
qboolean PerfTest_AddTestToSuite(perf_test_suite_t* suite, const perf_test_config_t* config);
qboolean PerfTest_RunSuite(perf_test_suite_t* suite);
perf_test_result_t* PerfTest_GetSuiteResults(perf_test_suite_t* suite, uint32_t* count);

// Individual Test Management
qboolean PerfTest_RunTest(const perf_test_config_t* config, perf_test_result_t* result);
qboolean PerfTest_CancelTest(void);
qboolean PerfTest_IsTestRunning(void);

// Baseline Management
qboolean PerfTest_LoadBaselines(const char* baseline_file);
qboolean PerfTest_SaveBaselines(const char* baseline_file);
qboolean PerfTest_SetBaseline(const char* test_name, const perf_test_result_t* result);
perf_baseline_t* PerfTest_GetBaseline(const char* test_name);
qboolean PerfTest_CheckRegression(const perf_test_result_t* result, const perf_baseline_t* baseline);

// Result Analysis
qboolean PerfTest_CompareResults(const perf_test_result_t* result1, const perf_test_result_t* result2,
                                double* improvement_percent, char* comparison_desc, size_t desc_size);
qboolean PerfTest_GenerateReport(const perf_test_result_t* results, uint32_t count,
                               const char* output_file, const char* format);

// CI/CD Integration
void PerfTest_SetCIConfig(const perf_ci_config_t* config);
qboolean PerfTest_ExportForCI(const perf_test_result_t* results, uint32_t count,
                            const char* output_dir);
qboolean PerfTest_UploadResults(const char* results_file);

// Utility Functions
uint64_t PerfTest_GetTimestamp(void);
const char* PerfTest_GetResultString(perf_test_result_t result);
const char* PerfTest_GetMetricString(perf_metric_type_t metric);
qboolean PerfTest_ValidateConfig(const perf_test_config_t* config);

// Real-time Performance Sampling
void PerfTest_StartSampling(void);
void PerfTest_StopSampling(void);
qboolean PerfTest_AddSample(perf_metric_type_t type, const char* name, double value);
perf_sample_t* PerfTest_GetSamples(uint32_t* count);

// Benchmark Functions (to be called during test execution)
void PerfTest_BenchmarkBeginFrame(void);
void PerfTest_BenchmarkEndFrame(void);
void PerfTest_BenchmarkLoadBegin(const char* asset_name);
void PerfTest_BenchmarkLoadEnd(const char* asset_name);
void PerfTest_BenchmarkRenderBegin(void);
void PerfTest_BenchmarkRenderEnd(void);

#endif // __PERF_TEST_H__
