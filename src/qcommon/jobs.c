/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Lock-free job system implementation.

Architecture:
  - Fixed thread pool sized to (logical cores - 1), clamped to JOBS_MAX_WORKERS
  - Three priority queues (high / normal / low), each a lock-free MPMC ring buffer
  - Atomic pending counter per job for dependency tracking
  - Workers spin-wait with progressive back-off (yield → usleep)
  - Main thread can participate in work while waiting (work-stealing)

Thread safety:
  - Queue head/tail use atomic compare-and-swap (lock-free)
  - Job slot state uses atomic load/store with acquire/release ordering
  - No mutexes on the hot path; a single condition variable for wake-up
===========================================================================
*/

#define _DEFAULT_SOURCE

#include "q_shared.h"
#include "qcommon.h"
#include "jobs.h"

_Static_assert( JOB_PRIORITY_COUNT == 3, "job priority queue count must match enum" );

#if defined(_MSC_VER)
/* CONDITION_VARIABLE / WakeConditionVariable require Vista+; q_platform.h defaults older. */
#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0600
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
/* Win32 threading via CreateThread + CRITICAL_SECTION + CONDITION_VARIABLE */
#include <windows.h>

typedef enum {
	SLOT_FREE = 0,
	SLOT_PENDING,
	SLOT_RUNNING,
	SLOT_COMPLETE
} slotState_t;

typedef struct {
	jobFunc_t       func;
	void            *data;
	uint32_t        count;
	uint32_t        rangeEnd;
	jobFunc_t       parallelFunc;
	jobPriority_t   priority;
	jobHandle_t     parent;
	volatile LONG   unfinished;
	volatile LONG   state;
} jobSlot_t;

typedef struct {
	volatile LONG   head;
	volatile LONG   tail;
	jobHandle_t     buf[JOBS_QUEUE_CAPACITY];
} jobQueue_t;

static struct {
	qboolean            initialized;
	int                 workerCount;
	HANDLE              workers[JOBS_MAX_WORKERS];
	volatile LONG       shutdownRequested;
	jobSlot_t           slots[JOBS_MAX_PENDING];
	volatile LONG       nextSlot;
	jobQueue_t          queues[JOB_PRIORITY_COUNT];
	CRITICAL_SECTION    wakeMutex;
	CONDITION_VARIABLE  wakeCond;
	volatile LONG       pendingJobCount;
} js;

static cvar_t *jobs_threads;
static cvar_t *jobs_enabled;
static cvar_t *jobs_mainPump;
static __declspec(thread) uint32_t tls_threadId = 0;

static void run_slot_job( jobSlot_t *s ) {
	if ( s->parallelFunc ) {
		for ( uint32_t i = s->count; i < s->rangeEnd; i++ ) {
			s->parallelFunc( s->data, i );
		}
	} else if ( s->func ) {
		s->func( s->data, s->count );
	}
}

static jobSlot_t *slot_get( jobHandle_t h ) {
	return ( h < JOBS_MAX_PENDING ) ? &js.slots[h] : NULL;
}

static void parallel_group_child_done( jobHandle_t groupHandle ) {
	jobSlot_t *p = slot_get( groupHandle );

	if ( !p ) {
		return;
	}
	if ( InterlockedDecrement( &p->unfinished ) == 0 ) {
		InterlockedExchange( &p->state, SLOT_COMPLETE );
	}
}

static qboolean queue_push( jobQueue_t *q, jobHandle_t h ) {
	LONG tail, next;
	for (;;) {
		tail = q->tail;
		next = ( tail + 1 ) % JOBS_QUEUE_CAPACITY;
		if ( next == q->head ) return qfalse;
		if ( InterlockedCompareExchange( &q->tail, next, tail ) == tail ) {
			q->buf[tail] = h;
			return qtrue;
		}
	}
}

static qboolean queue_pop( jobQueue_t *q, jobHandle_t *out ) {
	LONG head, next;
	for (;;) {
		head = q->head;
		if ( head == q->tail ) return qfalse;
		next = ( head + 1 ) % JOBS_QUEUE_CAPACITY;
		if ( InterlockedCompareExchange( &q->head, next, head ) == head ) {
			*out = q->buf[head];
			return qtrue;
		}
	}
}

