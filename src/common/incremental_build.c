/*
=============================================================================
Incremental Build Monitoring System Implementation

Tracks build times, dependencies, and provides statistics for incremental builds.
=============================================================================
*/

#include "incremental_build.h"
#include "q_shared.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

// Global incremental build monitor
incremental_build_monitor_t incremental_monitor = {0};

// Maximum number of files to track
#define MAX_TRACKED_FILES 4096
#define BUILD_STATS_FILE_VERSION 1

/*
=============================================================================
File System Utilities
=============================================================================
*/

static time_t get_file_modification_time(const char* filename) {
    struct stat st;
    if (stat(filename, &st) == 0) {
        return st.st_mtime;
    }
    return 0;
}

static uint64_t get_directory_size(const char* dirname) {
    DIR* dir = opendir(dirname);
    if (!dir) return 0;

    uint64_t total_size = 0;
    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[512];
        Q_snprintf(full_path, sizeof(full_path), "%s/%s", dirname, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                total_size += get_directory_size(full_path);
            } else {
                total_size += st.st_size;
            }
        }
    }

    closedir(dir);
    return total_size;
}

static uint32_t count_files_in_directory(const char* dirname) {
    DIR* dir = opendir(dirname);
    if (!dir) return 0;

    uint32_t count = 0;
    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[512];
        Q_snprintf(full_path, sizeof(full_path), "%s/%s", dirname, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                count += count_files_in_directory(full_path);
            } else {
                count++;
            }
        }
    }

    closedir(dir);
    return count;
}

/*
=============================================================================
Incremental Build API Implementation
=============================================================================
*/

qboolean IncrementalBuild_Init(const char* stats_file, const char* cache_dir) {
    if (incremental_monitor.enabled) {
        return qtrue; // Already initialized
    }

    memset(&incremental_monitor, 0, sizeof(incremental_build_monitor_t));

    // Set file paths
    if (stats_file) {
        Q_strncpyz(incremental_monitor.stats_file, stats_file, sizeof(incremental_monitor.stats_file));
    } else {
        Q_strncpyz(incremental_monitor.stats_file, "build_stats.txt", sizeof(incremental_monitor.stats_file));
    }

    if (cache_dir) {
        Q_strncpyz(incremental_monitor.cache_dir, cache_dir, sizeof(incremental_monitor.cache_dir));
    } else {
        Q_strncpyz(incremental_monitor.cache_dir, ".ccache", sizeof(incremental_monitor.cache_dir));
    }

    // Allocate file dependency array
    incremental_monitor.max_files = MAX_TRACKED_FILES;
    incremental_monitor.file_dependencies = (file_dependency_t*)malloc(
        sizeof(file_dependency_t) * incremental_monitor.max_files);

    if (!incremental_monitor.file_dependencies) {
        Com_Printf("Failed to allocate file dependency tracking array\n");
        return qfalse;
    }

    memset(incremental_monitor.file_dependencies, 0,
           sizeof(file_dependency_t) * incremental_monitor.max_files);

    // Load existing statistics
    if (!IncrementalBuild_LoadStatistics()) {
        // Initialize with defaults if loading failed
        memset(&incremental_monitor.statistics, 0, sizeof(build_statistics_t));
    }

    incremental_monitor.enabled = qtrue;

    Com_Printf("Incremental build monitoring initialized\n");
    Com_Printf("Statistics file: %s\n", incremental_monitor.stats_file);
    Com_Printf("Cache directory: %s\n", incremental_monitor.cache_dir);

    return qtrue;
}

void IncrementalBuild_Shutdown(void) {
    if (!incremental_monitor.enabled) {
        return;
    }

    // Save final statistics
    IncrementalBuild_SaveStatistics();

    // Free resources
    if (incremental_monitor.file_dependencies) {
        free(incremental_monitor.file_dependencies);
        incremental_monitor.file_dependencies = NULL;
    }

    incremental_monitor.enabled = qfalse;
    Com_Printf("Incremental build monitoring shutdown\n");
}

