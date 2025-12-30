/*
=============================================================================
Comprehensive Engine Profiling System Implementation

Integrates Tracy, Vulkan render profiling, and performance benchmarking
=============================================================================
*/

#include "profiler.h"
#include "qcommon.h"

// Command handlers for profiling controls
static void Profiler_Toggle_f(void) {
	Profiler_ToggleMode();
}

static void Profiler_Status_f(void) {
	Com_Printf("=== Profiler Status ===\n");
	Com_Printf("Mode: %s\n", Profiler_GetModeString(profiler_config.mode));
	Com_Printf("Enabled: %s\n", Profiler_IsEnabled() ? "Yes" : "No");
	Com_Printf("Vulkan Profiling: %s\n", Profiler_Vulkan_IsEnabled() ? "Yes" : "No");
	Com_Printf("Benchmark Profiling: %s\n", Profiler_Benchmark_IsEnabled() ? "Yes" : "No");
	Com_Printf("Overhead Limit: %.1f%%\n", profiler_config.profiling_overhead_limit);

	profiler_performance_stats_t perf_stats;
	Profiler_GetPerformanceStats(&perf_stats);
	if (perf_stats.frame_count > 0) {
                Com_Printf("Frames: %lu, FPS: %.1f\n", (unsigned long)perf_stats.frame_count, perf_stats.average_fps);
	}
}

static void Profiler_Dump_f(void) {
	Profiler_DumpCurrentFrame();
}

static void Profiler_Reset_f(void) {
	Profiler_ResetStats();
	Com_Printf("Profiler statistics reset\n");
}

static void Profiler_Export_f(void) {
	char filename[256];
	if (Cmd_Argc() < 2) {
                Com_sprintf(filename, sizeof(filename), "profiler_export_%lu.txt", (unsigned long)Profiler_GetTimestamp());
	} else {
		Com_sprintf(filename, sizeof(filename), "%s", Cmd_Argv(1));
	}

	if (Profiler_ExportToFile(filename)) {
		Com_Printf("Profiler data exported to %s\n", filename);
	} else {
		Com_Printf("Failed to export profiler data\n");
	}
}

static void Profiler_ExportJSON_f(void) {
	char filename[256];
	if (Cmd_Argc() < 2) {
                Com_sprintf(filename, sizeof(filename), "profiler_export_%lu.json", (unsigned long)Profiler_GetTimestamp());
	} else {
		Com_sprintf(filename, sizeof(filename), "%s", Cmd_Argv(1));
	}

	if (Profiler_ExportToJSON(filename)) {
		Com_Printf("Profiler data exported to %s\n", filename);
	} else {
		Com_Printf("Failed to export profiler data\n");
	}
}

static void Profiler_ExportCSV_f(void) {
	char filename[256];
	if (Cmd_Argc() < 2) {
                Com_sprintf(filename, sizeof(filename), "profiler_export_%lu.csv", (unsigned long)Profiler_GetTimestamp());
	} else {
		Com_sprintf(filename, sizeof(filename), "%s", Cmd_Argv(1));
	}

	if (Profiler_ExportToCSV(filename)) {
		Com_Printf("Profiler data exported to %s\n", filename);
	} else {
		Com_Printf("Failed to export profiler data\n");
	}
}

static void Profiler_IncreaseDetail_f(void) {
	Profiler_IncreaseDetail();
}

static void Profiler_DecreaseDetail_f(void) {
	Profiler_DecreaseDetail();
}

/*
===============
Profiler_ModeChanged
===============
*/
static void __attribute__((unused)) Profiler_ModeChanged(void) {
	cvar_t *mode_cvar = Cvar_Get("profiler_mode", "0", CVAR_ARCHIVE);
	int new_mode = mode_cvar->integer;

	if (new_mode < 0 || new_mode > 4) {
		Com_Printf("Invalid profiler mode %d, resetting to disabled\n", new_mode);
		Cvar_Set("profiler_mode", "0");
		return;
	}

	if ((int)profiler_config.mode != new_mode) {
		profiler_config_t new_config = profiler_config;
		new_config.mode = (profiler_mode_t)new_mode;

		// Update overhead limit if CVAR changed
		cvar_t *overhead_cvar = Cvar_Get("profiler_overhead_limit", "5.0", CVAR_ARCHIVE);
		new_config.profiling_overhead_limit = overhead_cvar->value;

		Com_Printf("Profiler mode changed from %s to %s\n",
			Profiler_GetModeString(profiler_config.mode),
			Profiler_GetModeString(new_config.mode));

		Profiler_Shutdown();
		Profiler_Init(&new_config);
	}
}

