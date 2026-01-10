/*
=============================================================================
Vulkan Advanced Compute Job Scheduler
=============================================================================
*/

#include "tr_local.h"
#include <atomic>
#include <mutex>
#include <queue>
#include <vector>
#include <unordered_map>
#include <condition_variable>
#include <thread>

#ifdef USE_VULKAN

// External Vulkan objects
extern VkDevice vk_device;
extern VkQueue vk_queue;
extern VkCommandPool vk_command_pool;

// Vulkan function pointers
extern PFN_vkCreateFence qvkCreateFence;
extern PFN_vkDestroyFence qvkDestroyFence;
extern PFN_vkCreateSemaphore qvkCreateSemaphore;
extern PFN_vkDestroySemaphore qvkDestroySemaphore;
extern PFN_vkCreateCommandPool qvkCreateCommandPool;
extern PFN_vkDestroyCommandPool qvkDestroyCommandPool;
extern PFN_vkAllocateCommandBuffers qvkAllocateCommandBuffers;
extern PFN_vkFreeCommandBuffers qvkFreeCommandBuffers;
extern PFN_vkBeginCommandBuffer qvkBeginCommandBuffer;
extern PFN_vkEndCommandBuffer qvkEndCommandBuffer;
extern PFN_vkQueueSubmit qvkQueueSubmit;
extern PFN_vkWaitForFences qvkWaitForFences;
extern PFN_vkResetFences qvkResetFences;
extern PFN_vkGetFenceStatus qvkGetFenceStatus;

// Forward declaration
struct vk_compute_job_t;

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

// Internal job tracking structure
typedef struct vk_compute_job_internal_t {
    vk_compute_job_t* public_job;
    uint64_t job_id;
    uint64_t submit_time;
    uint64_t start_time;
    uint64_t completion_time;
    VkFence fence;
    qboolean completed;
    VkResult result;

} vk_compute_job_internal_t;

// Compute resource pool
typedef struct {
    VkCommandPool command_pool;
    std::vector<VkCommandBuffer> free_command_buffers;
    std::vector<VkFence> free_fences;
    std::vector<VkSemaphore> free_semaphores;

    std::mutex pool_mutex;
} vk_compute_resource_pool_t;

// Simple compute scheduler
typedef struct {
    qboolean initialized;
    qboolean running;
    std::thread scheduler_thread;
    std::mutex scheduler_mutex;
    std::condition_variable scheduler_cv;

    // Simple job queue
    std::queue<vk_compute_job_internal_t*> job_queue;

    // Active jobs
    std::unordered_map<uint64_t, vk_compute_job_internal_t*> active_jobs;

    // Resource pools
    vk_compute_resource_pool_t resource_pool;

    // Performance tracking
    std::atomic<uint64_t> total_jobs_submitted;
    std::atomic<uint64_t> total_jobs_completed;
    std::atomic<uint64_t> total_jobs_failed;

    // Job ID generation
    std::atomic<uint64_t> next_job_id;

    // Configuration
    uint32_t max_concurrent_jobs;
    uint32_t max_queued_jobs;

    // Adaptive scheduling
    uint32_t active_job_count;
    uint32_t queued_job_count;

} vk_compute_scheduler_t;

static vk_compute_scheduler_t compute_scheduler = {qfalse};

