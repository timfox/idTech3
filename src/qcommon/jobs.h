/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Lock-free job system with thread pool.
Provides work-stealing, job dependencies, priorities, and parallel-for.
===========================================================================
*/

#ifndef JOBS_H
#define JOBS_H

#include "q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

#define JOBS_MAX_WORKERS        16
#define JOBS_QUEUE_CAPACITY     4096
#define JOBS_MAX_PENDING        8192
#define JOBS_INVALID_HANDLE     ((jobHandle_t)-1)

typedef uint32_t jobHandle_t;

typedef enum {
	JOB_PRIORITY_LOW    = 0,
	JOB_PRIORITY_NORMAL = 1,
	JOB_PRIORITY_HIGH   = 2,
	JOB_PRIORITY_COUNT  = 3
} jobPriority_t;

typedef void (*jobFunc_t)( void *data, uint32_t count );

typedef struct {
	jobFunc_t       func;
	void            *data;
	jobPriority_t   priority;
	jobHandle_t     parent;
} jobDesc_t;

qboolean    Jobs_Init( void );
void        Jobs_Shutdown( void );

int         Jobs_WorkerCount( void );

jobHandle_t Jobs_Submit( const jobDesc_t *desc );

/* C23 designated-initialiser helpers (defined after Jobs_Submit). */
static inline jobDesc_t Jobs_MakeDesc( jobFunc_t func, void *data, jobPriority_t priority )
{
	return (jobDesc_t){
		.func     = func,
		.data     = data,
		.priority = priority,
		.parent   = JOBS_INVALID_HANDLE,
	};
}

static inline jobDesc_t Jobs_MakeDescChild( jobFunc_t func, void *data, jobPriority_t priority,
	jobHandle_t parent )
{
	return (jobDesc_t){
		.func     = func,
		.data     = data,
		.priority = priority,
		.parent   = parent,
	};
}

static inline jobHandle_t Jobs_SubmitWork( jobFunc_t func, void *data, jobPriority_t priority )
{
	const jobDesc_t desc = Jobs_MakeDesc( func, data, priority );
	return Jobs_Submit( &desc );
}

jobHandle_t Jobs_ParallelFor( jobFunc_t func, void *data, uint32_t count, uint32_t batchSize, jobPriority_t priority );

void        Jobs_Wait( jobHandle_t handle );

void        Jobs_WaitAll( void );

qboolean    Jobs_IsComplete( jobHandle_t handle );

int         Jobs_PendingCount( void );

void        Jobs_Pump( int maxJobs );

uint32_t    Jobs_GetThreadId( void );

#ifdef __cplusplus
}
#endif

#endif /* JOBS_H */
