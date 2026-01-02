/*
===========================================================================
Crash Handler Implementation

Platform-specific crash handling with diagnostics.
===========================================================================
*/

// Feature test macros must come before any includes
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#endif

#include "crash_handler.h"
#include "qcommon.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#else
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#ifdef __linux__
#include <execinfo.h>
#endif
#ifdef __APPLE__
#include <execinfo.h>
#endif
#endif

// Forward declaration for Unix signal handler
#ifndef _WIN32
static void Crash_UnixSignalHandler(int sig, siginfo_t *info, void *context);
#endif

// Global crash info
static crash_info_t g_crash_info;
static qboolean g_crash_initialized = qfalse;
static int g_start_time = 0;
static crash_callback_t g_crash_callbacks[8];
static int g_crash_callback_count = 0;

// Mod loading context for crash debugging
static char g_mod_loading_name[MAX_QPATH] = {0};
static char g_mod_loading_operation[64] = {0};

// Forward declarations
static void Crash_WriteReport(const crash_info_t *info, const char *reason);
static void Crash_SetSafeModeFlag(void);
static const char *Crash_GetSignalName(int sig);

/*
=================
Crash_Init

Initialize crash handling system
=================
*/
void Crash_Init(void)
{
    if (g_crash_initialized) {
        return;
    }

    memset(&g_crash_info, 0, sizeof(g_crash_info));
    g_crash_info.build_id = BUILD_ID;
    g_crash_info.build_date = BUILD_DATE;
    g_start_time = Sys_Milliseconds();

#ifdef _WIN32
    // Windows: Set up structured exception handler
    SetUnhandledExceptionFilter(Crash_WindowsExceptionHandler);
#else
    // Unix: Set up signal handlers
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = Crash_UnixSignalHandler;
    sa.sa_flags = SA_SIGINFO | SA_RESETHAND;  // Get extra info, reset after catching
    sigemptyset(&sa.sa_mask);

    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
#endif

    g_crash_initialized = qtrue;
    Com_Printf("Crash handler initialized (Build: %s)\n", BUILD_ID);
}

/*
=================
Crash_Shutdown
=================
*/
void Crash_Shutdown(void)
{
    g_crash_initialized = qfalse;
}

/*
=================
Crash_LogMessage

Add message to the ring buffer for crash diagnostics
=================
*/
void Crash_LogMessage(const char *msg)
{
    int len, i;

    if (!msg || !g_crash_initialized) {
        return;
    }

    len = strlen(msg);
    for (i = 0; i < len; i++) {
        g_crash_info.log_ring_buffer[g_crash_info.log_ring_pos] = msg[i];
        g_crash_info.log_ring_pos = (g_crash_info.log_ring_pos + 1) % CRASH_LOG_RING_SIZE;
    }
}

/*
=================
Crash_ShouldBootSafeMode

Check if safe mode flag exists from previous crash
=================
*/
qboolean Crash_ShouldBootSafeMode(void)
{
#ifdef _WIN32
    return GetFileAttributesA(SAFE_MODE_FLAG_FILE) != INVALID_FILE_ATTRIBUTES;
#else
    struct stat st;
    return stat(SAFE_MODE_FLAG_FILE, &st) == 0;
#endif
}

/*
=================
Crash_ClearSafeModeFlag

Clear the safe mode flag after successful boot
=================
*/
void Crash_ClearSafeModeFlag(void)
{
    remove(SAFE_MODE_FLAG_FILE);
}

/*
=================
Crash_SetSafeModeFlag

Set flag to indicate crash occurred
=================
*/
static void Crash_SetSafeModeFlag(void)
{
    FILE *f = fopen(SAFE_MODE_FLAG_FILE, "w");
    if (f) {
        fprintf(f, "Crash at %s\n", BUILD_DATE);
        fclose(f);
    }
}

