/*
===========================================================================
Job System Implementation

Multi-threaded job system with work-stealing for parallel execution.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "job_system.h"
#include "thread_platform.h"
#include <string.h>

// Worker thread structure
typedef struct worker_thread_s {
	thread_handle_t handle;
	job_queue_t local_queue;
	int thread_index;
	qboolean running;
	volatile qboolean should_stop;
	mutex_t queue_mutex;
	condition_t work_available;
} worker_thread_t;

// Global job system state
static worker_thread_t s_workers[MAX_WORKER_THREADS];
static int s_num_workers = 0;
static job_queue_t s_global_queue;
static mutex_t s_global_mutex;
static condition_t s_global_condition;
static qboolean s_initialized = qfalse;
static volatile int s_pending_jobs = 0;
static volatile int s_completed_jobs = 0;

// Job pool for allocation (reserved for future use)
// #define JOB_POOL_SIZE 1024
// static job_t s_job_pool[JOB_POOL_SIZE];
// static volatile int s_job_pool_index = 0;

/*
=================
WorkerThread_Main
Main function for worker threads
=================
*/
static THREAD_RETURN THREAD_CALL WorkerThread_Main(void *arg)
{
	worker_thread_t *worker = (worker_thread_t *)arg;
	
	while (!worker->should_stop) {
		job_t *job = NULL;
		
		// Try to get job from local queue first
		MUTEX_LOCK(worker->queue_mutex);
		job = JobQueue_Dequeue(&worker->local_queue);
		MUTEX_UNLOCK(worker->queue_mutex);
		
		// If no local job, try global queue
		if (!job) {
			MUTEX_LOCK(s_global_mutex);
			job = JobQueue_Dequeue(&s_global_queue);
			MUTEX_UNLOCK(s_global_mutex);
		}
		
		// If still no job, try work-stealing from other threads
		if (!job) {
			for (int i = 0; i < s_num_workers; i++) {
				if (i == worker->thread_index) {
					continue;
				}
				
				MUTEX_LOCK(s_workers[i].queue_mutex);
				job = JobQueue_Steal(&s_workers[i].local_queue);
				MUTEX_UNLOCK(s_workers[i].queue_mutex);
				
				if (job) {
					break;
				}
			}
		}
		
		// Execute job if we have one
		if (job) {
			job->function(job->data);
			job->completed = qtrue;
			
			// Signal completion
			if (job->handle) {
				job->handle->completed = qtrue;
			}
			
			ATOMIC_DECREMENT(&s_pending_jobs);
			ATOMIC_INCREMENT(&s_completed_jobs);
			
			Z_Free(job);
		} else {
			// No work available, wait for signal
			MUTEX_LOCK(worker->queue_mutex);
			CONDITION_WAIT(worker->work_available, worker->queue_mutex);
			MUTEX_UNLOCK(worker->queue_mutex);
		}
	}
	
#ifdef _WIN32
	return 0;
#else
	return NULL;
#endif
}

/*
=================
JobSystem_Init
Initialize the job system
=================
*/
qboolean JobSystem_Init(int num_threads)
{
	if (s_initialized) {
		return qtrue;
	}

	if (num_threads <= 0) {
		num_threads = Sys_GetCPUCount() - 1;  // Leave one core for main thread
		if (num_threads < 1) {
			num_threads = 1;
		}
		if (num_threads > MAX_WORKER_THREADS) {
			num_threads = MAX_WORKER_THREADS;
		}
	}

	s_num_workers = num_threads;

	// Initialize global queue
	if (!JobQueue_Init(&s_global_queue, MAX_JOBS)) {
		return qfalse;
	}

	MUTEX_INIT(s_global_mutex);
	CONDITION_INIT(s_global_condition);

	// Initialize worker threads
	for (int i = 0; i < s_num_workers; i++) {
		worker_thread_t *worker = &s_workers[i];
		
		if (!JobQueue_Init(&worker->local_queue, MAX_JOBS / s_num_workers)) {
			// Cleanup on failure
			for (int j = 0; j < i; j++) {
				s_workers[j].should_stop = qtrue;
				CONDITION_SIGNAL(s_workers[j].work_available);
				Thread_Join(s_workers[j].handle);
				JobQueue_Shutdown(&s_workers[j].local_queue);
				MUTEX_DESTROY(s_workers[j].queue_mutex);
				CONDITION_DESTROY(s_workers[j].work_available);
			}
			JobQueue_Shutdown(&s_global_queue);
			MUTEX_DESTROY(s_global_mutex);
			CONDITION_DESTROY(s_global_condition);
			return qfalse;
		}

		worker->thread_index = i;
		worker->running = qtrue;
		worker->should_stop = qfalse;
		MUTEX_INIT(worker->queue_mutex);
		CONDITION_INIT(worker->work_available);

		char thread_name[32];
		Com_sprintf(thread_name, sizeof(thread_name), "WorkerThread_%d", i);

		if (!Thread_Create(&worker->handle, WorkerThread_Main, worker, thread_name, THREAD_PRIORITY_NORMAL)) {
			Com_Printf("JobSystem_Init: Failed to create worker thread %d\n", i);
			// Cleanup
			JobQueue_Shutdown(&worker->local_queue);
			MUTEX_DESTROY(worker->queue_mutex);
			CONDITION_DESTROY(worker->work_available);
			for (int j = 0; j < i; j++) {
				s_workers[j].should_stop = qtrue;
				CONDITION_SIGNAL(s_workers[j].work_available);
				Thread_Join(s_workers[j].handle);
				JobQueue_Shutdown(&s_workers[j].local_queue);
				MUTEX_DESTROY(s_workers[j].queue_mutex);
				CONDITION_DESTROY(s_workers[j].work_available);
			}
			JobQueue_Shutdown(&s_global_queue);
			MUTEX_DESTROY(s_global_mutex);
			CONDITION_DESTROY(s_global_condition);
			return qfalse;
		}
	}

	s_pending_jobs = 0;
	s_completed_jobs = 0;
	s_initialized = qtrue;

	Com_Printf("JobSystem_Init: Initialized with %d worker threads\n", s_num_workers);

	return qtrue;
}

