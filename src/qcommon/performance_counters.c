/*
===========================================================================
Performance Counters Implementation
===========================================================================
*/

#include "performance_counters.h"
#ifndef STANDALONE_TEST
#include "qcommon.h"
#endif

#ifdef STANDALONE_TEST
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <limits.h>
#include <time.h>

// Stub implementations for standalone testing
void Com_Memset(void *dest, int value, size_t size) {
    memset(dest, value, size);
}

int Sys_Milliseconds(void) {
    static int baseTime = 0;
    if (baseTime == 0) {
        baseTime = (int)time(NULL) * 1000;
    }
    return baseTime + (int)(clock() * 1000 / CLOCKS_PER_SEC);
}

void Com_sprintf(char *buffer, int bufferSize, const char *fmt, ...) {
    va_list argptr;
    va_start(argptr, fmt);
    vsnprintf(buffer, bufferSize, fmt, argptr);
    va_end(argptr);
}

void Com_Printf(const char *fmt, ...) {
    va_list argptr;
    va_start(argptr, fmt);
    vprintf(fmt, argptr);
    va_end(argptr);
}
#endif

// Global performance counters instance
performanceCounters_t perfCounters;

/*
================
Perf_Init
================
*/
void Perf_Init(void) {
	Com_Memset(&perfCounters, 0, sizeof(perfCounters));

	perfCounters.minFrameTime = 999.0f;
	perfCounters.minDrawCallsPerFrame = INT_MAX;
	perfCounters.lastFPSUpdate = Sys_Milliseconds();
}

/*
================
Perf_Frame
================
*/
void Perf_Frame(int frameTimeMs) {
	int currentTime = Sys_Milliseconds();

	// Update frame time
	perfCounters.currentFrameTime = frameTimeMs;

	// Update min/max frame times
	if (frameTimeMs < perfCounters.minFrameTime) {
		perfCounters.minFrameTime = frameTimeMs;
	}
	if (frameTimeMs > perfCounters.maxFrameTime) {
		perfCounters.maxFrameTime = frameTimeMs;
	}

	// Add to frame time history
	perfCounters.frameTimeHistory[perfCounters.frameTimeHistoryIndex] = frameTimeMs;
	perfCounters.frameTimeHistoryIndex = (perfCounters.frameTimeHistoryIndex + 1) % PERF_HISTORY_SIZE;
	if (perfCounters.frameTimeHistoryCount < PERF_HISTORY_SIZE) {
		perfCounters.frameTimeHistoryCount++;
	}

	// Calculate average frame time
	float totalFrameTime = 0.0f;
	for (int i = 0; i < perfCounters.frameTimeHistoryCount; i++) {
		totalFrameTime += perfCounters.frameTimeHistory[i];
	}
	perfCounters.averageFrameTime = totalFrameTime / perfCounters.frameTimeHistoryCount;

	// Update FPS
	perfCounters.frameCount++;
	int timeSinceLastUpdate = currentTime - perfCounters.lastFPSUpdate;

	if (timeSinceLastUpdate >= 1000) { // Update FPS once per second
		perfCounters.currentFPS = (float)perfCounters.frameCount / (timeSinceLastUpdate / 1000.0f);
		perfCounters.frameCount = 0;
		perfCounters.lastFPSUpdate = currentTime;

		// Update average FPS (simple moving average)
		if (perfCounters.averageFPS == 0.0f) {
			perfCounters.averageFPS = perfCounters.currentFPS;
		} else {
			perfCounters.averageFPS = (perfCounters.averageFPS * 0.9f) + (perfCounters.currentFPS * 0.1f);
		}
	}

	// Update draw call history
	perfCounters.drawCallHistory[perfCounters.drawCallHistoryIndex] = perfCounters.drawCallsThisFrame;
	perfCounters.drawCallHistoryIndex = (perfCounters.drawCallHistoryIndex + 1) % PERF_HISTORY_SIZE;
	if (perfCounters.drawCallHistoryCount < PERF_HISTORY_SIZE) {
		perfCounters.drawCallHistoryCount++;
	}

	// Update draw call stats
	if (perfCounters.drawCallsThisFrame < perfCounters.minDrawCallsPerFrame) {
		perfCounters.minDrawCallsPerFrame = perfCounters.drawCallsThisFrame;
	}
	if (perfCounters.drawCallsThisFrame > perfCounters.maxDrawCallsPerFrame) {
		perfCounters.maxDrawCallsPerFrame = perfCounters.drawCallsThisFrame;
	}

	// Calculate average draw calls
	int totalDrawCalls = 0;
	for (int i = 0; i < perfCounters.drawCallHistoryCount; i++) {
		totalDrawCalls += perfCounters.drawCallHistory[i];
	}
	perfCounters.averageDrawCallsPerFrame = (float)totalDrawCalls / perfCounters.drawCallHistoryCount;

	perfCounters.totalDrawCalls += perfCounters.drawCallsThisFrame;
}

