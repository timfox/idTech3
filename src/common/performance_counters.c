/*
===========================================================================
Performance Counters Implementation
===========================================================================
*/

#include "performance_counters.h"
#include "qcommon.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <limits.h>

// Global performance counters instance
performanceCounters_t perfCounters;

// Advanced performance monitoring cvars (reserved for future implementation)
static cvar_t *perf_detailed_gpu;
static cvar_t *perf_memory_tracking;
static cvar_t *perf_frame_analysis;
static cvar_t *perf_csv_output;
static cvar_t *perf_alert_threshold;
static cvar_t *perf_regression_detection;
static cvar_t *perf_baseline_frames;
static cvar_t *perf_regression_threshold;
static cvar_t *perf_auto_adjust;

/*
================
Perf_Init
================
*/
void Perf_Init(void) {
	memset(&perfCounters, 0, sizeof(perfCounters));

	perfCounters.minFrameTime = 999.0f;
	perfCounters.minDrawCallsPerFrame = INT_MAX;

	// Initialize advanced performance monitoring cvars
	perf_detailed_gpu = Cvar_Get("perf_detailed_gpu", "0", CVAR_ARCHIVE);
	Cvar_SetDescription(perf_detailed_gpu, "Enable detailed GPU performance monitoring");

	perf_memory_tracking = Cvar_Get("perf_memory_tracking", "0", CVAR_ARCHIVE);
	Cvar_SetDescription(perf_memory_tracking, "Enable detailed memory usage tracking");

	perf_frame_analysis = Cvar_Get("perf_frame_analysis", "0", CVAR_ARCHIVE);
	Cvar_SetDescription(perf_frame_analysis, "Enable per-frame performance analysis");

	perf_csv_output = Cvar_Get("perf_csv_output", "0", CVAR_ARCHIVE);
	Cvar_SetDescription(perf_csv_output, "Export performance data to CSV files");

	perf_alert_threshold = Cvar_Get("perf_alert_threshold", "16.67", CVAR_ARCHIVE);
	Cvar_SetDescription(perf_alert_threshold, "Frame time threshold for performance alerts (ms)");

	perf_regression_detection = Cvar_Get("perf_regression_detection", "1", CVAR_ARCHIVE);
	Cvar_SetDescription(perf_regression_detection, "Enable automatic performance regression detection");

	perf_baseline_frames = Cvar_Get("perf_baseline_frames", "300", CVAR_ARCHIVE);
	Cvar_SetDescription(perf_baseline_frames, "Number of frames to establish performance baseline");

	perf_regression_threshold = Cvar_Get("perf_regression_threshold", "1.2", CVAR_ARCHIVE);
	Cvar_SetDescription(perf_regression_threshold, "Performance regression threshold multiplier");

	perf_auto_adjust = Cvar_Get("perf_auto_adjust", "0", CVAR_ARCHIVE);
	Cvar_SetDescription(perf_auto_adjust, "Automatically adjust quality settings on performance issues");

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
	perfCounters.currentFrameTime = (float)frameTimeMs;

	// Update min/max frame times
	if ((float)frameTimeMs < perfCounters.minFrameTime) {
		perfCounters.minFrameTime = (float)frameTimeMs;
	}
	if ((float)frameTimeMs > perfCounters.maxFrameTime) {
		perfCounters.maxFrameTime = (float)frameTimeMs;
	}

	// Add to frame time history
	perfCounters.frameTimeHistory[perfCounters.frameTimeHistoryIndex] = (float)frameTimeMs;
	perfCounters.frameTimeHistoryIndex = (perfCounters.frameTimeHistoryIndex + 1) % PERF_HISTORY_SIZE;
	if (perfCounters.frameTimeHistoryCount < PERF_HISTORY_SIZE) {
		perfCounters.frameTimeHistoryCount++;
	}

	// Calculate average frame time
	float totalFrameTime = 0.0f;
	for (int i = 0; i < perfCounters.frameTimeHistoryCount; i++) {
		totalFrameTime += perfCounters.frameTimeHistory[i];
	}
	perfCounters.averageFrameTime = totalFrameTime / (float)perfCounters.frameTimeHistoryCount;

	// Update FPS every second
	if (currentTime - perfCounters.lastFPSUpdate >= 1000) {
		perfCounters.currentFPS = (float)perfCounters.frameCount * 1000.0f / (float)(currentTime - perfCounters.lastFPSUpdate);
		perfCounters.frameCount = 0;
		perfCounters.lastFPSUpdate = currentTime;
		
		// Update average FPS
		if (perfCounters.averageFPS == 0.0f) {
			perfCounters.averageFPS = perfCounters.currentFPS;
		} else {
			perfCounters.averageFPS = perfCounters.averageFPS * 0.95f + perfCounters.currentFPS * 0.05f;
		}
	}

	// Average draw calls
	if (perfCounters.drawCallHistoryCount > 0) {
		int totalDrawCalls = 0;
		for (int i = 0; i < perfCounters.drawCallHistoryCount; i++) {
			totalDrawCalls += perfCounters.drawCallHistory[i];
		}
		perfCounters.averageDrawCallsPerFrame = (float)totalDrawCalls / (float)perfCounters.drawCallHistoryCount;
	}
}

