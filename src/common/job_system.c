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
#include "profiler.h"
#include <string.h>

// Worker thread structure
typedef struct worker_thread_s {
	thread_handle_t handle;
	job_queue_t local_queue;
	int thread_index;
	qboolean running;
	volatile qboolean should_stop;
	mutex_t wait_mutex;
	condition_t work_available;
} worker_thread_t;

// Global job system state
static worker_thread_t s_workers[MAX_WORKER_THREADS];
static int s_num_workers = 0;
static atomic_int_t s_active_workers;
static job_queue_t s_global_queue;
static mutex_t s_global_mutex;
static condition_t s_global_condition;
static condition_t s_scaling_condition;
static mutex_t s_scaling_mutex;

// Load balancer state
static qboolean s_lb_enabled = qtrue;
static int s_min_workers = 1;
static int s_max_workers = MAX_WORKER_THREADS;
static uint64_t s_last_scaling_time = 0;
static const int SCALING_INTERVAL_MS = 100; // Evaluate scaling every 100ms
static float UNUSED_VAR s_avg_job_latency = 0.0f;
static float UNUSED_VAR s_worker_utilization = 0.0f;

// Main-thread completion queue
#define MAX_COMPLETIONS 1024
typedef struct completion_s {
	void (*fn)(void *user);
	void *user;
} completion_t;
static completion_t s_completions[MAX_COMPLETIONS];
static int s_completion_head = 0;
static int s_completion_tail = 0;
static mutex_t s_completion_mutex;
static qboolean s_initialized = qfalse;
static atomic_int_t s_pending_jobs;
static atomic_int_t s_completed_jobs;

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
	TracyCZoneCtx zone_main;
	
	while (!worker->should_stop) {
		// Load balancing: check if this thread should be active
		MUTEX_LOCK(s_scaling_mutex);
		while (worker->thread_index >= atomic_load_explicit(&s_active_workers, memory_order_acquire) && !worker->should_stop) {
			CONDITION_WAIT(s_scaling_condition, s_scaling_mutex);
		}
		MUTEX_UNLOCK(s_scaling_mutex);

		if (worker->should_stop) break;

		job_t *job = NULL;
		
		// Try to get job from local queue first (LOCK-FREE)
		job = JobQueue_Dequeue(&worker->local_queue);
		
		// If no local job, try global queue (LOCK-FREE)
		if (!job) {
			job = JobQueue_Dequeue(&s_global_queue);
		}
		
		// If still no job, try work-stealing from other threads (LOCK-FREE)
		if (!job) {
			for (int i = 0; i < s_num_workers; i++) {
				if (i == worker->thread_index) {
					continue;
				}
				
				job = JobQueue_Steal(&s_workers[i].local_queue);
				
				if (job) {
					break;
				}
			}
		}
		
		// Execute job if we have one
		if (job) {
			if (PROF_ENABLED) {
				PROF_THREAD_NAME("JobWorker");
			}

			PROF_ZONE_BEGIN(zone_main, "JobExecute");
			job->function(job->data);
			PROF_ZONE_END(zone_main);
			job->completed = qtrue;
			
			// Signal completion
			if (job->handle) {
				job->handle->completed = qtrue;
			}

			// Queue completion callback for main thread
			if (job->onComplete) {
				MUTEX_LOCK(s_completion_mutex);
				int next_tail = (s_completion_tail + 1) % MAX_COMPLETIONS;
				if (next_tail == s_completion_head) {
					Com_Printf(S_COLOR_YELLOW "JobSystem: completion queue overflow, dropping callback\n");
				} else {
					s_completions[s_completion_tail].fn = job->onComplete;
					s_completions[s_completion_tail].user = job->onCompleteData;
					s_completion_tail = next_tail;
				}
				MUTEX_UNLOCK(s_completion_mutex);
			}
			
			atomic_fetch_sub_explicit(&s_pending_jobs, 1, memory_order_relaxed);
			atomic_fetch_add_explicit(&s_completed_jobs, 1, memory_order_relaxed);
			
			Z_Free(job);
		} else {
			// No work available, wait for signal
			MUTEX_LOCK(worker->wait_mutex);
			CONDITION_WAIT(worker->work_available, worker->wait_mutex);
			MUTEX_UNLOCK(worker->wait_mutex);
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
	MUTEX_INIT(s_scaling_mutex);
	CONDITION_INIT(s_scaling_condition);
	MUTEX_INIT(s_completion_mutex);

	atomic_init(&s_active_workers, s_num_workers); // Start with all threads active

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
				MUTEX_DESTROY(s_workers[j].wait_mutex);
				CONDITION_DESTROY(s_workers[j].work_available);
			}
	JobQueue_Shutdown(&s_global_queue);
	MUTEX_DESTROY(s_global_mutex);
	CONDITION_DESTROY(s_global_condition);
	MUTEX_DESTROY(s_scaling_mutex);
			CONDITION_DESTROY(s_scaling_condition);
			MUTEX_DESTROY(s_completion_mutex);
			return qfalse;
		}

		worker->thread_index = i;
		worker->running = qtrue;
		worker->should_stop = qfalse;
		MUTEX_INIT(worker->wait_mutex);
		CONDITION_INIT(worker->work_available);

		char thread_name[32];
		Com_sprintf(thread_name, sizeof(thread_name), "WorkerThread_%d", i);

		if (!Thread_Create(&worker->handle, WorkerThread_Main, worker, thread_name, THREAD_PRIORITY_NORMAL)) {
			Com_Printf("JobSystem_Init: Failed to create worker thread %d\n", i);
			// Cleanup
			JobQueue_Shutdown(&worker->local_queue);
			CONDITION_DESTROY(worker->work_available);
			for (int j = 0; j < i; j++) {
				s_workers[j].should_stop = qtrue;
				CONDITION_SIGNAL(s_workers[j].work_available);
				Thread_Join(s_workers[j].handle);
				JobQueue_Shutdown(&s_workers[j].local_queue);
				MUTEX_DESTROY(s_workers[j].wait_mutex);
				CONDITION_DESTROY(s_workers[j].work_available);
			}
	JobQueue_Shutdown(&s_global_queue);
	MUTEX_DESTROY(s_global_mutex);
	CONDITION_DESTROY(s_global_condition);
	MUTEX_DESTROY(s_scaling_mutex);
			CONDITION_DESTROY(s_scaling_condition);
			MUTEX_DESTROY(s_completion_mutex);
			return qfalse;
		}
	}

	atomic_init(&s_pending_jobs, 0);
	atomic_init(&s_completed_jobs, 0);
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
	MUTEX_LOCK(s_scaling_mutex);
	for (int i = 0; i < s_num_workers; i++) {
		s_workers[i].should_stop = qtrue;
		CONDITION_SIGNAL(s_workers[i].work_available);
	}
	CONDITION_BROADCAST(s_scaling_condition);
	MUTEX_UNLOCK(s_scaling_mutex);

	// Wait for all workers to finish
	for (int i = 0; i < s_num_workers; i++) {
		Thread_Join(s_workers[i].handle);
		JobQueue_Shutdown(&s_workers[i].local_queue);
		MUTEX_DESTROY(s_workers[i].wait_mutex);
		CONDITION_DESTROY(s_workers[i].work_available);
	}

	JobQueue_Shutdown(&s_global_queue);
	MUTEX_DESTROY(s_global_mutex);
	CONDITION_DESTROY(s_global_condition);
	MUTEX_DESTROY(s_scaling_mutex);
	CONDITION_DESTROY(s_scaling_condition);
	MUTEX_DESTROY(s_completion_mutex);

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
	return JobSystem_SubmitJobWithCompletion(function, data, priority, NULL, NULL);
}

