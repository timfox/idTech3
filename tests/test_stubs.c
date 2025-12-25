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
int Sys_Milliseconds(void);
int Com_Milliseconds(void);
void Com_Quit_f(void);
int Hunk_MemoryRemaining(void);
void *Z_Malloc(int size);
void *Sys_LoadFunction(void *handle, const char *name);
float sqrtf(float x);
void Z_Free(void *ptr);
qboolean Q_ValidateFilePath(const char *path);

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

// Stub for CL_ForwardCommandToServer
void CL_ForwardCommandToServer(const char* cmd) {
    Q_UNUSED(cmd);
}

// Stub for Field_CompleteFilename
void Field_CompleteFilename(const char* dir, const char* ext, qboolean stripExt, qboolean allowNonPureFiles) {
    Q_UNUSED(dir);
    Q_UNUSED(ext);
    Q_UNUSED(stripExt);
    Q_UNUSED(allowNonPureFiles);
}

// Stub for CopyString
char* CopyString(const char* in) {
    if (!in) return NULL;
    size_t len = strlen(in) + 1;
    char* out = (char*)malloc(len);
    if (out) {
        strcpy(out, in);
    }
    return out;
}

// Stub for S_Malloc
void* S_Malloc(int size) {
    return malloc(size);
}

// Game-specific stubs
qboolean Cvar_Command(void) {
    return qfalse;
}

qboolean com_cl_running = qfalse;
qboolean com_sv_running = qfalse;

void UI_GameCommand(void) {}
void CL_GameCommand(void) {}
void SV_GameCommand(void) {}

void Cvar_CompleteCvarName(char* args, int argNum) {
    Q_UNUSED(args);
    Q_UNUSED(argNum);
}

// Cvar stubs
const char* Cvar_VariableString(const char* var_name) {
    Q_UNUSED(var_name);
    return "";
}

// Filesystem stubs
void FS_BypassPure(void) {}
int FS_ReadFile(const char* qpath, void** buffer) {
    Q_UNUSED(qpath);
    Q_UNUSED(buffer);
    return -1; // File not found
}
void FS_RestorePure(void) {}
void Com_WriteConfiguration(void) {}

// Utility stubs
float Q_atof(const char* str) {
    if (!str) return 0.0f;
    return (float)atof(str);
}

int Com_Filter(char* filter, char* name, int casesensitive) {
    Q_UNUSED(filter);
    Q_UNUSED(name);
    Q_UNUSED(casesensitive);
    return 0;
}

