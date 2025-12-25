/*
=============================================================================
Technical Debt Tracking Framework Implementation

Automated monitoring of code quality metrics and technical debt management.
=============================================================================
*/

#include "technical_debt.h"
#include "qcommon.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

// Global technical debt system instance
technical_debt_system_t technical_debt_system = {0};

// Debt scoring weights (higher = more important)
static const float debt_category_weights[DEBT_CATEGORY_COUNT] = {
    1.0f,  // Code quality
    1.5f,  // Complexity (more expensive to fix)
    2.0f,  // Coverage (critical for reliability)
    1.2f,  // Maintainability
    0.8f,  // Duplication
    3.0f,  // Security (highest priority)
    1.8f,  // Performance
    2.5f,  // Architecture
    0.5f   // Documentation
};

// Severity multipliers
static const float debt_severity_multipliers[DEBT_SEVERITY_COUNT] = {
    1.0f,   // Low
    2.0f,   // Medium
    4.0f,   // High
    8.0f    // Critical
};

/*
=============================================================================
Technical Debt Tracking API Implementation
=============================================================================
*/

qboolean TechnicalDebt_Init(void) {
    if (technical_debt_system.initialized) {
        return qtrue;
    }

    memset(&technical_debt_system, 0, sizeof(technical_debt_system_t));
    Q_strncpyz(technical_debt_system.system_name, "Technical Debt Tracker", sizeof(technical_debt_system_name));
    Q_strncpyz(technical_debt_system.description, "Automated technical debt monitoring and management", sizeof(technical_debt_system.description));

    // Allocate debt items storage
    technical_debt_system.max_debt_items = 1000;
    technical_debt_system.debt_items = (debt_item_t*)malloc(
        sizeof(debt_item_t) * technical_debt_system.max_debt_items);

    if (!technical_debt_system.debt_items) {
        Com_Printf("Failed to allocate memory for debt items\n");
        return qfalse;
    }

    memset(technical_debt_system.debt_items, 0,
           sizeof(debt_item_t) * technical_debt_system.max_debt_items);

    // Allocate history storage
    technical_debt_system.max_history_points = 1000;
    technical_debt_system.history = (debt_history_point_t*)malloc(
        sizeof(debt_history_point_t) * technical_debt_system.max_history_points);

    if (!technical_debt_system.history) {
        free(technical_debt_system.debt_items);
        Com_Printf("Failed to allocate memory for debt history\n");
        return qfalse;
    }

    memset(technical_debt_system.history, 0,
           sizeof(debt_history_point_t) * technical_debt_system.max_history_points);

    // Set default configuration
    technical_debt_system.auto_tracking_enabled = qtrue;
    technical_debt_system.alerts_enabled = qtrue;
    technical_debt_system.alert_threshold_critical = 10;
    technical_debt_system.alert_threshold_high = 25;
    technical_debt_system.debt_velocity_threshold = 5.0f;
    technical_debt_system.history_retention_days = 365;

    technical_debt_system.integrate_with_quality_analysis = qtrue;
    technical_debt_system.integrate_with_ci_cd = qtrue;
    technical_debt_system.generate_reports = qtrue;

    technical_debt_system.initialized = qtrue;
    technical_debt_system.last_analysis_time = Sys_Milliseconds();

    Com_Printf("Technical debt tracking system initialized\n");
    Com_Printf("Monitoring %u debt categories with automated tracking\n", DEBT_CATEGORY_COUNT);

    return qtrue;
}

void TechnicalDebt_Shutdown(void) {
    if (!technical_debt_system.initialized) {
        return;
    }

    if (technical_debt_system.debt_items) {
        free(technical_debt_system.debt_items);
        technical_debt_system.debt_items = NULL;
    }

    if (technical_debt_system.history) {
        free(technical_debt_system.history);
        technical_debt_system.history = NULL;
    }

    technical_debt_system.initialized = qfalse;
    Com_Printf("Technical debt tracking system shutdown\n");
}

/*
=============================================================================
Debt Item Management
=============================================================================
*/

