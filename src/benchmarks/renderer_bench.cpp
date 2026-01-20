// Lightweight benchmarks for rendering backends using the RendererAbstraction.
// Benchmarks demographics:
//  - vk_rt_trace_rays vs PathTracer_RenderSample (PathTracer path vs RTX path)
//  - Denoiser throughput/latency
//  - FSR path impact

#include <chrono>
#include <ctime>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <unistd.h>
#include <string>
#include <algorithm>
#include <numeric>
#include <cmath>
#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/mach_kernel.h>
#else
#endif

// Types used by the engine
#include "../renderers/renderercommon/tr_public.h"

// Forward declarations for functions we'll mock
static void PathTracer_RenderSample(vec3_t result, const vec3_t origin, const vec3_t direction);
static void RTX_RenderScene(const refdef_t *fd);

// Mock implementations for benchmark (since we don't link against full renderer libraries)

// Mock renderer functions that the wrappers call
static void PathTracer_RenderSample(vec3_t result, const vec3_t origin, const vec3_t direction) {
    (void)origin; (void)direction;
    // Mock: return a simple color based on direction
    result[0] = direction[0] * 0.5f + 0.5f;
    result[1] = direction[1] * 0.5f + 0.5f;
    result[2] = direction[2] * 0.5f + 0.5f;
}

static void RTX_RenderScene(const refdef_t *fd) {
    (void)fd;
    // Mock: do nothing
}

static void Denoiser_Init(void) {
    // Mock: do nothing
}

static void Denoiser_Shutdown(void) {
    // Mock: do nothing
}

static void Denoiser_Apply(vec3_t *input, vec3_t *output, int width, int height) {
    // Mock: simple copy
    int totalPixels = width * height;
    for (int i = 0; i < totalPixels; ++i) {
        output[i][0] = input[i][0];
        output[i][1] = input[i][1];
        output[i][2] = input[i][2];
    }
}

static void vk_fsr_apply(int width, int height) {
    (void)width; (void)height;
    // Mock: do nothing
}

// Simple memory measurement (RSS in MB)
static double __attribute__((unused)) get_current_memory_mb() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize / (1024.0 * 1024.0);
    }
    return -1.0;
#elif defined(__APPLE__)
    struct mach_task_basic_info info;
    mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &infoCount) == KERN_SUCCESS) {
        return info.resident_size / (1024.0 * 1024.0);
    }
    return -1.0;
#else
    // Linux fallback
    long resident_pages = 0;
    FILE *f = fopen("/proc/self/statm", "r");
    if (f) {
        long size_pages;
        if (fscanf(f, "%ld %ld", &size_pages, &resident_pages) == 2) {
            fclose(f);
            long page_size = sysconf(_SC_PAGESIZE);
            double mem_mb = (double)resident_pages * (double)page_size / 1024.0 / 1024.0;
            return mem_mb;
        }
        fclose(f);
    }
    return -1.0;
#endif
}

// Enhanced timing utility with per-iteration metrics
template <typename Fn>
static std::vector<double> time_function_detailed(Fn func, int iterations, const char* operation_name) {
    std::vector<double> timings;
    timings.reserve(iterations);

    double memory_before = get_current_memory_mb();

    for (int i = 0; i < iterations; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();

        double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();
        timings.push_back(duration_ms);

        // Write to time-series file immediately for real-time monitoring
        static FILE* timeseries_file = nullptr;
        if (!timeseries_file) {
            timeseries_file = fopen("tests/benchmarks/bench_timeseries.jsonl", "a");
        }
        if (timeseries_file) {
            double memory_current = get_current_memory_mb();
            fprintf(timeseries_file, "{\"timestamp\":\"%ld\",\"iteration\":%d,\"operation\":\"%s\",\"duration_ms\":%.6f,\"memory_mb\":%.2f}\n",
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count(),
                    i, operation_name, duration_ms, memory_current);
            fflush(timeseries_file); // Ensure data is written immediately
        }
    }

    return timings;
}

// Legacy compatibility function
template <typename Fn>
static double time_function(Fn func, int iterations) {
    auto timings = time_function_detailed(func, iterations, "legacy");
    double sum = 0.0;
    for (double t : timings) sum += t;
    return sum / iterations;
}

// Statistics calculation helpers
struct BenchmarkStats {
    double min_ms, max_ms, avg_ms, median_ms, stddev_ms;
    double p95_ms, p99_ms; // percentiles
};

