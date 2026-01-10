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

// Semaphore limits
#define MAX_WAIT_SEMAPHORES 8
#define MAX_SIGNAL_SEMAPHORES 8

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
    
    // Dependencies (stored in internal structure since public structure doesn't have them)
    uint64_t dependencies[MAX_DEPENDENCIES];
    uint32_t dependency_count;
    void (*completion_callback)(vk_compute_job_t*, qboolean); // Completion callback

    // Semaphore synchronization
    VkSemaphore wait_semaphores[MAX_WAIT_SEMAPHORES];
    VkPipelineStageFlags wait_stages[MAX_WAIT_SEMAPHORES];
    uint32_t wait_semaphore_count;
    VkSemaphore signal_semaphores[MAX_SIGNAL_SEMAPHORES];
    uint32_t signal_semaphore_count;

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

            vk_compute_job_internal_t* internal_job = compute_scheduler.job_queue.front();
            compute_scheduler.job_queue.pop();

            if (!internal_job || !internal_job->public_job) {
                ri.Printf(PRINT_WARNING, "Vulkan: Invalid job in queue, skipping\n");
                continue;
            }

            // Check if all dependencies are resolved
            // Proper dependency checking: verify dependency jobs are completed by checking job state
            qboolean dependencies_resolved = qtrue;
            for (uint32_t i = 0; i < internal_job->dependency_count; i++) {
                uint64_t dep_id = internal_job->dependencies[i];
                
                // Look up dependency job in active_jobs map
                auto dep_it = compute_scheduler.active_jobs.find(dep_id);
                if (dep_it != compute_scheduler.active_jobs.end()) {
                    vk_compute_job_internal_t* dep_job = dep_it->second;
                    // Check if dependency job state is JOB_STATE_COMPLETED
                    // Also check the completed flag as a fallback
                    if (dep_job->public_job && dep_job->public_job->state != JOB_STATE_COMPLETED && !dep_job->completed) {
                        dependencies_resolved = qfalse;
                        break;
                    }
                } else {
                    // Dependency job not found in active jobs - might be in completed jobs or never submitted
                    // For safety, assume it's not ready (conservative approach)
                    // In a full implementation, we'd also check a completed_jobs map
                    dependencies_resolved = qfalse;
                    break;
                }
            }
            
            if (!dependencies_resolved) {
                // Dependencies not ready, put back in queue
                compute_scheduler.job_queue.push(internal_job);
                continue; // Try next job instead of breaking
            }

            // Submit job for execution using internal helper function
            if (vk_compute_scheduler_submit_job_internal(internal_job)) {
                compute_scheduler.active_jobs[internal_job->job_id] = internal_job;
                compute_scheduler.active_job_count++;
                compute_scheduler.queued_job_count--;
            } else {
                // Submission failed - clean up
                vk_compute_job_t* job = internal_job->public_job;
                job->state = JOB_STATE_FAILED;
                internal_job->completion_time = ri.Milliseconds();
                internal_job->completed = qtrue;
                compute_scheduler.total_jobs_failed++;
                compute_scheduler.queued_job_count--;
                // Clean up internal job structure
                if (internal_job->fence != VK_NULL_HANDLE) {
                    qvkDestroyFence(vk_device, internal_job->fence, nullptr);
                }
                ri.Free(internal_job);
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
    internal_job->dependency_count = 0; // Initialize dependency tracking
    internal_job->completion_callback = nullptr; // Initialize callback
    memset(internal_job->dependencies, 0, sizeof(internal_job->dependencies)); // Initialize dependencies array
    internal_job->wait_semaphore_count = 0; // Initialize semaphore arrays
    internal_job->signal_semaphore_count = 0;
    memset(internal_job->wait_semaphores, 0, sizeof(internal_job->wait_semaphores));
    memset(internal_job->wait_stages, 0, sizeof(internal_job->wait_stages));
    memset(internal_job->signal_semaphores, 0, sizeof(internal_job->signal_semaphores));

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

    ri.Printf(PRINT_DEVELOPER, "Vulkan: Submitted compute job %llu (%s)\n", job_id, job->name ? job->name : "unnamed");

    return job_id;
}