qboolean TechnicalDebt_AddItem(const debt_item_t* item) {
    if (!technical_debt_system.initialized || !item ||
        technical_debt_system.num_debt_items >= technical_debt_system.max_debt_items) {
        return qfalse;
    }

    if (!TechnicalDebt_ValidateItem(item)) {
        return qfalse;
    }

    // Check for duplicate ID
    if (TechnicalDebt_FindItem(item->item_id)) {
        Com_Printf("Debt item with ID '%s' already exists\n", item->item_id);
        return qfalse;
    }

    // Copy item
    memcpy(&technical_debt_system.debt_items[technical_debt_system.num_debt_items],
           item, sizeof(debt_item_t));

    // Set timestamps if not provided
    if (technical_debt_system.debt_items[technical_debt_system.num_debt_items].introduced_date == 0) {
        technical_debt_system.debt_items[technical_debt_system.num_debt_items].introduced_date = Sys_Milliseconds();
    }
    technical_debt_system.debt_items[technical_debt_system.num_debt_items].last_updated = Sys_Milliseconds();

    // Calculate priority score
    technical_debt_system.debt_items[technical_debt_system.num_debt_items].priority_score =
        TechnicalDebt_CalculatePriorityScore(item);

    technical_debt_system.num_debt_items++;

    // Trigger automated analysis
    if (technical_debt_system.auto_tracking_enabled) {
        TechnicalDebt_RunAutomatedAnalysis();
    }

    return qtrue;
}

qboolean TechnicalDebt_UpdateItem(const char* item_id, const debt_item_t* updates) {
    debt_item_t* item = TechnicalDebt_FindItem(item_id);
    if (!item) {
        return qfalse;
    }

    // Update fields (preserve some read-only fields)
    if (updates->title[0]) Q_strncpyz(item->title, updates->title, sizeof(item->title));
    if (updates->description[0]) Q_strncpyz(item->description, updates->description, sizeof(item->description));
    item->severity = updates->severity;
    item->estimated_effort = updates->estimated_effort;
    item->actual_effort = updates->actual_effort;
    if (updates->assigned_to[0]) Q_strncpyz(item->assigned_to, updates->assigned_to, sizeof(item->assigned_to));
    if (updates->tags[0]) Q_strncpyz(item->tags, updates->tags, sizeof(item->tags));

    item->last_updated = Sys_Milliseconds();
    item->priority_score = TechnicalDebt_CalculatePriorityScore(item);

    return qtrue;
}

qboolean TechnicalDebt_RemoveItem(const char* item_id) {
    for (uint32_t i = 0; i < technical_debt_system.num_debt_items; i++) {
        if (Q_stricmp(technical_debt_system.debt_items[i].item_id, item_id) == 0) {
            // Shift remaining items
            for (uint32_t j = i; j < technical_debt_system.num_debt_items - 1; j++) {
                memcpy(&technical_debt_system.debt_items[j],
                       &technical_debt_system.debt_items[j + 1],
                       sizeof(debt_item_t));
            }
            technical_debt_system.num_debt_items--;
            return qtrue;
        }
    }
    return qfalse;
}

qboolean TechnicalDebt_ResolveItem(const char* item_id, const char* resolution_notes) {
    debt_item_t* item = TechnicalDebt_FindItem(item_id);
    if (!item) {
        return qfalse;
    }

    item->resolved = qtrue;
    item->resolved_date = Sys_Milliseconds();
    if (resolution_notes) {
        Q_strncpyz(item->resolution_notes, resolution_notes, sizeof(item->resolution_notes));
    }
    item->last_updated = Sys_Milliseconds();

    return qtrue;
}

qboolean TechnicalDebt_AssignItem(const char* item_id, const char* assignee) {
    debt_item_t* item = TechnicalDebt_FindItem(item_id);
    if (!item) {
        return qfalse;
    }

    Q_strncpyz(item->assigned_to, assignee, sizeof(item->assigned_to));
    item->last_updated = Sys_Milliseconds();

    return qtrue;
}