BenchmarkStats calculate_stats(const std::vector<double>& timings) {
    if (timings.empty()) return {0, 0, 0, 0, 0, 0, 0};

    BenchmarkStats stats;
    stats.min_ms = *std::min_element(timings.begin(), timings.end());
    stats.max_ms = *std::max_element(timings.begin(), timings.end());

    double sum = std::accumulate(timings.begin(), timings.end(), 0.0);
    stats.avg_ms = sum / timings.size();

    // Calculate median
    std::vector<double> sorted = timings;
    std::sort(sorted.begin(), sorted.end());
    size_t mid = sorted.size() / 2;
    stats.median_ms = (sorted.size() % 2 == 0) ?
        (sorted[mid - 1] + sorted[mid]) / 2.0 : sorted[mid];

    // Calculate standard deviation
    double variance = 0.0;
    for (double t : timings) {
        double diff = t - stats.avg_ms;
        variance += diff * diff;
    }
    stats.stddev_ms = std::sqrt(variance / timings.size());

    // Calculate percentiles
    size_t p95_idx = static_cast<size_t>(sorted.size() * 0.95);
    size_t p99_idx = static_cast<size_t>(sorted.size() * 0.99);
    stats.p95_ms = sorted[std::min(p95_idx, sorted.size() - 1)];
    stats.p99_ms = sorted[std::min(p99_idx, sorted.size() - 1)];

    return stats;
}

