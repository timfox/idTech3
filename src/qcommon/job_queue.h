/*
===========================================================================
Job Queue System

Work-stealing job queue for multi-threaded execution.
Inspired by id Tech 7's job system architecture.
===========================================================================
*/

#ifndef __JOB_QUEUE_H__
#define __JOB_QUEUE_H__

#include "q_shared.h"

// Maximum number of worker threads
#define MAX_WORKER_THREADS 16

// Job priority levels
typedef enum {
	JOB_PRIORITY_LOW,
	JOB_PRIORITY_NORMAL,
	JOB_PRIORITY_HIGH,
	JOB_PRIORITY_CRITICAL
} jobPriority_t;

// Job function pointer
typedef void (*jobFunction_t)(void *data);

// Forward declaration
typedef struct job_handle_s job_handle_t;

// Job structure
typedef struct job_s {
	jobFunction_t function;
	void *data;
	jobPriority_t priority;
	qboolean completed;
	int dependencies;  // Number of jobs this depends on
	job_handle_t *handle;  // Handle for completion tracking
	struct job_s *next;
} job_t;

// Job queue (lock-free work-stealing queue)
typedef struct job_queue_s {
	job_t *head;
	job_t *tail;
	volatile int count;
	volatile int max_size;
} job_queue_t;

/*
=================
JobQueue_Init
Initialize a job queue
=================
*/
qboolean JobQueue_Init(job_queue_t *queue, int max_size);

/*
=================
JobQueue_Shutdown
Shutdown a job queue
=================
*/
void JobQueue_Shutdown(job_queue_t *queue);

/*
=================
JobQueue_Enqueue
Add a job to the queue
=================
*/
qboolean JobQueue_Enqueue(job_queue_t *queue, jobFunction_t function, void *data, jobPriority_t priority);

/*
=================
JobQueue_Dequeue
Remove a job from the queue (thread-safe)
=================
*/
job_t *JobQueue_Dequeue(job_queue_t *queue);

/*
=================
JobQueue_Steal
Steal a job from another thread's queue (work-stealing)
=================
*/
job_t *JobQueue_Steal(job_queue_t *queue);

/*
=================
JobQueue_GetCount
Get number of jobs in queue
=================
*/
int JobQueue_GetCount(job_queue_t *queue);

#endif // __JOB_QUEUE_H__