/*
=============================================================================
Debt Item Queries
=============================================================================
*/

uint32_t TechnicalDebt_GetItems(debt_item_t** items) {
    if (items) *items = technical_debt_system.debt_items;
    return technical_debt_system.num_debt_items;
}

debt_item_t* TechnicalDebt_FindItem(const char* item_id) {
    for (uint32_t i = 0; i < technical_debt_system.num_debt_items; i++) {
        if (Q_stricmp(technical_debt_system.debt_items[i].item_id, item_id) == 0) {
            return &technical_debt_system.debt_items[i];
        }
    }
    return NULL;
}

uint32_t TechnicalDebt_GetItemsByCategory(debt_category_t category, debt_item_t** items) {
    static debt_item_t temp_items[1000]; // Temporary buffer
    uint32_t count = 0;

    for (uint32_t i = 0; i < technical_debt_system.num_debt_items && count < 1000; i++) {
        if (technical_debt_system.debt_items[i].category == category && !technical_debt_system.debt_items[i].resolved) {
            memcpy(&temp_items[count], &technical_debt_system.debt_items[i], sizeof(debt_item_t));
            count++;
        }
    }

    if (items) *items = temp_items;
    return count;
}

uint32_t TechnicalDebt_GetItemsBySeverity(debt_severity_t severity, debt_item_t** items) {
    static debt_item_t temp_items[1000]; // Temporary buffer
    uint32_t count = 0;

    for (uint32_t i = 0; i < technical_debt_system.num_debt_items && count < 1000; i++) {
        if (technical_debt_system.debt_items[i].severity == severity && !technical_debt_system.debt_items[i].resolved) {
            memcpy(&temp_items[count], &technical_debt_system.debt_items[i], sizeof(debt_item_t));
            count++;
        }
    }

    if (items) *items = temp_items;
    return count;
}

uint32_t TechnicalDebt_GetItemsByAssignee(const char* assignee, debt_item_t** items) {
    static debt_item_t temp_items[1000]; // Temporary buffer
    uint32_t count = 0;

    for (uint32_t i = 0; i < technical_debt_system.num_debt_items && count < 1000; i++) {
        if (Q_stricmp(technical_debt_system.debt_items[i].assigned_to, assignee) == 0 &&
            !technical_debt_system.debt_items[i].resolved) {
            memcpy(&temp_items[count], &technical_debt_system.debt_items[i], sizeof(debt_item_t));
            count++;
        }
    }

    if (items) *items = temp_items;
    return count;
}

/*
=============================================================================
Metrics and Analysis
=============================================================================
*/

