/*
=============================================================================
Pixel Buffer Objects (PBO) System Implementation
=============================================================================
*/

#include "tr_local.h"
#include "vk_pbo.h"
#include "vk_utils.h"
#include <string.h>

#ifdef USE_VULKAN

// CVars
cvar_t *r_pbo;
cvar_t *r_pboBuffers;
cvar_t *r_pboAsync;

// Global system state
static pbo_system_t pbo_system;

// Forward declarations
static qboolean vk_create_pbo_resources(void);
static void vk_destroy_pbo_resources(void);
static int vk_find_available_buffer(VkDeviceSize required_size);
static qboolean vk_execute_texture_upload(int buffer_index);

// Utility functions
extern void vk_set_object_name(uint64_t obj, const char *name, VkDebugReportObjectTypeEXT type);
#define SET_OBJECT_NAME(obj, objName, objType) vk_set_object_name((uint64_t)(obj), (objName), (objType))

// Vulkan function pointers
extern PFN_vkCreateBuffer qvkCreateBuffer;
extern PFN_vkDestroyBuffer qvkDestroyBuffer;
extern PFN_vkGetBufferMemoryRequirements qvkGetBufferMemoryRequirements;
extern PFN_vkAllocateMemory qvkAllocateMemory;
extern PFN_vkFreeMemory qvkFreeMemory;
extern PFN_vkBindBufferMemory qvkBindBufferMemory;
extern PFN_vkMapMemory qvkMapMemory;
extern PFN_vkUnmapMemory qvkUnmapMemory;
extern PFN_vkCreateCommandPool qvkCreateCommandPool;
extern PFN_vkDestroyCommandPool qvkDestroyCommandPool;
extern PFN_vkAllocateCommandBuffers qvkAllocateCommandBuffers;
extern PFN_vkFreeCommandBuffers qvkFreeCommandBuffers;
extern PFN_vkBeginCommandBuffer qvkBeginCommandBuffer;
extern PFN_vkEndCommandBuffer qvkEndCommandBuffer;
extern PFN_vkCmdCopyBufferToImage qvkCmdCopyBufferToImage;
extern PFN_vkCmdPipelineBarrier qvkCmdPipelineBarrier;
extern PFN_vkQueueSubmit qvkQueueSubmit;
extern PFN_vkQueueWaitIdle qvkQueueWaitIdle;
extern PFN_vkCreateSemaphore qvkCreateSemaphore;
extern PFN_vkDestroySemaphore qvkDestroySemaphore;
extern PFN_vkCreateFence qvkCreateFence;
extern PFN_vkDestroyFence qvkDestroyFence;
extern PFN_vkWaitForFences qvkWaitForFences;
extern PFN_vkResetFences qvkResetFences;

// Initialize PBO system
void vk_pbo_init(void) {
    ri.Printf(PRINT_ALL, "Vulkan: Initializing PBO system\n");

    memset(&pbo_system, 0, sizeof(pbo_system));

    // Register CVars
    r_pbo = ri.Cvar_Get("r_pbo", "1", CVAR_ARCHIVE);
    r_pboBuffers = ri.Cvar_Get("r_pboBuffers", "4", CVAR_ARCHIVE);
    r_pboAsync = ri.Cvar_Get("r_pboAsync", "1", CVAR_ARCHIVE);

    int num_buffers = ri.Min(r_pboBuffers->integer, MAX_PBO_BUFFERS);
    if (num_buffers <= 0) {
        num_buffers = 4;
    }

    pbo_system.next_buffer_index = num_buffers;

    // Create Vulkan resources
    if (!vk_create_pbo_resources()) {
        ri.Printf(PRINT_ERROR, "Vulkan: Failed to create PBO resources\n");
        return;
    }

    pbo_system.initialized = qtrue;
    pbo_system.enabled = qtrue;

    ri.Printf(PRINT_ALL, "Vulkan: PBO system initialized with %d buffers\n", num_buffers);
}

// Shutdown PBO system
void vk_pbo_shutdown(void) {
    ri.Printf(PRINT_ALL, "Vulkan: Shutting down PBO system\n");

    // Wait for all pending uploads to complete
    vk_pbo_wait_all_uploads();

    vk_destroy_pbo_resources();

    pbo_system.initialized = qfalse;
    ri.Printf(PRINT_ALL, "Vulkan: PBO system shut down\n");
}

