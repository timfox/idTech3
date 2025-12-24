#include "vk_compute.h"
#include "../renderercommon/tr_public.h"
#include "vk.h"
#include "vk_commands.h"
#include <string.h>

extern refimport_t ri;

// Vulkan function pointer extern declarations
extern PFN_vkResetFences qvkResetFences;
extern PFN_vkQueueSubmit qvkQueueSubmit;
extern PFN_vkWaitForFences qvkWaitForFences;
extern PFN_vkCreateCommandPool qvkCreateCommandPool;
extern PFN_vkDestroyCommandPool qvkDestroyCommandPool;
extern PFN_vkCreateFence qvkCreateFence;
extern PFN_vkDestroyFence qvkDestroyFence;
extern PFN_vkCreateSemaphore qvkCreateSemaphore;
extern PFN_vkDestroySemaphore qvkDestroySemaphore;

// Initialize compute manager
qboolean vk_init_compute_manager(void) {
    if (vk.compute_manager.initialized) {
        return qtrue;
    }

    ri.Printf(PRINT_ALL, "Vulkan: Initializing GPU-Async compute manager\n");

    vk_compute_manager_t *manager = &vk.compute_manager;

    // Check if compute queue is available
    if (manager->queue == VK_NULL_HANDLE) {
        ri.Printf(PRINT_WARNING, "Vulkan: Async compute queue not available\n");
        return qfalse;
    }

    // Create command pool for compute
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = manager->queue_family_index,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
    };

    if (qvkCreateCommandPool(vk.device, &pool_info, NULL, &manager->command_pool) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Vulkan: Failed to create compute command pool\n");
        return qfalse;
    }

    // Create timeline semaphore for compute sync
    VkSemaphoreTypeCreateInfo type_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .pNext = NULL,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
        .initialValue = 0
    };

    VkSemaphoreCreateInfo sem_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &type_info,
        .flags = 0
    };

    if (qvkCreateSemaphore(vk.device, &sem_info, NULL, &manager->timeline_semaphore) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Vulkan: Failed to create compute timeline semaphore\n");
        qvkDestroyCommandPool(vk.device, manager->command_pool, NULL);
        return qfalse;
    }

    manager->enabled = qtrue;
    atomic_store_explicit(&manager->current_timeline_value, 0, memory_order_relaxed);
    atomic_store_explicit(&manager->next_job_id, 1, memory_order_relaxed);
    manager->debug_name = "compute_manager";
    manager->initialized = qtrue;

    ri.Printf(PRINT_ALL, "Vulkan: GPU-Async compute manager initialized\n");

    return qtrue;
}

// Shutdown compute manager
void vk_shutdown_compute_manager(void) {
    if (!vk.compute_manager.initialized) {
        return;
    }

    ri.Printf(PRINT_ALL, "Vulkan: Shutting down GPU-Async compute manager\n");

    vk_compute_manager_t *manager = &vk.compute_manager;

    // Wait for all jobs to complete
    vk_wait_all_compute_jobs();

    // Destroy jobs
    for (int i = 0; i < MAX_COMPUTE_JOBS; i++) {
        if (manager->jobs[i].fence != VK_NULL_HANDLE) {
            qvkDestroyFence(vk.device, manager->jobs[i].fence, NULL);
            manager->jobs[i].fence = VK_NULL_HANDLE;
        }
    }

    // Destroy resources
    if (manager->timeline_semaphore != VK_NULL_HANDLE) {
        qvkDestroySemaphore(vk.device, manager->timeline_semaphore, NULL);
        manager->timeline_semaphore = VK_NULL_HANDLE;
    }
    
    if (manager->command_pool != VK_NULL_HANDLE) {
        qvkDestroyCommandPool(vk.device, manager->command_pool, NULL);
        manager->command_pool = VK_NULL_HANDLE;
    }

    manager->initialized = qfalse;
    ri.Printf(PRINT_ALL, "Vulkan: GPU-Async compute manager shutdown complete\n");
}