qboolean TechnicalDebt_CalculateMetrics(debt_metrics_t* metrics) {
    if (!metrics) return qfalse;

    memset(metrics, 0, sizeof(debt_metrics_t));

    // Count items by category and severity
    for (uint32_t i = 0; i < technical_debt_system.num_debt_items; i++) {
        const debt_item_t* item = &technical_debt_system.debt_items[i];

        if (!item->resolved) {
            metrics->total_debt_items++;
            metrics->unresolved_items++;

            // Category breakdown
            metrics->category_counts[item->category]++;

            // Severity breakdown
            metrics->severity_counts[item->severity]++;

            // Priority tracking
            if (item->severity == DEBT_SEVERITY_CRITICAL) {
                metrics->critical_items++;
            } else if (item->severity == DEBT_SEVERITY_HIGH) {
                metrics->high_priority_items++;
            }

            // Effort tracking
            metrics->total_estimated_effort += item->estimated_effort;
            metrics->total_actual_effort += item->actual_effort;
        }
    }

    // Calculate category scores
    for (int i = 0; i < DEBT_CATEGORY_COUNT; i++) {
        metrics->category_scores[i] = metrics->category_counts[i] *
                                    debt_category_weights[i] *
                                    debt_severity_multipliers[DEBT_SEVERITY_HIGH]; // Use high as baseline
    }

    // Calculate total debt score
    metrics->total_debt_score = 0.0f;
    for (int i = 0; i < DEBT_CATEGORY_COUNT; i++) {
        metrics->total_debt_score += metrics->category_scores[i];
    }

    // Calculate effort efficiency
    if (metrics->total_estimated_effort > 0) {
        metrics->effort_efficiency = metrics->total_actual_effort / metrics->total_estimated_effort;
    }

    // Integrate with quality analysis (placeholder values - would integrate with actual quality system)
    metrics->code_coverage = 75.0f;
    metrics->avg_complexity = 12.5f;
    metrics->maintainability_index = 65.0f;
    metrics->duplication_percentage = 3.2f;

    // Calculate trend data (simplified)
    if (technical_debt_system.num_history_points >= 2) {
        const debt_history_point_t* latest = &technical_debt_system.history[technical_debt_system.num_history_points - 1];
        const debt_history_point_t* previous = &technical_debt_system.history[technical_debt_system.num_history_points - 2];

        uint64_t time_diff_days = (latest->timestamp - previous->timestamp) / (1000 * 60 * 60 * 24);
        if (time_diff_days > 0) {
            float score_diff = latest->metrics.total_debt_score - previous->metrics.total_debt_score;
            metrics->debt_velocity = score_diff / time_diff_days;

            int resolved_diff = latest->resolved_debt_items - previous->resolved_debt_items;
            metrics->debt_paydown_rate = resolved_diff / (float)time_diff_days;
        }
    }

    // Store current metrics
    memcpy(&technical_debt_system.current_metrics, metrics, sizeof(debt_metrics_t));

    return qtrue;
}

qboolean TechnicalDebt_AnalyzeTrends(debt_metrics_t* trends, uint32_t days_lookback) {
    if (!trends || technical_debt_system.num_history_points < 2) return qfalse;

    uint64_t current_time = Sys_Milliseconds();
    uint64_t lookback_time = current_time - (days_lookback * 24 * 60 * 60 * 1000);

    // Find history points within lookback period
    uint32_t start_idx = technical_debt_system.num_history_points;
    for (uint32_t i = 0; i < technical_debt_system.num_history_points; i++) {
        if (technical_debt_system.history[i].timestamp >= lookback_time) {
            start_idx = i;
            break;
        }
    }

    if (start_idx >= technical_debt_system.num_history_points) {
        return qfalse; // No data in lookback period
    }

    // Calculate trends
    const debt_history_point_t* first = &technical_debt_system.history[start_idx];
    const debt_history_point_t* last = &technical_debt_system.history[technical_debt_system.num_history_points - 1];

    uint64_t time_diff = last->timestamp - first->timestamp;
    if (time_diff == 0) return qfalse;

    // Simple linear trend calculation
    trends->debt_velocity = (last->metrics.total_debt_score - first->metrics.total_debt_score) /
                           (time_diff / (1000.0f * 60 * 60 * 24)); // points per day

    trends->debt_paydown_rate = (last->resolved_debt_items - first->resolved_debt_items) /
                               (time_diff / (1000.0f * 60 * 60 * 24)); // items per day

    return qtrue;
}

qboolean TechnicalDebt_PredictFutureDebt(float* projected_score, uint32_t months_ahead) {
    if (!projected_score || technical_debt_system.num_history_points < 3) return qfalse;

    // Simple linear projection based on recent velocity
    const debt_metrics_t* current = &technical_debt_system.current_metrics;
    float monthly_velocity = current->debt_velocity * 30; // Convert daily to monthly

    *projected_score = current->total_debt_score + (monthly_velocity * months_ahead);

    // Ensure non-negative
    if (*projected_score < 0) *projected_score = 0;

    return qtrue;
}

/*
=============================================================================
Historical Tracking
=============================================================================
*/