// Upload texture asynchronously
qboolean vk_pbo_upload_texture_async(const void *data, VkDeviceSize size,
                                   VkImage dst_image, VkImageLayout final_layout,
                                   const VkBufferImageCopy *region,
                                   void (*completion_callback)(void *user_data),
                                   void *user_data) {
    if (!pbo_system.initialized || !pbo_system.enabled || !r_pboAsync->integer) {
        return qfalse;
    }

    int buffer_index = vk_find_available_buffer(size);
    if (buffer_index == -1) {
        return qfalse;
    }

    pbo_upload_job_t *job = &pbo_system.upload_jobs[buffer_index];

    // Initialize job
    job->active = qtrue;
    job->size = size;
    job->dst_image = dst_image;
    job->final_layout = final_layout;
    job->completion_callback = completion_callback;
    job->user_data = user_data;

    if (region) {
        job->region = *region;
    } else {
        // Default region
        job->region = (VkBufferImageCopy){
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1
            },
            .imageOffset = {0, 0, 0},
            .imageExtent = {0, 0, 0}  // Will be set when image info is available
        };
    }

    // Map buffer and copy data
    if (qvkMapMemory(vk.device, pbo_system.staging_memory[buffer_index], 0, size, 0, &job->mapped_data) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_pbo_upload_texture_async: Failed to map staging buffer\n");
        job->active = qfalse;
        return qfalse;
    }

    memcpy(job->mapped_data, data, size);
    qvkUnmapMemory(vk.device, pbo_system.staging_memory[buffer_index]);

    // Execute the upload
    if (!vk_execute_texture_upload(buffer_index)) {
        job->active = qfalse;
        return qfalse;
    }

    // Update statistics
    pbo_system.total_uploads++;
    pbo_system.total_bytes_uploaded += size;

    return qtrue;
}

// Upload texture synchronously
qboolean vk_pbo_upload_texture_sync(const void *data, VkDeviceSize size,
                                  VkImage dst_image, VkImageLayout final_layout,
                                  const VkBufferImageCopy *region) {
    if (!pbo_system.initialized || !pbo_system.enabled) {
        return qfalse;
    }

    int buffer_index = vk_find_available_buffer(size);
    if (buffer_index == -1) {
        // Fall back to synchronous upload using a temporary staging buffer
        return vk_upload_image_data(dst_image, 0, data, size, region, final_layout);
    }

    // For sync uploads, we can reuse the async path but wait for completion
    qboolean success = vk_pbo_upload_texture_async(data, size, dst_image, final_layout, region, NULL, NULL);
    if (success) {
        // Wait for the upload to complete
        vk_pbo_wait_all_uploads();
    }

    return success;
}

// Check if PBO system is available
qboolean vk_pbo_is_available(void) {
    return pbo_system.initialized && pbo_system.enabled;
}

// Wait for all uploads to complete
void vk_pbo_wait_all_uploads(void) {
    if (!pbo_system.initialized) {
        return;
    }

    // Wait for all transfer fences
    for (int i = 0; i < MAX_PBO_BUFFERS; i++) {
        if (pbo_system.upload_jobs[i].active && pbo_system.transfer_fences[i] != VK_NULL_HANDLE) {
            qvkWaitForFences(vk.device, 1, &pbo_system.transfer_fences[i], VK_TRUE, UINT64_MAX);
            qvkResetFences(vk.device, 1, &pbo_system.transfer_fences[i]);

            // Call completion callback
            pbo_upload_job_t *job = &pbo_system.upload_jobs[i];
            if (job->completion_callback) {
                job->completion_callback(job->user_data);
            }

            job->active = qfalse;
        }
    }
}

// Update PBO system (check for completed uploads)
void vk_pbo_update(void) {
    if (!pbo_system.initialized) {
        return;
    }

    // Check for completed async uploads
    for (int i = 0; i < MAX_PBO_BUFFERS; i++) {
        if (pbo_system.upload_jobs[i].active && pbo_system.transfer_fences[i] != VK_NULL_HANDLE) {
            VkResult result = qvkWaitForFences(vk.device, 1, &pbo_system.transfer_fences[i], VK_FALSE, 0);

            if (result == VK_SUCCESS) {
                // Upload completed
                qvkResetFences(vk.device, 1, &pbo_system.transfer_fences[i]);

                // Call completion callback
                pbo_upload_job_t *job = &pbo_system.upload_jobs[i];
                if (job->completion_callback) {
                    job->completion_callback(job->user_data);
                }

                job->active = qfalse;
            }
        }
    }
}