/*
=============================================================================
Build Timing
=============================================================================
*/

void IncrementalBuild_StartBuild(qboolean is_incremental) {
    if (!incremental_monitor.enabled) return;

    incremental_monitor.is_build_active = qtrue;
    incremental_monitor.current_build.start_time = time(NULL);
    incremental_monitor.current_build.was_incremental = is_incremental;
    incremental_monitor.current_build.files_compiled = 0;
    incremental_monitor.current_build.files_cached = 0;
    incremental_monitor.current_build.files_total = 0;
    incremental_monitor.current_build.duration_ms = 0;

    Com_Printf("Build started: %s\n", is_incremental ? "incremental" : "clean");
}

void IncrementalBuild_EndBuild(void) {
    if (!incremental_monitor.enabled || !incremental_monitor.is_build_active) return;

    incremental_monitor.current_build.end_time = time(NULL);
    incremental_monitor.current_build.duration_ms =
        (uint64_t)(incremental_monitor.current_build.end_time - incremental_monitor.current_build.start_time) * 1000;

    // Update statistics
    incremental_monitor.statistics.total_builds++;

    if (incremental_monitor.current_build.was_incremental) {
        incremental_monitor.statistics.incremental_builds++;
        incremental_monitor.statistics.total_incremental_time_ms += incremental_monitor.current_build.duration_ms;
    } else {
        incremental_monitor.statistics.clean_builds++;
        incremental_monitor.statistics.total_clean_time_ms += incremental_monitor.current_build.duration_ms;
    }

    incremental_monitor.statistics.total_build_time_ms += incremental_monitor.current_build.duration_ms;

    // Calculate averages
    if (incremental_monitor.statistics.total_builds > 0) {
        incremental_monitor.statistics.average_build_time_sec =
            (double)incremental_monitor.statistics.total_build_time_ms / incremental_monitor.statistics.total_builds / 1000.0;
    }

    if (incremental_monitor.statistics.incremental_builds > 0) {
        incremental_monitor.statistics.average_incremental_time_sec =
            (double)incremental_monitor.statistics.total_incremental_time_ms / incremental_monitor.statistics.incremental_builds / 1000.0;
    }

    if (incremental_monitor.statistics.clean_builds > 0) {
        incremental_monitor.statistics.average_clean_time_sec =
            (double)incremental_monitor.statistics.total_clean_time_ms / incremental_monitor.statistics.clean_builds / 1000.0;
    }

    // Calculate speedup ratio
    if (incremental_monitor.statistics.average_clean_time_sec > 0 && incremental_monitor.statistics.average_incremental_time_sec > 0) {
        incremental_monitor.statistics.incremental_speedup_ratio =
            incremental_monitor.statistics.average_clean_time_sec / incremental_monitor.statistics.average_incremental_time_sec;
    }

    incremental_monitor.is_build_active = qfalse;

    // Save statistics
    IncrementalBuild_SaveStatistics();

    Com_Printf("Build completed in %.2f seconds (%s)\n",
               incremental_monitor.current_build.duration_ms / 1000.0,
               incremental_monitor.current_build.was_incremental ? "incremental" : "clean");
}

