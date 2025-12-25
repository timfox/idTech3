/*
=============================================================================
Technical Debt Tracking Framework

Automated monitoring of code quality metrics and technical debt management.
=============================================================================
*/

#ifndef __TECHNICAL_DEBT_H__
#define __TECHNICAL_DEBT_H__

#include "q_shared.h"

// Technical debt severity levels
typedef enum {
    DEBT_SEVERITY_LOW = 0,      // Minor issues, low priority
    DEBT_SEVERITY_MEDIUM,       // Moderate issues, should address
    DEBT_SEVERITY_HIGH,         // Significant issues, high priority
    DEBT_SEVERITY_CRITICAL,     // Critical issues, immediate action
    DEBT_SEVERITY_COUNT
} debt_severity_t;

// Technical debt categories
typedef enum {
    DEBT_CATEGORY_CODE_QUALITY = 0,    // Code quality issues
    DEBT_CATEGORY_COMPLEXITY,          // High complexity
    DEBT_CATEGORY_COVERAGE,            // Low test coverage
    DEBT_CATEGORY_MAINTAINABILITY,     // Maintainability issues
    DEBT_CATEGORY_DUPLICATION,         // Code duplication
    DEBT_CATEGORY_SECURITY,            // Security vulnerabilities
    DEBT_CATEGORY_PERFORMANCE,         // Performance issues
    DEBT_CATEGORY_ARCHITECTURE,        // Architecture problems
    DEBT_CATEGORY_DOCUMENTATION,       // Documentation debt
    DEBT_CATEGORY_COUNT
} debt_category_t;

// Technical debt item
typedef struct {
    char item_id[64];              // Unique identifier
    char title[128];               // Human-readable title
    char description[512];         // Detailed description
    debt_category_t category;      // Debt category
    debt_severity_t severity;      // Severity level
    char file_path[256];           // File where debt exists
    char function_name[128];       // Function name (if applicable)
    int line_number;               // Line number (if applicable)
    uint64_t introduced_date;      // When debt was introduced
    uint64_t last_updated;         // Last modification date
    char introduced_by[64];        // Who introduced the debt
    char assigned_to[64];          // Who is responsible for fixing
    qboolean resolved;             // Whether debt has been resolved
    uint64_t resolved_date;        // When debt was resolved
    char resolution_notes[256];    // Notes about resolution
    float estimated_effort;        // Estimated hours to fix
    float actual_effort;           // Actual hours spent fixing
    int priority_score;            // Calculated priority score
    char tags[256];                // Comma-separated tags
} debt_item_t;

// Technical debt metrics
typedef struct {
    float total_debt_score;        // Overall debt score
    int total_debt_items;          // Total number of debt items
    int unresolved_items;          // Number of unresolved items
    int critical_items;            // Number of critical items
    int high_priority_items;       // Number of high priority items

    // Category breakdown
    int category_counts[DEBT_CATEGORY_COUNT];
    float category_scores[DEBT_CATEGORY_COUNT];

    // Severity breakdown
    int severity_counts[DEBT_SEVERITY_COUNT];

    // Trend data
    float debt_velocity;           // Rate of debt accumulation
    float debt_paydown_rate;       // Rate of debt resolution
    float projected_completion;    // Estimated completion time (months)

    // Quality metrics integration
    float code_coverage;           // Current code coverage
    float avg_complexity;          // Average complexity
    float maintainability_index;   // Current maintainability
    float duplication_percentage;  // Current duplication

    // Effort tracking
    float total_estimated_effort;  // Total estimated effort (hours)
    float total_actual_effort;     // Total actual effort spent
    float effort_efficiency;       // Actual vs estimated efficiency
} debt_metrics_t;

// Historical debt data point
typedef struct {
    uint64_t timestamp;            // When measurement was taken
    debt_metrics_t metrics;        // Metrics at this point
    int new_debt_items;            // New debt items since last measurement
    int resolved_debt_items;       // Resolved debt items since last measurement
    char commit_hash[64];          // Git commit hash
    char branch_name[64];          // Git branch name
} debt_history_point_t;

// Technical debt system
typedef struct {
    char system_name[64];
    char description[256];

    // Debt items storage
    debt_item_t* debt_items;
    uint32_t max_debt_items;
    uint32_t num_debt_items;

    // Historical data
    debt_history_point_t* history;
    uint32_t max_history_points;
    uint32_t num_history_points;

    // Current metrics
    debt_metrics_t current_metrics;

    // Configuration
    qboolean auto_tracking_enabled;
    qboolean alerts_enabled;
    int alert_threshold_critical;
    int alert_threshold_high;
    float debt_velocity_threshold;
    int history_retention_days;

    // System state
    qboolean initialized;
    uint64_t last_analysis_time;
    uint64_t next_scheduled_analysis;

    // Integration settings
    qboolean integrate_with_quality_analysis;
    qboolean integrate_with_ci_cd;
    qboolean generate_reports;
} technical_debt_system_t;

