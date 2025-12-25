/*
=============================================================================
Build Optimization Monitoring System Implementation

Tracks build performance, binary size, and optimization effectiveness.
=============================================================================
*/

#include "build_optimization.h"
#include "q_shared.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// Global build optimization monitor
build_optimization_monitor_t build_optimization_monitor = {0};

// Statistics file version
#define BUILD_OPTIMIZATION_STATS_VERSION 1

/*
=============================================================================
Build Optimization API Implementation
=============================================================================
*/

qboolean BuildOptimization_Init(const char* stats_file, const char* report_file) {
    if (build_optimization_monitor.enabled) {
        return qtrue; // Already initialized
    }

    memset(&build_optimization_monitor, 0, sizeof(build_optimization_monitor_t));

    // Set file paths
    if (stats_file) {
        Q_strncpyz(build_optimization_monitor.stats_file, stats_file, sizeof(build_optimization_monitor.stats_file));
    } else {
        Q_strncpyz(build_optimization_monitor.stats_file, "build_optimization_stats.txt", sizeof(build_optimization_monitor.stats_file));
    }

    if (report_file) {
        Q_strncpyz(build_optimization_monitor.report_file, report_file, sizeof(build_optimization_monitor.report_file));
    } else {
        Q_strncpyz(build_optimization_monitor.report_file, "build_optimization_report.txt", sizeof(build_optimization_monitor.report_file));
    }

    // Load existing statistics
    if (!BuildOptimization_LoadStatistics()) {
        // Initialize with defaults if loading failed
        memset(&build_optimization_monitor.effectiveness, 0, sizeof(optimization_effectiveness_t));
        memset(&build_optimization_monitor.binary_analysis, 0, sizeof(binary_analysis_t));
    }

    build_optimization_monitor.enabled = qtrue;

    Com_Printf("Build optimization monitoring initialized\n");
    Com_Printf("Statistics file: %s\n", build_optimization_monitor.stats_file);
    Com_Printf("Report file: %s\n", build_optimization_monitor.report_file);

    return qtrue;
}

void BuildOptimization_Shutdown(void) {
    if (!build_optimization_monitor.enabled) {
        return;
    }

    // Save final statistics and generate report
    BuildOptimization_SaveStatistics();
    BuildOptimization_GenerateReport();

    build_optimization_monitor.enabled = qfalse;
    Com_Printf("Build optimization monitoring shutdown\n");
}

/*
=============================================================================
Metrics Collection
=============================================================================
*/

void BuildOptimization_StartBuild(void) {
    if (!build_optimization_monitor.enabled) return;

    build_optimization_monitor.current_metrics.build_start_time = Sys_Milliseconds();
    build_optimization_monitor.current_metrics.build_duration_ms = 0;
    build_optimization_monitor.current_metrics.binary_size_bytes = 0;

    Com_Printf("Build optimization monitoring started\n");
}

void BuildOptimization_EndBuild(uint64_t binary_size_bytes) {
    if (!build_optimization_monitor.enabled) return;

    build_optimization_monitor.current_metrics.build_end_time = Sys_Milliseconds();
    build_optimization_monitor.current_metrics.build_duration_ms =
        build_optimization_monitor.current_metrics.build_end_time -
        build_optimization_monitor.current_metrics.build_start_time;
    build_optimization_monitor.current_metrics.binary_size_bytes = binary_size_bytes;

    // Update effectiveness statistics
    build_optimization_monitor.effectiveness.total_builds++;

    // Track min/max binary sizes
    if (build_optimization_monitor.effectiveness.total_builds == 1 ||
        binary_size_bytes < build_optimization_monitor.effectiveness.min_binary_size_bytes) {
        build_optimization_monitor.effectiveness.min_binary_size_bytes = binary_size_bytes;
    }

    if (binary_size_bytes > build_optimization_monitor.effectiveness.max_binary_size_bytes) {
        build_optimization_monitor.effectiveness.max_binary_size_bytes = binary_size_bytes;
    }

    // Calculate averages
    uint64_t total_time = build_optimization_monitor.effectiveness.average_build_time_ms *
                         (build_optimization_monitor.effectiveness.total_builds - 1);
    total_time += build_optimization_monitor.current_metrics.build_duration_ms;
    build_optimization_monitor.effectiveness.average_build_time_ms =
        total_time / build_optimization_monitor.effectiveness.total_builds;

    uint64_t total_size = build_optimization_monitor.effectiveness.average_binary_size_bytes *
                         (build_optimization_monitor.effectiveness.total_builds - 1);
    total_size += binary_size_bytes;
    build_optimization_monitor.effectiveness.average_binary_size_bytes =
        total_size / build_optimization_monitor.effectiveness.total_builds;

    // Calculate effectiveness
    BuildOptimization_CalculateEffectiveness();

    Com_Printf("Build completed: %llu ms, %llu bytes\n",
               build_optimization_monitor.current_metrics.build_duration_ms,
               binary_size_bytes);
}