/*
===============
Profiler_OverheadLimitChanged
===============
*/
static void __attribute__((unused)) Profiler_OverheadLimitChanged(void) {
	cvar_t *overhead_cvar = Cvar_Get("profiler_overhead_limit", "5.0", CVAR_ARCHIVE);
	profiler_config.profiling_overhead_limit = overhead_cvar->value;
}

// Global profiler state
profiler_config_t profiler_config = {
	.mode = PROFILER_MODE_DISABLED,
	.detailed_gpu_profiling = qfalse,
	.memory_profiling = qfalse,
	.cache_profiling = qfalse,
	.benchmark_profiling = qfalse,
	.profiling_overhead_limit = 5.0f // 5% max overhead
};


// Performance stats tracking
static profiler_performance_stats_t performance_stats = {0};
static uint64_t last_frame_time = 0;
static uint64_t frame_start_time = 0;

// Memory stats tracking
static profiler_memory_stats_t memory_stats = {0};

// Forward declarations for renderer functions
#if PROFILER_VULKAN
extern void vk_print_render_profiler_stats(void);
extern void vk_print_memory_bandwidth_stats(void);
extern void vk_print_cache_performance_stats(void);
extern void vk_print_layout_optimization_recommendations(void);
#endif

/*
===============
Profiler_Init
===============
*/
qboolean Profiler_Init(const profiler_config_t* config) {
	if (!config) {
		return qfalse;
	}

	// Copy configuration
	profiler_config = *config;

	// Initialize Tracy if enabled
#if PROFILER_TRACY
	if (profiler_config.mode == PROFILER_MODE_BASIC ||
		profiler_config.mode == PROFILER_MODE_FULL) {
		Com_Printf("Profiler: Tracy profiling enabled\n");
		PROF_THREAD_NAME("Main Thread");
	}
#endif

	// Initialize Vulkan profiling
#if PROFILER_VULKAN
	if (profiler_config.mode == PROFILER_MODE_VULKAN ||
		profiler_config.mode == PROFILER_MODE_FULL) {
		Com_Printf("Profiler: Vulkan render profiling enabled\n");
		Profiler_Vulkan_Enable();
	}
#endif

	// Initialize benchmark profiling
#if PROFILER_BENCHMARK
	if (profiler_config.mode == PROFILER_MODE_BENCHMARK ||
		profiler_config.mode == PROFILER_MODE_FULL) {
		Com_Printf("Profiler: Performance benchmarking enabled\n");
		Profiler_Benchmark_Enable();
	}
#endif

	// Reset stats
	Profiler_ResetStats();

	Com_Printf("Profiler initialized in mode: %s\n",
		Profiler_GetModeString(profiler_config.mode));

	// Get CVARs and set initial mode
	cvar_t *mode_cvar = Cvar_Get("profiler_mode", "0", CVAR_ARCHIVE);
	cvar_t *overhead_cvar = Cvar_Get("profiler_overhead_limit", "5.0", CVAR_ARCHIVE);

	// Set initial mode from CVAR
	profiler_config.mode = (profiler_mode_t)mode_cvar->integer;
	profiler_config.profiling_overhead_limit = overhead_cvar->value;

	// Register console commands for profiling control
	Cmd_AddCommand("profiler_toggle", Profiler_Toggle_f);
	Cmd_AddCommand("profiler_status", Profiler_Status_f);
	Cmd_AddCommand("profiler_dump", Profiler_Dump_f);
	Cmd_AddCommand("profiler_reset", Profiler_Reset_f);
	Cmd_AddCommand("profiler_export", Profiler_Export_f);
	Cmd_AddCommand("profiler_export_json", Profiler_ExportJSON_f);
	Cmd_AddCommand("profiler_export_csv", Profiler_ExportCSV_f);
	Cmd_AddCommand("profiler_increase_detail", Profiler_IncreaseDetail_f);
	Cmd_AddCommand("profiler_decrease_detail", Profiler_DecreaseDetail_f);

	return qtrue;
}

