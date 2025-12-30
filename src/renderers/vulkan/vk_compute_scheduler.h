/*
=============================================================================
Vulkan Advanced Compute Job Scheduler Header
=============================================================================
*/

#pragma once

#include "tr_local.h"

#ifdef USE_VULKAN

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct vk_compute_job_t vk_compute_job_t;

// Job priority levels
typedef enum {
    COMPUTE_PRIORITY_LOW = 0,
    COMPUTE_PRIORITY_NORMAL = 1,
    COMPUTE_PRIORITY_HIGH = 2,
    COMPUTE_PRIORITY_CRITICAL = 3
} vk_compute_priority_t;

// Job states
typedef enum {
    JOB_STATE_QUEUED = 0,
    JOB_STATE_RUNNING = 1,
    JOB_STATE_COMPLETED = 2,
    JOB_STATE_FAILED = 3,
    JOB_STATE_CANCELLED = 4
} vk_compute_job_state_t;

// Compute job structure
typedef struct vk_compute_job_t {
    // Vulkan objects
    VkCommandBuffer command_buffer;

    // Job metadata
    const char* debug_name;
    void* user_data;

    // Internal fields (implementation detail)
    uint64_t job_id;
    vk_compute_priority_t priority;
    vk_compute_job_state_t state;

} vk_compute_job_t;

// Compute scheduler functions
qboolean vk_compute_scheduler_init(void);
void vk_compute_scheduler_shutdown(void);
uint64_t vk_compute_scheduler_submit_job(vk_compute_job_t* job);
void vk_compute_scheduler_wait_all_jobs(void);
void vk_compute_scheduler_get_stats(uint64_t* submitted, uint64_t* completed, uint64_t* failed,
                                   float* avg_execution_time_us, float* avg_wait_time_us,
                                   float* load_factor, uint32_t* active_jobs, uint32_t* queued_jobs);
qboolean vk_compute_scheduler_cancel_job(uint64_t job_id);

// Compute job management functions
vk_compute_job_t* vk_compute_job_create(const char* debug_name, vk_compute_priority_t priority);
void vk_compute_job_destroy(vk_compute_job_t* job);
void vk_compute_job_add_dependency(vk_compute_job_t* job, uint64_t dependency_job_id);
void vk_compute_job_set_callback(vk_compute_job_t* job, void (*callback)(vk_compute_job_t*, qboolean));

// Access to job properties (for job setup)
vk_compute_priority_t vk_compute_job_get_priority(vk_compute_job_t* job);
vk_compute_job_state_t vk_compute_job_get_state(vk_compute_job_t* job);
uint64_t vk_compute_job_get_id(vk_compute_job_t* job);
const char* vk_compute_job_get_debug_name(vk_compute_job_t* job);

// Job configuration functions
void vk_compute_job_set_command_buffer(vk_compute_job_t* job, VkCommandBuffer cmd_buffer);
void vk_compute_job_add_wait_semaphore(vk_compute_job_t* job, VkSemaphore semaphore, VkPipelineStageFlags stage);
void vk_compute_job_add_signal_semaphore(vk_compute_job_t* job, VkSemaphore semaphore);
void vk_compute_job_set_estimated_duration(vk_compute_job_t* job, uint64_t duration_us);
void vk_compute_job_set_memory_usage(vk_compute_job_t* job, uint64_t memory_kb);
void vk_compute_job_set_user_data(vk_compute_job_t* job, void* user_data);

#ifdef __cplusplus
}
#endif

#endif // USE_VULKAN