void IncrementalBuild_RecordFileCompilation(const char* filename, uint64_t compile_time_ms, qboolean was_cached) {
    if (!incremental_monitor.enabled || !incremental_monitor.is_build_active) return;

    incremental_monitor.current_build.files_total++;

    if (was_cached) {
        incremental_monitor.current_build.files_cached++;
    } else {
        incremental_monitor.current_build.files_compiled++;
    }

    // Update file dependency information
    IncrementalBuild_UpdateFileTimestamp(filename);

    // Find or create file entry
    for (uint32_t i = 0; i < incremental_monitor.file_count; i++) {
        file_dependency_t* file_dep = &incremental_monitor.file_dependencies[i];
        if (strcmp(file_dep->filename, filename) == 0) {
            file_dep->last_compiled = time(NULL);
            file_dep->compile_time_ms = compile_time_ms;
            file_dep->was_cached = was_cached;
            return;
        }
    }

    // Add new file entry if space available
    if (incremental_monitor.file_count < incremental_monitor.max_files) {
        file_dependency_t* file_dep = &incremental_monitor.file_dependencies[incremental_monitor.file_count++];
        Q_strncpyz(file_dep->filename, filename, sizeof(file_dep->filename));
        file_dep->last_compiled = time(NULL);
        file_dep->compile_time_ms = compile_time_ms;
        file_dep->was_cached = was_cached;
        file_dep->last_modified = get_file_modification_time(filename);
    }
}

/*
=============================================================================
Dependency Tracking
=============================================================================
*/

void IncrementalBuild_AddFileDependency(const char* filename, const char* dependency) {
    if (!incremental_monitor.enabled || !filename || !dependency) return;

    // Find file entry
    for (uint32_t i = 0; i < incremental_monitor.file_count; i++) {
        file_dependency_t* file_dep = &incremental_monitor.file_dependencies[i];
        if (strcmp(file_dep->filename, filename) == 0) {
            // Add dependency if space available
            if (file_dep->dependency_count < 16) {
                Q_strncpyz(file_dep->dependencies[file_dep->dependency_count++],
                          dependency, sizeof(file_dep->dependencies[0]));
            }
            break;
        }
    }
}

void IncrementalBuild_UpdateFileTimestamp(const char* filename) {
    if (!incremental_monitor.enabled || !filename) return;

    // Find or create file entry
    for (uint32_t i = 0; i < incremental_monitor.file_count; i++) {
        file_dependency_t* file_dep = &incremental_monitor.file_dependencies[i];
        if (strcmp(file_dep->filename, filename) == 0) {
            file_dep->last_modified = get_file_modification_time(filename);
            return;
        }
    }

    // Add new file entry
    if (incremental_monitor.file_count < incremental_monitor.max_files) {
        file_dependency_t* file_dep = &incremental_monitor.file_dependencies[incremental_monitor.file_count++];
        Q_strncpyz(file_dep->filename, filename, sizeof(file_dep->filename));
        file_dep->last_modified = get_file_modification_time(filename);
        file_dep->last_compiled = 0;
    }
}

qboolean IncrementalBuild_IsFileModified(const char* filename) {
    if (!incremental_monitor.enabled || !filename) return qtrue; // Assume modified if not tracking

    time_t current_mtime = get_file_modification_time(filename);

    // Find file entry
    for (uint32_t i = 0; i < incremental_monitor.file_count; i++) {
        file_dependency_t* file_dep = &incremental_monitor.file_dependencies[i];
        if (strcmp(file_dep->filename, filename) == 0) {
            // Check if file was modified after last compilation
            return current_mtime > file_dep->last_compiled;
        }
    }

    // File not tracked, assume it needs compilation
    return qtrue;
}

/*
=============================================================================
Cache Management
=============================================================================
*/

void IncrementalBuild_UpdateCacheStats(uint64_t hits, uint64_t misses, uint64_t size_bytes, uint32_t cached_files) {
    if (!incremental_monitor.enabled) return;

    incremental_monitor.cache_info.cache_hits = hits;
    incremental_monitor.cache_info.cache_misses = misses;
    incremental_monitor.cache_info.cache_size_bytes = size_bytes;
    incremental_monitor.cache_info.cached_files = cached_files;

    // Calculate hit ratio
    uint64_t total = hits + misses;
    if (total > 0) {
        incremental_monitor.cache_info.cache_hit_ratio = (double)hits / (double)total * 100.0;
    }

    // Update directory information
    Q_strncpyz(incremental_monitor.cache_info.cache_directory,
               incremental_monitor.cache_dir,
               sizeof(incremental_monitor.cache_info.cache_directory));
}