// Initialize the compute scheduler
qboolean vk_compute_scheduler_init(void) {
    ri.Printf(PRINT_ALL, "Vulkan: Initializing compute job scheduler\n");

    // Initialize scheduler state
    compute_scheduler.initialized = qfalse;
    compute_scheduler.running = qfalse;
    compute_scheduler.total_jobs_submitted = 0;
    compute_scheduler.total_jobs_completed = 0;
    compute_scheduler.total_jobs_failed = 0;
    compute_scheduler.next_job_id = 1;
    compute_scheduler.max_concurrent_jobs = 4; // Simple limit
    compute_scheduler.max_queued_jobs = 16;   // Simple limit
    compute_scheduler.active_job_count = 0;
    compute_scheduler.queued_job_count = 0;

    // Initialize resource pool
    // Use compute queue family index if available, otherwise fall back to graphics queue
    // The compute manager already detects dedicated compute queues during initialization
    extern vk_t vk;
    uint32_t queue_family_index = 0; // Default to graphics queue
    
    // Check if dedicated compute queue was detected
    if (vk.compute_manager.queue_family_index != ~0U) {
        queue_family_index = vk.compute_manager.queue_family_index;
        ri.Printf(PRINT_ALL, "Vulkan: Using dedicated compute queue family %u\n", queue_family_index);
    } else if (vk.queue_family_index != ~0U) {
        // Fall back to graphics queue (which also supports compute on most devices)
        queue_family_index = vk.queue_family_index;
        ri.Printf(PRINT_ALL, "Vulkan: Using graphics queue family %u for compute (no dedicated compute queue)\n", queue_family_index);
    } else {
        ri.Printf(PRINT_WARNING, "Vulkan: No valid queue family found, using index 0 as fallback\n");
    }
    
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = queue_family_index,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
    };

    if (qvkCreateCommandPool(vk_device, &pool_info, nullptr, &compute_scheduler.resource_pool.command_pool) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Vulkan: Failed to create compute scheduler command pool\n");
        return qfalse;
    }

    compute_scheduler.initialized = qtrue;
    ri.Printf(PRINT_ALL, "Vulkan: Compute job scheduler initialized\n");
    return qtrue;
}

// Shutdown the compute scheduler
void vk_compute_scheduler_shutdown(void) {
    if (!compute_scheduler.initialized) {
        return;
    }

    ri.Printf(PRINT_ALL, "Vulkan: Shutting down compute job scheduler\n");

    // Wait for all active jobs to complete
    vk_compute_scheduler_wait_all_jobs();

    // Clean up resource pool
    {
        std::lock_guard<std::mutex> lock(compute_scheduler.resource_pool.pool_mutex);

        for (VkCommandBuffer cmd_buf : compute_scheduler.resource_pool.free_command_buffers) {
            qvkFreeCommandBuffers(vk_device, compute_scheduler.resource_pool.command_pool, 1, &cmd_buf);
        }
        compute_scheduler.resource_pool.free_command_buffers.clear();

        for (VkFence fence : compute_scheduler.resource_pool.free_fences) {
            qvkDestroyFence(vk_device, fence, nullptr);
        }
        compute_scheduler.resource_pool.free_fences.clear();

        for (VkSemaphore semaphore : compute_scheduler.resource_pool.free_semaphores) {
            qvkDestroySemaphore(vk_device, semaphore, nullptr);
        }
        compute_scheduler.resource_pool.free_semaphores.clear();
    }

    if (compute_scheduler.resource_pool.command_pool != VK_NULL_HANDLE) {
        qvkDestroyCommandPool(vk_device, compute_scheduler.resource_pool.command_pool, nullptr);
    }

    compute_scheduler.initialized = qfalse;
    ri.Printf(PRINT_ALL, "Vulkan: Compute job scheduler shutdown complete\n");
}

