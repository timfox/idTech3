/*
=================
Job Queue Implementation
=================
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

    // Ensure max_size is a power of 2 for LF_Queue
    int size = 1;
    while (size < max_size) size <<= 1;

    if (!LF_Queue_Init(&queue->queue, size)) {
        return qfalse;
    }

	queue->max_size = size;
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
    job_t *job;
    while (LF_Queue_Dequeue(&queue->queue, (void**)&job)) {
        Z_Free(job);
    }

    LF_Queue_Shutdown(&queue->queue);
}

/*
=================
JobQueue_Enqueue
Add a preallocated job to the queue
=================
*/
qboolean JobQueue_Enqueue(job_queue_t *queue, job_t *job)
{
	if (!queue || !job || !job->function) {
		return qfalse;
	}

    return LF_Queue_Enqueue(&queue->queue, job);
}

/*
=================
JobQueue_Dequeue
Remove a job from the queue (thread-safe)
=================
*/
job_t *JobQueue_Dequeue(job_queue_t *queue)
{
	if (!queue) {
		return NULL;
	}

    job_t *job = NULL;
    if (LF_Queue_Dequeue(&queue->queue, (void**)&job)) {
        return job;
    }
	return NULL;
}

/*
=================
JobQueue_Steal
Steal a job from another thread's queue (work-stealing)
=================
*/
job_t *JobQueue_Steal(job_queue_t *queue)
{
    // For MPMC queue, Steal is same as Dequeue
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
    return (int)LF_Queue_GetCount(&queue->queue);
}