static jobHandle_t alloc_slot( void ) {
	for ( uint32_t attempt = 0; attempt < JOBS_MAX_PENDING; attempt++ ) {
		uint32_t idx = (uint32_t)InterlockedIncrement( &js.nextSlot ) % JOBS_MAX_PENDING;
		if ( InterlockedCompareExchange( &js.slots[idx].state, SLOT_PENDING, SLOT_FREE ) == SLOT_FREE ) {
			return (jobHandle_t)idx;
		}
	}
	return JOBS_INVALID_HANDLE;
}

static int clamp_job_priority( jobPriority_t priority ) {
	int pri = (int)priority;
	if ( pri < 0 ) {
		pri = 0;
	}
	if ( pri >= JOB_PRIORITY_COUNT ) {
		pri = JOB_PRIORITY_COUNT - 1;
	}
	return pri;
}

static void wake_workers( int n ) {
	EnterCriticalSection( &js.wakeMutex );
	if ( n == 1 ) WakeConditionVariable( &js.wakeCond );
	else WakeAllConditionVariable( &js.wakeCond );
	LeaveCriticalSection( &js.wakeMutex );
}

static void finish_job( jobHandle_t h ) {
	jobSlot_t *s = slot_get( h );
	if ( !s ) return;
	if ( s->parent != JOBS_INVALID_HANDLE ) {
		jobSlot_t *p = slot_get( s->parent );
		if ( p && InterlockedDecrement( &p->unfinished ) == 0 ) {
			InterlockedExchange( &p->state, SLOT_COMPLETE );
		}
	}
	if ( InterlockedDecrement( &s->unfinished ) <= 0 ) {
		InterlockedExchange( &s->state, SLOT_COMPLETE );
	}
	InterlockedDecrement( &js.pendingJobCount );
}

static qboolean try_execute_one( void ) {
	jobHandle_t h;

	for ( int p = JOB_PRIORITY_HIGH; p >= JOB_PRIORITY_LOW; p-- ) {
		if ( queue_pop( &js.queues[p], &h ) ) {
			jobSlot_t *s = slot_get( h );
			if ( !s || s->state != SLOT_PENDING ) {
				return qtrue;
			}
			InterlockedExchange( &s->state, SLOT_RUNNING );
			run_slot_job( s );
			finish_job( h );
			return qtrue;
		}
	}
	return qfalse;
}

static DWORD WINAPI worker_main( LPVOID arg ) {
	uint32_t id = (uint32_t)(uintptr_t)arg;
	int spins = 0;

	tls_threadId = id;
	while ( !js.shutdownRequested ) {
		if ( try_execute_one() ) {
			spins = 0;
			continue;
		}
		spins++;
		if ( spins < 64 ) {
			SwitchToThread();
		} else {
			EnterCriticalSection( &js.wakeMutex );
			SleepConditionVariableCS( &js.wakeCond, &js.wakeMutex, 1 );
			LeaveCriticalSection( &js.wakeMutex );
			spins = 0;
		}
	}
	return 0;
}