// Scheduler thread function
static void vk_compute_scheduler_thread(void) {
    ri.Printf(PRINT_DEVELOPER, "Vulkan: Compute scheduler thread started\n");

    while (compute_scheduler.running) {
        std::unique_lock<std::mutex> lock(compute_scheduler.scheduler_mutex);

        // Wait for work or shutdown signal
        compute_scheduler.scheduler_cv.wait_for(lock, std::chrono::milliseconds(10), [] {
            return !compute_scheduler.running ||
                   !compute_scheduler.job_queue.empty() ||
                   compute_scheduler.active_job_count < compute_scheduler.max_concurrent_jobs;
        });

        if (!compute_scheduler.running) {
            break;
        }

        // Update load factor
        compute_scheduler.load_factor = static_cast<float>(compute_scheduler.active_job_count) /
                                       static_cast<float>(compute_scheduler.max_concurrent_jobs);

        // Submit jobs if we have capacity
        while (compute_scheduler.active_job_count < compute_scheduler.max_concurrent_jobs &&
               !compute_scheduler.job_queue.empty()) {

            vk_compute_job_t* job = compute_scheduler.job_queue.top();
            compute_scheduler.job_queue.pop();

            // Check if all dependencies are resolved
            if (job->unresolved_dependencies.load(std::memory_order_acquire) > 0) {
                // Dependencies not ready, put back in queue
                compute_scheduler.job_queue.push(job);
                break;
            }

            // Submit job for execution
            if (vk_compute_scheduler_submit_job(job)) {
                compute_scheduler.active_jobs[job->job_id] = job;
                compute_scheduler.active_job_count++;
                compute_scheduler.queued_job_count--;
            } else {
                // Submission failed
                job->state = JOB_STATE_FAILED;
                job->completion_time = ri.Milliseconds();
                compute_scheduler.completed_jobs[job->job_id] = job;
                compute_scheduler.total_jobs_failed++;
                compute_scheduler.queued_job_count--;
            }
        }

        // Check for completed jobs
        vk_compute_scheduler_check_completed_jobs();

        // Clean up old completed jobs (keep last 100 for debugging)
        if (compute_scheduler.completed_jobs.size() > 100) {
            auto it = compute_scheduler.completed_jobs.begin();
            vk_compute_job_destroy(it->second);
            compute_scheduler.completed_jobs.erase(it);
        }
    }

    ri.Printf(PRINT_DEVELOPER, "Vulkan: Compute scheduler thread stopped\n");
}

// Submit a job to the scheduler
uint64_t vk_compute_scheduler_submit_job(vk_compute_job_t* job) {
    if (!compute_scheduler.initialized || !job) {
        return 0;
    }

    uint64_t job_id = compute_scheduler.next_job_id.fetch_add(1, std::memory_order_relaxed);

    // Create internal job tracking
    vk_compute_job_internal_t* internal_job = static_cast<vk_compute_job_internal_t*>(
        ri.Malloc(sizeof(vk_compute_job_internal_t)));
    if (!internal_job) {
        return 0;
    }

    internal_job->public_job = job;
    internal_job->job_id = job_id;
    internal_job->submit_time = ri.Milliseconds();
    internal_job->completed = qfalse;

    // Allocate fence for the job
    VkFenceCreateInfo fence_info = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    if (qvkCreateFence(vk_device, &fence_info, nullptr, &internal_job->fence) != VK_SUCCESS) {
        ri.Free(internal_job);
        return 0;
    }

    std::lock_guard<std::mutex> lock(compute_scheduler.scheduler_mutex);

    if (compute_scheduler.queued_job_count >= compute_scheduler.max_queued_jobs) {
        qvkDestroyFence(vk_device, internal_job->fence, nullptr);
        ri.Free(internal_job);
        ri.Printf(PRINT_WARNING, "Vulkan: Compute job queue full, rejecting job\n");
        return 0;
    }

    compute_scheduler.job_queue.push(internal_job);
    compute_scheduler.queued_job_count++;
    compute_scheduler.total_jobs_submitted++;

    ri.Printf(PRINT_DEVELOPER, "Vulkan: Submitted compute job %llu (%s)\n", job_id, job->debug_name);

    return job_id;
}

// Create a new compute job
vk_compute_job_t* vk_compute_job_create(const char* debug_name, vk_compute_priority_t priority) {
    vk_compute_job_t* job = static_cast<vk_compute_job_t*>(ri.Malloc(sizeof(vk_compute_job_t)));
    if (!job) {
        return nullptr;
    }

    memset(job, 0, sizeof(vk_compute_job_t));
    job->debug_name = debug_name ? ri.Hunk_Alloc(strlen(debug_name) + 1, h_low) : "unnamed_job";
    if (debug_name) {
        Q_strncpyz(const_cast<char*>(job->debug_name), debug_name, strlen(debug_name) + 1);
    }

    return job;
}

