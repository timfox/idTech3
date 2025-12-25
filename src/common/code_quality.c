/*
=============================================================================
Code Quality Analysis Framework Implementation

Automated code quality gates with coverage requirements and complexity limits.
=============================================================================
*/

#include "code_quality.h"
#include "qcommon.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

// Global code quality system instance
code_quality_system_t code_quality_system = {0};

// Default quality gate configurations
static const quality_gate_config_t default_gates[] = {
    {
        "minimum_coverage",
        "Minimum code coverage requirement",
        QUALITY_METRIC_COVERAGE,
        80.0f,  // 80% minimum coverage
        -1.0f,  // No maximum
        qtrue,  // Enabled
        qtrue,  // Blocking
        10      // High priority
    },
    {
        "maximum_complexity",
        "Maximum cyclomatic complexity limit",
        QUALITY_METRIC_COMPLEXITY,
        0.0f,   // No minimum
        15.0f,  // Max 15 complexity
        qtrue,  // Enabled
        qtrue,  // Blocking
        8       // High priority
    },
    {
        "maintainability_index",
        "Minimum maintainability index",
        QUALITY_METRIC_MAINTAINABILITY,
        50.0f,  // Minimum 50
        -1.0f,  // No maximum
        qtrue,  // Enabled
        qfalse, // Warning only
        6       // Medium priority
    },
    {
        "code_duplication",
        "Maximum code duplication percentage",
        QUALITY_METRIC_DUPLICATION,
        0.0f,   // No minimum
        5.0f,   // Max 5% duplication
        qtrue,  // Enabled
        qfalse, // Warning only
        4       // Medium priority
    }
};

/*
=============================================================================
Code Quality Analysis API Implementation
=============================================================================
*/

qboolean CodeQuality_Init(void) {
    if (code_quality_system.initialized) {
        return qtrue;
    }

    memset(&code_quality_system, 0, sizeof(code_quality_system_t));
    Q_strncpyz(code_quality_system.system_name, "Code Quality Gates", sizeof(code_quality_system.system_name));
    Q_strncpyz(code_quality_system.description, "Automated code quality analysis and gate enforcement", sizeof(code_quality_system.description));

    // Allocate gate storage
    code_quality_system.max_gates = 50;
    code_quality_system.gates = (quality_gate_config_t*)malloc(
        sizeof(quality_gate_config_t) * code_quality_system.max_gates);

    if (!code_quality_system.gates) {
        Com_Printf("Failed to allocate memory for quality gates\n");
        return qfalse;
    }

    memset(code_quality_system.gates, 0,
           sizeof(quality_gate_config_t) * code_quality_system.max_gates);

    // Set default thresholds
    CodeQuality_SetDefaults();

    // Add default gates
    CodeQuality_AddDefaultGates();

    code_quality_system.initialized = qtrue;
    code_quality_system.strict_mode = qfalse;

    Com_Printf("Code quality analysis system initialized\n");
    Com_Printf("Loaded %u default quality gates\n", code_quality_system.gate_count);

    return qtrue;
}

void CodeQuality_Shutdown(void) {
    if (!code_quality_system.initialized) {
        return;
    }

    if (code_quality_system.gates) {
        free(code_quality_system.gates);
        code_quality_system.gates = NULL;
    }

    code_quality_system.initialized = qfalse;
    Com_Printf("Code quality analysis system shutdown\n");
}

/*
=============================================================================
Quality Gate Management
=============================================================================
*/

qboolean CodeQuality_AddGate(const quality_gate_config_t* config) {
    if (!code_quality_system.initialized || !config ||
        code_quality_system.gate_count >= code_quality_system.max_gates) {
        return qfalse;
    }

    if (!CodeQuality_ValidateConfig(config)) {
        return qfalse;
    }

    // Check for duplicate gate names
    for (uint32_t i = 0; i < code_quality_system.gate_count; i++) {
        if (Q_stricmp(code_quality_system.gates[i].gate_name, config->gate_name) == 0) {
            Com_Printf("Gate with name '%s' already exists\n", config->gate_name);
            return qfalse;
        }
    }

    memcpy(&code_quality_system.gates[code_quality_system.gate_count],
           config, sizeof(quality_gate_config_t));
    code_quality_system.gate_count++;

    return qtrue;
}

