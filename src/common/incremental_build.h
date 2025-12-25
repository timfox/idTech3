/*
=============================================================================
Incremental Build Monitoring System

Tracks build times, dependencies, and provides statistics for incremental builds.
=============================================================================
*/

#ifndef __INCREMENTAL_BUILD_H__
#define __INCREMENTAL_BUILD_H__

#include "q_shared.h"
#include <time.h>

// Build timing information
typedef struct {
    time_t start_time;
    time_t end_time;
    uint64_t duration_ms;
    qboolean was_incremental;
    uint32_t files_compiled;
    uint32_t files_cached;
    uint32_t files_total;
} build_timing_t;

// Build statistics
typedef struct {
    uint32_t total_builds;
    uint32_t incremental_builds;
    uint32_t clean_builds;
    uint64_t total_build_time_ms;
    uint64_t total_incremental_time_ms;
    uint64_t total_clean_time_ms;
    double average_build_time_sec;
    double average_incremental_time_sec;
    double average_clean_time_sec;
    double incremental_speedup_ratio; // clean_time / incremental_time
} build_statistics_t;

// File dependency information
typedef struct {
    char filename[256];
    time_t last_modified;
    time_t last_compiled;
    qboolean was_cached;
    uint64_t compile_time_ms;
    uint32_t dependency_count;
    char dependencies[16][256]; // Max 16 direct dependencies
} file_dependency_t;

// Build cache information
typedef struct {
    uint64_t cache_hits;
    uint64_t cache_misses;
    uint64_t cache_size_bytes;
    uint32_t cached_files;
    double cache_hit_ratio;
    char cache_directory[256];
} build_cache_info_t;

// Incremental build monitor
typedef struct {
    qboolean enabled;
    qboolean is_build_active;
    build_timing_t current_build;
    build_statistics_t statistics;
    build_cache_info_t cache_info;
    file_dependency_t* file_dependencies;
    uint32_t max_files;
    uint32_t file_count;
    char stats_file[256];
    char cache_dir[256];
} incremental_build_monitor_t;

extern incremental_build_monitor_t incremental_monitor;

// Incremental build API
qboolean IncrementalBuild_Init(const char* stats_file, const char* cache_dir);
void IncrementalBuild_Shutdown(void);

// Build timing
void IncrementalBuild_StartBuild(qboolean is_incremental);
void IncrementalBuild_EndBuild(void);
void IncrementalBuild_RecordFileCompilation(const char* filename, uint64_t compile_time_ms, qboolean was_cached);

// Dependency tracking
void IncrementalBuild_AddFileDependency(const char* filename, const char* dependency);
void IncrementalBuild_UpdateFileTimestamp(const char* filename);
qboolean IncrementalBuild_IsFileModified(const char* filename);

// Cache management
void IncrementalBuild_UpdateCacheStats(uint64_t hits, uint64_t misses, uint64_t size_bytes, uint32_t cached_files);
qboolean IncrementalBuild_ShouldUseCache(const char* filename);

// Statistics and reporting
void IncrementalBuild_GetStatistics(build_statistics_t* stats);
void IncrementalBuild_GetCacheInfo(build_cache_info_t* cache);
void IncrementalBuild_PrintStatistics(void);
void IncrementalBuild_PrintCacheInfo(void);
void IncrementalBuild_SaveStatistics(void);
qboolean IncrementalBuild_LoadStatistics(void);

// Utility functions
double IncrementalBuild_CalculateSpeedupRatio(void);
uint32_t IncrementalBuild_GetModifiedFiles(void);
void IncrementalBuild_PrintBuildReport(void);
void IncrementalBuild_ResetStatistics(void);

// Console commands
void IncrementalBuild_Status_f(void);
void IncrementalBuild_Stats_f(void);
void IncrementalBuild_Cache_f(void);
void IncrementalBuild_Report_f(void);
void IncrementalBuild_Reset_f(void);

#endif // __INCREMENTAL_BUILD_H__