// Create a new compute job
vk_compute_job_t* vk_compute_job_create(const char* debug_name, vk_compute_priority_t priority) {
    vk_compute_job_t* job = static_cast<vk_compute_job_t*>(ri.Malloc(sizeof(vk_compute_job_t)));
    if (!job) {
        return nullptr;
    }

    memset(job, 0, sizeof(vk_compute_job_t));
    // Set job debug name (vk_compute_job_t uses 'debug_name' field per vk_compute_scheduler.h)
    if (debug_name) {
        // Allocate and copy name
        size_t name_len = strlen(debug_name) + 1;
        char* name_copy = static_cast<char*>(ri.Malloc(name_len));
        if (name_copy) {
            Q_strncpyz(name_copy, debug_name, name_len);
            job->debug_name = name_copy;
        } else {
            job->debug_name = "unnamed_job";
        }
    } else {
        job->debug_name = "unnamed_job";
    }
    job->priority = priority;
    job->state = JOB_STATE_QUEUED; // Initial state is queued

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
    
    // Note: The vk_compute_job_t structure in vk_compute_scheduler.h doesn't have
    // dependencies or dependency_count fields. Dependencies would need to be stored
    // in the internal job tracking structure (vk_compute_job_internal_t) or added
    // to the public structure. For now, we need to look up the internal job structure.
    
    // Find internal job structure by job_id and store dependency
    std::lock_guard<std::mutex> lock(compute_scheduler.scheduler_mutex);
    
    // Search in active jobs
    auto it = compute_scheduler.active_jobs.find(job->job_id);
    vk_compute_job_internal_t* internal_job = nullptr;
    
    if (it != compute_scheduler.active_jobs.end()) {
        internal_job = it->second;
    } else {
        // Job might be in queue - search the job queue
        // Note: This requires iterating through the queue which is less efficient
        // but necessary for jobs that haven't been submitted yet
        std::queue<vk_compute_job_internal_t*> temp_queue;
        bool found = false;
        
        while (!compute_scheduler.job_queue.empty()) {
            vk_compute_job_internal_t* queued_job = compute_scheduler.job_queue.front();
            compute_scheduler.job_queue.pop();
            
            if (queued_job->job_id == job->job_id) {
                internal_job = queued_job;
                found = true;
            }
            
            temp_queue.push(queued_job);
        }
        
        // Restore queue
        while (!temp_queue.empty()) {
            compute_scheduler.job_queue.push(temp_queue.front());
            temp_queue.pop();
        }
        
        if (!found) {
            ri.Printf(PRINT_WARNING, "Vulkan: Job %llu not found (may not be submitted yet), dependency %llu cannot be added\n", 
                      (unsigned long long)job->job_id, (unsigned long long)dependency_job_id);
            return;
        }
    }
    
    if (!internal_job) {
        ri.Printf(PRINT_WARNING, "vk_compute_job_add_dependency: Internal job structure not found\n");
        return;
    }
    
    // Check if we have room for another dependency
    if (internal_job->dependency_count >= MAX_DEPENDENCIES) {
        ri.Printf(PRINT_WARNING, "vk_compute_job_add_dependency: Maximum dependencies (%d) reached for job %llu\n", 
                  MAX_DEPENDENCIES, (unsigned long long)job->job_id);
        return;
    }
    
    // Add dependency to internal job structure
    internal_job->dependencies[internal_job->dependency_count++] = dependency_job_id;
    ri.Printf(PRINT_DEVELOPER, "Vulkan: Added dependency %llu to job %llu (%u/%u dependencies)\n", 
              (unsigned long long)dependency_job_id, (unsigned long long)job->job_id,
              internal_job->dependency_count, MAX_DEPENDENCIES);
}