// Internal helper: Actually submit a job to the GPU
static qboolean vk_dispatch_compute_job(vk_compute_job_t *job) {
    vk_compute_manager_t *manager = &vk.compute_manager;

    // Create fence if not already created
    if (job->fence == VK_NULL_HANDLE) {
        VkFenceCreateInfo fence_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = 0
        };
        qvkCreateFence(vk.device, &fence_info, NULL, &job->fence);
    } else {
        qvkResetFences(vk.device, 1, &job->fence);
    }

    // Prepare submission
    uint64_t current_val = atomic_fetch_add_explicit(&manager->current_timeline_value, 1, memory_order_relaxed) + 1;
    job->timeline_signal_value = current_val;
    job->submission_time_ns = ri.Microseconds() * 1000;

    VkTimelineSemaphoreSubmitInfo timeline_info = {
        .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
        .pNext = NULL,
        .waitSemaphoreValueCount = 0,
        .pWaitSemaphoreValues = NULL,
        .signalSemaphoreValueCount = 1,
        .pSignalSemaphoreValues = &job->timeline_signal_value
    };

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = &timeline_info,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = NULL,
        .pWaitDstStageMask = NULL,
        .commandBufferCount = 1,
        .pCommandBuffers = &job->command_buffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &manager->timeline_semaphore
    };

    // Synchronization with graphics queue if requested
    if (job->wait_for_graphics && vk.timeline_semaphore != VK_NULL_HANDLE) {
        static VkPipelineStageFlags wait_stages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = &vk.timeline_semaphore;
        submit_info.pWaitDstStageMask = &wait_stages;
    }

    VkResult result = qvkQueueSubmit(manager->queue, 1, &submit_info, job->fence);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Vulkan: Failed to dispatch compute job '%s': %s\n", job->name, vk_result_string(result));
        job->status = VK_COMPUTE_STATUS_FAILED;
        return qfalse;
    }

    job->status = VK_COMPUTE_STATUS_SUBMITTED;
    atomic_fetch_add_explicit(&manager->total_jobs_submitted, 1, memory_order_relaxed);
    return qtrue;
}

// Submit a new compute job
uint32_t vk_submit_compute_job(const char *name, VkCommandBuffer cmd_buffer, vk_compute_priority_t priority, qboolean wait_for_graphics, uint32_t *dependencies, uint32_t dependency_count) {
    if (!vk.compute_manager.enabled || !vk.compute_manager.initialized) {
        return 0;
    }

    vk_compute_manager_t *manager = &vk.compute_manager;

    // Find free job slot
    int job_idx = -1;
    for (int i = 0; i < MAX_COMPUTE_JOBS; i++) {
        if (!manager->jobs[i].active) {
            job_idx = i;
            break;
        }
    }

    if (job_idx == -1) {
        ri.Printf(PRINT_WARNING, "Vulkan: Max compute jobs reached\n");
        return 0;
    }

    vk_compute_job_t *job = &manager->jobs[job_idx];
    memset(job, 0, sizeof(vk_compute_job_t));

    job->id = atomic_fetch_add_explicit(&manager->next_job_id, 1, memory_order_relaxed);
    job->name = name;
    job->priority = priority;
    job->status = VK_COMPUTE_STATUS_PENDING;
    job->command_buffer = cmd_buffer;
    job->wait_for_graphics = wait_for_graphics;
    job->active = qtrue;

    // Copy dependencies
    job->dependency_count = (dependency_count > MAX_DEPENDENCIES) ? MAX_DEPENDENCIES : dependency_count;
    for (uint32_t i = 0; i < job->dependency_count; i++) {
        job->dependencies[i] = dependencies[i];
    }

    atomic_fetch_add_explicit(&manager->active_job_count, 1, memory_order_relaxed);

    // If no dependencies, we can dispatch immediately (or wait for manager update)
    // For now, let's let the update loop handle all dispatching to ensure proper graph resolution
    
    return job->id;
}

