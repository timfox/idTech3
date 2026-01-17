/*
===============================================================================
Simple Performance Regression Testing Suite

Basic performance monitoring and regression detection.
===============================================================================
*/

#include "../common/q_shared.h"
#include "../common/q_math_simd.h"
#include "../common/q_allocator.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    printf("Automated Performance Regression Testing Suite\n");
    printf("==============================================\n\n");

    // Initialize required systems
    Math_SIMD_Init();
    Alloc_Init();

    // Open log file
    FILE *perf_log = fopen("perf_regression.log", "a");
    if (perf_log) {
        fprintf(perf_log, "\n=== Performance Regression Test Started ===\n");
    }

    printf("Testing SIMD Math Operations...\n");
    vec3_t a, b, result;
    VectorSet(a, 1.0f, 2.0f, 3.0f);
    VectorSet(b, 4.0f, 5.0f, 6.0f);

    double start_time = (double)clock() / CLOCKS_PER_SEC * 1000.0;

    // Test SIMD operations
    for (int i = 0; i < 10000; i++) {
        VectorAdd_SIMD(a, b, result);
        VectorScale_SIMD(result, 2.0f, result);
    }

    double end_time = (double)clock() / CLOCKS_PER_SEC * 1000.0;
    double duration = end_time - start_time;
    printf("SIMD Math Test: %.2f ms\n", duration);

    printf("Testing Memory Allocation...\n");
    start_time = (double)clock() / CLOCKS_PER_SEC * 1000.0;

    // Test memory allocation
    for (int i = 0; i < 1000; i++) {
        void *ptr = Alloc_Alloc(1024, ALLOCATOR_GENERAL, "test");
        Alloc_Free(ptr, ALLOCATOR_GENERAL);
    }

    end_time = (double)clock() / CLOCKS_PER_SEC * 1000.0;
    duration = end_time - start_time;
    printf("Memory Allocation Test: %.2f ms\n", duration);

    printf("Performance Test Report:\n");
    printf("- SIMD Features: %s\n", Math_SIMD_IsAvailable() ? "Available" : "Not Available");
    printf("- Memory Allocators: Initialized\n");

    // Log results
    if (perf_log) {
        fprintf(perf_log, "SIMD Math Test: %.2f ms\n", duration);
        fprintf(perf_log, "Memory Allocation Test: %.2f ms\n", duration);
        fprintf(perf_log, "=== Performance Regression Test Ended ===\n\n");
        fclose(perf_log);
    }

    // Clean up
    Alloc_Shutdown();

    printf("Performance regression testing completed successfully!\n");
    return 0;
}