qboolean CodeQuality_RemoveGate(const char* gate_name) {
    if (!gate_name) return qfalse;

    for (uint32_t i = 0; i < code_quality_system.gate_count; i++) {
        if (Q_stricmp(code_quality_system.gates[i].gate_name, gate_name) == 0) {
            // Shift remaining gates
            for (uint32_t j = i; j < code_quality_system.gate_count - 1; j++) {
                memcpy(&code_quality_system.gates[j],
                       &code_quality_system.gates[j + 1],
                       sizeof(quality_gate_config_t));
            }
            code_quality_system.gate_count--;
            return qtrue;
        }
    }

    return qfalse;
}

qboolean CodeQuality_EnableGate(const char* gate_name) {
    for (uint32_t i = 0; i < code_quality_system.gate_count; i++) {
        if (Q_stricmp(code_quality_system.gates[i].gate_name, gate_name) == 0) {
            code_quality_system.gates[i].enabled = qtrue;
            return qtrue;
        }
    }
    return qfalse;
}

qboolean CodeQuality_DisableGate(const char* gate_name) {
    for (uint32_t i = 0; i < code_quality_system.gate_count; i++) {
        if (Q_stricmp(code_quality_system.gates[i].gate_name, gate_name) == 0) {
            code_quality_system.gates[i].enabled = qfalse;
            return qtrue;
        }
    }
    return qfalse;
}

qboolean CodeQuality_SetGateThreshold(const char* gate_name, float min_threshold, float max_threshold) {
    for (uint32_t i = 0; i < code_quality_system.gate_count; i++) {
        if (Q_stricmp(code_quality_system.gates[i].gate_name, gate_name) == 0) {
            code_quality_system.gates[i].minimum_threshold = min_threshold;
            code_quality_system.gates[i].maximum_threshold = max_threshold;
            return qtrue;
        }
    }
    return qfalse;
}

/*
=============================================================================
Configuration Management
=============================================================================
*/

qboolean CodeQuality_LoadConfig(const char* config_file) {
    Q_UNUSED(config_file);
    // Implementation would load JSON/XML configuration file
    // For now, use defaults
    return CodeQuality_SetDefaults();
}

qboolean CodeQuality_SaveConfig(const char* config_file) {
    Q_UNUSED(config_file);
    // Implementation would save current configuration to file
    return qtrue;
}

qboolean CodeQuality_SetDefaults(void) {
    code_quality_system.min_coverage_percentage = 75.0f;
    code_quality_system.max_cyclomatic_complexity = 15;
    code_quality_system.min_maintainability_index = 50.0f;
    code_quality_system.max_duplication_percentage = 5.0f;
    return qtrue;
}

qboolean CodeQuality_SetStrictMode(qboolean strict) {
    code_quality_system.strict_mode = strict;
    return qtrue;
}

/*
=============================================================================
Analysis Execution
=============================================================================
*/

