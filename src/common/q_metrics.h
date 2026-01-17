/*
===============================================================================
System Metrics Collection for id Tech 3

Comprehensive performance monitoring and metrics export system.
===============================================================================
*/

#ifndef __Q_METRICS_H__
#define __Q_METRICS_H__

#include "q_shared.h"

// Metric types
typedef enum {
    METRIC_TYPE_COUNTER,      // Monotonically increasing counter
    METRIC_TYPE_GAUGE,        // Value that can go up or down
    METRIC_TYPE_HISTOGRAM,    // Distribution of values
    METRIC_TYPE_SUMMARY       // Quantiles and sums
} metric_type_t;

// Metric value
typedef union {
    int64_t counter;
    double gauge;
    struct {
        double sum;
        uint64_t count;
        double min;
        double max;
        double p50, p90, p95, p99; // Percentiles
    } histogram;
} metric_value_t;

// Individual metric
typedef struct metric_s {
    char name[64];
    char description[256];
    char unit[16];
    metric_type_t type;
    metric_value_t value;

    // Labels for categorization
    char labels[256]; // key1=value1,key2=value2,...

    struct metric_s *next;
} metric_t;

// Metrics registry
typedef struct {
    metric_t *metrics;
    int metric_count;
    qboolean enabled;
    char export_path[256];
    double last_export_time;
} metrics_registry_t;

// Global metrics registry
extern metrics_registry_t metrics_registry;

//============================================================================
// Core Metrics API
//============================================================================

// Initialize metrics system
void Metrics_Init(const char *export_path);

// Shutdown metrics system
void Metrics_Shutdown(void);

// Create a new metric
metric_t *Metrics_CreateMetric(const char *name, const char *description,
                              metric_type_t type, const char *unit);

// Set metric labels
void Metrics_SetLabels(metric_t *metric, const char *labels);

// Update metric values
void Metrics_IncrementCounter(metric_t *metric, int64_t value);
void Metrics_SetGauge(metric_t *metric, double value);
void Metrics_RecordHistogram(metric_t *metric, double value);

// Export metrics
void Metrics_ExportToFile(void);
void Metrics_ExportToConsole(void);
void Metrics_ExportJSON(const char *filename);

//============================================================================
// Built-in System Metrics
//============================================================================

// Engine performance metrics
extern metric_t *metric_frame_time;
extern metric_t *metric_fps;
extern metric_t *metric_frame_drops;

// Memory metrics
extern metric_t *metric_heap_usage;
extern metric_t *metric_heap_peak;
extern metric_t *metric_zone_usage;
extern metric_t *metric_hunk_usage;

// Network metrics
extern metric_t *metric_packets_sent;
extern metric_t *metric_packets_received;
extern metric_t *metric_bytes_sent;
extern metric_t *metric_bytes_received;
extern metric_t *metric_ping_time;

// Rendering metrics
extern metric_t *metric_triangles_rendered;
extern metric_t *metric_draw_calls;
extern metric_t *metric_texture_memory;
extern metric_t *metric_shader_switches;

// Audio metrics
extern metric_t *metric_sounds_playing;
extern metric_t *metric_audio_buffer_underruns;

// VM metrics
extern metric_t *metric_vm_calls;
extern metric_t *metric_vm_errors;

// Error metrics
extern metric_t *metric_crash_count;
extern metric_t *metric_error_count;
extern metric_t *metric_warning_count;

//============================================================================
// Convenience Macros
//============================================================================

#define METRICS_FRAME_TIME(value) Metrics_RecordHistogram(metric_frame_time, value)
#define METRICS_FPS(value) Metrics_SetGauge(metric_fps, value)
#define METRICS_MEMORY_HEAP(current, peak) \
    do { \
        Metrics_SetGauge(metric_heap_usage, current); \
        Metrics_SetGauge(metric_heap_peak, peak); \
    } while(0)

#define METRICS_NETWORK_PACKETS(sent, recv) \
    do { \
        Metrics_IncrementCounter(metric_packets_sent, sent); \
        Metrics_IncrementCounter(metric_packets_received, recv); \
    } while(0)

#define METRICS_NETWORK_BYTES(sent, recv) \
    do { \
        Metrics_IncrementCounter(metric_bytes_sent, sent); \
        Metrics_IncrementCounter(metric_bytes_received, recv); \
    } while(0)

#define METRICS_RENDERING(tris, draws) \
    do { \
        Metrics_IncrementCounter(metric_triangles_rendered, tris); \
        Metrics_IncrementCounter(metric_draw_calls, draws); \
    } while(0)

#define METRICS_ERROR_COUNT() Metrics_IncrementCounter(metric_error_count, 1)
#define METRICS_WARNING_COUNT() Metrics_IncrementCounter(metric_warning_count, 1)
#define METRICS_CRASH_COUNT() Metrics_IncrementCounter(metric_crash_count, 1)

//============================================================================
// Performance Profiling Integration
//============================================================================

// Auto-instrumentation for functions
#define METRICS_PROFILE_FUNCTION() \
    static metric_t *func_metric = NULL; \
    double func_start_time = 0; \
    if (!func_metric) { \
        char metric_name[64]; \
        Com_sprintf(metric_name, sizeof(metric_name), "func_%s_time", __FUNCTION__); \
        func_metric = Metrics_CreateMetric(metric_name, "Function execution time", \
                                          METRIC_TYPE_HISTOGRAM, "seconds"); \
    } \
    func_start_time = Sys_Milliseconds() * 0.001;

#define METRICS_PROFILE_END() \
    if (func_metric) { \
        double func_end_time = Sys_Milliseconds() * 0.001; \
        Metrics_RecordHistogram(func_metric, func_end_time - func_start_time); \
    }

//============================================================================
// Export Formats
//============================================================================

// Prometheus format export
void Metrics_ExportPrometheus(const char *filename);

// InfluxDB format export
void Metrics_ExportInfluxDB(const char *filename);

// CSV format export
void Metrics_ExportCSV(const char *filename);

//============================================================================
// Configuration
//============================================================================

// Enable/disable metrics collection
void Metrics_Enable(qboolean enable);

// Set export interval (seconds)
void Metrics_SetExportInterval(double interval);

// Set export format
typedef enum {
    METRICS_FORMAT_JSON,
    METRICS_FORMAT_PROMETHEUS,
    METRICS_FORMAT_INFLUXDB,
    METRICS_FORMAT_CSV
} metrics_format_t;

void Metrics_SetExportFormat(metrics_format_t format);

//============================================================================
// Health Checks Integration
//============================================================================

// Health check metrics
extern metric_t *metric_health_engine;
extern metric_t *metric_health_network;
extern metric_t *metric_health_memory;
extern metric_t *metric_health_disk;

// Health check functions
typedef enum {
    HEALTH_OK,
    HEALTH_WARNING,
    HEALTH_CRITICAL,
    HEALTH_UNKNOWN
} health_status_t;

health_status_t Health_CheckEngine(void);
health_status_t Health_CheckNetwork(void);
health_status_t Health_CheckMemory(void);
health_status_t Health_CheckDisk(void);

// Overall health status
health_status_t Health_GetOverallStatus(void);
const char *Health_StatusToString(health_status_t status);

#endif // __Q_METRICS_H__