qboolean Jobs_Init( void ) {
	int numCores;
	SYSTEM_INFO si;

	if ( js.initialized ) {
		return qtrue;
	}

	Com_Memset( &js, 0, sizeof( js ) );

	jobs_enabled = Cvar_Get( "jobs_enabled", "1", CVAR_ARCHIVE | CVAR_LATCH );
	Cvar_SetDescription( jobs_enabled, "Enable the engine job system (0 = disabled, 1 = enabled)." );
	jobs_threads = Cvar_Get( "jobs_threads", "0", CVAR_ARCHIVE | CVAR_LATCH );
	Cvar_SetDescription( jobs_threads, "Number of worker threads (0 = auto-detect based on CPU cores)." );
	jobs_mainPump = Cvar_Get( "jobs_mainPump", "4", CVAR_ARCHIVE );
	Cvar_SetDescription( jobs_mainPump, "Main thread helps drain the job queue each frame (0 = disabled)." );

	if ( !jobs_enabled->integer ) {
		Com_Printf( "Job system: disabled by cvar\n" );
		return qfalse;
	}

	GetSystemInfo( &si );
	numCores = (int)si.dwNumberOfProcessors;
	if ( numCores < 1 ) {
		numCores = 1;
	}

	js.workerCount = jobs_threads->integer;
	if ( js.workerCount <= 0 ) {
		js.workerCount = numCores - 1;
	}
	if ( js.workerCount < 1 ) {
		js.workerCount = 1;
	}
	if ( js.workerCount > JOBS_MAX_WORKERS ) {
		js.workerCount = JOBS_MAX_WORKERS;
	}

	for ( uint32_t i = 0; i < JOBS_MAX_PENDING; i++ ) {
		js.slots[i].state = SLOT_FREE;
		js.slots[i].parent = JOBS_INVALID_HANDLE;
	}

	InitializeCriticalSection( &js.wakeMutex );
	InitializeConditionVariable( &js.wakeCond );

	for ( int i = 0; i < js.workerCount; i++ ) {
		js.workers[i] = CreateThread( NULL, 0, worker_main, (LPVOID)(uintptr_t)( i + 1 ), 0, NULL );
		if ( !js.workers[i] ) {
			js.workerCount = i;
			break;
		}
	}

	js.initialized = qtrue;
	Com_Printf( "Job system: %d worker threads (%d cores detected, mainPump %d, Win32)\n",
		js.workerCount, numCores, jobs_mainPump ? jobs_mainPump->integer : 0 );
	return qtrue;
}

void Jobs_Shutdown( void ) {
	if ( !js.initialized ) {
		return;
	}

	InterlockedExchange( &js.shutdownRequested, 1 );
	WakeAllConditionVariable( &js.wakeCond );

	for ( int i = 0; i < js.workerCount; i++ ) {
		WaitForSingleObject( js.workers[i], INFINITE );
		CloseHandle( js.workers[i] );
	}

	DeleteCriticalSection( &js.wakeMutex );
	js.initialized = qfalse;
	Com_Printf( "Job system: shut down\n" );
}

int Jobs_WorkerCount( void ) {
	return js.workerCount;
}

jobHandle_t Jobs_Submit( const jobDesc_t *desc ) {
	if ( !js.initialized || !desc || !desc->func ) {
		return JOBS_INVALID_HANDLE;
	}

	jobHandle_t h = alloc_slot();
	if ( h == JOBS_INVALID_HANDLE ) {
		Com_DPrintf( S_COLOR_YELLOW "Job system: queue full, executing inline\n" );
		desc->func( desc->data, 0 );
		return JOBS_INVALID_HANDLE;
	}

	jobSlot_t *s = slot_get( h );
	s->func = desc->func;
	s->data = desc->data;
	s->count = 0;
	s->rangeEnd = 0;
	s->parallelFunc = NULL;
	s->priority = desc->priority;
	s->parent = desc->parent;
	InterlockedExchange( &s->unfinished, 1 );
	InterlockedExchange( &s->state, SLOT_PENDING );

	if ( desc->parent != JOBS_INVALID_HANDLE ) {
		jobSlot_t *p = slot_get( desc->parent );
		if ( p ) {
			InterlockedIncrement( &p->unfinished );
		}
	}

	const int pri = clamp_job_priority( desc->priority );
	if ( !queue_push( &js.queues[pri], h ) ) {
		InterlockedExchange( &s->state, SLOT_RUNNING );
		run_slot_job( s );
		finish_job( h );
		return h;
	}

	InterlockedIncrement( &js.pendingJobCount );
	wake_workers( 1 );
	return h;
}

