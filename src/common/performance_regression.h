/*
=============================================================================
Performance Regression Testing System

Automated performance regression detection with simplified interface.
=============================================================================
*/

#ifndef __PERFORMANCE_REGRESSION_H__
#define __PERFORMANCE_REGRESSION_H__

#include "q_shared.h"

// Regression test configuration
typedef struct {
    char test_name[64];
    char description[128];
    int warmup_frames;         // Frames to skip before measuring
    int measurement_frames;    // Frames to measure
    float fps_threshold;       // Minimum acceptable FPS
    float regression_threshold; // Percentage change that indicates regression
    qboolean enable_profiling; // Enable detailed profiling
} regression_test_config_t;

// Performance measurement
typedef struct {
    float avg_fps;
    float min_fps;
    float max_fps;
    float frame_time_avg;      // Average frame time in ms
    float frame_time_stddev;
    uint64_t measurement_time; // When measurement was taken
} performance_measurement_t;

// Regression test result
typedef struct {
    char test_name[64];
    performance_measurement_t current;
    performance_measurement_t baseline;
    float fps_change_percentage;
    float frame_time_change_percentage;
    qboolean is_regression;
    qboolean is_improvement;
    char status_message[256];
    qboolean test_passed;
} regression_test_result_t;

// Regression testing system
typedef struct {
    qboolean initialized;
    qboolean currently_testing;
    char current_test[64];

    // Test configurations
    regression_test_config_t* tests;
    uint32_t test_count;
    uint32_t max_tests;

    // Results storage
    regression_test_result_t* results;
    uint32_t result_count;
    uint32_t max_results;

    // Baseline management
    char baseline_file[256];
    qboolean auto_update_baseline;

    // Statistical parameters
    float confidence_level;    // Statistical confidence (0.95 = 95%)
    float regression_threshold; // Default regression threshold (%)
    int min_samples;           // Minimum samples for statistical analysis

    // System state
    uint64_t test_start_time;
    int current_frame_count;
    float* frame_times;        // Ring buffer for frame times
    uint32_t frame_buffer_size;
    uint32_t frame_count;
} regression_system_t;

extern regression_system_t regression_system;

// Performance Regression API
qboolean Regression_Init(const char* baseline_file);
void Regression_Shutdown(void);

// Test Configuration
qboolean Regression_AddTest(const regression_test_config_t* config);
qboolean Regression_RemoveTest(const char* test_name);
const regression_test_config_t* Regression_GetTest(const char* test_name);

// Test Execution
qboolean Regression_RunTest(const char* test_name);
qboolean Regression_RunAllTests(void);
qboolean Regression_IsTestRunning(void);
qboolean Regression_CancelCurrentTest(void);

// Frame Time Recording (called each frame during testing)
void Regression_RecordFrameTime(float frame_time_ms);

// Result Management
uint32_t Regression_GetResults(regression_test_result_t** results);
const regression_test_result_t* Regression_GetResult(const char* test_name);
qboolean Regression_SaveResults(const char* filename);
qboolean Regression_LoadResults(const char* filename);

// Baseline Management
qboolean Regression_UpdateBaseline(const char* test_name);
qboolean Regression_LoadBaseline(void);
qboolean Regression_SaveBaseline(void);
qboolean Regression_ResetBaseline(const char* test_name);

// Statistical Analysis
qboolean Regression_AnalyzeResult(regression_test_result_t* result);
float Regression_CalculateConfidenceInterval(const performance_measurement_t* measurements,
                                           int count, float confidence_level);

// Utility Functions
qboolean Regression_ValidateConfiguration(const regression_test_config_t* config);
const char* Regression_GetTestStatus(const regression_test_result_t* result);
void Regression_PrintTestSummary(void);
void Regression_PrintDetailedResults(void);

// Predefined Test Configurations
qboolean Regression_AddStandardRenderingTest(const char* map_name, int quality_preset);
qboolean Regression_AddMemoryStressTest(size_t allocation_size, int allocation_count);
qboolean Regression_AddIOBenchmarkTest(const char* test_file, size_t file_size);

// CI/CD Integration
qboolean Regression_CheckCIThresholds(void);
qboolean Regression_GenerateCIReport(const char* output_file);
qboolean Regression_HasRegressions(void);
int Regression_GetRegressionCount(void);

// Real-time Monitoring
void Regression_UpdateRealTimeStats(void);
void Regression_GetRealTimeStats(float* avg_fps, float* min_fps, float* max_fps);

#endif // __PERFORMANCE_REGRESSION_H__