void BuildOptimization_RecordBinaryAnalysis(const binary_analysis_t* analysis) {
    if (!build_optimization_monitor.enabled || !analysis) return;

    memcpy(&build_optimization_monitor.binary_analysis, analysis, sizeof(binary_analysis_t));
}

void BuildOptimization_SetCompilerInfo(const char* version, const char* build_type) {
    if (!build_optimization_monitor.enabled) return;

    if (version) {
        Q_strncpyz(build_optimization_monitor.current_metrics.compiler_version,
                  version, sizeof(build_optimization_monitor.current_metrics.compiler_version));
    }

    if (build_type) {
        Q_strncpyz(build_optimization_monitor.current_metrics.build_type,
                  build_type, sizeof(build_optimization_monitor.current_metrics.build_type));
    }
}

void BuildOptimization_SetOptimizationFlags(qboolean lto, qboolean optimizations) {
    if (!build_optimization_monitor.enabled) return;

    build_optimization_monitor.current_metrics.lto_enabled = lto;
    build_optimization_monitor.current_metrics.optimizations_enabled = optimizations;
}

/*
=============================================================================
Statistics and Analysis
=============================================================================
*/

void BuildOptimization_GetMetrics(build_metrics_t* metrics) {
    if (metrics) {
        memcpy(metrics, &build_optimization_monitor.current_metrics, sizeof(build_metrics_t));
    }
}

void BuildOptimization_GetEffectiveness(optimization_effectiveness_t* effectiveness) {
    if (effectiveness) {
        memcpy(effectiveness, &build_optimization_monitor.effectiveness, sizeof(optimization_effectiveness_t));
    }
}

void BuildOptimization_GetBinaryAnalysis(binary_analysis_t* analysis) {
    if (analysis) {
        memcpy(analysis, &build_optimization_monitor.binary_analysis, sizeof(binary_analysis_t));
    }
}

void BuildOptimization_CalculateEffectiveness(void) {
    optimization_effectiveness_t* eff = &build_optimization_monitor.effectiveness;

    // Calculate size reduction (comparing to theoretical maximum)
    if (eff->total_builds > 1 && eff->min_binary_size_bytes > 0) {
        eff->size_reduction_percentage =
            ((double)(eff->max_binary_size_bytes - eff->min_binary_size_bytes) /
             (double)eff->max_binary_size_bytes) * 100.0;
    }

    // Determine if optimizations are effective
    eff->lto_effective = build_optimization_monitor.current_metrics.lto_enabled &&
                        build_optimization_monitor.current_metrics.binary_size_bytes <
                        (eff->average_binary_size_bytes * 1.1); // Within 10% of average

    eff->dead_code_elimination_effective =
        build_optimization_monitor.binary_analysis.total_sections < 100; // Reasonable section count
}

/*
=============================================================================
Reporting
=============================================================================
*/