static jobHandle_t parallel_for_internal( jobFunc_t func, void *data, uint32_t count, uint32_t batchSize, jobPriority_t priority ) {
	jobHandle_t groupHandle;
	jobSlot_t *group;
	uint32_t numBatches;

	if ( !js.initialized || !func || count == 0 ) {
		if ( func ) {
			for ( uint32_t i = 0; i < count; i++ ) {
				func( data, i );
			}
		}
		return JOBS_INVALID_HANDLE;
	}

	if ( batchSize == 0 ) {
		batchSize = 1;
	}
	numBatches = ( count + batchSize - 1 ) / batchSize;

	groupHandle = alloc_slot();
	if ( groupHandle == JOBS_INVALID_HANDLE ) {
		for ( uint32_t i = 0; i < count; i++ ) {
			func( data, i );
		}
		return JOBS_INVALID_HANDLE;
	}

	group = slot_get( groupHandle );
	group->func = NULL;
	group->parallelFunc = NULL;
	group->data = NULL;
	group->count = 0;
	group->rangeEnd = 0;
	group->priority = priority;
	group->parent = JOBS_INVALID_HANDLE;
	InterlockedExchange( &group->unfinished, (LONG)numBatches );

	const int pri = clamp_job_priority( priority );

	for ( uint32_t b = 0; b < numBatches; b++ ) {
		uint32_t start = b * batchSize;
		uint32_t end = start + batchSize;

		if ( end > count ) {
			end = count;
		}

		jobHandle_t bh = alloc_slot();
		if ( bh == JOBS_INVALID_HANDLE ) {
			for ( uint32_t i = start; i < end; i++ ) {
				func( data, i );
			}
			parallel_group_child_done( groupHandle );
			continue;
		}

		jobSlot_t *bs = slot_get( bh );
		bs->func = NULL;
		bs->parallelFunc = func;
		bs->data = data;
		bs->count = start;
		bs->rangeEnd = end;
		bs->priority = priority;
		bs->parent = groupHandle;
		InterlockedExchange( &bs->unfinished, 1 );

		if ( !queue_push( &js.queues[pri], bh ) ) {
			InterlockedExchange( &bs->state, SLOT_RUNNING );
			run_slot_job( bs );
			finish_job( bh );
		} else {
			InterlockedIncrement( &js.pendingJobCount );
		}
	}

	wake_workers( js.workerCount );
	return groupHandle;
}

jobHandle_t Jobs_ParallelFor( jobFunc_t func, void *data, uint32_t count, uint32_t batchSize, jobPriority_t priority ) {
	return parallel_for_internal( func, data, count, batchSize, priority );
}

void Jobs_Pump( int maxJobs ) {
	if ( !js.initialized || maxJobs <= 0 ) {
		return;
	}
	for ( int i = 0; i < maxJobs; i++ ) {
		if ( !try_execute_one() ) {
			break;
		}
	}
}

int Jobs_PendingCount( void ) {
	return (int)js.pendingJobCount;
}

void Jobs_Wait( jobHandle_t handle ) {
	if ( handle == JOBS_INVALID_HANDLE ) {
		return;
	}

	jobSlot_t *s = slot_get( handle );
	if ( !s ) {
		return;
	}

	while ( s->state != SLOT_COMPLETE ) {
		if ( !try_execute_one() ) {
			SwitchToThread();
		}
	}
	InterlockedExchange( &s->state, SLOT_FREE );
}

void Jobs_WaitAll( void ) {
	while ( js.pendingJobCount > 0 ) {
		if ( !try_execute_one() ) {
			SwitchToThread();
		}
	}
}

qboolean Jobs_IsComplete( jobHandle_t handle ) {
	if ( handle == JOBS_INVALID_HANDLE ) {
		return qtrue;
	}

	jobSlot_t *s = slot_get( handle );
	return ( !s || s->state == SLOT_COMPLETE ) ? qtrue : qfalse;
}

uint32_t Jobs_GetThreadId( void ) {
	return tls_threadId;
}

#else /* POSIX (includes MinGW/MSYS2) */

#include <stdatomic.h>
#include <pthread.h>
#include <unistd.h>
#include <sched.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* ---------- job slot ---------- */

typedef enum {
	SLOT_FREE = 0,
	SLOT_PENDING,
	SLOT_RUNNING,
	SLOT_COMPLETE
} slotState_t;

typedef struct {
	jobFunc_t               func;
	void                    *data;
	uint32_t                count;
	uint32_t                rangeEnd;
	jobFunc_t               parallelFunc;
	jobPriority_t           priority;
	jobHandle_t             parent;
	_Atomic(int32_t)        unfinished;
	_Atomic(slotState_t)    state;
} jobSlot_t;

/* ---------- ring buffer queue ---------- */

typedef struct {
	_Atomic(uint32_t)       head;
	_Atomic(uint32_t)       tail;
	jobHandle_t             buf[JOBS_QUEUE_CAPACITY];
} jobQueue_t;

