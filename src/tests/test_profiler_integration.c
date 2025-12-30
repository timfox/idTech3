/*
=============================================================================
Profiler Integration Test

Tests the comprehensive profiling system integration with Tracy,
Vulkan render profiling, and performance benchmarking.
=============================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Exclude benchmark system to avoid conflicts
#define PROFILER_NO_BENCHMARK

// Include engine headers for profiler integration
#include "../common/profiler.h"

// Forward declarations for profiler functions
void Profiler_FrameUpdate(void);

int main(int argc, char **argv) {
    printf("=== Profiler Integration Test ===\n");

    // Test profiler configuration
    profiler_config_t config = {
        .mode = PROFILER_MODE_BASIC,
        .detailed_gpu_profiling = qtrue,
        .memory_profiling = qtrue,
        .cache_profiling = qfalse,
        .benchmark_profiling = qfalse,
        .profiling_overhead_limit = 5.0f
    };

    // Initialize profiler
    printf("Initializing profiler...\n");
    if (!Profiler_Init(&config)) {
        fprintf(stderr, "Failed to initialize profiler\n");
        return 1;
    }

    // Test basic profiling
    printf("Testing basic profiling...\n");
    Profiler_FrameBegin();

    // Simulate frame work
    sleep(1); // Simple delay for testing

    Profiler_FrameUpdate();
    Profiler_FrameEnd();

    // Test status reporting
    printf("Getting profiler status...\n");
    Profiler_PrintStats();

    // Test export functions
    printf("Testing export functions...\n");
    if (Profiler_ExportToJSON("test_profiler.json")) {
        printf("✓ JSON export successful\n");
    } else {
        printf("✗ JSON export failed\n");
    }

    if (Profiler_ExportToCSV("test_profiler.csv")) {
        printf("✓ CSV export successful\n");
    } else {
        printf("✗ CSV export failed\n");
    }

    // Test mode switching
    printf("Testing mode switching...\n");
    config.mode = PROFILER_MODE_VULKAN;
    Profiler_Shutdown();
    if (Profiler_Init(&config)) {
        printf("✓ Mode switch to Vulkan successful\n");
        Profiler_PrintStats();
        Profiler_Shutdown();
    } else {
        printf("✗ Mode switch failed\n");
    }

    // Test CVAR-based control (simulate)
    printf("Testing CVAR simulation...\n");
    config.mode = PROFILER_MODE_FULL;
    if (Profiler_Init(&config)) {
        printf("✓ Full profiling mode enabled\n");

        // Simulate a few frames
        for (int i = 0; i < 10; i++) {
            Profiler_FrameBegin();
            sleep(0); // Minimal delay for testing
            Profiler_FrameUpdate();
            Profiler_FrameEnd();
        }

        Profiler_PrintStats();
        Profiler_Shutdown();
    }

    printf("=== Profiler Integration Test Complete ===\n");
    return 0;
}