int main(int argc, char **argv) {
    printf("Enhanced Renderer Benchmark starting...\n");

    // Gate bench behind a feature flag: CLI or environment
    bool benchEnabled = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--enable-bench") == 0) {
            benchEnabled = true;
            break;
        }
    }
    if (!benchEnabled) {
        const char* env = getenv("ENABLE_RENDERER_BENCH");
        if (env && strcmp(env, "1") == 0) benchEnabled = true;
    }
    if (!benchEnabled) {
        fprintf(stderr, "Renderer bench disabled. Enable with --enable-bench or ENABLE_RENDERER_BENCH=1\\n");
        return 0;
    }

    printf("Benchmark enabled, running enhanced metrics collection...\n");

    // Enhanced benchmark configuration
    const int pathTracerIterations = 200;
    const int rtxIterations = 100;
    const int denoiserIterations = 50;
    const int fsrIterations = 50;
    const int width = 128;
    const int height = 128;

    printf("Configuration: %dx%d, iterations: pt=%d, rtx=%d, denoise=%d, fsr=%d\n",
           width, height, pathTracerIterations, rtxIterations, denoiserIterations, fsrIterations);

    // Record start time and initial memory
    auto benchmark_start = std::chrono::system_clock::now();
    double memory_start_mb = get_current_memory_mb();

    // Test data
    vec3_t result, origin = {0.0f, 0.0f, 0.0f}, direction = {0.0f, 0.0f, -1.0f};
    refdef_t dummyFd = {};

    // Run benchmarks with detailed timing
    printf("Running PathTracer benchmark...\n");
    auto pathTracerTimings = time_function_detailed([&]() {
        PathTracer_RenderSample(result, origin, direction);
    }, pathTracerIterations, "pathTracer");

    printf("Running RTX benchmark...\n");
    auto rtxTimings = time_function_detailed([&]() {
        RTX_RenderScene(&dummyFd);
    }, rtxIterations, "rtx");

    printf("Running Denoiser benchmark...\n");
    Denoiser_Init();
    vec3_t *input = reinterpret_cast<vec3_t*>(new float[width * height * 3]);
    vec3_t *output = reinterpret_cast<vec3_t*>(new float[width * height * 3]);
    memset(input, 0, sizeof(float) * width * height * 3);
    memset(output, 0, sizeof(float) * width * height * 3);

    auto denoiserTimings = time_function_detailed([&]() {
        Denoiser_Apply(input, output, width, height);
    }, denoiserIterations, "denoiser");

    Denoiser_Shutdown();
    delete[] reinterpret_cast<float*>(input);
    delete[] reinterpret_cast<float*>(output);

    printf("Running FSR benchmark...\n");
    auto fsrTimings = time_function_detailed([&]() {
        vk_fsr_apply(width, height);
    }, fsrIterations, "fsr");

    // Record end time and final memory
    auto benchmark_end = std::chrono::system_clock::now();
    double memory_end_mb = get_current_memory_mb();
    double duration_seconds = std::chrono::duration<double>(benchmark_end - benchmark_start).count();

    // Calculate statistics
    auto pathTracerStats = calculate_stats(pathTracerTimings);
    auto rtxStats = calculate_stats(rtxTimings);
    auto denoiserStats = calculate_stats(denoiserTimings);
    auto fsrStats = calculate_stats(fsrTimings);

    // Generate timestamp
    std::time_t now = std::chrono::system_clock::to_time_t(benchmark_start);
    char timestamp[32];
    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&now));

    // Write enhanced JSON output
    FILE* json_file = fopen("tests/benchmarks/bench.json", "w");
    if (json_file) {
        fprintf(json_file, "{\n");
        fprintf(json_file, "  \"timestamp\": \"%s\",\n", timestamp);
        fprintf(json_file, "  \"duration_seconds\": %.6f,\n", duration_seconds);
        fprintf(json_file, "  \"memory_start_mb\": %.2f,\n", memory_start_mb);
        fprintf(json_file, "  \"memory_end_mb\": %.2f,\n", memory_end_mb);
        fprintf(json_file, "  \"width\": %d,\n", width);
        fprintf(json_file, "  \"height\": %d,\n", height);
        fprintf(json_file, "  \"pathTracer\": {\n");
        fprintf(json_file, "    \"iterations\": %d,\n", pathTracerIterations);
        fprintf(json_file, "    \"avg_ms\": %.6f,\n", pathTracerStats.avg_ms);
        fprintf(json_file, "    \"min_ms\": %.6f,\n", pathTracerStats.min_ms);
        fprintf(json_file, "    \"max_ms\": %.6f,\n", pathTracerStats.max_ms);
        fprintf(json_file, "    \"median_ms\": %.6f,\n", pathTracerStats.median_ms);
        fprintf(json_file, "    \"stddev_ms\": %.6f,\n", pathTracerStats.stddev_ms);
        fprintf(json_file, "    \"p95_ms\": %.6f,\n", pathTracerStats.p95_ms);
        fprintf(json_file, "    \"p99_ms\": %.6f\n", pathTracerStats.p99_ms);
        fprintf(json_file, "  },\n");
        fprintf(json_file, "  \"rtx\": {\n");
        fprintf(json_file, "    \"iterations\": %d,\n", rtxIterations);
        fprintf(json_file, "    \"avg_ms\": %.6f,\n", rtxStats.avg_ms);
        fprintf(json_file, "    \"min_ms\": %.6f,\n", rtxStats.min_ms);
        fprintf(json_file, "    \"max_ms\": %.6f,\n", rtxStats.max_ms);
        fprintf(json_file, "    \"median_ms\": %.6f,\n", rtxStats.median_ms);
        fprintf(json_file, "    \"stddev_ms\": %.6f,\n", rtxStats.stddev_ms);
        fprintf(json_file, "    \"p95_ms\": %.6f,\n", rtxStats.p95_ms);
        fprintf(json_file, "    \"p99_ms\": %.6f\n", rtxStats.p99_ms);
        fprintf(json_file, "  },\n");
        fprintf(json_file, "  \"denoiser\": {\n");
        fprintf(json_file, "    \"iterations\": %d,\n", denoiserIterations);
        fprintf(json_file, "    \"avg_ms\": %.6f,\n", denoiserStats.avg_ms);
        fprintf(json_file, "    \"min_ms\": %.6f,\n", denoiserStats.min_ms);
        fprintf(json_file, "    \"max_ms\": %.6f,\n", denoiserStats.max_ms);
        fprintf(json_file, "    \"median_ms\": %.6f,\n", denoiserStats.median_ms);
        fprintf(json_file, "    \"stddev_ms\": %.6f,\n", denoiserStats.stddev_ms);
        fprintf(json_file, "    \"p95_ms\": %.6f,\n", denoiserStats.p95_ms);
        fprintf(json_file, "    \"p99_ms\": %.6f\n", denoiserStats.p99_ms);
        fprintf(json_file, "  },\n");
        fprintf(json_file, "  \"fsr\": {\n");
        fprintf(json_file, "    \"iterations\": %d,\n", fsrIterations);
        fprintf(json_file, "    \"avg_ms\": %.6f,\n", fsrStats.avg_ms);
        fprintf(json_file, "    \"min_ms\": %.6f,\n", fsrStats.min_ms);
        fprintf(json_file, "    \"max_ms\": %.6f,\n", fsrStats.max_ms);
        fprintf(json_file, "    \"median_ms\": %.6f,\n", fsrStats.median_ms);
        fprintf(json_file, "    \"stddev_ms\": %.6f,\n", fsrStats.stddev_ms);
        fprintf(json_file, "    \"p95_ms\": %.6f,\n", fsrStats.p95_ms);
        fprintf(json_file, "    \"p99_ms\": %.6f\n", fsrStats.p99_ms);
        fprintf(json_file, "  }\n");
        fprintf(json_file, "}\n");
        fclose(json_file);
    }

    // Write CSV output
    FILE* csv_file = fopen("tests/benchmarks/bench.csv", "w");
    if (csv_file) {
        fprintf(csv_file, "timestamp,duration_seconds,memory_start_mb,memory_end_mb,width,height,");
        fprintf(csv_file, "pathTracer_iterations,pathTracer_avg_ms,pathTracer_min_ms,pathTracer_max_ms,pathTracer_median_ms,pathTracer_stddev_ms,pathTracer_p95_ms,pathTracer_p99_ms,");
        fprintf(csv_file, "rtx_iterations,rtx_avg_ms,rtx_min_ms,rtx_max_ms,rtx_median_ms,rtx_stddev_ms,rtx_p95_ms,rtx_p99_ms,");
        fprintf(csv_file, "denoiser_iterations,denoiser_avg_ms,denoiser_min_ms,denoiser_max_ms,denoiser_median_ms,denoiser_stddev_ms,denoiser_p95_ms,denoiser_p99_ms,");
        fprintf(csv_file, "fsr_iterations,fsr_avg_ms,fsr_min_ms,fsr_max_ms,fsr_median_ms,fsr_stddev_ms,fsr_p95_ms,fsr_p99_ms\n");

        fprintf(csv_file, "%s,%.6f,%.2f,%.2f,%d,%d,", timestamp, duration_seconds, memory_start_mb, memory_end_mb, width, height);
        fprintf(csv_file, "%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,", pathTracerIterations, pathTracerStats.avg_ms, pathTracerStats.min_ms, pathTracerStats.max_ms, pathTracerStats.median_ms, pathTracerStats.stddev_ms, pathTracerStats.p95_ms, pathTracerStats.p99_ms);
        fprintf(csv_file, "%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,", rtxIterations, rtxStats.avg_ms, rtxStats.min_ms, rtxStats.max_ms, rtxStats.median_ms, rtxStats.stddev_ms, rtxStats.p95_ms, rtxStats.p99_ms);
        fprintf(csv_file, "%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,", denoiserIterations, denoiserStats.avg_ms, denoiserStats.min_ms, denoiserStats.max_ms, denoiserStats.median_ms, denoiserStats.stddev_ms, denoiserStats.p95_ms, denoiserStats.p99_ms);
        fprintf(csv_file, "%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n", fsrIterations, fsrStats.avg_ms, fsrStats.min_ms, fsrStats.max_ms, fsrStats.median_ms, fsrStats.stddev_ms, fsrStats.p95_ms, fsrStats.p99_ms);
        fclose(csv_file);
    }

    // Write summary JSON
    FILE* summary_file = fopen("tests/benchmarks/bench_summary.json", "w");
    if (summary_file) {
        fprintf(summary_file, "{\n");
        fprintf(summary_file, "  \"start_time\": \"%s\",\n", timestamp);
        fprintf(summary_file, "  \"end_time\": \"%s\",\n", timestamp);
        fprintf(summary_file, "  \"duration_seconds\": %.6f,\n", duration_seconds);
        fprintf(summary_file, "  \"memory_start_mb\": %.2f,\n", memory_start_mb);
        fprintf(summary_file, "  \"memory_end_mb\": %.2f,\n", memory_end_mb);
        fprintf(summary_file, "  \"pathTracer_iterations\": %d,\n", pathTracerIterations);
        fprintf(summary_file, "  \"rtx_iterations\": %d,\n", rtxIterations);
        fprintf(summary_file, "  \"denoiser_iterations\": %d,\n", denoiserIterations);
        fprintf(summary_file, "  \"fsr_iterations\": %d,\n", fsrIterations);
        fprintf(summary_file, "  \"pathTracer_avg_ms\": %.6f,\n", pathTracerStats.avg_ms);
        fprintf(summary_file, "  \"rtx_avg_ms\": %.6f,\n", rtxStats.avg_ms);
        fprintf(summary_file, "  \"denoiser_avg_ms\": %.6f,\n", denoiserStats.avg_ms);
        fprintf(summary_file, "  \"fsr_avg_ms\": %.6f\n", fsrStats.avg_ms);
        fprintf(summary_file, "}\n");
        fclose(summary_file);
    }

    printf("Enhanced benchmark completed successfully!\n");
    printf("Results written to:\n");
    printf("  - tests/benchmarks/bench.json (detailed metrics)\n");
    printf("  - tests/benchmarks/bench.csv (CSV format)\n");
    printf("  - tests/benchmarks/bench_summary.json (summary)\n");
    printf("  - tests/benchmarks/bench_timeseries.jsonl (time-series data)\n");

    return 0;
}