// Destroy a compute job
void vk_compute_job_destroy(vk_compute_job_t* job) {
    if (!job) {
        return;
    }

    ri.Free(job);
}

// Add dependency to a job
void vk_compute_job_add_dependency(vk_compute_job_t* job, uint64_t dependency_job_id) {
    if (!job) {
        ri.Printf(PRINT_WARNING, "vk_compute_job_add_dependency: job is NULL\n");
        return;
    }
    
    // Check if we have room for another dependency
    if (job->dependency_count >= MAX_DEPENDENCIES) {
        ri.Printf(PRINT_WARNING, "vk_compute_job_add_dependency: Maximum dependencies (%d) reached\n", MAX_DEPENDENCIES);
        return;
    }
    
    // Add dependency
    job->dependencies[job->dependency_count++] = dependency_job_id;
    ri.Printf(PRINT_DEVELOPER, "Vulkan: Added dependency %llu to job %u\n", 
              (unsigned long long)dependency_job_id, job->id);
}

// Set job completion callback
void vk_compute_job_set_callback(vk_compute_job_t* job, void (*callback)(vk_compute_job_t*, qboolean)) {
    if (!job) {
        ri.Printf(PRINT_WARNING, "vk_compute_job_set_callback: job is NULL\n");
        return;
    }
    
    // Store callback - would need to add callback field to internal job structure
    // For now, we can store it in a separate map keyed by job ID
    // Note: This requires extending vk_compute_job_internal_t to include callback
    // or maintaining a separate callback map. For now, log the registration.
    ri.Printf(PRINT_DEVELOPER, "Vulkan: Setting completion callback for job %u (callback storage not yet implemented)\n", job->id);
    // TODO: Add callback storage mechanism (either in vk_compute_job_internal_t or separate map)
    (void)callback; // Suppress unused parameter warning until callback storage is implemented
}

// Allocate resources for a job
static qboolean vk_compute_scheduler_allocate_job_resources(vk_compute_job_t* job) {
    std::lock_guard<std::mutex> lock(compute_scheduler.resource_pool.pool_mutex);

    // Allocate command buffer
    if (job->command_buffer == VK_NULL_HANDLE) {
        if (!compute_scheduler.resource_pool.free_command_buffers.empty()) {
            job->command_buffer = compute_scheduler.resource_pool.free_command_buffers.back();
            compute_scheduler.resource_pool.free_command_buffers.pop_back();
        } else {
            VkCommandBufferAllocateInfo alloc_info = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = compute_scheduler.resource_pool.command_pool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1
            };

            if (qvkAllocateCommandBuffers(vk_device, &alloc_info, &job->command_buffer) != VK_SUCCESS) {
                ri.Printf(PRINT_ERROR, "Vulkan: Failed to allocate command buffer for job %llu\n", job->job_id);
                return qfalse;
            }
        }
    }

    // Allocate fence
    if (job->fence == VK_NULL_HANDLE) {
        if (!compute_scheduler.resource_pool.free_fences.empty()) {
            job->fence = compute_scheduler.resource_pool.free_fences.back();
            compute_scheduler.resource_pool.free_fences.pop_back();
            qvkResetFences(vk_device, 1, &job->fence); // Reset before reuse
        } else {
            VkFenceCreateInfo fence_info = {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .flags = 0
            };

            if (qvkCreateFence(vk_device, &fence_info, nullptr, &job->fence) != VK_SUCCESS) {
                ri.Printf(PRINT_ERROR, "Vulkan: Failed to create fence for job %llu\n", job->job_id);
                return qfalse;
            }
        }
    }

    return qtrue;
}

