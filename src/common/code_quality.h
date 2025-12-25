/*
=============================================================================
Code Quality Analysis Framework

Automated code quality gates with coverage requirements and complexity limits.
=============================================================================
*/

#ifndef __CODE_QUALITY_H__
#define __CODE_QUALITY_H__

#include "q_shared.h"

// Code quality result types
typedef enum {
    QUALITY_RESULT_PASS = 0,         // All quality gates passed
    QUALITY_RESULT_COVERAGE_LOW,     // Code coverage below minimum
    QUALITY_RESULT_COMPLEXITY_HIGH,  // Code complexity above maximum
    QUALITY_RESULT_MAINTAINABILITY_LOW, // Maintainability index too low
    QUALITY_RESULT_DUPLICATION_HIGH, // Code duplication above threshold
    QUALITY_RESULT_STYLE_VIOLATIONS, // Style violations detected
    QUALITY_RESULT_SECURITY_ISSUES,  // Security issues found
    QUALITY_RESULT_TIMEOUT,          // Analysis timed out
    QUALITY_RESULT_ERROR,            // Analysis error
    QUALITY_RESULT_COUNT
} code_quality_result_t;

// Quality metric types
typedef enum {
    QUALITY_METRIC_COVERAGE = 0,     // Code coverage percentage
    QUALITY_METRIC_COMPLEXITY,       // Cyclomatic complexity
    QUALITY_METRIC_MAINTAINABILITY,  // Maintainability index
    QUALITY_METRIC_DUPLICATION,      // Code duplication percentage
    QUALITY_METRIC_STYLE,            // Style compliance score
    QUALITY_METRIC_SECURITY,         // Security score
    QUALITY_METRIC_COUNT
} quality_metric_type_t;

// Quality gate configuration
typedef struct {
    char gate_name[64];
    char description[256];
    quality_metric_type_t metric_type;
    float minimum_threshold;         // Minimum acceptable value
    float maximum_threshold;         // Maximum acceptable value (use -1 for no max)
    qboolean enabled;                // Whether this gate is active
    qboolean blocking;               // Whether failure blocks CI/CD
    int priority;                    // Gate priority (higher = more important)
} quality_gate_config_t;

// Coverage data structure
typedef struct {
    char file_path[256];
    char function_name[128];
    int line_number;
    int total_lines;
    int covered_lines;
    float coverage_percentage;
    uint64_t execution_count;
} coverage_data_t;

// Complexity data structure
typedef struct {
    char file_path[256];
    char function_name[128];
    int line_number;
    int cyclomatic_complexity;
    int cognitive_complexity;
    int nesting_depth;
    int parameter_count;
    int statement_count;
} complexity_data_t;

// Quality analysis result
typedef struct {
    char analysis_name[64];
    code_quality_result_t result;
    uint64_t start_time;
    uint64_t end_time;
    uint64_t duration_ms;

    // Coverage metrics
    float overall_coverage;
    int total_files;
    int total_functions;
    int covered_functions;
    coverage_data_t* coverage_data;
    uint32_t coverage_count;
    uint32_t max_coverage_entries;

    // Complexity metrics
    float average_complexity;
    int max_complexity;
    int functions_above_threshold;
    complexity_data_t* complexity_data;
    uint32_t complexity_count;
    uint32_t max_complexity_entries;

    // Quality scores
    float maintainability_index;
    float duplication_percentage;
    float style_score;
    float security_score;

    // Gate results
    quality_gate_config_t* failed_gates;
    uint32_t failed_gate_count;
    uint32_t max_failed_gates;

    // Analysis metadata
    char platform[32];
    char tool_version[32];
    qboolean incremental_analysis;
    uint64_t total_lines_analyzed;
} code_quality_analysis_t;

// Quality gate system
typedef struct {
    char system_name[64];
    char description[256];
    quality_gate_config_t* gates;
    uint32_t gate_count;
    uint32_t max_gates;

    // Default thresholds
    float min_coverage_percentage;
    int max_cyclomatic_complexity;
    float min_maintainability_index;
    float max_duplication_percentage;

    // System state
    qboolean initialized;
    qboolean strict_mode;
    uint32_t total_analyses_run;
    uint32_t total_gates_passed;
    uint32_t total_gates_failed;
} code_quality_system_t;

extern code_quality_system_t code_quality_system;

// Code Quality Analysis API
qboolean CodeQuality_Init(void);
void CodeQuality_Shutdown(void);

// Quality Gate Management
qboolean CodeQuality_AddGate(const quality_gate_config_t* config);
qboolean CodeQuality_RemoveGate(const char* gate_name);
qboolean CodeQuality_EnableGate(const char* gate_name);
qboolean CodeQuality_DisableGate(const char* gate_name);
qboolean CodeQuality_SetGateThreshold(const char* gate_name, float min_threshold, float max_threshold);