void BuildOptimization_GenerateReport(void) {
    if (!build_optimization_monitor.enabled) return;

    FILE* file = fopen(build_optimization_monitor.report_file, "w");
    if (!file) {
        Com_Printf("Warning: Failed to create optimization report: %s\n",
                  build_optimization_monitor.report_file);
        return;
    }

    fprintf(file, "Build Optimization Report\n");
    fprintf(file, "=========================\n\n");

    fprintf(file, "Current Build Metrics:\n");
    fprintf(file, "  Build Time: %.2f seconds\n",
            build_optimization_monitor.current_metrics.build_duration_ms / 1000.0);
    fprintf(file, "  Binary Size: %.2f MB (%llu bytes)\n",
            build_optimization_monitor.current_metrics.binary_size_bytes / (1024.0 * 1024.0),
            build_optimization_monitor.current_metrics.binary_size_bytes);
    fprintf(file, "  LTO Enabled: %s\n",
            build_optimization_monitor.current_metrics.lto_enabled ? "Yes" : "No");
    fprintf(file, "  Optimizations: %s\n",
            build_optimization_monitor.current_metrics.optimizations_enabled ? "Yes" : "No");

    if (build_optimization_monitor.current_metrics.compiler_version[0]) {
        fprintf(file, "  Compiler: %s\n",
                build_optimization_monitor.current_metrics.compiler_version);
    }

    if (build_optimization_monitor.current_metrics.build_type[0]) {
        fprintf(file, "  Build Type: %s\n",
                build_optimization_monitor.current_metrics.build_type);
    }

    fprintf(file, "\nBinary Analysis:\n");
    binary_analysis_t* analysis = &build_optimization_monitor.binary_analysis;
    fprintf(file, "  Text Section: %u bytes\n", analysis->text_section_size);
    fprintf(file, "  Data Section: %u bytes\n", analysis->data_section_size);
    fprintf(file, "  BSS Section: %u bytes\n", analysis->bss_section_size);
    fprintf(file, "  ROData Section: %u bytes\n", analysis->rodata_section_size);
    fprintf(file, "  Total Sections: %u\n", analysis->total_sections);
    fprintf(file, "  Stripped Symbols: %u\n", analysis->stripped_symbols);

    if (analysis->compression_ratio > 0) {
        fprintf(file, "  Compression Ratio: %.2fx\n", analysis->compression_ratio);
    }

    fprintf(file, "\nOptimization Effectiveness:\n");
    optimization_effectiveness_t* eff = &build_optimization_monitor.effectiveness;
    fprintf(file, "  Total Builds: %u\n", eff->total_builds);
    fprintf(file, "  Average Build Time: %.2f seconds\n", eff->average_build_time_ms / 1000.0);
    fprintf(file, "  Average Binary Size: %.2f MB\n",
            eff->average_binary_size_bytes / (1024.0 * 1024.0));
    fprintf(file, "  Size Reduction: %.1f%%\n", eff->size_reduction_percentage);
    fprintf(file, "  LTO Effective: %s\n", eff->lto_effective ? "Yes" : "No");
    fprintf(file, "  Dead Code Elimination: %s\n",
            eff->dead_code_elimination_effective ? "Effective" : "Not Effective");

    fprintf(file, "\nRecommendations:\n");

    if (build_optimization_monitor.current_metrics.build_duration_ms > 300000) { // 5 minutes
        fprintf(file, "  - Build time is high, consider enabling ccache\n");
        fprintf(file, "  - Consider using ThinLTO for faster compilation\n");
    }

    if (build_optimization_monitor.current_metrics.binary_size_bytes > 50 * 1024 * 1024) { // 50MB
        fprintf(file, "  - Binary size is large, enable more aggressive optimizations\n");
        fprintf(file, "  - Consider dead code elimination\n");
    }

    if (!build_optimization_monitor.current_metrics.lto_enabled) {
        fprintf(file, "  - Enable LTO for better cross-module optimization\n");
    }

    if (eff->size_reduction_percentage < 5.0) {
        fprintf(file, "  - Size reduction is minimal, review optimization settings\n");
    }

    fprintf(file, "\nReport generated at: %llu\n", Sys_Milliseconds());

    fclose(file);
}

void BuildOptimization_PrintSummary(void) {
    Com_Printf("=== Build Optimization Summary ===\n");

    build_metrics_t* metrics = &build_optimization_monitor.current_metrics;
    optimization_effectiveness_t* eff = &build_optimization_monitor.effectiveness;

    Com_Printf("Current Build:\n");
    Com_Printf("  Duration: %.2f seconds\n", metrics->build_duration_ms / 1000.0);
    Com_Printf("  Binary Size: %.2f MB\n", metrics->binary_size_bytes / (1024.0 * 1024.0));
    Com_Printf("  LTO: %s\n", metrics->lto_enabled ? "Enabled" : "Disabled");

    Com_Printf("\nEffectiveness:\n");
    Com_Printf("  Total Builds: %u\n", eff->total_builds);
    Com_Printf("  Average Time: %.2f seconds\n", eff->average_build_time_ms / 1000.0);
    Com_Printf("  Size Reduction: %.1f%%\n", eff->size_reduction_percentage);
    Com_Printf("  LTO Effective: %s\n", eff->lto_effective ? "Yes" : "No");

    Com_Printf("==================================\n");
}