/*
===============
Profiler_Shutdown
===============
*/
void Profiler_Shutdown(void) {
	Profiler_PrintStats();

	// Shutdown Vulkan profiling
	Profiler_Vulkan_Disable();

	// Shutdown benchmark profiling
	Profiler_Benchmark_Disable();

	// Unregister console commands
	Cmd_RemoveCommand("profiler_toggle");
	Cmd_RemoveCommand("profiler_status");
	Cmd_RemoveCommand("profiler_dump");
	Cmd_RemoveCommand("profiler_reset");
	Cmd_RemoveCommand("profiler_export");
	Cmd_RemoveCommand("profiler_export_json");
	Cmd_RemoveCommand("profiler_export_csv");
	Cmd_RemoveCommand("profiler_increase_detail");
	Cmd_RemoveCommand("profiler_decrease_detail");

	// Reset to disabled state
	profiler_config.mode = PROFILER_MODE_DISABLED;

	Com_Printf("Profiler shutdown complete\n");
}

/*
===============
Profiler_FrameBegin
===============
*/
void Profiler_FrameBegin(void) {
	if (!Profiler_IsEnabled()) {
		return;
	}

	frame_start_time = Profiler_GetTimestamp();

#if PROFILER_TRACY
	PROF_FRAME_MARK();
#endif

	PROF_FRAME_MARK();
}


/*
===============
Profiler_FrameEnd
===============
*/
void Profiler_FrameEnd(void) {
	if (!Profiler_IsEnabled()) {
		return;
	}

	uint64_t frame_end_time = Profiler_GetTimestamp();
	uint64_t frame_duration = frame_end_time - frame_start_time;

	// Convert to milliseconds
	double frame_time_ms = (double)frame_duration / 1000000.0; // Assuming nanoseconds

	// Update performance stats
	performance_stats.frame_count++;

	if (performance_stats.frame_count == 1) {
		performance_stats.min_frame_time = frame_time_ms;
		performance_stats.max_frame_time = frame_time_ms;
		performance_stats.average_frame_time = frame_time_ms;
	} else {
		performance_stats.min_frame_time = MIN(performance_stats.min_frame_time, frame_time_ms);
		performance_stats.max_frame_time = MAX(performance_stats.max_frame_time, frame_time_ms);
		performance_stats.average_frame_time =
			(performance_stats.average_frame_time * (performance_stats.frame_count - 1) + frame_time_ms) /
			performance_stats.frame_count;
	}

	// Calculate FPS
	if (last_frame_time != 0) {
		uint64_t frame_delta = frame_end_time - last_frame_time;
		if (frame_delta > 0) {
			double fps = 1000000000.0 / frame_delta; // Assuming nanoseconds
			if (performance_stats.frame_count == 1) {
				performance_stats.average_fps = fps;
				performance_stats.min_fps = fps;
				performance_stats.max_fps = fps;
			} else {
				performance_stats.min_fps = MIN(performance_stats.min_fps, fps);
				performance_stats.max_fps = MAX(performance_stats.max_fps, fps);
				performance_stats.average_fps =
					(performance_stats.average_fps * (performance_stats.frame_count - 1) + fps) /
					performance_stats.frame_count;
			}
		}
	}

	last_frame_time = frame_end_time;
}

/*
===============
Profiler_PrintStats
===============
*/
void Profiler_PrintStats(void) {
	Com_Printf("\n=== Profiler Statistics ===\n");
	Com_Printf("Mode: %s\n", Profiler_GetModeString(profiler_config.mode));

	if (performance_stats.frame_count > 0) {
		Com_Printf("Performance Stats:\n");
		Com_Printf("  Frames: %lu\n", (unsigned long)performance_stats.frame_count);
		Com_Printf("  FPS: %.1f avg, %.1f min, %.1f max\n",
			performance_stats.average_fps,
			performance_stats.min_fps,
			performance_stats.max_fps);
		Com_Printf("  Frame Time: %.2f ms avg, %.2f ms min, %.2f ms max\n",
			performance_stats.average_frame_time,
			performance_stats.min_frame_time,
			performance_stats.max_frame_time);
		Com_Printf("  Frames Dropped: %lu\n", (unsigned long)performance_stats.total_frames_dropped);
	}

	if (memory_stats.total_allocated > 0) {
		Com_Printf("Memory Stats:\n");
		Com_Printf("  Current: %zu bytes\n", memory_stats.current_allocation);
		Com_Printf("  Peak: %zu bytes\n", memory_stats.peak_allocation);
		Com_Printf("  Total Allocated: %zu bytes\n", memory_stats.total_allocated);
		Com_Printf("  Total Freed: %zu bytes\n", memory_stats.total_freed);
		Com_Printf("  Allocation Count: %u\n", memory_stats.allocation_count);
		Com_Printf("  Free Count: %u\n", memory_stats.free_count);
		Com_Printf("  Fragmentation: %.2f%%\n", memory_stats.fragmentation_ratio * 100.0f);
	}

#if PROFILER_VULKAN
	if (Profiler_Vulkan_IsEnabled()) {
		Com_Printf("\nVulkan Render Profiling:\n");
		vk_print_render_profiler_stats();
		Com_Printf("\nMemory Bandwidth Profiling:\n");
		vk_print_memory_bandwidth_stats();
		Com_Printf("\nCache Performance Profiling:\n");
		vk_print_cache_performance_stats();
	}
#endif

	Com_Printf("=== End Profiler Statistics ===\n\n");
}

