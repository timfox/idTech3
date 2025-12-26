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
void Com_DPrintf(const char *fmt, ...);
int Sys_Milliseconds(void);
void *Sys_LoadFunction(void *handle, const char *name);
float sqrtf(float x);
qboolean Q_ValidateFilePath(const char *path);
int CPU_Flags(void);
FILE *Sys_FOpen(const char *filepath, const char *mode);
cvar_t *com_assertLevel;
void Sys_Print(const char *msg);
int Sys_GetPhysicalMemoryMB(void);
int Sys_GetNumCPUCores(void);
void Sys_Error(const char *error, ...);

// Global variables
cvar_t *com_developer = NULL;
cvar_t *developer = NULL;
cvar_t *com_dedicated = NULL;
cvar_t *com_timescale = NULL;
cvar_t *cl_packetdelay = NULL;
cvar_t *sv_packetdelay = NULL;
cvar_t *cl_shownet = NULL;
void *com_journalDataFile = NULL;
qboolean com_fullyInitialized = qfalse;
void *com_journal = NULL;
qboolean com_errorEntered = qfalse;

// Stub functions
void Sys_UnloadLibrary(void *handle) {
    Q_UNUSED(handle);
}

void *Hunk_Alloc(int size, ha_pref pref) {
    Q_UNUSED(pref);
    return malloc(size);
}