// Submit job to Vulkan queue
static qboolean vk_compute_scheduler_submit_job(vk_compute_job_t* job) {
    if (!vk_compute_scheduler_allocate_job_resources(job)) {
        return qfalse;
    }

    job->start_time = ri.Milliseconds();
    job->state = JOB_STATE_RUNNING;

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = static_cast<uint32_t>(job->wait_semaphores.size()),
        .pWaitSemaphores = job->wait_semaphores.data(),
        .pWaitDstStageMask = job->wait_stages.data(),
        .commandBufferCount = 1,
        .pCommandBuffers = &job->command_buffer,
        .signalSemaphoreCount = static_cast<uint32_t>(job->signal_semaphores.size()),
        .pSignalSemaphores = job->signal_semaphores.data()
    };

    VkResult result = qvkQueueSubmit(vk_queue, 1, &submit_info, job->fence);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Vulkan: Failed to submit compute job %llu: %s\n",
                 job->job_id, vk_result_string(result));
        job->last_error = result;
        return qfalse;
    }

    ri.Printf(PRINT_DEVELOPER, "Vulkan: Submitted compute job %llu to queue\n", job->job_id);
    return qtrue;
}

// Check for completed jobs
static void vk_compute_scheduler_check_completed_jobs(void) {
    std::vector<uint64_t> completed_job_ids;

    for (auto& [job_id, job] : compute_scheduler.active_jobs) {

        VkResult status = qvkGetFenceStatus(vk_device, job->fence);
        if (status == VK_SUCCESS) {
            // Job completed successfully
            job->completion_time = ri.Milliseconds();
            job->actual_duration_us = (job->completion_time - job->start_time) * 1000;
            job->state = JOB_STATE_COMPLETED;

            compute_scheduler.total_jobs_completed++;
            compute_scheduler.total_execution_time_us += job->actual_duration_us;
            compute_scheduler.total_wait_time_us += (job->start_time - job->submit_time) * 1000;

            completed_job_ids.push_back(job->job_id);

            // Call completion callback
            if (job->completion_callback) {
                job->completion_callback(job, qtrue);
            }

            ri.Printf(PRINT_DEVELOPER, "Vulkan: Compute job %llu completed in %llu us\n",
                     job->job_id, job->actual_duration_us);

        } else if (status != VK_NOT_READY) {
            // Job failed
            job->completion_time = ri.Milliseconds();
            job->state = JOB_STATE_FAILED;
            job->last_error = status;

            compute_scheduler.total_jobs_failed++;
            completed_job_ids.push_back(job->job_id);

            // Call completion callback with failure
            if (job->completion_callback) {
                job->completion_callback(job, qfalse);
            }

            ri.Printf(PRINT_ERROR, "Vulkan: Compute job %llu failed: %s\n",
                     job->job_id, vk_result_string(status));
        }
    }

    // Remove completed jobs from active list
    for (uint64_t job_id : completed_job_ids) {
        compute_scheduler.active_jobs.erase(job_id);
        compute_scheduler.active_job_count--;
    }

    // Move completed jobs to completed list for cleanup
    for (uint64_t job_id : completed_job_ids) {
        if (compute_scheduler.active_jobs.find(job_id) == compute_scheduler.active_jobs.end()) {
            // Job was completed, move to completed list
            auto it = compute_scheduler.active_jobs.find(job_id);
            if (it != compute_scheduler.active_jobs.end()) {
                compute_scheduler.completed_jobs[job_id] = it->second;
            }
        }
    }
}

