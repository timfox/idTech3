/*
=============================================================================
Automated Code Review System

AI-assisted code review with style and best practice checks for C/C++ codebases.
=============================================================================
*/

#ifndef __CODE_REVIEW_H__
#define __CODE_REVIEW_H__

#include "q_shared.h"

// Code review severity levels
typedef enum {
    REVIEW_SEVERITY_INFO = 0,      // Informational suggestions
    REVIEW_SEVERITY_WARNING,       // Potential issues that should be addressed
    REVIEW_SEVERITY_ERROR,         // Serious issues that must be fixed
    REVIEW_SEVERITY_CRITICAL,      // Critical issues affecting functionality/safety
    REVIEW_SEVERITY_MAX
} review_severity_t;

// Code review categories
typedef enum {
    REVIEW_CATEGORY_STYLE = 0,     // Code style and formatting
    REVIEW_CATEGORY_BEST_PRACTICE, // Best practices and conventions
    REVIEW_CATEGORY_PERFORMANCE,   // Performance-related issues
    REVIEW_CATEGORY_SECURITY,      // Security vulnerabilities
    REVIEW_CATEGORY_MAINTAINABILITY, // Code maintainability
    REVIEW_CATEGORY_BUGS,          // Potential bugs or logic errors
    REVIEW_CATEGORY_MEMORY,        // Memory management issues
    REVIEW_CATEGORY_THREADING,     // Threading and concurrency issues
    REVIEW_CATEGORY_MAX
} review_category_t;

// Individual code review finding
typedef struct {
    char file[MAX_OSPATH];         // Source file path
    int line;                      // Line number (1-based)
    int column;                    // Column number (1-based)
    review_severity_t severity;    // Severity level
    review_category_t category;    // Issue category
    char rule[MAX_QPATH];          // Rule/check name
    char message[1024];            // Description of the issue
    char suggestion[1024];         // Suggested fix or improvement
    char code_snippet[512];        // Relevant code snippet
    uint64_t timestamp;            // When the finding was detected
} code_review_finding_t;

// Code review configuration
typedef struct {
    qboolean enabled;              // Master enable/disable
    review_severity_t min_severity; // Minimum severity to report
    qboolean enable_categories[REVIEW_CATEGORY_MAX]; // Enabled categories
    qboolean treat_warnings_as_errors; // Treat warnings as build errors

    // Style configuration
    qboolean check_naming_conventions;
    qboolean check_indentation;
    qboolean check_line_length;
    int max_line_length;

    // Performance configuration
    qboolean check_memory_allocations;
    qboolean check_function_complexity;
    int max_function_complexity;

    // Security configuration
    qboolean check_buffer_overflows;
    qboolean check_format_strings;
    qboolean check_null_pointers;

    // Custom rules
    char* custom_rules_file;       // Path to custom rules file
} code_review_config_t;

// Code review statistics
typedef struct {
    uint32_t total_findings;
    uint32_t findings_by_severity[REVIEW_SEVERITY_MAX];
    uint32_t findings_by_category[REVIEW_CATEGORY_MAX];
    uint32_t files_analyzed;
    uint32_t functions_analyzed;
    uint64_t analysis_time_ms;
    float average_findings_per_file;
} code_review_stats_t;

// Code review system
typedef struct {
    qboolean initialized;
    code_review_config_t config;
    code_review_stats_t stats;

    // Findings storage
    code_review_finding_t* findings;
    uint32_t max_findings;
    uint32_t num_findings;

    // File processing
    char** include_paths;
    uint32_t num_include_paths;
    char** exclude_patterns;
    uint32_t num_exclude_patterns;
} code_review_system_t;

extern code_review_system_t code_review_system;

// Code Review API
qboolean CodeReview_Init(void);
void CodeReview_Shutdown(void);

// Configuration
void CodeReview_SetConfig(const code_review_config_t* config);
void CodeReview_GetConfig(code_review_config_t* config);
void CodeReview_LoadConfig(const char* config_file);
void CodeReview_SaveConfig(const char* config_file);

// Analysis
qboolean CodeReview_AnalyzeFile(const char* filename);
qboolean CodeReview_AnalyzeDirectory(const char* directory);
qboolean CodeReview_AnalyzeProject(const char* project_root);

// Findings management
uint32_t CodeReview_GetNumFindings(void);
const code_review_finding_t* CodeReview_GetFinding(uint32_t index);
void CodeReview_ClearFindings(void);
qboolean CodeReview_SaveFindings(const char* output_file, const char* format);

// Filtering
void CodeReview_FilterBySeverity(review_severity_t min_severity);
void CodeReview_FilterByCategory(review_category_t category, qboolean enable);
void CodeReview_FilterByFile(const char* filename);

// Statistics
void CodeReview_GetStats(code_review_stats_t* stats);
void CodeReview_PrintSummary(void);

// Individual checks (called by analyzer)
void CodeReview_CheckStyle(const char* filename, const char* content, int line_count);
void CodeReview_CheckBestPractices(const char* filename, const char* content, int line_count);
void CodeReview_CheckPerformance(const char* filename, const char* content, int line_count);
void CodeReview_CheckSecurity(const char* filename, const char* content, int line_count);
void CodeReview_CheckBugs(const char* filename, const char* content, int line_count);
void CodeReview_CheckMemory(const char* filename, const char* content, int line_count);
void CodeReview_CheckThreading(const char* filename, const char* content, int line_count);

// Utility functions
void CodeReview_AddFinding(const char* file, int line, int column,
                          review_severity_t severity, review_category_t category,
                          const char* rule, const char* message,
                          const char* suggestion, const char* code_snippet);

// File processing utilities
char* CodeReview_ReadFile(const char* filename, int* line_count);
qboolean CodeReview_IsSourceFile(const char* filename);
qboolean CodeReview_ShouldAnalyzeFile(const char* filename);

#endif // __CODE_REVIEW_H__