/*
================
Perf_CountDrawCall
================
*/
void Perf_CountDrawCall(void) {
	perfCounters.drawCallsThisFrame++;
}

/*
================
Perf_ResetFrameCounters
================
*/
void Perf_ResetFrameCounters(void) {
	perfCounters.drawCallsThisFrame = 0;
}

/*
================
Perf_UpdateGPUTiming
================
*/
void Perf_UpdateGPUTiming(float gpuFrameTimeMs) {
	if (gpuFrameTimeMs > 0.0f) {
		perfCounters.gpuFrameTime = gpuFrameTimeMs;
		perfCounters.gpuTimingAvailable = qtrue;
	} else {
		perfCounters.gpuTimingAvailable = qfalse;
	}
}

/*
================
Perf_GetInfoString
================
*/
void Perf_GetInfoString(char *buffer, int bufferSize) {
	if (perfCounters.gpuTimingAvailable) {
		Com_sprintf(buffer, bufferSize,
			"FPS: %.1f (avg: %.1f)\n"
			"CPU Frame Time: %.1fms (avg: %.1fms, min: %.1fms, max: %.1fms)\n"
			"GPU Frame Time: %.1fms\n"
			"Draw Calls: %d (avg: %.1f, min: %d, max: %d, total: %d)\n",
			perfCounters.currentFPS,
			perfCounters.averageFPS,
			perfCounters.currentFrameTime,
			perfCounters.averageFrameTime,
			perfCounters.minFrameTime,
			perfCounters.maxFrameTime,
			perfCounters.gpuFrameTime,
			perfCounters.drawCallsThisFrame,
			perfCounters.averageDrawCallsPerFrame,
			perfCounters.minDrawCallsPerFrame,
			perfCounters.maxDrawCallsPerFrame,
			perfCounters.totalDrawCalls
		);
	} else {
		Com_sprintf(buffer, bufferSize,
			"FPS: %.1f (avg: %.1f)\n"
			"Frame Time: %.1fms (avg: %.1fms, min: %.1fms, max: %.1fms)\n"
			"Draw Calls: %d (avg: %.1f, min: %d, max: %d, total: %d)\n",
			perfCounters.currentFPS,
			perfCounters.averageFPS,
			perfCounters.currentFrameTime,
			perfCounters.averageFrameTime,
			perfCounters.minFrameTime,
			perfCounters.maxFrameTime,
			perfCounters.drawCallsThisFrame,
			perfCounters.averageDrawCallsPerFrame,
			perfCounters.minDrawCallsPerFrame,
			perfCounters.maxDrawCallsPerFrame,
			perfCounters.totalDrawCalls
		);
	}
}

/*
================
Perf_DisplayInfo_f
================
*/
void Perf_DisplayInfo_f(void) {
	char info[1024];
	Perf_GetInfoString(info, sizeof(info));
	Com_Printf("Performance Counters:\n%s\n", info);
}