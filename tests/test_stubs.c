// Stub implementations for standalone test executables
// These provide minimal implementations of functions normally provided by the main engine

#include "../src/common/q_shared.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <time.h>

// cvar_t is already defined in q_shared.h

// Forward declarations to avoid warnings
void Com_Printf(const char *fmt, ...);
void Com_DPrintf(const char *fmt, ...);
void Com_Error(errorParm_t level, const char *fmt, ...);
int Q_stricmp(const char *s1, const char *s2);
void Q_strncpyz(char *dest, const char *src, int destsize);
int Sys_Milliseconds(void);
int Com_Milliseconds(void);
void Com_Quit_f(void);
int Hunk_MemoryRemaining(void);
void *Z_Malloc(int size);
void *Sys_LoadFunction(void *handle, const char *name);
float sqrtf(float x);
void Z_Free(void *ptr);
qboolean Q_ValidateFilePath(const char *path);
qboolean FS_Initialized(void);
qboolean FS_StartupInProgress(void);
int Cmd_Argc(void);
char *Cmd_Argv(int arg);

// Global variables
cvar_t *com_developer = NULL;
cvar_t *developer = NULL;

// Stub functions
void Sys_UnloadLibrary(void *handle) {
    Q_UNUSED(handle);
}

void *Hunk_Alloc(int size, ha_pref pref) {
    Q_UNUSED(pref);
    return malloc(size);
}

// Stub for Com_Printf - redirect to stdout
void Com_Printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

// Stub for Com_Error - exit with error
void Com_Error(errorParm_t level, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "Com_Error: ");
    vfprintf(stderr, fmt, args);
    va_end(args);
    exit(1);
}

// Stub for Q_stricmp
int Q_stricmp(const char *s1, const char *s2) {
    return strcasecmp(s1, s2);
}

// Stub for Q_strncpyz
void Q_strncpyz(char *dest, const char *src, int destsize) {
    if (destsize <= 0) return;
    strncpy(dest, src, destsize - 1);
    dest[destsize - 1] = '\0';
}

// Stub for Sys_Milliseconds
int Sys_Milliseconds(void) {
    return (int)(clock() * 1000 / CLOCKS_PER_SEC);
}

// Stub for Com_Milliseconds
int Com_Milliseconds(void) {
    return Sys_Milliseconds();
}

// Stub for Com_Quit_f
void Com_Quit_f(void) {
    exit(0);
}

// Stub for Com_DPrintf - developer printf
void Com_DPrintf(const char *fmt, ...) {
    Q_UNUSED(fmt);
    // Only print if developer mode is enabled (stub always disabled)
}

// Stub for Hunk_MemoryRemaining
int Hunk_MemoryRemaining(void) {
    return 1024 * 1024; // Return 1MB as available memory
}

// Stub for Z_Malloc
void *Z_Malloc(int size) {
    return malloc(size);
}

// Stub for Sys_LoadFunction
void *Sys_LoadFunction(void *handle, const char *name) {
    Q_UNUSED(handle);
    Q_UNUSED(name);
    return NULL; // Always fail for tests
}

// Stub for sqrtf
float sqrtf(float x) {
    return (float)sqrt((double)x);
}

// Stub for Z_Free
void Z_Free(void *ptr) {
    free(ptr);
}

// Stub for Q_ValidateFilePath
qboolean Q_ValidateFilePath(const char *path) {
    Q_UNUSED(path); // Suppress unused parameter warning
    return qtrue; // Accept all paths for testing
}

// Stub for FS_Initialized
qboolean FS_Initialized(void) {
    return qtrue;
}

// Stub for FS_StartupInProgress
qboolean FS_StartupInProgress(void) {
    return qfalse;
}

// Stub for Cmd_Argc
int Cmd_Argc(void) {
    return 1;
}

// Stub for Cmd_Argv
char *Cmd_Argv(int arg) {
    static char *dummy = "test";
    Q_UNUSED(arg); // Suppress unused parameter warning for now
    return dummy;
}