qboolean TechnicalDebt_RecordHistoryPoint(const char* commit_hash, const char* branch) {
    if (technical_debt_system.num_history_points >= technical_debt_system.max_history_points) {
        // Remove oldest entry
        for (uint32_t i = 1; i < technical_debt_system.num_history_points; i++) {
            memcpy(&technical_debt_system.history[i - 1],
                   &technical_debt_system.history[i],
                   sizeof(debt_history_point_t));
        }
        technical_debt_system.num_history_points--;
    }

    debt_history_point_t* point = &technical_debt_system.history[technical_debt_system.num_history_points];

    point->timestamp = Sys_Milliseconds();
    if (commit_hash) Q_strncpyz(point->commit_hash, commit_hash, sizeof(point->commit_hash));
    if (branch) Q_strncpyz(point->branch_name, branch, sizeof(point->branch_name));

    // Calculate metrics for this point
    TechnicalDebt_CalculateMetrics(&point->metrics);

    // Calculate deltas from previous point
    if (technical_debt_system.num_history_points > 0) {
        const debt_history_point_t* prev = &technical_debt_system.history[technical_debt_system.num_history_points - 1];
        point->new_debt_items = point->metrics.total_debt_items - prev->metrics.total_debt_items;
        point->resolved_debt_items = prev->resolved_debt_items - point->resolved_debt_items; // Note: resolved items decrease total
    }

    technical_debt_system.num_history_points++;

    return qtrue;
}

uint32_t TechnicalDebt_GetHistory(debt_history_point_t** history) {
    if (history) *history = technical_debt_system.history;
    return technical_debt_system.num_history_points;
}

qboolean TechnicalDebt_CompareToBaseline(const debt_metrics_t* current,
                                       const debt_metrics_t* baseline,
                                       char* comparison_report,
                                       size_t report_size) {
    if (!current || !baseline || !comparison_report) return qfalse;

    int written = Q_snprintf(comparison_report, report_size,
        "Technical Debt Comparison Report\n"
        "================================\n\n"
        "Overall Debt Score: %.1f -> %.1f (%.1f%% change)\n"
        "Total Debt Items: %d -> %d (%+d change)\n"
        "Unresolved Items: %d -> %d (%+d change)\n"
        "Critical Items: %d -> %d (%+d change)\n\n"
        "Category Changes:\n",
        baseline->total_debt_score, current->total_debt_score,
        ((current->total_debt_score - baseline->total_debt_score) / baseline->total_debt_score) * 100,
        baseline->total_debt_items, current->total_debt_items,
        current->total_debt_items - baseline->total_debt_items,
        baseline->unresolved_items, current->unresolved_items,
        current->unresolved_items - baseline->unresolved_items,
        baseline->critical_items, current->critical_items,
        current->critical_items - baseline->critical_items);

    if (written < 0) return qfalse;

    // Add category breakdown
    for (int i = 0; i < DEBT_CATEGORY_COUNT; i++) {
        int change = current->category_counts[i] - baseline->category_counts[i];
        written += Q_snprintf(comparison_report + written, report_size - written,
            "  %s: %d -> %d (%+d)\n",
            TechnicalDebt_GetCategoryString(i),
            baseline->category_counts[i], current->category_counts[i], change);
    }

    return qtrue;
}

/*
=============================================================================
Automated Monitoring
=============================================================================
*/

qboolean TechnicalDebt_EnableAutoTracking(qboolean enable) {
    technical_debt_system.auto_tracking_enabled = enable;
    return qtrue;
}

qboolean TechnicalDebt_SetAlertThresholds(int critical_threshold, int high_threshold) {
    technical_debt_system.alert_threshold_critical = critical_threshold;
    technical_debt_system.alert_threshold_high = high_threshold;
    return qtrue;
}

