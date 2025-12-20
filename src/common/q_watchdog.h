/*
===========================================================================
Watchdog System - Deadlock and Hang Detection

Provides a mechanism to monitor threads/tasks and detect unresponsiveness.
If a thread fails to "pet" the watchdog within a timeout, a deadlock
condition is assumed, and diagnostics can be triggered.
===========================================================================
*/

#ifndef __Q_WATCHDOG_H__
#define __Q_WATCHDOG_H__

#include "q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

// Configuration defines
#define WATCHDOG_MAX_THREADS        16      // Maximum number of monitored threads
#define WATCHDOG_DEFAULT_TIMEOUT_MS 5000    // Default timeout for a thread heartbeat (5 seconds)

// Thread monitoring structure
typedef struct {
    qboolean        active;                 // Is this slot active?
    const char      *name;                  // Name of the monitored thread/task
    long long       last_heartbeat_time;    // Last time the thread reported being alive (Sys_Milliseconds)
    int             timeout_ms;             // How long before this thread is considered hung
    // Add more diagnostic info here if needed, e.g., current task, callstack
} watchdog_thread_t;

// Initialize the watchdog system (starts monitoring thread)
void Watchdog_Init(void);

// Shutdown the watchdog system
void Watchdog_Shutdown(void);

// Register a thread/task with the watchdog
// Returns an index (handle) to the registered thread, or -1 on failure
int Watchdog_RegisterThread(const char *name, int timeout_ms);

// Unregister a thread/task from the watchdog
void Watchdog_UnregisterThread(int thread_idx);

// "Pet" the watchdog for a specific thread, indicating it's alive
void Watchdog_Pet(int thread_idx);

// Force a check of all monitored threads (usually called by internal watchdog thread)
void Watchdog_Check(void);

#ifdef __cplusplus
}
#endif

#endif // __Q_WATCHDOG_H__