// Stub for Sys_Milliseconds
int Sys_Milliseconds(void) {
    return (int)(clock() * 1000 / CLOCKS_PER_SEC);
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

// Stub for Com_DPrintf - developer printf
void Com_DPrintf(const char *fmt, ...) {
    Q_UNUSED(fmt);
    // Only print if developer mode is enabled (stub always disabled)
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

// Stub for Q_ValidateFilePath
qboolean Q_ValidateFilePath(const char *path) {
    Q_UNUSED(path); // Suppress unused parameter warning
    return qtrue; // Accept all paths for testing
}

// Stub for CL_ForwardCommandToServer
void CL_ForwardCommandToServer(const char* cmd) {
    Q_UNUSED(cmd);
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
qboolean com_cl_running = qfalse;
qboolean com_sv_running = qfalse;

void Com_WriteConfiguration(void) {}

// Utility stubs
int Com_Filter(char* filter, char* name, int casesensitive) {
    Q_UNUSED(filter);
    Q_UNUSED(name);
    Q_UNUSED(casesensitive);
    return 0;
}


int CPU_Flags(void) {
    // Return basic CPU flags for testing - no special CPU features
    return 0;
}

FILE *Sys_FOpen(const char *filepath, const char *mode) {
    return fopen(filepath, mode);
}

// Global variable stubs
cvar_t *com_assertLevel = NULL;

// System function stubs
void Sys_Print(const char *msg) {
    printf("%s", msg);
}

int Sys_GetPhysicalMemoryMB(void) {
    return 4096; // Return 4GB as default
}

int Sys_GetNumCPUCores(void) {
    return 4; // Return 4 cores as default
}

void Sys_Error(const char *error, ...) {
    va_list args;
    va_start(args, error);
    vfprintf(stderr, error, args);
    va_end(args);
    exit(1);
}

void Com_RunAndTimeServerPacket(void *evFrom, void *buf) {
    // Stub - do nothing
    Q_UNUSED(evFrom);
    Q_UNUSED(buf);
}

void CL_PacketEvent(void *evFrom, void *buf) {
    // Stub - do nothing
    Q_UNUSED(evFrom);
    Q_UNUSED(buf);
}

void NetThread_QueueSendMessage(void *msg) {
    // Stub - do nothing
    Q_UNUSED(msg);
}

qboolean NetThread_IsThreadEnabled(void) {
    // Stub - return false (threading disabled in tests)
    return qfalse;
}

void *Sys_LoadLibrary(const char *name) {
    // Stub - return NULL (library loading disabled in tests)
    Q_UNUSED(name);
    return NULL;
}

qboolean Sys_RandomBytes(byte *data, int length) {
    // Stub - fill with zeros
    if (data && length > 0) {
        memset(data, 0, length);
    }
    return qtrue;
}

void Com_StartupVariable(const char *var) {
    // Stub - do nothing
    Q_UNUSED(var);
}

qboolean Com_SafeMode(void) {
    // Stub - return false (not in safe mode)
    return qfalse;
}

void Com_GameRestart(void) {
    // Stub - do nothing
}

const char *Sys_DefaultBasePath(void) {
    // Stub - return current directory
    return ".";
}

const char *Sys_SteamPath(void) {
    // Stub - return NULL (no Steam path in tests)
    return NULL;
}

const char *Sys_DefaultHomePath(void) {
    // Stub - return current directory
    return ".";
}

void FS_MountTable_Init(void) {
    // Stub - do nothing
}

void FS_MigrateLegacySearchPaths(void) {
    // Stub - do nothing
}

void FS_Mount_RegisterCommands(void) {
    // Stub - do nothing
}

void Com_ReadCDKey(void) {
    // Stub - do nothing
}

void Com_AppendCDKey(void) {
    // Stub - do nothing
}

void Sys_FreeFileList(char **list) {
    // Stub - do nothing (assuming the list is managed elsewhere)
    Q_UNUSED(list);
}

void FS_MountTable_Shutdown(void) {
    // Stub - do nothing
}

qboolean FS_Sandbox_ValidateOperation(const char *path, int operation) {
    // Stub - allow all operations
    Q_UNUSED(path);
    Q_UNUSED(operation);
    return qtrue;
}

qboolean FS_MountTable_IsActive(void) {
    // Stub - return false (mount table not active)
    return qfalse;
}

void *FS_Mount_FindFile(const char *path) {
    // Stub - return NULL (file not found)
    Q_UNUSED(path);
    return NULL;
}

void Sys_ResetReadOnlyAttribute(const char *filename) {
    // Stub - do nothing
    Q_UNUSED(filename);
}

void *FS_WritePolicy_GetMount(const char *path) {
    // Stub - return NULL (no mount)
    Q_UNUSED(path);
    return NULL;
}

int Sys_Mkdir(const char *path) {
    // Stub - return 0 (success)
    Q_UNUSED(path);
    return 0;
}

void BotDrawDebugPolygons(void *polys, int numPoints, int numPolys) {
    // Stub - do nothing
    Q_UNUSED(polys);
    Q_UNUSED(numPoints);
    Q_UNUSED(numPolys);
}

char **Sys_ListFiles(const char *directory, const char *extension, char *filter, int *numfiles, qboolean wantsubs) {
    // Stub - return NULL (no files found)
    Q_UNUSED(directory);
    Q_UNUSED(extension);
    Q_UNUSED(filter);
    Q_UNUSED(wantsubs);
    if (numfiles) *numfiles = 0;
    return NULL;
}

int Com_HasPatterns(const char *str) {
    // Stub - return 0 (no patterns)
    Q_UNUSED(str);
    return 0;
}

int Com_FilterPath(char *filter, int len, const char *name, qboolean casesensitive) {
    // Stub - return 0 (no filtering)
    Q_UNUSED(filter);
    Q_UNUSED(len);
    Q_UNUSED(name);
    Q_UNUSED(casesensitive);
    return 0;
}

int Com_FilterExt(char *filter, int len, const char *name) {
    // Stub - return 0 (no filtering)
    Q_UNUSED(filter);
    Q_UNUSED(len);
    Q_UNUSED(name);
    return 0;
}

int Sys_GetFileStats(const char *filename, void *stats) {
    // Stub - return -1 (file not found)
    Q_UNUSED(filename);
    Q_UNUSED(stats);
    return -1;
}

void Hunk_ClearTempMemory(void) {
    // Stub - do nothing
}

// Additional stubs for cmd.c dependencies
const char *Cvar_VariableString(const char *var_name) {
    Q_UNUSED(var_name);
    return "";
}

void FS_BypassPure(void) {
    // Stub - do nothing
}

int FS_ReadFile(const char* qpath, void** buffer) {
    Q_UNUSED(qpath);
    Q_UNUSED(buffer);
    return -1;
}

void FS_RestorePure(void) {
    // Stub - do nothing
}

void Z_Free(void *ptr) {
    free(ptr);
}

qboolean Cvar_Command(void) {
    return qfalse;
}

void UI_GameCommand(void) {
    // Stub - do nothing
}

void CL_GameCommand(void) {
    // Stub - do nothing
}

void SV_GameCommand(void) {
    // Stub - do nothing
}

void Cvar_CompleteCvarName(char *args, int argNum) {
    Q_UNUSED(args);
    Q_UNUSED(argNum);
}

void Field_CompleteFilename(const char* dir, const char* ext, qboolean stripExt, qboolean allowNonPureFiles) {
    Q_UNUSED(dir);
    Q_UNUSED(ext);
    Q_UNUSED(stripExt);
    Q_UNUSED(allowNonPureFiles);
}