static qboolean queue_push( jobQueue_t *q, jobHandle_t h ) {
	uint32_t tail, next;
	for (;;) {
		tail = atomic_load_explicit( &q->tail, memory_order_relaxed );
		next = ( tail + 1 ) % JOBS_QUEUE_CAPACITY;
		if ( next == atomic_load_explicit( &q->head, memory_order_acquire ) ) {
			return qfalse;
		}
		if ( atomic_compare_exchange_weak_explicit(
				&q->tail, &tail, next,
				memory_order_release, memory_order_relaxed ) ) {
			q->buf[tail] = h;
			return qtrue;
		}
	}
}

static qboolean queue_pop( jobQueue_t *q, jobHandle_t *out ) {
	uint32_t head, next;
	for (;;) {
		head = atomic_load_explicit( &q->head, memory_order_relaxed );
		if ( head == atomic_load_explicit( &q->tail, memory_order_acquire ) ) {
			return qfalse;
		}
		next = ( head + 1 ) % JOBS_QUEUE_CAPACITY;
		if ( atomic_compare_exchange_weak_explicit(
				&q->head, &head, next,
				memory_order_release, memory_order_relaxed ) ) {
			*out = q->buf[head];
			return qtrue;
		}
	}
}

/* ---------- global state ---------- */

static struct {
	qboolean            initialized;
	int                 workerCount;
	pthread_t           workers[JOBS_MAX_WORKERS];
	_Atomic(qboolean)   shutdownRequested;

	jobSlot_t           slots[JOBS_MAX_PENDING];
	_Atomic(uint32_t)   nextSlot;

	jobQueue_t          queues[JOB_PRIORITY_COUNT];

	pthread_mutex_t     wakeMutex;
	pthread_cond_t      wakeCond;
	_Atomic(int32_t)    pendingJobCount;
} js;

static cvar_t *jobs_threads;
static cvar_t *jobs_enabled;
static cvar_t *jobs_mainPump;

static __thread uint32_t tls_threadId = 0;

static jobSlot_t *slot_get( jobHandle_t h ) {
	if ( h >= JOBS_MAX_PENDING ) return NULL;
	return &js.slots[h];
}

static int clamp_job_priority( jobPriority_t priority ) {
	int pri = (int)priority;
	if ( pri < 0 ) {
		pri = 0;
	}
	if ( pri >= JOB_PRIORITY_COUNT ) {
		pri = JOB_PRIORITY_COUNT - 1;
	}
	return pri;
}

static void run_slot_job( jobSlot_t *s ) {
	if ( s->parallelFunc ) {
		for ( uint32_t i = s->count; i < s->rangeEnd; i++ ) {
			s->parallelFunc( s->data, i );
		}
	} else if ( s->func ) {
		s->func( s->data, s->count );
	}
}

static void parallel_group_child_done( jobHandle_t groupHandle ) {
	jobSlot_t *p = slot_get( groupHandle );
	if ( !p ) {
		return;
	}
	int32_t remaining = atomic_fetch_sub_explicit( &p->unfinished, 1, memory_order_acq_rel ) - 1;
	if ( remaining == 0 ) {
		atomic_store_explicit( &p->state, SLOT_COMPLETE, memory_order_release );
	}
}

static jobHandle_t alloc_slot( void ) {
	for ( uint32_t attempt = 0; attempt < JOBS_MAX_PENDING; attempt++ ) {
		uint32_t idx = atomic_fetch_add_explicit( &js.nextSlot, 1, memory_order_relaxed ) % JOBS_MAX_PENDING;
		slotState_t expected = SLOT_FREE;
		if ( atomic_compare_exchange_strong_explicit(
				&js.slots[idx].state, &expected, SLOT_PENDING,
				memory_order_acq_rel, memory_order_relaxed ) ) {
			return (jobHandle_t)idx;
		}
	}
	return JOBS_INVALID_HANDLE;
}

static void wake_workers( int n ) {
	if ( n <= 0 ) return;
	pthread_mutex_lock( &js.wakeMutex );
	if ( n == 1 ) {
		pthread_cond_signal( &js.wakeCond );
	} else {
		pthread_cond_broadcast( &js.wakeCond );
	}
	pthread_mutex_unlock( &js.wakeMutex );
}

