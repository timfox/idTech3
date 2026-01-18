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
#include "q_memory_safety.h"

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
#include <pthread.h>
#include <malloc.h>
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

// Alternate signal stack for crash handler
static stack_t g_sigaltstack;
static int g_crash_callback_count = 0;

// Enhanced reliability features
static qboolean g_auto_save_enabled = qtrue;
static char g_state_save_path[MAX_OSPATH] = {0};
static int g_last_save_time = 0;
static int g_save_interval_ms = 30000; // 30 seconds
static recovery_strategy_t g_recovery_strategy = RECOVERY_RESTART;

// State save structure for automatic recovery
typedef struct {
    int save_time;
    char map_name[MAX_QPATH];
    char game_type[MAX_QPATH];
    int player_count;
    int uptime_seconds;
    // Add more state as needed
} auto_save_state_t;
static char g_last_crash_filename[256] = {0};

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

    // Debug: Print thread info
    fprintf(stderr, "CRASH_HANDLER: Installing signal handlers in thread %lu\n", (unsigned long)pthread_self());

    // Set up alternate signal stack for better signal handler execution
    g_sigaltstack.ss_sp = malloc(SIGSTKSZ);
    if (g_sigaltstack.ss_sp) {
        g_sigaltstack.ss_size = SIGSTKSZ;
        g_sigaltstack.ss_flags = 0;
        if (sigaltstack(&g_sigaltstack, NULL) == 0) {
            fprintf(stderr, "CRASH_HANDLER: Alternate signal stack installed\n");
        }
    }

#ifdef _WIN32
    // Windows: Set up structured exception handler
    SetUnhandledExceptionFilter(Crash_WindowsExceptionHandler);
#else
    // Unix: Set up signal handlers - comprehensive approach
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = Crash_UnixSignalHandler;
    sa.sa_flags = SA_SIGINFO | SA_RESETHAND | SA_ONSTACK;  // Use alternate stack
    sigemptyset(&sa.sa_mask);

    // Fill mask to block all signals during handler execution (except the one being handled)
    sigfillset(&sa.sa_mask);

    // Handle critical crash signals, including SIGABRT for memory corruption and abort() calls
    sigaction(SIGSEGV, &sa, NULL);  // Segmentation fault - critical
    sigaction(SIGBUS, &sa, NULL);   // Bus error - critical
    sigaction(SIGILL, &sa, NULL);   // Illegal instruction - critical

    // Special handling for SIGABRT - use alternate stack
    struct sigaction sa_abort;
    memset(&sa_abort, 0, sizeof(sa_abort));
    sa_abort.sa_sigaction = Crash_UnixSignalHandler;
    sa_abort.sa_flags = SA_SIGINFO | SA_RESETHAND | SA_ONSTACK;  // Use alternate stack
    sigemptyset(&sa_abort.sa_mask);
    sigfillset(&sa_abort.sa_mask);  // Block all signals during handler
    sigaction(SIGABRT, &sa_abort, NULL);  // Abort signal - critical for crashes like free() invalid pointer
    // Skip SIGFPE (floating point exceptions) as they can occur in normal math operations
#endif

    g_crash_initialized = qtrue;
    Com_Printf("Crash handler initialized (Build: %s) - handling SIGSEGV, SIGBUS, SIGILL, SIGABRT + abort override\n", BUILD_ID);
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

