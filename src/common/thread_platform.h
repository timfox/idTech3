/*
===========================================================================
Cross-Platform Threading Abstraction

Provides unified threading API across Windows, Linux, macOS.
===========================================================================
*/

#ifndef __THREAD_PLATFORM_H__
#define __THREAD_PLATFORM_H__

#include <stdint.h>

#ifdef _WIN32
	#include <windows.h>
	#include <process.h>
#else
	#include <pthread.h>
	#ifndef __APPLE__
		#include <semaphore.h>
	#endif
#endif

#ifdef _WIN32
	#include <windows.h>
	#include <process.h>
    
    typedef int qboolean;
    #define qfalse 0
    #define qtrue 1
	
	typedef HANDLE thread_handle_t;
	typedef CRITICAL_SECTION mutex_t;
	typedef CONDITION_VARIABLE condition_t;
	typedef SRWLOCK rwlock_t;
	typedef volatile LONG atomic_int_t;
	typedef volatile ULONG atomic_uint_t;
	typedef volatile LONGLONG atomic_int64_t;
	typedef volatile ULONGLONG atomic_uint64_t;
	typedef volatile ULONG_PTR atomic_uintptr_t;
	
    // Spin Lock
    typedef struct {
        atomic_int_t lock;
    } spinlock_t;

    // Semaphore
    typedef HANDLE semaphore_t;

	#define THREAD_CALL WINAPI
	#define THREAD_RETURN DWORD
	
	#define MUTEX_INIT(mutex) InitializeCriticalSection(&(mutex))
	#define MUTEX_DESTROY(mutex) DeleteCriticalSection(&(mutex))
	#define MUTEX_LOCK(mutex) EnterCriticalSection(&(mutex))
	#define MUTEX_UNLOCK(mutex) LeaveCriticalSection(&(mutex))
	
	#define CONDITION_INIT(cond) InitializeConditionVariable(&(cond))
	#define CONDITION_DESTROY(cond) (void)0  // Windows doesn't need destroy
	#define CONDITION_WAIT(cond, mutex) SleepConditionVariableCS(&(cond), &(mutex), INFINITE)
	#define CONDITION_TIMED_WAIT(cond, mutex, timeout_ms) SleepConditionVariableCS(&(cond), &(mutex), (timeout_ms))
	#define CONDITION_SIGNAL(cond) WakeConditionVariable(&(cond))
	#define CONDITION_BROADCAST(cond) WakeAllConditionVariable(&(cond))
	
	#define ATOMIC_INCREMENT(ptr) InterlockedIncrement((LONG volatile*)(ptr))
	#define ATOMIC_DECREMENT(ptr) InterlockedDecrement((LONG volatile*)(ptr))
	#define ATOMIC_ADD(ptr, val) InterlockedAdd((LONG volatile*)(ptr), (val))
	#define ATOMIC_COMPARE_EXCHANGE(ptr, exchange, compare) \
		(InterlockedCompareExchange((LONG volatile*)(ptr), (exchange), (compare)) == (compare))

    #define ATOMIC_ADD64(ptr, val) InterlockedAdd64((LONGLONG volatile*)(ptr), (val))
    #define ATOMIC_INCREMENT64(ptr) InterlockedIncrement64((LONGLONG volatile*)(ptr))
    #define ATOMIC_DECREMENT64(ptr) InterlockedDecrement64((LONGLONG volatile*)(ptr))

    // C11-like memory ordering for Windows
    typedef enum {
        memory_order_relaxed,
        memory_order_consume,
        memory_order_acquire,
        memory_order_release,
        memory_order_acq_rel,
        memory_order_seq_cst
    } memory_order_t;

    #define atomic_init(ptr, val) *(ptr) = (val)
    #define atomic_load_explicit(ptr, order) *(ptr)
    #define atomic_store_explicit(ptr, val, order) *(ptr) = (val)
    #define atomic_compare_exchange_weak_explicit(ptr, expected, desired, success, failure) \
        (InterlockedCompareExchange((LONG volatile*)(ptr), (desired), *(expected)) == *(expected) ? \
        (*(expected) = *(expected), qtrue) : (*(expected) = *(ptr), qfalse))
    #define atomic_fetch_add_explicit(ptr, val, order) InterlockedExchangeAdd((LONG volatile*)(ptr), (val))
    #define atomic_fetch_sub_explicit(ptr, val, order) InterlockedExchangeAdd((LONG volatile*)(ptr), -(val))
    #define atomic_exchange_explicit(ptr, val, order) InterlockedExchange((LONG volatile*)(ptr), (val))