/*
=================
JobSystem_SubmitJobWithCompletion
=================
*/
job_handle_t *JobSystem_SubmitJobWithCompletion(jobFunction_t function, void *data, jobPriority_t priority,
	void (*onComplete)(void *user), void *onCompleteData)
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
	job->onComplete = onComplete;
	job->onCompleteData = onCompleteData;

	// Enqueue job (prefer local queue for better cache locality)
	int worker_index = Thread_GetCurrentID() % s_num_workers;
	qboolean enqueued = qfalse;

	if (worker_index < s_num_workers) {
		enqueued = JobQueue_Enqueue(&s_workers[worker_index].local_queue, job);
		if (enqueued) {
			MUTEX_LOCK(s_workers[worker_index].wait_mutex);
			CONDITION_SIGNAL(s_workers[worker_index].work_available);
			MUTEX_UNLOCK(s_workers[worker_index].wait_mutex);
		}
	}

	if (!enqueued) {
		// Fall back to global queue
		enqueued = JobQueue_Enqueue(&s_global_queue, job);
		if (enqueued) {
			MUTEX_LOCK(s_global_mutex); // We still need this mutex for the global condition
			CONDITION_BROADCAST(s_global_condition);
			MUTEX_UNLOCK(s_global_mutex);
		}
	}

	if (enqueued) {
		atomic_fetch_add_explicit(&s_pending_jobs, 1, memory_order_relaxed);
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
	return atomic_load_explicit(&s_pending_jobs, memory_order_relaxed);
}