qboolean TechnicalDebt_CheckAlerts(char* alert_message, size_t message_size) {
    if (!alert_message) return qfalse;

    const debt_metrics_t* metrics = &technical_debt_system.current_metrics;
    qboolean has_alerts = qfalse;

    Q_strncpyz(alert_message, "Technical Debt Alerts:\n", message_size);

    if (metrics->critical_items >= technical_debt_system.alert_threshold_critical) {
        Q_strcat(alert_message, message_size,
                va("🚨 CRITICAL: %d critical debt items detected!\n", metrics->critical_items));
        has_alerts = qtrue;
    }

    if (metrics->high_priority_items >= technical_debt_system.alert_threshold_high) {
        Q_strcat(alert_message, message_size,
                va("⚠️  HIGH PRIORITY: %d high-priority debt items detected!\n", metrics->high_priority_items));
        has_alerts = qtrue;
    }

    if (TechnicalDebt_IsTrendingUp(metrics->total_debt_score,
                                 technical_debt_system.history[technical_debt_system.num_history_points - 2].metrics.total_debt_score,
                                 technical_debt_system.debt_velocity_threshold)) {
        Q_strcat(alert_message, message_size,
                va("📈 TREND: Debt score increasing (velocity: %.1f/day)\n", metrics->debt_velocity));
        has_alerts = qtrue;
    }

    if (!has_alerts) {
        Q_strncpyz(alert_message, "✅ No technical debt alerts at this time.", message_size);
    }

    return has_alerts;
}

qboolean TechnicalDebt_RunAutomatedAnalysis(void) {
    // Update metrics
    TechnicalDebt_CalculateMetrics(&technical_debt_system.current_metrics);

    // Record history point
    TechnicalDebt_RecordHistoryPoint(NULL, NULL);

    // Check for alerts
    if (technical_debt_system.alerts_enabled) {
        char alert_message[1024];
        if (TechnicalDebt_CheckAlerts(alert_message, sizeof(alert_message))) {
            Com_Printf("Technical Debt Alert:\n%s\n", alert_message);
        }
    }

    technical_debt_system.last_analysis_time = Sys_Milliseconds();

    return qtrue;
}

/*
=============================================================================
Reporting and Export
=============================================================================
*/

qboolean TechnicalDebt_GenerateReport(const char* output_file, const char* format) {
    Q_UNUSED(output_file);
    Q_UNUSED(format);
    // Implementation would generate detailed reports in various formats
    return qtrue;
}

qboolean TechnicalDebt_GenerateDashboardData(const char* output_file) {
    Q_UNUSED(output_file);
    // Implementation would generate dashboard data (JSON/CSV)
    return qtrue;
}

qboolean TechnicalDebt_ExportForCI(const char* output_dir) {
    Q_UNUSED(output_dir);
    // Implementation would export debt data for CI consumption
    return qtrue;
}

qboolean TechnicalDebt_SaveState(const char* filename) {
    Q_UNUSED(filename);
    // Implementation would save current debt state to file
    return qtrue;
}

qboolean TechnicalDebt_LoadState(const char* filename) {
    Q_UNUSED(filename);
    // Implementation would load debt state from file
    return qtrue;
}

/*
=============================================================================
Configuration
=============================================================================
*/

qboolean TechnicalDebt_LoadConfig(const char* config_file) {
    Q_UNUSED(config_file);
    // Implementation would load configuration from JSON file
    return qtrue;
}

qboolean TechnicalDebt_SaveConfig(const char* config_file) {
    Q_UNUSED(config_file);
    // Implementation would save configuration to JSON file
    return qtrue;
}

qboolean TechnicalDebt_SetRetentionPolicy(int days) {
    technical_debt_system.history_retention_days = days;
    return qtrue;
}

qboolean TechnicalDebt_EnableIntegrations(qboolean quality_analysis,
                                        qboolean ci_cd,
                                        qboolean reports) {
    technical_debt_system.integrate_with_quality_analysis = quality_analysis;
    technical_debt_system.integrate_with_ci_cd = ci_cd;
    technical_debt_system.generate_reports = reports;
    return qtrue;
}

/*
=============================================================================
Utility Functions
=============================================================================
*/

const char* TechnicalDebt_GetSeverityString(debt_severity_t severity) {
    switch (severity) {
        case DEBT_SEVERITY_LOW: return "Low";
        case DEBT_SEVERITY_MEDIUM: return "Medium";
        case DEBT_SEVERITY_HIGH: return "High";
        case DEBT_SEVERITY_CRITICAL: return "Critical";
        default: return "Unknown";
    }
}