qboolean IncrementalBuild_ShouldUseCache(const char* filename) {
    if (!incremental_monitor.enabled) return qfalse;

    // Simple heuristic: use cache if file hasn't been modified recently
    return !IncrementalBuild_IsFileModified(filename);
}

/*
=============================================================================
Statistics and Reporting
=============================================================================
*/

void IncrementalBuild_GetStatistics(build_statistics_t* stats) {
    if (stats) {
        memcpy(stats, &incremental_monitor.statistics, sizeof(build_statistics_t));
    }
}

void IncrementalBuild_GetCacheInfo(build_cache_info_t* cache) {
    if (cache) {
        memcpy(cache, &incremental_monitor.cache_info, sizeof(build_cache_info_t));
    }
}

void IncrementalBuild_PrintStatistics(void) {
    build_statistics_t stats = incremental_monitor.statistics;

    Com_Printf("=== Incremental Build Statistics ===\n");
    Com_Printf("Total Builds: %u\n", stats.total_builds);
    Com_Printf("Incremental Builds: %u\n", stats.incremental_builds);
    Com_Printf("Clean Builds: %u\n", stats.clean_builds);
    Com_Printf("Average Build Time: %.2f seconds\n", stats.average_build_time_sec);
    Com_Printf("Average Incremental Time: %.2f seconds\n", stats.average_incremental_time_sec);
    Com_Printf("Average Clean Time: %.2f seconds\n", stats.average_clean_time_sec);

    if (stats.incremental_speedup_ratio > 1.0) {
        Com_Printf("Incremental Speedup: %.2fx faster than clean builds\n", stats.incremental_speedup_ratio);
    }

    Com_Printf("====================================\n");
}

void IncrementalBuild_PrintCacheInfo(void) {
    build_cache_info_t cache = incremental_monitor.cache_info;

    Com_Printf("=== Build Cache Information ===\n");
    Com_Printf("Cache Hits: %llu\n", cache.cache_hits);
    Com_Printf("Cache Misses: %llu\n", cache.cache_misses);
    Com_Printf("Hit Ratio: %.1f%%\n", cache.cache_hit_ratio);
    Com_Printf("Cache Size: %.2f MB\n", cache.cache_size_bytes / (1024.0 * 1024.0));
    Com_Printf("Cached Files: %u\n", cache.cached_files);
    Com_Printf("Cache Directory: %s\n", cache.cache_directory);
    Com_Printf("=================================\n");
}