/*
=================
JobSystem_AnalyzeWorkload
Internal: Adjust worker count based on pending jobs and latency
=================
*/
static void JobSystem_AnalyzeWorkload(void) {
	if (!s_lb_enabled || !s_initialized) return;

	uint64_t now = Sys_Milliseconds();
	if (now - s_last_scaling_time < SCALING_INTERVAL_MS) return;
	s_last_scaling_time = now;

	int pending = atomic_load_explicit(&s_pending_jobs, memory_order_relaxed);
	int active = atomic_load_explicit(&s_active_workers, memory_order_relaxed);

	// Calculate target workers based on workload
	int target = active;

	// Heuristic:
	// If pending jobs > 2x current active workers, scale up
	// If pending jobs < 0.5x current active workers, scale down
	if (pending > active * 2 && active < s_num_workers && active < s_max_workers) {
		target = active + 1;
	} else if (pending < active / 2 && active > s_min_workers) {
		target = active - 1;
	}

	if (target != active) {
		MUTEX_LOCK(s_scaling_mutex);
		atomic_store_explicit(&s_active_workers, target, memory_order_release);
		if (target > active) {
			CONDITION_BROADCAST(s_scaling_condition);
		}
		MUTEX_UNLOCK(s_scaling_mutex);
		
		Com_DPrintf("JobSystem: Scaled %s to %d workers (pending jobs: %d)\n", 
			target > active ? "up" : "down", target, pending);
	}

	// Update stats for reporting
	s_worker_utilization = (float)pending / (float)(s_num_workers * 10); // Very rough estimate
	if (s_worker_utilization > 1.0f) s_worker_utilization = 1.0f;
}

void JobSystem_SetLoadBalancerEnabled(qboolean enabled) {
	s_lb_enabled = enabled;
	if (!enabled) {
		MUTEX_LOCK(s_scaling_mutex);
		atomic_store_explicit(&s_active_workers, s_num_workers, memory_order_release);
		CONDITION_BROADCAST(s_scaling_condition);
		MUTEX_UNLOCK(s_scaling_mutex);
	}
}

int JobSystem_GetActiveWorkerCount(void) {
	return atomic_load_explicit(&s_active_workers, memory_order_relaxed);
}

void JobSystem_SetWorkerRange(int min_workers, int max_workers) {
	s_min_workers = Com_Clamp(1, s_num_workers, min_workers);
	s_max_workers = Com_Clamp(s_min_workers, s_num_workers, max_workers);
}