const char* TechnicalDebt_GetCategoryString(debt_category_t category) {
    switch (category) {
        case DEBT_CATEGORY_CODE_QUALITY: return "Code Quality";
        case DEBT_CATEGORY_COMPLEXITY: return "Complexity";
        case DEBT_CATEGORY_COVERAGE: return "Coverage";
        case DEBT_CATEGORY_MAINTAINABILITY: return "Maintainability";
        case DEBT_CATEGORY_DUPLICATION: return "Duplication";
        case DEBT_CATEGORY_SECURITY: return "Security";
        case DEBT_CATEGORY_PERFORMANCE: return "Performance";
        case DEBT_CATEGORY_ARCHITECTURE: return "Architecture";
        case DEBT_CATEGORY_DOCUMENTATION: return "Documentation";
        default: return "Unknown";
    }
}

qboolean TechnicalDebt_ValidateItem(const debt_item_t* item) {
    if (!item) return qfalse;
    if (!item->item_id[0]) return qfalse;
    if (!item->title[0]) return qfalse;
    if (item->category >= DEBT_CATEGORY_COUNT) return qfalse;
    if (item->severity >= DEBT_SEVERITY_COUNT) return qfalse;
    return qtrue;
}

float TechnicalDebt_CalculatePriorityScore(const debt_item_t* item) {
    if (!item) return 0.0f;

    float base_score = debt_category_weights[item->category] *
                      debt_severity_multipliers[item->severity];

    // Factor in age (older debt gets higher priority)
    uint64_t age_days = (Sys_Milliseconds() - item->introduced_date) / (1000 * 60 * 60 * 24);
    float age_multiplier = 1.0f + (age_days / 365.0f) * 0.5f; // 50% increase per year

    // Factor in estimated effort (higher effort = lower priority for same severity)
    float effort_multiplier = 1.0f;
    if (item->estimated_effort > 0) {
        effort_multiplier = 1.0f / (1.0f + item->estimated_effort / 40.0f); // Diminishing returns
    }

    return base_score * age_multiplier * effort_multiplier;
}

qboolean TechnicalDebt_IsTrendingUp(float current_value, float previous_value, float threshold) {
    return (current_value - previous_value) > threshold;
}

/*
=============================================================================
Debt Item Templates
=============================================================================
*/

qboolean TechnicalDebt_AddComplexityDebt(const char* file, const char* function,
                                       int complexity, int line) {
    debt_item_t item;
    memset(&item, 0, sizeof(item));

    Q_strncpyz(item.item_id, va("complexity_%s_%s_%d", file, function, line), sizeof(item.item_id));
    Q_strncpyz(item.title, va("High Complexity in %s", function), sizeof(item.title));
    Q_strncpyz(item.description, va("Function %s has cyclomatic complexity of %d, exceeding recommended limit",
                                  function, complexity), sizeof(item.description));
    item.category = DEBT_CATEGORY_COMPLEXITY;
    item.severity = (complexity > 25) ? DEBT_SEVERITY_HIGH :
                   (complexity > 15) ? DEBT_SEVERITY_MEDIUM : DEBT_SEVERITY_LOW;
    Q_strncpyz(item.file_path, file, sizeof(item.file_path));
    Q_strncpyz(item.function_name, function, sizeof(item.function_name));
    item.line_number = line;
    item.estimated_effort = complexity / 5.0f; // Rough estimate: 1 hour per 5 complexity points

    return TechnicalDebt_AddItem(&item);
}

qboolean TechnicalDebt_AddCoverageDebt(const char* file, float coverage) {
    debt_item_t item;
    memset(&item, 0, sizeof(item));

    Q_strncpyz(item.item_id, va("coverage_%s", file), sizeof(item.item_id));
    Q_strncpyz(item.title, va("Low Test Coverage in %s", file), sizeof(item.title));
    Q_strncpyz(item.description, va("File %s has %.1f%% test coverage, below recommended minimum",
                                  file, coverage), sizeof(item.description));
    item.category = DEBT_CATEGORY_COVERAGE;
    item.severity = (coverage < 50) ? DEBT_SEVERITY_CRITICAL :
                   (coverage < 70) ? DEBT_SEVERITY_HIGH :
                   (coverage < 80) ? DEBT_SEVERITY_MEDIUM : DEBT_SEVERITY_LOW;
    Q_strncpyz(item.file_path, file, sizeof(item.file_path));
    item.estimated_effort = (100 - coverage) / 10.0f; // Rough estimate

    return TechnicalDebt_AddItem(&item);
}

