/*
===========================================================================
Job System

Multi-threaded job system with work-stealing for parallel execution.
===========================================================================
*/

#ifndef __JOB_SYSTEM_H__
#define __JOB_SYSTEM_H__

#include "q_shared.h"
#include "job_queue.h"

// Maximum number of jobs that can be queued
#define MAX_JOBS 4096

// Job handle for tracking completion
typedef struct job_handle_s {
	volatile qboolean completed;
	void *result;
} job_handle_t;

/*
=================
JobSystem_Init
Initialize the job system
num_threads: Number of worker threads (0 = auto-detect)
=================
*/
qboolean JobSystem_Init(int num_threads);

/*
=================
JobSystem_Shutdown
Shutdown the job system
=================
*/
void JobSystem_Shutdown(void);

/*
=================
JobSystem_SubmitJob
Submit a job for execution
function: Function to execute
data: Data to pass to function
priority: Job priority
Returns job handle for tracking completion
=================
*/
job_handle_t *JobSystem_SubmitJob(jobFunction_t function, void *data, jobPriority_t priority);

/*
=================
JobSystem_SubmitJobWithCompletion
Submit a job and invoke a completion callback on the main thread
function: Worker function
data: Data to pass to worker
priority: Job priority
onComplete: Called from JobSystem_Update on main thread
onCompleteData: Data passed to completion callback
=================
*/
job_handle_t *JobSystem_SubmitJobWithCompletion(jobFunction_t function, void *data, jobPriority_t priority,
	void (*onComplete)(void *user), void *onCompleteData);

/*
=================
JobSystem_WaitForJob
Wait for a job to complete
handle: Job handle returned from SubmitJob
timeout_ms: Timeout in milliseconds (0 = wait forever)
Returns qtrue if job completed, qfalse on timeout
=================
*/
qboolean JobSystem_WaitForJob(job_handle_t *handle, int timeout_ms);

/*
=================
JobSystem_WaitForAllJobs
Wait for all jobs in the system to complete
timeout_ms: Timeout in milliseconds (0 = wait forever)
=================
*/
qboolean JobSystem_WaitForAllJobs(int timeout_ms);

/*
=================
JobSystem_GetNumWorkers
Get number of worker threads
=================
*/
int JobSystem_GetNumWorkers(void);

/*
=================
JobSystem_GetPendingJobCount
Get number of pending jobs
=================
*/
int JobSystem_GetPendingJobCount(void);

/*
=================
JobSystem_Update
Update job system (process completed jobs, etc.)
Should be called once per frame
=================
*/
void JobSystem_Update(void);

/*
=================
JobSystem_ParallelFor
Execute a function in parallel across a range
index: Current iteration index
userData: User data passed to function
=================
*/
typedef void (*parallel_for_func_t)(int index, void *userData);

qboolean JobSystem_ParallelFor(int start, int end, parallel_for_func_t func, void *userData);

#endif // __JOB_SYSTEM_H__