void JobSystem_PrintLoadBalancerStats(void) {
	Com_Printf("=== Job System Load Balancer ===\n");
	Com_Printf("Enabled: %s\n", s_lb_enabled ? "YES" : "NO");
	Com_Printf("Workers: %d active / %d total (Range: %d-%d)\n", 
		atomic_load_explicit(&s_active_workers, memory_order_relaxed), s_num_workers, s_min_workers, s_max_workers);
	Com_Printf("Pending Jobs: %d\n", atomic_load_explicit(&s_pending_jobs, memory_order_relaxed));
	Com_Printf("Estimated Utilization: %.1f%%\n", s_worker_utilization * 100.0f);
}

/*
=================
JobSystem_Update
Update job system (process completed jobs, etc.)
=================
*/
void JobSystem_SetWorkerAffinity(int worker_index, uint64_t mask) {
	if (!s_initialized || worker_index < 0 || worker_index >= s_num_workers) return;
	Thread_SetAffinity(s_workers[worker_index].handle, mask);
	Com_DPrintf("JobSystem: Worker %d affinity set to 0x%llx\n", worker_index, (unsigned long long)mask);
}

void JobSystem_Update(void)
{
	if (!s_initialized) {
		return;
	}

	// Dynamic scaling
	JobSystem_AnalyzeWorkload();

	for (;;) {
		void (*fn)(void *) = NULL;
		void *user = NULL;

		MUTEX_LOCK(s_completion_mutex);
		if (s_completion_head != s_completion_tail) {
			fn = s_completions[s_completion_head].fn;
			user = s_completions[s_completion_head].user;
			s_completion_head = (s_completion_head + 1) % MAX_COMPLETIONS;
		}
		MUTEX_UNLOCK(s_completion_mutex);

		if (!fn) {
			break;
		}

		fn(user);
	}
}

/*
=================
ParallelFor_JobFunc
Job function for parallel for loop
=================
*/
typedef struct {
	parallel_for_func_t func;
	void *userData;
	int start_index;
	int end_index;
} parallel_for_job_data_t;

static void ParallelFor_JobFunc(void *data) {
	parallel_for_job_data_t *job_data = (parallel_for_job_data_t *)data;

	for (int i = job_data->start_index; i < job_data->end_index; i++) {
		job_data->func(i, job_data->userData);
	}
}

/*
=================
JobSystem_ParallelFor
Execute a function in parallel across a range
=================
*/
qboolean JobSystem_ParallelFor(int start, int end, parallel_for_func_t func, void *userData) {
	if (!s_initialized || start >= end) {
		return qfalse;
	}

	int total_iterations = end - start;
	int num_workers = s_num_workers;

	if (num_workers <= 0) {
		// Fallback to single-threaded execution
		for (int i = start; i < end; i++) {
			func(i, userData);
		}
		return qtrue;
	}

	// Divide work among workers
	int iterations_per_worker = total_iterations / num_workers;
	int remaining_iterations = total_iterations % num_workers;

	job_handle_t **handles = (job_handle_t **)Z_Malloc(sizeof(job_handle_t *) * num_workers);
	int handle_count = 0;

	int current_start = start;

	for (int i = 0; i < num_workers; i++) {
		int worker_iterations = iterations_per_worker + (i < remaining_iterations ? 1 : 0);

		if (worker_iterations > 0) {
			parallel_for_job_data_t *job_data = (parallel_for_job_data_t *)Z_Malloc(sizeof(parallel_for_job_data_t));
			job_data->func = func;
			job_data->userData = userData;
			job_data->start_index = current_start;
			job_data->end_index = current_start + worker_iterations;

			handles[handle_count] = JobSystem_SubmitJob(ParallelFor_JobFunc, job_data, JOB_PRIORITY_NORMAL);
			handle_count++;

			current_start += worker_iterations;
		}
	}

	// Wait for all jobs to complete
	for (int i = 0; i < handle_count; i++) {
		JobSystem_WaitForJob(handles[i], 0);
	}

	// Cleanup
	for (int i = 0; i < handle_count; i++) {
		// Note: Job handles are automatically freed by the job system
	}
	Z_Free(handles);

	return qtrue;
}