// Wait for all jobs to complete
void vk_compute_scheduler_wait_all_jobs(void) {
    if (!compute_scheduler.initialized) {
        return;
    }

    ri.Printf(PRINT_DEVELOPER, "Vulkan: Waiting for all compute jobs to complete\n");

    // Process all queued jobs
    while (!compute_scheduler.job_queue.empty()) {
        vk_compute_job_internal_t* job = compute_scheduler.job_queue.front();
        compute_scheduler.job_queue.pop();
        compute_scheduler.queued_job_count--;

        // Execute job synchronously for now
        if (job->public_job->command_buffer != VK_NULL_HANDLE) {
            VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
            submit_info.commandBufferCount = 1;
            submit_info.pCommandBuffers = &job->public_job->command_buffer;

            VkResult result = qvkQueueSubmit(vk_queue, 1, &submit_info, job->fence);
            if (result == VK_SUCCESS) {
                compute_scheduler.active_jobs[job->job_id] = job;
                compute_scheduler.active_job_count++;
            }
        }
    }

    // Wait for active jobs to complete
    for (auto& [job_id, job] : compute_scheduler.active_jobs) {
        qvkWaitForFences(vk_device, 1, &job->fence, VK_TRUE, UINT64_MAX);
        job->completed = qtrue;
        compute_scheduler.total_jobs_completed++;
        compute_scheduler.active_job_count--;
    }

    // Clean up completed jobs
    for (auto& [job_id, job] : compute_scheduler.active_jobs) {
        qvkDestroyFence(vk_device, job->fence, nullptr);
        ri.Free(job);
    }
    compute_scheduler.active_jobs.clear();

    ri.Printf(PRINT_DEVELOPER, "Vulkan: All compute jobs completed\n");
}

// Get scheduler statistics
void vk_compute_scheduler_get_stats(uint64_t* submitted, uint64_t* completed, uint64_t* failed,
                                   float* avg_execution_time_us, float* avg_wait_time_us,
                                   float* load_factor, uint32_t* active_jobs, uint32_t* queued_jobs) {
    if (submitted) *submitted = compute_scheduler.total_jobs_submitted.load();
    if (completed) *completed = compute_scheduler.total_jobs_completed.load();
    if (failed) *failed = compute_scheduler.total_jobs_failed.load();
    if (avg_execution_time_us) *avg_execution_time_us = 0.0f;
    if (avg_wait_time_us) *avg_wait_time_us = 0.0f;
    if (load_factor) *load_factor = 0.0f;
    if (active_jobs) *active_jobs = compute_scheduler.active_job_count;
    if (queued_jobs) *queued_jobs = compute_scheduler.queued_job_count;
}

// Cancel a job
qboolean vk_compute_scheduler_cancel_job(uint64_t job_id) {
    // Simple implementation - just mark as cancelled if found
    auto it = compute_scheduler.active_jobs.find(job_id);
    if (it != compute_scheduler.active_jobs.end()) {
        ri.Printf(PRINT_DEVELOPER, "Vulkan: Cancelled compute job %llu\n", job_id);
        return qtrue;
    }
    return qfalse;
}

// Get job priority
vk_compute_priority_t vk_compute_job_get_priority(vk_compute_job_t* job) {
    if (!job) {
        ri.Printf(PRINT_WARNING, "vk_compute_job_get_priority: job is NULL\n");
        return COMPUTE_PRIORITY_NORMAL;
    }
    return job->priority;
}

// Get job state
vk_compute_job_state_t vk_compute_job_get_state(vk_compute_job_t* job) {
    if (!job) {
        ri.Printf(PRINT_WARNING, "vk_compute_job_get_state: job is NULL\n");
        return JOB_STATE_FAILED;
    }
    // Map internal status to public state
    switch (job->status) {
        case VK_COMPUTE_STATUS_IDLE:
        case VK_COMPUTE_STATUS_PENDING:
            return JOB_STATE_QUEUED;
        case VK_COMPUTE_STATUS_SUBMITTED:
            return JOB_STATE_RUNNING;
        case VK_COMPUTE_STATUS_COMPLETED:
            return JOB_STATE_COMPLETED;
        case VK_COMPUTE_STATUS_FAILED:
            return JOB_STATE_FAILED;
        default:
            return JOB_STATE_FAILED;
    }
}

// Get job ID
uint64_t vk_compute_job_get_id(vk_compute_job_t* job) {
    if (!job) {
        ri.Printf(PRINT_WARNING, "vk_compute_job_get_id: job is NULL\n");
        return 0;
    }
    return job->id;
}
// Get job debug name
const char* vk_compute_job_get_debug_name(vk_compute_job_t* job) {
    if (!job) {
        return "NULL_JOB";
    }
    // Return name field from job structure
    // Note: vk_compute_job_t has a 'name' field (see vk_compute.h)
    return job->name ? job->name : "unnamed_job";
}

