/*
===========================================================================
Comprehensive Engine Profiling System

Integrates multiple profiling backends:
- Tracy-based CPU/GPU profiling (when USE_TRACY=1)
- Vulkan render graph profiling
- Memory bandwidth profiling
- Performance benchmarking framework
- Custom high-level profiling macros
===========================================================================
*/

#ifndef __PROFILER_H__
#define __PROFILER_H__

#include "q_shared.h"

// Detect TracyC.h availability when USE_TRACY is enabled
#if defined(USE_TRACY) && defined(__has_include)
#	if __has_include("TracyC.h")
#		include "TracyC.h"
#		define PROFILER_TRACY 1
#	else
#		define PROFILER_TRACY 0
#	endif
#elif defined(USE_TRACY)
#	define PROFILER_TRACY 0
#else
#	define PROFILER_TRACY 0
#endif

// Vulkan profiler availability - temporarily disabled due to build issues
#define PROFILER_VULKAN 0

// Performance benchmark integration
#define PROFILER_BENCHMARK 1

// Combined profiling enabled flag
#define PROFILER_ENABLED (PROFILER_TRACY || PROFILER_VULKAN || PROFILER_BENCHMARK)

// Tracy-based profiling macros (when available)
#if PROFILER_TRACY
#	define PROF_ENABLED 1
#	define PROF_THREAD_NAME(name) TracyCSetThreadName(name)
#	define PROF_FRAME_MARK() TracyCFrameMark
#	define PROF_ZONE_BEGIN(ctx, name) TracyCZoneN(ctx, name, 1)
#	define PROF_ZONE_BEGIN_COLOR(ctx, name, color) TracyCZoneNC(ctx, name, color, 1)
#	define PROF_ZONE_END(ctx) TracyCZoneEnd(ctx)
#	define PROF_VALUE(name, value) TracyCPlot(name, value)
#	define PROF_MESSAGE(text) TracyCMessage(text, strlen(text))
#	define PROF_ALLOC(ptr, size) TracyCAlloc(ptr, size)
#	define PROF_FREE(ptr) TracyCFree(ptr)
#else
	typedef void *TracyCZoneCtx;
#	define PROF_ENABLED 0
#	define PROF_THREAD_NAME(name) ((void)(name))
#	define PROF_FRAME_MARK() ((void)0)
#	define PROF_ZONE_BEGIN(ctx, name) ((void)(ctx))
#	define PROF_ZONE_BEGIN_COLOR(ctx, name, color) ((void)(ctx))
#	define PROF_ZONE_END(ctx) ((void)(ctx))
#	define PROF_VALUE(name, value) ((void)(name), (void)(value))
#	define PROF_MESSAGE(text) ((void)(text))
#	define PROF_ALLOC(ptr, size) ((void)(ptr), (void)(size))
#	define PROF_FREE(ptr) ((void)(ptr))
#endif

// High-level profiling macros for common engine operations
#define PROF_SCOPE(name) \
	PROF_ZONE_BEGIN(__prof_ctx, name); \
	PROF_ZONE_END(__prof_ctx)

#define PROF_SCOPE_COLOR(name, color) \
	PROF_ZONE_BEGIN_COLOR(__prof_ctx, name, color); \
	PROF_DEFER(PROF_ZONE_END(__prof_ctx))

#define PROF_FUNCTION() PROF_SCOPE(__FUNCTION__)
#define PROF_RENDER_BEGIN() PROF_SCOPE("Render Frame")
#define PROF_RENDER_END() PROF_ZONE_END(__prof_ctx)
#define PROF_UPDATE_BEGIN() PROF_SCOPE("Game Update")
#define PROF_UPDATE_END() PROF_ZONE_END(__prof_ctx)
#define PROF_PHYSICS_BEGIN() PROF_SCOPE("Physics")
#define PROF_PHYSICS_END() PROF_ZONE_END(__prof_ctx)
#define PROF_AI_BEGIN() PROF_SCOPE("AI")
#define PROF_AI_END() PROF_ZONE_END(__prof_ctx)

// Vulkan-specific profiling integration
#if PROFILER_VULKAN
// Vulkan render profiler functions
void vk_print_render_profiler_stats(void);
void vk_print_memory_bandwidth_stats(void);
void vk_print_cache_performance_stats(void);
void vk_print_layout_optimization_recommendations(void);

