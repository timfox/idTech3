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

// Enhanced stability monitoring
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

static stability_metrics_t stability_metrics = {0};

// Advanced performance monitoring cvars
static cvar_t *perf_detailed_gpu;
static cvar_t *perf_memory_tracking;
static cvar_t *perf_frame_analysis;
static cvar_t *perf_csv_output;
static cvar_t *perf_alert_threshold;
static cvar_t *perf_regression_detection;
static cvar_t *perf_baseline_frames;
static cvar_t *perf_regression_threshold;
static cvar_t *perf_auto_adjust;
static cvar_t *perf_stability_monitoring;
static cvar_t *perf_anomaly_detection;

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

	perf_stability_monitoring = Cvar_Get("perf_stability_monitoring", "1", CVAR_ARCHIVE);
	Cvar_SetDescription(perf_stability_monitoring, "Enable real-time stability monitoring and anomaly detection");

	perf_anomaly_detection = Cvar_Get("perf_anomaly_detection", "1", CVAR_ARCHIVE);
	Cvar_SetDescription(perf_anomaly_detection, "Enable automatic anomaly detection and reporting");

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

	// Update stability metrics
	Perf_UpdateStabilityMetrics();
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

/*
================
Perf_UpdateStabilityMetrics
================
*/
static void Perf_UpdateStabilityMetrics(void) {
    if (!perf_stability_monitoring || !perf_stability_monitoring->integer) {
        return;
    }

    // Calculate frame time variance for stability score
    float variance = 0.0f;
    int valid_samples = 0;

    for (int i = 0; i < perfCounters.frameTimeHistoryCount; i++) {
        float diff = perfCounters.frameTimeHistory[i] - perfCounters.averageFrameTime;
        variance += diff * diff;
        valid_samples++;
    }

    if (valid_samples > 0) {
        variance /= valid_samples;
        stability_metrics.average_frame_time_variance = variance;

        // Stability score based on variance (lower variance = higher stability)
        // Scale so that 1ms variance = 100 points, 10ms variance = 10 points
        stability_metrics.stability_score = 100.0f / (1.0f + variance);
        if (stability_metrics.stability_score > 100.0f) stability_metrics.stability_score = 100.0f;
    }

    // Check for frame drops (frames taking > 2x average time)
    if (perfCounters.currentFrameTime > perfCounters.averageFrameTime * 2.0f) {
        stability_metrics.total_frame_drops++;
        stability_metrics.consecutive_stable_frames = 0;
    } else {
        stability_metrics.consecutive_stable_frames++;
    }

    // Anomaly detection
    stability_metrics.anomaly_detected = qfalse;

    if (perfCounters.currentFrameTime > perfCounters.maxFrameTime * 0.8f) {
        stability_metrics.anomaly_detected = qtrue;
        stability_metrics.anomaly_count++;
        Q_snprintf(stability_metrics.last_anomaly_description,
                  sizeof(stability_metrics.last_anomaly_description),
                  "High frame time: %.2fms (max: %.2fms)",
                  perfCounters.currentFrameTime, perfCounters.maxFrameTime);
    }

    // System health checks
    // Note: Memory and CPU monitoring would need platform-specific implementations
    stability_metrics.memory_usage_percent = 0.0f; // Placeholder
    stability_metrics.cpu_usage_percent = 0.0f;    // Placeholder
    stability_metrics.thread_count = 1;            // Placeholder

    // Determine if system is overloaded
    stability_metrics.system_overloaded =
        (stability_metrics.stability_score < 50.0f) ||
        (stability_metrics.total_frame_drops > 10);

    // Generate recovery suggestions
    if (stability_metrics.system_overloaded) {
        Q_snprintf(stability_metrics.recovery_suggestions,
                  sizeof(stability_metrics.recovery_suggestions),
                  "Reduce graphics quality, disable multi-threading, check for memory leaks");
    } else {
        Q_strncpyz(stability_metrics.recovery_suggestions, "System performing normally", sizeof(stability_metrics.recovery_suggestions));
    }
}

/*
================
Perf_GetStabilityMetrics
================
*/
const stability_metrics_t* Perf_GetStabilityMetrics(void) {
    return &stability_metrics;
}

/*
================
Perf_GetStabilityReport
================
*/
void Perf_GetStabilityReport(char *buffer, int bufferSize) {
    Q_snprintf(buffer, bufferSize,
        "Stability Report:\n"
        "  Stability Score: %.1f/100\n"
        "  Frame Drops: %d\n"
        "  Consecutive Stable Frames: %d\n"
        "  Anomalies Detected: %d\n"
        "  System Overloaded: %s\n"
        "  Recovery Suggestions: %s\n"
        "%s%s",
        stability_metrics.stability_score,
        stability_metrics.total_frame_drops,
        stability_metrics.consecutive_stable_frames,
        stability_metrics.anomaly_count,
        stability_metrics.system_overloaded ? "YES" : "NO",
        stability_metrics.recovery_suggestions,
        stability_metrics.anomaly_detected ? "  Last Anomaly: " : "",
        stability_metrics.anomaly_detected ? stability_metrics.last_anomaly_description : "");
}