void BuildOptimization_SaveStatistics(void) {
    if (!build_optimization_monitor.enabled) return;

    FILE* file = fopen(build_optimization_monitor.stats_file, "w");
    if (!file) {
        Com_Printf("Warning: Failed to save optimization statistics: %s\n",
                  build_optimization_monitor.stats_file);
        return;
    }

    fprintf(file, "# Id Tech 3 Build Optimization Statistics\n");
    fprintf(file, "# Version: %d\n", BUILD_OPTIMIZATION_STATS_VERSION);
    fprintf(file, "# Generated: %llu\n", Sys_Milliseconds());
    fprintf(file, "\n");

    fprintf(file, "[CURRENT_METRICS]\n");
    fprintf(file, "build_start_time=%llu\n", build_optimization_monitor.current_metrics.build_start_time);
    fprintf(file, "build_end_time=%llu\n", build_optimization_monitor.current_metrics.build_end_time);
    fprintf(file, "build_duration_ms=%llu\n", build_optimization_monitor.current_metrics.build_duration_ms);
    fprintf(file, "binary_size_bytes=%llu\n", build_optimization_monitor.current_metrics.binary_size_bytes);
    fprintf(file, "lto_enabled=%d\n", build_optimization_monitor.current_metrics.lto_enabled ? 1 : 0);
    fprintf(file, "optimizations_enabled=%d\n", build_optimization_monitor.current_metrics.optimizations_enabled ? 1 : 0);

    fprintf(file, "\n[EFFECTIVENESS]\n");
    fprintf(file, "total_builds=%u\n", build_optimization_monitor.effectiveness.total_builds);
    fprintf(file, "average_build_time_ms=%llu\n", build_optimization_monitor.effectiveness.average_build_time_ms);
    fprintf(file, "average_binary_size_bytes=%llu\n", build_optimization_monitor.effectiveness.average_binary_size_bytes);
    fprintf(file, "min_binary_size_bytes=%llu\n", build_optimization_monitor.effectiveness.min_binary_size_bytes);
    fprintf(file, "max_binary_size_bytes=%llu\n", build_optimization_monitor.effectiveness.max_binary_size_bytes);
    fprintf(file, "size_reduction_percentage=%.2f\n", build_optimization_monitor.effectiveness.size_reduction_percentage);
    fprintf(file, "lto_effective=%d\n", build_optimization_monitor.effectiveness.lto_effective ? 1 : 0);
    fprintf(file, "dead_code_elimination_effective=%d\n", build_optimization_monitor.effectiveness.dead_code_elimination_effective ? 1 : 0);

    fprintf(file, "\n[BINARY_ANALYSIS]\n");
    fprintf(file, "text_section_size=%u\n", build_optimization_monitor.binary_analysis.text_section_size);
    fprintf(file, "data_section_size=%u\n", build_optimization_monitor.binary_analysis.data_section_size);
    fprintf(file, "bss_section_size=%u\n", build_optimization_monitor.binary_analysis.bss_section_size);
    fprintf(file, "rodata_section_size=%u\n", build_optimization_monitor.binary_analysis.rodata_section_size);
    fprintf(file, "total_sections=%u\n", build_optimization_monitor.binary_analysis.total_sections);
    fprintf(file, "compression_ratio=%.2f\n", build_optimization_monitor.binary_analysis.compression_ratio);

    fclose(file);
}