/*
===============
Profiler_ResetStats
===============
*/
void Profiler_ResetStats(void) {
	performance_stats.frame_count = 0;
	performance_stats.average_fps = 0.0;
	performance_stats.min_fps = 999.0;
	performance_stats.max_fps = 0.0;
	performance_stats.average_frame_time = 0.0;
	performance_stats.min_frame_time = 999.0;
	performance_stats.max_frame_time = 0.0;
	performance_stats.total_frames_dropped = 0;

	memory_stats.current_allocation = 0;
	memory_stats.peak_allocation = 0;
	memory_stats.total_allocated = 0;
	memory_stats.total_freed = 0;
	memory_stats.allocation_count = 0;
	memory_stats.free_count = 0;
	memory_stats.fragmentation_ratio = 0.0f;

	last_frame_time = 0;
	frame_start_time = 0;
}

/*
===============
Profiler_IsEnabled
===============
*/
qboolean Profiler_IsEnabled(void) {
	return profiler_config.mode != PROFILER_MODE_DISABLED;
}

/*
===============
Profiler_GetMode
===============
*/
profiler_mode_t Profiler_GetMode(void) {
	return profiler_config.mode;
}

/*
===============
Profiler_Vulkan_Enable
===============
*/
void Profiler_Vulkan_Enable(void) {
#if PROFILER_VULKAN
	// Vulkan profiler is controlled by renderer initialization
	profiler_config.detailed_gpu_profiling = qtrue;
#endif
}

/*
===============
Profiler_Vulkan_Disable
===============
*/
void Profiler_Vulkan_Disable(void) {
#if PROFILER_VULKAN
	profiler_config.detailed_gpu_profiling = qfalse;
#endif
}

/*
===============
Profiler_Vulkan_IsEnabled
===============
*/
qboolean Profiler_Vulkan_IsEnabled(void) {
#if PROFILER_VULKAN
	return profiler_config.detailed_gpu_profiling;
#else
	return qfalse;
#endif
}

/*
===============
Profiler_Benchmark_Enable
===============
*/
void Profiler_Benchmark_Enable(void) {
#if PROFILER_BENCHMARK
	profiler_config.benchmark_profiling = qtrue;
	// Benchmark_Init(); // Not implemented yet
#endif
}

/*
===============
Profiler_Benchmark_Disable
===============
*/
void Profiler_Benchmark_Disable(void) {
#if PROFILER_BENCHMARK
	profiler_config.benchmark_profiling = qfalse;
	// Benchmark_Shutdown(); // Not implemented yet
#endif
}

/*
===============
Profiler_Benchmark_IsEnabled
===============
*/
qboolean Profiler_Benchmark_IsEnabled(void) {
#if PROFILER_BENCHMARK
	return profiler_config.benchmark_profiling;
#else
	return qfalse;
#endif
}

/*
===============
Profiler_GetModeString
===============
*/
const char* Profiler_GetModeString(profiler_mode_t mode) {
	switch (mode) {
		case PROFILER_MODE_DISABLED: return "Disabled";
		case PROFILER_MODE_BASIC: return "Basic (Tracy)";
		case PROFILER_MODE_VULKAN: return "Vulkan Render";
		case PROFILER_MODE_FULL: return "Full Profiling";
		case PROFILER_MODE_BENCHMARK: return "Benchmark";
		default: return "Unknown";
	}
}