extern technical_debt_system_t technical_debt_system;

// Technical Debt Tracking API
qboolean TechnicalDebt_Init(void);
void TechnicalDebt_Shutdown(void);

// Debt Item Management
qboolean TechnicalDebt_AddItem(const debt_item_t* item);
qboolean TechnicalDebt_UpdateItem(const char* item_id, const debt_item_t* updates);
qboolean TechnicalDebt_RemoveItem(const char* item_id);
qboolean TechnicalDebt_ResolveItem(const char* item_id, const char* resolution_notes);
qboolean TechnicalDebt_AssignItem(const char* item_id, const char* assignee);

// Debt Item Queries
uint32_t TechnicalDebt_GetItems(debt_item_t** items);
debt_item_t* TechnicalDebt_FindItem(const char* item_id);
uint32_t TechnicalDebt_GetItemsByCategory(debt_category_t category, debt_item_t** items);
uint32_t TechnicalDebt_GetItemsBySeverity(debt_severity_t severity, debt_item_t** items);
uint32_t TechnicalDebt_GetItemsByAssignee(const char* assignee, debt_item_t** items);

// Metrics and Analysis
qboolean TechnicalDebt_CalculateMetrics(debt_metrics_t* metrics);
qboolean TechnicalDebt_AnalyzeTrends(debt_metrics_t* trends, uint32_t days_lookback);
qboolean TechnicalDebt_PredictFutureDebt(float* projected_score, uint32_t months_ahead);

// Historical Tracking
qboolean TechnicalDebt_RecordHistoryPoint(const char* commit_hash, const char* branch);
uint32_t TechnicalDebt_GetHistory(debt_history_point_t** history);
qboolean TechnicalDebt_CompareToBaseline(const debt_metrics_t* current,
                                       const debt_metrics_t* baseline,
                                       char* comparison_report,
                                       size_t report_size);

// Automated Monitoring
qboolean TechnicalDebt_EnableAutoTracking(qboolean enable);
qboolean TechnicalDebt_SetAlertThresholds(int critical_threshold, int high_threshold);
qboolean TechnicalDebt_CheckAlerts(char* alert_message, size_t message_size);
qboolean TechnicalDebt_RunAutomatedAnalysis(void);

// Reporting and Export
qboolean TechnicalDebt_GenerateReport(const char* output_file, const char* format);
qboolean TechnicalDebt_GenerateDashboardData(const char* output_file);
qboolean TechnicalDebt_ExportForCI(const char* output_dir);
qboolean TechnicalDebt_SaveState(const char* filename);
qboolean TechnicalDebt_LoadState(const char* filename);

// Configuration
qboolean TechnicalDebt_LoadConfig(const char* config_file);
qboolean TechnicalDebt_SaveConfig(const char* config_file);
qboolean TechnicalDebt_SetRetentionPolicy(int days);
qboolean TechnicalDebt_EnableIntegrations(qboolean quality_analysis,
                                        qboolean ci_cd,
                                        qboolean reports);

// Utility Functions
const char* TechnicalDebt_GetSeverityString(debt_severity_t severity);
const char* TechnicalDebt_GetCategoryString(debt_category_t category);
qboolean TechnicalDebt_ValidateItem(const debt_item_t* item);
float TechnicalDebt_CalculatePriorityScore(const debt_item_t* item);
qboolean TechnicalDebt_IsTrendingUp(float current_value, float previous_value, float threshold);

// Debt Item Templates
qboolean TechnicalDebt_AddComplexityDebt(const char* file, const char* function,
                                       int complexity, int line);
qboolean TechnicalDebt_AddCoverageDebt(const char* file, float coverage);
qboolean TechnicalDebt_AddDuplicationDebt(const char* file1, const char* file2,
                                        int duplicated_lines);
qboolean TechnicalDebt_AddSecurityDebt(const char* file, const char* vulnerability,
                                     debt_severity_t severity);

// CI/CD Integration
qboolean TechnicalDebt_CheckCIDebtLimits(const debt_metrics_t* metrics);
qboolean TechnicalDebt_GenerateCIBadge(const debt_metrics_t* metrics,
                                     const char* badge_file);
qboolean TechnicalDebt_GetDebtHealthStatus(const debt_metrics_t* metrics,
                                         char* status, size_t status_size);

#endif // __TECHNICAL_DEBT_H__
