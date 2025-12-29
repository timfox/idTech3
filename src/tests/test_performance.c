/*
=============================================================================
Performance Benchmarking Suite

Comprehensive performance testing for renderer and engine subsystems.
=============================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../common/q_shared.h"

// Benchmark configuration
#define BENCHMARK_ITERATIONS 100
#define BENCHMARK_WARMUP_ITERATIONS 10

// Benchmark results
typedef struct benchmarkResult_s {
    const char* name;
    double minTime;
    double maxTime;
    double avgTime;
    double totalTime;
    int iterations;
} benchmarkResult_t;

// Performance metrics
typedef struct performanceMetrics_s {
    double fps;
    double frameTime;
    double cpuTime;
    double gpuTime;
    int trianglesRendered;
    int drawCalls;
    int textureUploads;
    int shaderSwitches;
} performanceMetrics_t;

// Global benchmark state
static benchmarkResult_t* benchmarkResults = NULL;
static int benchmarkCount = 0;
static int benchmarkCapacity = 0;

/*
==================
Benchmark_Init
==================
*/
// Forward declarations for benchmarks
void Benchmark_Init(void);
void Benchmark_Shutdown(void);
void Benchmark_RunAll(void);
void Benchmark_Init(void) {
    benchmarkCapacity = 32;
    benchmarkResults = (benchmarkResult_t*)malloc(sizeof(benchmarkResult_t) * benchmarkCapacity);
    benchmarkCount = 0;

    printf("Performance Benchmarking Suite Initialized\n");
}

/*
==================
Benchmark_Shutdown
==================
*/
void Benchmark_Shutdown(void) {
    if (benchmarkResults) {
        free(benchmarkResults);
        benchmarkResults = NULL;
    }
    benchmarkCount = 0;
    benchmarkCapacity = 0;

    printf("Performance Benchmarking Suite Shutdown\n");
}

/*
==================
Benchmark_AddResult
==================
*/
static void Benchmark_AddResult(const char* name, double minTime, double maxTime,
                               double avgTime, double totalTime, int iterations) {
    if (benchmarkCount >= benchmarkCapacity) {
        benchmarkCapacity *= 2;
        benchmarkResults = (benchmarkResult_t*)realloc(benchmarkResults,
                          sizeof(benchmarkResult_t) * benchmarkCapacity);
    }

    benchmarkResult_t* result = &benchmarkResults[benchmarkCount++];
    result->name = name;
    result->minTime = minTime;
    result->maxTime = maxTime;
    result->avgTime = avgTime;
    result->totalTime = totalTime;
    result->iterations = iterations;
}

/*
==================
Benchmark_RunBenchmark
==================
*/
static void Benchmark_RunBenchmark(const char* name,
                                  void (*benchmarkFunc)(void),
                                  int iterations) {
    double* times = (double*)malloc(sizeof(double) * iterations);
    double minTime = 999999.0;
    double maxTime = 0.0;
    double totalTime = 0.0;

    printf("Running benchmark: %s (%d iterations)\n", name, iterations);

    // Warmup iterations
    for (int i = 0; i < BENCHMARK_WARMUP_ITERATIONS; i++) {
        benchmarkFunc();
    }

    // Benchmark iterations
    for (int i = 0; i < iterations; i++) {
        clock_t start = clock();
        benchmarkFunc();
        clock_t end = clock();

        double timeMs = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
        times[i] = timeMs;

        minTime = (timeMs < minTime) ? timeMs : minTime;
        maxTime = (timeMs > maxTime) ? timeMs : maxTime;
        totalTime += timeMs;
    }

    double avgTime = totalTime / iterations;

    Benchmark_AddResult(name, minTime, maxTime, avgTime, totalTime, iterations);

    printf("  Min: %.3f ms, Max: %.3f ms, Avg: %.3f ms\n", minTime, maxTime, avgTime);

    free(times);
}

/*
==================
Benchmark Functions
==================
*/