/*
=================
Crash_GetSignalName
=================
*/
static const char *Crash_GetSignalName(int sig)
{
#ifndef _WIN32
    switch (sig) {
        case SIGSEGV: return "SIGSEGV (Segmentation fault)";
        case SIGBUS:  return "SIGBUS (Bus error)";
        case SIGFPE:  return "SIGFPE (Floating point exception)";
        case SIGILL:  return "SIGILL (Illegal instruction)";
        case SIGABRT: return "SIGABRT (Abort)";
        default:      return "Unknown signal";
    }
#else
    return "Windows Exception";
#endif
}

/*
=================
Crash_CaptureStackTrace
=================
*/
static void Crash_CaptureStackTrace(crash_info_t *info)
{
#if defined(__linux__) || defined(__APPLE__)
    info->stack_frame_count = backtrace(info->stack_frames, CRASH_MAX_STACK_FRAMES);
#elif defined(_WIN32)
    info->stack_frame_count = CaptureStackBackTrace(0, CRASH_MAX_STACK_FRAMES,
                                                     info->stack_frames, NULL);
#else
    info->stack_frame_count = 0;
#endif
}

/*
=================
Crash_WriteReport

Write crash report to file
=================
*/
static void Crash_WriteReport(const crash_info_t *info, const char *reason)
{
    FILE *f;
    time_t now;
    char time_str[64];
    int i;

    // Ensure logs directory exists
    struct stat st = {0};
    if (stat(CRASH_LOG_DIR, &st) == -1) {
        mkdir(CRASH_LOG_DIR, 0755);
    }

    f = fopen(CRASH_REPORT_FILENAME, "w");
    if (!f) {
        // Try writing to stderr as fallback
        f = stderr;
    }

    now = time(NULL);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));

    fprintf(f, "========================================\n");
    fprintf(f, "CRASH REPORT\n");
    fprintf(f, "========================================\n\n");

    fprintf(f, "Time: %s\n", time_str);
    fprintf(f, "Build ID: %s\n", info->build_id);
    fprintf(f, "Build Date: %s\n", info->build_date);
    fprintf(f, "Uptime: %d seconds\n\n", info->uptime_seconds);

    fprintf(f, "Crash Reason: %s\n", reason ? reason : "Unknown");
    if (info->signal_name) {
        fprintf(f, "Signal: %s (%d)\n", info->signal_name, info->signal_num);
    }
    if (info->fault_address) {
        fprintf(f, "Fault Address: %p\n", info->fault_address);
    }

    // Add more verbose system information
    fprintf(f, "\n--- System Information ---\n");
#ifdef __linux__
    fprintf(f, "Platform: Linux\n");
#elif defined(__APPLE__)
    fprintf(f, "Platform: macOS\n");
#elif defined(_WIN32)
    fprintf(f, "Platform: Windows\n");
#else
    fprintf(f, "Platform: Unknown\n");
