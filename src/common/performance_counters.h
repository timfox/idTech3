/*
===========================================================================
Performance Counters - FPS, frame times, draw calls
===========================================================================
*/

#ifndef __PERFORMANCE_COUNTERS_H__
#define __PERFORMANCE_COUNTERS_H__

#ifndef STANDALONE_TEST
#include "q_shared.h"
#include "thread_platform.h"
#else
// Minimal definitions for standalone testing
#include <stddef.h>
#include <stdint.h>
typedef uint8_t qboolean;
typedef int qhandle_t;
#define qfalse 0
#define qtrue 1

// Atomic stubs for standalone test
typedef volatile int atomic_int_t;
#define ATOMIC_INCREMENT(ptr) (*(ptr))++
#define ATOMIC_DECREMENT(ptr) (*(ptr))--
#define ATOMIC_ADD(ptr, val) (*(ptr)) += (val)
#define atomic_load_explicit(ptr, order) *(ptr)
#define atomic_store_explicit(ptr, val, order) *(ptr) = (val)

// Forward declarations
typedef enum {
	ERR_FATAL,
	ERR_DROP,
	ERR_SERVERDISCONNECT,
	ERR_DISCONNECT,
	ERR_NEED_CD
} errorParm_t;

// Stub function prototypes for standalone testing
void Com_Memset(void *dest, int value, size_t size);
int Sys_Milliseconds(void);
void Com_sprintf(char *buffer, int bufferSize, const char *fmt, ...);
void Com_Printf(const char *fmt, ...);
#endif

// Performance counter structure
typedef struct {
	// FPS calculation
	atomic_int_t frameCount;
	atomic_int_t lastFPSUpdate;
	float currentFPS;
	float averageFPS;

	// Frame time tracking (in milliseconds)
	float currentFrameTime;
	float minFrameTime;
	float maxFrameTime;
	float averageFrameTime;

	// Draw call counters
	atomic_int_t drawCallsThisFrame;
	atomic_int_t totalDrawCalls;
	int minDrawCallsPerFrame;
	int maxDrawCallsPerFrame;
	float averageDrawCallsPerFrame;

	// Timing history for averages
#define PERF_HISTORY_SIZE 60  // 60 frames for ~1 second at 60fps
	float frameTimeHistory[PERF_HISTORY_SIZE];
	int frameTimeHistoryIndex;
	int frameTimeHistoryCount;

	int drawCallHistory[PERF_HISTORY_SIZE];
	int drawCallHistoryIndex;
	int drawCallHistoryCount;

	// GPU timing (when available)
	float gpuFrameTime;
	qboolean gpuTimingAvailable;
} performanceCounters_t;

// Global performance counters
extern performanceCounters_t perfCounters;

// Initialize performance counters
void Perf_Init(void);

// Update counters each frame
void Perf_Frame(int frameTimeMs);

// Increment draw call counter
void Perf_CountDrawCall(void);

// Reset per-frame counters
void Perf_ResetFrameCounters(void);

// Get formatted performance info string
void Perf_GetInfoString(char *buffer, int bufferSize);

// Update GPU timing (called by renderer)
void Perf_UpdateGPUTiming(float gpuFrameTimeMs);

// Console command to display performance info
void Perf_DisplayInfo_f(void);

#endif // __PERFORMANCE_COUNTERS_H__
// Stability monitoring structures
typedef struct {
    // Stability metrics
    float stability_score;        // 0-100 based on frame time variance
    float reliability_score;      // 0-100 based on crash frequency
    int consecutive_stable_frames;
    int total_frame_drops;
    float average_frame_time_variance;

    // Anomaly detection
    qboolean anomaly_detected;
    char last_anomaly_description[256];
    int anomaly_count;

    // System health
    float memory_usage_percent;
    float cpu_usage_percent;
    int thread_count;
    qboolean system_overloaded;

    // Recovery suggestions
    char recovery_suggestions[512];
} stability_metrics_t;

// Enhanced performance monitoring functions
const stability_metrics_t* Perf_GetStabilityMetrics(void);
void Perf_GetStabilityReport(char *buffer, int bufferSize);