// Math benchmarks
static void Benchmark_VectorOperations(void) {
    float a[3] = {1.0f, 2.0f, 3.0f};
    float b[3] = {4.0f, 5.0f, 6.0f};
    float result[3];

    for (int i = 0; i < 1000; i++) {
        // Vector addition
        result[0] = a[0] + b[0];
        result[1] = a[1] + b[1];
        result[2] = a[2] + b[2];

        // Vector subtraction
        result[0] = result[0] - a[0];
        result[1] = result[1] - a[1];
        result[2] = result[2] - a[2];

        // Vector scaling
        result[0] *= 2.0f;
        result[1] *= 2.0f;
        result[2] *= 2.0f;

        // Vector normalization (simplified)
        float length = sqrtf(result[0]*result[0] + result[1]*result[1] + result[2]*result[2]);
        if (length > 0.0f) {
            result[0] /= length;
            result[1] /= length;
            result[2] /= length;
        }
    }
}

static void Benchmark_MatrixOperations(void) {
    float matrix1[16], matrix2[16], result[16];

    // Initialize test matrices
    for (int i = 0; i < 16; i++) {
        matrix1[i] = (float)i;
        matrix2[i] = (float)(i * 2);
        result[i] = 0.0f;
    }

    // Simple matrix multiplication (4x4)
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            for (int k = 0; k < 4; k++) {
                result[row * 4 + col] += matrix1[row * 4 + k] * matrix2[k * 4 + col];
            }
        }
    }
}

// Memory benchmarks
static void Benchmark_MemoryAllocation(void) {
    void* ptrs[100];

    for (int i = 0; i < 100; i++) {
        ptrs[i] = malloc(1024);
        if (ptrs[i]) {
            memset(ptrs[i], i, 1024);
            free(ptrs[i]);
        }
    }
}

// String benchmarks
static void Benchmark_StringOperations(void) {
    char buffer[1024];
    const char* testString = "This is a test string for benchmarking string operations";
    size_t len;
    char* found;

    for (int i = 0; i < 100; i++) {
        strcpy(buffer, testString);
        strcat(buffer, " additional text");
        len = strlen(buffer);
        found = strstr(buffer, "test");
        (void)len; (void)found; // Avoid unused variable warnings
    }
}

// File I/O benchmarks
static void Benchmark_FileOperations(void) {
    FILE* file;
    char buffer[256];

    for (int i = 0; i < 10; i++) {
        file = fopen("/tmp/benchmark_test.tmp", "w");
        if (file) {
            fprintf(file, "Benchmark test data %d\n", i);
            fclose(file);
        }

        file = fopen("/tmp/benchmark_test.tmp", "r");
        if (file) {
            if (fgets(buffer, sizeof(buffer), file) != NULL) {
                // Successfully read data
            }
            fclose(file);
        }

        remove("/tmp/benchmark_test.tmp");
    }
}

/*
==================
Benchmark_RunAll
==================
*/
void Benchmark_RunAll(void) {
    printf("=== Performance Benchmark Suite ===\n\n");

    // Math benchmarks
    Benchmark_RunBenchmark("Vector Operations", Benchmark_VectorOperations, BENCHMARK_ITERATIONS);
    Benchmark_RunBenchmark("Matrix Operations", Benchmark_MatrixOperations, BENCHMARK_ITERATIONS);

    // Memory benchmarks
    Benchmark_RunBenchmark("Memory Allocation", Benchmark_MemoryAllocation, BENCHMARK_ITERATIONS / 10);

    // String benchmarks
    Benchmark_RunBenchmark("String Operations", Benchmark_StringOperations, BENCHMARK_ITERATIONS);

    // File I/O benchmarks
    Benchmark_RunBenchmark("File Operations", Benchmark_FileOperations, BENCHMARK_ITERATIONS / 10);

    printf("\n=== Benchmark Results Summary ===\n");
    printf("%-30s %-10s %-10s %-10s\n", "Benchmark", "Min(ms)", "Max(ms)", "Avg(ms)");
    printf("%-30s %-10s %-10s %-10s\n", "------------------------------", "--------", "--------", "--------");

    for (int i = 0; i < benchmarkCount; i++) {
        benchmarkResult_t* result = &benchmarkResults[i];
        printf("%-30s %-10.3f %-10.3f %-10.3f\n",
               result->name, result->minTime, result->maxTime, result->avgTime);
    }

    printf("\nBenchmark suite completed: %d benchmarks run\n", benchmarkCount);
}

/*
==================
main
==================
*/
int main(int argc, char* argv[]) {
    (void)argc; (void)argv; // Suppress unused parameter warnings

    Benchmark_Init();
    Benchmark_RunAll();
    Benchmark_Shutdown();

    return 0;
}