/*
===========================================================================
Cross-Platform Threading Implementation

Provides unified threading API across Windows, Linux, macOS.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "thread_platform.h"

#ifdef _WIN32
	#include <windows.h>
#else
	#include <pthread.h>
	#include <unistd.h>
	#include <sys/syscall.h>
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
				wchar_t *wname = (wchar_t *)Z_Malloc((len + 1) * sizeof(wchar_t), TAG_STATIC);
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

