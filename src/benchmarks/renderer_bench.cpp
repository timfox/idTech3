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
#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/mach_kernel.h>
#else
#endif

// Include renderer abstraction and wrappers
#include "../renderers/renderer_abstraction.h"

// Types used by the engine
#include "../renderers/renderercommon/tr_public.h"

// Lightweight externs for denoiser and FSR (best-effort if not initialized)
extern void Denoiser_Init(void);
extern void Denoiser_Shutdown(void);
extern void Denoiser_Apply(vec3_t *input, vec3_t *output, int width, int height);
extern void vk_fsr_apply(int width, int height);

// Simple Linux memory measurement (RSS in MB)
static double get_current_memory_mb() {
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

// Basic utility: time a callable for N iterations
template <typename Fn>
static double time_function(Fn func, int iterations) {
    using namespace std::chrono;
    auto t0 = steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        func();
    }
    auto t1 = steady_clock::now();
    duration<double, std::milli> dt = t1 - t0;
    return dt.count() / iterations;
}

int main(int argc, char **argv) {
    // Gate bench behind a feature flag: CLI or environment
    bool benchEnabled = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--enable-bench") == 0) {
            benchEnabled = true;
            break;
        }
    }
    if (!benchEnabled) {
        const char* env = std::getenv("ENABLE_RENDERER_BENCH");
        if (env && std::string(env) == "1") benchEnabled = true;
    }
    if (!benchEnabled) {
        fprintf(stderr, "Renderer bench disabled. Enable with --enable-bench or ENABLE_RENDERER_BENCH=1\\n");
        return 0;
    }
    // Start time for duration measurement
    auto benchStartPoint = std::chrono::steady_clock::now();
    // Start time string for JSON summaries
    char benchStartTimeStr[32];
    std::time_t startNow = std::time(nullptr);
    struct tm startTm;
    #ifdef _WIN32
        gmtime_s(&startTm, &startNow);
    #else
        gmtime_r(&startNow, &startTm);
    #endif
    std::strftime(benchStartTimeStr, sizeof(benchStartTimeStr), "%Y-%m-%dT%H:%M:%SZ", &startTm);
    // Time cap configuration
    int capSec = 60;
    const char* capEnvLocal = std::getenv("BENCH_TIME_CAP_SECONDS");
    if (capEnvLocal) {
        int v = std::atoi(capEnvLocal);
        if (v > 0) capSec = v;
    }
    long long capMs = (long long)capSec * 1000;
    // Simple, deterministic benchmark configuration
    const int pathTracerIterations = 200;
    const int rtxIterations = 100;
    const int denoiserIterations = 50;
    const int width = 128;
    const int height = 128;

    // Per-iteration measurement containers
    std::vector<double> pathTracerPerIterMs;
    std::vector<double> rtxPerIterMs;
    std::vector<double> denoiserPerIterMs;
    std::vector<double> fsrPerIterMs;
    std::vector<double> memoryPerIterMb;

    // Prepare inputs
    RendererAbstraction ra = get_default_renderer_abstraction();

    vec3_t origin = {0.0f, 0.0f, 0.0f};
    vec3_t direction = {0.0f, 0.0f, -1.0f};
    vec3_t sampleOutput;
    refdef_t dummyFd = {};

    // Warm-up
    ra.renderSample(sampleOutput, origin, direction);
    ra.renderScene(&dummyFd);

    // Benchmark PathTracer path with cap and per-iteration timings
    int pathTracerExec = 0;
    double pathTracerTotalMs = 0.0;
    for (int i = 0; i < pathTracerIterations; ++i) {
        auto tCheck = std::chrono::steady_clock::now();
        long long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(tCheck - benchStartPoint).count();
        if (elapsed >= capMs) break;
        auto t0 = std::chrono::steady_clock::now();
        ra.renderSample(sampleOutput, origin, direction);
        auto t1 = std::chrono::steady_clock::now();
        double iterMs = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(t1 - t0).count();
        pathTracerPerIterMs.push_back(iterMs);
        memoryPerIterMb.push_back(get_current_memory_mb());
        pathTracerTotalMs += iterMs;
        pathTracerExec++;
    }
    double pathTracerAvgMs = (pathTracerExec > 0) ? pathTracerTotalMs / pathTracerExec : 0.0;

    // Benchmark RTX path with cap and per-iteration timings
    int rtxExec = 0;
    double rtxTotalMs = 0.0;
    for (int i = 0; i < rtpxIterations; ++i) {
        auto tCheck = std::chrono::steady_clock::now();
        long long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(tCheck - benchStartPoint).count();
        if (elapsed >= capMs) break;
        auto t0 = std::chrono::steady_clock::now();
        ra.renderScene(&dummyFd);
        auto t1 = std::chrono::steady_clock::now();
        double iterMs = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(t1 - t0).count();
        rtxPerIterMs.push_back(iterMs);
        memoryPerIterMb.push_back(get_current_memory_mb());
        rtxTotalMs += iterMs;
        rtxExec++;
    }
    double rtxAvgMs = (rtxExec > 0) ? rtxTotalMs / rtxExec : 0.0;
    // The bench above reuses the same wrapper; we provide per-iteration isolation by resetting state if possible
    // Note: In a real environment, you'd want separate measurement for vk_rt_trace_rays vs PathTracer,
    // this wrapper approach gives a high-level comparison in a single call path.

    // Denoiser throughput/latency with cap (per-iteration)
    Denoiser_Init();
    vec3_t *input = new vec3_t[width * height];
    vec3_t *output = new vec3_t[width * height];
    for (int i = 0; i < width * height; ++i) input[i] = {0.0f, 0.0f, 0.0f};
    int denoiseExec = 0;
    double denoiseTotalMs = 0.0;
    for (int i = 0; i < denoiserIterations; ++i) {
        auto tCheck = std::chrono::steady_clock::now();
        long long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(tCheck - benchStartPoint).count();
        if (elapsed >= capMs) break;
        auto t0 = std::chrono::steady_clock::now();
        Denoiser_Apply(input, output, width, height);
        auto t1 = std::chrono::steady_clock::now();
        double iterMs = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(t1 - t0).count();
        denoisePerIterMs.push_back(iterMs);
        memoryPerIterMb.push_back(get_current_memory_mb());
        denoiseTotalMs += iterMs;
        denoiseExec++;
    }
    double denoiseAvgMs = (denoiseExec > 0) ? denoiseTotalMs / denoiseExec : 0.0;
    Denoiser_Shutdown();
    delete[] input;
    delete[] output;

    // FSR path impact
    int fsrExec = 0;
    double fsrTotalMs = 0.0;
    auto fsrBenchFn = [&]() { vk_fsr_apply(width, height); };
    auto fsrBenchStart = std::chrono::steady_clock::now();
    for (int i = 0; i < 50; ++i) {
        auto tCheck = std::chrono::steady_clock::now();
        long long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(tCheck - benchStartPoint).count();
        if (elapsed >= capMs) break;
        auto t0 = std::chrono::steady_clock::now();
        fsrBenchFn();
        auto t1 = std::chrono::steady_clock::now();
        fsrTotalMs += std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(t1 - t0).count();
        fsrExec++;
    }
    double fsrAvgMs = (fsrExec > 0) ? fsrTotalMs / fsrExec : 0.0;

    // Report results
    std::printf("Renderer benchmarks (avg ms per iteration):\n");
    std::printf(" - PathTracer path (renderSample): %.3f ms\n", pathTracerAvgMs);
    std::printf(" - RTX path (RTX render via wrapper): %.3f ms\n", rtxAvgMs);
    std::printf(" - Denoiser throughput/latency: %.3f ms\n", denoiseAvgMs);
    std::printf(" - FSR application: %.3f ms\n", fsrAvgMs);

    // Write a second, richer JSON summary file with timings and iteration counts
    // Capture end time for summary
    auto benchEndPoint = std::chrono::steady_clock::now();
    double totalDurationSec = std::chrono::duration_cast<std::chrono::milliseconds>(benchEndPoint - benchStartPoint).count() / 1000.0;
    // Also capture start time string (ensure initialized)
    // benchStartTimeStr already computed above
    FILE *summaryFile = fopen("bench_summary.json", "w");
    if (summaryFile) {
        // Start/end times
        char endTimeBuffer[32];
        std::time_t endNow = std::time(nullptr);
        struct tm endTm;
        #ifdef _WIN32
            gmtime_s(&endTm, &endNow);
        #else
            gmtime_r(&endNow, &endTm);
        #endif
        std::strftime(endTimeBuffer, sizeof(endTimeBuffer), "%Y-%m-%dT%H:%M:%SZ", &endTm);
        double durationSec = (std::chrono::steady_clock::now() - benchStart).count() / 1e3;
        // Include memory end metric in summary
        double memoryEndMb = -1.0;
        if (!memoryPerIterMb.empty()) memoryEndMb = memoryPerIterMb.back();
        fprintf(summaryFile,
            "{\n"
            "  \"start_time\": \"%s\",\n"
            "  \"end_time\": \"%s\",\n"
            "  \"duration_seconds\": %.6f,\n"
            "  \"memory_end_mb\": %.2f,\n"
            "  \"pathTracer_iterations\": %d,\n"
            "  \"rtx_iterations\": %d,\n"
            "  \"denoiser_iterations\": %d,\n"
            "  \"fsr_iterations\": %d,\n"
            "  \"pathTracer_avg_ms\": %.6f,\n"
            "  \"rtx_avg_ms\": %.6f,\n"
            "  \"denoiser_avg_ms\": %.6f,\n"
            "  \"fsr_avg_ms\": %.6f\n"
            "}\n",
            benchStartTimeStr,
            endTimeBuffer,
            durationSec,
            memoryEndMb,
            pathTracerIterations,
            rtpxIterations,
            denoiserIterations,
            50,
            pathTracerAvgMs,
            rtxAvgMs,
            denoiseAvgMs,
            fsrAvgMs);
        fclose(summaryFile);
    }

    // Also export a JSON record for easy machine-readable comparisons over time
    FILE *jsonFile = fopen("bench.json", "w");
    if (jsonFile) {
        char timeBuffer[32];
        std::time_t now = std::time(nullptr);
        struct tm tmInfo;
        #ifdef _WIN32
            gmtime_s(&tmInfo, &now);
        #else
            gmtime_r(&now, &tmInfo);
        #endif
        std::strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%dT%H:%M:%SZ", &tmInfo);
        // Compute a memory end sample
        double memoryEndMb = 0.0;
        if (!memoryPerIterMb.empty()) memoryEndMb = memoryPerIterMb.back();
        fprintf(jsonFile,
                "{\n"
                "  \"timestamp\": \"%s\",\n"
                "  \"pathTracer_ms\": %.6f,\n"
                "  \"rtx_ms\": %.6f,\n"
                "  \"denoiser_ms\": %.6f,\n"
                "  \"fsr_ms\": %.6f,\n"
                "  \"iterations_pathTracer\": %d,\n"
                "  \"width\": %d,\n"
                "  \"height\": %d,\n"
                "  \"pathTracer_perIterMs\": [",
                timeBuffer,
                pathTracerAvgMs,
                rtxAvgMs,
                denoiseAvgMs,
                fsrAvgMs,
                pathTracerIterations,
                width,
                height);
        // pathTracerPerIterMs
        for (size_t i = 0; i < pathTracerPerIterMs.size(); ++i) {
            fprintf(jsonFile, i ? ",%.6f" : "%.6f", pathTracerPerIterMs[i]);
        }
        fprintf(jsonFile, "],\n  \"rtx_perIterMs\": [");
        for (size_t i = 0; i < rtxPerIterMs.size(); ++i) {
            fprintf(jsonFile, i ? ",%.6f" : "%.6f", rtxPerIterMs[i]);
        }
        fprintf(jsonFile, "],\n  \"denoiser_perIterMs\": [");
        for (size_t i = 0; i < denoiserPerIterMs.size(); ++i) {
            fprintf(jsonFile, i ? ",%.6f" : "%.6f", denoiserPerIterMs[i]);
        }
        fprintf(jsonFile, "],\n  \"fsr_perIterMs\": [");
        for (size_t i = 0; i < fsrPerIterMs.size(); ++i) {
            fprintf(jsonFile, i ? ",%.6f" : "%.6f", fsrPerIterMs[i]);
        }
        fprintf(jsonFile, "],\n  \"memory_per_iter_mb\": [");
        for (size_t i = 0; i < memoryPerIterMb.size(); ++i) {
            fprintf(jsonFile, i ? ",%.3f" : "%.3f", memoryPerIterMb[i]);
        }
        fprintf(jsonFile, "],\n  \"memory_end_mb\": %.2f\n", memoryEndMb);
        fprintf(jsonFile, "}\n");
        fclose(jsonFile);
    }

    // Emit bench_timeseries.jsonl for time-series data (per-iteration samples)
    {
        FILE *tsFile = fopen("bench_timeseries.jsonl", "a");
        if (tsFile) {
            size_t maxIter = std::max({pathTracerPerIterMs.size(), rtxPerIterMs.size(), denoiserPerIterMs.size(), fsrPerIterMs.size(), memoryPerIterMb.size()});
            for (size_t i = 0; i < maxIter; ++i) {
                double pt = (i < pathTracerPerIterMs.size()) ? pathTracerPerIterMs[i] : 0.0;
                double rt = (i < rtxPerIterMs.size()) ? rtxPerIterMs[i] : 0.0;
                double dn = (i < denoiserPerIterMs.size()) ? denoiserPerIterMs[i] : 0.0;
                double fs = (i < fsrPerIterMs.size()) ? fsrPerIterMs[i] : 0.0;
                double mem = (i < memoryPerIterMb.size()) ? memoryPerIterMb[i] : -1.0;
                double deltaPt = pt - (i > 0 ? pathTracerPerIterMs[i-1] : 0.0);
                double deltaRt = rt - (i > 0 ? rtxPerIterMs[i-1] : 0.0);
                double deltaDn = dn - (i > 0 ? denoiserPerIterMs[i-1] : 0.0);
                double deltaFs = fs - (i > 0 ? fsrPerIterMs[i-1] : 0.0);
                double deltaMem = (i == 0) ? (mem - 0.0) : (mem - (i-1 < memoryPerIterMb.size() ? memoryPerIterMb[i-1] : 0.0));
                // Timestamp
                std::time_t tNow = std::time(nullptr);
                char tsBuf[32];
                struct tm tInfo;
                #ifdef _WIN32
                    gmtime_s(&tInfo, &tNow);
                #else
                    gmtime_r(&tNow, &tInfo);
                #endif
                std::strftime(tsBuf, sizeof(tsBuf), "%Y-%m-%dT%H:%M:%SZ", &tInfo);
                fprintf(tsFile,
                        "{\"timestamp\":\"%s\",\"idx\":%zu,"
                        "\"pathTracer_ms\":%.6f,\"pathTracer_delta_ms\":%.6f,"
                        "\"rtx_ms\":%.6f,\"rtx_delta_ms\":%.6f,"
                        "\"denoiser_ms\":%.6f,\"denoiser_delta_ms\":%.6f,"
                        "\"fsr_ms\":%.6f,\"fsr_delta_ms\":%.6f,"
                        "\"memory_mb\":%.3f,\"memory_delta_mb\":%.3f}\n",
                        tsBuf, i, pt, deltaPt, rt, deltaRt, dn, deltaDn, fs, deltaFs, mem, deltaMem);
            }
            fclose(tsFile);
        }
    }

    return 0;
}

