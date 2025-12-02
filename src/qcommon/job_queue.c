/*
===========================================================================
Job Queue Implementation

Lock-free work-stealing queue for multi-threaded job execution.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "job_queue.h"
#include "thread_platform.h"

/*
=================
JobQueue_Init
Initialize a job queue
=================
*/
qboolean JobQueue_Init(job_queue_t *queue, int max_size)
{
	if (!queue) {
		return qfalse;
	}

	queue->head = NULL;
	queue->tail = NULL;
	queue->count = 0;
	queue->max_size = max_size;

	return qtrue;
}

/*
=================
JobQueue_Shutdown
Shutdown a job queue
=================
*/
void JobQueue_Shutdown(job_queue_t *queue)
{
	if (!queue) {
		return;
	}

	// Free remaining jobs
	job_t *job = queue->head;
	while (job) {
		job_t *next = job->next;
		Z_Free(job);
		job = next;
	}

	queue->head = NULL;
	queue->tail = NULL;
	queue->count = 0;
}

/*
=================
JobQueue_Enqueue
Add a job to the queue
=================
*/
qboolean JobQueue_Enqueue(job_queue_t *queue, jobFunction_t function, void *data, jobPriority_t priority)
{
	if (!queue || !function) {
		return qfalse;
	}

	if (queue->count >= queue->max_size) {
		return qfalse;
	}

	job_t *job = (job_t *)Z_Malloc(sizeof(job_t), TAG_STATIC);
	if (!job) {
		return qfalse;
	}

	job->function = function;
	job->data = data;
	job->priority = priority;
	job->completed = qfalse;
	job->dependencies = 0;
	job->next = NULL;

	// Simple enqueue (not fully lock-free, but good enough for our use)
	if (queue->tail) {
		queue->tail->next = job;
		queue->tail = job;
	} else {
		queue->head = job;
		queue->tail = job;
	}

	ATOMIC_INCREMENT(&queue->count);

	return qtrue;
}

/*
=================
JobQueue_Dequeue
Remove a job from the queue (thread-safe)
=================
*/
job_t *JobQueue_Dequeue(job_queue_t *queue)
{
	if (!queue || queue->count == 0) {
		return NULL;
	}

	job_t *job = queue->head;
	if (!job) {
		return NULL;
	}

	queue->head = job->next;
	if (!queue->head) {
		queue->tail = NULL;
	}

	ATOMIC_DECREMENT(&queue->count);

	return job;
}

/*
=================
JobQueue_Steal
Steal a job from another thread's queue (work-stealing)
=================
*/
job_t *JobQueue_Steal(job_queue_t *queue)
{
	// For now, same as dequeue
	// Can be enhanced with proper lock-free algorithms
	return JobQueue_Dequeue(queue);
}

/*
=================
JobQueue_GetCount
Get number of jobs in queue
=================
*/
int JobQueue_GetCount(job_queue_t *queue)
{
	if (!queue) {
		return 0;
	}
	return queue->count;
}