qboolean BuildOptimization_LoadStatistics(void) {
    if (!build_optimization_monitor.enabled) return qfalse;

    FILE* file = fopen(build_optimization_monitor.stats_file, "r");
    if (!file) {
        return qfalse; // File doesn't exist, not an error
    }

    char line[512];
    while (fgets(line, sizeof(line), file)) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;

        // Parse sections and values
        if (strcmp(line, "[CURRENT_METRICS]\n") == 0 ||
            strcmp(line, "[EFFECTIVENESS]\n") == 0 ||
            strcmp(line, "[BINARY_ANALYSIS]\n") == 0) {
            // Section headers - values will be parsed below
        } else if (strstr(line, "=")) {
            char* equals = strchr(line, '=');
            if (equals) {
                *equals = '\0';
                char* key = line;
                char* value = equals + 1;

                // Remove newline
                char* newline = strchr(value, '\n');
                if (newline) *newline = '\0';

                // Parse values (simplified - only loading effectiveness for now)
                if (strcmp(key, "total_builds") == 0) {
                    build_optimization_monitor.effectiveness.total_builds = atoi(value);
                } else if (strcmp(key, "average_build_time_ms") == 0) {
                    build_optimization_monitor.effectiveness.average_build_time_ms = strtoull(value, NULL, 10);
                } else if (strcmp(key, "average_binary_size_bytes") == 0) {
                    build_optimization_monitor.effectiveness.average_binary_size_bytes = strtoull(value, NULL, 10);
                } else if (strcmp(key, "size_reduction_percentage") == 0) {
                    build_optimization_monitor.effectiveness.size_reduction_percentage = atof(value);
                }
            }
        }
    }

    fclose(file);
    return qtrue;
}

/*
=============================================================================
Utility Functions
=============================================================================
*/

uint64_t BuildOptimization_GetAverageBuildTime(void) {
    return build_optimization_monitor.effectiveness.average_build_time_ms;
}

uint64_t BuildOptimization_GetAverageBinarySize(void) {
    return build_optimization_monitor.effectiveness.average_binary_size_bytes;
}

double BuildOptimization_GetSizeReduction(void) {
    return build_optimization_monitor.effectiveness.size_reduction_percentage;
}

double BuildOptimization_GetBuildTimeImprovement(void) {
    // This would require baseline measurements
    return 0.0; // Placeholder
}

qboolean BuildOptimization_IsOptimizationEffective(void) {
    return build_optimization_monitor.effectiveness.lto_effective &&
           build_optimization_monitor.effectiveness.dead_code_elimination_effective;
}

/*
=============================================================================
Console Commands
=============================================================================
*/

void BuildOptimization_Status_f(void) {
    BuildOptimization_PrintSummary();
}

void BuildOptimization_Metrics_f(void) {
    build_metrics_t metrics;
    BuildOptimization_GetMetrics(&metrics);

    Com_Printf("=== Build Metrics ===\n");
    Com_Printf("Build Time: %.2f seconds\n", metrics.build_duration_ms / 1000.0);
    Com_Printf("Binary Size: %.2f MB\n", metrics.binary_size_bytes / (1024.0 * 1024.0));
    Com_Printf("Sections: %u\n", metrics.num_sections);
    Com_Printf("Functions: %u\n", metrics.num_functions);
    Com_Printf("LTO: %s\n", metrics.lto_enabled ? "Enabled" : "Disabled");
    Com_Printf("Optimizations: %s\n", metrics.optimizations_enabled ? "Enabled" : "Disabled");
    Com_Printf("====================\n");
}

void BuildOptimization_Report_f(void) {
    BuildOptimization_GenerateReport();
    Com_Printf("Build optimization report generated: %s\n",
              build_optimization_monitor.report_file);
}

void BuildOptimization_Analyze_f(void) {
    binary_analysis_t analysis;
    BuildOptimization_GetBinaryAnalysis(&analysis);

    Com_Printf("=== Binary Analysis ===\n");
    Com_Printf("Text Section: %u bytes\n", analysis.text_section_size);
    Com_Printf("Data Section: %u bytes\n", analysis.data_section_size);
    Com_Printf("BSS Section: %u bytes\n", analysis.bss_section_size);
    Com_Printf("ROData Section: %u bytes\n", analysis.rodata_section_size);
    Com_Printf("Total Sections: %u\n", analysis.total_sections);

    if (analysis.compression_ratio > 0) {
        Com_Printf("Compression Ratio: %.2fx\n", analysis.compression_ratio);
    }

    Com_Printf("=======================\n");
}

void BuildOptimization_Reset_f(void) {
    memset(&build_optimization_monitor.effectiveness, 0, sizeof(optimization_effectiveness_t));
    memset(&build_optimization_monitor.binary_analysis, 0, sizeof(binary_analysis_t));

    BuildOptimization_SaveStatistics();
    Com_Printf("Build optimization statistics reset\n");
}