#endif

    // Add mod loading context if available
    if (g_mod_loading_name[0]) {
        fprintf(f, "\n--- Mod Loading Context ---\n");
        fprintf(f, "Mod Name: %s\n", g_mod_loading_name);
        fprintf(f, "Operation: %s\n", g_mod_loading_operation);
    }

    // Add environment info
    fprintf(f, "\n--- Environment ---\n");
    char *cmdline = getenv("_");
    if (cmdline) {
        fprintf(f, "Executable: %s\n", cmdline);
    }
    char *cwd = getcwd(NULL, 0);
    if (cwd) {
        fprintf(f, "Working Directory: %s\n", cwd);
        free(cwd);
    }

    // Stack trace
    fprintf(f, "\n--- Stack Trace ---\n");
    if (info->stack_frame_count > 0) {
#if defined(__linux__) || defined(__APPLE__)
        char **symbols = backtrace_symbols(info->stack_frames, info->stack_frame_count);
        if (symbols) {
            for (i = 0; i < info->stack_frame_count; i++) {
                fprintf(f, "  [%d] %p %s\n", i, info->stack_frames[i], symbols[i]);
            }
            free(symbols);
        } else {
            fprintf(f, "  (Failed to get symbol names)\n");
            for (i = 0; i < info->stack_frame_count; i++) {
                fprintf(f, "  [%d] %p\n", i, info->stack_frames[i]);
            }
        }
#elif defined(_WIN32)
        for (i = 0; i < info->stack_frame_count; i++) {
            fprintf(f, "  [%d] %p\n", i, info->stack_frames[i]);
        }
#endif
    } else {
        fprintf(f, "  (Stack trace unavailable)\n");
    }

    // Log ring buffer
    fprintf(f, "\n--- Last %d bytes of log ---\n", CRASH_LOG_RING_SIZE);
    if (info->log_ring_buffer[0]) {
        // Find the start of the ring buffer (wrap around point)
        int start_pos = info->log_ring_pos;
        for (i = 0; i < CRASH_LOG_RING_SIZE; i++) {
            char c = info->log_ring_buffer[(start_pos + i) % CRASH_LOG_RING_SIZE];
            if (c == '\0') break;
            fputc(c, f);
        }
        fprintf(f, "\n");
    } else {
        fprintf(f, "(No log data available)\n");
    }

    // Memory information
    fprintf(f, "\n--- Memory Information ---\n");
    // Try to get memory usage if available
    FILE *meminfo = fopen("/proc/meminfo", "r");
    if (meminfo) {
        char line[256];
        while (fgets(line, sizeof(line), meminfo)) {
            if (strstr(line, "MemTotal") || strstr(line, "MemFree") ||
                strstr(line, "MemAvailable")) {
                fprintf(f, "%s", line);
            }
        }
        fclose(meminfo);
    } else {
        fprintf(f, "(Memory information unavailable)\n");
    }

    // Recent filesystem operations (if tracked)
    fprintf(f, "\n--- Recent Operations ---\n");
    fprintf(f, "Command Line: ");
    // Try to reconstruct command line from environment
    extern char **environ;
    for (i = 0; environ[i]; i++) {
        if (strstr(environ[i], "PWD=")) continue;  // Skip PWD
        if (strlen(environ[i]) < 256) {  // Safety check
            fprintf(f, "%s ", environ[i]);
        }
    }
    fprintf(f, "\n");

    // Log ring buffer
    fprintf(f, "\n--- Last %d bytes of log ---\n", CRASH_LOG_RING_SIZE);
    {
        int start = info->log_ring_pos;
        int count = 0;
        for (i = 0; i < CRASH_LOG_RING_SIZE; i++) {
            int pos = (start + i) % CRASH_LOG_RING_SIZE;
            char c = info->log_ring_buffer[pos];
            if (c != '\0') {
                fputc(c, f);
                count++;
            }
        }
        if (count == 0) {
            fprintf(f, "(Log buffer empty)\n");
        }
    }

    fprintf(f, "\n========================================\n");
    fprintf(f, "END CRASH REPORT\n");
    fprintf(f, "========================================\n");

    if (f != stderr) {
        fclose(f);
    }
}

/*
=================
Crash_HandleCrash

Common crash handling logic
=================
*/
static void Crash_HandleCrash(int sig, void *fault_addr, const char *reason)
{
    int i;

    // Update crash info
    g_crash_info.signal_num = sig;
    g_crash_info.signal_name = Crash_GetSignalName(sig);
    g_crash_info.fault_address = fault_addr;
    g_crash_info.uptime_seconds = (Sys_Milliseconds() - g_start_time) / 1000;

    // Capture stack trace
    Crash_CaptureStackTrace(&g_crash_info);

    // Set safe mode flag for next boot
    Crash_SetSafeModeFlag();

    // Write crash report
    Crash_WriteReport(&g_crash_info, reason);

    // Call registered callbacks
    for (i = 0; i < g_crash_callback_count; i++) {
        if (g_crash_callbacks[i]) {
            g_crash_callbacks[i](&g_crash_info);
        }
    }

    // Print to stderr
    fprintf(stderr, "\n*** CRASH: %s ***\n", g_crash_info.signal_name);
    fprintf(stderr, "Crash report written to: %s\n", CRASH_REPORT_FILENAME);
    fprintf(stderr, "Build: %s (%s)\n", BUILD_ID, BUILD_DATE);
}

