#ifndef __VK_COMPUTE_H__
#define __VK_COMPUTE_H__

#include <vulkan/vulkan.h>
#include <stdint.h>
#include "../../common/q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_COMPUTE_JOBS 64
#define MAX_DEPENDENCIES 8
#define NUM_COMMAND_BUFFERS 2	// number of command buffers / render semaphores / framebuffer sets

// Compute job priority
typedef enum {
    VK_COMPUTE_PRIORITY_LOW = 0,
    VK_COMPUTE_PRIORITY_NORMAL,
    VK_COMPUTE_PRIORITY_HIGH,
    VK_COMPUTE_PRIORITY_URGENT
} vk_compute_priority_t;

// Compute job status
typedef enum {
    VK_COMPUTE_STATUS_IDLE = 0,
    VK_COMPUTE_STATUS_PENDING,
    VK_COMPUTE_STATUS_SUBMITTED,
    VK_COMPUTE_STATUS_COMPLETED,
    VK_COMPUTE_STATUS_FAILED
} vk_compute_status_t;

// Async compute job structure
typedef struct {
    uint32_t id;
    const char *name;
    vk_compute_priority_t priority;
    vk_compute_status_t status;
    
    VkCommandBuffer command_buffer;
    VkFence fence;
    
    // Dependencies
    uint32_t dependencies[MAX_DEPENDENCIES];
    uint32_t dependency_count;
    
    // Synchronization
    uint64_t timeline_wait_value;
    uint64_t timeline_signal_value;
    qboolean wait_for_graphics;
    
    // Timing
    uint64_t submission_time_ns;
    uint64_t completion_time_ns;
    float execution_duration_ms;
    
    qboolean active;
} vk_compute_job_t;

// Async compute manager structure
typedef struct {
    qboolean enabled;
    qboolean initialized;
    
    // Jobs
    vk_compute_job_t jobs[MAX_COMPUTE_JOBS];
    atomic_uint_t active_job_count;
    atomic_uint_t next_job_id;
    
    // Queue configuration
    VkQueue queue;
    uint32_t queue_family_index;
    VkCommandPool command_pool;
    
    // Synchronization
    VkSemaphore timeline_semaphore;
    atomic_uint64_t current_timeline_value;
    
    // Statistics
    atomic_uint64_t total_jobs_submitted;
    atomic_uint64_t total_jobs_completed;
    float average_execution_time_ms;
    float peak_execution_time_ms;
    
    const char *debug_name;
} vk_compute_manager_t;

// Async compute function declarations
qboolean vk_init_compute_manager(void);
void vk_shutdown_compute_manager(void);
uint32_t vk_submit_compute_job(const char *name, VkCommandBuffer cmd_buffer, vk_compute_priority_t priority, qboolean wait_for_graphics, uint32_t *dependencies, uint32_t dependency_count);
qboolean vk_is_compute_job_complete(uint32_t job_id);
void vk_wait_for_compute_job(uint32_t job_id);
void vk_wait_all_compute_jobs(void);
void vk_update_compute_manager(void);
void vk_print_compute_stats(void);
void vk_print_compute_task_graph(void);
void vk_set_compute_enabled(qboolean enabled);

#ifdef __cplusplus
}
#endif

#endif // __VK_COMPUTE_H__