static void finish_job( jobHandle_t h ) {
	jobSlot_t *s = slot_get( h );
	if ( !s ) return;

	if ( s->parent != JOBS_INVALID_HANDLE ) {
		jobSlot_t *p = slot_get( s->parent );
		if ( p ) {
			int32_t remaining = atomic_fetch_sub_explicit( &p->unfinished, 1, memory_order_acq_rel ) - 1;
			if ( remaining == 0 ) {
				atomic_store_explicit( &p->state, SLOT_COMPLETE, memory_order_release );
			}
		}
	}

	int32_t remaining = atomic_fetch_sub_explicit( &s->unfinished, 1, memory_order_acq_rel ) - 1;
	if ( remaining <= 0 ) {
		atomic_store_explicit( &s->state, SLOT_COMPLETE, memory_order_release );
	}

	atomic_fetch_sub_explicit( &js.pendingJobCount, 1, memory_order_relaxed );
}

static qboolean try_execute_one( void ) {
	jobHandle_t h;

	for ( int p = JOB_PRIORITY_HIGH; p >= JOB_PRIORITY_LOW; p-- ) {
		if ( queue_pop( &js.queues[p], &h ) ) {
			jobSlot_t *s = slot_get( h );
			if ( !s || atomic_load_explicit( &s->state, memory_order_acquire ) != SLOT_PENDING ) {
				return qtrue;
			}

			atomic_store_explicit( &s->state, SLOT_RUNNING, memory_order_release );
			run_slot_job( s );
			finish_job( h );
			return qtrue;
		}
	}
	return qfalse;
}

/* ---------- worker thread ---------- */

static void *worker_main( void *arg ) {
	uint32_t id = (uint32_t)(uintptr_t)arg;
	tls_threadId = id;
	int spins = 0;

	while ( !atomic_load_explicit( &js.shutdownRequested, memory_order_acquire ) ) {
		if ( try_execute_one() ) {
			spins = 0;
			continue;
		}

		spins++;
		if ( spins < 64 ) {
			sched_yield();
		} else {
			struct timespec ts;
			clock_gettime( CLOCK_REALTIME, &ts );
			ts.tv_nsec += 1000000;  /* 1 ms */
			if ( ts.tv_nsec >= 1000000000 ) {
				ts.tv_sec++;
				ts.tv_nsec -= 1000000000;
			}
			pthread_mutex_lock( &js.wakeMutex );
			pthread_cond_timedwait( &js.wakeCond, &js.wakeMutex, &ts );
			pthread_mutex_unlock( &js.wakeMutex );
			spins = 0;
		}
	}

	return NULL;
}

/* ---------- public API ---------- */

qboolean Jobs_Init( void ) {
	int numCores;

	if ( js.initialized ) {
		return qtrue;
	}

	Com_Memset( &js, 0, sizeof( js ) );

	jobs_enabled = Cvar_Get( "jobs_enabled", "1", CVAR_ARCHIVE | CVAR_LATCH );
	Cvar_SetDescription( jobs_enabled, "Enable the engine job system (0 = disabled, 1 = enabled)." );

	jobs_threads = Cvar_Get( "jobs_threads", "0", CVAR_ARCHIVE | CVAR_LATCH );
	Cvar_SetDescription( jobs_threads, "Number of worker threads (0 = auto-detect based on CPU cores)." );

	jobs_mainPump = Cvar_Get( "jobs_mainPump", "4", CVAR_ARCHIVE );
	Cvar_SetDescription( jobs_mainPump, "Main thread helps drain the job queue each frame (0 = disabled)." );

	if ( !jobs_enabled->integer ) {
		Com_Printf( "Job system: disabled by cvar\n" );
		return qfalse;
	}

#ifdef _WIN32
	{
		SYSTEM_INFO si;
		GetSystemInfo( &si );
		numCores = (int)si.dwNumberOfProcessors;
	}
#else
	numCores = (int)sysconf( _SC_NPROCESSORS_ONLN );
#endif
	if ( numCores < 1 ) numCores = 1;

	js.workerCount = jobs_threads->integer;
	if ( js.workerCount <= 0 ) {
		js.workerCount = numCores - 1;
	}
	if ( js.workerCount < 1 ) js.workerCount = 1;
	if ( js.workerCount > JOBS_MAX_WORKERS ) js.workerCount = JOBS_MAX_WORKERS;

	for ( uint32_t i = 0; i < JOBS_MAX_PENDING; i++ ) {
		atomic_store( &js.slots[i].state, SLOT_FREE );
		atomic_store( &js.slots[i].unfinished, 0 );
		js.slots[i].parent = JOBS_INVALID_HANDLE;
	}

	atomic_store( &js.nextSlot, 0 );
	atomic_store( &js.shutdownRequested, qfalse );
	atomic_store( &js.pendingJobCount, 0 );

	for ( int p = 0; p < JOB_PRIORITY_COUNT; p++ ) {
		atomic_store( &js.queues[p].head, 0 );
		atomic_store( &js.queues[p].tail, 0 );
	}

	pthread_mutex_init( &js.wakeMutex, NULL );
	pthread_cond_init( &js.wakeCond, NULL );

	tls_threadId = 0;

	for ( int i = 0; i < js.workerCount; i++ ) {
		if ( pthread_create( &js.workers[i], NULL, worker_main, (void *)(uintptr_t)( i + 1 ) ) != 0 ) {
			Com_Printf( S_COLOR_RED "Job system: failed to create worker %d\n", i );
			js.workerCount = i;
			break;
		}
	}

	js.initialized = qtrue;

	Com_Printf( "Job system: %d worker threads (%d cores detected, mainPump %d)\n",
		js.workerCount, numCores, jobs_mainPump ? jobs_mainPump->integer : 0 );

	return qtrue;
}