/*
================
Perf_CountDrawCall
================
*/
void Perf_CountDrawCall(void) {
	perfCounters.drawCallsThisFrame++;
	perfCounters.totalDrawCalls++;
}

/*
================
Perf_ResetFrameCounters
================
*/
void Perf_ResetFrameCounters(void) {
	// Add current frame to history before reset
	perfCounters.drawCallHistory[perfCounters.drawCallHistoryIndex] = perfCounters.drawCallsThisFrame;
	perfCounters.drawCallHistoryIndex = (perfCounters.drawCallHistoryIndex + 1) % PERF_HISTORY_SIZE;
	if (perfCounters.drawCallHistoryCount < PERF_HISTORY_SIZE) {
		perfCounters.drawCallHistoryCount++;
	}

	// Update min/max draw calls
	if (perfCounters.drawCallsThisFrame < perfCounters.minDrawCallsPerFrame) {
		perfCounters.minDrawCallsPerFrame = perfCounters.drawCallsThisFrame;
	}
	if (perfCounters.drawCallsThisFrame > perfCounters.maxDrawCallsPerFrame) {
		perfCounters.maxDrawCallsPerFrame = perfCounters.drawCallsThisFrame;
	}

	perfCounters.drawCallsThisFrame = 0;
}

/*
================
Perf_GetInfoString
================
*/
void Perf_GetInfoString(char *buffer, int bufferSize) {
	Com_sprintf(buffer, bufferSize,
		"FPS: %.1f (Avg: %.1f)\n"
		"Frame Time: %.2f ms (Min: %.2f, Max: %.2f)\n"
		"Draw Calls: %d (Avg: %.1f)\n"
		"GPU Time: %.2f ms",
		perfCounters.currentFPS, perfCounters.averageFPS,
		perfCounters.currentFrameTime, perfCounters.minFrameTime, perfCounters.maxFrameTime,
		perfCounters.drawCallsThisFrame, perfCounters.averageDrawCallsPerFrame,
		perfCounters.gpuFrameTime);
}

/*
================
Perf_UpdateGPUTiming
================
*/
void Perf_UpdateGPUTiming(float gpuFrameTimeMs) {
	perfCounters.gpuFrameTime = gpuFrameTimeMs;
	perfCounters.gpuTimingAvailable = qtrue;
}

/*
================
Perf_DisplayInfo_f
================
*/
void Perf_DisplayInfo_f(void) {
	char info[512];
	Perf_GetInfoString(info, sizeof(info));
	Com_Printf("--- Performance Info ---\n%s\n------------------------\n", info);
}
