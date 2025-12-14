/*
===========================================================================
Performance Counters - FPS, frame times, draw calls
===========================================================================
*/

#ifndef __PERFORMANCE_COUNTERS_H__
#define __PERFORMANCE_COUNTERS_H__

#ifndef STANDALONE_TEST
#include "q_shared.h"
#else
// Minimal definitions for standalone testing
#include <stdint.h>
typedef uint8_t qboolean;
typedef int qhandle_t;
#define qfalse 0
#define qtrue 1

// Forward declarations
typedef enum {
	ERR_FATAL,
	ERR_DROP,
	ERR_SERVERDISCONNECT,
	ERR_DISCONNECT,
	ERR_NEED_CD
} errorParm_t;
#endif

// Performance counter structure
typedef struct {
	// FPS calculation
	int frameCount;
	int lastFPSUpdate;
	float currentFPS;
	float averageFPS;

	// Frame time tracking (in milliseconds)
	float currentFrameTime;
	float minFrameTime;
	float maxFrameTime;
	float averageFrameTime;

	// Draw call counters
	int drawCallsThisFrame;
	int totalDrawCalls;
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

// Console command to display performance info
void Perf_DisplayInfo_f(void);

#endif // __PERFORMANCE_COUNTERS_H__