qboolean CodeQuality_RunAnalysis(code_quality_analysis_t* analysis) {
    if (!analysis) return qfalse;

    analysis->start_time = Sys_Milliseconds();
    Q_strncpyz(analysis->analysis_name, "Full Code Quality Analysis", sizeof(analysis->analysis_name));

    // Allocate storage for results
    analysis->max_coverage_entries = 1000;
    analysis->max_complexity_entries = 1000;
    analysis->max_failed_gates = 50;

    analysis->coverage_data = (coverage_data_t*)malloc(
        sizeof(coverage_data_t) * analysis->max_coverage_entries);
    analysis->complexity_data = (complexity_data_t*)malloc(
        sizeof(complexity_data_t) * analysis->max_complexity_entries);
    analysis->failed_gates = (quality_gate_config_t*)malloc(
        sizeof(quality_gate_config_t) * analysis->max_failed_gates);

    if (!analysis->coverage_data || !analysis->complexity_data || !analysis->failed_gates) {
        if (analysis->coverage_data) free(analysis->coverage_data);
        if (analysis->complexity_data) free(analysis->complexity_data);
        if (analysis->failed_gates) free(analysis->failed_gates);
        return qfalse;
    }

    memset(analysis->coverage_data, 0,
           sizeof(coverage_data_t) * analysis->max_coverage_entries);
    memset(analysis->complexity_data, 0,
           sizeof(complexity_data_t) * analysis->max_complexity_entries);
    memset(analysis->failed_gates, 0,
           sizeof(quality_gate_config_t) * analysis->max_failed_gates);

    // Perform coverage analysis
    if (!CodeQuality_MeasureCoverage(NULL, NULL,
                                   analysis->coverage_data,
                                   analysis->max_coverage_entries,
                                   &analysis->coverage_count)) {
        analysis->result = QUALITY_RESULT_ERROR;
        analysis->end_time = Sys_Milliseconds();
        analysis->duration_ms = analysis->end_time - analysis->start_time;
        return qtrue; // Analysis completed with error
    }

    // Calculate overall coverage
    analysis->overall_coverage = CodeQuality_CalculateOverallCoverage(
        analysis->coverage_data, analysis->coverage_count);

    // Perform complexity analysis
    const char* source_files[] = {"src/common/q_shared.c", "src/common/common.c"}; // Example files
    if (!CodeQuality_AnalyzeComplexity(source_files, 2,
                                     analysis->complexity_data,
                                     analysis->max_complexity_entries,
                                     &analysis->complexity_count)) {
        analysis->result = QUALITY_RESULT_ERROR;
        analysis->end_time = Sys_Milliseconds();
        analysis->duration_ms = analysis->end_time - analysis->start_time;
        return qtrue; // Analysis completed with error
    }

    // Calculate complexity metrics
    if (analysis->complexity_count > 0) {
        int total_complexity = 0;
        analysis->max_complexity = 0;

        for (uint32_t i = 0; i < analysis->complexity_count; i++) {
            total_complexity += analysis->complexity_data[i].cyclomatic_complexity;
            if (analysis->complexity_data[i].cyclomatic_complexity > analysis->max_complexity) {
                analysis->max_complexity = analysis->complexity_data[i].cyclomatic_complexity;
            }
        }

        analysis->average_complexity = (float)total_complexity / analysis->complexity_count;

        // Check complexity thresholds
        CodeQuality_CheckComplexityThresholds(analysis->complexity_data,
                                            analysis->complexity_count,
                                            code_quality_system.max_cyclomatic_complexity,
                                            &analysis->functions_above_threshold);
    }

    // Calculate quality scores
    analysis->maintainability_index = 75.0f; // Placeholder - would analyze actual code
    analysis->duplication_percentage = 2.5f;  // Placeholder
    analysis->style_score = 85.0f;           // Placeholder
    analysis->security_score = 90.0f;        // Placeholder

    // Evaluate quality gates
    if (!CodeQuality_EvaluateGates(analysis)) {
        analysis->result = QUALITY_RESULT_ERROR;
    } else if (analysis->failed_gate_count > 0) {
        // Determine most severe failure
        analysis->result = QUALITY_RESULT_PASS; // Default to pass
        for (uint32_t i = 0; i < analysis->failed_gate_count; i++) {
            if (analysis->failed_gates[i].blocking) {
                if (analysis->failed_gates[i].metric_type == QUALITY_METRIC_COVERAGE) {
                    analysis->result = QUALITY_RESULT_COVERAGE_LOW;
                } else if (analysis->failed_gates[i].metric_type == QUALITY_METRIC_COMPLEXITY) {
                    analysis->result = QUALITY_RESULT_COMPLEXITY_HIGH;
                } else if (analysis->failed_gates[i].metric_type == QUALITY_METRIC_MAINTAINABILITY) {
                    analysis->result = QUALITY_RESULT_MAINTAINABILITY_LOW;
                } else if (analysis->failed_gates[i].metric_type == QUALITY_METRIC_DUPLICATION) {
                    analysis->result = QUALITY_RESULT_DUPLICATION_HIGH;
                }
                break; // Most severe blocking failure found
            }
        }
    } else {
        analysis->result = QUALITY_RESULT_PASS;
    }

    analysis->end_time = Sys_Milliseconds();
    analysis->duration_ms = analysis->end_time - analysis->start_time;

    code_quality_system.total_analyses_run++;

    return qtrue;
}