// Configuration Management
qboolean CodeQuality_LoadConfig(const char* config_file);
qboolean CodeQuality_SaveConfig(const char* config_file);
qboolean CodeQuality_SetDefaults(void);
qboolean CodeQuality_SetStrictMode(qboolean strict);

// Analysis Execution
qboolean CodeQuality_RunAnalysis(code_quality_analysis_t* analysis);
qboolean CodeQuality_RunIncrementalAnalysis(const char* changed_files[],
                                          uint32_t file_count,
                                          code_quality_analysis_t* analysis);
qboolean CodeQuality_CancelAnalysis(void);
qboolean CodeQuality_IsAnalysisRunning(void);

// Coverage Analysis
qboolean CodeQuality_MeasureCoverage(const char* executable_path,
                                   const char* test_command,
                                   coverage_data_t* coverage,
                                   uint32_t max_entries,
                                   uint32_t* num_entries);
qboolean CodeQuality_ParseCoverageData(const char* coverage_file,
                                     coverage_data_t* coverage,
                                     uint32_t max_entries,
                                     uint32_t* num_entries);
float CodeQuality_CalculateOverallCoverage(const coverage_data_t* coverage,
                                         uint32_t count);

// Complexity Analysis
qboolean CodeQuality_AnalyzeComplexity(const char* source_files[],
                                      uint32_t file_count,
                                      complexity_data_t* complexity,
                                      uint32_t max_entries,
                                      uint32_t* num_entries);
int CodeQuality_CalculateCyclomaticComplexity(const char* source_code);
int CodeQuality_CalculateCognitiveComplexity(const char* source_code);
qboolean CodeQuality_CheckComplexityThresholds(const complexity_data_t* complexity,
                                             uint32_t count,
                                             int max_complexity,
                                             uint32_t* violations);

// Quality Metrics Calculation
float CodeQuality_CalculateMaintainabilityIndex(const char* source_code,
                                              int cyclomatic_complexity,
                                              int lines_of_code);
float CodeQuality_CalculateDuplicationPercentage(const char* source_files[],
                                                uint32_t file_count);
float CodeQuality_CalculateStyleScore(const char* source_code);
float CodeQuality_CalculateSecurityScore(const char* source_code);

// Gate Evaluation
qboolean CodeQuality_EvaluateGates(const code_quality_analysis_t* analysis);
qboolean CodeQuality_CheckGate(const quality_gate_config_t* gate,
                             const code_quality_analysis_t* analysis);
qboolean CodeQuality_GetFailedGates(const code_quality_analysis_t* analysis,
                                  quality_gate_config_t* failed_gates,
                                  uint32_t max_gates,
                                  uint32_t* num_failed);

// Reporting and Export
qboolean CodeQuality_GenerateReport(const code_quality_analysis_t* analysis,
                                  const char* output_file,
                                  const char* format);
qboolean CodeQuality_ExportForCI(const code_quality_analysis_t* analysis,
                               const char* output_dir);
qboolean CodeQuality_SaveResults(const char* filename,
                               const code_quality_analysis_t* analysis);
qboolean CodeQuality_LoadResults(const char* filename,
                               code_quality_analysis_t* analysis);

// Utility Functions
const char* CodeQuality_GetResultString(code_quality_result_t result);
const char* CodeQuality_GetMetricString(quality_metric_type_t metric);
qboolean CodeQuality_ValidateConfig(const quality_gate_config_t* config);
qboolean CodeQuality_IsCoverageSufficient(float coverage_percentage,
                                        float minimum_required);
qboolean CodeQuality_IsComplexityAcceptable(int complexity,
                                          int maximum_allowed);

// Built-in Quality Gates
qboolean CodeQuality_AddDefaultGates(void);
qboolean CodeQuality_AddCoverageGate(float min_coverage);
qboolean CodeQuality_AddComplexityGate(int max_complexity);
qboolean CodeQuality_AddMaintainabilityGate(float min_index);
qboolean CodeQuality_AddDuplicationGate(float max_duplication);

// CI/CD Integration Helpers
qboolean CodeQuality_CheckCIGates(const code_quality_analysis_t* analysis);
qboolean CodeQuality_GenerateCIBadge(const code_quality_analysis_t* analysis,
                                   const char* badge_file);
qboolean CodeQuality_CompareToBaseline(const code_quality_analysis_t* current,
                                     const code_quality_analysis_t* baseline,
                                     char* comparison_report,
                                     size_t report_size);

#endif // __CODE_QUALITY_H__