void Jobs_Shutdown( void ) {
	if ( !js.initialized ) return;

	atomic_store_explicit( &js.shutdownRequested, qtrue, memory_order_release );

	pthread_mutex_lock( &js.wakeMutex );
	pthread_cond_broadcast( &js.wakeCond );
	pthread_mutex_unlock( &js.wakeMutex );

	for ( int i = 0; i < js.workerCount; i++ ) {
		pthread_join( js.workers[i], NULL );
	}

	pthread_mutex_destroy( &js.wakeMutex );
	pthread_cond_destroy( &js.wakeCond );

	js.initialized = qfalse;
	Com_Printf( "Job system: shut down\n" );
}

int Jobs_WorkerCount( void ) {
	return js.workerCount;
}

jobHandle_t Jobs_Submit( const jobDesc_t *desc ) {
	if ( !js.initialized || !desc || !desc->func ) {
		return JOBS_INVALID_HANDLE;
	}

	jobHandle_t h = alloc_slot();
	if ( h == JOBS_INVALID_HANDLE ) {
		Com_DPrintf( S_COLOR_YELLOW "Job system: queue full, executing inline\n" );
		desc->func( desc->data, 0 );
		return JOBS_INVALID_HANDLE;
	}

	jobSlot_t *s = slot_get( h );
	s->func     = desc->func;
	s->data     = desc->data;
	s->count    = 0;
	s->rangeEnd = 0;
	s->parallelFunc = NULL;
	s->priority = desc->priority;
	s->parent   = desc->parent;
	atomic_store_explicit( &s->unfinished, 1, memory_order_release );
	atomic_store_explicit( &s->state, SLOT_PENDING, memory_order_release );

	if ( desc->parent != JOBS_INVALID_HANDLE ) {
		jobSlot_t *p = slot_get( desc->parent );
		if ( p ) {
			atomic_fetch_add_explicit( &p->unfinished, 1, memory_order_acq_rel );
		}
	}

	int pri = clamp_job_priority( desc->priority );

	if ( !queue_push( &js.queues[pri], h ) ) {
		Com_DPrintf( S_COLOR_YELLOW "Job system: priority queue %d full, executing inline\n", pri );
		atomic_store_explicit( &s->state, SLOT_RUNNING, memory_order_release );
		run_slot_job( s );
		finish_job( h );
		return h;
	}

	atomic_fetch_add_explicit( &js.pendingJobCount, 1, memory_order_relaxed );
	wake_workers( 1 );

	return h;
}

