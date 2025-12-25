/*
=============================================================================
Build Optimization Monitoring System

Tracks build performance, binary size, and optimization effectiveness.
=============================================================================
*/

#ifndef __BUILD_OPTIMIZATION_H__
#define __BUILD_OPTIMIZATION_H__

#include "q_shared.h"

// Build optimization metrics
typedef struct {
    uint64_t build_start_time;
    uint64_t build_end_time;
    uint64_t build_duration_ms;
    uint64_t binary_size_bytes;
    uint32_t num_sections;
    uint32_t num_functions;
    uint32_t num_variables;
    qboolean lto_enabled;
    qboolean optimizations_enabled;
    char compiler_version[64];
    char build_type[32];
} build_metrics_t;

// Optimization effectiveness tracking
typedef struct {
    uint32_t total_builds;
    uint64_t average_build_time_ms;
    uint64_t average_binary_size_bytes;
    uint64_t min_binary_size_bytes;
    uint64_t max_binary_size_bytes;
    double size_reduction_percentage;
    double build_time_improvement_percentage;
    qboolean lto_effective;
    qboolean dead_code_elimination_effective;
} optimization_effectiveness_t;

// Binary analysis data
typedef struct {
    uint32_t text_section_size;
    uint32_t data_section_size;
    uint32_t bss_section_size;
    uint32_t rodata_section_size;
    uint32_t total_sections;
    uint32_t stripped_symbols;
    double compression_ratio;
} binary_analysis_t;

// Build optimization monitor
typedef struct {
    qboolean enabled;
    build_metrics_t current_metrics;
    optimization_effectiveness_t effectiveness;
    binary_analysis_t binary_analysis;
    char stats_file[256];
    char report_file[256];
} build_optimization_monitor_t;

extern build_optimization_monitor_t build_optimization_monitor;

// Build optimization API
qboolean BuildOptimization_Init(const char* stats_file, const char* report_file);
void BuildOptimization_Shutdown(void);

// Metrics collection
void BuildOptimization_StartBuild(void);
void BuildOptimization_EndBuild(uint64_t binary_size_bytes);
void BuildOptimization_RecordBinaryAnalysis(const binary_analysis_t* analysis);
void BuildOptimization_SetCompilerInfo(const char* version, const char* build_type);
void BuildOptimization_SetOptimizationFlags(qboolean lto, qboolean optimizations);

// Statistics and analysis
void BuildOptimization_GetMetrics(build_metrics_t* metrics);
void BuildOptimization_GetEffectiveness(optimization_effectiveness_t* effectiveness);
void BuildOptimization_GetBinaryAnalysis(binary_analysis_t* analysis);
void BuildOptimization_CalculateEffectiveness(void);

// Reporting
void BuildOptimization_GenerateReport(void);
void BuildOptimization_PrintSummary(void);
void BuildOptimization_SaveStatistics(void);
qboolean BuildOptimization_LoadStatistics(void);

// Utility functions
uint64_t BuildOptimization_GetAverageBuildTime(void);
uint64_t BuildOptimization_GetAverageBinarySize(void);
double BuildOptimization_GetSizeReduction(void);
double BuildOptimization_GetBuildTimeImprovement(void);
qboolean BuildOptimization_IsOptimizationEffective(void);

// Console commands
void BuildOptimization_Status_f(void);
void BuildOptimization_Metrics_f(void);
void BuildOptimization_Report_f(void);
void BuildOptimization_Analyze_f(void);
void BuildOptimization_Reset_f(void);

#endif // __BUILD_OPTIMIZATION_H__