// Check if a compute job is complete
qboolean vk_is_compute_job_complete(uint32_t job_id) {
    if (!vk.compute_manager.initialized || job_id == 0) return qtrue;

    vk_compute_manager_t *manager = &vk.compute_manager;
    for (int i = 0; i < MAX_COMPUTE_JOBS; i++) {
        if (manager->jobs[i].active && manager->jobs[i].id == job_id) {
            if (manager->jobs[i].status == VK_COMPUTE_STATUS_COMPLETED) return qtrue;
            if (manager->jobs[i].status == VK_COMPUTE_STATUS_PENDING) return qfalse;
            if (manager->jobs[i].status == VK_COMPUTE_STATUS_FAILED) return qtrue;
            
            VkResult result = qvkWaitForFences(vk.device, 1, &manager->jobs[i].fence, VK_TRUE, 0);
            if (result == VK_SUCCESS) {
                manager->jobs[i].status = VK_COMPUTE_STATUS_COMPLETED;
                manager->jobs[i].completion_time_ns = ri.Microseconds() * 1000;
                manager->jobs[i].execution_duration_ms = (float)(manager->jobs[i].completion_time_ns - manager->jobs[i].submission_time_ns) / 1000000.0f;
                
                // Update stats
                uint64_t completed_count = atomic_fetch_add_explicit(&manager->total_jobs_completed, 1, memory_order_relaxed) + 1;
                manager->average_execution_time_ms = (manager->average_execution_time_ms * (float)(completed_count - 1) + manager->jobs[i].execution_duration_ms) / (float)completed_count;
                if (manager->jobs[i].execution_duration_ms > manager->peak_execution_time_ms) {
                    manager->peak_execution_time_ms = manager->jobs[i].execution_duration_ms;
                }
                
                return qtrue;
            }
            return qfalse;
        }
    }
    return qtrue;
}

// Wait for a specific compute job
void vk_wait_for_compute_job(uint32_t job_id) {
    if (!vk.compute_manager.initialized || job_id == 0) return;

    vk_compute_manager_t *manager = &vk.compute_manager;
    for (int i = 0; i < MAX_COMPUTE_JOBS; i++) {
        if (manager->jobs[i].active && manager->jobs[i].id == job_id) {
            // If pending, we must force update to dispatch it (or dispatch it here)
            while (manager->jobs[i].status == VK_COMPUTE_STATUS_PENDING) {
                vk_update_compute_manager();
            }

            if (manager->jobs[i].status == VK_COMPUTE_STATUS_SUBMITTED) {
                qvkWaitForFences(vk.device, 1, &manager->jobs[i].fence, VK_TRUE, UINT64_MAX);
                manager->jobs[i].status = VK_COMPUTE_STATUS_COMPLETED;
                manager->jobs[i].completion_time_ns = ri.Microseconds() * 1000;
                manager->jobs[i].execution_duration_ms = (float)(manager->jobs[i].completion_time_ns - manager->jobs[i].submission_time_ns) / 1000000.0f;
                atomic_fetch_add_explicit(&manager->total_jobs_completed, 1, memory_order_relaxed);
            }
            return;
        }
    }
}

// Wait for all compute jobs to complete
void vk_wait_all_compute_jobs(void) {
    if (!vk.compute_manager.initialized) return;

    vk_compute_manager_t *manager = &vk.compute_manager;
    qboolean all_done;
    do {
        all_done = qtrue;
        vk_update_compute_manager();
        for (int i = 0; i < MAX_COMPUTE_JOBS; i++) {
            if (manager->jobs[i].active && manager->jobs[i].status != VK_COMPUTE_STATUS_COMPLETED && manager->jobs[i].status != VK_COMPUTE_STATUS_FAILED) {
                all_done = qfalse;
                if (manager->jobs[i].status == VK_COMPUTE_STATUS_SUBMITTED) {
                    vk_wait_for_compute_job(manager->jobs[i].id);
                }
                break;
            }
        }
    } while (!all_done);
}

