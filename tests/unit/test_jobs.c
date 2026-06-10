/*
 * Unit tests: engine job system (ParallelFor index ranges + Jobs_Wait).
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>

#include "qcommon/jobs.h"

#define ASSERT( cond, msg ) do { \
	if ( !(cond) ) { \
		fprintf( stderr, "FAIL: %s\n", (msg) ); \
		return 1; \
	} \
} while ( 0 )

static _Atomic( int ) parallel_sum;

static void sum_index( void *data, uint32_t index )
{
	(void)data;
	atomic_fetch_add_explicit( &parallel_sum, (int)index, memory_order_relaxed );
}

int main( void )
{
	const uint32_t count = 10;
	const uint32_t batchSize = 2;
	const int expected = 45;
	jobHandle_t group;

	if ( !Jobs_Init() ) {
		fprintf( stderr, "SKIP: jobs_enabled=0 or init failed\n" );
		return 0;
	}

	atomic_store_explicit( &parallel_sum, 0, memory_order_relaxed );
	group = Jobs_ParallelFor( sum_index, NULL, count, batchSize, JOB_PRIORITY_NORMAL );
	ASSERT( group != JOBS_INVALID_HANDLE, "ParallelFor returns group handle" );
	Jobs_Wait( group );
	ASSERT( atomic_load_explicit( &parallel_sum, memory_order_relaxed ) == expected,
		"ParallelFor runs each index once" );

	Jobs_Shutdown();
	printf( "PASS: jobs parallel-for\n" );
	return 0;
}