qboolean CodeQuality_RunIncrementalAnalysis(const char* changed_files[],
                                          uint32_t file_count,
                                          code_quality_analysis_t* analysis) {
    Q_UNUSED(changed_files);
    Q_UNUSED(file_count);
    // Implementation would run incremental analysis on changed files only
    return CodeQuality_RunAnalysis(analysis);
}

qboolean CodeQuality_CancelAnalysis(void) {
    // Implementation for cancelling running analysis
    return qtrue;
}

qboolean CodeQuality_IsAnalysisRunning(void) {
    // Check if any analysis is currently running
    return qfalse;
}

/*
=============================================================================
Coverage Analysis
=============================================================================
*/

qboolean CodeQuality_MeasureCoverage(const char* executable_path,
                                   const char* test_command,
                                   coverage_data_t* coverage,
                                   uint32_t max_entries,
                                   uint32_t* num_entries) {
    Q_UNUSED(executable_path);
    Q_UNUSED(test_command);

    if (!coverage || !num_entries) return qfalse;

    // Placeholder implementation - in real implementation would:
    // 1. Run tests with coverage instrumentation (gcov, lcov, etc.)
    // 2. Parse coverage data files
    // 3. Populate coverage structures

    // Generate sample coverage data
    if (max_entries >= 3) {
        // Sample coverage entries
        Q_strncpyz(coverage[0].file_path, "src/common/q_shared.c", sizeof(coverage[0].file_path));
        Q_strncpyz(coverage[0].function_name, "Com_sprintf", sizeof(coverage[0].function_name));
        coverage[0].line_number = 100;
        coverage[0].total_lines = 50;
        coverage[0].covered_lines = 45;
        coverage[0].coverage_percentage = 90.0f;
        coverage[0].execution_count = 1000;

        Q_strncpyz(coverage[1].file_path, "src/common/common.c", sizeof(coverage[1].file_path));
        Q_strncpyz(coverage[1].function_name, "Com_Printf", sizeof(coverage[1].function_name));
        coverage[1].line_number = 200;
        coverage[1].total_lines = 30;
        coverage[1].covered_lines = 25;
        coverage[1].coverage_percentage = 83.3f;
        coverage[1].execution_count = 500;

        Q_strncpyz(coverage[2].file_path, "src/renderers/vulkan/vk.c", sizeof(coverage[2].file_path));
        Q_strncpyz(coverage[2].function_name, "vk_initialize", sizeof(coverage[2].function_name));
        coverage[2].line_number = 50;
        coverage[2].total_lines = 100;
        coverage[2].covered_lines = 75;
        coverage[2].coverage_percentage = 75.0f;
        coverage[2].execution_count = 200;

        *num_entries = 3;
    }

    return qtrue;
}

qboolean CodeQuality_ParseCoverageData(const char* coverage_file,
                                     coverage_data_t* coverage,
                                     uint32_t max_entries,
                                     uint32_t* num_entries) {
    Q_UNUSED(coverage_file);
    Q_UNUSED(coverage);
    Q_UNUSED(max_entries);
    Q_UNUSED(num_entries);
    // Implementation would parse LCOV, gcov, or other coverage formats
    return qtrue;
}

float CodeQuality_CalculateOverallCoverage(const coverage_data_t* coverage,
                                         uint32_t count) {
    if (!coverage || count == 0) return 0.0f;

    float total_weighted_coverage = 0.0f;
    int total_lines = 0;

    for (uint32_t i = 0; i < count; i++) {
        total_weighted_coverage += coverage[i].covered_lines;
        total_lines += coverage[i].total_lines;
    }

    return total_lines > 0 ? (total_weighted_coverage / total_lines) * 100.0f : 0.0f;
}

/*
=============================================================================
Complexity Analysis
=============================================================================
*/

