/*
===========================================================================
Cross-Platform Threading Implementation

Provides unified threading API across Windows, Linux, macOS.
===========================================================================
*/

#ifndef _WIN32
	#define _GNU_SOURCE // For pthread_setname_np (must be before any includes)
#endif

#include "q_shared.h"
#include "qcommon.h"
#include "thread_platform.h"

#ifdef _WIN32
	#include <windows.h>
#else
	#include <pthread.h>
	#include <unistd.h>
	#include <sys/syscall.h>
	#ifdef __linux__
		// pthread_setname_np is available on Linux with _GNU_SOURCE
		#ifndef pthread_setname_np
			// Fallback: use prctl if pthread_setname_np is not available
			#include <sys/prctl.h>
			#define pthread_setname_np(thread, name) prctl(PR_SET_NAME, name)
		#endif
	#endif
#endif

/*
=================
Thread_Create
Create a new thread
=================
*/
qboolean Thread_Create(thread_handle_t *handle, thread_func_t func, void *arg, const char *name, threadPriority_t priority)
{
	if (!handle || !func) {
		return qfalse;
	}

#ifdef _WIN32
	*handle = (HANDLE)_beginthreadex(NULL, 0, (unsigned (__stdcall *)(void *))func, arg, 0, NULL);
	if (*handle == NULL) {
		return qfalse;
	}
	
	// Set thread priority
	int win_priority;
	switch (priority) {
		case THREAD_PRIORITY_LOW:
			win_priority = THREAD_PRIORITY_BELOW_NORMAL;
			break;
		case THREAD_PRIORITY_NORMAL:
			win_priority = THREAD_PRIORITY_NORMAL;
			break;
		case THREAD_PRIORITY_HIGH:
			win_priority = THREAD_PRIORITY_ABOVE_NORMAL;
			break;
		case THREAD_PRIORITY_CRITICAL:
			win_priority = THREAD_PRIORITY_HIGHEST;
			break;
		default:
			win_priority = THREAD_PRIORITY_NORMAL;
	}
	SetThreadPriority(*handle, win_priority);
	
	// Set thread name (Windows 10+)
	if (name) {
		typedef HRESULT (WINAPI *SetThreadDescription_t)(HANDLE, PCWSTR);
		HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
		if (kernel32) {
			SetThreadDescription_t SetThreadDescription = (SetThreadDescription_t)GetProcAddress(kernel32, "SetThreadDescription");
			if (SetThreadDescription) {
				// Convert name to wide string
				int len = strlen(name);
				wchar_t *wname = (wchar_t *)Z_Malloc((len + 1) * sizeof(wchar_t));
				mbstowcs(wname, name, len);
				wname[len] = 0;
				SetThreadDescription(*handle, wname);
				Z_Free(wname);
			}
		}
	}
#else
	int result = pthread_create(handle, NULL, func, arg);
	if (result != 0) {
		return qfalse;
	}
	
	// Set thread name (Linux)
	if (name) {
		pthread_setname_np(*handle, name);
	}
	
	// Set thread priority via scheduling policy
	struct sched_param param;
	int policy;
	pthread_getschedparam(*handle, &policy, &param);
	
	switch (priority) {
		case THREAD_PRIORITY_LOW:
			param.sched_priority = sched_get_priority_min(policy);
			break;
		case THREAD_PRIORITY_NORMAL:
			param.sched_priority = (sched_get_priority_min(policy) + sched_get_priority_max(policy)) / 2;
			break;
		case THREAD_PRIORITY_HIGH:
			param.sched_priority = sched_get_priority_max(policy) - 1;
			break;
		case THREAD_PRIORITY_CRITICAL:
			param.sched_priority = sched_get_priority_max(policy);
			break;
		default:
			param.sched_priority = (sched_get_priority_min(policy) + sched_get_priority_max(policy)) / 2;
	}
	pthread_setschedparam(*handle, policy, &param);
#endif

	return qtrue;
}

/*
=================
Thread_Join
Wait for a thread to complete
=================
*/
void Thread_Join(thread_handle_t handle)
{
#ifdef _WIN32
	WaitForSingleObject(handle, INFINITE);
	CloseHandle(handle);
#else
	pthread_join(handle, NULL);
#endif
}