/*
===============
Profiler_SetThreadName
===============
*/
void Profiler_SetThreadName(const char* name) {
	PROF_THREAD_NAME(name);
}

/*
===============
Profiler_GetTimestamp
===============
*/
uint64_t Profiler_GetTimestamp(void) {
#if PROFILER_TRACY
	return TracyCGetTime();
#else
	// Use engine's millisecond timer, convert to nanoseconds for compatibility
	return (uint64_t)Sys_Milliseconds() * 1000000ULL;
#endif
}

/*
===============
Profiler_GetElapsedTime
===============
*/
double Profiler_GetElapsedTime(uint64_t start, uint64_t end) {
	uint64_t delta = end - start;
	return (double)delta / 1000000.0; // Convert to milliseconds
}

/*
===============
Profiler_GetPerformanceStats
===============
*/
void Profiler_GetPerformanceStats(profiler_performance_stats_t* stats) {
	if (stats) {
		*stats = performance_stats;
	}
}

/*
===============
Profiler_GetMemoryStats
===============
*/
void Profiler_GetMemoryStats(profiler_memory_stats_t* stats) {
	if (stats) {
		*stats = memory_stats;
	}
}

/*
===============
Profiler_ExportToFile
===============
*/
qboolean Profiler_ExportToFile(const char* filename) {
	FILE* f = fopen(filename, "w");
	if (!f) {
		return qfalse;
	}

	fprintf(f, "=== Profiler Export ===\n");
	fprintf(f, "Mode: %s\n", Profiler_GetModeString(profiler_config.mode));
	fprintf(f, "Timestamp: %lu\n", (unsigned long)Profiler_GetTimestamp());

	if (performance_stats.frame_count > 0) {
		fprintf(f, "\nPerformance Stats:\n");
		fprintf(f, "Frames: %lu\n", (unsigned long)performance_stats.frame_count);
		fprintf(f, "FPS: %.1f avg, %.1f min, %.1f max\n",
			performance_stats.average_fps,
			performance_stats.min_fps,
			performance_stats.max_fps);
		fprintf(f, "Frame Time: %.2f ms avg, %.2f min, %.2f max\n",
			performance_stats.average_frame_time,
			performance_stats.min_frame_time,
			performance_stats.max_frame_time);
	}

	fclose(f);
	return qtrue;
}

/*
===============
Profiler_ExportToJSON
===============
*/
qboolean Profiler_ExportToJSON(const char* filename) {
	FILE* f = fopen(filename, "w");
	if (!f) {
		return qfalse;
	}

	fprintf(f, "{\n");
	fprintf(f, "  \"mode\": \"%s\",\n", Profiler_GetModeString(profiler_config.mode));
    fprintf(f, "  \"timestamp\": %lu,\n", (unsigned long)Profiler_GetTimestamp());

	if (performance_stats.frame_count > 0) {
		fprintf(f, "  \"performance\": {\n");
				fprintf(f, "    \"frame_count\": %lu,\n", (unsigned long)performance_stats.frame_count);
		fprintf(f, "    \"fps_average\": %.1f,\n", performance_stats.average_fps);
		fprintf(f, "    \"fps_min\": %.1f,\n", performance_stats.min_fps);
		fprintf(f, "    \"fps_max\": %.1f,\n", performance_stats.max_fps);
		fprintf(f, "    \"frame_time_average_ms\": %.2f,\n", performance_stats.average_frame_time);
		fprintf(f, "    \"frame_time_min_ms\": %.2f,\n", performance_stats.min_frame_time);
		fprintf(f, "    \"frame_time_max_ms\": %.2f\n", performance_stats.max_frame_time);
		fprintf(f, "  },\n");
	}

	if (memory_stats.total_allocated > 0) {
		fprintf(f, "  \"memory\": {\n");
		fprintf(f, "    \"current_allocation\": %zu,\n", memory_stats.current_allocation);
		fprintf(f, "    \"peak_allocation\": %zu,\n", memory_stats.peak_allocation);
		fprintf(f, "    \"total_allocated\": %zu,\n", memory_stats.total_allocated);
		fprintf(f, "    \"total_freed\": %zu,\n", memory_stats.total_freed);
		fprintf(f, "    \"allocation_count\": %u,\n", memory_stats.allocation_count);
		fprintf(f, "    \"free_count\": %u,\n", memory_stats.free_count);
		fprintf(f, "    \"fragmentation_ratio\": %.2f\n", memory_stats.fragmentation_ratio);
		fprintf(f, "  }\n");
	} else {
		fprintf(f, "  \"memory\": null\n");
	}

	fprintf(f, "}\n");
	fclose(f);
	return qtrue;
}