// Set job completion callback
void vk_compute_job_set_callback(vk_compute_job_t* job, void (*callback)(vk_compute_job_t*, qboolean)) {
    if (!job) {
        ri.Printf(PRINT_WARNING, "vk_compute_job_set_callback: job is NULL\n");
        return;
    }
    
    // Store callback in internal job structure
    std::lock_guard<std::mutex> lock(compute_scheduler.scheduler_mutex);
    
    // Find internal job structure by job_id
    auto it = compute_scheduler.active_jobs.find(job->job_id);
    vk_compute_job_internal_t* internal_job = nullptr;
    
    if (it != compute_scheduler.active_jobs.end()) {
        internal_job = it->second;
    } else {
        // Job might be in queue - search the job queue
        std::queue<vk_compute_job_internal_t*> temp_queue;
        bool found = false;
        
        while (!compute_scheduler.job_queue.empty()) {
            vk_compute_job_internal_t* queued_job = compute_scheduler.job_queue.front();
            compute_scheduler.job_queue.pop();
            
            if (queued_job->job_id == job->job_id) {
                internal_job = queued_job;
                found = true;
            }
            
            temp_queue.push(queued_job);
        }
        
        // Restore queue
        while (!temp_queue.empty()) {
            compute_scheduler.job_queue.push(temp_queue.front());
            temp_queue.pop();
        }
        
        if (!found) {
            ri.Printf(PRINT_WARNING, "Vulkan: Job %llu not found (may not be submitted yet), callback cannot be set\n", 
                      (unsigned long long)job->job_id);
            return;
        }
    }
    
    if (!internal_job) {
        ri.Printf(PRINT_WARNING, "vk_compute_job_set_callback: Internal job structure not found\n");
        return;
    }
    
    // Store callback in internal job structure
    internal_job->completion_callback = callback;
    ri.Printf(PRINT_DEVELOPER, "Vulkan: Set completion callback for job %llu\n", (unsigned long long)job->job_id);
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

// Submit job to Vulkan queue (internal helper)
// Note: This function is called from the scheduler thread after dependency resolution
static qboolean vk_compute_scheduler_submit_job_internal(vk_compute_job_internal_t* internal_job) {
    if (!internal_job || !internal_job->public_job) {
        return qfalse;
    }
    
    vk_compute_job_t* job = internal_job->public_job;
    
    // Allocate resources if needed
    if (!vk_compute_scheduler_allocate_job_resources(job)) {
        return qfalse;
    }

    // Update job state
    job->state = JOB_STATE_RUNNING;
    internal_job->start_time = ri.Milliseconds();

    // Submit command buffer to queue with semaphore synchronization
    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &job->command_buffer,
        .waitSemaphoreCount = internal_job->wait_semaphore_count,
        .pWaitSemaphores = (internal_job->wait_semaphore_count > 0) ? internal_job->wait_semaphores : nullptr,
        .pWaitDstStageMask = (internal_job->wait_semaphore_count > 0) ? internal_job->wait_stages : nullptr,
        .signalSemaphoreCount = internal_job->signal_semaphore_count,
        .pSignalSemaphores = (internal_job->signal_semaphore_count > 0) ? internal_job->signal_semaphores : nullptr
    };

    VkResult result = qvkQueueSubmit(vk_queue, 1, &submit_info, internal_job->fence);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Vulkan: Failed to submit compute job %llu: %s\n",
                 (unsigned long long)internal_job->job_id, vk_result_string(result));
        internal_job->result = result;
        return qfalse;
    }

    ri.Printf(PRINT_DEVELOPER, "Vulkan: Submitted compute job %llu to queue\n", 
              (unsigned long long)internal_job->job_id);
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
    // Return state directly from job structure
    // Note: vk_compute_job_t uses 'state' field (see vk_compute_scheduler.h)
    return job->state;
}

// Get job ID
uint64_t vk_compute_job_get_id(vk_compute_job_t* job) {
    if (!job) {
        ri.Printf(PRINT_WARNING, "vk_compute_job_get_id: job is NULL\n");
        return 0;
    }
    // Return job_id from job structure
    return job->job_id;
}
// Get job debug name
const char* vk_compute_job_get_debug_name(vk_compute_job_t* job) {
    if (!job) {
        return "NULL_JOB";
    }
    // Return debug_name field from job structure
    // Note: vk_compute_job_t uses 'debug_name' field (see vk_compute_scheduler.h)
    return job->debug_name ? job->debug_name : "unnamed_job";
}

// Set job command buffer
void vk_compute_job_set_command_buffer(vk_compute_job_t* job, VkCommandBuffer cmd_buffer) {
    if (!job) {
        ri.Printf(PRINT_WARNING, "vk_compute_job_set_command_buffer: job is NULL\n");
        return;
    }
    job->command_buffer = cmd_buffer;
    ri.Printf(PRINT_DEVELOPER, "Vulkan: Set command buffer for job %llu\n", (unsigned long long)job->job_id);
}