void IncrementalBuild_SaveStatistics(void) {
    if (!incremental_monitor.enabled) return;

    FILE* file = fopen(incremental_monitor.stats_file, "w");
    if (!file) {
        Com_Printf("Warning: Failed to save build statistics to %s\n", incremental_monitor.stats_file);
        return;
    }

    fprintf(file, "# Id Tech 3 Incremental Build Statistics\n");
    fprintf(file, "# Version: %d\n", BUILD_STATS_FILE_VERSION);
    fprintf(file, "# Generated: %llu\n", (unsigned long long)time(NULL));
    fprintf(file, "\n");

    fprintf(file, "[STATISTICS]\n");
    fprintf(file, "total_builds=%u\n", incremental_monitor.statistics.total_builds);
    fprintf(file, "incremental_builds=%u\n", incremental_monitor.statistics.incremental_builds);
    fprintf(file, "clean_builds=%u\n", incremental_monitor.statistics.clean_builds);
    fprintf(file, "total_build_time_ms=%llu\n", incremental_monitor.statistics.total_build_time_ms);
    fprintf(file, "total_incremental_time_ms=%llu\n", incremental_monitor.statistics.total_incremental_time_ms);
    fprintf(file, "total_clean_time_ms=%llu\n", incremental_monitor.statistics.total_clean_time_ms);
    fprintf(file, "average_build_time_sec=%.2f\n", incremental_monitor.statistics.average_build_time_sec);
    fprintf(file, "average_incremental_time_sec=%.2f\n", incremental_monitor.statistics.average_incremental_time_sec);
    fprintf(file, "average_clean_time_sec=%.2f\n", incremental_monitor.statistics.average_clean_time_sec);
    fprintf(file, "incremental_speedup_ratio=%.2f\n", incremental_monitor.statistics.incremental_speedup_ratio);

    fprintf(file, "\n[CACHE]\n");
    fprintf(file, "cache_hits=%llu\n", incremental_monitor.cache_info.cache_hits);
    fprintf(file, "cache_misses=%llu\n", incremental_monitor.cache_info.cache_misses);
    fprintf(file, "cache_size_bytes=%llu\n", incremental_monitor.cache_info.cache_size_bytes);
    fprintf(file, "cached_files=%u\n", incremental_monitor.cache_info.cached_files);
    fprintf(file, "cache_hit_ratio=%.2f\n", incremental_monitor.cache_info.cache_hit_ratio);

    fprintf(file, "\n[FILES]\n");
    fprintf(file, "tracked_files=%u\n", incremental_monitor.file_count);

    for (uint32_t i = 0; i < incremental_monitor.file_count; i++) {
        file_dependency_t* file_dep = &incremental_monitor.file_dependencies[i];
        fprintf(file, "file_%u=%s,%llu,%llu,%llu,%d\n",
                i,
                file_dep->filename,
                (unsigned long long)file_dep->last_modified,
                (unsigned long long)file_dep->last_compiled,
                (unsigned long long)file_dep->compile_time_ms,
                file_dep->was_cached ? 1 : 0);
    }

    fclose(file);
}