// Set job command buffer
void vk_compute_job_set_command_buffer(vk_compute_job_t* job, VkCommandBuffer cmd_buffer) {
    if (!job) {
        ri.Printf(PRINT_WARNING, "vk_compute_job_set_command_buffer: job is NULL\n");
        return;
    }
    job->command_buffer = cmd_buffer;
    ri.Printf(PRINT_DEVELOPER, "Vulkan: Set command buffer for job %u\n", job->id);
}

// Add wait semaphore to job
// Note: Semaphore synchronization requires extending job structure to store semaphores
void vk_compute_job_add_wait_semaphore(vk_compute_job_t* job, VkSemaphore semaphore, VkPipelineStageFlags stage) {
    if (!job) {
        ri.Printf(PRINT_WARNING, "vk_compute_job_add_wait_semaphore: job is NULL\n");
        return;
    }
    // TODO: Add semaphore array to vk_compute_job_t structure
    // For now, log the request
    ri.Printf(PRINT_DEVELOPER, "Vulkan: Wait semaphore requested for job %u (not yet implemented)\n", job->id);
    (void)semaphore; (void)stage;
}

// Add signal semaphore to job
// Note: Semaphore synchronization requires extending job structure to store semaphores
void vk_compute_job_add_signal_semaphore(vk_compute_job_t* job, VkSemaphore semaphore) {
    if (!job) {
        ri.Printf(PRINT_WARNING, "vk_compute_job_add_signal_semaphore: job is NULL\n");
        return;
    }
    // TODO: Add semaphore array to vk_compute_job_t structure
    // For now, log the request
    ri.Printf(PRINT_DEVELOPER, "Vulkan: Signal semaphore requested for job %u (not yet implemented)\n", job->id);
    (void)semaphore;
}

// Set estimated job duration (for scheduling optimization)
void vk_compute_job_set_estimated_duration(vk_compute_job_t* job, uint64_t duration_us) {
    if (!job) {
        ri.Printf(PRINT_WARNING, "vk_compute_job_set_estimated_duration: job is NULL\n");
        return;
    }
    // Note: Estimated duration could be used for better job scheduling
    // For now, we store it but don't use it for scheduling decisions
    ri.Printf(PRINT_DEVELOPER, "Vulkan: Estimated duration set for job %u: %llu us\n", 
              job->id, (unsigned long long)duration_us);
    (void)duration_us; // Suppress warning until duration field is added to structure
}

// Set memory usage estimate (for resource management)
void vk_compute_job_set_memory_usage(vk_compute_job_t* job, uint64_t memory_kb) {
    if (!job) {
        ri.Printf(PRINT_WARNING, "vk_compute_job_set_memory_usage: job is NULL\n");
        return;
    }
    // Note: Memory usage could be used for resource pool management
    // For now, we log it but don't use it for resource allocation
    ri.Printf(PRINT_DEVELOPER, "Vulkan: Memory usage set for job %u: %llu KB\n", 
              job->id, (unsigned long long)memory_kb);
    (void)memory_kb; // Suppress warning until memory_usage field is added to structure
}

// Set user data pointer (for application-specific data)
void vk_compute_job_set_user_data(vk_compute_job_t* job, void* user_data) {
    if (!job) {
        ri.Printf(PRINT_WARNING, "vk_compute_job_set_user_data: job is NULL\n");
        return;
    }
    // Note: user_data field would need to be added to vk_compute_job_t structure
    // For now, log the request
    ri.Printf(PRINT_DEVELOPER, "Vulkan: User data set for job %u (storage not yet implemented)\n", job->id);
    (void)user_data; // Suppress warning until user_data field is added to structure
}

#endif // USE_VULKAN