qboolean CodeQuality_AnalyzeComplexity(const char* source_files[],
                                      uint32_t file_count,
                                      complexity_data_t* complexity,
                                      uint32_t max_entries,
                                      uint32_t* num_entries) {
    if (!source_files || !complexity || !num_entries) return qfalse;

    *num_entries = 0;

    for (uint32_t i = 0; i < file_count && *num_entries < max_entries; i++) {
        FILE* fp = fopen(source_files[i], "r");
        if (!fp) continue;

        fseek(fp, 0, SEEK_END);
        long file_size = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        char* source_code = (char*)malloc(file_size + 1);
        if (!source_code) {
            fclose(fp);
            continue;
        }

        size_t read_size = fread(source_code, 1, file_size, fp);
        source_code[read_size] = '\0';
        fclose(fp);

        // Parse functions and calculate complexity
        char* ptr = source_code;
        int line_number = 1;
        char function_name[128];

        while (*ptr && *num_entries < max_entries) {
            // Simple function detection (very basic)
            if (strstr(ptr, "qboolean") || strstr(ptr, "void") || strstr(ptr, "int") || strstr(ptr, "float")) {
                char* func_start = ptr;

                // Extract function name (simplified)
                char* paren = strchr(ptr, '(');
                if (paren) {
                    // Look backwards for function name
                    char* name_start = paren;
                    while (name_start > func_start && !isspace(*name_start) && *name_start != '*') {
                        name_start--;
                    }
                    if (*name_start) name_start++;

                    size_t name_len = paren - name_start;
                    if (name_len < sizeof(function_name)) {
                        strncpy(function_name, name_start, name_len);
                        function_name[name_len] = '\0';

                        // Calculate complexity for this function
                        int cyclo_complexity = CodeQuality_CalculateCyclomaticComplexity(ptr);
                        int cognitive_complexity = CodeQuality_CalculateCognitiveComplexity(ptr);

                        Q_strncpyz(complexity[*num_entries].file_path, source_files[i],
                                 sizeof(complexity[*num_entries].file_path));
                        Q_strncpyz(complexity[*num_entries].function_name, function_name,
                                 sizeof(complexity[*num_entries].function_name));
                        complexity[*num_entries].line_number = line_number;
                        complexity[*num_entries].cyclomatic_complexity = cyclo_complexity;
                        complexity[*num_entries].cognitive_complexity = cognitive_complexity;
                        complexity[*num_entries].nesting_depth = 1; // Simplified
                        complexity[*num_entries].parameter_count = 1; // Simplified
                        complexity[*num_entries].statement_count = 10; // Simplified

                        (*num_entries)++;
                    }
                }
            }

            // Move to next line
            while (*ptr && *ptr != '\n') ptr++;
            if (*ptr == '\n') {
                ptr++;
                line_number++;
            }
        }

        free(source_code);
    }

    return qtrue;
}

int CodeQuality_CalculateCyclomaticComplexity(const char* source_code) {
    if (!source_code) return 1;

    int complexity = 1; // Base complexity

    const char* keywords[] = {"if", "while", "for", "case", "&&", "||", "??"};
    const int num_keywords = sizeof(keywords) / sizeof(keywords[0]);

    for (int i = 0; i < num_keywords; i++) {
        const char* ptr = source_code;
        while ((ptr = strstr(ptr, keywords[i])) != NULL) {
            complexity++;
            ptr += strlen(keywords[i]);
        }
    }

    return complexity;
}

int CodeQuality_CalculateCognitiveComplexity(const char* source_code) {
    // Simplified cognitive complexity calculation
    int complexity = CodeQuality_CalculateCyclomaticComplexity(source_code);

    // Add nesting penalties
    const char* ptr = source_code;
    int nesting_level = 0;
    int max_nesting = 0;

    while (*ptr) {
        if (*ptr == '{') {
            nesting_level++;
            if (nesting_level > max_nesting) max_nesting = nesting_level;
        } else if (*ptr == '}') {
            nesting_level--;
        }
        ptr++;
    }

    // Add nesting complexity
    complexity += max_nesting;

    return complexity;
}

