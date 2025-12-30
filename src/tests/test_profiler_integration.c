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
#include <stdarg.h>

// Include common types
#include "../common/q_shared.h"

// Exclude benchmark system to avoid conflicts
#define PROFILER_NO_BENCHMARK

// Include engine headers for profiler integration
#include "../common/profiler.h"

// Profiler functions are defined in profiler.h

// Prototypes for stub functions to avoid missing prototype warnings
void Sys_Error(const char *error, ...);
void Sys_Print(const char *msg, ...);
const char *Sys_DefaultBasePath(void);
void UI_GameCommand(void);
void CL_GameCommand(void);
void SV_GameCommand(void);
void CL_ForwardCommandToServer(const char *cmd);
void SV_Init(void);
void CL_Init(void);
void Sys_SendKeyEvents(void);
void Sys_ConsoleInput(void);
void CL_JoystickEvent(int port, int key, int value);
void CL_KeyEvent(int key, qboolean down, unsigned time);
void CL_MouseEvent(int dx, int dy, int time);
void CL_CharEvent(int key);
void CL_PacketEvent(void *from, void *msg);
void CL_Frame(int msec);
void SV_Frame(int msec);
sfxHandle_t S_RegisterSound(const char *name, qboolean compressed);
void S_StartLocalSound(sfxHandle_t sfx, int channelNum);
void S_StartSound(const vec3_t origin, int entityNum, int entchannel, sfxHandle_t sfxHandle);
void BotDrawDebugPolygons(void (*drawPoly)(int color, int numPoints, float *points), int value);

// Stub implementations for missing engine functions
void Sys_Error(const char *error, ...) {
    fprintf(stderr, "Sys_Error: ");
    va_list argptr;
    va_start(argptr, error);
    vfprintf(stderr, error, argptr);
    va_end(argptr);
    fprintf(stderr, "\n");
    exit(1);
}

void Sys_Print(const char *msg, ...) {
    va_list argptr;
    va_start(argptr, msg);
    vprintf(msg, argptr);
    va_end(argptr);
}

const char *Sys_DefaultBasePath(void) {
    return "/usr/local/games/quake3";
}

void UI_GameCommand(void) {}
void CL_GameCommand(void) {}
void SV_GameCommand(void) {}
void CL_ForwardCommandToServer(const char *cmd __attribute__((unused))) {}
void SV_Init(void) {}
void CL_Init(void) {}
void Sys_SendKeyEvents(void) {}
void Sys_ConsoleInput(void) {}
void CL_JoystickEvent(int port __attribute__((unused)), int key __attribute__((unused)), int value __attribute__((unused))) {}
void CL_KeyEvent(int key __attribute__((unused)), qboolean down __attribute__((unused)), unsigned time __attribute__((unused))) {}
void CL_MouseEvent(int dx __attribute__((unused)), int dy __attribute__((unused)), int time __attribute__((unused))) {}
void CL_CharEvent(int key __attribute__((unused))) {}
void CL_PacketEvent(void *from __attribute__((unused)), void *msg __attribute__((unused))) {}
void CL_Frame(int msec __attribute__((unused))) {}
void SV_Frame(int msec __attribute__((unused))) {}
sfxHandle_t S_RegisterSound(const char *name __attribute__((unused)), qboolean compressed __attribute__((unused))) { return 0; }
void S_StartLocalSound(sfxHandle_t sfx __attribute__((unused)), int channelNum __attribute__((unused))) {}
void S_StartSound(const vec3_t origin __attribute__((unused)), int entityNum __attribute__((unused)), int entchannel __attribute__((unused)), sfxHandle_t sfxHandle __attribute__((unused))) {}
void BotDrawDebugPolygons(void (*drawPoly)(int color, int numPoints, float *points) __attribute__((unused)), int value __attribute__((unused))) {}
cvar_t *cl_shownet = NULL;

int main(int argc __attribute__((unused)), char **argv __attribute__((unused))) {
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
            Profiler_FrameEnd();
        }

        Profiler_PrintStats();
        Profiler_Shutdown();
    }

    printf("=== Profiler Integration Test Complete ===\n");
    return 0;
}