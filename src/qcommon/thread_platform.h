/*
===========================================================================
Cross-Platform Threading Abstraction

Provides unified threading API across Windows, Linux, macOS.
===========================================================================
*/

#ifndef __THREAD_PLATFORM_H__
#define __THREAD_PLATFORM_H__

#include "q_shared.h"

#ifdef _WIN32
	#include <windows.h>
	#include <process.h>
	
	typedef HANDLE thread_handle_t;
	typedef CRITICAL_SECTION mutex_t;
	typedef CONDITION_VARIABLE condition_t;
	typedef SRWLOCK rwlock_t;
	typedef volatile LONG atomic_int_t;
	
	#define THREAD_CALL WINAPI
	#define THREAD_RETURN DWORD
	
	#define MUTEX_INIT(mutex) InitializeCriticalSection(&(mutex))
	#define MUTEX_DESTROY(mutex) DeleteCriticalSection(&(mutex))
	#define MUTEX_LOCK(mutex) EnterCriticalSection(&(mutex))
	#define MUTEX_UNLOCK(mutex) LeaveCriticalSection(&(mutex))
	
	#define CONDITION_INIT(cond) InitializeConditionVariable(&(cond))
	#define CONDITION_DESTROY(cond) (void)0  // Windows doesn't need destroy
	#define CONDITION_WAIT(cond, mutex) SleepConditionVariableCS(&(cond), &(mutex), INFINITE)
	#define CONDITION_SIGNAL(cond) WakeConditionVariable(&(cond))
	#define CONDITION_BROADCAST(cond) WakeAllConditionVariable(&(cond))
	
	#define ATOMIC_INCREMENT(ptr) InterlockedIncrement((LONG volatile*)(ptr))
	#define ATOMIC_DECREMENT(ptr) InterlockedDecrement((LONG volatile*)(ptr))
	#define ATOMIC_ADD(ptr, val) InterlockedAdd((LONG volatile*)(ptr), (val))
	#define ATOMIC_COMPARE_EXCHANGE(ptr, exchange, compare) \
		(InterlockedCompareExchange((LONG volatile*)(ptr), (exchange), (compare)) == (compare))
#else
	#include <pthread.h>
	#include <unistd.h>
	#include <stdatomic.h>
	
	typedef pthread_t thread_handle_t;
	typedef pthread_mutex_t mutex_t;
	typedef pthread_cond_t condition_t;
	typedef pthread_rwlock_t rwlock_t;
	typedef atomic_int atomic_int_t;
	
	#define THREAD_CALL
	#define THREAD_RETURN void*
	
	#define MUTEX_INIT(mutex) pthread_mutex_init(&(mutex), NULL)
	#define MUTEX_DESTROY(mutex) pthread_mutex_destroy(&(mutex))
	#define MUTEX_LOCK(mutex) pthread_mutex_lock(&(mutex))
	#define MUTEX_UNLOCK(mutex) pthread_mutex_unlock(&(mutex))
	
	#define CONDITION_INIT(cond) pthread_cond_init(&(cond), NULL)
	#define CONDITION_DESTROY(cond) pthread_cond_destroy(&(cond))
	#define CONDITION_WAIT(cond, mutex) pthread_cond_wait(&(cond), &(mutex))
	#define CONDITION_SIGNAL(cond) pthread_cond_signal(&(cond))
	#define CONDITION_BROADCAST(cond) pthread_cond_broadcast(&(cond))
	
	#define ATOMIC_INCREMENT(ptr) atomic_fetch_add((ptr), 1)
	#define ATOMIC_DECREMENT(ptr) atomic_fetch_sub((ptr), 1)
	#define ATOMIC_ADD(ptr, val) atomic_fetch_add((ptr), (val))
	#define ATOMIC_COMPARE_EXCHANGE(ptr, exchange, compare) \
		atomic_compare_exchange_strong((ptr), &(compare), (exchange))
#endif

// Thread priority levels
typedef enum {
	THREAD_PRIORITY_LOW,
	THREAD_PRIORITY_NORMAL,
	THREAD_PRIORITY_HIGH,
	THREAD_PRIORITY_CRITICAL
} threadPriority_t;

// Thread function signature
typedef THREAD_RETURN (THREAD_CALL *thread_func_t)(void *arg);

/*
=================
Thread_Create
Create a new thread
handle: Thread handle (output)
func: Thread function
arg: Argument to pass to thread function
name: Thread name (for debugging)
priority: Thread priority
Returns qtrue on success
=================
*/
qboolean Thread_Create(thread_handle_t *handle, thread_func_t func, void *arg, const char *name, threadPriority_t priority);

/*
=================
Thread_Join
Wait for a thread to complete
handle: Thread handle
=================
*/
void Thread_Join(thread_handle_t handle);

/*
=================
Thread_SetPriority
Set thread priority
handle: Thread handle
priority: New priority
=================
*/
void Thread_SetPriority(thread_handle_t handle, threadPriority_t priority);

/*
=================
Thread_Yield
Yield CPU to other threads
=================
*/
void Thread_Yield(void);

/*
=================
Thread_Sleep
Sleep for specified milliseconds
ms: Milliseconds to sleep
=================
*/
void Thread_Sleep(int ms);

/*
=================
Thread_GetCurrentID
Get current thread ID
=================
*/
unsigned long Thread_GetCurrentID(void);

/*
=================
Sys_GetCPUCount
Get number of CPU cores
=================
*/
int Sys_GetCPUCount(void);

#endif // __THREAD_PLATFORM_H__