qboolean CodeQuality_CheckComplexityThresholds(const complexity_data_t* complexity,
                                             uint32_t count,
                                             int max_complexity,
                                             uint32_t* violations) {
    if (!complexity || !violations) return qfalse;

    *violations = 0;

    for (uint32_t i = 0; i < count; i++) {
        if (complexity[i].cyclomatic_complexity > max_complexity) {
            (*violations)++;
        }
    }

    return qtrue;
}

/*
=============================================================================
Quality Metrics Calculation
=============================================================================
*/

float CodeQuality_CalculateMaintainabilityIndex(const char* source_code,
                                              int cyclomatic_complexity,
                                              int lines_of_code) {
    if (!source_code || lines_of_code <= 0) return 0.0f;

    // Simplified maintainability index calculation
    // MI = 171 - 5.2 * ln(V) - 0.23 * G - 16.2 * ln(LOC)
    // Where V is volume, G is cyclomatic complexity, LOC is lines of code

    float volume = strlen(source_code) * log2f(2.0f); // Simplified
    float mi = 171.0f - 5.2f * logf(volume) - 0.23f * cyclomatic_complexity - 16.2f * logf(lines_of_code);

    // Clamp to 0-100 range
    if (mi < 0.0f) mi = 0.0f;
    if (mi > 100.0f) mi = 100.0f;

    return mi;
}

float CodeQuality_CalculateDuplicationPercentage(const char* source_files[],
                                                uint32_t file_count) {
    Q_UNUSED(source_files);
    Q_UNUSED(file_count);
    // Implementation would analyze code duplication across files
    // For now, return a placeholder value
    return 2.5f;
}

float CodeQuality_CalculateStyleScore(const char* source_code) {
    Q_UNUSED(source_code);
    // Implementation would analyze code style compliance
    // For now, return a placeholder value
    return 85.0f;
}

float CodeQuality_CalculateSecurityScore(const char* source_code) {
    Q_UNUSED(source_code);
    // Implementation would analyze security issues
    // For now, return a placeholder value
    return 90.0f;
}

/*
=============================================================================
Gate Evaluation
=============================================================================
*/

qboolean CodeQuality_EvaluateGates(const code_quality_analysis_t* analysis) {
    if (!analysis) return qfalse;

    analysis->failed_gate_count = 0;

    for (uint32_t i = 0; i < code_quality_system.gate_count; i++) {
        const quality_gate_config_t* gate = &code_quality_system.gates[i];

        if (!gate->enabled) continue;

        if (!CodeQuality_CheckGate(gate, analysis)) {
            if (analysis->failed_gate_count < analysis->max_failed_gates) {
                memcpy(&analysis->failed_gates[analysis->failed_gate_count],
                       gate, sizeof(quality_gate_config_t));
                analysis->failed_gate_count++;
            }

            if (gate->blocking) {
                code_quality_system.total_gates_failed++;
            }
        } else {
            code_quality_system.total_gates_passed++;
        }
    }

    return qtrue;
}

qboolean CodeQuality_CheckGate(const quality_gate_config_t* gate,
                             const code_quality_analysis_t* analysis) {
    if (!gate || !analysis) return qfalse;

    float value = 0.0f;

    switch (gate->metric_type) {
        case QUALITY_METRIC_COVERAGE:
            value = analysis->overall_coverage;
            break;
        case QUALITY_METRIC_COMPLEXITY:
            value = analysis->max_complexity;
            break;
        case QUALITY_METRIC_MAINTAINABILITY:
            value = analysis->maintainability_index;
            break;
        case QUALITY_METRIC_DUPLICATION:
            value = analysis->duplication_percentage;
            break;
        case QUALITY_METRIC_STYLE:
            value = analysis->style_score;
            break;
        case QUALITY_METRIC_SECURITY:
            value = analysis->security_score;
            break;
        default:
            return qfalse;
    }

    // Check thresholds
    if (gate->minimum_threshold >= 0.0f && value < gate->minimum_threshold) {
        return qfalse;
    }

    if (gate->maximum_threshold >= 0.0f && value > gate->maximum_threshold) {
        return qfalse;
    }

    return qtrue;
}