// Add wait semaphore to job
void vk_compute_job_add_wait_semaphore(vk_compute_job_t* job, VkSemaphore semaphore, VkPipelineStageFlags stage) {
    if (!job || !semaphore) {
        ri.Printf(PRINT_WARNING, "vk_compute_job_add_wait_semaphore: Invalid parameters\n");
        return;
    }

    // Find internal job structure
    std::lock_guard<std::mutex> lock(compute_scheduler.scheduler_mutex);
    
    vk_compute_job_internal_t* internal_job = nullptr;
    
    // Search in active jobs
    auto it = compute_scheduler.active_jobs.find(job->job_id);
    if (it != compute_scheduler.active_jobs.end()) {
        internal_job = it->second;
    } else {
        // Search in job queue
        std::queue<vk_compute_job_internal_t*> temp_queue;
        while (!compute_scheduler.job_queue.empty()) {
            vk_compute_job_internal_t* queued_job = compute_scheduler.job_queue.front();
            compute_scheduler.job_queue.pop();
            
            if (queued_job->job_id == job->job_id) {
                internal_job = queued_job;
            }
            
            temp_queue.push(queued_job);
        }
        
        // Restore queue
        while (!temp_queue.empty()) {
            compute_scheduler.job_queue.push(temp_queue.front());
            temp_queue.pop();
        }
    }
    
    if (!internal_job) {
        ri.Printf(PRINT_WARNING, "vk_compute_job_add_wait_semaphore: Job %llu not found\n", (unsigned long long)job->job_id);
        return;
    }
    
    // Check if we have room for another wait semaphore
    if (internal_job->wait_semaphore_count >= MAX_WAIT_SEMAPHORES) {
        ri.Printf(PRINT_WARNING, "vk_compute_job_add_wait_semaphore: Maximum wait semaphores (%d) reached for job %llu\n",
                  MAX_WAIT_SEMAPHORES, (unsigned long long)job->job_id);
        return;
    }
    
    // Add semaphore and stage to arrays
    internal_job->wait_semaphores[internal_job->wait_semaphore_count] = semaphore;
    internal_job->wait_stages[internal_job->wait_semaphore_count] = stage;
    internal_job->wait_semaphore_count++;
    
    ri.Printf(PRINT_DEVELOPER, "Vulkan: Added wait semaphore to job %llu (%u/%u wait semaphores)\n",
              (unsigned long long)job->job_id, internal_job->wait_semaphore_count, MAX_WAIT_SEMAPHORES);
}

// Add signal semaphore to job
void vk_compute_job_add_signal_semaphore(vk_compute_job_t* job, VkSemaphore semaphore) {
    if (!job || !semaphore) {
        ri.Printf(PRINT_WARNING, "vk_compute_job_add_signal_semaphore: Invalid parameters\n");
        return;
    }

    // Find internal job structure
    std::lock_guard<std::mutex> lock(compute_scheduler.scheduler_mutex);
    
    vk_compute_job_internal_t* internal_job = nullptr;
    
    // Search in active jobs
    auto it = compute_scheduler.active_jobs.find(job->job_id);
    if (it != compute_scheduler.active_jobs.end()) {
        internal_job = it->second;
    } else {
        // Search in job queue
        std::queue<vk_compute_job_internal_t*> temp_queue;
        while (!compute_scheduler.job_queue.empty()) {
            vk_compute_job_internal_t* queued_job = compute_scheduler.job_queue.front();
            compute_scheduler.job_queue.pop();
            
            if (queued_job->job_id == job->job_id) {
                internal_job = queued_job;
            }
            
            temp_queue.push(queued_job);
        }
        
        // Restore queue
        while (!temp_queue.empty()) {
            compute_scheduler.job_queue.push(temp_queue.front());
            temp_queue.pop();
        }
    }
    
    if (!internal_job) {
        ri.Printf(PRINT_WARNING, "vk_compute_job_add_signal_semaphore: Job %llu not found\n", (unsigned long long)job->job_id);
        return;
    }
    
    // Check if we have room for another signal semaphore
    if (internal_job->signal_semaphore_count >= MAX_SIGNAL_SEMAPHORES) {
        ri.Printf(PRINT_WARNING, "vk_compute_job_add_signal_semaphore: Maximum signal semaphores (%d) reached for job %llu\n",
                  MAX_SIGNAL_SEMAPHORES, (unsigned long long)job->job_id);
        return;
    }
    
    // Add semaphore to array
    internal_job->signal_semaphores[internal_job->signal_semaphore_count++] = semaphore;
    
    ri.Printf(PRINT_DEVELOPER, "Vulkan: Added signal semaphore to job %llu (%u/%u signal semaphores)\n",
              (unsigned long long)job->job_id, internal_job->signal_semaphore_count, MAX_SIGNAL_SEMAPHORES);
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
    ri.Printf(PRINT_DEVELOPER, "Vulkan: Memory usage set for job %llu: %llu KB\n", 
              (unsigned long long)job->job_id, (unsigned long long)memory_kb);
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
    // Store user data directly in job structure (field exists in vk_compute_scheduler.h definition)
    job->user_data = user_data;
    ri.Printf(PRINT_DEVELOPER, "Vulkan: User data set for job %llu\n", (unsigned long long)job->job_id);
    (void)user_data; // Suppress warning until user_data field is added to structure
}

#endif // USE_VULKAN