/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Defer system implementation.
Thread-safe queue: workers add, main thread flushes.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "defer.h"

#define DEFER_QUEUE_CAPACITY  256

typedef struct {
	deferFunc_t func;
	void        *data;
} deferEntry_t;

#if defined(_MSC_VER)
#include <windows.h>

static CRITICAL_SECTION deferMutex;
static deferEntry_t     deferQueue[DEFER_QUEUE_CAPACITY];
static volatile LONG   deferHead;
static volatile LONG   deferTail;
static volatile LONG   deferCount;
static qboolean        deferInitialized = qfalse;

#define DEFER_LOCK()    EnterCriticalSection( &deferMutex )
#define DEFER_UNLOCK()  LeaveCriticalSection( &deferMutex )

#else
#include <pthread.h>
#include <stdatomic.h>

static pthread_mutex_t  deferMutex;
static deferEntry_t     deferQueue[DEFER_QUEUE_CAPACITY];
static _Atomic(uint32_t) deferHead;
static _Atomic(uint32_t) deferTail;
static _Atomic(int32_t)  deferCount;
static qboolean         deferInitialized = qfalse;

#define DEFER_LOCK()    pthread_mutex_lock( &deferMutex )
#define DEFER_UNLOCK()  pthread_mutex_unlock( &deferMutex )
#endif

void Defer_Init( void ) {
	if ( deferInitialized ) return;

#if defined(_MSC_VER)
	InitializeCriticalSection( &deferMutex );
	deferHead = deferTail = deferCount = 0;
#else
	pthread_mutex_init( &deferMutex, NULL );
	atomic_store( &deferHead, 0u );
	atomic_store( &deferTail, 0u );
	atomic_store( &deferCount, 0 );
#endif

	deferInitialized = qtrue;
	Com_Printf( "Defer system initialized (queue capacity %d)\n", DEFER_QUEUE_CAPACITY );
}

void Defer_Shutdown( void ) {
	if ( !deferInitialized ) return;

	Defer_Flush();

#if defined(_MSC_VER)
	DeleteCriticalSection( &deferMutex );
#else
	pthread_mutex_destroy( &deferMutex );
#endif

	deferInitialized = qfalse;
	Com_Printf( "Defer system shut down\n" );
}

qboolean Defer_Add( deferFunc_t func, void *data ) {
	uint32_t tail, next;
	int32_t  count;

	if ( !deferInitialized || !func ) return qfalse;

	DEFER_LOCK();

#if defined(_MSC_VER)
	count = deferCount;
	tail  = (uint32_t)deferTail;
#else
	count = atomic_load_explicit( &deferCount, memory_order_acquire );
	tail  = atomic_load_explicit( &deferTail, memory_order_relaxed );
#endif

	if ( count >= (int32_t)DEFER_QUEUE_CAPACITY ) {
		DEFER_UNLOCK();
		Com_DPrintf( S_COLOR_YELLOW "Defer: queue full, dropping callback\n" );
		return qfalse;
	}

	next = ( tail + 1 ) % DEFER_QUEUE_CAPACITY;
	deferQueue[tail].func = func;
	deferQueue[tail].data = data;

#if defined(_MSC_VER)
	deferTail = (LONG)next;
	InterlockedIncrement( &deferCount );
#else
	atomic_store_explicit( &deferTail, next, memory_order_release );
	atomic_fetch_add_explicit( &deferCount, 1, memory_order_relaxed );
#endif

	DEFER_UNLOCK();
	return qtrue;
}

void Defer_Flush( void ) {
	deferEntry_t batch[DEFER_QUEUE_CAPACITY];
	int          n = 0;
	int          i;

	if ( !deferInitialized ) return;

#if defined(_MSC_VER)
	if ( deferCount <= 0 ) return;
#else
	if ( atomic_load_explicit( &deferCount, memory_order_acquire ) <= 0 ) return;
#endif

	DEFER_LOCK();

#if defined(_MSC_VER)
	while ( deferCount > 0 ) {
		uint32_t head = (uint32_t)deferHead;
		batch[n].func = deferQueue[head].func;
		batch[n].data = deferQueue[head].data;
		deferQueue[head].func = NULL;
		deferQueue[head].data = NULL;
		deferHead = (LONG)( ( head + 1 ) % DEFER_QUEUE_CAPACITY );
		InterlockedDecrement( &deferCount );
		n++;
		if ( n >= DEFER_QUEUE_CAPACITY ) break;
	}
#else
	while ( atomic_load_explicit( &deferCount, memory_order_acquire ) > 0 ) {
		uint32_t head = atomic_load_explicit( &deferHead, memory_order_relaxed );
		uint32_t tail = atomic_load_explicit( &deferTail, memory_order_acquire );
		if ( head == tail ) break;

		batch[n].func = deferQueue[head].func;
		batch[n].data = deferQueue[head].data;
		deferQueue[head].func = NULL;
		deferQueue[head].data = NULL;

		atomic_store_explicit( &deferHead, ( head + 1 ) % DEFER_QUEUE_CAPACITY, memory_order_release );
		atomic_fetch_sub_explicit( &deferCount, 1, memory_order_relaxed );
		n++;
		if ( n >= DEFER_QUEUE_CAPACITY ) break;
	}
#endif

	DEFER_UNLOCK();

	for ( i = 0; i < n; i++ ) {
		if ( batch[i].func ) {
			batch[i].func( batch[i].data );
		}
	}
}

int Defer_PendingCount( void ) {
#if defined(_MSC_VER)
	return (int)deferCount;
#else
	return (int)atomic_load_explicit( &deferCount, memory_order_acquire );
#endif
}