#else
	#ifndef _GNU_SOURCE
	#define _GNU_SOURCE
	#endif
	#include <pthread.h>
	#include <unistd.h>
	#include <stdatomic.h>
    
    #ifndef __Q_SHARED_CORE_H__
    typedef enum { qfalse = 0, qtrue } qboolean;
    #endif
	
	typedef pthread_t thread_handle_t;
	typedef pthread_mutex_t mutex_t;
	typedef pthread_cond_t condition_t;

	// Proper RW lock implementation for POSIX
	// Check if pthread_rwlock is available, otherwise fall back to mutex
	#if defined(_POSIX_RWLOCKS) && (_POSIX_RWLOCKS > 0)
		typedef pthread_rwlock_t rwlock_t;
	#else
		// Fallback to mutex when RW locks are not available
		typedef pthread_mutex_t rwlock_t;
	#endif
	typedef atomic_int atomic_int_t;
	typedef atomic_uint atomic_uint_t;
    typedef atomic_long atomic_int64_t;
	typedef atomic_ulong atomic_uint64_t;
	typedef atomic_uintptr_t atomic_uintptr_t;
    typedef memory_order memory_order_t;
	
    // Spin Lock
    typedef struct {
        atomic_int_t lock;
    } spinlock_t;

    // Semaphore
    #ifdef __APPLE__
        #include <dispatch/dispatch.h>
        typedef dispatch_semaphore_t semaphore_t;
    #else
        #include <semaphore.h>
        typedef sem_t semaphore_t;
    #endif

	#define THREAD_CALL
	#define THREAD_RETURN void*

	#define MUTEX_INIT(mutex) pthread_mutex_init(&(mutex), NULL)
	#define MUTEX_DESTROY(mutex) pthread_mutex_destroy(&(mutex))
	#define MUTEX_LOCK(mutex) pthread_mutex_lock(&(mutex))
	#define MUTEX_UNLOCK(mutex) pthread_mutex_unlock(&(mutex))

	#define CONDITION_INIT(cond) pthread_cond_init(&(cond), NULL)
	#define CONDITION_DESTROY(cond) pthread_cond_destroy(&(cond))
	#define CONDITION_WAIT(cond, mutex) pthread_cond_wait(&(cond), &(mutex))
	#define CONDITION_TIMED_WAIT(cond, mutex, timeout_ms) do { \
		struct timespec ts; \
		struct timeval tv; \
		gettimeofday(&tv, NULL); \
		ts.tv_sec = tv.tv_sec; \
		ts.tv_nsec = tv.tv_usec * 1000; \
		ts.tv_nsec += (timeout_ms) * 1000000; \
		if (ts.tv_nsec >= 1000000000) { \
			ts.tv_sec += ts.tv_nsec / 1000000000; \
			ts.tv_nsec %= 1000000000; \
		} \
		pthread_cond_timedwait(&(cond), &(mutex), &ts); \
	} while(0)
	#define CONDITION_SIGNAL(cond) pthread_cond_signal(&(cond))
	#define CONDITION_BROADCAST(cond) pthread_cond_broadcast(&(cond))

	// RW Lock macros
	#if defined(_POSIX_RWLOCKS) && (_POSIX_RWLOCKS > 0)
		#define RWLOCK_INIT(lock) pthread_rwlock_init(&(lock), NULL)
		#define RWLOCK_DESTROY(lock) pthread_rwlock_destroy(&(lock))
		#define RWLOCK_RDLOCK(lock) pthread_rwlock_rdlock(&(lock))
		#define RWLOCK_WRLOCK(lock) pthread_rwlock_wrlock(&(lock))
		#define RWLOCK_UNLOCK(lock) pthread_rwlock_unlock(&(lock))
	#else
		// Fallback to mutex when RW locks are not available
		#define RWLOCK_INIT(lock) pthread_mutex_init(&(lock), NULL)
		#define RWLOCK_DESTROY(lock) pthread_mutex_destroy(&(lock))
		#define RWLOCK_RDLOCK(lock) pthread_mutex_lock(&(lock))
		#define RWLOCK_WRLOCK(lock) pthread_mutex_lock(&(lock))
		#define RWLOCK_UNLOCK(lock) pthread_mutex_unlock(&(lock))
	#endif

	#define ATOMIC_INCREMENT(ptr) atomic_fetch_add((ptr), 1)
	#define ATOMIC_DECREMENT(ptr) atomic_fetch_sub((ptr), 1)
	#define ATOMIC_ADD(ptr, val) atomic_fetch_add((ptr), (val))
	#define ATOMIC_COMPARE_EXCHANGE(ptr, exchange, compare) \
		atomic_compare_exchange_strong((ptr), &(compare), (exchange))

    #define ATOMIC_ADD64(ptr, val) atomic_fetch_add((ptr), (val))
    #define ATOMIC_INCREMENT64(ptr) atomic_fetch_add((ptr), 1)
    #define ATOMIC_DECREMENT64(ptr) atomic_fetch_sub((ptr), 1)

	// Standard C11 atomics are available - no need to redefine them
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
#ifdef __cplusplus
extern "C" {
#endif
int Sys_GetCPUCount(void);
#ifdef __cplusplus
}
#endif

/*
=================
Thread_SetAffinity
Set thread affinity (CPU core pinning)
mask: Bitmask of CPU cores (1 << core_index)
=================
*/
void Thread_SetAffinity(thread_handle_t handle, uint64_t mask);
void Thread_SetCurrentAffinity(uint64_t mask);

/*
=================
Spin Locks
=================
*/
void SpinLock_Init(spinlock_t *lock);
void SpinLock_Lock(spinlock_t *lock);
qboolean SpinLock_TryLock(spinlock_t *lock);
void SpinLock_Unlock(spinlock_t *lock);

/*
=================
Semaphores
=================
*/
qboolean Semaphore_Init(semaphore_t *semaphore, int initial_count);
void Semaphore_Destroy(semaphore_t *semaphore);
qboolean Semaphore_Wait(semaphore_t *semaphore);
qboolean Semaphore_TryWait(semaphore_t *semaphore);
qboolean Semaphore_TimedWait(semaphore_t *semaphore, int timeout_ms);
void Semaphore_Post(semaphore_t *semaphore);
int Semaphore_GetValue(semaphore_t *semaphore);

/*
=================
Barriers
=================
*/
#ifdef _WIN32
typedef SYNCHRONIZATION_BARRIER barrier_t;
#else
// Use fallback barrier implementation (POSIX barriers have compatibility issues)
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int count;
    int waiting;
    int phase;
} barrier_t;
#endif

qboolean Barrier_Init(barrier_t *barrier, int thread_count);
void Barrier_Destroy(barrier_t *barrier);
qboolean Barrier_Wait(barrier_t *barrier);

#endif // __THREAD_PLATFORM_H__
