/*
===========================================================================
Watchdog System Implementation
===========================================================================
*/

#include "q_watchdog.h"
#include "qcommon.h"
#include "crash_handler.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

static watchdog_thread_t g_watchdog_threads[WATCHDOG_MAX_THREADS];
static qboolean g_watchdog_initialized = qfalse;
static qboolean g_watchdog_running = qfalse;

#ifdef _WIN32
static HANDLE g_watchdog_thread_handle = NULL;
static unsigned int g_watchdog_thread_id;
#else
static pthread_t g_watchdog_pthread;
#endif

// Forward declarations
#ifdef _WIN32
static unsigned int WINAPI Watchdog_Thread(void *arg);
#else
static void *Watchdog_Thread(void *arg);
#endif

/*
=================
Watchdog_Init
=================
*/
void Watchdog_Init(void)
{
    if (g_watchdog_initialized) {
        return;
    }

    memset(g_watchdog_threads, 0, sizeof(g_watchdog_threads));

    g_watchdog_running = qtrue;

#ifdef _WIN32
    g_watchdog_thread_handle = (HANDLE)_beginthreadex(NULL, 0, Watchdog_Thread, NULL, 0, &g_watchdog_thread_id);
    if (g_watchdog_thread_handle == 0) {
        Com_Printf(S_COLOR_RED "ERROR: Failed to create watchdog thread!\n");
        g_watchdog_running = qfalse;
    }
#else
    if (pthread_create(&g_watchdog_pthread, NULL, Watchdog_Thread, NULL) != 0) {
        Com_Printf(S_COLOR_RED "ERROR: Failed to create watchdog thread!\n");
        g_watchdog_running = qfalse;
    }
#endif

    g_watchdog_initialized = qtrue;
    Com_Printf("Watchdog system initialized.\n");
}

/*
=================
Watchdog_Shutdown
=================
*/
void Watchdog_Shutdown(void)
{
    if (!g_watchdog_initialized) {
        return;
    }

    g_watchdog_running = qfalse;

#ifdef _WIN32
    if (g_watchdog_thread_handle) {
        WaitForSingleObject(g_watchdog_thread_handle, INFINITE);
        CloseHandle(g_watchdog_thread_handle);
        g_watchdog_thread_handle = NULL;
    }
#else
    if (g_watchdog_pthread) {
        pthread_join(g_watchdog_pthread, NULL);
    }
#endif

    g_watchdog_initialized = qfalse;
    Com_Printf("Watchdog system shut down.\n");
}

/*
=================
Watchdog_RegisterThread
=================
*/
int Watchdog_RegisterThread(const char *name, int timeout_ms)
{
    int i;
    if (!g_watchdog_initialized) {
        Com_Printf(S_COLOR_YELLOW "WARNING: Watchdog not initialized, cannot register thread '%s'.\n", name);
        return -1;
    }

    for (i = 0; i < WATCHDOG_MAX_THREADS; i++) {
        if (!g_watchdog_threads[i].active) {
            g_watchdog_threads[i].active = qtrue;
            g_watchdog_threads[i].name = name;
            g_watchdog_threads[i].last_heartbeat_time = Sys_Milliseconds();
            g_watchdog_threads[i].timeout_ms = (timeout_ms > 0) ? timeout_ms : WATCHDOG_DEFAULT_TIMEOUT_MS;
            Com_Printf("Watchdog: Registered thread '%s' with timeout %dms (idx %d).\n", name, g_watchdog_threads[i].timeout_ms, i);
            return i;
        }
    }

    Com_Printf(S_COLOR_RED "ERROR: Watchdog: No free slots to register thread '%s'.\n", name);
    return -1;
}

/*
=================
Watchdog_UnregisterThread
=================
*/
void Watchdog_UnregisterThread(int thread_idx)
{
    if (!g_watchdog_initialized || thread_idx < 0 || thread_idx >= WATCHDOG_MAX_THREADS) {
        return;
    }
    if (g_watchdog_threads[thread_idx].active) {
        Com_Printf("Watchdog: Unregistered thread '%s' (idx %d).\n", g_watchdog_threads[thread_idx].name, thread_idx);
        g_watchdog_threads[thread_idx].active = qfalse;
        g_watchdog_threads[thread_idx].name = NULL;
    }
}

/*
=================
Watchdog_Pet
=================
*/
void Watchdog_Pet(int thread_idx)
{
    if (!g_watchdog_initialized || thread_idx < 0 || thread_idx >= WATCHDOG_MAX_THREADS) {
        return;
    }
    if (g_watchdog_threads[thread_idx].active) {
        g_watchdog_threads[thread_idx].last_heartbeat_time = Sys_Milliseconds();
    }
}

/*
=================
Watchdog_Check

Iterate through all monitored threads and check for timeouts
=================
*/
void Watchdog_Check(void)
{
    long long current_time = Sys_Milliseconds();
    int i;

    for (i = 0; i < WATCHDOG_MAX_THREADS; i++) {
        if (g_watchdog_threads[i].active) {
            if (current_time - g_watchdog_threads[i].last_heartbeat_time > g_watchdog_threads[i].timeout_ms) {
                // Thread has hung!
                Com_Printf(S_COLOR_RED "FATAL ERROR: Watchdog detected hang in thread '%s'!\n", g_watchdog_threads[i].name);
                Crash_GenerateReport(va("Watchdog: Thread '%s' hung", g_watchdog_threads[i].name));
                Sys_Error("Watchdog: Thread '%s' hung", g_watchdog_threads[i].name);
            }
        }
    }
}

/*
=================
Watchdog_Thread

Separate thread that periodically checks for hung tasks.
=================
*/
#ifdef _WIN32
static unsigned int WINAPI Watchdog_Thread(void *arg)
#else
static void *Watchdog_Thread(void *arg)
#endif
{
    (void)arg;
    Com_Printf("Watchdog monitoring thread started.\n");

    while (g_watchdog_running) {
#ifdef _WIN32
        Sleep(1000); // Check every second on Windows
#else
        sleep(1);    // Check every second on Unix
#endif
        if (g_watchdog_running) {
            Watchdog_Check();
        }
    }

    Com_Printf("Watchdog monitoring thread stopped.\n");
#ifdef _WIN32
    _endthreadex(0);
    return 0;
#else
    return NULL;
#endif
}

