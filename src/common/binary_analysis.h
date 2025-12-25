/*
=============================================================================
Binary Analysis System

Automated security scanning and optimization analysis framework.
=============================================================================
*/

#ifndef __BINARY_ANALYSIS_H__
#define __BINARY_ANALYSIS_H__

#include "q_shared.h"

// Analysis result types
typedef enum {
    ANALYSIS_PASS = 0,          // Analysis passed
    ANALYSIS_WARNING,           // Analysis found warnings
    ANALYSIS_FAIL,              // Analysis failed
    ANALYSIS_ERROR              // Analysis encountered an error
} analysis_result_t;

// Analysis categories
typedef enum {
    ANALYSIS_SECURITY = 0,      // Security vulnerability scanning
    ANALYSIS_OPTIMIZATION,      // Performance and size optimization
    ANALYSIS_DEPENDENCY,        // Dependency and linking analysis
    ANALYSIS_PERFORMANCE,       // Runtime performance analysis
    ANALYSIS_COMPATIBILITY,     // Platform compatibility analysis
    ANALYSIS_COUNT
} analysis_category_t;

// Security vulnerability types
typedef enum {
    VULN_BUFFER_OVERFLOW = 0,   // Stack/heap buffer overflows
    VULN_FORMAT_STRING,         // Format string vulnerabilities
    VULN_INTEGER_OVERFLOW,      // Integer overflow/underflow
    VULN_USE_AFTER_FREE,        // Use-after-free vulnerabilities
    VULN_DOUBLE_FREE,           // Double-free vulnerabilities
    VULN_NULL_POINTER,          // Null pointer dereferences
    VULN_UNINITIALIZED_MEMORY,  // Use of uninitialized memory
    VULN_RACE_CONDITION,        // Race conditions
    VULN_INSECURE_FUNCTIONS,    // Use of insecure functions
    VULN_COUNT
} vulnerability_type_t;

// Optimization issue types
typedef enum {
    OPT_LARGE_BINARY_SIZE = 0,  // Binary size optimization
    OPT_UNUSED_CODE,            // Dead code elimination
    OPT_FUNCTION_SIZE,          // Large function detection
    OPT_BRANCH_PREDICTION,      // Branch prediction optimization
    OPT_MEMORY_ACCESS,          // Memory access pattern optimization
    OPT_INSTRUCTION_COUNT,      // Instruction count optimization
    OPT_REGISTER_USAGE,         // Register allocation optimization
    OPT_CACHE_MISSES,           // Cache miss optimization
    OPT_COUNT
} optimization_type_t;

// Analysis finding
typedef struct {
    char description[512];      // Human-readable description
    char recommendation[512];   // Suggested fix or improvement
    analysis_category_t category; // Analysis category
    int severity_level;         // 1-10 severity scale
    qboolean exploitable;       // Whether this is exploitable
    qboolean auto_fixable;      // Whether this can be auto-fixed
    char file_path[256];        // Source file (if applicable)
    uint32_t line_number;       // Line number (if applicable)
    char function_name[128];    // Function name (if applicable)
    uint64_t address;           // Memory address (if applicable)
    char additional_info[1024]; // Additional technical details
} analysis_finding_t;

// Binary analysis result
typedef struct {
    char binary_path[512];      // Path to analyzed binary
    char analysis_timestamp[64]; // When analysis was performed
    analysis_result_t overall_result; // Overall analysis result

    // Findings tracking
    analysis_finding_t* findings; // Array of findings
    uint32_t finding_count;     // Number of findings
    uint32_t max_findings;      // Maximum findings that can be stored

    // Statistics by category
    uint32_t findings_by_category[ANALYSIS_COUNT];
    uint32_t findings_by_severity[11]; // 0-10 severity levels

    // Security statistics
    uint32_t vulnerabilities_found;
    uint32_t exploitable_vulnerabilities;
    uint32_t critical_vulnerabilities;

    // Performance metrics
    uint64_t binary_size_bytes; // Size of binary
    uint32_t function_count;    // Number of functions
    uint32_t symbol_count;      // Number of symbols
    float average_function_size; // Average function size
    uint32_t large_functions;   // Functions > 1000 instructions

    // Dependency information
    uint32_t shared_library_count;
    uint32_t static_library_count;
    char* dependency_list;      // List of dependencies
    uint32_t dependency_count;

    // Platform information
    char platform[32];
    char architecture[16];
    char compiler[32];
    qboolean stripped;          // Whether symbols are stripped
    qboolean pie_enabled;       // Position independent executable
    qboolean stack_protector;   // Stack protector enabled

    // Analysis metadata
    uint64_t analysis_time_ms;  // Time spent analyzing
    char analyzer_version[32];  // Version of analysis tool
} binary_analysis_result_t;

// Analysis configuration
typedef struct {
    qboolean enable_security_scanning;    // Enable security vulnerability scanning
    qboolean enable_optimization_analysis; // Enable optimization analysis
    qboolean enable_dependency_analysis;   // Enable dependency analysis
    qboolean enable_performance_analysis;  // Enable performance analysis
    qboolean enable_detailed_logging;      // Enable verbose logging

    // Security scanning options
    qboolean check_buffer_overflows;       // Check for buffer overflows
    qboolean check_format_strings;         // Check for format string issues
    qboolean check_integer_overflows;      // Check for integer overflows
    qboolean check_null_pointers;          // Check for null pointer issues
    qboolean check_race_conditions;        // Check for race conditions

    // Optimization analysis options
    uint32_t max_function_size;           // Maximum function size (instructions)
    uint64_t max_binary_size;             // Maximum binary size
    qboolean check_unused_code;           // Check for unused code
    qboolean analyze_branch_prediction;   // Analyze branch prediction
    qboolean analyze_memory_access;       // Analyze memory access patterns

    // Thresholds
    int critical_severity_threshold;      // Minimum severity for critical issues
    int warning_severity_threshold;       // Minimum severity for warnings
    qboolean treat_warnings_as_errors;    // Treat warnings as errors

    // Output options
    char report_directory[256];           // Directory for reports
    qboolean generate_html_report;        // Generate HTML reports
    qboolean generate_json_report;        // Generate JSON reports
    qboolean generate_sarif_report;       // Generate SARIF reports for CI/CD
} analysis_config_t;