// Get PBO statistics
void vk_pbo_get_stats(uint64_t *total_uploads, uint64_t *total_bytes, float *avg_time) {
    if (total_uploads) *total_uploads = pbo_system.total_uploads;
    if (total_bytes) *total_bytes = pbo_system.total_bytes_uploaded;
    if (avg_time) *avg_time = pbo_system.average_upload_time_ms;
}

// Create Vulkan resources for PBO system
static qboolean vk_create_pbo_resources(void) {
    VkResult result;
    int num_buffers = ri.Min(r_pboBuffers->integer, MAX_PBO_BUFFERS);

    // Create staging buffers
    for (int i = 0; i < num_buffers; i++) {
        VkBufferCreateInfo bufferInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = MAX_PBO_SIZE,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };

        result = qvkCreateBuffer(vk.device, &bufferInfo, NULL, &pbo_system.staging_buffers[i]);
        if (result != VK_SUCCESS) {
            ri.Printf(PRINT_ERROR, "vk_create_pbo_resources: Failed to create staging buffer %d\n", i);
            return qfalse;
        }

        SET_OBJECT_NAME(pbo_system.staging_buffers[i], va("pbo_staging_buffer_%d", i), VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT);

        // Allocate memory
        VkMemoryRequirements memReqs;
        qvkGetBufferMemoryRequirements(vk.device, pbo_system.staging_buffers[i], &memReqs);

        VkMemoryAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memReqs.size,
            .memoryTypeIndex = find_memory_type(memReqs.memoryTypeBits,
                                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        };

        result = qvkAllocateMemory(vk.device, &allocInfo, NULL, &pbo_system.staging_memory[i]);
        if (result != VK_SUCCESS) {
            ri.Printf(PRINT_ERROR, "vk_create_pbo_resources: Failed to allocate staging buffer memory %d\n", i);
            return qfalse;
        }

        qvkBindBufferMemory(vk.device, pbo_system.staging_buffers[i], pbo_system.staging_memory[i], 0);
        pbo_system.buffer_sizes[i] = MAX_PBO_SIZE;
    }

    // Create command pool for transfers
    VkCommandPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = vk.queue_family_index
    };

    result = qvkCreateCommandPool(vk.device, &poolInfo, NULL, &pbo_system.transfer_command_pool);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_pbo_resources: Failed to create transfer command pool\n");
        return qfalse;
    }

    // Allocate command buffers
    VkCommandBufferAllocateInfo cmdAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pbo_system.transfer_command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = num_buffers
    };

    result = qvkAllocateCommandBuffers(vk.device, &cmdAllocInfo, pbo_system.transfer_command_buffers);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_pbo_resources: Failed to allocate transfer command buffers\n");
        return qfalse;
    }

    // Create semaphores and fences
    VkSemaphoreCreateInfo semInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = 0  // Unsigneled initially
    };

    for (int i = 0; i < num_buffers; i++) {
        result = qvkCreateSemaphore(vk.device, &semInfo, NULL, &pbo_system.transfer_semaphores[i]);
        if (result != VK_SUCCESS) {
            ri.Printf(PRINT_ERROR, "vk_create_pbo_resources: Failed to create transfer semaphore %d\n", i);
            return qfalse;
        }

        result = qvkCreateFence(vk.device, &fenceInfo, NULL, &pbo_system.transfer_fences[i]);
        if (result != VK_SUCCESS) {
            ri.Printf(PRINT_ERROR, "vk_create_pbo_resources: Failed to create transfer fence %d\n", i);
            return qfalse;
        }

        SET_OBJECT_NAME(pbo_system.transfer_command_buffers[i], va("pbo_cmd_buffer_%d", i), VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT);
    }

    ri.Printf(PRINT_ALL, "Vulkan: PBO resources created successfully\n");
    return qtrue;
}