static jobHandle_t parallel_for_internal( jobFunc_t func, void *data, uint32_t count, uint32_t batchSize, jobPriority_t priority ) {
	uint32_t numBatches;
	jobHandle_t groupHandle;
	jobSlot_t *group;

	if ( !js.initialized || !func || count == 0 ) {
		if ( func ) {
			for ( uint32_t i = 0; i < count; i++ ) {
				func( data, i );
			}
		}
		return JOBS_INVALID_HANDLE;
	}

	if ( batchSize == 0 ) {
		batchSize = 1;
	}
	numBatches = ( count + batchSize - 1 ) / batchSize;

	groupHandle = alloc_slot();
	if ( groupHandle == JOBS_INVALID_HANDLE ) {
		for ( uint32_t i = 0; i < count; i++ ) {
			func( data, i );
		}
		return JOBS_INVALID_HANDLE;
	}

	group = slot_get( groupHandle );
	group->func = NULL;
	group->parallelFunc = NULL;
	group->data = NULL;
	group->count = 0;
	group->rangeEnd = 0;
	group->priority = priority;
	group->parent = JOBS_INVALID_HANDLE;
	atomic_store_explicit( &group->unfinished, (int32_t)numBatches, memory_order_release );

	int pri = clamp_job_priority( priority );

	for ( uint32_t b = 0; b < numBatches; b++ ) {
		uint32_t start = b * batchSize;
		uint32_t end = start + batchSize;
		if ( end > count ) {
			end = count;
		}

		jobHandle_t bh = alloc_slot();
		if ( bh == JOBS_INVALID_HANDLE ) {
			for ( uint32_t i = start; i < end; i++ ) {
				func( data, i );
			}
			parallel_group_child_done( groupHandle );
			continue;
		}

		jobSlot_t *bs = slot_get( bh );
		bs->func = NULL;
		bs->parallelFunc = func;
		bs->data = data;
		bs->count = start;
		bs->rangeEnd = end;
		bs->priority = priority;
		bs->parent = groupHandle;
		atomic_store_explicit( &bs->unfinished, 1, memory_order_release );
		atomic_store_explicit( &bs->state, SLOT_PENDING, memory_order_release );

		if ( !queue_push( &js.queues[pri], bh ) ) {
			atomic_store_explicit( &bs->state, SLOT_RUNNING, memory_order_release );
			run_slot_job( bs );
			finish_job( bh );
		} else {
			atomic_fetch_add_explicit( &js.pendingJobCount, 1, memory_order_relaxed );
		}
	}

	wake_workers( js.workerCount );
	return groupHandle;
}

jobHandle_t Jobs_ParallelFor( jobFunc_t func, void *data, uint32_t count, uint32_t batchSize, jobPriority_t priority ) {
	return parallel_for_internal( func, data, count, batchSize, priority );
}

void Jobs_Pump( int maxJobs ) {
	if ( !js.initialized || maxJobs <= 0 ) {
		return;
	}
	for ( int i = 0; i < maxJobs; i++ ) {
		if ( !try_execute_one() ) {
			break;
		}
	}
}

int Jobs_PendingCount( void ) {
	return (int)atomic_load_explicit( &js.pendingJobCount, memory_order_acquire );
}

void Jobs_Wait( jobHandle_t handle ) {
	if ( handle == JOBS_INVALID_HANDLE ) return;

	jobSlot_t *s = slot_get( handle );
	if ( !s ) return;

	while ( atomic_load_explicit( &s->state, memory_order_acquire ) != SLOT_COMPLETE ) {
		if ( !try_execute_one() ) {
			sched_yield();
		}
	}

	atomic_store_explicit( &s->state, SLOT_FREE, memory_order_release );
}

void Jobs_WaitAll( void ) {
	while ( atomic_load_explicit( &js.pendingJobCount, memory_order_acquire ) > 0 ) {
		if ( !try_execute_one() ) {
			sched_yield();
		}
	}
}

qboolean Jobs_IsComplete( jobHandle_t handle ) {
	if ( handle == JOBS_INVALID_HANDLE ) return qtrue;

	jobSlot_t *s = slot_get( handle );
	if ( !s ) return qtrue;

	return atomic_load_explicit( &s->state, memory_order_acquire ) == SLOT_COMPLETE;
}

uint32_t Jobs_GetThreadId( void ) {
	return tls_threadId;
}

#endif /* !_MSC_VER */