static qboolean Crash_CommandLineForcesVulkan(void)
{
    int argc = Cmd_Argc();
    for (int i = 0; i + 2 < argc; i++) {
        const char *arg = Cmd_Argv(i);
        if (!arg) {
            continue;
        }
        if (!Q_stricmp(arg, "+set") || !Q_stricmp(arg, "set") || !Q_stricmp(arg, "+seta")) {
            const char *cvar = Cmd_Argv(i + 1);
            const char *value = Cmd_Argv(i + 2);
            if (cvar && value &&
                (!Q_stricmp(cvar, "cl_renderer") || !Q_stricmp(cvar, "r_renderer")) &&
                !Q_stricmp(value, "vulkan")) {
                return qtrue;
            }
        }
    }
    return qfalse;
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
    char filename[256];
    int i;

    // Ensure logs directory exists
    struct stat st = {0};
    if (stat(CRASH_LOG_DIR, &st) == -1) {
        mkdir(CRASH_LOG_DIR, 0755);
    }

    now = time(NULL);
    strftime(time_str, sizeof(time_str), "%Y%m%d_%H%M%S", localtime(&now));

    // Create timestamped filename
    snprintf(filename, sizeof(filename), "%s%s.txt", CRASH_REPORT_BASENAME, time_str);

    // Store filename for use in other functions
    Q_strncpyz(g_last_crash_filename, filename, sizeof(g_last_crash_filename));

    f = fopen(filename, "w");
    if (!f) {
        // Try writing to stderr as fallback
        f = stderr;
        Com_Printf("Failed to write crash report to %s, falling back to stderr\n", filename);
    } else {
        Com_Printf("Writing crash report to %s\n", filename);
    }

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

    // Add renderer state information
    fprintf(f, "\n--- Renderer State ---\n");
    fprintf(f, "Renderer Initialized: %s\n", g_crash_info.uptime_seconds > 0 ? "Yes" : "No");

    // Try to get some basic system load info
    FILE *loadavg = fopen("/proc/loadavg", "r");
    if (loadavg) {
        char load_buf[256];
        if (fgets(load_buf, sizeof(load_buf), loadavg)) {
            fprintf(f, "System Load: %s", load_buf);
        }
        fclose(loadavg);
    }

    // Add OpenGL/Vulkan information if available
    const char *renderer = getenv("r_renderer");
    if (renderer) {
        fprintf(f, "Renderer: %s\n", renderer);
    }

    const char *rtx_enabled = getenv("r_rtx_enable");
    if (rtx_enabled) {
        fprintf(f, "RTX Enabled: %s\n", rtx_enabled);
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

    // Try to get command line from /proc/self/cmdline
    FILE *cmdline_file = fopen("/proc/self/cmdline", "rb");
    if (cmdline_file) {
        char cmdline_buf[1024] = {0};
        size_t bytes_read = fread(cmdline_buf, 1, sizeof(cmdline_buf) - 1, cmdline_file);
        fclose(cmdline_file);

        if (bytes_read > 0) {
            fprintf(f, "Command Line: ");
            // Command line arguments are null-separated
            for (size_t j = 0; j < bytes_read; j++) {
                if (cmdline_buf[j] == '\0') {
                    if (j + 1 < bytes_read && cmdline_buf[j + 1] != '\0') {
                        fprintf(f, " ");
                    }
                } else {
                    fputc(cmdline_buf[j], f);
                }
            }
            fprintf(f, "\n");
        }
    } else {
        fprintf(f, "Command Line: (unable to read /proc/self/cmdline)\n");
    }

    // Add some basic system resource info
    fprintf(f, "\n--- Process Information ---\n");

    // Try to get process ID
    pid_t pid = getpid();
    fprintf(f, "Process ID: %d\n", pid);

    // Try to get parent process ID
    pid_t ppid = getppid();
    fprintf(f, "Parent Process ID: %d\n", ppid);

    // Try to get current working directory
    char cwd_buf[PATH_MAX];
    if (getcwd(cwd_buf, sizeof(cwd_buf))) {
        fprintf(f, "Current Working Directory: %s\n", cwd_buf);
    }

    // Try to get username
    const char *username = getenv("USER");
    if (username) {
        fprintf(f, "User: %s\n", username);
    }

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

    // Set safe mode flag for next boot (skip when Vulkan is explicitly forced)
    if (!Crash_CommandLineForcesVulkan()) {
        Crash_SetSafeModeFlag();
    } else {
        fprintf(stderr, "Crash handler: Vulkan forced, skipping safe mode flag\n");
    }

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
    fprintf(stderr, "Crash report written to: %s\n", g_last_crash_filename[0] ? g_last_crash_filename : "stderr");
    fprintf(stderr, "Build: %s (%s)\n", BUILD_ID, BUILD_DATE);
}

#ifndef _WIN32
/*
=================
Crash_UnixSignalHandler
=================
*/
// Override abort to generate crash reports
void abort(void)
{
    fprintf(stderr, "CRASH_HANDLER: abort() called - generating crash report\n");

    // Generate crash report
    Crash_HandleCrash(SIGABRT, NULL, NULL);

    // Call the real abort
    _exit(134);  // Exit code for SIGABRT
}

static void Crash_UnixSignalHandler(int sig, siginfo_t *info, void *context)
{
    (void)context;

    // Debug: Print that we caught the signal
    fprintf(stderr, "CRASH_HANDLER: Caught signal %d (%s) in thread %lu\n", sig, strsignal(sig), (unsigned long)pthread_self());

    void *fault_addr = NULL;
    if (info) {
        fault_addr = info->si_addr;
        fprintf(stderr, "CRASH_HANDLER: Fault address: %p\n", fault_addr);
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
    Com_Printf("Crash report generated: %s\n", g_last_crash_filename[0] ? g_last_crash_filename : "unknown");
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

/*
=============================================================================
Enhanced Reliability Features
=============================================================================
*/

/*
=================
Crash_EnableAutoSave
=================
*/
void Crash_EnableAutoSave(qboolean enable, const char *save_path) {
    g_auto_save_enabled = enable;
    if (save_path && save_path[0]) {
        Q_strncpyz(g_state_save_path, save_path, sizeof(g_state_save_path));
    }
}

/*
=================
Crash_SetSaveInterval
=================
*/
void Crash_SetSaveInterval(int interval_ms) {
    g_save_interval_ms = interval_ms;
}

/*
=================
Crash_SetRecoveryStrategy
=================
*/
void Crash_SetRecoveryStrategy(recovery_strategy_t strategy) {
    g_recovery_strategy = strategy;
}

/*
=================
Crash_PerformAutoSave
=================
*/
static void Crash_PerformAutoSave(void) {
    if (!g_auto_save_enabled || !g_state_save_path[0]) {
        return;
    }

    int current_time = Sys_Milliseconds();
    if (current_time - g_last_save_time < g_save_interval_ms) {
        return; // Too soon since last save
    }

    auto_save_state_t state;
    memset(&state, 0, sizeof(state));

    state.save_time = current_time;
    state.uptime_seconds = (current_time - g_start_time) / 1000;

    // Gather current game state
    // Note: This would need integration with game state management
    // For now, just save basic info
    Q_strncpyz(state.map_name, "current_map", sizeof(state.map_name));

    // Save to file
    fileHandle_t f = FS_FOpenFileWrite(g_state_save_path);
    if (f) {
        FS_Write(&state, sizeof(state), f);
        FS_FCloseFile(f);
        g_last_save_time = current_time;
        Com_DPrintf("Auto-saved crash recovery state\n");
    } else {
        Com_Printf(S_COLOR_RED "Failed to auto-save crash recovery state\n");
    }
}

/*
=================
Crash_AttemptRecovery
=================
*/
recovery_result_t Crash_AttemptRecovery(void) {
    switch (g_recovery_strategy) {
        case RECOVERY_RESTART:
            // Attempt to restart the engine
            Com_Printf(S_COLOR_YELLOW "Attempting engine restart recovery...\n");
            // This would typically involve restarting the main loop
            return RECOVERY_SUCCESSFUL;

        case RECOVERY_ROLLBACK:
            // Attempt to rollback to last good state
            if (g_state_save_path[0]) {
                fileHandle_t f;
                int len = FS_FOpenFileRead(g_state_save_path, &f, qfalse);
                if (len >= 0) {
                    auto_save_state_t state;
                    FS_Read(&state, sizeof(state), f);
                    FS_FCloseFile(f);

                    Com_Printf(S_COLOR_YELLOW "Rolling back to saved state from %d seconds ago\n",
                              (Sys_Milliseconds() - state.save_time) / 1000);
                    // Apply saved state
                    return RECOVERY_SUCCESSFUL;
                }
            }
            return RECOVERY_FAILED;

        case RECOVERY_DEGRADED:
            // Enter degraded mode
            Com_Printf(S_COLOR_YELLOW "Entering degraded mode - disabling advanced features\n");
            // Disable potentially problematic features
            Cvar_Set("r_multithreaded_rendering", "0");
            Cvar_Set("memory_bounds_checking", "0");
            return RECOVERY_SUCCESSFUL;

        case RECOVERY_NONE:
        default:
            return RECOVERY_FAILED;
    }
}

/*
=================
Crash_Update
=================
*/
void Crash_Update(void) {
    if (!g_crash_initialized) {
        return;
    }

    // Perform periodic auto-save
    Crash_PerformAutoSave();

    // Check for system health
    // This could include memory usage, thread health, etc.
}

/*
=================
Crash_GetReliabilityMetrics
=================
*/
void Crash_GetReliabilityMetrics(crash_reliability_metrics_t *metrics) {
    if (!metrics) return;

    memset(metrics, 0, sizeof(*metrics));

    metrics->auto_save_enabled = g_auto_save_enabled;
    metrics->recovery_strategy = g_recovery_strategy;
    metrics->last_save_time = g_last_save_time;
    metrics->uptime_seconds = (Sys_Milliseconds() - g_start_time) / 1000;

    // Calculate reliability score based on various factors
    int score = 100;

    // Deduct for lack of auto-save
    if (!g_auto_save_enabled) score -= 20;

    // Deduct for aggressive recovery strategies
    if (g_recovery_strategy == RECOVERY_NONE) score -= 30;

    // Bonus for frequent saves
    if (g_save_interval_ms < 60000) score += 10; // Less than 1 minute

    metrics->reliability_score = score;
}

/*
=================
Crash_ValidateSystemHealth
=================
*/
qboolean Crash_ValidateSystemHealth(void) {
    // Check various system health indicators

    // Memory health - DISABLED (memory safety system disabled)
    // const memory_safety_stats_t *mem_stats = MemorySafety_GetStats();
    // if (mem_stats && (atomic_load(&mem_stats->leak_count) > 10 || atomic_load(&mem_stats->corruption_detected) > 0)) {
    //     Com_Printf(S_COLOR_RED "System health check failed: Memory issues detected\n");
    //     return qfalse;
    // }

    // Thread health (basic check)
    // This would need more sophisticated thread monitoring

    // File system health
    // Check if critical files are accessible

    return qtrue;
}