// Destroy Vulkan resources for PBO system
static void vk_destroy_pbo_resources(void) {
    int num_buffers = ri.Min(r_pboBuffers->integer, MAX_PBO_BUFFERS);

    // Destroy semaphores and fences
    for (int i = 0; i < num_buffers; i++) {
        if (pbo_system.transfer_semaphores[i]) {
            qvkDestroySemaphore(vk.device, pbo_system.transfer_semaphores[i], NULL);
            pbo_system.transfer_semaphores[i] = VK_NULL_HANDLE;
        }

        if (pbo_system.transfer_fences[i]) {
            qvkDestroyFence(vk.device, pbo_system.transfer_fences[i], NULL);
            pbo_system.transfer_fences[i] = VK_NULL_HANDLE;
        }
    }

    // Free command buffers
    if (pbo_system.transfer_command_pool && pbo_system.transfer_command_buffers[0]) {
        qvkFreeCommandBuffers(vk.device, pbo_system.transfer_command_pool, num_buffers, pbo_system.transfer_command_buffers);
        memset(pbo_system.transfer_command_buffers, 0, sizeof(pbo_system.transfer_command_buffers));
    }

    // Destroy command pool
    if (pbo_system.transfer_command_pool) {
        qvkDestroyCommandPool(vk.device, pbo_system.transfer_command_pool, NULL);
        pbo_system.transfer_command_pool = VK_NULL_HANDLE;
    }

    // Destroy staging buffers
    for (int i = 0; i < num_buffers; i++) {
        if (pbo_system.staging_buffers[i]) {
            qvkDestroyBuffer(vk.device, pbo_system.staging_buffers[i], NULL);
            pbo_system.staging_buffers[i] = VK_NULL_HANDLE;
        }

        if (pbo_system.staging_memory[i]) {
            qvkFreeMemory(vk.device, pbo_system.staging_memory[i], NULL);
            pbo_system.staging_memory[i] = VK_NULL_HANDLE;
        }

        pbo_system.buffer_sizes[i] = 0;
    }
}

// Find an available buffer for upload
static int vk_find_available_buffer(VkDeviceSize required_size) {
    int num_buffers = ri.Min(r_pboBuffers->integer, MAX_PBO_BUFFERS);

    // Check if we have a round-robin buffer available
    for (int attempts = 0; attempts < num_buffers; attempts++) {
        int index = pbo_system.next_buffer_index % num_buffers;

        if (!pbo_system.upload_jobs[index].active && pbo_system.buffer_sizes[index] >= required_size) {
            pbo_system.next_buffer_index = (index + 1) % num_buffers;
            return index;
        }

        pbo_system.next_buffer_index = (pbo_system.next_buffer_index + 1) % num_buffers;
    }

    return -1;  // No available buffer
}

// Execute texture upload
static qboolean vk_execute_texture_upload(int buffer_index) {
    VkCommandBuffer cmdBuf = pbo_system.transfer_command_buffers[buffer_index];
    pbo_upload_job_t *job = &pbo_system.upload_jobs[buffer_index];

    // Begin command buffer
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    VkResult result = qvkBeginCommandBuffer(cmdBuf, &beginInfo);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_execute_texture_upload: Failed to begin command buffer\n");
        return qfalse;
    }

    // Transition image to transfer dst
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = job->dst_image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    qvkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, NULL, 0, NULL, 1, &barrier);

    // Copy buffer to image
    qvkCmdCopyBufferToImage(cmdBuf, pbo_system.staging_buffers[buffer_index], job->dst_image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &job->region);

    // Transition image to final layout
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = job->final_layout;

    qvkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, NULL, 0, NULL, 1, &barrier);

    // End command buffer
    result = qvkEndCommandBuffer(cmdBuf);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_execute_texture_upload: Failed to end command buffer\n");
        return qfalse;
    }

    // Submit to queue
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmdBuf,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &pbo_system.transfer_semaphores[buffer_index]
    };

    result = qvkQueueSubmit(vk.queue, 1, &submitInfo, pbo_system.transfer_fences[buffer_index]);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_execute_texture_upload: Failed to submit transfer\n");
        return qfalse;
    }

    return qtrue;
}

#endif // USE_VULKAN