// Update compute manager (cleanup completed jobs and dispatch pending ones)
void vk_update_compute_manager(void) {
    if (!vk.compute_manager.initialized) return;

    vk_compute_manager_t *manager = &vk.compute_manager;

    // 1. Check for completed jobs
    for (int i = 0; i < MAX_COMPUTE_JOBS; i++) {
        if (manager->jobs[i].active && manager->jobs[i].status == VK_COMPUTE_STATUS_SUBMITTED) {
            vk_is_compute_job_complete(manager->jobs[i].id);
        }
    }

    // 2. Dispatch pending jobs whose dependencies are met
    for (int i = 0; i < MAX_COMPUTE_JOBS; i++) {
        if (manager->jobs[i].active && manager->jobs[i].status == VK_COMPUTE_STATUS_PENDING) {
            qboolean dependencies_met = qtrue;
            for (uint32_t d = 0; d < manager->jobs[i].dependency_count; d++) {
                if (!vk_is_compute_job_complete(manager->jobs[i].dependencies[d])) {
                    dependencies_met = qfalse;
                    break;
                }
            }

            if (dependencies_met) {
                vk_dispatch_compute_job(&manager->jobs[i]);
            }
        }
    }

    // 3. Cleanup old completed jobs (optional: could keep for history, but for now reuse slots)
    // We only cleanup jobs if they are COMPLETED or FAILED and not referenced as dependency by any active job
    for (int i = 0; i < MAX_COMPUTE_JOBS; i++) {
        if (manager->jobs[i].active && (manager->jobs[i].status == VK_COMPUTE_STATUS_COMPLETED || manager->jobs[i].status == VK_COMPUTE_STATUS_FAILED)) {
            qboolean is_dependency = qfalse;
            for (int j = 0; j < MAX_COMPUTE_JOBS; j++) {
                if (manager->jobs[j].active && manager->jobs[j].status == VK_COMPUTE_STATUS_PENDING) {
                    for (uint32_t d = 0; d < manager->jobs[j].dependency_count; d++) {
                        if (manager->jobs[j].dependencies[d] == manager->jobs[i].id) {
                            is_dependency = qtrue;
                            break;
                        }
                    }
                }
                if (is_dependency) break;
            }

            if (!is_dependency) {
                manager->jobs[i].active = qfalse;
                atomic_fetch_sub_explicit(&manager->active_job_count, 1, memory_order_relaxed);
            }
        }
    }
}

// Print compute task graph
void vk_print_compute_task_graph(void) {
    if (!vk.compute_manager.initialized) return;

    vk_compute_manager_t *manager = &vk.compute_manager;
    ri.Printf(PRINT_ALL, "=== GPU-Async Compute Task Graph ===\n");
    for (int i = 0; i < MAX_COMPUTE_JOBS; i++) {
        if (manager->jobs[i].active) {
            const char *status_str = "Unknown";
            switch(manager->jobs[i].status) {
                case VK_COMPUTE_STATUS_PENDING: status_str = "PENDING"; break;
                case VK_COMPUTE_STATUS_SUBMITTED: status_str = "SUBMITTED"; break;
                case VK_COMPUTE_STATUS_COMPLETED: status_str = "COMPLETED"; break;
                case VK_COMPUTE_STATUS_FAILED: status_str = "FAILED"; break;
                default: break;
            }
            ri.Printf(PRINT_ALL, "Job [%u] '%s': %s\n", manager->jobs[i].id, manager->jobs[i].name, status_str);
            if (manager->jobs[i].dependency_count > 0) {
                ri.Printf(PRINT_ALL, "  Dependencies: ");
                for (uint32_t d = 0; d < manager->jobs[i].dependency_count; d++) {
                    ri.Printf(PRINT_ALL, "%u ", manager->jobs[i].dependencies[d]);
                }
                ri.Printf(PRINT_ALL, "\n");
            }
        }
    }
}

// Print compute statistics
void vk_print_compute_stats(void) {
    if (!vk.compute_manager.initialized) return;

    vk_compute_manager_t *manager = &vk.compute_manager;
    ri.Printf(PRINT_ALL, "=== GPU-Async Compute Statistics ===\n");
    ri.Printf(PRINT_ALL, "Status: %s\n", manager->enabled ? "ENABLED" : "DISABLED");
    ri.Printf(PRINT_ALL, "Total Jobs Submitted: %llu\n", (unsigned long long)atomic_load_explicit(&manager->total_jobs_submitted, memory_order_relaxed));
    ri.Printf(PRINT_ALL, "Total Jobs Completed: %llu\n", (unsigned long long)atomic_load_explicit(&manager->total_jobs_completed, memory_order_relaxed));
    ri.Printf(PRINT_ALL, "Active Jobs: %u\n", atomic_load_explicit(&manager->active_job_count, memory_order_relaxed));
    ri.Printf(PRINT_ALL, "Avg Execution Time: %.2f ms\n", manager->average_execution_time_ms);
    ri.Printf(PRINT_ALL, "Peak Execution Time: %.2f ms\n", manager->peak_execution_time_ms);
    ri.Printf(PRINT_ALL, "Timeline Value: %llu\n", (unsigned long long)atomic_load_explicit(&manager->current_timeline_value, memory_order_relaxed));
}

// Enable/disable compute
void vk_set_compute_enabled(qboolean enabled) {
    vk.compute_manager.enabled = enabled;
    ri.Printf(PRINT_ALL, "Vulkan: GPU-Async compute %s\n", enabled ? "enabled" : "disabled");
}