/*
=================
JobSystem_Shutdown
Shutdown the job system
=================
*/
void JobSystem_Shutdown(void)
{
	if (!s_initialized) {
		return;
	}

	// Signal all workers to stop
	for (int i = 0; i < s_num_workers; i++) {
		s_workers[i].should_stop = qtrue;
		CONDITION_SIGNAL(s_workers[i].work_available);
	}

	// Wait for all workers to finish
	for (int i = 0; i < s_num_workers; i++) {
		Thread_Join(s_workers[i].handle);
		JobQueue_Shutdown(&s_workers[i].local_queue);
		MUTEX_DESTROY(s_workers[i].queue_mutex);
		CONDITION_DESTROY(s_workers[i].work_available);
	}

	JobQueue_Shutdown(&s_global_queue);
	MUTEX_DESTROY(s_global_mutex);
	CONDITION_DESTROY(s_global_condition);

	s_num_workers = 0;
	s_initialized = qfalse;
}

/*
=================
JobSystem_SubmitJob
Submit a job for execution
=================
*/
job_handle_t *JobSystem_SubmitJob(jobFunction_t function, void *data, jobPriority_t priority)
{
	if (!s_initialized || !function) {
		return NULL;
	}

	// Allocate job handle
	job_handle_t *handle = (job_handle_t *)Z_Malloc(sizeof(job_handle_t));
	if (!handle) {
		return NULL;
	}

	handle->completed = qfalse;
	handle->result = NULL;

	// Create job
	job_t *job = (job_t *)Z_Malloc(sizeof(job_t));
	if (!job) {
		Z_Free(handle);
		return NULL;
	}

	job->function = function;
	job->data = data;
	job->priority = priority;
	job->completed = qfalse;
	job->dependencies = 0;
	job->next = NULL;
	job->handle = handle;  // Store handle for completion notification

	// Enqueue job (prefer local queue for better cache locality)
	int worker_index = Thread_GetCurrentID() % s_num_workers;
	qboolean enqueued = qfalse;

	if (worker_index < s_num_workers) {
		MUTEX_LOCK(s_workers[worker_index].queue_mutex);
		enqueued = JobQueue_Enqueue(&s_workers[worker_index].local_queue, function, data, priority);
		if (enqueued) {
			CONDITION_SIGNAL(s_workers[worker_index].work_available);
		}
		MUTEX_UNLOCK(s_workers[worker_index].queue_mutex);
	}

	if (!enqueued) {
		// Fall back to global queue
		MUTEX_LOCK(s_global_mutex);
		enqueued = JobQueue_Enqueue(&s_global_queue, function, data, priority);
		if (enqueued) {
			CONDITION_BROADCAST(s_global_condition);
		}
		MUTEX_UNLOCK(s_global_mutex);
	}

	if (enqueued) {
		ATOMIC_INCREMENT(&s_pending_jobs);
		return handle;
	} else {
		Z_Free(job);
		Z_Free(handle);
		return NULL;
	}
}

/*
=================
JobSystem_WaitForJob
Wait for a job to complete
=================
*/
qboolean JobSystem_WaitForJob(job_handle_t *handle, int timeout_ms)
{
	if (!handle) {
		return qfalse;
	}

	if (handle->completed) {
		return qtrue;
	}

	// Simple spin-wait (can be enhanced with condition variables)
	int start_time = Sys_Milliseconds();
	while (!handle->completed) {
		if (timeout_ms > 0) {
			int elapsed = Sys_Milliseconds() - start_time;
			if (elapsed >= timeout_ms) {
				return qfalse;
			}
		}
		Thread_Yield();
	}

	return qtrue;
}

/*
=================
JobSystem_WaitForAllJobs
Wait for all jobs in the system to complete
=================
*/
qboolean JobSystem_WaitForAllJobs(int timeout_ms)
{
	int start_time = Sys_Milliseconds();
	
	while (s_pending_jobs > 0) {
		if (timeout_ms > 0) {
			int elapsed = Sys_Milliseconds() - start_time;
			if (elapsed >= timeout_ms) {
				return qfalse;
			}
		}
		Thread_Yield();
	}

	return qtrue;
}

/*
=================
JobSystem_GetNumWorkers
Get number of worker threads
=================
*/
int JobSystem_GetNumWorkers(void)
{
	return s_num_workers;
}

/*
=================
JobSystem_GetPendingJobCount
Get number of pending jobs
=================
*/
int JobSystem_GetPendingJobCount(void)
{
	return s_pending_jobs;
}

/*
=================
JobSystem_Update
Update job system (process completed jobs, etc.)
=================
*/
void JobSystem_Update(void)
{
	// This can be used for cleanup, statistics, etc.
	// For now, it's a placeholder
}