// Binary analysis system
typedef struct {
    qboolean initialized;
    analysis_config_t config;

    // Analysis results
    binary_analysis_result_t* results;
    uint32_t result_count;
    uint32_t max_results;

    // Global statistics
    uint32_t total_binaries_analyzed;
    uint32_t total_findings;
    uint32_t total_vulnerabilities;
    uint32_t total_exploitable_issues;

    // Analysis state
    qboolean currently_analyzing;
    char current_binary[512];
    uint64_t analysis_start_time;
} binary_analysis_system_t;

extern binary_analysis_system_t binary_analysis;

// Binary Analysis API
qboolean BinaryAnalysis_Init(void);
void BinaryAnalysis_Shutdown(void);

// Configuration
void BinaryAnalysis_SetConfig(const analysis_config_t* config);
const analysis_config_t* BinaryAnalysis_GetConfig(void);

// Binary Analysis
binary_analysis_result_t* BinaryAnalysis_AnalyzeBinary(const char* binary_path);
qboolean BinaryAnalysis_AnalyzeSecurity(const char* binary_path, binary_analysis_result_t* result);
qboolean BinaryAnalysis_AnalyzeOptimization(const char* binary_path, binary_analysis_result_t* result);
qboolean BinaryAnalysis_AnalyzeDependencies(const char* binary_path, binary_analysis_result_t* result);
qboolean BinaryAnalysis_AnalyzePerformance(const char* binary_path, binary_analysis_result_t* result);

// Batch Analysis
uint32_t BinaryAnalysis_AnalyzeDirectory(const char* directory_path);
qboolean BinaryAnalysis_AnalyzeAllBinaries(void);

// Result Management
uint32_t BinaryAnalysis_GetResults(binary_analysis_result_t** results);
binary_analysis_result_t* BinaryAnalysis_GetResult(const char* binary_path);
qboolean BinaryAnalysis_SaveResults(const char* filename);
qboolean BinaryAnalysis_LoadResults(const char* filename);
void BinaryAnalysis_ClearResults(void);

// Finding Management
qboolean BinaryAnalysis_AddFinding(binary_analysis_result_t* result,
                                 const char* description,
                                 const char* recommendation,
                                 analysis_category_t category,
                                 int severity_level,
                                 qboolean exploitable,
                                 qboolean auto_fixable,
                                 const char* file_path,
                                 uint32_t line_number,
                                 const char* function_name,
                                 uint64_t address,
                                 const char* additional_info);

// Reporting
qboolean BinaryAnalysis_GenerateReport(const char* output_file, const char* format);
qboolean BinaryAnalysis_GenerateSecurityReport(const char* output_file);
qboolean BinaryAnalysis_GenerateOptimizationReport(const char* output_file);
qboolean BinaryAnalysis_GenerateHTMLReport(const char* output_file);
qboolean BinaryAnalysis_GenerateSARIFReport(const char* output_file);

// Statistics and Metrics
void BinaryAnalysis_PrintStatistics(void);
void BinaryAnalysis_PrintSecuritySummary(void);
void BinaryAnalysis_PrintOptimizationSummary(void);
uint32_t BinaryAnalysis_GetVulnerabilityCount(void);
uint32_t BinaryAnalysis_GetCriticalIssueCount(void);

// Utility Functions
const char* BinaryAnalysis_GetResultString(analysis_result_t result);
const char* BinaryAnalysis_GetCategoryString(analysis_category_t category);
const char* BinaryAnalysis_GetVulnerabilityString(vulnerability_type_t vuln);
const char* BinaryAnalysis_GetOptimizationString(optimization_type_t opt);
qboolean BinaryAnalysis_IsBinaryFile(const char* file_path);
qboolean BinaryAnalysis_GetBinaryInfo(const char* binary_path, binary_analysis_result_t* info);

// CI/CD Integration
qboolean BinaryAnalysis_CheckCISecurityGates(void);
qboolean BinaryAnalysis_GenerateCIBadges(const char* output_dir);
qboolean BinaryAnalysis_GetSecurityStatus(char* status, size_t status_size);
qboolean BinaryAnalysis_GetOptimizationStatus(char* status, size_t status_size);

// Auto-fix capabilities
uint32_t BinaryAnalysis_AutoFixIssues(binary_analysis_result_t* result);
qboolean BinaryAnalysis_CanAutoFix(binary_analysis_result_t* result);

// Performance profiling integration
qboolean BinaryAnalysis_AnalyzeProfileData(const char* profile_file, binary_analysis_result_t* result);
qboolean BinaryAnalysis_GenerateOptimizationHints(const char* binary_path, const char* output_file);

// Console Commands
void BinaryAnalysis_Status_f(void);
void BinaryAnalysis_Analyze_f(void);
void BinaryAnalysis_BatchAnalyze_f(void);
void BinaryAnalysis_Report_f(void);
void BinaryAnalysis_Stats_f(void);
void BinaryAnalysis_AutoFix_f(void);

#endif // __BINARY_ANALYSIS_H__