qboolean IncrementalBuild_LoadStatistics(void) {
    if (!incremental_monitor.enabled) return qfalse;

    FILE* file = fopen(incremental_monitor.stats_file, "r");
    if (!file) {
        return qfalse; // File doesn't exist, not an error
    }

    char line[512];
    while (fgets(line, sizeof(line), file)) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;

        // Parse sections and values
        if (strcmp(line, "[STATISTICS]\n") == 0) {
            // Statistics section - values will be parsed below
        } else if (strcmp(line, "[CACHE]\n") == 0) {
            // Cache section
        } else if (strcmp(line, "[FILES]\n") == 0) {
            // Files section
        } else if (strstr(line, "=")) {
            char* equals = strchr(line, '=');
            if (equals) {
                *equals = '\0';
                char* key = line;
                char* value = equals + 1;

                // Remove newline
                char* newline = strchr(value, '\n');
                if (newline) *newline = '\0';

                // Parse statistics
                if (strcmp(key, "total_builds") == 0) {
                    incremental_monitor.statistics.total_builds = atoi(value);
                } else if (strcmp(key, "incremental_builds") == 0) {
                    incremental_monitor.statistics.incremental_builds = atoi(value);
                } else if (strcmp(key, "clean_builds") == 0) {
                    incremental_monitor.statistics.clean_builds = atoi(value);
                } else if (strcmp(key, "total_build_time_ms") == 0) {
                    incremental_monitor.statistics.total_build_time_ms = strtoull(value, NULL, 10);
                } else if (strcmp(key, "total_incremental_time_ms") == 0) {
                    incremental_monitor.statistics.total_incremental_time_ms = strtoull(value, NULL, 10);
                } else if (strcmp(key, "total_clean_time_ms") == 0) {
                    incremental_monitor.statistics.total_clean_time_ms = strtoull(value, NULL, 10);
                } else if (strcmp(key, "average_build_time_sec") == 0) {
                    incremental_monitor.statistics.average_build_time_sec = atof(value);
                } else if (strcmp(key, "average_incremental_time_sec") == 0) {
                    incremental_monitor.statistics.average_incremental_time_sec = atof(value);
                } else if (strcmp(key, "average_clean_time_sec") == 0) {
                    incremental_monitor.statistics.average_clean_time_sec = atof(value);
                } else if (strcmp(key, "incremental_speedup_ratio") == 0) {
                    incremental_monitor.statistics.incremental_speedup_ratio = atof(value);
                }
                // Cache parsing would go here
                else if (strcmp(key, "cache_hits") == 0) {
                    incremental_monitor.cache_info.cache_hits = strtoull(value, NULL, 10);
                } else if (strcmp(key, "cache_misses") == 0) {
                    incremental_monitor.cache_info.cache_misses = strtoull(value, NULL, 10);
                } else if (strcmp(key, "cache_size_bytes") == 0) {
                    incremental_monitor.cache_info.cache_size_bytes = strtoull(value, NULL, 10);
                } else if (strcmp(key, "cached_files") == 0) {
                    incremental_monitor.cache_info.cached_files = atoi(value);
                } else if (strcmp(key, "cache_hit_ratio") == 0) {
                    incremental_monitor.cache_info.cache_hit_ratio = atof(value);
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

double IncrementalBuild_CalculateSpeedupRatio(void) {
    return incremental_monitor.statistics.incremental_speedup_ratio;
}

uint32_t IncrementalBuild_GetModifiedFiles(void) {
    uint32_t modified = 0;

    for (uint32_t i = 0; i < incremental_monitor.file_count; i++) {
        file_dependency_t* file_dep = &incremental_monitor.file_dependencies[i];
        if (IncrementalBuild_IsFileModified(file_dep->filename)) {
            modified++;
        }
    }

    return modified;
}

void IncrementalBuild_PrintBuildReport(void) {
    Com_Printf("=== Incremental Build Report ===\n");

    if (incremental_monitor.is_build_active) {
        Com_Printf("Build Status: ACTIVE\n");
        Com_Printf("Build Type: %s\n", incremental_monitor.current_build.was_incremental ? "Incremental" : "Clean");
        Com_Printf("Files Compiled: %u\n", incremental_monitor.current_build.files_compiled);
        Com_Printf("Files Cached: %u\n", incremental_monitor.current_build.files_cached);
        Com_Printf("Total Files: %u\n", incremental_monitor.current_build.files_total);
    } else {
        Com_Printf("Build Status: IDLE\n");
        Com_Printf("Last Build Duration: %.2f seconds\n",
                   incremental_monitor.current_build.duration_ms / 1000.0);
    }

    uint32_t modified_files = IncrementalBuild_GetModifiedFiles();
    Com_Printf("Modified Files Since Last Build: %u\n", modified_files);

    Com_Printf("Tracked Files: %u/%u\n", incremental_monitor.file_count, incremental_monitor.max_files);

    Com_Printf("===============================\n");
}

void IncrementalBuild_ResetStatistics(void) {
    memset(&incremental_monitor.statistics, 0, sizeof(build_statistics_t));
    memset(&incremental_monitor.cache_info, 0, sizeof(build_cache_info_t));
    incremental_monitor.file_count = 0;

    if (incremental_monitor.file_dependencies) {
        memset(incremental_monitor.file_dependencies, 0,
               sizeof(file_dependency_t) * incremental_monitor.max_files);
    }

    Com_Printf("Incremental build statistics reset\n");
}

/*
=============================================================================
Console Commands
=============================================================================
*/

void IncrementalBuild_Status_f(void) {
    IncrementalBuild_PrintBuildReport();
}

void IncrementalBuild_Stats_f(void) {
    IncrementalBuild_PrintStatistics();
}

void IncrementalBuild_Cache_f(void) {
    IncrementalBuild_PrintCacheInfo();
}

void IncrementalBuild_Report_f(void) {
    IncrementalBuild_PrintBuildReport();
    Com_Printf("\n");
    IncrementalBuild_PrintStatistics();
    Com_Printf("\n");
    IncrementalBuild_PrintCacheInfo();
}

void IncrementalBuild_Reset_f(void) {
    IncrementalBuild_ResetStatistics();
    IncrementalBuild_SaveStatistics();
}