/*
=================
Thread_SetPriority
Set thread priority
=================
*/
void Thread_SetPriority(thread_handle_t handle, threadPriority_t priority)
{
#ifdef _WIN32
	int win_priority;
	switch (priority) {
		case THREAD_PRIORITY_LOW:
			win_priority = THREAD_PRIORITY_BELOW_NORMAL;
			break;
		case THREAD_PRIORITY_NORMAL:
			win_priority = THREAD_PRIORITY_NORMAL;
			break;
		case THREAD_PRIORITY_HIGH:
			win_priority = THREAD_PRIORITY_ABOVE_NORMAL;
			break;
		case THREAD_PRIORITY_CRITICAL:
			win_priority = THREAD_PRIORITY_HIGHEST;
			break;
		default:
			return;
	}
	SetThreadPriority(handle, win_priority);
#else
	struct sched_param param;
	int policy;
	pthread_getschedparam(handle, &policy, &param);
	
	switch (priority) {
		case THREAD_PRIORITY_LOW:
			param.sched_priority = sched_get_priority_min(policy);
			break;
		case THREAD_PRIORITY_NORMAL:
			param.sched_priority = (sched_get_priority_min(policy) + sched_get_priority_max(policy)) / 2;
			break;
		case THREAD_PRIORITY_HIGH:
			param.sched_priority = sched_get_priority_max(policy) - 1;
			break;
		case THREAD_PRIORITY_CRITICAL:
			param.sched_priority = sched_get_priority_max(policy);
			break;
		default:
			return;
	}
	pthread_setschedparam(handle, policy, &param);
#endif
}

/*
=================
Thread_Yield
Yield CPU to other threads
=================
*/
void Thread_Yield(void)
{
#ifdef _WIN32
	SwitchToThread();
#else
	sched_yield();
#endif
}

/*
=================
Thread_Sleep
Sleep for specified milliseconds
=================
*/
void Thread_Sleep(int ms)
{
#ifdef _WIN32
	Sleep(ms);
#else
	usleep(ms * 1000);
#endif
}

/*
=================
Thread_SetAffinity
Set thread affinity (CPU core pinning)
=================
*/
void Thread_SetAffinity(thread_handle_t handle, uint64_t mask)
{
#ifdef _WIN32
	SetThreadAffinityMask(handle, (DWORD_PTR)mask);
#else
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	for (int i = 0; i < 64; i++) {
		if (mask & ((uint64_t)1 << i)) {
			CPU_SET(i, &cpuset);
		}
	}
	pthread_setaffinity_np(handle, sizeof(cpu_set_t), &cpuset);
#endif
}

/*
=================
Thread_SetCurrentAffinity
Set affinity for current thread
=================
*/
void Thread_SetCurrentAffinity(uint64_t mask)
{
#ifdef _WIN32
	SetThreadAffinityMask(GetCurrentThread(), (DWORD_PTR)mask);
#else
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	for (int i = 0; i < 64; i++) {
		if (mask & ((uint64_t)1 << i)) {
			CPU_SET(i, &cpuset);
		}
	}
	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#endif
}

/*
=================
Thread_GetCurrentID
Get current thread ID
=================
*/
unsigned long Thread_GetCurrentID(void)
{
#ifdef _WIN32
	return GetCurrentThreadId();
#else
	return (unsigned long)syscall(SYS_gettid);
#endif
}

/*
=================
Sys_GetCPUCount
Get number of CPU cores
=================
*/
int Sys_GetCPUCount(void)
{
#ifdef _WIN32
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	return sysinfo.dwNumberOfProcessors;
	#else
	return sysconf(_SC_NPROCESSORS_ONLN);
#endif
}

/*
===========================================================================
Spin Locks
===========================================================================
*/

void SpinLock_Init(spinlock_t *lock) {
    if (!lock) return;
    atomic_init(&lock->lock, 0);
}

void SpinLock_Lock(spinlock_t *lock) {
    if (!lock) return;
    
    int expected = 0;
    int spin_count = 0;
    
    while (!atomic_compare_exchange_weak_explicit(&lock->lock, &expected, 1, 
                                                memory_order_acquire, memory_order_relaxed)) {
        expected = 0;
        
        // Adaptive spinning
        spin_count++;
        if (spin_count < 1024) {
            // CPU specific hint for spin loops
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
            __builtin_ia32_pause();
#elif defined(__aarch64__) || defined(_M_ARM64)
            __asm__ __volatile__("yield");
#endif
        } else if (spin_count < 2048) {
            Thread_Yield();
        } else {
            Thread_Sleep(0); // Force context switch
            spin_count = 1024; // Reset to intermediate spinning
        }
    }
}

qboolean SpinLock_TryLock(spinlock_t *lock) {
    if (!lock) return qfalse;
    
    int expected = 0;
    return atomic_compare_exchange_strong_explicit(&lock->lock, &expected, 1,
                                                 memory_order_acquire, memory_order_relaxed);
}

void SpinLock_Unlock(spinlock_t *lock) {
    if (!lock) return;
    atomic_store_explicit(&lock->lock, 0, memory_order_release);
}