qboolean CodeQuality_GetFailedGates(const code_quality_analysis_t* analysis,
                                  quality_gate_config_t* failed_gates,
                                  uint32_t max_gates,
                                  uint32_t* num_failed) {
    if (!analysis || !failed_gates || !num_failed) return qfalse;

    *num_failed = 0;

    for (uint32_t i = 0; i < analysis->failed_gate_count && i < max_gates; i++) {
        memcpy(&failed_gates[i], &analysis->failed_gates[i], sizeof(quality_gate_config_t));
        (*num_failed)++;
    }

    return qtrue;
}

/*
=============================================================================
Reporting and Export
=============================================================================
*/

qboolean CodeQuality_GenerateReport(const code_quality_analysis_t* analysis,
                                  const char* output_file,
                                  const char* format) {
    Q_UNUSED(analysis);
    Q_UNUSED(output_file);
    Q_UNUSED(format);
    // Implementation would generate detailed reports in various formats
    return qtrue;
}

qboolean CodeQuality_ExportForCI(const code_quality_analysis_t* analysis,
                               const char* output_dir) {
    Q_UNUSED(analysis);
    Q_UNUSED(output_dir);
    // Implementation would export results for CI consumption
    return qtrue;
}

qboolean CodeQuality_SaveResults(const char* filename,
                               const code_quality_analysis_t* analysis) {
    Q_UNUSED(filename);
    Q_UNUSED(analysis);
    // Implementation would save results to file
    return qtrue;
}

qboolean CodeQuality_LoadResults(const char* filename,
                               code_quality_analysis_t* analysis) {
    Q_UNUSED(filename);
    Q_UNUSED(analysis);
    // Implementation would load results from file
    return qtrue;
}

/*
=============================================================================
Utility Functions
=============================================================================
*/