#ifndef _WIN32
/*
=================
Crash_UnixSignalHandler
=================
*/
static void Crash_UnixSignalHandler(int sig, siginfo_t *info, void *context)
{
    (void)context;

    void *fault_addr = NULL;
    if (info) {
        fault_addr = info->si_addr;
    }

    Crash_HandleCrash(sig, fault_addr, NULL);

    // Re-raise signal for default handler (core dump if enabled)
    signal(sig, SIG_DFL);
    raise(sig);
}
#else
/*
=================
Crash_WindowsExceptionHandler
=================
*/
LONG WINAPI Crash_WindowsExceptionHandler(EXCEPTION_POINTERS *ExceptionInfo)
{
    void *fault_addr = NULL;
    DWORD code = 0;

    if (ExceptionInfo && ExceptionInfo->ExceptionRecord) {
        code = ExceptionInfo->ExceptionRecord->ExceptionCode;
        fault_addr = (void *)ExceptionInfo->ExceptionRecord->ExceptionAddress;
    }

    Crash_HandleCrash((int)code, fault_addr, "Windows Exception");

    // Try to write minidump
    {
        HANDLE hFile = CreateFileA("crash.dmp", GENERIC_WRITE, 0, NULL,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            MINIDUMP_EXCEPTION_INFORMATION mei;
            mei.ThreadId = GetCurrentThreadId();
            mei.ExceptionPointers = ExceptionInfo;
            mei.ClientPointers = FALSE;

            MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
                              hFile, MiniDumpNormal, &mei, NULL, NULL);
            CloseHandle(hFile);
        }
    }

    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

/*
=================
Crash_GenerateReport

Manually generate a crash report (for testing or soft errors)
=================
*/
void Crash_GenerateReport(const char *reason)
{
    g_crash_info.uptime_seconds = (Sys_Milliseconds() - g_start_time) / 1000;
    Crash_CaptureStackTrace(&g_crash_info);
    Crash_WriteReport(&g_crash_info, reason);
    Com_Printf("Crash report generated: %s\n", CRASH_REPORT_FILENAME);
}

/*
=================
Crash_GetInfo
=================
*/
const crash_info_t *Crash_GetInfo(void)
{
    return &g_crash_info;
}

/*
=================
Crash_RegisterCallback
=================
*/
void Crash_RegisterCallback(crash_callback_t callback)
{
    if (g_crash_callback_count < 8 && callback) {
        g_crash_callbacks[g_crash_callback_count++] = callback;
    }
}

/*
=================
Crash_SetModLoadingContext

Set context for mod loading operations to help debug crashes
=================
*/
void Crash_SetModLoadingContext(const char *modName, const char *operation)
{
    if (modName) {
        Q_strncpyz(g_mod_loading_name, modName, sizeof(g_mod_loading_name));
    } else {
        g_mod_loading_name[0] = '\0';
    }
    
    if (operation) {
        Q_strncpyz(g_mod_loading_operation, operation, sizeof(g_mod_loading_operation));
    } else {
        g_mod_loading_operation[0] = '\0';
    }
}

/*
=================
Crash_ClearModLoadingContext

Clear mod loading context after successful load
=================
*/
void Crash_ClearModLoadingContext(void)
{
    g_mod_loading_name[0] = '\0';
    g_mod_loading_operation[0] = '\0';
}

/*
=================
Crash_ReportModLoad

Report mod loading errors with detailed context
=================
*/
void Crash_ReportModLoad(const char *modName, const char *error)
{
    char report[1024];
    
    Com_Printf(S_COLOR_RED "Mod loading error:\n");
    Com_Printf(S_COLOR_YELLOW "  Mod: %s\n", modName ? modName : "unknown");
    Com_Printf(S_COLOR_YELLOW "  Error: %s\n", error ? error : "unknown error");
    
    if (g_mod_loading_operation[0]) {
        Com_Printf(S_COLOR_YELLOW "  Operation: %s\n", g_mod_loading_operation);
    }
    
    Com_sprintf(report, sizeof(report), "Mod load failure: %s - %s", 
                modName ? modName : "unknown", error ? error : "unknown");
    Crash_GenerateReport(report);
}