qboolean TechnicalDebt_AddDuplicationDebt(const char* file1, const char* file2,
                                        int duplicated_lines) {
    debt_item_t item;
    memset(&item, 0, sizeof(item));

    Q_strncpyz(item.item_id, va("duplication_%s_%s_%d", file1, file2, duplicated_lines), sizeof(item.item_id));
    Q_strncpyz(item.title, va("Code Duplication: %d lines", duplicated_lines), sizeof(item.title));
    Q_strncpyz(item.description, va("Duplicate code found between %s and %s (%d lines)",
                                  file1, file2, duplicated_lines), sizeof(item.description));
    item.category = DEBT_CATEGORY_DUPLICATION;
    item.severity = (duplicated_lines > 100) ? DEBT_SEVERITY_HIGH :
                   (duplicated_lines > 50) ? DEBT_SEVERITY_MEDIUM : DEBT_SEVERITY_LOW;
    Q_strncpyz(item.file_path, file1, sizeof(item.file_path));
    item.estimated_effort = duplicated_lines / 20.0f; // Rough estimate: 1 hour per 20 duplicated lines
    Q_strncpyz(item.tags, va("duplicate,%s", file2), sizeof(item.tags));

    return TechnicalDebt_AddItem(&item);
}

qboolean TechnicalDebt_AddSecurityDebt(const char* file, const char* vulnerability,
                                     debt_severity_t severity) {
    debt_item_t item;
    memset(&item, 0, sizeof(item));

    Q_strncpyz(item.item_id, va("security_%s_%s", file, vulnerability), sizeof(item.item_id));
    Q_strncpyz(item.title, va("Security Vulnerability: %s", vulnerability), sizeof(item.title));
    Q_strncpyz(item.description, va("Security vulnerability '%s' found in %s",
                                  vulnerability, file), sizeof(item.description));
    item.category = DEBT_CATEGORY_SECURITY;
    item.severity = severity;
    Q_strncpyz(item.file_path, file, sizeof(item.file_path));
    item.estimated_effort = 8.0f; // Security fixes typically take longer
    Q_strncpyz(item.tags, "security,critical", sizeof(item.tags));

    return TechnicalDebt_AddItem(&item);
}

/*
=============================================================================
CI/CD Integration Helpers
=============================================================================
*/

qboolean TechnicalDebt_CheckCIDebtLimits(const debt_metrics_t* metrics) {
    if (!metrics) return qfalse;

    // Check against CI limits (configurable)
    return metrics->critical_items <= technical_debt_system.alert_threshold_critical &&
           metrics->high_priority_items <= technical_debt_system.alert_threshold_high;
}

qboolean TechnicalDebt_GenerateCIBadge(const debt_metrics_t* metrics,
                                     const char* badge_file) {
    Q_UNUSED(metrics);
    Q_UNUSED(badge_file);
    // Implementation would generate CI badge based on debt metrics
    return qtrue;
}

qboolean TechnicalDebt_GetDebtHealthStatus(const debt_metrics_t* metrics,
                                         char* status, size_t status_size) {
    if (!metrics || !status) return qfalse;

    const char* health_status;
    if (metrics->total_debt_score < 50 && metrics->critical_items == 0) {
        health_status = "Excellent";
    } else if (metrics->total_debt_score < 100 && metrics->critical_items <= 2) {
        health_status = "Good";
    } else if (metrics->total_debt_score < 200 && metrics->critical_items <= 5) {
        health_status = "Needs Attention";
    } else {
        health_status = "Critical";
    }

    Q_strncpyz(status, health_status, status_size);
    return qtrue;
}