const char* CodeQuality_GetResultString(code_quality_result_t result) {
    switch (result) {
        case QUALITY_RESULT_PASS: return "PASS";
        case QUALITY_RESULT_COVERAGE_LOW: return "COVERAGE_LOW";
        case QUALITY_RESULT_COMPLEXITY_HIGH: return "COMPLEXITY_HIGH";
        case QUALITY_RESULT_MAINTAINABILITY_LOW: return "MAINTAINABILITY_LOW";
        case QUALITY_RESULT_DUPLICATION_HIGH: return "DUPLICATION_HIGH";
        case QUALITY_RESULT_STYLE_VIOLATIONS: return "STYLE_VIOLATIONS";
        case QUALITY_RESULT_SECURITY_ISSUES: return "SECURITY_ISSUES";
        case QUALITY_RESULT_TIMEOUT: return "TIMEOUT";
        case QUALITY_RESULT_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

const char* CodeQuality_GetMetricString(quality_metric_type_t metric) {
    switch (metric) {
        case QUALITY_METRIC_COVERAGE: return "Coverage";
        case QUALITY_METRIC_COMPLEXITY: return "Complexity";
        case QUALITY_METRIC_MAINTAINABILITY: return "Maintainability";
        case QUALITY_METRIC_DUPLICATION: return "Duplication";
        case QUALITY_METRIC_STYLE: return "Style";
        case QUALITY_METRIC_SECURITY: return "Security";
        default: return "Unknown";
    }
}

qboolean CodeQuality_ValidateConfig(const quality_gate_config_t* config) {
    if (!config) return qfalse;
    if (!config->gate_name[0]) return qfalse;
    if (config->metric_type >= QUALITY_METRIC_COUNT) return qfalse;
    if (config->minimum_threshold > config->maximum_threshold &&
        config->maximum_threshold >= 0.0f) return qfalse;
    return qtrue;
}

qboolean CodeQuality_IsCoverageSufficient(float coverage_percentage,
                                        float minimum_required) {
    return coverage_percentage >= minimum_required;
}

qboolean CodeQuality_IsComplexityAcceptable(int complexity,
                                          int maximum_allowed) {
    return complexity <= maximum_allowed;
}

/*
=============================================================================
Built-in Quality Gates
=============================================================================
*/

qboolean CodeQuality_AddDefaultGates(void) {
    for (uint32_t i = 0; i < sizeof(default_gates) / sizeof(default_gates[0]); i++) {
        if (!CodeQuality_AddGate(&default_gates[i])) {
            Com_Printf("Failed to add default gate: %s\n", default_gates[i].gate_name);
        }
    }

    return qtrue;
}

qboolean CodeQuality_AddCoverageGate(float min_coverage) {
    quality_gate_config_t gate;
    memset(&gate, 0, sizeof(gate));

    Q_strncpyz(gate.gate_name, "coverage_gate", sizeof(gate.gate_name));
    Q_strncpyz(gate.description, "Code coverage requirement", sizeof(gate.description));
    gate.metric_type = QUALITY_METRIC_COVERAGE;
    gate.minimum_threshold = min_coverage;
    gate.maximum_threshold = -1.0f;
    gate.enabled = qtrue;
    gate.blocking = qtrue;
    gate.priority = 9;

    return CodeQuality_AddGate(&gate);
}

qboolean CodeQuality_AddComplexityGate(int max_complexity) {
    quality_gate_config_t gate;
    memset(&gate, 0, sizeof(gate));

    Q_strncpyz(gate.gate_name, "complexity_gate", sizeof(gate.gate_name));
    Q_strncpyz(gate.description, "Cyclomatic complexity limit", sizeof(gate.description));
    gate.metric_type = QUALITY_METRIC_COMPLEXITY;
    gate.minimum_threshold = 0.0f;
    gate.maximum_threshold = max_complexity;
    gate.enabled = qtrue;
    gate.blocking = qtrue;
    gate.priority = 8;

    return CodeQuality_AddGate(&gate);
}

qboolean CodeQuality_AddMaintainabilityGate(float min_index) {
    quality_gate_config_t gate;
    memset(&gate, 0, sizeof(gate));

    Q_strncpyz(gate.gate_name, "maintainability_gate", sizeof(gate.gate_name));
    Q_strncpyz(gate.description, "Maintainability index requirement", sizeof(gate.description));
    gate.metric_type = QUALITY_METRIC_MAINTAINABILITY;
    gate.minimum_threshold = min_index;
    gate.maximum_threshold = -1.0f;
    gate.enabled = qtrue;
    gate.blocking = qfalse;
    gate.priority = 6;

    return CodeQuality_AddGate(&gate);
}

qboolean CodeQuality_AddDuplicationGate(float max_duplication) {
    quality_gate_config_t gate;
    memset(&gate, 0, sizeof(gate));

    Q_strncpyz(gate.gate_name, "duplication_gate", sizeof(gate.gate_name));
    Q_strncpyz(gate.description, "Code duplication limit", sizeof(gate.description));
    gate.metric_type = QUALITY_METRIC_DUPLICATION;
    gate.minimum_threshold = 0.0f;
    gate.maximum_threshold = max_duplication;
    gate.enabled = qtrue;
    gate.blocking = qfalse;
    gate.priority = 4;

    return CodeQuality_AddGate(&gate);
}

/*
=============================================================================
CI/CD Integration Helpers
=============================================================================
*/

qboolean CodeQuality_CheckCIGates(const code_quality_analysis_t* analysis) {
    if (!analysis) return qfalse;

    // Check if any blocking gates failed
    for (uint32_t i = 0; i < analysis->failed_gate_count; i++) {
        if (analysis->failed_gates[i].blocking) {
            return qfalse; // CI should fail
        }
    }

    return qtrue; // CI should pass
}

qboolean CodeQuality_GenerateCIBadge(const code_quality_analysis_t* analysis,
                                   const char* badge_file) {
    Q_UNUSED(analysis);
    Q_UNUSED(badge_file);
    // Implementation would generate CI badge (SVG/PNG)
    return qtrue;
}

qboolean CodeQuality_CompareToBaseline(const code_quality_analysis_t* current,
                                     const code_quality_analysis_t* baseline,
                                     char* comparison_report,
                                     size_t report_size) {
    Q_UNUSED(current);
    Q_UNUSED(baseline);
    Q_UNUSED(comparison_report);
    Q_UNUSED(report_size);
    // Implementation would compare current analysis to baseline
    return qtrue;
}