// Vulkan profiling macros
#define PROF_VULKAN_FRAME_BEGIN() PROF_SCOPE("Vulkan Frame")
#define PROF_VULKAN_FRAME_END() PROF_ZONE_END(__prof_ctx)
#define PROF_VULKAN_PASS_BEGIN(pass_name) PROF_SCOPE_COLOR(pass_name, 0xFF4444)
#define PROF_VULKAN_PASS_END() PROF_ZONE_END(__prof_ctx)
#define PROF_VULKAN_DRAW_BEGIN() PROF_SCOPE("Draw Call")
#define PROF_VULKAN_DRAW_END() PROF_ZONE_END(__prof_ctx)
#endif

// Performance benchmark integration (only when not in test mode)
#ifndef PROFILER_NO_BENCHMARK
#if PROFILER_BENCHMARK
#include "performance_benchmark.h"

// Benchmark profiling macros
#define PROF_BENCH_BEGIN(benchmark_id, category) \
	PROF_SCOPE("Benchmark: " benchmark_id); \
	Benchmark_StartMeasurement(benchmark_id, category)

#define PROF_BENCH_END(benchmark_id) \
	Benchmark_EndMeasurement(benchmark_id); \
	PROF_ZONE_END(__prof_ctx)

#define PROF_BENCH_VALUE(metric, value) \
	Benchmark_RecordMetric(metric, value)
#endif
#endif

// Profiling system initialization and control
typedef enum {
	PROFILER_MODE_DISABLED = 0,
	PROFILER_MODE_BASIC,        // Tracy only
	PROFILER_MODE_VULKAN,       // Vulkan render profiling
	PROFILER_MODE_FULL,         // All profilers enabled
	PROFILER_MODE_BENCHMARK     // Benchmark-focused profiling
} profiler_mode_t;

typedef struct {
	profiler_mode_t mode;
	qboolean detailed_gpu_profiling;
	qboolean memory_profiling;
	qboolean cache_profiling;
	qboolean benchmark_profiling;
	float profiling_overhead_limit; // Max acceptable overhead percentage
} profiler_config_t;

// Global profiler state
extern profiler_config_t profiler_config;

// Profiler system API
qboolean Profiler_Init(const profiler_config_t* config);
void Profiler_Shutdown(void);
void Profiler_FrameBegin(void);
void Profiler_FrameEnd(void);
void Profiler_PrintStats(void);
void Profiler_ResetStats(void);
qboolean Profiler_IsEnabled(void);
profiler_mode_t Profiler_GetMode(void);

// Vulkan profiler control
void Profiler_Vulkan_Enable(void);
void Profiler_Vulkan_Disable(void);
qboolean Profiler_Vulkan_IsEnabled(void);

// Benchmark profiler control
void Profiler_Benchmark_Enable(void);
void Profiler_Benchmark_Disable(void);
qboolean Profiler_Benchmark_IsEnabled(void);

// Utility functions
const char* Profiler_GetModeString(profiler_mode_t mode);
void Profiler_SetThreadName(const char* name);
uint64_t Profiler_GetTimestamp(void);
double Profiler_GetElapsedTime(uint64_t start, uint64_t end);

// Performance counter integration
typedef struct {
	uint64_t frame_count;
	double average_fps;
	double min_fps;
	double max_fps;
	double average_frame_time;
	double min_frame_time;
	double max_frame_time;
	uint64_t total_frames_dropped;
} profiler_performance_stats_t;

void Profiler_GetPerformanceStats(profiler_performance_stats_t* stats);

// Memory profiling integration
typedef struct {
	size_t current_allocation;
	size_t peak_allocation;
	size_t total_allocated;
	size_t total_freed;
	uint32_t allocation_count;
	uint32_t free_count;
	float fragmentation_ratio;
} profiler_memory_stats_t;

void Profiler_GetMemoryStats(profiler_memory_stats_t* stats);

// Export profiling data
qboolean Profiler_ExportToFile(const char* filename);
qboolean Profiler_ExportToJSON(const char* filename);
qboolean Profiler_ExportToCSV(const char* filename);

// Real-time profiling controls (for development/debugging)
void Profiler_ToggleMode(void);
void Profiler_IncreaseDetail(void);
void Profiler_DecreaseDetail(void);
void Profiler_DumpCurrentFrame(void);

#endif // __PROFILER_H__