/*
===============
Profiler_ExportToCSV
===============
*/
qboolean Profiler_ExportToCSV(const char* filename) {
	FILE* f = fopen(filename, "w");
	if (!f) {
		return qfalse;
	}

	// Header
	fprintf(f, "metric,value,unit\n");

	// Performance metrics
	if (performance_stats.frame_count > 0) {
				fprintf(f, "frame_count,%lu,count\n", (unsigned long)performance_stats.frame_count);
		fprintf(f, "fps_average,%.1f,fps\n", performance_stats.average_fps);
		fprintf(f, "fps_min,%.1f,fps\n", performance_stats.min_fps);
		fprintf(f, "fps_max,%.1f,fps\n", performance_stats.max_fps);
		fprintf(f, "frame_time_average,%.2f,ms\n", performance_stats.average_frame_time);
		fprintf(f, "frame_time_min,%.2f,ms\n", performance_stats.min_frame_time);
		fprintf(f, "frame_time_max,%.2f,ms\n", performance_stats.max_frame_time);
	}

	// Memory metrics
	if (memory_stats.total_allocated > 0) {
		fprintf(f, "memory_current,%zu,bytes\n", memory_stats.current_allocation);
		fprintf(f, "memory_peak,%zu,bytes\n", memory_stats.peak_allocation);
		fprintf(f, "memory_total_allocated,%zu,bytes\n", memory_stats.total_allocated);
		fprintf(f, "memory_total_freed,%zu,bytes\n", memory_stats.total_freed);
		fprintf(f, "allocation_count,%u,count\n", memory_stats.allocation_count);
		fprintf(f, "free_count,%u,count\n", memory_stats.free_count);
		fprintf(f, "fragmentation_ratio,%.2f,ratio\n", memory_stats.fragmentation_ratio);
	}

	fclose(f);
	return qtrue;
}

/*
===============
Profiler_ToggleMode
===============
*/
void Profiler_ToggleMode(void) {
	profiler_mode_t next_mode = (profiler_mode_t)((profiler_config.mode + 1) % 5);
	profiler_config_t new_config = profiler_config;
	new_config.mode = next_mode;

	Profiler_Shutdown();
	Profiler_Init(&new_config);

	Com_Printf("Profiler mode changed to: %s\n", Profiler_GetModeString(new_config.mode));
}

/*
===============
Profiler_IncreaseDetail
===============
*/
void Profiler_IncreaseDetail(void) {
	if (profiler_config.profiling_overhead_limit < 20.0f) {
		profiler_config.profiling_overhead_limit += 2.5f;
		Com_Printf("Profiler overhead limit increased to: %.1f%%\n",
			profiler_config.profiling_overhead_limit);
	}
}

/*
===============
Profiler_DecreaseDetail
===============
*/
void Profiler_DecreaseDetail(void) {
	if (profiler_config.profiling_overhead_limit > 0.5f) {
		profiler_config.profiling_overhead_limit -= 2.5f;
		Com_Printf("Profiler overhead limit decreased to: %.1f%%\n",
			profiler_config.profiling_overhead_limit);
	}
}

/*
===============
Profiler_DumpCurrentFrame
===============
*/
void Profiler_DumpCurrentFrame(void) {
	Com_Printf("=== Current Frame Profiler Dump ===\n");

	if (performance_stats.frame_count > 0) {
				Com_Printf("Frame: %lu\n", (unsigned long)performance_stats.frame_count);
		Com_Printf("Frame Time: %.2f ms\n", performance_stats.average_frame_time);
		Com_Printf("FPS: %.1f\n", performance_stats.average_fps);
	}

#if PROFILER_VULKAN
	if (Profiler_Vulkan_IsEnabled()) {
		Com_Printf("Vulkan Profiling:\n");
		vk_print_render_profiler_stats();
	}
#endif

	Com_Printf("=== End Current Frame Dump ===\n");
}