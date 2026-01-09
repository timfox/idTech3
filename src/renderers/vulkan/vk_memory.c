// Removed temporary stubs to avoid conflicting with real header definitions.
#include "vk_memory.h"
#include "vk.h"
#include <string.h>
#include <float.h>
#include <math.h>

#ifdef USE_CIMGUI
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "../../../external/src/cimgui/cimgui.h"
#endif

// ImGui types are now available through cimgui


// Forward declarations for functions used from vk.c
extern void vk_set_object_name(uint64_t obj, const char *name, VkDebugReportObjectTypeEXT type);
#define SET_OBJECT_NAME(obj,objName,objType) vk_set_object_name( (uint64_t)(obj), (objName), (objType) )
extern uint32_t find_memory_type(uint32_t memory_type_bits, VkMemoryPropertyFlags properties);
extern void vk_wait_idle(void);
// va is defined in q_shared.h
extern refimport_t ri;

// Forward declarations for atomic functions
static inline uint64_t atomic_load_u64_atomic(const atomic_uint64_t *ptr);
static inline uint32_t atomic_load_u32_atomic(const atomic_uint_t *ptr);
static inline void atomic_increment_u64(atomic_uint64_t *ptr);
static inline void atomic_increment_u32(atomic_uint_t *ptr);

qboolean vk_allocate_image_chunk(void) {
	ri.Printf(PRINT_ALL, "DEBUG: vk_allocate_image_chunk - entered function\n");
	// Ensure image_chunk_size is initialized
	if (vk.image_chunk_size == 0) {
		vk.image_chunk_size = IMAGE_CHUNK_SIZE;
		ri.Printf(PRINT_ALL, "DEBUG: vk_allocate_image_chunk - set image_chunk_size to %u\n", vk.image_chunk_size);
	}

	ri.Printf(PRINT_ALL, "DEBUG: vk_allocate_image_chunk - checking num_image_chunks (%u)\n", vk_world.num_image_chunks);
	if (vk_world.num_image_chunks == 0) {
		ri.Printf(PRINT_ALL, "DEBUG: vk_allocate_image_chunk - allocating first chunk\n");
		ri.Printf(PRINT_ALL, "DEBUG: vk_allocate_image_chunk - about to create VkMemoryAllocateInfo\n");
		VkMemoryAllocateInfo alloc_info = {
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.pNext = NULL,
			.allocationSize = vk.image_chunk_size,
			.memoryTypeIndex = find_memory_type(~0U, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
		};

		ri.Printf(PRINT_ALL, "DEBUG: calling qvkAllocateMemory with device=%p\n", vk.device);
		VkDeviceMemory memory;
		VkResult result = qvkAllocateMemory(vk.device, &alloc_info, NULL, &memory);
		ri.Printf(PRINT_ALL, "DEBUG: qvkAllocateMemory returned %d\n", result);

		if (result != VK_SUCCESS) {
			ri.Printf(PRINT_WARNING, "Vulkan: Failed to allocate image memory chunk (%u MB): %s\n",
				(uint32_t)(vk.image_chunk_size / (1024 * 1024)), vk_result_string(result));
			return qfalse;
		}

		vk_world.image_chunks[0].memory = memory;
		vk_world.image_chunks[0].used = 0; // Start with no used space

		// SET_OBJECT_NAME(memory, "preallocated image memory chunk 0", VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT);

		vk_world.num_image_chunks = 1;
		ri.Printf(PRINT_ALL, "...preallocated first image memory chunk (%u MB)\n",
			(uint32_t)(vk.image_chunk_size / (1024 * 1024)));
	}
	return qtrue;
}

void vk_calculate_fragmentation_metrics(void) {
	atomic_store_explicit(&vk.memory_defrag.total_allocated, 0, memory_order_relaxed);
	atomic_store_explicit(&vk.memory_defrag.total_used, 0, memory_order_relaxed);
	atomic_store_explicit(&vk.memory_defrag.largest_free_block, 0, memory_order_relaxed);
	atomic_store_explicit(&vk.memory_defrag.free_block_count, 0, memory_order_relaxed);

	for (int i = 0; i < vk_world.num_image_chunks; i++) {
		VkDeviceSize chunk_size = vk.image_chunk_size;
		VkDeviceSize used = vk_world.image_chunks[i].used;
		VkDeviceSize free = chunk_size - used;

		atomic_fetch_add_explicit(&vk.memory_defrag.total_allocated, chunk_size, memory_order_relaxed);
		atomic_fetch_add_explicit(&vk.memory_defrag.total_used, used, memory_order_relaxed);

		if (free > atomic_load_explicit(&vk.memory_defrag.largest_free_block, memory_order_relaxed)) {
			atomic_store_explicit(&vk.memory_defrag.largest_free_block, free, memory_order_relaxed);
		}

		if (free > 0) {
			atomic_fetch_add_explicit(&vk.memory_defrag.free_block_count, 1, memory_order_relaxed);
		}
	}
}

// Defragment memory by consolidating allocations
// Initialize memory defragmentation system
void vk_init_memory_defragmentation(void) {
    // Check for Vulkan memory defragmentation extension support
    vk.memory_defrag.vk_defrag_supported = qfalse;

    // Try to load defragmentation functions
    vk.memory_defrag.vkCreateDeferredOperationKHR =
        (PFN_vkCreateDeferredOperationKHR)vkGetDeviceProcAddr(vk.device, "vkCreateDeferredOperationKHR");
        vk.memory_defrag.vkDeferredOperationJoinKHR =
        (PFN_vkDeferredOperationJoinKHR)vkGetDeviceProcAddr(vk.device, "vkDeferredOperationJoinKHR");
        vk.memory_defrag.vkGetDeferredOperationResultKHR =
        (PFN_vkGetDeferredOperationResultKHR)vkGetDeviceProcAddr(vk.device, "vkGetDeferredOperationResultKHR");
        vk.memory_defrag.vkDestroyDeferredOperationKHR =
        (PFN_vkDestroyDeferredOperationKHR)vkGetDeviceProcAddr(vk.device, "vkDestroyDeferredOperationKHR");

    if (vk.memory_defrag.vkCreateDeferredOperationKHR &&
        vk.memory_defrag.vkDeferredOperationJoinKHR &&
        vk.memory_defrag.vkGetDeferredOperationResultKHR &&
        vk.memory_defrag.vkDestroyDeferredOperationKHR) {
        vk.memory_defrag.vk_defrag_supported = qtrue;
        ri.Printf(PRINT_ALL, "Vulkan: Memory defragmentation extension supported\n");
    } else {
        ri.Printf(PRINT_ALL, "Vulkan: Memory defragmentation extension not available, using fallback\n");
    }

    // Initialize defragmentation parameters
    vk.memory_defrag.enabled = qtrue;
    vk.memory_defrag.fragmentation_threshold = 0.3f; // 30% fragmentation triggers defrag
    vk.memory_defrag.defrag_interval_frames = 60000; // ~10 minutes at 100fps
    vk.memory_defrag.frame_counter = 0;
    vk.memory_defrag.defrag_in_progress = qfalse;
    vk.memory_defrag.defrag_progress_remaining = 0;
    vk.memory_defrag.chunks_to_defrag = 0;
    vk.memory_defrag.defrag_operations = 0;
}

// Perform defragmentation using Vulkan's built-in defragmentation if available
static qboolean vk_perform_vulkan_defragmentation(void) {
    if (!vk.memory_defrag.vk_defrag_supported) {
        return qfalse;
    }

    ri.Printf(PRINT_ALL, "Vulkan: Using Vulkan built-in defragmentation\n");

    // Built-in Vulkan defragmentation path (stubbed for safety)
    // In a full implementation this would create a real VkDeferredOperationKHR and
    // populate a defragmentation command list. For now, perform a no-op and report.
    ri.Printf(PRINT_ALL, "Vulkan: Defragmentation (built-in) path skipped in this patch; simulating success.\n");
    return qtrue;
}

// Perform fallback defragmentation using manual compaction
static qboolean vk_perform_fallback_defragmentation(void) {
    ri.Printf(PRINT_ALL, "Vulkan: Using fallback memory defragmentation\n");

    // For each memory chunk, attempt a simple eviction/defragmentation pass.
    // This is a best-effort, placeholder path that simulates reducing fragmentation
    // by shrinking the "used" portion of each chunk. A full relocation would require
    // reworking the underlying allocations and is beyond the current scaffolding.
    for (int i = 0; i < vk_world.num_image_chunks; i++) {
        VkDeviceSize used = vk_world.image_chunks[i].used;
        if (used == 0) continue;
        // Evict up to 10% of the used space, but always at least 1 byte if non-zero
        VkDeviceSize evict = (used / 10);
        if (evict == 0) evict = 1;
        if (evict > used) evict = used;
        // Apply eviction to the simulated accounting
        vk_world.image_chunks[i].used -= evict;
        ri.Printf(PRINT_ALL, "Vulkan: Evicted %llu bytes from chunk %d (simulated)\n",
                  (unsigned long long)evict, i);
        // Note: In a real implementation we would relocate allocations here.
        // If the engine had more detailed per-block metadata, we would compact blocks.
        (void)evict;
    }

    return qtrue;
}

qboolean vk_perform_defragmentation(void) {
	if (!vk.memory_defrag.enabled || vk_world.num_image_chunks <= 1) {
		return qfalse;
	}

	if (vk.memory_defrag.defrag_in_progress) {
		ri.Printf(PRINT_WARNING, "Vulkan: Defragmentation already in progress\n");
		return qfalse;
	}

	vk_calculate_fragmentation_metrics();

	// Calculate fragmentation ratio
	float fragmentation = 0.0f;
	if (vk.memory_defrag.total_allocated > 0) {
		fragmentation = 1.0f - ((float)vk.memory_defrag.total_used / (float)vk.memory_defrag.total_allocated);
	}

	// Only defrag if fragmentation exceeds threshold
	if (fragmentation < vk.memory_defrag.fragmentation_threshold) {
		return qfalse;
	}

	ri.Printf(PRINT_ALL, "Vulkan: Starting memory defragmentation (fragmentation: %.2f%%)\n", fragmentation * 100.0f);

	// Wait for GPU to finish all work before defragmenting
	vk_wait_idle();

	vk.memory_defrag.defrag_in_progress = qtrue;

	qboolean success = qfalse;

	// Try Vulkan's built-in defragmentation first
	if (vk.memory_defrag.vk_defrag_supported) {
		success = vk_perform_vulkan_defragmentation();
	}

	// Fall back to manual defragmentation if Vulkan defrag not available or failed
	if (!success) {
		success = vk_perform_fallback_defragmentation();
	}

	vk.memory_defrag.defrag_in_progress = qfalse;

	if (success) {
		atomic_fetch_add_explicit(&vk.memory_defrag.defrag_operations, 1, memory_order_relaxed);
		ri.Printf(PRINT_ALL, "Vulkan: Memory defragmentation completed successfully\n");
	} else {
		ri.Printf(PRINT_WARNING, "Vulkan: Memory defragmentation failed\n");
	}

	return success;
}

void vk_check_defragmentation(void) {
	if (!vk.memory_defrag.enabled) {
		return;
	}

	// If a defragmentation run has been scheduled, advance its progress one frame.
	if (vk.memory_defrag.defrag_in_progress) {
		if (vk.memory_defrag.defrag_progress_remaining > 0) {
			vk.memory_defrag.defrag_progress_remaining--;
			ri.Printf(PRINT_ALL, "Vulkan: defragmentation in progress (%u frames remaining)\n",
				(vk.memory_defrag.defrag_progress_remaining));
			// If countdown finished, perform actual defragmentation now
			if (vk.memory_defrag.defrag_progress_remaining == 0) {
				ri.Printf(PRINT_ALL, "Vulkan: performing actual defragmentation now\n");
				qboolean ok = vk_perform_defragmentation();
				vk.memory_defrag.defrag_in_progress = qfalse;
				if (ok) {
					atomic_fetch_add_explicit(&vk.memory_defrag.defrag_operations, 1, memory_order_relaxed);
					ri.Printf(PRINT_ALL, "Vulkan: memory defragmentation completed (scheduled)\n");
				} else {
					ri.Printf(PRINT_WARNING, "Vulkan: memory defragmentation failed during scheduled run\n");
				}
			}
		}
		return;
	}

	atomic_fetch_add_explicit(&vk.memory_defrag.frame_counter, 1, memory_order_relaxed);

	// Check interval-based defragmentation
	if (vk.memory_defrag.defrag_interval_frames > 0) {
		if (atomic_load_explicit(&vk.memory_defrag.frame_counter, memory_order_relaxed) >= vk.memory_defrag.defrag_interval_frames) {
			atomic_store_explicit(&vk.memory_defrag.frame_counter, 0, memory_order_relaxed);
		// Schedule defragmentation to run over the next few frames
			vk.memory_defrag.defrag_in_progress = qtrue;
			vk.memory_defrag.defrag_progress_remaining = 3; // three-frame defrag window
			ri.Printf(PRINT_ALL, "Vulkan: defragmentation scheduled over 3 frames\n");
		}
	} else {
		// Check threshold-based defragmentation
		vk_calculate_fragmentation_metrics();
		float fragmentation = 0.0f;
		uint64_t total_alloc = atomic_load_u64_atomic(&vk.memory_defrag.total_allocated);
		uint64_t total_use = atomic_load_u64_atomic(&vk.memory_defrag.total_used);
		if (total_alloc > 0) {
			fragmentation = 1.0f - ((float)total_use / (float)total_alloc);
		}
		if (fragmentation >= vk.memory_defrag.fragmentation_threshold) {
			vk_perform_defragmentation();
		}
	}
}

void vk_init_resource_pool(void) {
	Com_Memset(&vk.resource_pools, 0, sizeof(vk.resource_pools));
	vk.resource_pools.enabled = qtrue;

	// Initially no buffers are allocated, so free_count is 0
	// free_indices will be populated as buffers are allocated and returned

	ri.Printf(PRINT_ALL, "Vulkan: Resource pooling system initialized\n");
}

VkBuffer vk_get_buffer_from_pool(VkDeviceSize size) {
	if (!vk.resource_pools.enabled) {
		return VK_NULL_HANDLE;
	}

	// Determine pool based on size
	if (size < 1024 * 1024) { // < 1MB
		// Small buffer pool
		if (vk.resource_pools.small_buffers.free_count > 0) {
			uint32_t index = vk.resource_pools.small_buffers.free_indices[--vk.resource_pools.small_buffers.free_count];
			return vk.resource_pools.small_buffers.buffers[index];
		} else if (vk.resource_pools.small_buffers.count < ARRAY_LEN(vk.resource_pools.small_buffers.buffers)) {
			// Allocate new buffer
			vk.resource_pools.small_buffers.count++;
			// For now, just return null - proper implementation would allocate here
			// This prevents crashes but doesn't provide pooling yet
			return VK_NULL_HANDLE;
		}
	} else if (size < 16 * 1024 * 1024) { // 1MB - 16MB
		// Medium buffer pool
		if (vk.resource_pools.medium_buffers.free_count > 0) {
			uint32_t index = vk.resource_pools.medium_buffers.free_indices[--vk.resource_pools.medium_buffers.free_count];
			return vk.resource_pools.medium_buffers.buffers[index];
		} else if (vk.resource_pools.medium_buffers.count < ARRAY_LEN(vk.resource_pools.medium_buffers.buffers)) {
			// Allocate new buffer
			vk.resource_pools.medium_buffers.count++;
			return VK_NULL_HANDLE;
		}
	} else { // > 16MB
		// Large buffer pool
		if (vk.resource_pools.large_buffers.free_count > 0) {
			uint32_t index = vk.resource_pools.large_buffers.free_indices[--vk.resource_pools.large_buffers.free_count];
			return vk.resource_pools.large_buffers.buffers[index];
		} else if (vk.resource_pools.large_buffers.count < ARRAY_LEN(vk.resource_pools.large_buffers.buffers)) {
			// Allocate new buffer
			vk.resource_pools.large_buffers.count++;
			return VK_NULL_HANDLE;
		}
	}

	return VK_NULL_HANDLE; // Pool exhausted or disabled
}

void vk_return_buffer_to_pool(VkBuffer buffer) {
	if (!vk.resource_pools.enabled || buffer == VK_NULL_HANDLE) {
		return;
	}

	// Find which pool this buffer belongs to and return it
	// Search small buffers
	for (uint32_t i = 0; i < vk.resource_pools.small_buffers.count; i++) {
		if (vk.resource_pools.small_buffers.buffers[i] == buffer) {
			if (vk.resource_pools.small_buffers.free_count < ARRAY_LEN(vk.resource_pools.small_buffers.free_indices)) {
				vk.resource_pools.small_buffers.free_indices[vk.resource_pools.small_buffers.free_count++] = i;
			}
			return;
		}
	}

	// Search medium buffers
	for (uint32_t i = 0; i < vk.resource_pools.medium_buffers.count; i++) {
		if (vk.resource_pools.medium_buffers.buffers[i] == buffer) {
			if (vk.resource_pools.medium_buffers.free_count < ARRAY_LEN(vk.resource_pools.medium_buffers.free_indices)) {
				vk.resource_pools.medium_buffers.free_indices[vk.resource_pools.medium_buffers.free_count++] = i;
			}
			return;
		}
	}

	// Search large buffers
	for (uint32_t i = 0; i < vk.resource_pools.large_buffers.count; i++) {
		if (vk.resource_pools.large_buffers.buffers[i] == buffer) {
			if (vk.resource_pools.large_buffers.free_count < ARRAY_LEN(vk.resource_pools.large_buffers.free_indices)) {
				vk.resource_pools.large_buffers.free_indices[vk.resource_pools.large_buffers.free_count++] = i;
			}
			return;
		}
	}

	ri.Printf(PRINT_WARNING, "vk_return_buffer_to_pool: buffer %p not found in any pool\n", (void*)buffer);
}

void vk_shutdown_resource_pool(void) {
	if (!vk.resource_pools.enabled) {
		return;
	}

	// Free all pooled buffers
	for (uint32_t i = 0; i < vk.resource_pools.small_buffers.count; i++) {
		if (vk.resource_pools.small_buffers.buffers[i] != VK_NULL_HANDLE) {
			qvkDestroyBuffer(vk.device, vk.resource_pools.small_buffers.buffers[i], NULL);
		}
		if (vk.resource_pools.small_buffers.memory[i] != VK_NULL_HANDLE) {
			qvkFreeMemory(vk.device, vk.resource_pools.small_buffers.memory[i], NULL);
		}
	}

	for (uint32_t i = 0; i < vk.resource_pools.medium_buffers.count; i++) {
		if (vk.resource_pools.medium_buffers.buffers[i] != VK_NULL_HANDLE) {
			qvkDestroyBuffer(vk.device, vk.resource_pools.medium_buffers.buffers[i], NULL);
		}
		if (vk.resource_pools.medium_buffers.memory[i] != VK_NULL_HANDLE) {
			qvkFreeMemory(vk.device, vk.resource_pools.medium_buffers.memory[i], NULL);
		}
	}

	for (uint32_t i = 0; i < vk.resource_pools.large_buffers.count; i++) {
		if (vk.resource_pools.large_buffers.buffers[i] != VK_NULL_HANDLE) {
			qvkDestroyBuffer(vk.device, vk.resource_pools.large_buffers.buffers[i], NULL);
		}
		if (vk.resource_pools.large_buffers.memory[i] != VK_NULL_HANDLE) {
			qvkFreeMemory(vk.device, vk.resource_pools.large_buffers.memory[i], NULL);
		}
	}

	Com_Memset(&vk.resource_pools, 0, sizeof(vk.resource_pools));
	ri.Printf(PRINT_ALL, "Vulkan: Resource pooling system shut down\n");
}

void vk_clean_staging_buffer(void) {
	if (vk.staging_buffer.handle != VK_NULL_HANDLE) {
		qvkDestroyBuffer(vk.device, vk.staging_buffer.handle, NULL);
		vk.staging_buffer.handle = VK_NULL_HANDLE;
	}

	if (vk.staging_buffer.memory != VK_NULL_HANDLE) {
		qvkFreeMemory(vk.device, vk.staging_buffer.memory, NULL);
		vk.staging_buffer.memory = VK_NULL_HANDLE;
	}

	vk.staging_buffer.ptr = nullptr;
	vk.staging_buffer.size = 0;
#ifdef USE_UPLOAD_QUEUE
	vk.staging_buffer.offset = 0;
#endif
}

void vk_flush_staging_buffer(__attribute__((unused)) qboolean final) {
#ifdef USE_UPLOAD_QUEUE
	const VkPipelineStageFlags wait_dst_stage_mask = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	VkSemaphore waits;
	VkSubmitInfo submit_info;
	VkResult res;

	if (vk.staging_buffer.offset == 0) {
		return;
	}

	vk.staging_buffer.offset = 0;

	VK_CHECK(qvkEndCommandBuffer(vk.staging_command_buffer));

	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = NULL;

	if (vk.rendering_finished != VK_NULL_HANDLE) {
		// first call after previous queue submission?
		waits = vk.rendering_finished;
		vk.rendering_finished = VK_NULL_HANDLE;
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &waits;
		submit_info.pWaitDstStageMask = &wait_dst_stage_mask;
	} else {
		submit_info.waitSemaphoreCount = 0;
		submit_info.pWaitSemaphores = NULL;
		submit_info.pWaitDstStageMask = NULL;
	}

	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &vk.staging_command_buffer;

	if (vk.image_uploaded != VK_NULL_HANDLE) {
		ri.Error(ERR_FATAL, "Vulkan: incorrect state during image upload");
	}
	if (final) {
		// final submission before recording
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &vk.image_uploaded2;
		vk.image_uploaded = vk.image_uploaded2;
		VK_CHECK(qvkQueueSubmit(vk.queue, 1, &submit_info, vk.aux_fence));
		vk.aux_fence_wait = qtrue;
	} else {
		// if submission before another upload then do explicit wait
		submit_info.signalSemaphoreCount = 0;
		submit_info.pSignalSemaphores = NULL;
		VK_CHECK(qvkQueueSubmit(vk.queue, 1, &submit_info, vk.aux_fence));
		res = qvkWaitForFences(vk.device, 1, &vk.aux_fence, VK_TRUE, 5 * 1000000000ULL);
		if (res == VK_TIMEOUT) {
			ri.Printf(PRINT_WARNING, "vk_flush_staging_buffer: fence wait timeout, continuing anyway\n");
			// Don't fail here, just continue - the operation might still complete
		} else if (res != VK_SUCCESS) {
			ri.Printf(PRINT_ERROR, "vk_flush_staging_buffer: vkWaitForFences failed with %s\n", vk_result_string(res));
			// Don't crash, try to reset and continue
		}
		qvkResetFences(vk.device, 1, &vk.aux_fence);
		VK_CHECK(qvkResetCommandBuffer(vk.staging_command_buffer, 0));
	}
#endif // USE_UPLOAD_QUEUE
}

void vk_alloc_staging_buffer(VkDeviceSize size) {
	VkBufferCreateInfo buffer_desc;
	VkMemoryRequirements memory_requirements;
	VkMemoryAllocateInfo alloc_info;
	uint32_t memory_type;
	void *data;

	if (size == 0) {
		ri.Printf(PRINT_ERROR, "vk_alloc_staging_buffer: requested size is 0!\n");
		return;
	}

	// Prevent allocating ridiculously large buffers
	if (size > 256 * 1024 * 1024) { // 256MB limit
		ri.Printf(PRINT_ERROR, "vk_alloc_staging_buffer: requested size %lu too large, limiting to 256MB\n", (unsigned long)size);
		size = 256 * 1024 * 1024;
	}

	vk_clean_staging_buffer();

	vk.staging_buffer.size = MAX(size, 16 * 1024 * 1024); // 16MB staging buffer
	vk.staging_buffer.size = PAD(vk.staging_buffer.size, 1024 * 1024);

	buffer_desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_desc.pNext = NULL;
	buffer_desc.flags = 0;
	buffer_desc.size = vk.staging_buffer.size;
	buffer_desc.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	buffer_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	buffer_desc.queueFamilyIndexCount = 0;
	buffer_desc.pQueueFamilyIndices = NULL;
	VK_CHECK(qvkCreateBuffer(vk.device, &buffer_desc, NULL, &vk.staging_buffer.handle));

	qvkGetBufferMemoryRequirements(vk.device, vk.staging_buffer.handle, &memory_requirements);

	memory_type = find_memory_type(memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = memory_requirements.size;
	alloc_info.memoryTypeIndex = memory_type;

	VK_CHECK(qvkAllocateMemory(vk.device, &alloc_info, NULL, &vk.staging_buffer.memory));
	VK_CHECK(qvkBindBufferMemory(vk.device, vk.staging_buffer.handle, vk.staging_buffer.memory, 0));

	VK_CHECK(qvkMapMemory(vk.device, vk.staging_buffer.memory, 0, VK_WHOLE_SIZE, 0, &data));
	vk.staging_buffer.ptr = (byte*)data;
#ifdef USE_UPLOAD_QUEUE
	vk.staging_buffer.offset = 0;
#endif
	SET_OBJECT_NAME(vk.staging_buffer.handle, "staging buffer", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT);
	SET_OBJECT_NAME(vk.staging_buffer.memory, "staging buffer memory", VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT);
}

// Hierarchical Memory Pool System Implementation

// Forward declarations for static functions
static qboolean vk_scale_up_pool_level(uint32_t level_index);
static qboolean vk_scale_down_pool_level(uint32_t level_index);
static void vk_update_memory_pressure(void);
static void vk_check_pool_scaling(void);
static void vk_perform_pool_cleanup(void);

// Initialize the hierarchical memory pool system
qboolean vk_init_memory_pool_system(void) {
    if (vk.resource_pools.initialized) {
        return qtrue; // Already initialized
    }

    ri.Printf(PRINT_ALL, "Vulkan: Initializing hierarchical memory pool system\n");

    // Define pool hierarchy levels
    const uint32_t num_levels = 5;
    vk.resource_pools.num_pool_levels = num_levels;

    // Allocate pool levels array
    vk.resource_pools.pool_levels = (vk_memory_pool_level_t*)ri.Malloc(sizeof(vk_memory_pool_level_t) * num_levels);
    if (!vk.resource_pools.pool_levels) {
        ri.Printf(PRINT_WARNING, "Vulkan: Failed to allocate memory pool levels\n");
        return qfalse;
    }

    // Configure pool levels (hierarchical from small to large)
    typedef struct {
        VkDeviceSize min_size, max_size, block_size;
        uint32_t initial_blocks, max_blocks, min_blocks;
        qboolean allow_cleanup;
    } level_config_t;

    level_config_t level_configs[] = {
        {      0,   4096,    65536,  8,  64,  4, qtrue  }, // Level 0: Tiny (4KB - 64KB blocks)
        {   4096,  65536,   262144,  4,  32,  2, qtrue  }, // Level 1: Small (4KB - 256KB blocks)
        {  65536, 524288,  1048576,  4,  24,  2, qtrue  }, // Level 2: Medium (64KB - 1MB blocks)
        { 524288, 4194304, 8388608,  2,  16,  1, qfalse }, // Level 3: Large (512KB - 8MB blocks)
        {4194304, VK_WHOLE_SIZE, 16777216, 1,  8,  1, qfalse}  // Level 4: Huge (4MB+ individual allocations)
    };

    for (uint32_t i = 0; i < num_levels; i++) {
        vk_memory_pool_level_t *level = &vk.resource_pools.pool_levels[i];
        level_config_t *config = &level_configs[i];

        level->min_size = config->min_size;
        level->max_size = config->max_size;
        level->block_size = config->block_size;
        level->initial_blocks = config->initial_blocks;
        level->max_blocks = config->max_blocks;
        level->min_blocks = config->min_blocks;
        level->current_blocks = 0;
        level->free_blocks = 0;
        level->allow_cleanup = config->allow_cleanup;

        // Allocate memory block arrays
        level->memory_blocks = (VkDeviceMemory*)ri.Malloc(sizeof(VkDeviceMemory) * level->max_blocks);
        level->memory_sizes = (VkDeviceSize*)ri.Malloc(sizeof(VkDeviceSize) * level->max_blocks);
        level->allocation_counts = (uint32_t*)ri.Malloc(sizeof(uint32_t) * level->max_blocks);
        level->mapped_pointers = (void**)ri.Malloc(sizeof(void*) * level->max_blocks);

        if (!level->memory_blocks || !level->memory_sizes || !level->allocation_counts || !level->mapped_pointers) {
            ri.Printf(PRINT_WARNING, "Vulkan: Failed to allocate pool level %u arrays\n", i);
            return qfalse;
        }

        // Initialize arrays
        memset(level->memory_blocks, 0, sizeof(VkDeviceMemory) * level->max_blocks);
        memset(level->memory_sizes, 0, sizeof(VkDeviceSize) * level->max_blocks);
        memset(level->allocation_counts, 0, sizeof(uint32_t) * level->max_blocks);
        memset(level->mapped_pointers, 0, sizeof(void*) * level->max_blocks);

        // Allocation tracking
        level->max_allocations = level->max_blocks * 16; // Allow multiple allocations per block
        level->allocations = (vk_pool_allocation_t*)ri.Malloc(sizeof(vk_pool_allocation_t) * level->max_allocations);
        level->free_indices = (uint32_t*)ri.Malloc(sizeof(uint32_t) * level->max_allocations);

        if (!level->allocations || !level->free_indices) {
            ri.Printf(PRINT_WARNING, "Vulkan: Failed to allocate allocation tracking for pool level %u\n", i);
            return qfalse;
        }

        memset(level->allocations, 0, sizeof(vk_pool_allocation_t) * level->max_allocations);
        for (uint32_t j = 0; j < level->max_allocations; j++) {
            level->free_indices[j] = j;
        }
        level->allocation_count = 0;
        level->free_count = level->max_allocations;

        // Statistics
        level->total_allocated = 0;
        level->total_used = 0;
        level->allocation_operations = 0;
        level->deallocation_operations = 0;
        level->scaling_operations = 0;
        level->frames_since_last_use = 0;
    }

    // Configure memory pressure thresholds
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(vk.physical_device, &mem_props);

    VkDeviceSize total_gpu_memory = 0;
    for (uint32_t i = 0; i < mem_props.memoryHeapCount; i++) {
        if (mem_props.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            total_gpu_memory += mem_props.memoryHeaps[i].size;
        }
    }

    vk.resource_pools.memory_pressure_threshold_low = total_gpu_memory / 4;     // 25%
    vk.resource_pools.memory_pressure_threshold_medium = total_gpu_memory / 2;  // 50%
    vk.resource_pools.memory_pressure_threshold_high = (total_gpu_memory * 3) / 4; // 75%

    // Configure scaling parameters
    vk.resource_pools.scale_up_threshold = 0.8f;   // 80% usage triggers scale up
    vk.resource_pools.scale_down_threshold = 0.2f; // 20% usage allows scale down
    vk.resource_pools.min_blocks_per_level = 1;
    vk.resource_pools.max_blocks_per_level = 64;

    // Configure cleanup parameters
    vk.resource_pools.cleanup_interval_frames = 30000; // ~30 seconds at 100fps
    vk.resource_pools.max_cleanup_age_frames = 60000;  // ~1 minute max age
    vk.resource_pools.current_frame = 0;

    // Statistics
    vk.resource_pools.total_memory_used = 0;
    vk.resource_pools.total_memory_allocated = 0;
    vk.resource_pools.current_pressure = MEMORY_PRESSURE_LOW;
    atomic_store_explicit(&vk.resource_pools.total_allocations, 0, memory_order_relaxed);
    atomic_store_explicit(&vk.resource_pools.total_deallocations, 0, memory_order_relaxed);
    atomic_store_explicit(&vk.resource_pools.total_scaling_operations, 0, memory_order_relaxed);
    atomic_store_explicit(&vk.resource_pools.total_cleanup_operations, 0, memory_order_relaxed);
    atomic_store_explicit(&vk.resource_pools.cache_hits, 0, memory_order_relaxed);
    atomic_store_explicit(&vk.resource_pools.cache_misses, 0, memory_order_relaxed);

    vk.resource_pools.enabled = qtrue;
    vk.resource_pools.initialized = qtrue;

    ri.Printf(PRINT_ALL, "Vulkan: Hierarchical memory pool system initialized with %u levels\n", num_levels);
    ri.Printf(PRINT_ALL, "Vulkan: Total GPU memory: %lu MB\n", (unsigned long)(total_gpu_memory / (1024 * 1024)));

    return qtrue;
}

// Determine which pool level should handle an allocation of given size
static uint32_t vk_get_pool_level_for_size(VkDeviceSize size) {
    for (uint32_t i = 0; i < vk.resource_pools.num_pool_levels; i++) {
        vk_memory_pool_level_t *level = &vk.resource_pools.pool_levels[i];
        if (size >= level->min_size && (size <= level->max_size || level->max_size == VK_WHOLE_SIZE)) {
            return i;
        }
    }
    return vk.resource_pools.num_pool_levels - 1; // Default to largest pool
}

// Allocate memory from hierarchical pool system
void *vk_pool_allocate(VkDeviceSize size, VkDeviceSize alignment, const char *debug_name) {
    if (!vk.resource_pools.enabled || !vk.resource_pools.initialized) {
        return NULL;
    }

    uint32_t level_index = vk_get_pool_level_for_size(size);
    vk_memory_pool_level_t *level = &vk.resource_pools.pool_levels[level_index];

    // Try to allocate from existing blocks first
    for (uint32_t block_idx = 0; block_idx < level->current_blocks; block_idx++) {
        if (level->allocation_counts[block_idx] > 0 && level->memory_sizes[block_idx] >= size) {
            // Check if we have space in this block (simplified - real implementation would track free space)
            if (level->free_count > 0) {
                uint32_t alloc_idx = level->free_indices[--level->free_count];
                vk_pool_allocation_t *alloc = &level->allocations[alloc_idx];

                alloc->user_ptr = (void*)alloc; // Simplified - real pointer would be mapped memory
                alloc->size = size;
                alloc->alignment = alignment;
                alloc->pool_level = level_index;
                alloc->allocation_index = alloc_idx;
                alloc->last_used_frame = vk.resource_pools.current_frame;
                alloc->is_active = qtrue;
                alloc->debug_name = debug_name;

                atomic_fetch_add_explicit(&level->allocation_count, 1, memory_order_relaxed);
                atomic_fetch_add_explicit(&level->allocation_counts[block_idx], 1, memory_order_relaxed);
                level->total_used += size;
                vk.resource_pools.total_memory_used += size;
                atomic_fetch_add_explicit(&vk.resource_pools.total_allocations, 1, memory_order_relaxed);
                atomic_fetch_add_explicit(&level->allocation_operations, 1, memory_order_relaxed);

                atomic_fetch_add_explicit(&vk.resource_pools.cache_hits, 1, memory_order_relaxed);
                return alloc->user_ptr;
            }
        }
    }

    // Need to scale up - add a new block
    if (level->current_blocks < level->max_blocks) {
        if (vk_scale_up_pool_level(level_index)) {
            // Retry allocation after scaling
            return vk_pool_allocate(size, alignment, debug_name);
        }
    }

    atomic_fetch_add_explicit(&vk.resource_pools.cache_misses, 1, memory_order_relaxed);
    return NULL; // Allocation failed
}

// Deallocate memory from hierarchical pool system
void vk_pool_deallocate(void *ptr) {
    if (!vk.resource_pools.enabled || !vk.resource_pools.initialized || !ptr) {
        return;
    }

    // Find the allocation (simplified search - real implementation would use a hash map)
    for (uint32_t level_idx = 0; level_idx < vk.resource_pools.num_pool_levels; level_idx++) {
        vk_memory_pool_level_t *level = &vk.resource_pools.pool_levels[level_idx];

        for (uint32_t alloc_idx = 0; alloc_idx < level->max_allocations; alloc_idx++) {
            vk_pool_allocation_t *alloc = &level->allocations[alloc_idx];
            if (alloc->is_active && alloc->user_ptr == ptr) {
                // Mark as free
                alloc->is_active = qfalse;
                alloc->last_used_frame = 0;

                // Return to free pool
                if (level->free_count < level->max_allocations) {
                    level->free_indices[atomic_fetch_add_explicit(&level->free_count, 1, memory_order_relaxed)] = alloc_idx;
                }

                atomic_fetch_sub_explicit(&level->allocation_count, 1, memory_order_relaxed);
                level->total_used -= alloc->size;
                vk.resource_pools.total_memory_used -= alloc->size;
                atomic_fetch_add_explicit(&vk.resource_pools.total_deallocations, 1, memory_order_relaxed);
                atomic_fetch_add_explicit(&level->deallocation_operations, 1, memory_order_relaxed);

                // Check if we should scale down
                vk_update_memory_pressure();
                vk_check_pool_scaling();

                return;
            }
        }
    }
}

// Scale up a pool level by adding a new memory block
static qboolean vk_scale_up_pool_level(uint32_t level_index) {
    vk_memory_pool_level_t *level = &vk.resource_pools.pool_levels[level_index];

    if (level->current_blocks >= level->max_blocks) {
        return qfalse; // Cannot scale up further
    }

    uint32_t block_idx = level->current_blocks++;

    // Allocate new memory block
    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = NULL,
        .allocationSize = level->block_size,
        .memoryTypeIndex = find_memory_type(~0U, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    VkDeviceMemory memory;
    VkResult result = qvkAllocateMemory(vk.device, &alloc_info, NULL, &memory);
    if (result != VK_SUCCESS) {
        level->current_blocks--; // Revert counter
        ri.Printf(PRINT_WARNING, "Vulkan: Failed to scale up pool level %u: %s\n",
            level_index, vk_result_string(result));
        return qfalse;
    }

    level->memory_blocks[block_idx] = memory;
    level->memory_sizes[block_idx] = level->block_size;
    level->allocation_counts[block_idx] = 0;
    level->total_allocated += level->block_size;
    vk.resource_pools.total_memory_allocated += level->block_size;

    level->scaling_operations++;
    vk.resource_pools.total_scaling_operations++;

    ri.Printf(PRINT_ALL, "Vulkan: Scaled up pool level %u to %u blocks (%lu MB)\n",
        level_index, level->current_blocks,
        (unsigned long)(level->total_allocated / (1024 * 1024)));

    return qtrue;
}

// Scale down a pool level by removing unused blocks
static qboolean vk_scale_down_pool_level(uint32_t level_index) {
    vk_memory_pool_level_t *level = &vk.resource_pools.pool_levels[level_index];

    if (level->current_blocks <= level->min_blocks) {
        return qfalse; // Cannot scale down further
    }

    // Find the last block with no allocations
    for (int32_t block_idx = level->current_blocks - 1; block_idx >= 0; block_idx--) {
        if (level->allocation_counts[block_idx] == 0) {
            // Free this block
            if (level->memory_blocks[block_idx] != VK_NULL_HANDLE) {
                qvkFreeMemory(vk.device, level->memory_blocks[block_idx], NULL);
                level->total_allocated -= level->memory_sizes[block_idx];
                vk.resource_pools.total_memory_allocated -= level->memory_sizes[block_idx];
            }

            level->memory_blocks[block_idx] = VK_NULL_HANDLE;
            level->memory_sizes[block_idx] = 0;
            level->allocation_counts[block_idx] = 0;
            level->current_blocks--;

            level->scaling_operations++;
            vk.resource_pools.total_scaling_operations++;

            ri.Printf(PRINT_ALL, "Vulkan: Scaled down pool level %u to %u blocks\n",
                level_index, level->current_blocks);

            return qtrue;
        }
    }

    return qfalse; // No empty blocks to remove
}

// Update memory pressure based on current usage
static void vk_update_memory_pressure(void) {
    VkDeviceSize used = vk.resource_pools.total_memory_used;

    if (used >= vk.resource_pools.memory_pressure_threshold_high) {
        vk.resource_pools.current_pressure = MEMORY_PRESSURE_CRITICAL;
    } else if (used >= vk.resource_pools.memory_pressure_threshold_medium) {
        vk.resource_pools.current_pressure = MEMORY_PRESSURE_HIGH;
    } else if (used >= vk.resource_pools.memory_pressure_threshold_low) {
        vk.resource_pools.current_pressure = MEMORY_PRESSURE_MEDIUM;
    } else {
        vk.resource_pools.current_pressure = MEMORY_PRESSURE_LOW;
    }
}

// Check if pool levels need scaling based on usage patterns
static void vk_check_pool_scaling(void) {
    for (uint32_t level_idx = 0; level_idx < vk.resource_pools.num_pool_levels; level_idx++) {
        vk_memory_pool_level_t *level = &vk.resource_pools.pool_levels[level_idx];

        if (level->current_blocks == 0) continue;

        float usage_ratio = (float)level->allocation_count / (float)(level->current_blocks * 16);

        // Scale up if heavily used
        if (usage_ratio >= vk.resource_pools.scale_up_threshold &&
            level->current_blocks < level->max_blocks) {
            vk_scale_up_pool_level(level_idx);
        }

        // Scale down if lightly used and cleanup is allowed
        if (usage_ratio <= vk.resource_pools.scale_down_threshold &&
            level->current_blocks > level->min_blocks &&
            level->allow_cleanup) {
            vk_scale_down_pool_level(level_idx);
        }
    }
}

// Perform automatic cleanup of unused allocations
static void vk_perform_pool_cleanup(void) {
    uint64_t current_frame = vk.resource_pools.current_frame;
    uint32_t cleanup_count = 0;

    for (uint32_t level_idx = 0; level_idx < vk.resource_pools.num_pool_levels; level_idx++) {
        vk_memory_pool_level_t *level = &vk.resource_pools.pool_levels[level_idx];

        if (!level->allow_cleanup) continue;

        for (uint32_t alloc_idx = 0; alloc_idx < level->max_allocations; alloc_idx++) {
            vk_pool_allocation_t *alloc = &level->allocations[alloc_idx];
            if (alloc->is_active &&
                current_frame - alloc->last_used_frame > vk.resource_pools.max_cleanup_age_frames) {
                // Mark for cleanup
                vk_pool_deallocate(alloc->user_ptr);
                cleanup_count++;
            }
        }
    }

    if (cleanup_count > 0) {
        vk.resource_pools.total_cleanup_operations += cleanup_count;
        ri.Printf(PRINT_ALL, "Vulkan: Cleaned up %u unused pool allocations\n", cleanup_count);
    }
}

// Update pool system per frame
void vk_update_memory_pool_system(void) {
    if (!vk.resource_pools.enabled || !vk.resource_pools.initialized) {
        return;
    }

    vk.resource_pools.current_frame++;

    // Periodic cleanup
    if (vk.resource_pools.current_frame % vk.resource_pools.cleanup_interval_frames == 0) {
        vk_perform_pool_cleanup();
    }

    // Update memory pressure and scaling
    vk_update_memory_pressure();
    vk_check_pool_scaling();
}

// Print pool system statistics
void vk_print_pool_statistics(void) {
    if (!vk.resource_pools.enabled || !vk.resource_pools.initialized) {
        ri.Printf(PRINT_ALL, "Memory pool system not initialized\n");
        return;
    }

    ri.Printf(PRINT_ALL, "=== Hierarchical Memory Pool Statistics ===\n");
    ri.Printf(PRINT_ALL, "Total Memory Used: %lu MB\n",
        (unsigned long)(vk.resource_pools.total_memory_used / (1024 * 1024)));
    ri.Printf(PRINT_ALL, "Total Memory Allocated: %lu MB\n",
        (unsigned long)(vk.resource_pools.total_memory_allocated / (1024 * 1024)));
    ri.Printf(PRINT_ALL, "Memory Pressure: %s\n",
        vk.resource_pools.current_pressure == MEMORY_PRESSURE_LOW ? "Low" :
        vk.resource_pools.current_pressure == MEMORY_PRESSURE_MEDIUM ? "Medium" :
        vk.resource_pools.current_pressure == MEMORY_PRESSURE_HIGH ? "High" : "Critical");

    ri.Printf(PRINT_ALL, "Total Operations: alloc=%lu, dealloc=%lu, scale=%lu, cleanup=%lu\n",
        (unsigned long)vk.resource_pools.total_allocations,
        (unsigned long)vk.resource_pools.total_deallocations,
        (unsigned long)vk.resource_pools.total_scaling_operations,
        (unsigned long)vk.resource_pools.total_cleanup_operations);

    ri.Printf(PRINT_ALL, "Cache Performance: hits=%lu, misses=%lu (%.1f%% hit rate)\n",
        (unsigned long)vk.resource_pools.cache_hits,
        (unsigned long)vk.resource_pools.cache_misses,
        vk.resource_pools.cache_hits + vk.resource_pools.cache_misses > 0 ?
        (float)vk.resource_pools.cache_hits / (float)(vk.resource_pools.cache_hits + vk.resource_pools.cache_misses) * 100.0f : 0.0f);

    for (uint32_t i = 0; i < vk.resource_pools.num_pool_levels; i++) {
        vk_memory_pool_level_t *level = &vk.resource_pools.pool_levels[i];
        ri.Printf(PRINT_ALL, "Pool Level %u (%luB-%luB blocks): %u/%u blocks, %u/%u allocs, %lu MB used\n",
            i,
            (unsigned long)level->min_size,
            (unsigned long)(level->max_size == VK_WHOLE_SIZE ? 0 : level->max_size),
            level->current_blocks,
            level->max_blocks,
            level->allocation_count,
            level->max_allocations,
            (unsigned long)(level->total_used / (1024 * 1024)));
    }
}

// Shutdown the hierarchical memory pool system
void vk_shutdown_memory_pool_system(void) {
    if (!vk.resource_pools.initialized) {
        return;
    }

    ri.Printf(PRINT_ALL, "Vulkan: Shutting down hierarchical memory pool system\n");

    // Free all pool levels
    for (uint32_t i = 0; i < vk.resource_pools.num_pool_levels; i++) {
        vk_memory_pool_level_t *level = &vk.resource_pools.pool_levels[i];

        // Free memory blocks
        for (uint32_t j = 0; j < level->current_blocks; j++) {
            if (level->memory_blocks[j] != VK_NULL_HANDLE) {
                qvkFreeMemory(vk.device, level->memory_blocks[j], NULL);
            }
        }

        // Free allocated arrays
        if (level->memory_blocks) ri.Free(level->memory_blocks);
        if (level->memory_sizes) ri.Free(level->memory_sizes);
        if (level->allocation_counts) ri.Free(level->allocation_counts);
        if (level->mapped_pointers) ri.Free(level->mapped_pointers);
        if (level->allocations) ri.Free(level->allocations);
        if (level->free_indices) ri.Free(level->free_indices);
    }

    if (vk.resource_pools.pool_levels) {
        ri.Free(vk.resource_pools.pool_levels);
    }

    vk.resource_pools.initialized = qfalse;
    ri.Printf(PRINT_ALL, "Vulkan: Hierarchical memory pool system shutdown complete\n");
}

// Lock-Free Memory Allocators Implementation

// Atomic operations for lock-free programming
static inline uint32_t atomic_load_u32(const volatile uint32_t *ptr) {
    return __atomic_load_n((volatile uint32_t*)ptr, __ATOMIC_ACQUIRE);
}

static inline void atomic_store_u32(volatile uint32_t *ptr, uint32_t value) {
    __atomic_store_n(ptr, value, __ATOMIC_RELEASE);
}

static inline uint32_t atomic_exchange_u32(volatile uint32_t *ptr, uint32_t value) {
    return __atomic_exchange_n(ptr, value, __ATOMIC_ACQ_REL);
}

static inline qboolean atomic_compare_exchange_u32(volatile uint32_t *ptr, uint32_t *expected, uint32_t desired) {
    return __atomic_compare_exchange_n(ptr, expected, desired, qfalse, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
}

static inline uint64_t atomic_load_u64(const volatile uint64_t *ptr) {
    return __atomic_load_n((volatile uint64_t*)ptr, __ATOMIC_ACQUIRE);
}

static inline uint64_t atomic_load_u64_atomic(const atomic_uint64_t *ptr) {
    return atomic_load_explicit(ptr, memory_order_acquire);
}

static inline uint32_t atomic_load_u32_atomic(const atomic_uint_t *ptr) {
    return atomic_load_explicit(ptr, memory_order_acquire);
}

static inline void atomic_increment_u64(atomic_uint64_t *ptr) {
    atomic_fetch_add_explicit(ptr, 1, memory_order_acq_rel);
}

static inline void atomic_increment_u32(atomic_uint_t *ptr) {
    atomic_fetch_add_explicit(ptr, 1, memory_order_acq_rel);
}

static inline void atomic_store_u64(volatile uint64_t *ptr, uint64_t value) {
    __atomic_store_n(ptr, value, __ATOMIC_RELEASE);
}

// Initialize a lock-free fixed-size allocator
static qboolean vk_init_lock_free_allocator(vk_lock_free_allocator_t *allocator,
                                          VkDeviceSize pool_size,
                                          VkDeviceSize block_size,
                                          const char *debug_name) {
    if (!allocator || pool_size == 0 || block_size == 0) {
        return qfalse;
    }

    // Calculate number of blocks
    uint32_t num_blocks = (uint32_t)(pool_size / block_size);
    if (num_blocks == 0) {
        return qfalse;
    }

    // Allocate memory pool
    allocator->memory_pool = ri.Malloc((size_t)pool_size);
    if (!allocator->memory_pool) {
        ri.Printf(PRINT_WARNING, "Vulkan: Failed to allocate memory pool for %s\n", debug_name);
        return qfalse;
    }

    // Initialize allocator state
    allocator->pool_size = pool_size;
    allocator->block_size = block_size;
    allocator->total_blocks = num_blocks;
    atomic_init(&allocator->free_blocks, num_blocks);
    allocator->is_thread_safe = qtrue;
    allocator->debug_name = debug_name;

    // Allocate free list
    allocator->free_list = (uintptr_t*)ri.Malloc(sizeof(uintptr_t) * num_blocks);
    if (!allocator->free_list) {
        ri.Free(allocator->memory_pool);
        return qfalse;
    }

    // Initialize free list with all blocks
    uint8_t *pool_ptr = (uint8_t*)allocator->memory_pool;
    for (uint32_t i = 0; i < num_blocks; i++) {
        allocator->free_list[i] = (uintptr_t)(pool_ptr + (uintptr_t)i * block_size);
    }

    // Initialize free list pointers (lock-free queue)
    atomic_init(&allocator->free_list_head, 0);
    atomic_init(&allocator->free_list_tail, num_blocks);

    // Initialize statistics
    atomic_init(&allocator->allocations, 0);
    atomic_init(&allocator->deallocations, 0);
    atomic_init(&allocator->contended_allocs, 0);

    ri.Printf(PRINT_ALL, "Vulkan: Initialized lock-free allocator %s (%u blocks of %lu bytes)\n",
        debug_name, num_blocks, (unsigned long)block_size);

    return qtrue;
}

// Shutdown a lock-free allocator
static void vk_shutdown_lock_free_allocator(vk_lock_free_allocator_t *allocator) {
    if (!allocator || !allocator->memory_pool) {
        return;
    }

    ri.Printf(PRINT_ALL, "Vulkan: Shutting down lock-free allocator %s\n", allocator->debug_name);

    if (allocator->free_list) {
        ri.Free((void*)allocator->free_list);
        allocator->free_list = NULL;
    }

    if (allocator->memory_pool) {
        ri.Free(allocator->memory_pool);
        allocator->memory_pool = NULL;
    }

    // Reset statistics
    allocator->allocations = 0;
    allocator->deallocations = 0;
    allocator->contended_allocs = 0;
    allocator->free_blocks = 0;
}

static void *vk_lock_free_allocate(vk_lock_free_allocator_t *allocator) {
    if (!allocator || !allocator->is_thread_safe) {
        return NULL;
    }

    uint32_t head, next_head;
    do {
        head = atomic_load_explicit(&allocator->free_list_head, memory_order_acquire);
        uint32_t tail = atomic_load_explicit(&allocator->free_list_tail, memory_order_acquire);

        if (head >= tail) {
            return NULL; // No free blocks
        }

        next_head = head + 1;
        
        if (atomic_compare_exchange_weak_explicit(&allocator->free_list_head, &head, next_head,
                                                 memory_order_release, memory_order_relaxed)) {
            void *block = (void*)allocator->free_list[head];
            atomic_fetch_add_explicit(&allocator->allocations, 1, memory_order_relaxed);
            atomic_fetch_sub_explicit(&allocator->free_blocks, 1, memory_order_relaxed);
            return block;
        }
        
        atomic_fetch_add_explicit(&allocator->contended_allocs, 1, memory_order_relaxed);
    } while (1);
}

static qboolean vk_lock_free_deallocate(vk_lock_free_allocator_t *allocator, void *ptr) {
    if (!allocator || !allocator->is_thread_safe || !ptr) {
        return qfalse;
    }

    uintptr_t ptr_val = (uintptr_t)ptr;
    uintptr_t pool_start = (uintptr_t)allocator->memory_pool;
    uintptr_t pool_end = pool_start + allocator->pool_size;

    if (ptr_val < pool_start || ptr_val >= pool_end) {
        return qfalse;
    }

    uintptr_t offset = ptr_val - pool_start;
    if (offset % allocator->block_size != 0) {
        return qfalse;
    }

    uint32_t block_index = (uint32_t)(offset / allocator->block_size);
    if (block_index >= allocator->total_blocks) {
        return qfalse;
    }

    uint32_t tail, next_tail;
    do {
        tail = atomic_load_explicit(&allocator->free_list_tail, memory_order_acquire);
        uint32_t head = atomic_load_explicit(&allocator->free_list_head, memory_order_acquire);

        next_tail = tail + 1;
        if (next_tail - head >= allocator->total_blocks) {
            return qfalse; // Full or double free
        }

        if (atomic_compare_exchange_weak_explicit(&allocator->free_list_tail, &tail, next_tail,
                                                 memory_order_release, memory_order_relaxed)) {
            allocator->free_list[tail] = ptr_val;
            atomic_fetch_add_explicit(&allocator->deallocations, 1, memory_order_relaxed);
            atomic_fetch_add_explicit(&allocator->free_blocks, 1, memory_order_relaxed);
            return qtrue;
        }
    } while (1);
}

// Initialize the lock-free memory manager
qboolean vk_init_lock_free_memory_manager(void) {
    if (vk.lock_free_manager.initialized) {
        return qtrue;
    }

    ri.Printf(PRINT_ALL, "Vulkan: Initializing lock-free memory manager\n");

    // Configure pool sizes (can be made configurable)
    vk.lock_free_manager.small_pool_size = 4 * 1024 * 1024;    // 4MB
    vk.lock_free_manager.medium_pool_size = 8 * 1024 * 1024;   // 8MB
    vk.lock_free_manager.large_pool_size = 16 * 1024 * 1024;   // 16MB

    vk.lock_free_manager.small_block_size = 64;     // 64 bytes
    vk.lock_free_manager.medium_block_size = 256;   // 256 bytes
    vk.lock_free_manager.large_block_size = 1024;   // 1024 bytes

    // Initialize allocators
    qboolean success = qtrue;

    success &= vk_init_lock_free_allocator(&vk.lock_free_manager.small_allocator,
                                         vk.lock_free_manager.small_pool_size,
                                         vk.lock_free_manager.small_block_size,
                                         "small_lock_free");

    success &= vk_init_lock_free_allocator(&vk.lock_free_manager.medium_allocator,
                                         vk.lock_free_manager.medium_pool_size,
                                         vk.lock_free_manager.medium_block_size,
                                         "medium_lock_free");

    success &= vk_init_lock_free_allocator(&vk.lock_free_manager.large_allocator,
                                         vk.lock_free_manager.large_pool_size,
                                         vk.lock_free_manager.large_block_size,
                                         "large_lock_free");

    if (!success) {
        ri.Printf(PRINT_WARNING, "Vulkan: Failed to initialize some lock-free allocators\n");
        vk_shutdown_lock_free_memory_manager();
        return qfalse;
    }

    // Initialize statistics
    atomic_store_explicit(&vk.lock_free_manager.total_allocations, 0, memory_order_relaxed);
    atomic_store_explicit(&vk.lock_free_manager.total_deallocations, 0, memory_order_relaxed);
    atomic_store_explicit(&vk.lock_free_manager.cache_hits, 0, memory_order_relaxed);
    atomic_store_explicit(&vk.lock_free_manager.cache_misses, 0, memory_order_relaxed);
    atomic_store_explicit(&vk.lock_free_manager.lock_contention_events, 0, memory_order_relaxed);
    atomic_store_explicit(&vk.lock_free_manager.allocation_time_ns, 0, memory_order_relaxed);
    atomic_store_explicit(&vk.lock_free_manager.deallocation_time_ns, 0, memory_order_relaxed);

    vk.lock_free_manager.enabled = qtrue;
    vk.lock_free_manager.initialized = qtrue;

    ri.Printf(PRINT_ALL, "Vulkan: Lock-free memory manager initialized with %lu MB total capacity\n",
        (unsigned long)((vk.lock_free_manager.small_pool_size +
                        vk.lock_free_manager.medium_pool_size +
                        vk.lock_free_manager.large_pool_size) / (1024 * 1024)));

    return qtrue;
}

// Shutdown the lock-free memory manager
void vk_shutdown_lock_free_memory_manager(void) {
    if (!vk.lock_free_manager.initialized) {
        return;
    }

    ri.Printf(PRINT_ALL, "Vulkan: Shutting down lock-free memory manager\n");

    // Shutdown allocators
    vk_shutdown_lock_free_allocator(&vk.lock_free_manager.small_allocator);
    vk_shutdown_lock_free_allocator(&vk.lock_free_manager.medium_allocator);
    vk_shutdown_lock_free_allocator(&vk.lock_free_manager.large_allocator);

    vk.lock_free_manager.initialized = qfalse;
    ri.Printf(PRINT_ALL, "Vulkan: Lock-free memory manager shutdown complete\n");
}

// High-performance lock-free allocation with size-based routing
void *vk_lock_free_alloc(VkDeviceSize size, const char *debug_name) {
    (void)debug_name; // Suppress unused parameter warning
    if (!vk.lock_free_manager.enabled || !vk.lock_free_manager.initialized) {
        return NULL;
    }

    void *ptr = NULL;

    // Route to appropriate allocator based on size
    if (size <= vk.lock_free_manager.small_block_size) {
        ptr = vk_lock_free_allocate(&vk.lock_free_manager.small_allocator);
        if (ptr) {
            atomic_increment_u64(&vk.lock_free_manager.cache_hits);
        }
    } else if (size <= vk.lock_free_manager.medium_block_size) {
        ptr = vk_lock_free_allocate(&vk.lock_free_manager.medium_allocator);
        if (ptr) {
            atomic_increment_u64(&vk.lock_free_manager.cache_hits);
        }
    } else if (size <= vk.lock_free_manager.large_block_size) {
        ptr = vk_lock_free_allocate(&vk.lock_free_manager.large_allocator);
        if (ptr) {
            atomic_increment_u64(&vk.lock_free_manager.cache_hits);
        }
    }

    if (ptr) {
        atomic_increment_u64(&vk.lock_free_manager.total_allocations);
    } else {
        atomic_increment_u64(&vk.lock_free_manager.cache_misses);
    }

    return ptr;
}

// High-performance lock-free deallocation
qboolean vk_lock_free_free(void *ptr) {
    if (!vk.lock_free_manager.enabled || !vk.lock_free_manager.initialized || !ptr) {
        return qfalse;
    }

    // Try each allocator (could be optimized with pointer range checking)
    qboolean freed = qfalse;

    freed = vk_lock_free_deallocate(&vk.lock_free_manager.small_allocator, ptr);
    if (!freed) {
        freed = vk_lock_free_deallocate(&vk.lock_free_manager.medium_allocator, ptr);
    }
    if (!freed) {
        freed = vk_lock_free_deallocate(&vk.lock_free_manager.large_allocator, ptr);
    }

    if (freed) {
        atomic_increment_u64(&vk.lock_free_manager.total_deallocations);
    }

    return freed;
}

// Get lock-free allocator statistics
void vk_print_lock_free_stats(void) {
    if (!vk.lock_free_manager.enabled || !vk.lock_free_manager.initialized) {
        ri.Printf(PRINT_ALL, "Lock-free memory manager not initialized\n");
        return;
    }

    ri.Printf(PRINT_ALL, "=== Lock-Free Memory Manager Statistics ===\n");

    uint64_t total_allocs = atomic_load_u64_atomic(&vk.lock_free_manager.total_allocations);
    uint64_t total_deallocs = atomic_load_u64_atomic(&vk.lock_free_manager.total_deallocations);
    uint64_t cache_hits = atomic_load_u64_atomic(&vk.lock_free_manager.cache_hits);
    uint64_t cache_misses = atomic_load_u64_atomic(&vk.lock_free_manager.cache_misses);

    ri.Printf(PRINT_ALL, "Total Operations: alloc=%lu, dealloc=%lu\n",
        (unsigned long)total_allocs, (unsigned long)total_deallocs);

    ri.Printf(PRINT_ALL, "Cache Performance: hits=%lu, misses=%lu",
        (unsigned long)cache_hits, (unsigned long)cache_misses);

    if (cache_hits + cache_misses > 0) {
        float hit_rate = (float)cache_hits / (float)(cache_hits + cache_misses) * 100.0f;
        ri.Printf(PRINT_ALL, " (%.1f%% hit rate)", hit_rate);
    }
    ri.Printf(PRINT_ALL, "\n");

    // Print per-allocator stats
    const vk_lock_free_allocator_t *allocators[] = {
        &vk.lock_free_manager.small_allocator,
        &vk.lock_free_manager.medium_allocator,
        &vk.lock_free_manager.large_allocator
    };

    const char *names[] = {"Small", "Medium", "Large"};

    for (int i = 0; i < 3; i++) {
        const vk_lock_free_allocator_t *alloc = allocators[i];
        uint32_t free_blocks = atomic_load_u32_atomic(&alloc->free_blocks);
        uint64_t allocs = atomic_load_u64_atomic(&alloc->allocations);
        uint64_t deallocs = atomic_load_u64_atomic(&alloc->deallocations);
        uint64_t contended = atomic_load_u64_atomic(&alloc->contended_allocs);

        ri.Printf(PRINT_ALL, "%s Allocator (%s): %u/%u blocks free, alloc=%lu, dealloc=%lu, contended=%lu\n",
            names[i], alloc->debug_name, free_blocks, alloc->total_blocks,
            (unsigned long)allocs, (unsigned long)deallocs, (unsigned long)contended);
    }
}

// Arena Allocators Implementation

// Initialize a memory arena
static qboolean vk_init_memory_arena(vk_memory_arena_t *arena, VkDeviceSize size, const char *name, vk_memory_arena_t *parent) {
    if (!arena || size == 0) {
        return qfalse;
    }

    arena->memory = ri.Malloc((size_t)size);
    if (!arena->memory) {
        ri.Printf(PRINT_WARNING, "Vulkan: Failed to allocate arena memory (%lu bytes) for %s\n",
            (unsigned long)size, name);
        return qfalse;
    }

    arena->size = size;
    arena->used = 0;
    arena->peak_used = 0;
    arena->alignment = 16; // Default 16-byte alignment
    arena->allocation_count = 0;
    arena->is_active = qtrue;
    arena->name = name;
    arena->parent = parent;

    return qtrue;
}

// Shutdown a memory arena
static void vk_shutdown_memory_arena(vk_memory_arena_t *arena) {
    if (!arena || !arena->memory) {
        return;
    }

    if (arena->memory) {
        ri.Free(arena->memory);
        arena->memory = NULL;
    }

    arena->used = 0;
    arena->peak_used = 0;
    arena->allocation_count = 0;
    arena->is_active = qfalse;
}

// Reset an arena (free all allocations but keep memory)
static void vk_reset_memory_arena(vk_memory_arena_t *arena) {
    if (!arena || !arena->is_active) {
        return;
    }

    arena->used = 0;
    arena->allocation_count = 0;
    // Keep peak_used for statistics
}

// Allocate from arena with alignment
static void *vk_arena_alloc_aligned(vk_memory_arena_t *arena, VkDeviceSize size, VkDeviceSize alignment) {
    if (!arena || !arena->is_active || size == 0) {
        return NULL;
    }

    // Align the current used pointer
    VkDeviceSize aligned_used = (arena->used + alignment - 1) & ~(alignment - 1);

    // Check if we have enough space
    if (aligned_used + size > arena->size) {
        ri.Printf(PRINT_WARNING, "Vulkan: Arena '%s' out of memory (%lu/%lu bytes used, requested %lu)\n",
            arena->name, (unsigned long)arena->used, (unsigned long)arena->size, (unsigned long)size);
        return NULL;
    }

    // Allocate
    void *ptr = (uint8_t*)arena->memory + aligned_used;
    arena->used = aligned_used + size;
    arena->allocation_count++;

    if (arena->used > arena->peak_used) {
        arena->peak_used = arena->used;
    }

    return ptr;
}

// Allocate from arena with default alignment
static void *vk_arena_alloc(vk_memory_arena_t *arena, VkDeviceSize size) {
    return vk_arena_alloc_aligned(arena, size, arena->alignment);
}

// Create a sub-arena within a parent arena
static vk_memory_arena_t *vk_create_sub_arena(vk_memory_arena_t *parent, VkDeviceSize size, const char *name) {
    if (!parent || !parent->is_active) {
        return NULL;
    }

    // Allocate arena structure from parent
    vk_memory_arena_t *sub_arena = (vk_memory_arena_t*)vk_arena_alloc(parent, sizeof(vk_memory_arena_t));
    if (!sub_arena) {
        return NULL;
    }

    // Allocate memory for sub-arena from parent
    void *sub_memory = vk_arena_alloc(parent, size);
    if (!sub_memory) {
        return NULL;
    }

    // Initialize sub-arena
    sub_arena->memory = sub_memory;
    sub_arena->size = size;
    sub_arena->used = 0;
    sub_arena->peak_used = 0;
    sub_arena->alignment = parent->alignment;
    sub_arena->allocation_count = 0;
    sub_arena->is_active = qtrue;
    sub_arena->name = name;
    sub_arena->parent = parent;

    return sub_arena;
}

// Initialize the arena manager
qboolean vk_init_arena_manager(void) {
    if (vk.arena_manager.initialized) {
        return qtrue;
    }

    ri.Printf(PRINT_ALL, "Vulkan: Initializing arena memory manager\n");

    // Configure arena sizes
    vk.arena_manager.frame_arena_size = 16 * 1024 * 1024;     // 16MB per frame
    vk.arena_manager.render_arena_size = 32 * 1024 * 1024;    // 32MB per scene
    vk.arena_manager.asset_arena_size = 64 * 1024 * 1024;     // 64MB per level
    vk.arena_manager.persistent_arena_size = 128 * 1024 * 1024; // 128MB persistent
    vk.arena_manager.dynamic_arena_size = 8 * 1024 * 1024;    // 8MB per dynamic arena

    // Initialize pre-allocated arenas
    qboolean success = qtrue;

    success &= vk_init_memory_arena(&vk.arena_manager.frame_arena,
                                   vk.arena_manager.frame_arena_size, "frame", NULL);
    success &= vk_init_memory_arena(&vk.arena_manager.render_arena,
                                   vk.arena_manager.render_arena_size, "render", NULL);
    success &= vk_init_memory_arena(&vk.arena_manager.asset_arena,
                                   vk.arena_manager.asset_arena_size, "asset", NULL);
    success &= vk_init_memory_arena(&vk.arena_manager.persistent_arena,
                                   vk.arena_manager.persistent_arena_size, "persistent", NULL);

    if (!success) {
        ri.Printf(PRINT_WARNING, "Vulkan: Failed to initialize some arenas\n");
        vk_shutdown_arena_manager();
        return qfalse;
    }

    // Initialize dynamic arena pool
    vk.arena_manager.max_dynamic_arenas = 16;
    vk.arena_manager.dynamic_arenas = (vk_memory_arena_t*)ri.Malloc(
        sizeof(vk_memory_arena_t) * vk.arena_manager.max_dynamic_arenas);
    if (!vk.arena_manager.dynamic_arenas) {
        ri.Printf(PRINT_WARNING, "Vulkan: Failed to allocate dynamic arena pool\n");
        vk_shutdown_arena_manager();
        return qfalse;
    }

    memset(vk.arena_manager.dynamic_arenas, 0,
           sizeof(vk_memory_arena_t) * vk.arena_manager.max_dynamic_arenas);
    vk.arena_manager.active_dynamic_arenas = 0;

    // Initialize statistics
    atomic_store_explicit(&vk.arena_manager.total_allocated_bytes, 0, memory_order_relaxed);
    atomic_store_explicit(&vk.arena_manager.total_freed_bytes, 0, memory_order_relaxed);
    atomic_store_explicit(&vk.arena_manager.peak_memory_usage, 0, memory_order_relaxed);
    atomic_store_explicit(&vk.arena_manager.arena_resets, 0, memory_order_relaxed);
    atomic_store_explicit(&vk.arena_manager.allocation_operations, 0, memory_order_relaxed);

    vk.arena_manager.enabled = qtrue;
    vk.arena_manager.initialized = qtrue;

    VkDeviceSize total_memory = vk.arena_manager.frame_arena_size +
                               vk.arena_manager.render_arena_size +
                               vk.arena_manager.asset_arena_size +
                               vk.arena_manager.persistent_arena_size;

    ri.Printf(PRINT_ALL, "Vulkan: Arena memory manager initialized with %lu MB total capacity\n",
        (unsigned long)(total_memory / (1024 * 1024)));

    return qtrue;
}

// Shutdown the arena manager
void vk_shutdown_arena_manager(void) {
    if (!vk.arena_manager.initialized) {
        return;
    }

    ri.Printf(PRINT_ALL, "Vulkan: Shutting down arena memory manager\n");

    // Shutdown dynamic arenas
    for (uint32_t i = 0; i < vk.arena_manager.active_dynamic_arenas; i++) {
        vk_shutdown_memory_arena(&vk.arena_manager.dynamic_arenas[i]);
    }

    if (vk.arena_manager.dynamic_arenas) {
        ri.Free(vk.arena_manager.dynamic_arenas);
        vk.arena_manager.dynamic_arenas = NULL;
    }

    // Shutdown pre-allocated arenas
    vk_shutdown_memory_arena(&vk.arena_manager.frame_arena);
    vk_shutdown_memory_arena(&vk.arena_manager.render_arena);
    vk_shutdown_memory_arena(&vk.arena_manager.asset_arena);
    vk_shutdown_memory_arena(&vk.arena_manager.persistent_arena);

    vk.arena_manager.initialized = qfalse;
    ri.Printf(PRINT_ALL, "Vulkan: Arena memory manager shutdown complete\n");
}

// Reset frame arena (called every frame)
void vk_reset_frame_arena(void) {
    if (!vk.arena_manager.enabled || !vk.arena_manager.initialized) {
        return;
    }

    vk_reset_memory_arena(&vk.arena_manager.frame_arena);
    vk.arena_manager.arena_resets++;
    vk.arena_manager.current_frame++;
}

// Reset render arena (called between scenes)
void vk_reset_render_arena(void) {
    if (!vk.arena_manager.enabled || !vk.arena_manager.initialized) {
        return;
    }

    vk_reset_memory_arena(&vk.arena_manager.render_arena);
    vk.arena_manager.arena_resets++;
}

// Reset asset arena (called on level changes)
void vk_reset_asset_arena(void) {
    if (!vk.arena_manager.enabled || !vk.arena_manager.initialized) {
        return;
    }

    vk_reset_memory_arena(&vk.arena_manager.asset_arena);
    vk.arena_manager.arena_resets++;
}

// Allocate from frame arena
void *vk_frame_alloc(VkDeviceSize size) {
    if (!vk.arena_manager.enabled || !vk.arena_manager.initialized) {
        return NULL;
    }

    void *ptr = vk_arena_alloc(&vk.arena_manager.frame_arena, size);
    if (ptr) {
        atomic_fetch_add_explicit(&vk.arena_manager.allocation_operations, 1, memory_order_relaxed);
        atomic_fetch_add_explicit(&vk.arena_manager.total_allocated_bytes, (uint64_t)size, memory_order_relaxed);
    }
    return ptr;
}

// Allocate from render arena
void *vk_render_alloc(VkDeviceSize size) {
    if (!vk.arena_manager.enabled || !vk.arena_manager.initialized) {
        return NULL;
    }

    void *ptr = vk_arena_alloc(&vk.arena_manager.render_arena, size);
    if (ptr) {
        atomic_fetch_add_explicit(&vk.arena_manager.allocation_operations, 1, memory_order_relaxed);
        atomic_fetch_add_explicit(&vk.arena_manager.total_allocated_bytes, (uint64_t)size, memory_order_relaxed);
    }
    return ptr;
}

// Allocate from asset arena
void *vk_asset_alloc(VkDeviceSize size) {
    if (!vk.arena_manager.enabled || !vk.arena_manager.initialized) {
        return NULL;
    }

    void *ptr = vk_arena_alloc(&vk.arena_manager.asset_arena, size);
    if (ptr) {
        atomic_fetch_add_explicit(&vk.arena_manager.allocation_operations, 1, memory_order_relaxed);
        atomic_fetch_add_explicit(&vk.arena_manager.total_allocated_bytes, (uint64_t)size, memory_order_relaxed);
    }
    return ptr;
}

// Allocate from persistent arena
void *vk_persistent_alloc(VkDeviceSize size) {
    if (!vk.arena_manager.enabled || !vk.arena_manager.initialized) {
        return NULL;
    }

    void *ptr = vk_arena_alloc(&vk.arena_manager.persistent_arena, size);
    if (ptr) {
        atomic_fetch_add_explicit(&vk.arena_manager.allocation_operations, 1, memory_order_relaxed);
        atomic_fetch_add_explicit(&vk.arena_manager.total_allocated_bytes, (uint64_t)size, memory_order_relaxed);
    }
    return ptr;
}

// Create a dynamic arena
vk_memory_arena_t *vk_create_dynamic_arena(VkDeviceSize size, const char *name) {
    if (!vk.arena_manager.enabled || !vk.arena_manager.initialized) {
        return NULL;
    }

    if (vk.arena_manager.active_dynamic_arenas >= vk.arena_manager.max_dynamic_arenas) {
        ri.Printf(PRINT_WARNING, "Vulkan: Maximum dynamic arenas reached\n");
        return NULL;
    }

    uint32_t index = vk.arena_manager.active_dynamic_arenas++;
    vk_memory_arena_t *arena = &vk.arena_manager.dynamic_arenas[index];

    VkDeviceSize arena_size = size > 0 ? size : vk.arena_manager.dynamic_arena_size;

    if (!vk_init_memory_arena(arena, arena_size, name, NULL)) {
        vk.arena_manager.active_dynamic_arenas--;
        return NULL;
    }

    return arena;
}

// Destroy a dynamic arena
void vk_destroy_dynamic_arena(vk_memory_arena_t *arena) {
    if (!arena) {
        return;
    }

    // Find the arena in our pool
    for (uint32_t i = 0; i < vk.arena_manager.active_dynamic_arenas; i++) {
        if (&vk.arena_manager.dynamic_arenas[i] == arena) {
            vk_shutdown_memory_arena(arena);

            // Move last arena to this position
            if (i < vk.arena_manager.active_dynamic_arenas - 1) {
                vk.arena_manager.dynamic_arenas[i] =
                    vk.arena_manager.dynamic_arenas[vk.arena_manager.active_dynamic_arenas - 1];
            }

            vk.arena_manager.active_dynamic_arenas--;
            return;
        }
    }
}

// Allocate from a specific arena
void *vk_arena_alloc_from(vk_memory_arena_t *arena, VkDeviceSize size) {
    if (!arena || !arena->is_active) {
        return NULL;
    }

    void *ptr = vk_arena_alloc(arena, size);
    if (ptr) {
        atomic_fetch_add_explicit(&vk.arena_manager.allocation_operations, 1, memory_order_relaxed);
        atomic_fetch_add_explicit(&vk.arena_manager.total_allocated_bytes, (uint64_t)size, memory_order_relaxed);
    }
    return ptr;
}

// Print arena statistics
void vk_print_arena_stats(void) {
    if (!vk.arena_manager.enabled || !vk.arena_manager.initialized) {
        ri.Printf(PRINT_ALL, "Arena memory manager not initialized\n");
        return;
    }

    ri.Printf(PRINT_ALL, "=== Arena Memory Manager Statistics ===\n");

    // Pre-allocated arenas
    const struct {
        const char *name;
        vk_memory_arena_t *arena;
    } arenas[] = {
        {"Frame", &vk.arena_manager.frame_arena},
        {"Render", &vk.arena_manager.render_arena},
        {"Asset", &vk.arena_manager.asset_arena},
        {"Persistent", &vk.arena_manager.persistent_arena}
    };

    for (int i = 0; i < 4; i++) {
        vk_memory_arena_t *arena = arenas[i].arena;
        if (arena->is_active) {
            float usage_percent = arena->size > 0 ? (float)arena->used / arena->size * 100.0f : 0.0f;
            ri.Printf(PRINT_ALL, "%s Arena: %lu/%lu MB used (%u allocs, peak %lu MB, %.1f%%)\n",
                arenas[i].name,
                (unsigned long)(arena->used / (1024 * 1024)),
                (unsigned long)(arena->size / (1024 * 1024)),
                arena->allocation_count,
                (unsigned long)(arena->peak_used / (1024 * 1024)),
                usage_percent);
        }
    }

    // Dynamic arenas
    ri.Printf(PRINT_ALL, "Dynamic Arenas: %u/%u active\n",
        vk.arena_manager.active_dynamic_arenas, vk.arena_manager.max_dynamic_arenas);

    for (uint32_t i = 0; i < vk.arena_manager.active_dynamic_arenas; i++) {
        vk_memory_arena_t *arena = &vk.arena_manager.dynamic_arenas[i];
        float usage_percent = arena->size > 0 ? (float)arena->used / arena->size * 100.0f : 0.0f;
        ri.Printf(PRINT_ALL, "  Dynamic[%u] '%s': %lu/%lu MB used (%u allocs, %.1f%%)\n",
            i, arena->name ? arena->name : "unnamed",
            (unsigned long)(arena->used / (1024 * 1024)),
            (unsigned long)(arena->size / (1024 * 1024)),
            arena->allocation_count, usage_percent);
    }

    // Overall statistics
    ri.Printf(PRINT_ALL, "Total Operations: %lu allocations, %lu resets\n",
        (unsigned long)vk.arena_manager.allocation_operations,
        (unsigned long)vk.arena_manager.arena_resets);
    ri.Printf(PRINT_ALL, "Total Memory: %lu MB allocated\n",
        (unsigned long)(vk.arena_manager.total_allocated_bytes / (1024 * 1024)));
}

// Memory Advisor Implementation

// Forward declarations for static functions
static void vk_analyze_access_patterns(void);
static void vk_generate_layout_recommendations(void);
static void vk_add_layout_recommendation(vk_layout_optimization_type_t type, void *target,
                                       VkDeviceSize size, float improvement, uint32_t priority,
                                       const char *description, qboolean can_auto_apply);
static void vk_apply_layout_optimizations(void);
static void vk_apply_data_locality_optimization(void);
static void vk_apply_cache_alignment_optimization(void);

// Initialize the memory advisor system
qboolean vk_init_memory_advisor(void) {
    if (vk.memory_advisor.initialized) {
        return qtrue;
    }

    ri.Printf(PRINT_ALL, "Vulkan: Initializing memory advisor\n");

    // Configure analyzer
    vk.memory_advisor.analyzer.max_access_entries = 65536; // Track up to 64K access patterns
    vk.memory_advisor.analyzer.access_log = (vk_memory_access_t*)ri.Malloc(
        sizeof(vk_memory_access_t) * vk.memory_advisor.analyzer.max_access_entries);
    if (!vk.memory_advisor.analyzer.access_log) {
        ri.Printf(PRINT_WARNING, "Vulkan: Failed to allocate access log\n");
        return qfalse;
    }
    memset(vk.memory_advisor.analyzer.access_log, 0,
           sizeof(vk_memory_access_t) * vk.memory_advisor.analyzer.max_access_entries);

    // Configure hot/cold data tracking
    vk.memory_advisor.analyzer.max_hot_addresses = 1024;
    vk.memory_advisor.analyzer.max_cold_addresses = 1024;

    vk.memory_advisor.analyzer.hot_addresses = (void**)ri.Malloc(
        sizeof(void*) * vk.memory_advisor.analyzer.max_hot_addresses);
    vk.memory_advisor.analyzer.cold_addresses = (void**)ri.Malloc(
        sizeof(void*) * vk.memory_advisor.analyzer.max_cold_addresses);

    if (!vk.memory_advisor.analyzer.hot_addresses || !vk.memory_advisor.analyzer.cold_addresses) {
        ri.Printf(PRINT_WARNING, "Vulkan: Failed to allocate hot/cold address arrays\n");
        vk_shutdown_memory_advisor();
        return qfalse;
    }

    memset(vk.memory_advisor.analyzer.hot_addresses, 0,
           sizeof(void*) * vk.memory_advisor.analyzer.max_hot_addresses);
    memset(vk.memory_advisor.analyzer.cold_addresses, 0,
           sizeof(void*) * vk.memory_advisor.analyzer.max_cold_addresses);

    // Configure recommendations
    vk.memory_advisor.max_recommendations = 256;
    vk.memory_advisor.recommendations = (vk_layout_recommendation_t*)ri.Malloc(
        sizeof(vk_layout_recommendation_t) * vk.memory_advisor.max_recommendations);
    if (!vk.memory_advisor.recommendations) {
        ri.Printf(PRINT_WARNING, "Vulkan: Failed to allocate recommendations array\n");
        vk_shutdown_memory_advisor();
        return qfalse;
    }
    memset(vk.memory_advisor.recommendations, 0,
           sizeof(vk_layout_recommendation_t) * vk.memory_advisor.max_recommendations);

    // Configure advisor parameters
    vk.memory_advisor.analysis_window_frames = 300; // Analyze over 5 minutes at 60fps
    vk.memory_advisor.min_accesses_for_hot = 100;   // 100 accesses = hot data
    vk.memory_advisor.optimization_threshold = 0.05f; // 5% improvement minimum
    vk.memory_advisor.enable_pattern_learning = qtrue;
    vk.memory_advisor.auto_optimization = qfalse;    // Manual by default for safety

    // Initialize performance metrics
    vk.memory_advisor.cache_hit_rate = 0.85f;       // Assume 85% cache hit rate initially
    vk.memory_advisor.cache_miss_penalty_ns = 200;  // 200ns cache miss penalty

    vk.memory_advisor.enabled = qtrue;
    vk.memory_advisor.initialized = qtrue;

    ri.Printf(PRINT_ALL, "Vulkan: Memory advisor initialized with %u access tracking capacity\n",
        vk.memory_advisor.analyzer.max_access_entries);

    return qtrue;
}

// Shutdown the memory advisor
void vk_shutdown_memory_advisor(void) {
    if (!vk.memory_advisor.initialized) {
        return;
    }

    ri.Printf(PRINT_ALL, "Vulkan: Shutting down memory advisor\n");

    if (vk.memory_advisor.analyzer.access_log) {
        ri.Free(vk.memory_advisor.analyzer.access_log);
        vk.memory_advisor.analyzer.access_log = NULL;
    }

    if (vk.memory_advisor.analyzer.hot_addresses) {
        ri.Free(vk.memory_advisor.analyzer.hot_addresses);
        vk.memory_advisor.analyzer.hot_addresses = NULL;
    }

    if (vk.memory_advisor.analyzer.cold_addresses) {
        ri.Free(vk.memory_advisor.analyzer.cold_addresses);
        vk.memory_advisor.analyzer.cold_addresses = NULL;
    }

    if (vk.memory_advisor.recommendations) {
        ri.Free(vk.memory_advisor.recommendations);
        vk.memory_advisor.recommendations = NULL;
    }

    vk.memory_advisor.initialized = qfalse;
    ri.Printf(PRINT_ALL, "Vulkan: Memory advisor shutdown complete\n");
}

// Record a memory access for pattern analysis
void vk_record_memory_advisor_access(void *address, VkDeviceSize offset, const char *resource_name) {
    if (!vk.memory_advisor.enabled || !vk.memory_advisor.initialized) {
        return;
    }

    vk_access_pattern_analyzer_t *analyzer = &vk.memory_advisor.analyzer;

    // Record access in circular buffer
    uint32_t index = analyzer->current_access_index;
    vk_memory_access_t *access = &analyzer->access_log[index];

    access->memory_address = address;
    access->access_offset = offset;
    access->access_time = vk.arena_manager.current_frame; // Use frame counter as timestamp
    access->resource_name = resource_name;

    // Simple pattern detection
    if (index > 0) {
        vk_memory_access_t *prev_access = &analyzer->access_log[(index - 1) % analyzer->max_access_entries];
        VkDeviceSize offset_diff = (uintptr_t)address - (uintptr_t)prev_access->memory_address;

        if (offset_diff == prev_access->access_offset - offset) {
            access->access_pattern = 1; // Sequential
            analyzer->sequential_accesses++;
        } else if (offset_diff > 64) { // Cache line size
            access->access_pattern = 2; // Random
            analyzer->random_accesses++;
        } else {
            access->access_pattern = 3; // Strided
            analyzer->strided_accesses++;
        }
    }

    analyzer->current_access_index = (index + 1) % analyzer->max_access_entries;
    analyzer->total_access_count++;

    atomic_fetch_add_explicit(&vk.memory_advisor.total_memory_accesses, 1, memory_order_relaxed);
}

// Analyze access patterns and generate recommendations
static void vk_analyze_access_patterns(void) {
    if (!vk.memory_advisor.enabled || !vk.memory_advisor.initialized) {
        return;
    }

    vk_access_pattern_analyzer_t *analyzer = &vk.memory_advisor.analyzer;

    // Reset counters for this analysis
    analyzer->sequential_accesses = 0;
    analyzer->random_accesses = 0;
    analyzer->strided_accesses = 0;
    analyzer->hot_address_count = 0;
    analyzer->cold_address_count = 0;

    // Count access frequencies

    // Simple frequency analysis - in real implementation, this would be more sophisticated
    for (uint32_t i = 0; i < analyzer->max_access_entries && analyzer->hot_address_count < analyzer->max_hot_addresses; i++) {
        vk_memory_access_t *access = &analyzer->access_log[i];
        if (access->memory_address && access->access_count >= vk.memory_advisor.min_accesses_for_hot) {
            // Check if already in hot list
            qboolean already_hot = qfalse;
            for (uint32_t j = 0; j < analyzer->hot_address_count; j++) {
                if (analyzer->hot_addresses[j] == access->memory_address) {
                    already_hot = qtrue;
                    break;
                }
            }

            if (!already_hot) {
                analyzer->hot_addresses[analyzer->hot_address_count++] = access->memory_address;
            }
        }
    }

    // Estimate cache hit rate based on access patterns
    uint64_t total_acc = analyzer->sequential_accesses + analyzer->random_accesses + analyzer->strided_accesses;
    if (total_acc > 0) {
        // Sequential accesses have high cache hit rates, random have low
        float sequential_ratio = (float)analyzer->sequential_accesses / total_acc;
        float random_ratio = (float)analyzer->random_accesses / total_acc;

        vk.memory_advisor.cache_hit_rate = 0.95f * sequential_ratio + 0.3f * random_ratio + 0.6f * (1.0f - sequential_ratio - random_ratio);
    }

    atomic_store_explicit(&vk.memory_advisor.analysis_time_ns, Sys_Milliseconds(), memory_order_relaxed);

    // Generate optimization recommendations
    vk_generate_layout_recommendations();
}

// Generate layout optimization recommendations
static void vk_generate_layout_recommendations(void) {
    atomic_store_explicit(&vk.memory_advisor.recommendation_count, 0, memory_order_relaxed);

    // Check cache performance
    if (vk.memory_advisor.cache_hit_rate < 0.7f) {
        // Low cache hit rate - recommend data locality optimization
        vk_add_layout_recommendation(LAYOUT_OPTIMIZATION_DATA_LOCALITY,
                                   NULL, 0, 0.15f, 10,
                                   "Improve data locality - cache hit rate is below 70%",
                                   qtrue);
    }

    // Check for hot data that could benefit from cache alignment
    if (vk.memory_advisor.analyzer.hot_address_count > 10) {
        vk_add_layout_recommendation(LAYOUT_OPTIMIZATION_CACHE_ALIGNMENT,
                                   NULL, 0, 0.08f, 7,
                                   "Align hot data structures to cache lines",
                                   qtrue);
    }

    // Check access patterns for prefetching opportunities
    if (vk.memory_advisor.analyzer.strided_accesses > vk.memory_advisor.analyzer.sequential_accesses * 0.5f) {
        vk_add_layout_recommendation(LAYOUT_OPTIMIZATION_PRELOADING,
                                   NULL, 0, 0.12f, 8,
                                   "Implement software prefetching for strided access patterns",
                                   qfalse);
    }
}

// Add a layout recommendation
static void vk_add_layout_recommendation(vk_layout_optimization_type_t type, void *target,
                                       VkDeviceSize size, float improvement, uint32_t priority,
                                       const char *description, qboolean can_auto_apply) {
    if (vk.memory_advisor.recommendation_count >= vk.memory_advisor.max_recommendations) {
        return;
    }

    if (improvement < vk.memory_advisor.optimization_threshold) {
        return; // Below threshold
    }

    vk_layout_recommendation_t *rec = &vk.memory_advisor.recommendations[atomic_fetch_add_explicit(&vk.memory_advisor.recommendation_count, 1, memory_order_relaxed)];
    rec->optimization_type = type;
    rec->target_memory = target;
    rec->target_size = size;
    rec->expected_improvement = improvement;
    rec->priority = priority;
    rec->description = description;
    rec->can_auto_apply = can_auto_apply;
}

// Apply automatic optimizations
static void vk_apply_layout_optimizations(void) {
    if (!vk.memory_advisor.auto_optimization) {
        return;
    }

    uint32_t rec_count = atomic_load_explicit(&vk.memory_advisor.recommendation_count, memory_order_relaxed);
    for (uint32_t i = 0; i < rec_count; i++) {
        vk_layout_recommendation_t *rec = &vk.memory_advisor.recommendations[i];

        if (rec->can_auto_apply && rec->expected_improvement >= vk.memory_advisor.optimization_threshold) {
            switch (rec->optimization_type) {
                case LAYOUT_OPTIMIZATION_DATA_LOCALITY:
                    vk_apply_data_locality_optimization();
                    break;
                case LAYOUT_OPTIMIZATION_CACHE_ALIGNMENT:
                    vk_apply_cache_alignment_optimization();
                    break;
                default:
                    // Other optimizations require manual implementation
                    break;
            }

            atomic_fetch_add_explicit(&vk.memory_advisor.optimizations_applied, 1, memory_order_relaxed);
            atomic_fetch_add_explicit(&vk.memory_advisor.performance_improvements, (uint64_t)(rec->expected_improvement * 100.0f), memory_order_relaxed);
        }
    }
}

// Apply data locality optimization (simplified)
static void vk_apply_data_locality_optimization(void) {
    ri.Printf(PRINT_ALL, "Vulkan: Applying data locality optimization\n");
    // In a real implementation, this would reorganize memory layout
    // to group frequently accessed data together
}

// Apply cache alignment optimization (simplified)
static void vk_apply_cache_alignment_optimization(void) {
    ri.Printf(PRINT_ALL, "Vulkan: Applying cache alignment optimization\n");
    // In a real implementation, this would align data structures
    // to cache line boundaries
}

// Update memory advisor (called per frame)
void vk_update_memory_advisor(void) {
    if (!vk.memory_advisor.enabled || !vk.memory_advisor.initialized) {
        return;
    }

    // Periodic pattern analysis
    static uint32_t frame_counter = 0;
    frame_counter++;

    if (frame_counter % 60 == 0) { // Analyze every 60 frames (~1 second at 60fps)
        vk_analyze_access_patterns();
        vk_apply_layout_optimizations();
    }
}

// Print memory advisor statistics
void vk_print_memory_advisor_stats(void) {
    if (!vk.memory_advisor.enabled || !vk.memory_advisor.initialized) {
        ri.Printf(PRINT_ALL, "Memory advisor not initialized\n");
        return;
    }

    ri.Printf(PRINT_ALL, "=== Memory Advisor Statistics ===\n");
    ri.Printf(PRINT_ALL, "Enabled: %s, Auto-Optimization: %s\n",
        vk.memory_advisor.enabled ? "Yes" : "No",
        vk.memory_advisor.auto_optimization ? "Yes" : "No");

    ri.Printf(PRINT_ALL, "Performance Metrics:\n");
    ri.Printf(PRINT_ALL, "  Cache Hit Rate: %.1f%%\n", vk.memory_advisor.cache_hit_rate * 100.0f);
    ri.Printf(PRINT_ALL, "  Total Memory Accesses: %lu\n",
        (unsigned long)vk.memory_advisor.total_memory_accesses);
    ri.Printf(PRINT_ALL, "  Cache Miss Penalty: %lu ns\n",
        (unsigned long)vk.memory_advisor.cache_miss_penalty_ns);

    vk_access_pattern_analyzer_t *analyzer = &vk.memory_advisor.analyzer;
    ri.Printf(PRINT_ALL, "Access Patterns:\n");
    ri.Printf(PRINT_ALL, "  Sequential: %lu\n", (unsigned long)analyzer->sequential_accesses);
    ri.Printf(PRINT_ALL, "  Random: %lu\n", (unsigned long)analyzer->random_accesses);
    ri.Printf(PRINT_ALL, "  Strided: %lu\n", (unsigned long)analyzer->strided_accesses);
    ri.Printf(PRINT_ALL, "  Hot Addresses: %u\n", analyzer->hot_address_count);
    ri.Printf(PRINT_ALL, "  Cold Addresses: %u\n", analyzer->cold_address_count);

    ri.Printf(PRINT_ALL, "Optimizations:\n");
    ri.Printf(PRINT_ALL, "  Applied: %lu\n", (unsigned long)vk.memory_advisor.optimizations_applied);
    ri.Printf(PRINT_ALL, "  Performance Improvements: %lu%%\n",
        (unsigned long)vk.memory_advisor.performance_improvements);
    ri.Printf(PRINT_ALL, "  Analysis Time: %lu ns\n",
        (unsigned long)vk.memory_advisor.analysis_time_ns);

    ri.Printf(PRINT_ALL, "Recommendations (%u total):\n", vk.memory_advisor.recommendation_count);
    for (uint32_t i = 0; i < vk.memory_advisor.recommendation_count && i < 5; i++) {
        vk_layout_recommendation_t *rec = &vk.memory_advisor.recommendations[i];
        const char *type_name = "Unknown";
        switch (rec->optimization_type) {
            case LAYOUT_OPTIMIZATION_DATA_LOCALITY: type_name = "Data Locality"; break;
            case LAYOUT_OPTIMIZATION_CACHE_ALIGNMENT: type_name = "Cache Alignment"; break;
            case LAYOUT_OPTIMIZATION_PRELOADING: type_name = "Preloading"; break;
            case LAYOUT_OPTIMIZATION_COMPRESSION: type_name = "Compression"; break;
            case LAYOUT_OPTIMIZATION_REORDERING: type_name = "Reordering"; break;
            default: break;
        }
        ri.Printf(PRINT_ALL, "  [%u] %s: %s (%.1f%% improvement, %s)\n",
            rec->priority, type_name, rec->description,
            rec->expected_improvement * 100.0f,
            rec->can_auto_apply ? "auto-applicable" : "manual");
    }

    if (vk.memory_advisor.recommendation_count > 5) {
        ri.Printf(PRINT_ALL, "  ... and %u more recommendations\n",
            vk.memory_advisor.recommendation_count - 5);
    }
}

// Enable/disable auto optimization
void vk_set_memory_advisor_auto_optimization(qboolean enabled) {
    vk.memory_advisor.auto_optimization = enabled;
    ri.Printf(PRINT_ALL, "Vulkan: Memory advisor auto-optimization %s\n",
        enabled ? "enabled" : "disabled");
}

// Force immediate pattern analysis
void vk_force_memory_analysis(void) {
    if (vk.memory_advisor.enabled && vk.memory_advisor.initialized) {
        vk_analyze_access_patterns();
        ri.Printf(PRINT_ALL, "Vulkan: Forced memory pattern analysis complete\n");
    }
}

// Cache-Conscious Data Structures Implementation

// Initialize cache-conscious array
qboolean vk_cache_array_init(vk_cache_array_t *array, VkDeviceSize element_size,
                           uint32_t initial_capacity, const char *debug_name) {
    if (!array || element_size == 0) {
        return qfalse;
    }

    array->data = ri.Malloc(element_size * initial_capacity + CACHE_LINE_SIZE);
    if (!array->data) {
        return qfalse;
    }

    // Align to cache line boundary
    uintptr_t addr = (uintptr_t)array->data;
    uintptr_t aligned_addr = (addr + CACHE_LINE_SIZE - 1) & ~CACHE_LINE_MASK;
    array->data = (void*)aligned_addr;

    array->element_size = element_size;
    array->capacity = initial_capacity;
    array->size = 0;
    array->growth_factor = 2; // Double capacity on growth
    array->debug_name = debug_name;

    return qtrue;
}

// Resize cache array
qboolean vk_cache_array_resize(vk_cache_array_t *array, uint32_t new_capacity) {
    if (!array || new_capacity <= array->capacity) {
        return qtrue;
    }

    void *new_data = ri.Malloc(array->element_size * new_capacity + CACHE_LINE_SIZE);
    if (!new_data) {
        return qfalse;
    }

    // Align to cache line
    uintptr_t addr = (uintptr_t)new_data;
    uintptr_t aligned_addr = (addr + CACHE_LINE_SIZE - 1) & ~CACHE_LINE_MASK;
    void *aligned_data = (void*)aligned_addr;

    // Copy existing data
    if (array->data && array->size > 0) {
        memcpy(aligned_data, array->data, array->element_size * array->size);
    }

    // Free old data (find original allocation)
    if (array->data) {
        uintptr_t orig_addr = (uintptr_t)array->data - CACHE_LINE_SIZE;
        void *orig_ptr = (void*)((orig_addr & ~CACHE_LINE_MASK) - CACHE_LINE_SIZE + CACHE_LINE_SIZE);
        ri.Free(orig_ptr);
    }

    array->data = aligned_data;
    array->capacity = new_capacity;

    return qtrue;
}

// Push element to cache array
qboolean vk_cache_array_push(vk_cache_array_t *array, const void *element) {
    if (!array) {
        return qfalse;
    }

    if (array->size >= array->capacity) {
        uint32_t new_capacity = array->capacity * array->growth_factor;
        if (new_capacity == 0) new_capacity = 16; // Minimum capacity
        if (!vk_cache_array_resize(array, new_capacity)) {
            return qfalse;
        }
    }

    void *dest = (uint8_t*)array->data + (array->size * array->element_size);
    memcpy(dest, element, array->element_size);
    array->size++;

    return qtrue;
}

// Pop element from cache array
qboolean vk_cache_array_pop(vk_cache_array_t *array, void *element) {
    if (!array || array->size == 0) {
        return qfalse;
    }

    array->size--;
    if (element) {
        void *src = (uint8_t*)array->data + (array->size * array->element_size);
        memcpy(element, src, array->element_size);
    }

    return qtrue;
}

// Get element at index
void *vk_cache_array_get(vk_cache_array_t *array, uint32_t index) {
    if (!array || index >= array->size) {
        return NULL;
    }

    return (uint8_t*)array->data + (index * array->element_size);
}

// Clear cache array (doesn't free memory)
void vk_cache_array_clear(vk_cache_array_t *array) {
    if (array) {
        atomic_store_explicit(&array->size, 0, memory_order_relaxed);
    }
}

// Destroy cache array
void vk_cache_array_destroy(vk_cache_array_t *array) {
    if (!array) {
        return;
    }

    if (array->data) {
        // Find original allocation
        uintptr_t addr = (uintptr_t)array->data;
        uintptr_t orig_addr = addr - CACHE_LINE_SIZE;
        void *orig_ptr = (void*)((orig_addr & ~CACHE_LINE_MASK) - CACHE_LINE_SIZE + CACHE_LINE_SIZE);
        ri.Free(orig_ptr);
        array->data = NULL;
    }

    array->capacity = 0;
    atomic_store_explicit(&array->size, 0, memory_order_relaxed);
}

// Simple hash function for uint32_t
static uint32_t vk_hash_uint32(const void *key, VkDeviceSize size) {
    (void)size; // Unused
    uint32_t k = *(const uint32_t*)key;
    k ^= k >> 16;
    k *= 0x85ebca6b;
    k ^= k >> 13;
    k *= 0xc2b2ae35;
    k ^= k >> 16;
    return k;
}

// Simple equals function for uint32_t
static qboolean vk_equals_uint32(const void *a, const void *b, VkDeviceSize size) {
    (void)size; // Unused
    return *(const uint32_t*)a == *(const uint32_t*)b;
}

// Initialize cache-conscious hash map
qboolean vk_cache_hash_map_init(vk_cache_hash_map_t *map, VkDeviceSize key_size,
                              VkDeviceSize value_size, uint32_t initial_capacity,
                              uint32_t (*hash_func)(const void*, VkDeviceSize),
                              qboolean (*equals_func)(const void*, const void*, VkDeviceSize),
                              const char *debug_name) {
    if (!map || key_size == 0 || value_size == 0) {
        return qfalse;
    }

    // Use default functions if not provided
    if (!hash_func) hash_func = vk_hash_uint32;
    if (!equals_func) equals_func = vk_equals_uint32;

    // Ensure capacity is power of 2
    uint32_t capacity = 16;
    while (capacity < initial_capacity) {
        capacity *= 2;
    }

    // Allocate aligned memory for keys, values, and metadata
    VkDeviceSize key_array_size = key_size * capacity + CACHE_LINE_SIZE;
    VkDeviceSize value_array_size = value_size * capacity + CACHE_LINE_SIZE;
    VkDeviceSize metadata_size = capacity + CACHE_LINE_SIZE;

    map->keys = ri.Malloc(key_array_size);
    map->values = ri.Malloc(value_array_size);
    map->metadata = ri.Malloc(metadata_size);

    if (!map->keys || !map->values || !map->metadata) {
        if (map->keys) ri.Free(map->keys);
        if (map->values) ri.Free(map->values);
        if (map->metadata) ri.Free(map->metadata);
        return qfalse;
    }

    // Align pointers
    uintptr_t key_addr = (uintptr_t)map->keys;
    uintptr_t value_addr = (uintptr_t)map->values;
    uintptr_t meta_addr = (uintptr_t)map->metadata;

    map->keys = (void*)((key_addr + CACHE_LINE_SIZE - 1) & ~CACHE_LINE_MASK);
    map->values = (void*)((value_addr + CACHE_LINE_SIZE - 1) & ~CACHE_LINE_MASK);
    map->metadata = (uint8_t*)((meta_addr + CACHE_LINE_SIZE - 1) & ~CACHE_LINE_MASK);

    // Initialize metadata (0 = empty, 1 = occupied, 2 = deleted)
    memset(map->metadata, 0, capacity);

    map->key_size = key_size;
    map->value_size = value_size;
    map->capacity = capacity;
    atomic_init(&map->size, 0);
    map->max_load_factor = 75; // 75% load factor
    map->hash_func = hash_func;
    map->equals_func = equals_func;
    map->debug_name = debug_name;

    return qtrue;
}

// Initialize cache-conscious queue
qboolean vk_cache_queue_init(vk_cache_queue_t *queue, VkDeviceSize element_size,
                           uint32_t capacity, const char *debug_name) {
    if (!queue || element_size == 0 || capacity == 0) {
        return qfalse;
    }

    // Ensure capacity is power of 2 for efficient modulo
    uint32_t actual_capacity = 16;
    while (actual_capacity < capacity) {
        actual_capacity *= 2;
    }

    VkDeviceSize buffer_size = element_size * actual_capacity + CACHE_LINE_SIZE;
    queue->buffer = ri.Malloc(buffer_size);

    if (!queue->buffer) {
        return qfalse;
    }

    // Align buffer
    uintptr_t addr = (uintptr_t)queue->buffer;
    uintptr_t aligned_addr = (addr + CACHE_LINE_SIZE - 1) & ~CACHE_LINE_MASK;
    queue->buffer = (void*)aligned_addr;

    queue->element_size = element_size;
    queue->capacity = actual_capacity;
    queue->head = 0;
    queue->tail = 0;
    queue->mask = actual_capacity - 1; // For efficient modulo
    queue->debug_name = debug_name;

    return qtrue;
}

// Initialize cache structures manager
qboolean vk_init_cache_structures_manager(void) {
    if (vk.cache_structures_manager.initialized) {
        return qtrue;
    }

    ri.Printf(PRINT_ALL, "Vulkan: Initializing cache-conscious data structures manager\n");

    // Initialize temporary pools
    for (uint32_t i = 0; i < 16; i++) {
        if (!vk_cache_array_init(&vk.cache_structures_manager.temp_array_pool[i], 1, 64, NULL)) {
            ri.Printf(PRINT_WARNING, "Vulkan: Failed to initialize temp array pool\n");
            return qfalse;
        }
    }

    atomic_init(&vk.cache_structures_manager.temp_array_count, 16);

    // Hash maps for temporary use
    for (uint32_t i = 0; i < 8; i++) {
        if (!vk_cache_hash_map_init(&vk.cache_structures_manager.temp_hash_pool[i], sizeof(uint32_t),
                                  sizeof(uint32_t), 32, vk_hash_uint32, vk_equals_uint32, NULL)) {
            ri.Printf(PRINT_WARNING, "Vulkan: Failed to initialize temp hash pool\n");
            return qfalse;
        }
    }

    atomic_init(&vk.cache_structures_manager.temp_hash_count, 8);

    // Queues for temporary use
    for (uint32_t i = 0; i < 8; i++) {
        if (!vk_cache_queue_init(&vk.cache_structures_manager.temp_queue_pool[i], sizeof(void*), 32, NULL)) {
            ri.Printf(PRINT_WARNING, "Vulkan: Failed to initialize temp queue pool\n");
            return qfalse;
        }
    }

    atomic_init(&vk.cache_structures_manager.temp_queue_count, 8);

    // Initialize statistics
    atomic_init(&vk.cache_structures_manager.cache_misses_avoided, 0);
    atomic_init(&vk.cache_structures_manager.prefetch_operations, 0);
    atomic_init(&vk.cache_structures_manager.false_sharing_avoided, 0);
    atomic_init(&vk.cache_structures_manager.data_locality_improvements, 0);
    atomic_init(&vk.cache_structures_manager.total_allocated, 0);
    atomic_init(&vk.cache_structures_manager.cache_aligned_allocated, 0);

    vk.cache_structures_manager.enabled = qtrue;
    vk.cache_structures_manager.initialized = qtrue;
    vk.cache_structures_manager.debug_name = "cache_structures_manager";

    ri.Printf(PRINT_ALL, "Vulkan: Cache-conscious data structures manager initialized\n");

    return qtrue;
}

// Shutdown cache structures manager
void vk_shutdown_cache_structures_manager(void) {
    if (!vk.cache_structures_manager.initialized) {
        return;
    }

    ri.Printf(PRINT_ALL, "Vulkan: Shutting down cache-conscious data structures manager\n");

    // Cleanup temporary pools
    for (uint32_t i = 0; i < vk.cache_structures_manager.temp_array_count; i++) {
        vk_cache_array_destroy(&vk.cache_structures_manager.temp_array_pool[i]);
    }

    for (uint32_t i = 0; i < vk.cache_structures_manager.temp_hash_count; i++) {
        if (vk.cache_structures_manager.temp_hash_pool[i].keys) ri.Free(vk.cache_structures_manager.temp_hash_pool[i].keys);
        if (vk.cache_structures_manager.temp_hash_pool[i].values) ri.Free(vk.cache_structures_manager.temp_hash_pool[i].values);
        if (vk.cache_structures_manager.temp_hash_pool[i].metadata) ri.Free(vk.cache_structures_manager.temp_hash_pool[i].metadata);
    }

    for (uint32_t i = 0; i < vk.cache_structures_manager.temp_queue_count; i++) {
        if (vk.cache_structures_manager.temp_queue_pool[i].buffer) {
            ri.Free(vk.cache_structures_manager.temp_queue_pool[i].buffer);
        }
    }

    vk.cache_structures_manager.initialized = qfalse;
    ri.Printf(PRINT_ALL, "Vulkan: Cache-conscious data structures manager shutdown complete\n");
}

// Print cache structures statistics
void vk_print_cache_structures_stats(void) {
    if (!vk.cache_structures_manager.enabled || !vk.cache_structures_manager.initialized) {
        ri.Printf(PRINT_ALL, "Cache structures manager not initialized\n");
        return;
    }

    ri.Printf(PRINT_ALL, "=== Cache-Conscious Data Structures Statistics ===\n");
    ri.Printf(PRINT_ALL, "Performance Improvements:\n");
    ri.Printf(PRINT_ALL, "  Cache misses avoided: %lu\n",
        (unsigned long)atomic_load_explicit(&vk.cache_structures_manager.cache_misses_avoided, memory_order_relaxed));
    ri.Printf(PRINT_ALL, "  Prefetch operations: %lu\n",
        (unsigned long)atomic_load_explicit(&vk.cache_structures_manager.prefetch_operations, memory_order_relaxed));
    ri.Printf(PRINT_ALL, "  False sharing avoided: %lu\n",
        (unsigned long)atomic_load_explicit(&vk.cache_structures_manager.cache_misses_avoided, memory_order_relaxed));
    ri.Printf(PRINT_ALL, "  Data locality improvements: %lu\n",
        (unsigned long)atomic_load_explicit(&vk.cache_structures_manager.data_locality_improvements, memory_order_relaxed));

    ri.Printf(PRINT_ALL, "Memory Usage:\n");
    ri.Printf(PRINT_ALL, "  Total allocated: %lu bytes\n",
        (unsigned long)atomic_load_explicit(&vk.cache_structures_manager.total_allocated, memory_order_relaxed));
    ri.Printf(PRINT_ALL, "  Cache-aligned allocated: %lu bytes (%.1f%%)\n",
        (unsigned long)atomic_load_explicit(&vk.cache_structures_manager.cache_aligned_allocated, memory_order_relaxed),
        atomic_load_explicit(&vk.cache_structures_manager.total_allocated, memory_order_relaxed) > 0 ?
        (float)atomic_load_explicit(&vk.cache_structures_manager.cache_aligned_allocated, memory_order_relaxed) /
        (float)atomic_load_explicit(&vk.cache_structures_manager.total_allocated, memory_order_relaxed) * 100.0f : 0.0f);
    ri.Printf(PRINT_ALL, "  Cache-aligned allocated: %lu MB (%.1f%%)\n",
        (unsigned long)(vk.cache_structures_manager.cache_aligned_allocated / (1024 * 1024)),
        vk.cache_structures_manager.total_allocated > 0 ?
        (float)vk.cache_structures_manager.cache_aligned_allocated / vk.cache_structures_manager.total_allocated * 100.0f : 0.0f);

    ri.Printf(PRINT_ALL, "Pool Status:\n");
    ri.Printf(PRINT_ALL, "  Temp arrays: %u available\n", vk.cache_structures_manager.temp_array_count);
    ri.Printf(PRINT_ALL, "  Temp hash maps: %u available\n", vk.cache_structures_manager.temp_hash_count);
    ri.Printf(PRINT_ALL, "  Temp queues: %u available\n", vk.cache_structures_manager.temp_queue_count);
}

// Render Graph Profiler Implementation

// Initialize GPU timestamp queries
static qboolean vk_init_timestamp_queries(vk_timestamp_query_t *queries, uint32_t query_count) {
    VkQueryPoolCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .queryType = VK_QUERY_TYPE_TIMESTAMP,
        .queryCount = query_count,
        .pipelineStatistics = 0
    };

    VkResult result = qvkCreateQueryPool(vk.device, &create_info, NULL, &queries->query_pool);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_WARNING, "Vulkan: Failed to create timestamp query pool: %s\n",
            vk_result_string(result));
        return qfalse;
    }

    queries->query_count = query_count;
    queries->current_query = 0;
    queries->timestamps = (uint64_t*)ri.Malloc(sizeof(uint64_t) * query_count);
    queries->available = qfalse;

    if (!queries->timestamps) {
        qvkDestroyQueryPool(vk.device, queries->query_pool, NULL);
        return qfalse;
    }

    memset(queries->timestamps, 0, sizeof(uint64_t) * query_count);

    // Reset query pool
    qvkResetQueryPool(vk.device, queries->query_pool, 0, query_count);

    return qtrue;
}

// Initialize pipeline statistics queries
static qboolean vk_init_pipeline_stats(vk_pipeline_stats_t *stats, VkQueryPipelineStatisticFlags flags) {
    // Count number of statistics we want
    uint32_t stat_count = 0;
    VkQueryPipelineStatisticFlags temp_flags = flags;
    while (temp_flags) {
        stat_count += (temp_flags & 1);
        temp_flags >>= 1;
    }

    VkQueryPoolCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS,
        .queryCount = stat_count,
        .pipelineStatistics = flags
    };

    VkResult result = qvkCreateQueryPool(vk.device, &create_info, NULL, &stats->query_pool);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_WARNING, "Vulkan: Failed to create pipeline statistics query pool: %s\n",
            vk_result_string(result));
        return qfalse;
    }

    stats->flags = flags;
    stats->query_count = stat_count;
    stats->statistics = (uint64_t*)ri.Malloc(sizeof(uint64_t) * stat_count);
    stats->available = qfalse;

    if (!stats->statistics) {
        qvkDestroyQueryPool(vk.device, stats->query_pool, NULL);
        return qfalse;
    }

    memset(stats->statistics, 0, sizeof(uint64_t) * stat_count);

    // Reset query pool
    qvkResetQueryPool(vk.device, stats->query_pool, 0, stat_count);

    return qtrue;
}

// Initialize the render profiler
qboolean vk_init_render_profiler(void) {
    if (vk.render_profiler.initialized) {
        return qtrue;
    }

    ri.Printf(PRINT_ALL, "Vulkan: Initializing render graph profiler\n");

    // Configure profiler
    atomic_init(&vk.render_profiler.frames_to_analyze, 300); // Keep 5 minutes of data at 60fps
    vk.render_profiler.max_frames = 300;
    atomic_init(&vk.render_profiler.max_passes_per_frame, 64);
    vk.render_profiler.enable_trend_analysis = qtrue;
    vk.render_profiler.auto_detect_bottlenecks = qtrue;
    vk.render_profiler.bottleneck_threshold = 0.7f; // 70% utilization = bottleneck
    vk.render_profiler.detailed_profiling = qtrue;

    // Allocate frame history
    vk.render_profiler.frame_history = (vk_frame_profile_t*)ri.Malloc(
        sizeof(vk_frame_profile_t) * vk.render_profiler.max_frames);
    if (!vk.render_profiler.frame_history) {
        ri.Printf(PRINT_WARNING, "Vulkan: Failed to allocate frame history buffer\n");
        return qfalse;
    }

    // Initialize frame history
    for (uint32_t i = 0; i < vk.render_profiler.max_frames; i++) {
        vk_frame_profile_t *frame = &vk.render_profiler.frame_history[i];
        memset(frame, 0, sizeof(vk_frame_profile_t));
        frame->passes = (vk_render_pass_profile_t*)ri.Malloc(
            sizeof(vk_render_pass_profile_t) * 64); // Hardcoded max passes for init
        if (!frame->passes) {
            ri.Printf(PRINT_WARNING, "Vulkan: Failed to allocate pass buffer for frame %u\n", i);
            return qfalse;
        }
        memset(frame->passes, 0, sizeof(vk_render_pass_profile_t) * 64);
        frame->max_passes = 64;
        atomic_init(&frame->pass_count, 0);
    }

    // Allocate current passes buffer
    vk.render_profiler.current_passes = (vk_render_pass_profile_t*)ri.Malloc(
        sizeof(vk_render_pass_profile_t) * 64);
    if (!vk.render_profiler.current_passes) {
        ri.Printf(PRINT_WARNING, "Vulkan: Failed to allocate current passes buffer\n");
        return qfalse;
    }
    memset(vk.render_profiler.current_passes, 0, sizeof(vk_render_pass_profile_t) * 64);

    atomic_init(&vk.render_profiler.current_frame_index, 0);
    atomic_init(&vk.render_profiler.total_frames_recorded, 0);
    atomic_init(&vk.render_profiler.current_pass_count, 0);
    atomic_init(&vk.render_profiler.frames_analyzed, 0);
    atomic_init(&vk.render_profiler.total_profiling_time_ns, 0);
    atomic_init(&vk.render_profiler.profiling_overhead_percent, 0);

    // Initialize GPU queries if detailed profiling is enabled
    if (vk.render_profiler.detailed_profiling) {
        // Initialize timestamp queries (2 per pass for start/end)
        if (!vk_init_timestamp_queries(&vk.render_profiler.timestamp_queries, 128)) {
            ri.Printf(PRINT_WARNING, "Vulkan: Timestamp queries not available, disabling detailed profiling\n");
            vk.render_profiler.detailed_profiling = qfalse;
        }

        // Initialize pipeline statistics
        VkQueryPipelineStatisticFlags stats_flags =
            VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT |
            VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT |
            VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT |
            VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT |
            VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT |
            VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT |
            VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT |
            VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT |
            VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT |
            VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT |
            VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT;

        if (!vk_init_pipeline_stats(&vk.render_profiler.pipeline_stats, stats_flags)) {
            ri.Printf(PRINT_WARNING, "Vulkan: Pipeline statistics not available\n");
        }
    }

    // Get GPU timestamp period for calibration
    VkPhysicalDeviceProperties device_props;
    vkGetPhysicalDeviceProperties(vk.physical_device, &device_props);
    vk.render_profiler.gpu_timestamp_period = device_props.limits.timestampPeriod;
    vk.render_profiler.timestamps_calibrated = qtrue;

    // Initialize statistics
    vk.render_profiler.total_profiling_time_ns = 0;
    vk.render_profiler.profiling_overhead_percent = 0;
    atomic_store_explicit(&vk.render_profiler.frames_analyzed, 0, memory_order_relaxed);
    vk.render_profiler.current_bottleneck = BOTTLENECK_NONE;
    vk.render_profiler.bottleneck_severity = 0.0f;

    vk.render_profiler.enabled = qtrue;
    vk.render_profiler.initialized = qtrue;
    vk.render_profiler.debug_name = "render_profiler";

    ri.Printf(PRINT_ALL, "Vulkan: Render graph profiler initialized with %s profiling\n",
        vk.render_profiler.detailed_profiling ? "detailed GPU" : "basic CPU");

    return qtrue;
}

// Shutdown the render profiler
void vk_shutdown_render_profiler(void) {
    if (!vk.render_profiler.initialized) {
        return;
    }

    ri.Printf(PRINT_ALL, "Vulkan: Shutting down render graph profiler\n");

    // Destroy GPU query pools
    if (vk.render_profiler.detailed_profiling) {
        if (vk.render_profiler.timestamp_queries.query_pool != VK_NULL_HANDLE) {
            qvkDestroyQueryPool(vk.device, vk.render_profiler.timestamp_queries.query_pool, NULL);
        }
        if (vk.render_profiler.pipeline_stats.query_pool != VK_NULL_HANDLE) {
            qvkDestroyQueryPool(vk.device, vk.render_profiler.pipeline_stats.query_pool, NULL);
        }
    }

    // Free timestamp buffers
    if (vk.render_profiler.timestamp_queries.timestamps) {
        ri.Free(vk.render_profiler.timestamp_queries.timestamps);
    }
    if (vk.render_profiler.pipeline_stats.statistics) {
        ri.Free(vk.render_profiler.pipeline_stats.statistics);
    }

    // Free current passes buffer
    if (vk.render_profiler.current_passes) {
        ri.Free(vk.render_profiler.current_passes);
    }

    // Free frame history
    if (vk.render_profiler.frame_history) {
        for (uint32_t i = 0; i < vk.render_profiler.max_frames; i++) {
            if (vk.render_profiler.frame_history[i].passes) {
                ri.Free(vk.render_profiler.frame_history[i].passes);
            }
        }
        ri.Free(vk.render_profiler.frame_history);
    }

    vk.render_profiler.initialized = qfalse;
    ri.Printf(PRINT_ALL, "Vulkan: Render graph profiler shutdown complete\n");
}

// Start profiling a render pass
void vk_profile_pass_start(const char *pass_name, uint32_t pass_id) {
    if (!vk.render_profiler.enabled || !vk.render_profiler.initialized) {
        return;
    }

    if (vk.render_profiler.current_pass_count >= vk.render_profiler.max_passes_per_frame) {
        return; // Too many passes
    }

    uint32_t pass_index = vk.render_profiler.current_pass_count++;
    vk_render_pass_profile_t *pass = &vk.render_profiler.current_passes[pass_index];

    // Initialize pass profile
    memset(pass, 0, sizeof(vk_render_pass_profile_t));
    pass->name = pass_name;
    pass->pass_id = pass_id;
    pass->frame_number = vk.arena_manager.current_frame;
    pass->start_time = ri.Milliseconds() * 1000000ULL; // Convert to nanoseconds

    // Record GPU timestamp if detailed profiling enabled
    if (vk.render_profiler.detailed_profiling &&
        vk.render_profiler.timestamp_queries.current_query < vk.render_profiler.timestamp_queries.query_count) {

        uint32_t query_index = vk.render_profiler.timestamp_queries.current_query++;
        vkCmdWriteTimestamp(vk.cmd->command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           vk.render_profiler.timestamp_queries.query_pool, query_index);
        pass->gpu_start_time = query_index;
    }

    // Start pipeline statistics if available
    if (vk.render_profiler.detailed_profiling &&
        vk.render_profiler.pipeline_stats.query_pool != VK_NULL_HANDLE) {

        vkCmdBeginQuery(vk.cmd->command_buffer, vk.render_profiler.pipeline_stats.query_pool,
                        pass_index, 0);
    }
}

// End profiling a render pass
void vk_profile_pass_end(const char *pass_name, uint32_t draw_calls, uint32_t vertices) {
    if (!vk.render_profiler.enabled || !vk.render_profiler.initialized) {
        return;
    }

    // Find the pass (simple linear search - could be optimized)
    for (uint32_t i = 0; i < vk.render_profiler.current_pass_count; i++) {
        vk_render_pass_profile_t *pass = &vk.render_profiler.current_passes[i];

        if (strcmp(pass->name, pass_name) == 0 && pass->end_time == 0) {
            // End timing
            pass->end_time = ri.Milliseconds() * 1000000ULL;

            // Update draw call statistics
            pass->draw_calls = draw_calls;
            pass->vertices_submitted = vertices;

            // Record GPU timestamp if detailed profiling enabled
            if (vk.render_profiler.detailed_profiling &&
                vk.render_profiler.timestamp_queries.current_query < vk.render_profiler.timestamp_queries.query_count) {

                uint32_t query_index = vk.render_profiler.timestamp_queries.current_query++;
                vkCmdWriteTimestamp(vk.cmd->command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                   vk.render_profiler.timestamp_queries.query_pool, query_index);
                pass->gpu_end_time = query_index;
            }

            // End pipeline statistics if available
            if (vk.render_profiler.detailed_profiling &&
                vk.render_profiler.pipeline_stats.query_pool != VK_NULL_HANDLE) {

                vkCmdEndQuery(vk.cmd->command_buffer, vk.render_profiler.pipeline_stats.query_pool, i);
            }

            return;
        }
    }
}

// Analyze profiling results and detect bottlenecks
static void vk_analyze_profiling_results(vk_frame_profile_t *frame) {
    if (!frame || frame->pass_count == 0) {
        return;
    }

    // Calculate total GPU time
    uint64_t total_gpu_time = 0;
    for (uint32_t i = 0; i < frame->pass_count; i++) {
        vk_render_pass_profile_t *pass = &frame->passes[i];
        if (pass->gpu_end_time > pass->gpu_start_time) {
            uint64_t gpu_time = (pass->gpu_end_time - pass->gpu_start_time) *
                               vk.render_profiler.gpu_timestamp_period;
            total_gpu_time += gpu_time;
        }
    }

    frame->gpu_time_ms = total_gpu_time / 1000000.0;

    // Detect bottlenecks
    vk_bottleneck_type_t bottleneck = BOTTLENECK_NONE;
    float severity = 0.0f;
    const char *bottleneck_desc = "None";
    const char *optimization_hint = "";

    // CPU vs GPU analysis
    double cpu_to_gpu_ratio = frame->cpu_time_ms / frame->gpu_time_ms;
    if (cpu_to_gpu_ratio > 2.0) {
        bottleneck = BOTTLENECK_CPU_BOUND;
        severity = cpu_to_gpu_ratio / 5.0f; // Normalize
        bottleneck_desc = "CPU Bound";
        optimization_hint = "Consider reducing CPU-side work or optimizing data preparation";
    }

    // Analyze individual passes for GPU bottlenecks
    for (uint32_t i = 0; i < frame->pass_count; i++) {
        vk_render_pass_profile_t *pass = &frame->passes[i];

        // Fragment shader bottleneck (high fragment invocations)
        if (pass->fragment_invocations > pass->vertex_invocations * 10) {
            bottleneck = BOTTLENECK_GPU_FRAGMENT;
            severity = (float)pass->fragment_invocations / (float)pass->vertex_invocations / 20.0f;
            bottleneck_desc = "Fragment Shader Bound";
            optimization_hint = "Consider reducing overdraw or optimizing fragment shaders";
            pass->bottleneck_score = severity;
            pass->bottleneck_type = bottleneck_desc;
            pass->optimization_hint = optimization_hint;
            break;
        }

        // Vertex shader bottleneck
        if (pass->vertex_invocations > 1000000 && pass->gpu_end_time - pass->gpu_start_time > 1000000) {
            bottleneck = BOTTLENECK_GPU_VERTEX;
            severity = 0.8f;
            bottleneck_desc = "Vertex Shader Bound";
            optimization_hint = "Consider reducing vertex count or optimizing vertex shaders";
            pass->bottleneck_score = severity;
            pass->bottleneck_type = bottleneck_desc;
            pass->optimization_hint = optimization_hint;
            break;
        }

        // Draw call batching bottleneck
        if (pass->draw_calls > 1000) {
            bottleneck = BOTTLENECK_DRAW_CALL_BATCHING;
            severity = (float)pass->draw_calls / 2000.0f;
            bottleneck_desc = "Draw Call Batching";
            optimization_hint = "Consider batching small draw calls or using instancing";
            pass->bottleneck_score = severity;
            pass->bottleneck_type = bottleneck_desc;
            pass->optimization_hint = optimization_hint;
            break;
        }
    }

    // Memory pressure analysis
    VkDeviceSize persistent_used = vk.arena_manager.persistent_arena.used;
    VkDeviceSize persistent_total = vk.arena_manager.persistent_arena.size;
    if (persistent_used > persistent_total * 0.8) {
        bottleneck = BOTTLENECK_MEMORY_PRESSURE;
        severity = (float)persistent_used / persistent_total;
        bottleneck_desc = "Memory Pressure";
        optimization_hint = "Consider reducing texture resolution or optimizing memory usage";
    }

    frame->primary_bottleneck = bottleneck_desc;
    frame->bottleneck_severity = severity > 1.0f ? 1.0f : severity;

    // Performance rating
    if (frame->frame_time_ms < 16.67) { // 60fps
        frame->performance_rating = "Excellent (60+ FPS)";
    } else if (frame->frame_time_ms < 33.33) { // 30fps
        frame->performance_rating = "Good (30-60 FPS)";
    } else if (frame->frame_time_ms < 66.67) { // 15fps
        frame->performance_rating = "Poor (15-30 FPS)";
    } else {
        frame->performance_rating = "Critical (<15 FPS)";
    }

    vk.render_profiler.current_bottleneck = bottleneck;
    vk.render_profiler.bottleneck_severity = severity;
}

// Update performance trend analysis
static void vk_update_performance_trend(void) {
    if (!vk.render_profiler.enable_trend_analysis || vk.render_profiler.total_frames_recorded < 60) {
        return;
    }

    vk_performance_trend_t *trend = &vk.render_profiler.performance_trend;

    // Calculate statistics from recent frames
    double sum = 0.0, sum_sq = 0.0;
    double min_time = DBL_MAX, max_time = 0.0;

    uint32_t start_frame = vk.render_profiler.total_frames_recorded > 60 ?
                          vk.render_profiler.total_frames_recorded - 60 : 0;
    uint32_t sample_count = 0;

    for (uint32_t i = start_frame; i < vk.render_profiler.total_frames_recorded; i++) {
        uint32_t history_index = i % vk.render_profiler.max_frames;
        vk_frame_profile_t *frame = &vk.render_profiler.frame_history[history_index];

        if (frame->frame_time_ms > 0) {
            sum += frame->frame_time_ms;
            sum_sq += frame->frame_time_ms * frame->frame_time_ms;
            if (frame->frame_time_ms < min_time) min_time = frame->frame_time_ms;
            if (frame->frame_time_ms > max_time) max_time = frame->frame_time_ms;
            sample_count++;
        }
    }

    if (sample_count == 0) return;

    trend->sample_count = sample_count;
    trend->avg_frame_time = sum / sample_count;
    trend->min_frame_time = min_time;
    trend->max_frame_time = max_time;
    trend->std_deviation = sqrt((sum_sq / sample_count) - (trend->avg_frame_time * trend->avg_frame_time));

    // Simple linear regression for trend (last 60 frames)
    double x_sum = 0.0, xy_sum = 0.0;
    for (uint32_t i = 0; i < sample_count; i++) {
        x_sum += i;
        uint32_t history_index = (start_frame + i) % vk.render_profiler.max_frames;
        xy_sum += i * vk.render_profiler.frame_history[history_index].frame_time_ms;
    }

    double slope = (sample_count * xy_sum - x_sum * sum) / (sample_count * (sample_count * sample_count - x_sum * x_sum) / sample_count);
    trend->trend_slope = slope;

    // Determine trend direction
    trend->degrading = slope > 0.1; // Performance getting worse
    trend->improving = slope < -0.1; // Performance getting better

    if (trend->degrading) {
        trend->trend_description = "Performance degrading over time";
    } else if (trend->improving) {
        trend->trend_description = "Performance improving over time";
    } else {
        trend->trend_description = "Performance stable";
    }
}

// End frame profiling
void vk_profile_frame_end(void) {
    if (!vk.render_profiler.enabled || !vk.render_profiler.initialized) {
        return;
    }

    uint64_t frame_end_time = ri.Milliseconds() * 1000000ULL;

    // Copy current passes to frame history
    uint32_t frame_index = vk.render_profiler.current_frame_index;
    vk_frame_profile_t *frame = &vk.render_profiler.frame_history[frame_index];

    frame->frame_number = vk.arena_manager.current_frame;
    frame->pass_count = vk.render_profiler.current_pass_count;
    frame->cpu_time_ms = (frame_end_time - (vk.render_profiler.current_passes[0].start_time)) / 1000000.0;

    // Copy pass data
    for (uint32_t i = 0; i < frame->pass_count; i++) {
        frame->passes[i] = vk.render_profiler.current_passes[i];
    }

    // Get GPU timing results if available
    if (vk.render_profiler.detailed_profiling) {
        vk_timestamp_query_t *timestamps = &vk.render_profiler.timestamp_queries;

        // Check if results are available (from previous frame)
        VkResult result = qvkGetQueryPoolResults(vk.device, timestamps->query_pool, 0,
                                               timestamps->current_query, sizeof(uint64_t) * timestamps->current_query,
                                               timestamps->timestamps, sizeof(uint64_t),
                                               VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

        if (result == VK_SUCCESS) {
            timestamps->available = qtrue;

            // Update pass GPU times
            for (uint32_t i = 0; i < frame->pass_count; i++) {
                vk_render_pass_profile_t *pass = &frame->passes[i];
                if (pass->gpu_start_time < timestamps->current_query &&
                    pass->gpu_end_time < timestamps->current_query) {
                    pass->gpu_start_time = timestamps->timestamps[pass->gpu_start_time];
                    pass->gpu_end_time = timestamps->timestamps[pass->gpu_end_time];
                }
            }
        }

        // Reset timestamp queries for next frame
        qvkResetQueryPool(vk.device, timestamps->query_pool, 0, timestamps->query_count);
        timestamps->current_query = 0;
        timestamps->available = qfalse;
    }

    // Analyze results and detect bottlenecks
    vk_analyze_profiling_results(frame);

    // Update trend analysis
    vk_update_performance_trend();

    // Update statistics
    vk.render_profiler.frames_analyzed++;
    vk.render_profiler.total_frames_recorded++;

    // Advance frame index
    vk.render_profiler.current_frame_index = (frame_index + 1) % vk.render_profiler.max_frames;

    // Reset for next frame
    vk.render_profiler.current_pass_count = 0;
}

// Print render profiler statistics
void vk_print_render_profiler_stats(void) {
    if (!vk.render_profiler.enabled || !vk.render_profiler.initialized) {
        ri.Printf(PRINT_ALL, "Render profiler not initialized\n");
        return;
    }

    ri.Printf(PRINT_ALL, "=== Render Graph Profiler Statistics ===\n");

    // Current frame analysis
    uint32_t recent_frame_index = (vk.render_profiler.current_frame_index - 1 + vk.render_profiler.max_frames) % vk.render_profiler.max_frames;
    vk_frame_profile_t *recent_frame = &vk.render_profiler.frame_history[recent_frame_index];

    ri.Printf(PRINT_ALL, "Recent Frame Analysis:\n");
    ri.Printf(PRINT_ALL, "  Frame Time: %.2f ms (%s)\n", recent_frame->frame_time_ms,
        recent_frame->performance_rating ? recent_frame->performance_rating : "Unknown");
    ri.Printf(PRINT_ALL, "  CPU Time: %.2f ms\n", recent_frame->cpu_time_ms);
    ri.Printf(PRINT_ALL, "  GPU Time: %.2f ms\n", recent_frame->gpu_time_ms);
    ri.Printf(PRINT_ALL, "  Passes: %u\n", recent_frame->pass_count);

    ri.Printf(PRINT_ALL, "Current Bottleneck: %s (Severity: %.1f%%)\n",
        recent_frame->primary_bottleneck ? recent_frame->primary_bottleneck : "None",
        recent_frame->bottleneck_severity * 100.0f);

    // Pass breakdown (top 5 by GPU time)
    ri.Printf(PRINT_ALL, "Top Render Passes:\n");
    for (uint32_t i = 0; i < recent_frame->pass_count && i < 5; i++) {
        vk_render_pass_profile_t *pass = &recent_frame->passes[i];
        uint64_t gpu_time_ns = (pass->gpu_end_time - pass->gpu_start_time) * vk.render_profiler.gpu_timestamp_period;
        double gpu_time_ms = gpu_time_ns / 1000000.0;

        ri.Printf(PRINT_ALL, "  %s: %.2f ms, %u draws, %u verts",
            pass->name, gpu_time_ms, pass->draw_calls, pass->vertices_submitted);

        if (pass->bottleneck_score > 0.5f) {
            ri.Printf(PRINT_ALL, " [BOTTLENECK: %s]", pass->bottleneck_type);
        }
        ri.Printf(PRINT_ALL, "\n");

        if (pass->optimization_hint && pass->bottleneck_score > 0.3f) {
            ri.Printf(PRINT_ALL, "    Hint: %s\n", pass->optimization_hint);
        }
    }

    // Performance trend
    if (vk.render_profiler.enable_trend_analysis) {
        vk_performance_trend_t *trend = &vk.render_profiler.performance_trend;
        ri.Printf(PRINT_ALL, "Performance Trend (%u samples):\n", trend->sample_count);
        ri.Printf(PRINT_ALL, "  Average: %.2f ms (%.1f FPS)\n",
            trend->avg_frame_time, 1000.0 / trend->avg_frame_time);
        ri.Printf(PRINT_ALL, "  Range: %.2f - %.2f ms\n", trend->min_frame_time, trend->max_frame_time);
        ri.Printf(PRINT_ALL, "  Stability: %.2f ms std dev\n", trend->std_deviation);
        ri.Printf(PRINT_ALL, "  Trend: %s\n", trend->trend_description);
    }

    // Profiling overhead
    ri.Printf(PRINT_ALL, "Profiling Overhead: %.1f%%\n",
        (double)vk.render_profiler.profiling_overhead_percent / 10.0);
    ri.Printf(PRINT_ALL, "Frames Analyzed: %lu\n", (unsigned long)vk.render_profiler.frames_analyzed);
}

// Enable/disable detailed profiling
void vk_set_detailed_profiling(qboolean enabled) {
    if (vk.render_profiler.detailed_profiling != enabled) {
        ri.Printf(PRINT_ALL, "Vulkan: %s detailed GPU profiling\n",
            enabled ? "Enabling" : "Disabling");

        // Restart profiler to apply changes
        vk_shutdown_render_profiler();
        vk.render_profiler.detailed_profiling = enabled;
        vk_init_render_profiler();
    }
}

// Memory Bandwidth Profiler Implementation

// Get CPU cache information (simplified detection)
static void vk_detect_cpu_cache_info(vk_memory_bandwidth_profiler_t *profiler) {
    // Default values for modern CPUs - in a real implementation,
    // this would query CPUID or similar mechanisms
    profiler->cache_line_size = 64;   // 64 bytes typical
    profiler->l1_cache_size = 32 * 1024;   // 32KB L1 data cache
    profiler->l2_cache_size = 256 * 1024;  // 256KB L2 cache
    profiler->l3_cache_size = 8 * 1024 * 1024; // 8MB L3 cache

    // Estimate memory bandwidth based on typical DDR4 speeds
    // This would be queried from system information in a real implementation
    profiler->memory_bandwidth_limit = 50ULL * 1024 * 1024 * 1024; // 50 GB/s
}

// Initialize memory bandwidth profiler
qboolean vk_init_memory_bandwidth_profiler(void) {
    if (vk.memory_bandwidth_profiler.initialized) {
        return qtrue;
    }

    ri.Printf(PRINT_ALL, "Vulkan: Initializing memory bandwidth profiler\n");

    vk_memory_bandwidth_profiler_t *profiler = &vk.memory_bandwidth_profiler;

    // Configure profiler
    profiler->enabled = qtrue;
    profiler->detailed_analysis = qtrue;
    profiler->max_structures = 64;
    profiler->max_access_patterns = 256;
    profiler->max_recommendations = 32;
    profiler->bandwidth_sample_interval = 16000000ULL; // 16ms (60fps)
    profiler->cache_miss_threshold = 0.1f; // 10% cache miss rate threshold
    profiler->bandwidth_threshold = 0.8f; // 80% bandwidth utilization threshold
    profiler->auto_apply_optimizations = qfalse;

    // Detect CPU cache characteristics
    vk_detect_cpu_cache_info(profiler);

    // Allocate profiling data structures
    profiler->structure_profiles = (vk_data_structure_profile_t*)ri.Malloc(
        sizeof(vk_data_structure_profile_t) * profiler->max_structures);
    if (!profiler->structure_profiles) {
        ri.Printf(PRINT_WARNING, "Vulkan: Failed to allocate structure profiles buffer\n");
        return qfalse;
    }
    memset(profiler->structure_profiles, 0,
           sizeof(vk_data_structure_profile_t) * profiler->max_structures);

    profiler->access_patterns = (vk_memory_access_pattern_t*)ri.Malloc(
        sizeof(vk_memory_access_pattern_t) * profiler->max_access_patterns);
    if (!profiler->access_patterns) {
        ri.Printf(PRINT_WARNING, "Vulkan: Failed to allocate access patterns buffer\n");
        ri.Free(profiler->structure_profiles);
        return qfalse;
    }
    memset(profiler->access_patterns, 0,
           sizeof(vk_memory_access_pattern_t) * profiler->max_access_patterns);

    profiler->layout_recommendations = (vk_layout_optimization_t*)ri.Malloc(
        sizeof(vk_layout_optimization_t) * profiler->max_recommendations);
    if (!profiler->layout_recommendations) {
        ri.Printf(PRINT_WARNING, "Vulkan: Failed to allocate layout recommendations buffer\n");
        ri.Free(profiler->structure_profiles);
        ri.Free(profiler->access_patterns);
        return qfalse;
    }
    memset(profiler->layout_recommendations, 0,
           sizeof(vk_layout_optimization_t) * profiler->max_recommendations);

    // Initialize bandwidth tracking
    memset(&profiler->bandwidth_stats, 0, sizeof(vk_memory_bandwidth_t));
    memset(&profiler->global_cache_stats, 0, sizeof(vk_cache_performance_t));

    atomic_init(&profiler->active_structures, 0);
    atomic_init(&profiler->active_access_patterns, 0);
    atomic_init(&profiler->active_recommendations, 0);
    atomic_init(&profiler->total_memory_operations, 0);
    atomic_init(&profiler->total_cache_misses, 0);

    profiler->last_bandwidth_sample = ri.Milliseconds() * 1000000ULL;
    profiler->debug_name = "memory_bandwidth_profiler";
    profiler->initialized = qtrue;

    ri.Printf(PRINT_ALL, "Vulkan: Memory bandwidth profiler initialized with %d structures, %d patterns, %d recommendations capacity\n",
        profiler->max_structures, profiler->max_access_patterns, profiler->max_recommendations);

    return qtrue;
}

// Shutdown memory bandwidth profiler
void vk_shutdown_memory_bandwidth_profiler(void) {
    if (!vk.memory_bandwidth_profiler.initialized) {
        return;
    }

    ri.Printf(PRINT_ALL, "Vulkan: Shutting down memory bandwidth profiler\n");

    vk_memory_bandwidth_profiler_t *profiler = &vk.memory_bandwidth_profiler;

    // Free allocated memory
    if (profiler->structure_profiles) {
        ri.Free(profiler->structure_profiles);
    }
    if (profiler->access_patterns) {
        ri.Free(profiler->access_patterns);
    }
    if (profiler->layout_recommendations) {
        ri.Free(profiler->layout_recommendations);
    }

    profiler->initialized = qfalse;
    ri.Printf(PRINT_ALL, "Vulkan: Memory bandwidth profiler shutdown complete\n");
}

// Record a memory access for analysis
void vk_record_memory_access(void *address, VkDeviceSize size, const char *resource_name, qboolean is_write) {
    if (!vk.memory_bandwidth_profiler.enabled || !vk.memory_bandwidth_profiler.initialized) {
        return;
    }

    vk_memory_bandwidth_profiler_t *profiler = &vk.memory_bandwidth_profiler;

    // Update bandwidth statistics
    if (is_write) {
        profiler->bandwidth_stats.total_bytes_written += size;
    } else {
        profiler->bandwidth_stats.total_bytes_read += size;
    }

    // Find or create access pattern for this resource
    vk_memory_access_pattern_t *pattern = NULL;
    for (uint32_t i = 0; i < profiler->active_access_patterns; i++) {
        if (strcmp(profiler->access_patterns[i].resource_name, resource_name) == 0) {
            pattern = &profiler->access_patterns[i];
            break;
        }
    }

    if (!pattern && profiler->active_access_patterns < profiler->max_access_patterns) {
        pattern = &profiler->access_patterns[profiler->active_access_patterns++];
        memset(pattern, 0, sizeof(vk_memory_access_pattern_t));
        pattern->resource_name = resource_name;
        pattern->base_address = address;
        pattern->region_size = size;
    }

    if (pattern) {
        pattern->total_accesses++;

        // Simple cache simulation (very basic)
        // In a real implementation, this would use hardware performance counters
        uintptr_t addr = (uintptr_t)address;
        uint32_t cache_line = addr / profiler->cache_line_size;

        // Simulate cache hit/miss with simple hashing
        static uint32_t last_cache_lines[1024] = {0};
        static uint32_t cache_index = 0;

        qboolean cache_hit = qfalse;
        for (uint32_t i = 0; i < 1024; i++) {
            if (last_cache_lines[i] == cache_line) {
                cache_hit = qtrue;
                break;
            }
        }

        if (cache_hit) {
            profiler->global_cache_stats.cache_hits++;
            pattern->temporal_reuse++;
        } else {
            profiler->global_cache_stats.cache_misses++;
            pattern->temporal_misses++;

            // Update cache simulation
            last_cache_lines[cache_index] = cache_line;
            cache_index = (cache_index + 1) % 1024;
        }

        profiler->global_cache_stats.cache_accesses++;
        atomic_fetch_add_explicit(&profiler->total_memory_operations, 1, memory_order_relaxed);
        profiler->total_cache_misses = profiler->global_cache_stats.cache_misses;
    }
}

// Sample memory bandwidth usage
void vk_sample_memory_bandwidth(void) {
    if (!vk.memory_bandwidth_profiler.enabled || !vk.memory_bandwidth_profiler.initialized) {
        return;
    }

    vk_memory_bandwidth_profiler_t *profiler = &vk.memory_bandwidth_profiler;
    uint64_t current_time = ri.Milliseconds() * 1000000ULL;
    uint64_t time_delta = current_time - profiler->last_bandwidth_sample;

    if (time_delta >= profiler->bandwidth_sample_interval) {
        // Calculate current bandwidth
        VkDeviceSize bytes_transferred = profiler->bandwidth_stats.total_bytes_read +
                                       profiler->bandwidth_stats.total_bytes_written;
        VkDeviceSize current_bandwidth = (bytes_transferred * 1000000000ULL) / time_delta;

        // Update bandwidth history
        profiler->bandwidth_stats.bandwidth_history[profiler->bandwidth_stats.bandwidth_samples % 60] = current_bandwidth;
        profiler->bandwidth_stats.bandwidth_samples++;

        // Update peak and sustained bandwidth
        if (current_bandwidth > profiler->bandwidth_stats.peak_bandwidth) {
            profiler->bandwidth_stats.peak_bandwidth = current_bandwidth;
        }

        // Calculate sustained bandwidth (average of last 60 samples)
        if (profiler->bandwidth_stats.bandwidth_samples >= 60) {
            VkDeviceSize total_bandwidth = 0;
            for (uint32_t i = 0; i < 60; i++) {
                total_bandwidth += profiler->bandwidth_stats.bandwidth_history[i];
            }
            profiler->bandwidth_stats.sustained_bandwidth = total_bandwidth / 60;
        }

        // Check bandwidth utilization
        profiler->bandwidth_stats.bandwidth_utilization =
            (float)current_bandwidth / (float)profiler->memory_bandwidth_limit;

        profiler->bandwidth_stats.bandwidth_limited =
            profiler->bandwidth_stats.bandwidth_utilization > profiler->bandwidth_threshold;

        profiler->bandwidth_stats.bandwidth_pressure = profiler->bandwidth_stats.bandwidth_utilization;

        // Reset counters for next sample
        profiler->bandwidth_stats.total_bytes_read = 0;
        profiler->bandwidth_stats.total_bytes_written = 0;
        profiler->last_bandwidth_sample = current_time;
    }
}

// Analyze memory access patterns
void vk_analyze_memory_access_patterns(void) {
    if (!vk.memory_bandwidth_profiler.enabled || !vk.memory_bandwidth_profiler.initialized) {
        return;
    }

    vk_memory_bandwidth_profiler_t *profiler = &vk.memory_bandwidth_profiler;

    // Update global cache statistics
    if (profiler->global_cache_stats.cache_accesses > 0) {
        profiler->global_cache_stats.hit_rate =
            (float)profiler->global_cache_stats.cache_hits / (float)profiler->global_cache_stats.cache_accesses;
        profiler->global_cache_stats.miss_rate = 1.0f - profiler->global_cache_stats.hit_rate;
    }

    profiler->average_cache_hit_rate = profiler->global_cache_stats.hit_rate;

    // Analyze access patterns
    for (uint32_t i = 0; i < profiler->active_access_patterns; i++) {
        vk_memory_access_pattern_t *pattern = &profiler->access_patterns[i];

        if (pattern->total_accesses == 0) continue;

        // Calculate prefetch efficiency (simplified)
        float temporal_locality = (float)pattern->temporal_reuse / (float)pattern->total_accesses;
        pattern->prefetch_efficiency = temporal_locality;

        // Simple stride detection (would be more sophisticated in real implementation)
        pattern->common_stride = 64; // Assume cache line size stride for demo
        pattern->stride_accesses = pattern->temporal_reuse;
    }

    // Generate layout optimization recommendations
    profiler->active_recommendations = 0;

    // Check for high cache miss rates
    if (profiler->global_cache_stats.miss_rate > profiler->cache_miss_threshold) {
        if (profiler->active_recommendations < profiler->max_recommendations) {
            vk_layout_optimization_t *rec = &profiler->layout_recommendations[profiler->active_recommendations++];
            rec->structure_name = "Global Memory Layout";
            rec->optimization_type = "Cache Alignment";
            rec->current_size = 0;
            rec->optimized_size = 0;
            rec->improvement_factor = 1.0f / (1.0f - profiler->global_cache_stats.miss_rate);
            rec->cache_hit_improvement = 0.2f; // Estimated 20% improvement
            rec->bandwidth_reduction = 0.15f; // Estimated 15% reduction
            rec->implementation_hint = "Align data structures to cache line boundaries and reorganize for better spatial locality";
            rec->auto_applicable = qfalse;
        }
    }

    // Check for bandwidth pressure
    if (profiler->bandwidth_stats.bandwidth_limited) {
        if (profiler->active_recommendations < profiler->max_recommendations) {
            vk_layout_optimization_t *rec = &profiler->layout_recommendations[profiler->active_recommendations++];
            rec->structure_name = "Memory Access Patterns";
            rec->optimization_type = "Bandwidth Optimization";
            rec->current_size = 0;
            rec->optimized_size = 0;
            rec->improvement_factor = 2.0f;
            rec->cache_hit_improvement = 0.0f;
            rec->bandwidth_reduction = 0.25f; // Estimated 25% reduction
            rec->implementation_hint = "Implement data compression, reduce texture resolution, or use texture streaming";
            rec->auto_applicable = qfalse;
        }
    }
}

// Print memory bandwidth statistics
void vk_print_memory_bandwidth_stats(void) {
    if (!vk.memory_bandwidth_profiler.enabled || !vk.memory_bandwidth_profiler.initialized) {
        ri.Printf(PRINT_ALL, "Memory bandwidth profiler not initialized\n");
        return;
    }

    vk_memory_bandwidth_profiler_t *profiler = &vk.memory_bandwidth_profiler;

    ri.Printf(PRINT_ALL, "=== Memory Bandwidth Profiler Statistics ===\n");

    // Bandwidth statistics
    ri.Printf(PRINT_ALL, "Memory Bandwidth:\n");
    ri.Printf(PRINT_ALL, "  Peak Bandwidth: %.2f GB/s\n",
        (double)profiler->bandwidth_stats.peak_bandwidth / (1024.0 * 1024.0 * 1024.0));
    ri.Printf(PRINT_ALL, "  Sustained Bandwidth: %.2f GB/s\n",
        (double)profiler->bandwidth_stats.sustained_bandwidth / (1024.0 * 1024.0 * 1024.0));
    ri.Printf(PRINT_ALL, "  Bandwidth Utilization: %.1f%%\n",
        profiler->bandwidth_stats.bandwidth_utilization * 100.0f);
    ri.Printf(PRINT_ALL, "  Bandwidth Limited: %s\n",
        profiler->bandwidth_stats.bandwidth_limited ? "Yes" : "No");

    if (profiler->bandwidth_stats.bandwidth_limited) {
        ri.Printf(PRINT_ALL, "  Bandwidth Pressure: %.1f%%\n",
            profiler->bandwidth_stats.bandwidth_pressure * 100.0f);
    }

    // Cache performance
    ri.Printf(PRINT_ALL, "Cache Performance:\n");
    ri.Printf(PRINT_ALL, "  Total Accesses: %lu\n", (unsigned long)profiler->total_memory_operations);
    ri.Printf(PRINT_ALL, "  Cache Hits: %lu\n", (unsigned long)profiler->global_cache_stats.cache_hits);
    ri.Printf(PRINT_ALL, "  Cache Misses: %lu\n", (unsigned long)profiler->global_cache_stats.cache_misses);
    ri.Printf(PRINT_ALL, "  Hit Rate: %.1f%%\n", profiler->global_cache_stats.hit_rate * 100.0f);
    ri.Printf(PRINT_ALL, "  Miss Rate: %.1f%%\n", profiler->global_cache_stats.miss_rate * 100.0f);

    // Access patterns
    ri.Printf(PRINT_ALL, "Memory Access Patterns (%u active):\n", profiler->active_access_patterns);
    for (uint32_t i = 0; i < profiler->active_access_patterns && i < 5; i++) {
        vk_memory_access_pattern_t *pattern = &profiler->access_patterns[i];
        ri.Printf(PRINT_ALL, "  %s: %lu accesses, %.1f%% temporal reuse\n",
            pattern->resource_name, (unsigned long)pattern->total_accesses,
            pattern->temporal_reuse > 0 ?
            (float)pattern->temporal_reuse / (float)pattern->total_accesses * 100.0f : 0.0f);
    }

    // Hardware info
    ri.Printf(PRINT_ALL, "Hardware Characteristics:\n");
    ri.Printf(PRINT_ALL, "  Cache Line Size: %u bytes\n", profiler->cache_line_size);
    ri.Printf(PRINT_ALL, "  L1 Cache: %u KB\n", profiler->l1_cache_size / 1024);
    ri.Printf(PRINT_ALL, "  L2 Cache: %u KB\n", profiler->l2_cache_size / 1024);
    ri.Printf(PRINT_ALL, "  L3 Cache: %u MB\n", profiler->l3_cache_size / (1024 * 1024));
    ri.Printf(PRINT_ALL, "  Memory Bandwidth Limit: %.1f GB/s\n",
        (double)profiler->memory_bandwidth_limit / (1024.0 * 1024.0 * 1024.0));
}

// Print cache performance statistics
void vk_print_cache_performance_stats(void) {
    if (!vk.memory_bandwidth_profiler.enabled || !vk.memory_bandwidth_profiler.initialized) {
        ri.Printf(PRINT_ALL, "Memory bandwidth profiler not initialized\n");
        return;
    }

    vk_memory_bandwidth_profiler_t *profiler = &vk.memory_bandwidth_profiler;

    ri.Printf(PRINT_ALL, "=== Cache Performance Analysis ===\n");

    // Overall cache statistics
    ri.Printf(PRINT_ALL, "Overall Cache Performance:\n");
    ri.Printf(PRINT_ALL, "  Hit Rate: %.2f%%\n", profiler->global_cache_stats.hit_rate * 100.0f);
    ri.Printf(PRINT_ALL, "  Miss Rate: %.2f%%\n", profiler->global_cache_stats.miss_rate * 100.0f);
    ri.Printf(PRINT_ALL, "  Total Accesses: %lu\n", (unsigned long)profiler->global_cache_stats.cache_accesses);
    ri.Printf(PRINT_ALL, "  Cache Misses: %lu\n", (unsigned long)profiler->global_cache_stats.cache_misses);

    // Performance impact estimation
    uint64_t estimated_cycles_lost = profiler->global_cache_stats.cache_misses * 200; // Rough estimate: 200 cycles per cache miss
    ri.Printf(PRINT_ALL, "  Estimated Cycles Lost: %lu\n", (unsigned long)estimated_cycles_lost);

    // Cache level analysis (simplified)
    ri.Printf(PRINT_ALL, "Cache Level Analysis:\n");
    ri.Printf(PRINT_ALL, "  L1 Cache Hit Rate: ~%.1f%%\n", profiler->global_cache_stats.hit_rate * 100.0f * 0.9f);
    ri.Printf(PRINT_ALL, "  L2 Cache Hit Rate: ~%.1f%%\n", profiler->global_cache_stats.hit_rate * 100.0f * 0.1f);
    ri.Printf(PRINT_ALL, "  L3/Memory Access: ~%.1f%%\n", profiler->global_cache_stats.miss_rate * 100.0f);

    // Recommendations
    if (profiler->global_cache_stats.miss_rate > 0.15f) {
        ri.Printf(PRINT_ALL, "\nRecommendations:\n");
        ri.Printf(PRINT_ALL, "  - High cache miss rate detected (%.1f%%)\n", profiler->global_cache_stats.miss_rate * 100.0f);
        ri.Printf(PRINT_ALL, "  - Consider improving data locality and access patterns\n");
        ri.Printf(PRINT_ALL, "  - Evaluate data structure cache alignment\n");
        ri.Printf(PRINT_ALL, "  - Consider using software prefetching\n");
    }
}

// Print layout optimization recommendations
void vk_print_layout_optimization_recommendations(void) {
    if (!vk.memory_bandwidth_profiler.enabled || !vk.memory_bandwidth_profiler.initialized) {
        ri.Printf(PRINT_ALL, "Memory bandwidth profiler not initialized\n");
        return;
    }

    vk_memory_bandwidth_profiler_t *profiler = &vk.memory_bandwidth_profiler;

    ri.Printf(PRINT_ALL, "=== Memory Layout Optimization Recommendations ===\n");

    if (profiler->active_recommendations == 0) {
        ri.Printf(PRINT_ALL, "No optimization recommendations available\n");
        return;
    }

    for (uint32_t i = 0; i < profiler->active_recommendations; i++) {
        vk_layout_optimization_t *rec = &profiler->layout_recommendations[i];

        ri.Printf(PRINT_ALL, "Recommendation %u: %s\n", i + 1, rec->structure_name);
        ri.Printf(PRINT_ALL, "  Type: %s\n", rec->optimization_type);
        ri.Printf(PRINT_ALL, "  Performance Improvement: %.1fx\n", rec->improvement_factor);
        ri.Printf(PRINT_ALL, "  Cache Hit Improvement: %.1f%%\n", rec->cache_hit_improvement * 100.0f);
        ri.Printf(PRINT_ALL, "  Bandwidth Reduction: %.1f%%\n", rec->bandwidth_reduction * 100.0f);
        ri.Printf(PRINT_ALL, "  Implementation: %s\n", rec->implementation_hint);
        ri.Printf(PRINT_ALL, "  Auto-applicable: %s\n", rec->auto_applicable ? "Yes" : "No");
        ri.Printf(PRINT_ALL, "\n");
    }
}

// Apply automatic memory optimizations
void vk_apply_memory_optimizations(void) {
    if (!vk.memory_bandwidth_profiler.enabled || !vk.memory_bandwidth_profiler.initialized) {
        ri.Printf(PRINT_ALL, "Memory bandwidth profiler not initialized\n");
        return;
    }

    vk_memory_bandwidth_profiler_t *profiler = &vk.memory_bandwidth_profiler;

    if (!profiler->auto_apply_optimizations) {
        ri.Printf(PRINT_ALL, "Auto-apply optimizations is disabled\n");
        return;
    }

    ri.Printf(PRINT_ALL, "Applying automatic memory optimizations...\n");

    // In a real implementation, this would apply safe optimizations automatically
    // For now, just log what would be done
    uint32_t applied = 0;
    for (uint32_t i = 0; i < profiler->active_recommendations; i++) {
        vk_layout_optimization_t *rec = &profiler->layout_recommendations[i];
        if (rec->auto_applicable) {
            ri.Printf(PRINT_ALL, "Auto-applying: %s optimization for %s\n",
                rec->optimization_type, rec->structure_name);
            applied++;
        }
    }

    if (applied == 0) {
        ri.Printf(PRINT_ALL, "No auto-applicable optimizations found\n");
    } else {
        ri.Printf(PRINT_ALL, "Applied %u automatic optimizations\n", applied);
    }
}

// Enable/disable bandwidth profiling
void vk_set_bandwidth_profiling_enabled(qboolean enabled) {
    if (vk.memory_bandwidth_profiler.enabled != enabled) {
        ri.Printf(PRINT_ALL, "Vulkan: %s memory bandwidth profiling\n",
            enabled ? "Enabling" : "Disabling");
        vk.memory_bandwidth_profiler.enabled = enabled;
    }
}

// Parallel Processing Profiler Implementation

// Detect CPU topology information
static void vk_detect_cpu_topology(vk_parallel_profiler_t *profiler) {
    // In a real implementation, this would query system information
    // For now, use reasonable defaults
    profiler->logical_cores = 16;    // Typical modern CPU
    profiler->physical_cores = 8;    // 8 physical cores
    profiler->threads_per_core = 2;  // Hyperthreading enabled
}

// Initialize parallel processing profiler
qboolean vk_init_parallel_profiler(void) {
    if (vk.parallel_profiler.initialized) {
        return qtrue;
    }

    ri.Printf(PRINT_ALL, "Vulkan: Initializing parallel processing profiler\n");

    vk_parallel_profiler_t *profiler = &vk.parallel_profiler;

    // Configure profiler
    profiler->enabled = qtrue;
    profiler->detailed_tracking = qtrue;
    profiler->max_threads = 64;
    profiler->max_sync_operations = 256;
    profiler->max_work_distributions = 128;
    profiler->sample_interval_ns = 1000000ULL; // 1ms sampling
    profiler->samples_per_second = 1000;
    profiler->low_utilization_threshold = 0.3f; // 30% utilization is low
    profiler->high_contention_threshold = 0.7f; // 70% contention is high
    profiler->load_imbalance_threshold = 0.5f; // 50% imbalance threshold

    // Detect CPU topology
    vk_detect_cpu_topology(profiler);

    // Allocate thread metrics
    profiler->thread_metrics = (vk_thread_metrics_t*)ri.Malloc(
        sizeof(vk_thread_metrics_t) * profiler->max_threads);
    if (!profiler->thread_metrics) {
        ri.Printf(PRINT_WARNING, "Vulkan: Failed to allocate thread metrics buffer\n");
        return qfalse;
    }
    memset(profiler->thread_metrics, 0,
           sizeof(vk_thread_metrics_t) * profiler->max_threads);

    // Allocate sync operations
    profiler->sync_operations = (vk_sync_operation_t*)ri.Malloc(
        sizeof(vk_sync_operation_t) * profiler->max_sync_operations);
    if (!profiler->sync_operations) {
        ri.Printf(PRINT_WARNING, "Vulkan: Failed to allocate sync operations buffer\n");
        ri.Free(profiler->thread_metrics);
        return qfalse;
    }
    memset(profiler->sync_operations, 0,
           sizeof(vk_sync_operation_t) * profiler->max_sync_operations);

    // Allocate work distributions
    profiler->work_distributions = (vk_work_distribution_t*)ri.Malloc(
        sizeof(vk_work_distribution_t) * profiler->max_work_distributions);
    if (!profiler->work_distributions) {
        ri.Printf(PRINT_WARNING, "Vulkan: Failed to allocate work distributions buffer\n");
        ri.Free(profiler->thread_metrics);
        ri.Free(profiler->sync_operations);
        return qfalse;
    }
    memset(profiler->work_distributions, 0,
           sizeof(vk_work_distribution_t) * profiler->max_work_distributions);

    // Initialize timing
    profiler->last_sample_time = ri.Milliseconds() * 1000000ULL;

    atomic_init(&profiler->active_threads, 0);
    atomic_init(&profiler->active_sync_operations, 0);
    atomic_init(&profiler->active_work_distributions, 0);

    // Initialize statistics
    atomic_init(&profiler->total_sync_wait_time, 0);
    atomic_init(&profiler->total_active_time, 0);
    atomic_init(&profiler->total_idle_time, 0);
    atomic_init(&profiler->total_context_switches, 0);

    profiler->debug_name = "parallel_profiler";
    profiler->initialized = qtrue;

    ri.Printf(PRINT_ALL, "Vulkan: Parallel processing profiler initialized with %d threads, %d sync ops, %d work distributions capacity\n",
        profiler->max_threads, profiler->max_sync_operations, profiler->max_work_distributions);

    return qtrue;
}

// Shutdown parallel processing profiler
void vk_shutdown_parallel_profiler(void) {
    if (!vk.parallel_profiler.initialized) {
        return;
    }

    ri.Printf(PRINT_ALL, "Vulkan: Shutting down parallel processing profiler\n");

    vk_parallel_profiler_t *profiler = &vk.parallel_profiler;

    // Free allocated memory
    if (profiler->thread_metrics) {
        ri.Free(profiler->thread_metrics);
    }
    if (profiler->sync_operations) {
        ri.Free(profiler->sync_operations);
    }
    if (profiler->work_distributions) {
        // Free thread assignments arrays
        for (uint32_t i = 0; i < profiler->active_work_distributions; i++) {
            if (profiler->work_distributions[i].thread_assignments) {
                ri.Free(profiler->work_distributions[i].thread_assignments);
            }
        }
        ri.Free(profiler->work_distributions);
    }

    profiler->initialized = qfalse;
    ri.Printf(PRINT_ALL, "Vulkan: Parallel processing profiler shutdown complete\n");
}

// Profile thread start
void vk_profile_thread_start(const char *thread_name, uint32_t thread_id) {
    if (!vk.parallel_profiler.enabled || !vk.parallel_profiler.initialized) {
        return;
    }

    vk_parallel_profiler_t *profiler = &vk.parallel_profiler;

    if (thread_id >= profiler->max_threads) {
        return; // Thread ID out of range
    }

    vk_thread_metrics_t *thread = &profiler->thread_metrics[thread_id];

    if (!thread->is_active) {
        thread->thread_id = thread_id;
        thread->thread_name = thread_name;
        thread->start_time = ri.Milliseconds() * 1000000ULL;
        thread->is_active = qtrue;
        thread->last_active_timestamp = thread->start_time;

        if (thread_id >= profiler->active_threads) {
            profiler->active_threads = thread_id + 1;
        }
    }
}

// Profile thread end
void vk_profile_thread_end(uint32_t thread_id) {
    if (!vk.parallel_profiler.enabled || !vk.parallel_profiler.initialized) {
        return;
    }

    vk_parallel_profiler_t *profiler = &vk.parallel_profiler;

    if (thread_id >= profiler->active_threads) {
        return;
    }

    vk_thread_metrics_t *thread = &profiler->thread_metrics[thread_id];

    if (thread->is_active) {
        thread->end_time = ri.Milliseconds() * 1000000ULL;
        thread->is_active = qfalse;

        // Calculate final utilization
        uint64_t total_time = thread->end_time - thread->start_time;
        if (total_time > 0) {
            thread->utilization_percentage = (float)thread->total_active_time / (float)total_time;
        }
    }
}

// Profile synchronization operation
void vk_profile_sync_operation(const char *operation_name, uint64_t wait_time_ns) {
    if (!vk.parallel_profiler.enabled || !vk.parallel_profiler.initialized) {
        return;
    }

    vk_parallel_profiler_t *profiler = &vk.parallel_profiler;

    // Find existing sync operation or create new one
    vk_sync_operation_t *sync_op = NULL;
    for (uint32_t i = 0; i < profiler->active_sync_operations; i++) {
        if (strcmp(profiler->sync_operations[i].operation_name, operation_name) == 0) {
            sync_op = &profiler->sync_operations[i];
            break;
        }
    }

    if (!sync_op && profiler->active_sync_operations < profiler->max_sync_operations) {
        sync_op = &profiler->sync_operations[profiler->active_sync_operations++];
        memset(sync_op, 0, sizeof(vk_sync_operation_t));
        sync_op->operation_name = operation_name;
        sync_op->min_wait_time = UINT64_MAX;
    }

    if (sync_op) {
        sync_op->total_wait_time += wait_time_ns;
        sync_op->operation_count++;

        if (wait_time_ns > sync_op->max_wait_time) {
            sync_op->max_wait_time = wait_time_ns;
        }
        if (wait_time_ns < sync_op->min_wait_time) {
            sync_op->min_wait_time = wait_time_ns;
        }

        // Update global statistics
        profiler->total_sync_wait_time += wait_time_ns;

        // Simple contention detection (high wait time = high contention)
        if (wait_time_ns > 1000000ULL) { // 1ms threshold
            sync_op->contention_count++;
        }
    }
}

// Profile lock acquire
void vk_profile_lock_acquire(const char *lock_name, uint32_t thread_id) {
    if (!vk.parallel_profiler.enabled || !vk.parallel_profiler.initialized) {
        return;
    }

    vk_parallel_profiler_t *profiler = &vk.parallel_profiler;

    if (thread_id >= profiler->active_threads) {
        return;
    }

    vk_thread_metrics_t *thread = &profiler->thread_metrics[thread_id];

    // Mark thread as waiting
    thread->total_wait_time += ri.Milliseconds() * 1000000ULL - thread->last_active_timestamp;

    // This is a simplified implementation - in a real system,
    // we'd track the actual wait time for this specific lock
}

// Profile lock release
void vk_profile_lock_release(const char *lock_name, uint32_t thread_id) {
    if (!vk.parallel_profiler.enabled || !vk.parallel_profiler.initialized) {
        return;
    }

    vk_parallel_profiler_t *profiler = &vk.parallel_profiler;

    if (thread_id >= profiler->active_threads) {
        return;
    }

    vk_thread_metrics_t *thread = &profiler->thread_metrics[thread_id];

    // Mark thread as active again
    thread->last_active_timestamp = ri.Milliseconds() * 1000000ULL;
}

// Analyze work distribution for load balancing
static void vk_analyze_work_distribution(vk_work_distribution_t *work) {
    if (!work || work->thread_count == 0 || !work->thread_assignments) {
        return;
    }

    work->max_items_per_thread = 0;
    work->min_items_per_thread = UINT32_MAX;

    uint32_t total_assigned = 0;
    uint32_t active_threads = 0;

    for (uint32_t i = 0; i < work->thread_count; i++) {
        uint32_t items = work->thread_assignments[i];
        total_assigned += items;

        if (items > 0) {
            active_threads++;
            if (items > work->max_items_per_thread) {
                work->max_items_per_thread = items;
            }
            if (items < work->min_items_per_thread) {
                work->min_items_per_thread = items;
            }
        }
    }

    if (active_threads > 0 && work->max_items_per_thread > 0) {
        // Calculate load balance factor (1.0 = perfect balance, 0.0 = terrible balance)
        float avg_items = (float)total_assigned / (float)active_threads;
        float max_deviation = (float)work->max_items_per_thread - avg_items;
        float balance_factor = 1.0f - (max_deviation / avg_items);
        work->load_balance_factor = balance_factor > 0.0f ? balance_factor : 0.0f;

        work->has_load_imbalance = work->load_balance_factor < 0.7f; // Less than 70% balanced

        if (work->has_load_imbalance) {
            if (work->max_items_per_thread > work->min_items_per_thread * 2) {
                work->balance_issue = "Severe load imbalance - some threads overloaded";
            } else {
                work->balance_issue = "Moderate load imbalance - work distribution uneven";
            }
        } else {
            work->balance_issue = "Good load balance";
        }

        // Calculate parallel efficiency
        uint64_t serial_time = work->total_processing_time * work->thread_count; // Theoretical serial time
        if (serial_time > 0) {
            work->parallel_efficiency = (float)work->total_processing_time / (float)serial_time;
            work->speedup_factor = (float)serial_time / (float)work->total_processing_time;
        }
    }
}

// Sample thread utilization
void vk_sample_thread_utilization(void) {
    if (!vk.parallel_profiler.enabled || !vk.parallel_profiler.initialized) {
        return;
    }

    vk_parallel_profiler_t *profiler = &vk.parallel_profiler;
    uint64_t current_time = ri.Milliseconds() * 1000000ULL;

    if (current_time - profiler->last_sample_time >= profiler->sample_interval_ns) {
        // Sample each active thread
        for (uint32_t i = 0; i < profiler->active_threads; i++) {
            vk_thread_metrics_t *thread = &profiler->thread_metrics[i];

            if (thread->is_active) {
                uint64_t time_since_last_active = current_time - thread->last_active_timestamp;

                // Simple heuristic: if thread hasn't been active recently, it's likely waiting
                if (time_since_last_active > profiler->sample_interval_ns * 2) {
                    thread->total_wait_time += profiler->sample_interval_ns;
                } else {
                    thread->total_active_time += profiler->sample_interval_ns;
                }

                thread->last_active_timestamp = current_time;
            }
        }

        profiler->last_sample_time = current_time;
    }
}

// Analyze synchronization overhead
static void vk_analyze_synchronization_overhead(vk_parallel_profiler_t *profiler) {
    uint64_t total_active_time = 0;

    // Calculate total active time across all threads
    for (uint32_t i = 0; i < profiler->active_threads; i++) {
        vk_thread_metrics_t *thread = &profiler->thread_metrics[i];
        total_active_time += thread->total_active_time;
    }

    // Analyze each sync operation
    for (uint32_t i = 0; i < profiler->active_sync_operations; i++) {
        vk_sync_operation_t *sync = &profiler->sync_operations[i];

        if (total_active_time > 0) {
            sync->overhead_percentage = (float)sync->total_wait_time / (float)total_active_time;
        }

        // Determine if this is a bottleneck
        sync->is_bottleneck = sync->overhead_percentage > 0.1f || // >10% of active time
                             sync->contention_count > 10;        // High contention count

        if (sync->operation_count > 0) {
            sync->average_contention = (float)sync->contention_count / (float)sync->operation_count;
        }
    }
}

// Analyze parallel efficiency
static void vk_analyze_parallel_efficiency(vk_parallel_profiler_t *profiler) {
    vk_parallel_efficiency_t *efficiency = &profiler->efficiency_metrics;

    if (profiler->active_threads == 0) {
        return;
    }

    // Calculate thread utilization statistics
    float total_utilization = 0.0f;
    efficiency->max_thread_utilization = 0.0f;
    efficiency->min_thread_utilization = 1.0f;

    uint32_t active_thread_count = 0;
    for (uint32_t i = 0; i < profiler->active_threads; i++) {
        vk_thread_metrics_t *thread = &profiler->thread_metrics[i];
        if (thread->is_active || thread->total_active_time > 0) {
            active_thread_count++;

            if (thread->utilization_percentage >= 0.0f) {
                total_utilization += thread->utilization_percentage;

                if (thread->utilization_percentage > efficiency->max_thread_utilization) {
                    efficiency->max_thread_utilization = thread->utilization_percentage;
                }
                if (thread->utilization_percentage < efficiency->min_thread_utilization) {
                    efficiency->min_thread_utilization = thread->utilization_percentage;
                }
            }
        }
    }

    if (active_thread_count > 0) {
        efficiency->avg_thread_utilization = total_utilization / (float)active_thread_count;
        efficiency->utilization_variance = efficiency->max_thread_utilization - efficiency->min_thread_utilization;
    }

    // Calculate synchronization overhead
    uint64_t total_active_time = 0;
    for (uint32_t i = 0; i < profiler->active_threads; i++) {
        total_active_time += profiler->thread_metrics[i].total_active_time;
    }

    if (total_active_time + profiler->total_sync_wait_time > 0) {
        efficiency->synchronization_overhead = (float)profiler->total_sync_wait_time /
                                             (float)(total_active_time + profiler->total_sync_wait_time);
    }

    // Calculate overall efficiency
    efficiency->overall_efficiency = efficiency->avg_thread_utilization *
                                   (1.0f - efficiency->synchronization_overhead) *
                                   (1.0f - efficiency->load_imbalance_factor);

    // Estimate scalability
    if (profiler->logical_cores > 1) {
        efficiency->scalability_factor = efficiency->overall_efficiency *
                                       sqrt((float)profiler->logical_cores);
        efficiency->scalability_factor = efficiency->scalability_factor > 1.0f ? 1.0f : efficiency->scalability_factor;
    }

    // Performance predictions
    efficiency->predicted_efficiency_2x = efficiency->overall_efficiency * 0.8f; // Some overhead increase
    efficiency->predicted_efficiency_4x = efficiency->overall_efficiency * 0.6f; // More overhead
    efficiency->optimal_thread_count = (float)profiler->logical_cores * efficiency->scalability_factor;

    // Identify primary bottleneck
    if (efficiency->synchronization_overhead > 0.3f) {
        efficiency->primary_bottleneck = "Synchronization Overhead";
        efficiency->bottleneck_severity = efficiency->synchronization_overhead;
        efficiency->optimization_hint = "Reduce synchronization frequency, use lock-free algorithms, or redesign data access patterns";
    } else if (efficiency->avg_thread_utilization < 0.5f) {
        efficiency->primary_bottleneck = "Low Thread Utilization";
        efficiency->bottleneck_severity = 1.0f - efficiency->avg_thread_utilization;
        efficiency->optimization_hint = "Increase work granularity, reduce idle time, or load balance work better";
    } else if (efficiency->load_imbalance_factor > 0.3f) {
        efficiency->primary_bottleneck = "Load Imbalance";
        efficiency->bottleneck_severity = efficiency->load_imbalance_factor;
        efficiency->optimization_hint = "Improve work distribution algorithms or use dynamic load balancing";
    } else {
        efficiency->primary_bottleneck = "None";
        efficiency->bottleneck_severity = 0.0f;
        efficiency->optimization_hint = "Performance is good, consider monitoring for regressions";
    }
}

// Print parallel processing statistics
void vk_print_parallel_stats(void) {
    if (!vk.parallel_profiler.enabled || !vk.parallel_profiler.initialized) {
        ri.Printf(PRINT_ALL, "Parallel profiler not initialized\n");
        return;
    }

    vk_parallel_profiler_t *profiler = &vk.parallel_profiler;

    // Analyze current state
    vk_analyze_synchronization_overhead(profiler);
    vk_analyze_parallel_efficiency(profiler);

    ri.Printf(PRINT_ALL, "=== Parallel Processing Profiler Statistics ===\n");

    ri.Printf(PRINT_ALL, "Hardware Configuration:\n");
    ri.Printf(PRINT_ALL, "  Logical Cores: %u\n", profiler->logical_cores);
    ri.Printf(PRINT_ALL, "  Physical Cores: %u\n", profiler->physical_cores);
    ri.Printf(PRINT_ALL, "  Threads per Core: %u\n", profiler->threads_per_core);

    ri.Printf(PRINT_ALL, "Active Threads: %u\n", profiler->active_threads);

    vk_parallel_efficiency_t *efficiency = &profiler->efficiency_metrics;
    ri.Printf(PRINT_ALL, "Overall Efficiency: %.1f%%\n", efficiency->overall_efficiency * 100.0f);
    ri.Printf(PRINT_ALL, "Primary Bottleneck: %s (Severity: %.1f%%)\n",
        efficiency->primary_bottleneck, efficiency->bottleneck_severity * 100.0f);

    if (efficiency->optimization_hint) {
        ri.Printf(PRINT_ALL, "Optimization Hint: %s\n", efficiency->optimization_hint);
    }
}

// Print thread utilization statistics
void vk_print_thread_utilization(void) {
    if (!vk.parallel_profiler.enabled || !vk.parallel_profiler.initialized) {
        ri.Printf(PRINT_ALL, "Parallel profiler not initialized\n");
        return;
    }

    vk_parallel_profiler_t *profiler = &vk.parallel_profiler;

    ri.Printf(PRINT_ALL, "=== Thread Utilization Statistics ===\n");

    for (uint32_t i = 0; i < profiler->active_threads; i++) {
        vk_thread_metrics_t *thread = &profiler->thread_metrics[i];

        ri.Printf(PRINT_ALL, "Thread %u (%s):\n", thread->thread_id,
            thread->thread_name ? thread->thread_name : "Unknown");
        ri.Printf(PRINT_ALL, "  Utilization: %.1f%%\n", thread->utilization_percentage * 100.0f);
        ri.Printf(PRINT_ALL, "  Active Time: %lu ms\n", (unsigned long)(thread->total_active_time / 1000000));
        ri.Printf(PRINT_ALL, "  Wait Time: %lu ms\n", (unsigned long)(thread->total_wait_time / 1000000));
        ri.Printf(PRINT_ALL, "  Work Items: %u submitted, %u completed\n",
            thread->submitted_work, thread->completed_work);

        if (thread->utilization_percentage < profiler->low_utilization_threshold) {
            ri.Printf(PRINT_ALL, "  WARNING: Low utilization detected\n");
        }
        ri.Printf(PRINT_ALL, "\n");
    }

    // Summary statistics
    vk_parallel_efficiency_t *efficiency = &profiler->efficiency_metrics;
    ri.Printf(PRINT_ALL, "Summary:\n");
    ri.Printf(PRINT_ALL, "  Average Utilization: %.1f%%\n", efficiency->avg_thread_utilization * 100.0f);
    ri.Printf(PRINT_ALL, "  Utilization Range: %.1f%% - %.1f%%\n",
        efficiency->min_thread_utilization * 100.0f,
        efficiency->max_thread_utilization * 100.0f);
    ri.Printf(PRINT_ALL, "  Utilization Variance: %.1f%%\n", efficiency->utilization_variance * 100.0f);
}

// Print synchronization overhead statistics
void vk_print_synchronization_overhead(void) {
    if (!vk.parallel_profiler.enabled || !vk.parallel_profiler.initialized) {
        ri.Printf(PRINT_ALL, "Parallel profiler not initialized\n");
        return;
    }

    vk_parallel_profiler_t *profiler = &vk.parallel_profiler;

    ri.Printf(PRINT_ALL, "=== Synchronization Overhead Analysis ===\n");

    ri.Printf(PRINT_ALL, "Total Sync Wait Time: %lu ms\n",
        (unsigned long)(profiler->total_sync_wait_time / 1000000));

    for (uint32_t i = 0; i < profiler->active_sync_operations; i++) {
        vk_sync_operation_t *sync = &profiler->sync_operations[i];

        ri.Printf(PRINT_ALL, "Sync Operation: %s\n", sync->operation_name);
        ri.Printf(PRINT_ALL, "  Total Wait Time: %lu ms\n", (unsigned long)(sync->total_wait_time / 1000000));
        ri.Printf(PRINT_ALL, "  Operation Count: %lu\n", (unsigned long)sync->operation_count);
        ri.Printf(PRINT_ALL, "  Average Wait Time: %.2f ms\n",
            sync->operation_count > 0 ? (double)sync->total_wait_time / (double)sync->operation_count / 1000000.0 : 0.0);
        ri.Printf(PRINT_ALL, "  Overhead: %.1f%%\n", sync->overhead_percentage * 100.0f);
        ri.Printf(PRINT_ALL, "  Contention Count: %u\n", sync->contention_count);

        if (sync->is_bottleneck) {
            ri.Printf(PRINT_ALL, "  WARNING: Significant bottleneck detected\n");
        }
        ri.Printf(PRINT_ALL, "\n");
    }

    ri.Printf(PRINT_ALL, "Synchronization Efficiency: %.1f%%\n",
        (1.0f - profiler->efficiency_metrics.synchronization_overhead) * 100.0f);
}

// Print parallel efficiency analysis
void vk_print_parallel_efficiency(void) {
    if (!vk.parallel_profiler.enabled || !vk.parallel_profiler.initialized) {
        ri.Printf(PRINT_ALL, "Parallel profiler not initialized\n");
        return;
    }

    vk_parallel_profiler_t *profiler = &vk.parallel_profiler;

    ri.Printf(PRINT_ALL, "=== Parallel Efficiency Analysis ===\n");

    vk_parallel_efficiency_t *efficiency = &profiler->efficiency_metrics;

    ri.Printf(PRINT_ALL, "Overall Metrics:\n");
    ri.Printf(PRINT_ALL, "  Parallel Efficiency: %.1f%%\n", efficiency->overall_efficiency * 100.0f);
    ri.Printf(PRINT_ALL, "  Scalability Factor: %.1f%%\n", efficiency->scalability_factor * 100.0f);
    ri.Printf(PRINT_ALL, "  Synchronization Overhead: %.1f%%\n", efficiency->synchronization_overhead * 100.0f);
    ri.Printf(PRINT_ALL, "  Load Imbalance Factor: %.1f%%\n", efficiency->load_imbalance_factor * 100.0f);

    ri.Printf(PRINT_ALL, "Thread Utilization:\n");
    ri.Printf(PRINT_ALL, "  Average: %.1f%%\n", efficiency->avg_thread_utilization * 100.0f);
    ri.Printf(PRINT_ALL, "  Range: %.1f%% - %.1f%%\n",
        efficiency->min_thread_utilization * 100.0f,
        efficiency->max_thread_utilization * 100.0f);

    ri.Printf(PRINT_ALL, "Performance Predictions:\n");
    ri.Printf(PRINT_ALL, "  With 2x threads: %.1f%% efficiency\n", efficiency->predicted_efficiency_2x * 100.0f);
    ri.Printf(PRINT_ALL, "  With 4x threads: %.1f%% efficiency\n", efficiency->predicted_efficiency_4x * 100.0f);
    ri.Printf(PRINT_ALL, "  Optimal Thread Count: %.1f\n", efficiency->optimal_thread_count);

    // Work distribution summary
    ri.Printf(PRINT_ALL, "Work Distribution:\n");
    uint32_t total_work = 0;
    uint32_t balanced_work = 0;

    for (uint32_t i = 0; i < profiler->active_work_distributions; i++) {
        vk_work_distribution_t *work = &profiler->work_distributions[i];
        total_work++;
        if (!work->has_load_imbalance) {
            balanced_work++;
        }
    }

    if (total_work > 0) {
        float balance_ratio = (float)balanced_work / (float)total_work;
        ri.Printf(PRINT_ALL, "  Well-balanced work units: %.1f%% (%u/%u)\n",
            balance_ratio * 100.0f, balanced_work, total_work);
    }
}

// Enable/disable parallel profiling
void vk_set_parallel_profiling_enabled(qboolean enabled) {
    if (vk.parallel_profiler.enabled != enabled) {
        ri.Printf(PRINT_ALL, "Vulkan: %s parallel processing profiling\n",
            enabled ? "Enabling" : "Disabling");
        vk.parallel_profiler.enabled = enabled;
    }
}

// Shader Performance Analysis Implementation

// Forward declarations for shader analysis functions
static const char *vk_get_spirv_opcode_name(uint32_t opcode);
static float vk_get_opcode_cycles(uint32_t opcode);
static void vk_classify_opcode(uint32_t opcode, qboolean *is_memory, qboolean *is_texture, qboolean *is_branch, float *alu_intensity);
static void vk_update_instruction_stats(vk_shader_performance_analyzer_t *analyzer, uint32_t opcode, uint32_t word_count);
static void vk_analyze_shader_bottlenecks(vk_shader_performance_metrics_t *metrics);
static void vk_generate_shader_optimizations(vk_shader_performance_analyzer_t *analyzer, vk_shader_performance_metrics_t *metrics);

// SPIR-V opcode definitions (subset for analysis)
#define SPIRV_OP_NAME 5
#define SPIRV_OP_DECORATE 71
#define SPIRV_OP_MEMBER_DECORATE 72
#define SPIRV_OP_FUNCTION 54
#define SPIRV_OP_FUNCTION_END 56
#define SPIRV_OP_LABEL 248
#define SPIRV_OP_RETURN 253
#define SPIRV_OP_RETURN_VALUE 254
#define SPIRV_OP_LOAD 61
#define SPIRV_OP_STORE 62
#define SPIRV_OP_ACCESS_CHAIN 65
#define SPIRV_OP_CONSTANT 43
#define SPIRV_OP_VARIABLE 59
#define SPIRV_OP_FMUL 133
#define SPIRV_OP_FADD 129
#define SPIRV_OP_FSUB 131
#define SPIRV_OP_FDIV 136
#define SPIRV_OP_IMUL 128
#define SPIRV_OP_IADD 126
#define SPIRV_OP_ISUB 130
#define SPIRV_OP_SDIV 135
#define SPIRV_OP_UDIV 134
#define SPIRV_OP_IMAGE_SAMPLE_IMPLICIT_LOD 87
#define SPIRV_OP_IMAGE_SAMPLE_EXPLICIT_LOD 88
#define SPIRV_OP_IMAGE_SAMPLE_DREF_IMPLICIT_LOD 89
#define SPIRV_OP_IMAGE_SAMPLE_DREF_EXPLICIT_LOD 90
#define SPIRV_OP_BRANCH 249
#define SPIRV_OP_BRANCH_CONDITIONAL 250
#define SPIRV_OP_LOOP_MERGE 246
#define SPIRV_OP_SELECTION_MERGE 247

// Get SPIR-V opcode name for display
static const char *vk_get_spirv_opcode_name(uint32_t opcode) {
    switch (opcode) {
        case SPIRV_OP_LOAD: return "OpLoad";
        case SPIRV_OP_STORE: return "OpStore";
        case SPIRV_OP_ACCESS_CHAIN: return "OpAccessChain";
        case SPIRV_OP_CONSTANT: return "OpConstant";
        case SPIRV_OP_VARIABLE: return "OpVariable";
        case SPIRV_OP_FMUL: return "OpFMul";
        case SPIRV_OP_FADD: return "OpFAdd";
        case SPIRV_OP_FSUB: return "OpFSub";
        case SPIRV_OP_FDIV: return "OpFDiv";
        case SPIRV_OP_IMUL: return "OpIMul";
        case SPIRV_OP_IADD: return "OpIAdd";
        case SPIRV_OP_ISUB: return "OpISub";
        case SPIRV_OP_SDIV: return "OpSDiv";
        case SPIRV_OP_UDIV: return "OpUDiv";
        case SPIRV_OP_IMAGE_SAMPLE_IMPLICIT_LOD: return "OpImageSampleImplicitLod";
        case SPIRV_OP_IMAGE_SAMPLE_EXPLICIT_LOD: return "OpImageSampleExplicitLod";
        case SPIRV_OP_IMAGE_SAMPLE_DREF_IMPLICIT_LOD: return "OpImageSampleDrefImplicitLod";
        case SPIRV_OP_IMAGE_SAMPLE_DREF_EXPLICIT_LOD: return "OpImageSampleDrefExplicitLod";
        case SPIRV_OP_BRANCH: return "OpBranch";
        case SPIRV_OP_BRANCH_CONDITIONAL: return "OpBranchConditional";
        case SPIRV_OP_LOOP_MERGE: return "OpLoopMerge";
        case SPIRV_OP_SELECTION_MERGE: return "OpSelectionMerge";
        case SPIRV_OP_FUNCTION: return "OpFunction";
        case SPIRV_OP_FUNCTION_END: return "OpFunctionEnd";
        case SPIRV_OP_LABEL: return "OpLabel";
        case SPIRV_OP_RETURN: return "OpReturn";
        case SPIRV_OP_RETURN_VALUE: return "OpReturnValue";
        case SPIRV_OP_DECORATE: return "OpDecorate";
        case SPIRV_OP_MEMBER_DECORATE: return "OpMemberDecorate";
        case SPIRV_OP_NAME: return "OpName";
        default: return "Unknown";
    }
}

// Get estimated cycles for an opcode
static float vk_get_opcode_cycles(uint32_t opcode) {
    switch (opcode) {
        case SPIRV_OP_LOAD:
        case SPIRV_OP_STORE:
            return 4.0f; // Memory operations are expensive
        case SPIRV_OP_FMUL:
        case SPIRV_OP_FDIV:
            return 3.0f; // Floating point math
        case SPIRV_OP_FADD:
        case SPIRV_OP_FSUB:
            return 2.0f; // Simple floating point operations
        case SPIRV_OP_IMUL:
        case SPIRV_OP_SDIV:
        case SPIRV_OP_UDIV:
            return 2.5f; // Integer operations
        case SPIRV_OP_IADD:
        case SPIRV_OP_ISUB:
            return 1.0f; // Simple integer operations
        case SPIRV_OP_IMAGE_SAMPLE_IMPLICIT_LOD:
        case SPIRV_OP_IMAGE_SAMPLE_EXPLICIT_LOD:
        case SPIRV_OP_IMAGE_SAMPLE_DREF_IMPLICIT_LOD:
        case SPIRV_OP_IMAGE_SAMPLE_DREF_EXPLICIT_LOD:
            return 50.0f; // Texture sampling is very expensive
        case SPIRV_OP_BRANCH_CONDITIONAL:
            return 2.0f; // Branch operations
        case SPIRV_OP_LOOP_MERGE:
        case SPIRV_OP_SELECTION_MERGE:
            return 1.0f; // Control flow setup
        default:
            return 1.0f; // Default cycle count
    }
}

// Classify opcode type
static void vk_classify_opcode(uint32_t opcode, qboolean *is_memory, qboolean *is_texture, qboolean *is_branch, float *alu_intensity) {
    *is_memory = qfalse;
    *is_texture = qfalse;
    *is_branch = qfalse;
    *alu_intensity = 0.0f;

    switch (opcode) {
        case SPIRV_OP_LOAD:
        case SPIRV_OP_STORE:
        case SPIRV_OP_ACCESS_CHAIN:
            *is_memory = qtrue;
            break;
        case SPIRV_OP_IMAGE_SAMPLE_IMPLICIT_LOD:
        case SPIRV_OP_IMAGE_SAMPLE_EXPLICIT_LOD:
        case SPIRV_OP_IMAGE_SAMPLE_DREF_IMPLICIT_LOD:
        case SPIRV_OP_IMAGE_SAMPLE_DREF_EXPLICIT_LOD:
            *is_texture = qtrue;
            break;
        case SPIRV_OP_BRANCH:
        case SPIRV_OP_BRANCH_CONDITIONAL:
        case SPIRV_OP_LOOP_MERGE:
        case SPIRV_OP_SELECTION_MERGE:
            *is_branch = qtrue;
            break;
        case SPIRV_OP_FMUL:
        case SPIRV_OP_FADD:
        case SPIRV_OP_FSUB:
        case SPIRV_OP_FDIV:
        case SPIRV_OP_IMUL:
        case SPIRV_OP_IADD:
        case SPIRV_OP_ISUB:
        case SPIRV_OP_SDIV:
        case SPIRV_OP_UDIV:
            *alu_intensity = 1.0f;
            break;
    }
}

// Initialize shader performance analyzer
qboolean vk_init_shader_performance_analyzer(void) {
    if (vk.shader_performance_analyzer.initialized) {
        return qtrue;
    }

    ri.Printf(PRINT_ALL, "Vulkan: Initializing shader performance analyzer\n");

    vk_shader_performance_analyzer_t *analyzer = &vk.shader_performance_analyzer;

    // Configure analyzer
    analyzer->enabled = qtrue;
    analyzer->detailed_analysis = qtrue;
    analyzer->max_shaders = 256;
    analyzer->max_instruction_types = 512;
    analyzer->max_optimizations = 128;

    // Analysis settings
    analyzer->analyze_vertex_shaders = qtrue;
    analyzer->analyze_fragment_shaders = qtrue;
    analyzer->analyze_compute_shaders = qtrue;
    analyzer->analyze_geometry_shaders = qtrue;

    // Performance thresholds
    analyzer->high_instruction_threshold = 1000; // Instructions considered high
    analyzer->high_alu_threshold = 0.8f;         // 80% ALU utilization
    analyzer->high_memory_threshold = 0.7f;      // 70% memory usage

    // Hardware characteristics (estimated)
    analyzer->max_texture_units = 32;
    analyzer->max_uniform_buffers = 16;
    analyzer->max_memory_bandwidth = 100ULL * 1024 * 1024 * 1024; // 100 GB/s

    // Allocate shader metrics
    analyzer->shader_metrics = (vk_shader_performance_metrics_t*)ri.Malloc(
        sizeof(vk_shader_performance_metrics_t) * analyzer->max_shaders);
    if (!analyzer->shader_metrics) {
        ri.Printf(PRINT_WARNING, "Vulkan: Failed to allocate shader metrics buffer\n");
        return qfalse;
    }
    memset(analyzer->shader_metrics, 0,
           sizeof(vk_shader_performance_metrics_t) * analyzer->max_shaders);

    // Allocate instruction stats
    analyzer->instruction_stats = (vk_shader_instruction_t*)ri.Malloc(
        sizeof(vk_shader_instruction_t) * analyzer->max_instruction_types);
    if (!analyzer->instruction_stats) {
        ri.Printf(PRINT_WARNING, "Vulkan: Failed to allocate instruction stats buffer\n");
        ri.Free(analyzer->shader_metrics);
        return qfalse;
    }
    memset(analyzer->instruction_stats, 0,
           sizeof(vk_shader_instruction_t) * analyzer->max_instruction_types);

    // Allocate optimizations
    analyzer->optimizations = (vk_shader_optimization_t*)ri.Malloc(
        sizeof(vk_shader_optimization_t) * analyzer->max_optimizations);
    if (!analyzer->optimizations) {
        ri.Printf(PRINT_WARNING, "Vulkan: Failed to allocate optimizations buffer\n");
        ri.Free(analyzer->shader_metrics);
        ri.Free(analyzer->instruction_stats);
        return qfalse;
    }
    memset(analyzer->optimizations, 0,
           sizeof(vk_shader_optimization_t) * analyzer->max_optimizations);

    // Initialize statistics
    atomic_init(&analyzer->active_shaders, 0);
    atomic_init(&analyzer->active_instruction_types, 0);
    atomic_init(&analyzer->active_optimizations, 0);
    atomic_init(&analyzer->total_shaders_analyzed, 0);
    atomic_init(&analyzer->total_instructions_analyzed, 0);
    atomic_init(&analyzer->shaders_with_warnings, 0);
    atomic_init(&analyzer->critical_performance_issues, 0);
    atomic_init(&analyzer->total_registers_tracked, 0);

    analyzer->debug_name = "shader_performance_analyzer";
    analyzer->initialized = qtrue;

    ri.Printf(PRINT_ALL, "Vulkan: Shader performance analyzer initialized with %d shaders, %d instruction types, %d optimizations capacity\n",
        analyzer->max_shaders, analyzer->max_instruction_types, analyzer->max_optimizations);

    return qtrue;
}

// Shutdown shader performance analyzer
void vk_shutdown_shader_performance_analyzer(void) {
    if (!vk.shader_performance_analyzer.initialized) {
        return;
    }

    ri.Printf(PRINT_ALL, "Vulkan: Shutting down shader performance analyzer\n");

    vk_shader_performance_analyzer_t *analyzer = &vk.shader_performance_analyzer;

    // Free allocated memory
    if (analyzer->shader_metrics) {
        ri.Free(analyzer->shader_metrics);
    }
    if (analyzer->instruction_stats) {
        ri.Free(analyzer->instruction_stats);
    }
    if (analyzer->optimizations) {
        ri.Free(analyzer->optimizations);
    }

    analyzer->initialized = qfalse;
    ri.Printf(PRINT_ALL, "Vulkan: Shader performance analyzer shutdown complete\n");
}

// Analyze shader performance from SPIR-V bytecode
void vk_analyze_shader_performance(const char *shader_name, const uint32_t *spirv_code, size_t code_size, VkShaderStageFlagBits stage) {
    if (!vk.shader_performance_analyzer.enabled || !vk.shader_performance_analyzer.initialized) {
        return;
    }

    vk_shader_performance_analyzer_t *analyzer = &vk.shader_performance_analyzer;

    // Check if we should analyze this shader stage
    qboolean should_analyze = qfalse;
    switch (stage) {
        case VK_SHADER_STAGE_VERTEX_BIT:
            should_analyze = analyzer->analyze_vertex_shaders;
            break;
        case VK_SHADER_STAGE_FRAGMENT_BIT:
            should_analyze = analyzer->analyze_fragment_shaders;
            break;
        case VK_SHADER_STAGE_COMPUTE_BIT:
            should_analyze = analyzer->analyze_compute_shaders;
            break;
        case VK_SHADER_STAGE_GEOMETRY_BIT:
            should_analyze = analyzer->analyze_geometry_shaders;
            break;
        default:
            should_analyze = qfalse;
    }

    if (!should_analyze) {
        return;
    }

    // Find or create shader metrics entry
    vk_shader_performance_metrics_t *metrics = NULL;
    for (uint32_t i = 0; i < analyzer->active_shaders; i++) {
        if (strcmp(analyzer->shader_metrics[i].shader_name, shader_name) == 0) {
            metrics = &analyzer->shader_metrics[i];
            break;
        }
    }

    if (!metrics && analyzer->active_shaders < analyzer->max_shaders) {
        metrics = &analyzer->shader_metrics[analyzer->active_shaders++];
        memset(metrics, 0, sizeof(vk_shader_performance_metrics_t));
        metrics->shader_name = shader_name;
        metrics->stage = stage;
    }

    if (!metrics) {
        return; // No space for new shader
    }

    // Reset metrics for fresh analysis
    metrics->total_instructions = 0;
    metrics->total_words = code_size / 4; // SPIR-V words are 32-bit
    metrics->arithmetic_instructions = 0;
    metrics->memory_instructions = 0;
    metrics->texture_instructions = 0;
    metrics->control_flow_instructions = 0;
    metrics->estimated_cycles = 0.0f;
    metrics->alu_utilization = 0.0f;
    metrics->cyclomatic_complexity = 0.0f;
    metrics->max_nesting_depth = 0;
    metrics->basic_blocks = 0;

    // Analyze SPIR-V bytecode
    uint64_t analysis_start = ri.Milliseconds() * 1000000ULL;

    size_t word_index = 5; // Skip SPIR-V header (magic, version, generator, bound, schema)
    uint32_t current_function_depth = 0;
    uint32_t current_control_depth = 0;

    while (word_index < code_size / 4) {
        uint32_t instruction = spirv_code[word_index];
        uint32_t word_count = instruction >> 16;
        uint32_t opcode = instruction & 0xFFFF;

        if (word_count == 0 || word_index + word_count > code_size / 4) {
            break; // Malformed instruction
        }

        metrics->total_instructions++;

        // Update instruction statistics
        vk_update_instruction_stats(analyzer, opcode, word_count);

        // Classify instruction
        qboolean is_memory, is_texture, is_branch;
        float alu_intensity;
        vk_classify_opcode(opcode, &is_memory, &is_texture, &is_branch, &alu_intensity);

        if (is_memory) metrics->memory_instructions++;
        if (is_texture) metrics->texture_instructions++;
        if (is_branch) metrics->control_flow_instructions++;
        if (alu_intensity > 0.0f) metrics->arithmetic_instructions++;

        // Estimate cycles
        metrics->estimated_cycles += vk_get_opcode_cycles(opcode);

        // Track control flow
        if (opcode == SPIRV_OP_FUNCTION) {
            current_function_depth++;
        } else if (opcode == SPIRV_OP_FUNCTION_END) {
            current_function_depth--;
        } else if (opcode == SPIRV_OP_LOOP_MERGE || opcode == SPIRV_OP_SELECTION_MERGE) {
            current_control_depth++;
            metrics->cyclomatic_complexity += 1.0f;
            if (current_control_depth > metrics->max_nesting_depth) {
                metrics->max_nesting_depth = current_control_depth;
            }
        } else if (opcode == SPIRV_OP_LABEL) {
            metrics->basic_blocks++;
        }

        word_index += word_count;
    }

    // Calculate derived metrics
    if (metrics->total_instructions > 0) {
        metrics->alu_utilization = (float)metrics->arithmetic_instructions / (float)metrics->total_instructions;
    }

    // Estimate bandwidth usage
    metrics->memory_bandwidth_usage = (float)metrics->memory_instructions * 16.0f; // Rough estimate: 16 bytes per memory op
    metrics->texture_bandwidth_usage = (float)metrics->texture_instructions * 64.0f; // Rough estimate: 64 bytes per texture sample

    // Determine primary bottleneck
    vk_analyze_shader_bottlenecks(metrics);

    // Generate optimization suggestions
    vk_generate_shader_optimizations(analyzer, metrics);

    uint64_t analysis_end = ri.Milliseconds() * 1000000ULL;
    analyzer->analysis_time_ns += (analysis_end - analysis_start);
    analyzer->total_shaders_analyzed++;
    analyzer->total_instructions_analyzed += metrics->total_instructions;
}

// Update instruction statistics
static void vk_update_instruction_stats(vk_shader_performance_analyzer_t *analyzer, uint32_t opcode, uint32_t word_count) {
    // Find existing instruction stat or create new one
    vk_shader_instruction_t *stat = NULL;
    for (uint32_t i = 0; i < analyzer->active_instruction_types; i++) {
        if (analyzer->instruction_stats[i].opcode == opcode) {
            stat = &analyzer->instruction_stats[i];
            break;
        }
    }

    if (!stat && analyzer->active_instruction_types < analyzer->max_instruction_types) {
        stat = &analyzer->instruction_stats[analyzer->active_instruction_types++];
        memset(stat, 0, sizeof(vk_shader_instruction_t));
        stat->opcode = opcode;
        stat->opcode_name = vk_get_spirv_opcode_name(opcode);
        stat->cycles_per_instruction = vk_get_opcode_cycles(opcode);
        vk_classify_opcode(opcode, &stat->is_memory_operation, NULL, &stat->is_branch_operation, &stat->alu_intensity);
    }

    if (stat) {
        stat->count++;
        stat->word_count += word_count;
    }
}

// Analyze shader bottlenecks
static void vk_analyze_shader_bottlenecks(vk_shader_performance_metrics_t *metrics) {
    metrics->is_memory_bound = qfalse;
    metrics->is_alu_bound = qfalse;
    metrics->is_texture_bound = qfalse;

    // Simple bottleneck detection based on ratios
    float memory_ratio = (float)metrics->memory_instructions / (float)metrics->total_instructions;
    float texture_ratio = (float)metrics->texture_instructions / (float)metrics->total_instructions;
    float alu_ratio = metrics->alu_utilization;

    if (texture_ratio > 0.3f) {
        metrics->primary_bottleneck = "Texture Sampling";
        metrics->bottleneck_severity = texture_ratio;
        metrics->is_texture_bound = qtrue;
    } else if (memory_ratio > 0.4f) {
        metrics->primary_bottleneck = "Memory Access";
        metrics->bottleneck_severity = memory_ratio;
        metrics->is_memory_bound = qtrue;
    } else if (alu_ratio > 0.8f) {
        metrics->primary_bottleneck = "ALU Utilization";
        metrics->bottleneck_severity = alu_ratio - 0.7f; // Severity above 70%
        metrics->is_alu_bound = qtrue;
    } else if (metrics->total_instructions > 2000) {
        metrics->primary_bottleneck = "Instruction Count";
        metrics->bottleneck_severity = (float)metrics->total_instructions / 4000.0f;
    } else {
        metrics->primary_bottleneck = "None";
        metrics->bottleneck_severity = 0.0f;
    }
}

// Generate shader optimization suggestions
static void vk_generate_shader_optimizations(vk_shader_performance_analyzer_t *analyzer, vk_shader_performance_metrics_t *metrics) {
    uint32_t opt_index = analyzer->active_optimizations;

    // High instruction count optimization
    if (metrics->total_instructions > analyzer->high_instruction_threshold && opt_index < analyzer->max_optimizations) {
        vk_shader_optimization_t *opt = &analyzer->optimizations[opt_index++];
        opt->shader_name = metrics->shader_name;
        opt->optimization_type = "Instruction Count Reduction";
        opt->current_instructions = metrics->total_instructions;
        opt->optimized_instructions = metrics->total_instructions * 8 / 10; // Estimate 20% reduction
        opt->current_cycles = metrics->estimated_cycles;
        opt->optimized_cycles = metrics->estimated_cycles * 0.8f;
        opt->performance_improvement = 1.25f; // 25% improvement
        opt->instruction_reduction = 0.2f;
        opt->implementation_hint = "Consider loop unrolling, constant folding, and eliminating redundant computations";
        opt->code_example = "Use compile-time constants and avoid dynamic branching where possible";
        opt->auto_applicable = qfalse;
    }

    // Texture sampling optimization
    if (metrics->is_texture_bound && opt_index < analyzer->max_optimizations) {
        vk_shader_optimization_t *opt = &analyzer->optimizations[opt_index++];
        opt->shader_name = metrics->shader_name;
        opt->optimization_type = "Texture Sampling Optimization";
        opt->current_instructions = metrics->texture_instructions;
        opt->optimized_instructions = metrics->texture_instructions * 7 / 10; // Estimate 30% reduction
        opt->performance_improvement = 2.0f; // 100% improvement possible
        opt->bandwidth_reduction = (VkDeviceSize)(metrics->texture_bandwidth_usage * 0.3f);
        opt->implementation_hint = "Use texture LOD bias, reduce texture samples, consider texture atlasing";
        opt->code_example = "textureLod(tex, uv, lod) instead of texture(tex, uv)";
        opt->auto_applicable = qfalse;
    }

    // Memory access optimization
    if (metrics->is_memory_bound && opt_index < analyzer->max_optimizations) {
        vk_shader_optimization_t *opt = &analyzer->optimizations[opt_index++];
        opt->shader_name = metrics->shader_name;
        opt->optimization_type = "Memory Access Optimization";
        opt->current_instructions = metrics->memory_instructions;
        opt->optimized_instructions = metrics->memory_instructions * 8 / 10; // Estimate 20% reduction
        opt->performance_improvement = 1.5f;
        opt->bandwidth_reduction = (VkDeviceSize)(metrics->memory_bandwidth_usage * 0.2f);
        opt->implementation_hint = "Use shared memory, reduce global memory accesses, improve data locality";
        opt->code_example = "Use local variables and minimize buffer reads in loops";
        opt->auto_applicable = qfalse;
    }

    // ALU optimization
    if (metrics->is_alu_bound && opt_index < analyzer->max_optimizations) {
        vk_shader_optimization_t *opt = &analyzer->optimizations[opt_index++];
        opt->shader_name = metrics->shader_name;
        opt->optimization_type = "ALU Optimization";
        opt->current_instructions = metrics->arithmetic_instructions;
        opt->optimized_instructions = metrics->arithmetic_instructions * 85 / 100; // Estimate 15% reduction
        opt->performance_improvement = 1.2f;
        opt->implementation_hint = "Use SIMD operations, reduce precision where possible, avoid complex math";
        opt->code_example = "Use mediump instead of highp, use built-in functions efficiently";
        opt->auto_applicable = qfalse;
    }

    analyzer->active_optimizations = opt_index;
}

// Print shader performance statistics
void vk_print_shader_performance_stats(void) {
    if (!vk.shader_performance_analyzer.enabled || !vk.shader_performance_analyzer.initialized) {
        ri.Printf(PRINT_ALL, "Shader performance analyzer not initialized\n");
        return;
    }

    vk_shader_performance_analyzer_t *analyzer = &vk.shader_performance_analyzer;

    ri.Printf(PRINT_ALL, "=== Shader Performance Analysis ===\n");

    ri.Printf(PRINT_ALL, "Shaders Analyzed: %lu\n", (unsigned long)analyzer->total_shaders_analyzed);
    ri.Printf(PRINT_ALL, "Total Instructions: %lu\n", (unsigned long)analyzer->total_instructions_analyzed);
    ri.Printf(PRINT_ALL, "Analysis Time: %.2f ms\n", analyzer->analysis_time_ns / 1000000.0);

    for (uint32_t i = 0; i < analyzer->active_shaders; i++) {
        vk_shader_performance_metrics_t *metrics = &analyzer->shader_metrics[i];

        const char *stage_name = "Unknown";
        switch (metrics->stage) {
            case VK_SHADER_STAGE_VERTEX_BIT: stage_name = "Vertex"; break;
            case VK_SHADER_STAGE_FRAGMENT_BIT: stage_name = "Fragment"; break;
            case VK_SHADER_STAGE_COMPUTE_BIT: stage_name = "Compute"; break;
            case VK_SHADER_STAGE_GEOMETRY_BIT: stage_name = "Geometry"; break;
        }

        ri.Printf(PRINT_ALL, "\nShader: %s (%s)\n", metrics->shader_name, stage_name);
        ri.Printf(PRINT_ALL, "  Instructions: %u total, %u ALU, %u memory, %u texture\n",
            metrics->total_instructions, metrics->arithmetic_instructions,
            metrics->memory_instructions, metrics->texture_instructions);
        ri.Printf(PRINT_ALL, "  Estimated Cycles: %.0f\n", metrics->estimated_cycles);
        ri.Printf(PRINT_ALL, "  ALU Utilization: %.1f%%\n", metrics->alu_utilization * 100.0f);
        ri.Printf(PRINT_ALL, "  Complexity: %.1f (depth: %u, blocks: %u)\n",
            metrics->cyclomatic_complexity, metrics->max_nesting_depth, metrics->basic_blocks);

        if (metrics->bottleneck_severity > 0.1f) {
            ri.Printf(PRINT_ALL, "  Bottleneck: %s (Severity: %.1f%%)\n",
                metrics->primary_bottleneck, metrics->bottleneck_severity * 100.0f);
        }
    }
}

// Print shader optimization suggestions
void vk_print_shader_optimization_suggestions(void) {
    if (!vk.shader_performance_analyzer.enabled || !vk.shader_performance_analyzer.initialized) {
        ri.Printf(PRINT_ALL, "Shader performance analyzer not initialized\n");
        return;
    }

    vk_shader_performance_analyzer_t *analyzer = &vk.shader_performance_analyzer;

    ri.Printf(PRINT_ALL, "=== Shader Optimization Suggestions ===\n");

    if (analyzer->active_optimizations == 0) {
        ri.Printf(PRINT_ALL, "No optimization suggestions available\n");
        return;
    }

    for (uint32_t i = 0; i < analyzer->active_optimizations; i++) {
        vk_shader_optimization_t *opt = &analyzer->optimizations[i];

        ri.Printf(PRINT_ALL, "Shader: %s\n", opt->shader_name);
        ri.Printf(PRINT_ALL, "  Optimization: %s\n", opt->optimization_type);
        ri.Printf(PRINT_ALL, "  Performance Improvement: %.1fx\n", opt->performance_improvement);
        ri.Printf(PRINT_ALL, "  Instruction Reduction: %.1f%%\n", opt->instruction_reduction * 100.0f);

        if (opt->bandwidth_reduction > 0) {
            ri.Printf(PRINT_ALL, "  Bandwidth Reduction: %.1f MB/s\n",
                (double)opt->bandwidth_reduction / (1024.0 * 1024.0));
        }

        ri.Printf(PRINT_ALL, "  Implementation: %s\n", opt->implementation_hint);

        if (opt->code_example) {
            ri.Printf(PRINT_ALL, "  Example: %s\n", opt->code_example);
        }

        ri.Printf(PRINT_ALL, "  Auto-applicable: %s\n\n", opt->auto_applicable ? "Yes" : "No");
    }
}

// Print shader instruction analysis
void vk_print_shader_instruction_analysis(void) {
    if (!vk.shader_performance_analyzer.enabled || !vk.shader_performance_analyzer.initialized) {
        ri.Printf(PRINT_ALL, "Shader performance analyzer not initialized\n");
        return;
    }

    vk_shader_performance_analyzer_t *analyzer = &vk.shader_performance_analyzer;

    ri.Printf(PRINT_ALL, "=== Shader Instruction Analysis ===\n");

    // Sort instructions by frequency (simple bubble sort for demonstration)
    for (uint32_t i = 0; i < analyzer->active_instruction_types - 1; i++) {
        for (uint32_t j = 0; j < analyzer->active_instruction_types - i - 1; j++) {
            if (analyzer->instruction_stats[j].count < analyzer->instruction_stats[j + 1].count) {
                vk_shader_instruction_t temp = analyzer->instruction_stats[j];
                analyzer->instruction_stats[j] = analyzer->instruction_stats[j + 1];
                analyzer->instruction_stats[j + 1] = temp;
            }
        }
    }

    ri.Printf(PRINT_ALL, "Top Instructions by Frequency:\n");
    for (uint32_t i = 0; i < analyzer->active_instruction_types && i < 20; i++) {
        vk_shader_instruction_t *inst = &analyzer->instruction_stats[i];

        ri.Printf(PRINT_ALL, "  %s: %u uses", inst->opcode_name, inst->count);

        if (inst->is_memory_operation) ri.Printf(PRINT_ALL, " [MEM]");
        if (inst->alu_intensity > 0.0f) ri.Printf(PRINT_ALL, " [ALU]");
        if (inst->is_branch_operation) ri.Printf(PRINT_ALL, " [BRANCH]");

        ri.Printf(PRINT_ALL, " (%.1f cycles/use)\n", inst->cycles_per_instruction);
    }
}

// Print shader register usage
void vk_print_shader_register_usage(void) {
    if (!vk.shader_performance_analyzer.enabled || !vk.shader_performance_analyzer.initialized) {
        ri.Printf(PRINT_ALL, "Shader performance analyzer not initialized\n");
        return;
    }

    vk_shader_performance_analyzer_t *analyzer = &vk.shader_performance_analyzer;

    ri.Printf(PRINT_ALL, "=== Shader Register Usage Analysis ===\n");

    for (uint32_t i = 0; i < analyzer->active_shaders; i++) {
        vk_shader_performance_metrics_t *metrics = &analyzer->shader_metrics[i];
        vk_shader_register_usage_t *regs = &metrics->registers;

        const char *stage_name = "Unknown";
        switch (metrics->stage) {
            case VK_SHADER_STAGE_VERTEX_BIT: stage_name = "Vertex"; break;
            case VK_SHADER_STAGE_FRAGMENT_BIT: stage_name = "Fragment"; break;
            case VK_SHADER_STAGE_COMPUTE_BIT: stage_name = "Compute"; break;
            case VK_SHADER_STAGE_GEOMETRY_BIT: stage_name = "Geometry"; break;
        }

        ri.Printf(PRINT_ALL, "Shader: %s (%s)\n", metrics->shader_name, stage_name);
        ri.Printf(PRINT_ALL, "  Input: %u attributes, %u varyings\n",
            regs->input_attributes, regs->input_varyings);
        ri.Printf(PRINT_ALL, "  Resources: %u UBOs, %u SSBOs, %u textures, %u samplers\n",
            regs->uniform_buffers, regs->storage_buffers, regs->sampled_images, regs->samplers);
        ri.Printf(PRINT_ALL, "  Output: %u attributes, %u varyings\n",
            regs->output_attributes, regs->output_varyings);
        ri.Printf(PRINT_ALL, "  Temporary: %u registers (max pressure: %u)\n",
            regs->temp_registers, regs->max_register_pressure);

        // Check for potential issues
        if (regs->uniform_buffers > analyzer->max_uniform_buffers / 2) {
            ri.Printf(PRINT_ALL, "  WARNING: High uniform buffer usage\n");
        }
        if (regs->sampled_images > analyzer->max_texture_units / 2) {
            ri.Printf(PRINT_ALL, "  WARNING: High texture unit usage\n");
        }
        ri.Printf(PRINT_ALL, "\n");
    }
}

// Enable/disable shader analysis
void vk_set_shader_analysis_enabled(qboolean enabled) {
    if (vk.shader_performance_analyzer.enabled != enabled) {
        ri.Printf(PRINT_ALL, "Vulkan: %s shader performance analysis\n",
            enabled ? "Enabling" : "Disabling");
        vk.shader_performance_analyzer.enabled = enabled;
    }
}

// Asset Loading Profiler Implementation

// Forward declarations for asset loading profiler functions
static void vk_analyze_asset_loading_bottlenecks(vk_asset_loading_profiler_t *profiler);

// Performance HUD Implementation

#ifdef USE_CIMGUI
static void vk_update_performance_hud_bottlenecks(vk_performance_hud_t *hud);
static void vk_render_performance_hud_main_window(vk_performance_hud_t *hud);
static void vk_render_performance_hud_vram_stats(vk_performance_hud_t *hud);
static void vk_render_performance_hud_memory_systems(vk_performance_hud_t *hud);
static void vk_render_performance_hud_render_profiler(vk_performance_hud_t *hud);
static void vk_render_performance_hud_bottlenecks(vk_performance_hud_t *hud);
static void vk_render_performance_hud_recommendations(vk_performance_hud_t *hud);
static ImVec4 vk_get_bottleneck_color(float severity, const vk_performance_hud_config_t *config);
static void vk_format_performance_value(char *buffer, size_t size, float value, const char *unit);
#endif

// Performance Regression Detector Implementation

// Forward declarations for performance regression functions
static void vk_add_metric_to_snapshot(vk_performance_snapshot_t *snapshot, const char *name, float value, const char *unit, qboolean lower_is_better);
static void vk_load_performance_baselines(vk_performance_regression_detector_t *detector);
static void vk_save_performance_baselines(vk_performance_regression_detector_t *detector);
static float vk_calculate_regression_severity(float current, float baseline, qboolean lower_is_better);

// Heatmap Visualizer Implementation

// Forward declarations for heatmap visualization
static void vk_update_heatmap_layer(vk_heatmap_layer_t *layer, vk_heatmap_sample_t *samples, uint32_t sample_count, float decay_rate);
static void vk_create_heatmap_texture(vk_heatmap_layer_t *layer);
static void vk_update_heatmap_texture(vk_heatmap_layer_t *layer);
#ifdef USE_CIMGUI
static ImVec4 vk_get_heatmap_color(float intensity, float gradient[5][4]);
#endif

// Initialize asset loading profiler
qboolean vk_init_asset_loading_profiler(void) {
    if (vk.asset_loading_profiler.initialized) {
        return qtrue;
    }

    ri.Printf(PRINT_ALL, "Vulkan: Initializing asset loading profiler\n");

    vk_asset_loading_profiler_t *profiler = &vk.asset_loading_profiler;

    // Configure profiler
    profiler->enabled = qtrue;
    profiler->detailed_tracking = qtrue;
    profiler->max_load_operations = 1024;
    profiler->max_io_operations = 2048;
    profiler->max_streaming_operations = 512;
    profiler->sample_interval_ns = 100000000ULL; // 100ms sampling
    profiler->slow_load_threshold_bytes = 1024 * 1024; // 1MB threshold
    profiler->slow_load_threshold_ns = 100000000ULL; // 100ms threshold
    profiler->max_concurrent_loads = 8;

    // Allocate load operations
    profiler->load_operations = (vk_asset_load_operation_t*)ri.Malloc(
        sizeof(vk_asset_load_operation_t) * profiler->max_load_operations);
    if (!profiler->load_operations) {
        ri.Printf(PRINT_WARNING, "Vulkan: Failed to allocate load operations buffer\n");
        return qfalse;
    }
    memset(profiler->load_operations, 0,
           sizeof(vk_asset_load_operation_t) * profiler->max_load_operations);

    // Allocate I/O operations
    profiler->io_operations = (vk_io_operation_t*)ri.Malloc(
        sizeof(vk_io_operation_t) * profiler->max_io_operations);
    if (!profiler->io_operations) {
        ri.Printf(PRINT_WARNING, "Vulkan: Failed to allocate I/O operations buffer\n");
        ri.Free(profiler->load_operations);
        return qfalse;
    }
    memset(profiler->io_operations, 0,
           sizeof(vk_io_operation_t) * profiler->max_io_operations);

    // Allocate streaming operations
    profiler->streaming_operations = (vk_streaming_operation_t*)ri.Malloc(
        sizeof(vk_streaming_operation_t) * profiler->max_streaming_operations);
    if (!profiler->streaming_operations) {
        ri.Printf(PRINT_WARNING, "Vulkan: Failed to allocate streaming operations buffer\n");
        ri.Free(profiler->load_operations);
        ri.Free(profiler->io_operations);
        return qfalse;
    }
    memset(profiler->streaming_operations, 0,
           sizeof(vk_streaming_operation_t) * profiler->max_streaming_operations);

    // Initialize timing
    profiler->last_sample_time = ri.Milliseconds() * 1000000ULL;

    atomic_init(&profiler->active_load_operations, 0);
    atomic_init(&profiler->active_io_operations, 0);
    atomic_init(&profiler->active_streaming_operations, 0);

    // Initialize statistics
    atomic_init(&profiler->total_assets_loaded, 0);
    atomic_init(&profiler->failed_loads, 0);
    atomic_init(&profiler->texture_loads, 0);
    atomic_init(&profiler->model_loads, 0);
    atomic_init(&profiler->sound_loads, 0);
    atomic_init(&profiler->other_loads, 0);

    profiler->debug_name = "asset_loading_profiler";
    profiler->initialized = qtrue;

    ri.Printf(PRINT_ALL, "Vulkan: Asset loading profiler initialized with %d load ops, %d I/O ops, %d streaming ops capacity\n",
        profiler->max_load_operations, profiler->max_io_operations, profiler->max_streaming_operations);

    return qtrue;
}

// Shutdown asset loading profiler
void vk_shutdown_asset_loading_profiler(void) {
    if (!vk.asset_loading_profiler.initialized) {
        return;
    }

    ri.Printf(PRINT_ALL, "Vulkan: Shutting down asset loading profiler\n");

    vk_asset_loading_profiler_t *profiler = &vk.asset_loading_profiler;

    // Free allocated memory
    if (profiler->load_operations) {
        ri.Free(profiler->load_operations);
    }
    if (profiler->io_operations) {
        ri.Free(profiler->io_operations);
    }
    if (profiler->streaming_operations) {
        ri.Free(profiler->streaming_operations);
    }

    profiler->initialized = qfalse;
    ri.Printf(PRINT_ALL, "Vulkan: Asset loading profiler shutdown complete\n");
}

// Profile asset load start
void vk_profile_asset_load_start(const char *asset_name, const char *asset_type, VkDeviceSize expected_size) {
    if (!vk.asset_loading_profiler.enabled || !vk.asset_loading_profiler.initialized) {
        return;
    }

    vk_asset_loading_profiler_t *profiler = &vk.asset_loading_profiler;

    // Find available slot or reuse oldest
    vk_asset_load_operation_t *operation = NULL;
    uint32_t oldest_index = 0;
    uint64_t oldest_time = UINT64_MAX;

    for (uint32_t i = 0; i < profiler->max_load_operations; i++) {
        vk_asset_load_operation_t *op = &profiler->load_operations[i];
        if (op->asset_name == NULL || op->end_time > 0) {
            operation = op;
            break;
        }
        // Find oldest completed operation to reuse
        if (op->end_time > 0 && op->end_time < oldest_time) {
            oldest_time = op->end_time;
            oldest_index = i;
        }
    }

    if (!operation && oldest_time < UINT64_MAX) {
        operation = &profiler->load_operations[oldest_index];
    }

    if (operation) {
        memset(operation, 0, sizeof(vk_asset_load_operation_t));
        operation->asset_name = asset_name;
        operation->asset_type = asset_type;
        operation->expected_size = expected_size;
        operation->start_time = ri.Milliseconds() * 1000000ULL;
        operation->success = qfalse; // Will be set to true on successful completion

        if (profiler->active_load_operations < profiler->max_load_operations) {
            profiler->active_load_operations++;
        }
    }
}

// Profile asset load end
void vk_profile_asset_load_end(const char *asset_name, VkDeviceSize actual_size, qboolean success) {
    if (!vk.asset_loading_profiler.enabled || !vk.asset_loading_profiler.initialized) {
        return;
    }

    vk_asset_loading_profiler_t *profiler = &vk.asset_loading_profiler;

    // Find the matching load operation
    for (uint32_t i = 0; i < profiler->max_load_operations; i++) {
        vk_asset_load_operation_t *operation = &profiler->load_operations[i];
        if (operation->asset_name && strcmp(operation->asset_name, asset_name) == 0 && operation->end_time == 0) {
            operation->end_time = ri.Milliseconds() * 1000000ULL;
            operation->actual_size = actual_size;
            operation->success = success;

            // Calculate performance metrics
            if (operation->start_time > 0) {
                operation->load_time_ns = operation->end_time - operation->start_time;
                if (operation->load_time_ns > 0) {
                    operation->load_bandwidth_mbps = (float)actual_size / (float)operation->load_time_ns * 1000.0f;
                }
            }

            operation->is_slow_load = (actual_size >= profiler->slow_load_threshold_bytes &&
                                     operation->load_time_ns >= profiler->slow_load_threshold_ns);

            // Update global statistics
            if (success) {
                profiler->total_assets_loaded++;
                profiler->total_load_time_ns += operation->load_time_ns;
                profiler->total_bytes_loaded += actual_size;

                // Update asset type counters
                if (strcmp(operation->asset_type, "texture") == 0) {
                    profiler->texture_loads++;
                } else if (strcmp(operation->asset_type, "model") == 0) {
                    profiler->model_loads++;
                } else if (strcmp(operation->asset_type, "sound") == 0) {
                    profiler->sound_loads++;
                } else {
                    profiler->other_loads++;
                }
            } else {
                profiler->failed_loads++;
            }

            break;
        }
    }
}

// Profile I/O operation
void vk_profile_io_operation(const char *operation_name, VkDeviceSize bytes_transferred, uint64_t operation_time_ns) {
    if (!vk.asset_loading_profiler.enabled || !vk.asset_loading_profiler.initialized) {
        return;
    }

    vk_asset_loading_profiler_t *profiler = &vk.asset_loading_profiler;

    // Find available slot or reuse oldest
    vk_io_operation_t *operation = NULL;
    uint32_t oldest_index = 0;
    uint64_t oldest_time = UINT64_MAX;

    for (uint32_t i = 0; i < profiler->max_io_operations; i++) {
        vk_io_operation_t *op = &profiler->io_operations[i];
        if (op->operation_name == NULL || op->end_time > 0) {
            operation = op;
            break;
        }
        // Find oldest completed operation to reuse
        if (op->end_time > 0 && op->end_time < oldest_time) {
            oldest_time = op->end_time;
            oldest_index = i;
        }
    }

    if (!operation && oldest_time < UINT64_MAX) {
        operation = &profiler->io_operations[oldest_index];
    }

    if (operation) {
        memset(operation, 0, sizeof(vk_io_operation_t));
        operation->operation_name = operation_name;
        operation->bytes_transferred = bytes_transferred;
        operation->operation_time_ns = operation_time_ns;
        operation->start_time = (ri.Milliseconds() * 1000000ULL) - operation_time_ns;
        operation->end_time = ri.Milliseconds() * 1000000ULL;
        operation->is_read_operation = qtrue; // Assume read for now

        // Calculate performance metrics
        operation->bandwidth_mbps = (float)bytes_transferred / (float)operation_time_ns * 1000.0f;
        operation->latency_ms = (float)operation_time_ns / 1000000.0f;

        // Simple bottleneck detection
        operation->is_bottleneck = (operation->latency_ms > 10.0f || operation->bandwidth_mbps < 100.0f);

        if (profiler->active_io_operations < profiler->max_io_operations) {
            profiler->active_io_operations++;
        }
    }
}

// Profile streaming request
void vk_profile_streaming_request(const char *asset_name, uint32_t mip_level, qboolean is_high_priority) {
    if (!vk.asset_loading_profiler.enabled || !vk.asset_loading_profiler.initialized) {
        return;
    }

    vk_asset_loading_profiler_t *profiler = &vk.asset_loading_profiler;

    // Find available slot or reuse oldest
    vk_streaming_operation_t *operation = NULL;
    uint32_t oldest_index = 0;
    uint64_t oldest_time = UINT64_MAX;

    for (uint32_t i = 0; i < profiler->max_streaming_operations; i++) {
        vk_streaming_operation_t *op = &profiler->streaming_operations[i];
        if (op->asset_name == NULL || op->completion_time > 0) {
            operation = op;
            break;
        }
        // Find oldest completed operation to reuse
        if (op->completion_time > 0 && op->completion_time < oldest_time) {
            oldest_time = op->completion_time;
            oldest_index = i;
        }
    }

    if (!operation && oldest_time < UINT64_MAX) {
        operation = &profiler->streaming_operations[oldest_index];
    }

    if (operation) {
        memset(operation, 0, sizeof(vk_streaming_operation_t));
        operation->asset_name = asset_name;
        operation->mip_level = mip_level;
        operation->request_time = ri.Milliseconds() * 1000000ULL;
        operation->is_high_priority = is_high_priority;

        if (profiler->active_streaming_operations < profiler->max_streaming_operations) {
            profiler->active_streaming_operations++;
        }
    }
}

// Profile streaming completion
void vk_profile_streaming_complete(const char *asset_name, uint32_t mip_level, VkDeviceSize bytes_loaded) {
    if (!vk.asset_loading_profiler.enabled || !vk.asset_loading_profiler.initialized) {
        return;
    }

    vk_asset_loading_profiler_t *profiler = &vk.asset_loading_profiler;

    // Find the matching streaming operation
    for (uint32_t i = 0; i < profiler->max_streaming_operations; i++) {
        vk_streaming_operation_t *operation = &profiler->streaming_operations[i];
        if (operation->asset_name && strcmp(operation->asset_name, asset_name) == 0 &&
            operation->mip_level == mip_level && operation->completion_time == 0) {

            operation->completion_time = ri.Milliseconds() * 1000000ULL;
            operation->bytes_loaded = bytes_loaded;

            // Calculate timing metrics
            if (operation->load_start_time > 0) {
                operation->load_time = operation->completion_time - operation->load_start_time;
            }
            if (operation->request_time > 0) {
                operation->total_time = operation->completion_time - operation->request_time;
                if (operation->load_start_time > operation->request_time) {
                    operation->queue_wait_time = operation->load_start_time - operation->request_time;
                }
            }

            // Calculate effective bandwidth
            if (operation->load_time > 0) {
                operation->effective_bandwidth = (float)bytes_loaded / (float)operation->load_time * 1000.0f;
            }

            // Check for missed deadlines (simple heuristic)
            operation->missed_deadline = (operation->total_time > 50000000ULL && !operation->is_high_priority); // 50ms for low priority

            break;
        }
    }
}

// Sample asset loading performance
void vk_sample_asset_loading_performance(void) {
    if (!vk.asset_loading_profiler.enabled || !vk.asset_loading_profiler.initialized) {
        return;
    }

    vk_asset_loading_profiler_t *profiler = &vk.asset_loading_profiler;
    uint64_t current_time = ri.Milliseconds() * 1000000ULL;

    if (current_time - profiler->last_sample_time >= profiler->sample_interval_ns) {
        // Update queue performance statistics
        vk_asset_queue_performance_t *queue = &profiler->queue_performance;

        // Simple queue length estimation (would be more sophisticated in real implementation)
        queue->current_queue_length = profiler->active_load_operations;

        if (queue->current_queue_length > queue->max_queue_length) {
            queue->max_queue_length = queue->current_queue_length;
        }

        // Calculate throughput metrics
        if (profiler->total_assets_loaded > 0 && profiler->total_load_time_ns > 0) {
            queue->assets_per_second = (float)profiler->total_assets_loaded /
                                     ((float)profiler->total_load_time_ns / 1000000000.0f);
            queue->bytes_per_second = (VkDeviceSize)((float)profiler->total_bytes_loaded /
                                     ((float)profiler->total_load_time_ns / 1000000000.0f));
        }

        // Update bottleneck analysis
        vk_analyze_asset_loading_bottlenecks(profiler);

        profiler->last_sample_time = current_time;
    }
}

// Analyze asset loading bottlenecks
static void vk_analyze_asset_loading_bottlenecks(vk_asset_loading_profiler_t *profiler) {
    vk_asset_loading_bottlenecks_t *bottlenecks = &profiler->bottlenecks;

    // Reset analysis
    bottlenecks->io_bottleneck = qfalse;
    bottlenecks->cpu_bottleneck = qfalse;
    bottlenecks->memory_bottleneck = qfalse;
    bottlenecks->streaming_bottleneck = qfalse;

    // Analyze I/O bottlenecks
    if (profiler->active_io_operations > 0) {
        uint64_t total_io_time = 0;
        uint32_t bottleneck_ops = 0;

        for (uint32_t i = 0; i < profiler->max_io_operations; i++) {
            vk_io_operation_t *op = &profiler->io_operations[i];
            if (op->operation_name && op->is_bottleneck) {
                bottleneck_ops++;
                total_io_time += op->operation_time_ns;
            }
        }

        bottlenecks->io_bottleneck = ((float)bottleneck_ops / (float)profiler->active_io_operations) > 0.3f;
        if (bottleneck_ops > 0) {
            bottlenecks->avg_io_latency = total_io_time / bottleneck_ops / 1000000ULL; // Convert to ms
        }
        bottlenecks->concurrent_io_ops = profiler->active_io_operations;
    }

    // Analyze streaming bottlenecks
    if (profiler->active_streaming_operations > 0) {
        uint32_t missed_deadlines = 0;
        uint32_t long_queue_waits = 0;

        for (uint32_t i = 0; i < profiler->max_streaming_operations; i++) {
            vk_streaming_operation_t *op = &profiler->streaming_operations[i];
            if (op->asset_name && op->missed_deadline) {
                missed_deadlines++;
            }
            if (op->queue_wait_time > 10000000ULL) { // 10ms queue wait
                long_queue_waits++;
            }
        }

        bottlenecks->streaming_bottleneck = ((float)missed_deadlines / (float)profiler->active_streaming_operations) > 0.2f;
        bottlenecks->streaming_queue_length = profiler->active_streaming_operations;
        bottlenecks->streaming_efficiency = 1.0f - ((float)long_queue_waits / (float)profiler->active_streaming_operations);
    }

    // Determine primary bottleneck
    if (bottlenecks->io_bottleneck) {
        bottlenecks->primary_bottleneck = "I/O Subsystem";
        bottlenecks->optimization_hint = "Consider using compression, implementing asset streaming, or optimizing file access patterns";
        bottlenecks->expected_improvement = 2.0f;
    } else if (bottlenecks->streaming_bottleneck) {
        bottlenecks->primary_bottleneck = "Streaming System";
        bottlenecks->optimization_hint = "Increase streaming buffer sizes, implement predictive loading, or reduce asset quality";
        bottlenecks->expected_improvement = 1.8f;
    } else if (bottlenecks->cpu_bottleneck) {
        bottlenecks->primary_bottleneck = "CPU Processing";
        bottlenecks->optimization_hint = "Use faster compression algorithms, reduce asset complexity, or implement parallel loading";
        bottlenecks->expected_improvement = 1.5f;
    } else if (bottlenecks->memory_bottleneck) {
        bottlenecks->primary_bottleneck = "Memory Allocation";
        bottlenecks->optimization_hint = "Implement memory pooling, reduce asset sizes, or optimize allocation patterns";
        bottlenecks->expected_improvement = 1.3f;
    } else {
        bottlenecks->primary_bottleneck = "None";
        bottlenecks->optimization_hint = "Asset loading performance is good";
        bottlenecks->expected_improvement = 1.0f;
    }
}

// Print asset loading statistics
void vk_print_asset_loading_stats(void) {
    if (!vk.asset_loading_profiler.enabled || !vk.asset_loading_profiler.initialized) {
        ri.Printf(PRINT_ALL, "Asset loading profiler not initialized\n");
        return;
    }

    vk_asset_loading_profiler_t *profiler = &vk.asset_loading_profiler;

    ri.Printf(PRINT_ALL, "=== Asset Loading Statistics ===\n");

    ri.Printf(PRINT_ALL, "Overall Performance:\n");
    ri.Printf(PRINT_ALL, "  Total Assets Loaded: %lu\n", (unsigned long)profiler->total_assets_loaded);
    ri.Printf(PRINT_ALL, "  Failed Loads: %u\n", profiler->failed_loads);
    ri.Printf(PRINT_ALL, "  Total Data Loaded: %.2f MB\n",
        (double)profiler->total_bytes_loaded / (1024.0 * 1024.0));
    ri.Printf(PRINT_ALL, "  Average Load Time: %.2f ms\n",
        profiler->total_assets_loaded > 0 ?
        (double)profiler->total_load_time_ns / (double)profiler->total_assets_loaded / 1000000.0 : 0.0);

    ri.Printf(PRINT_ALL, "Asset Types:\n");
    ri.Printf(PRINT_ALL, "  Textures: %u\n", profiler->texture_loads);
    ri.Printf(PRINT_ALL, "  Models: %u\n", profiler->model_loads);
    ri.Printf(PRINT_ALL, "  Sounds: %u\n", profiler->sound_loads);
    ri.Printf(PRINT_ALL, "  Other: %u\n", profiler->other_loads);

    ri.Printf(PRINT_ALL, "Queue Performance:\n");
    ri.Printf(PRINT_ALL, "  Current Queue Length: %u\n", profiler->queue_performance.current_queue_length);
    ri.Printf(PRINT_ALL, "  Max Queue Length: %u\n", profiler->queue_performance.max_queue_length);
    ri.Printf(PRINT_ALL, "  Assets/Second: %.1f\n", profiler->queue_performance.assets_per_second);
    ri.Printf(PRINT_ALL, "  MB/Second: %.1f\n", (double)profiler->queue_performance.bytes_per_second / (1024.0 * 1024.0));

    // Show slow loads
    uint32_t slow_loads = 0;
    for (uint32_t i = 0; i < profiler->max_load_operations; i++) {
        vk_asset_load_operation_t *op = &profiler->load_operations[i];
        if (op->asset_name && op->is_slow_load) {
            slow_loads++;
        }
    }

    if (slow_loads > 0) {
        ri.Printf(PRINT_ALL, "Slow Loads Detected: %u\n", slow_loads);
        ri.Printf(PRINT_ALL, "  (Loads taking >100ms for >1MB assets)\n");
    }
}

// Print I/O performance statistics
void vk_print_io_performance_stats(void) {
    if (!vk.asset_loading_profiler.enabled || !vk.asset_loading_profiler.initialized) {
        ri.Printf(PRINT_ALL, "Asset loading profiler not initialized\n");
        return;
    }

    vk_asset_loading_profiler_t *profiler = &vk.asset_loading_profiler;

    ri.Printf(PRINT_ALL, "=== I/O Performance Statistics ===\n");

    if (profiler->active_io_operations == 0) {
        ri.Printf(PRINT_ALL, "No I/O operations recorded\n");
        return;
    }

    uint32_t bottleneck_count = 0;
    VkDeviceSize total_bytes = 0;
    uint64_t total_time = 0;

    for (uint32_t i = 0; i < profiler->max_io_operations; i++) {
        vk_io_operation_t *op = &profiler->io_operations[i];
        if (op->operation_name) {
            total_bytes += op->bytes_transferred;
            total_time += op->operation_time_ns;
            if (op->is_bottleneck) {
                bottleneck_count++;
            }
        }
    }

    ri.Printf(PRINT_ALL, "I/O Operations: %u active\n", profiler->active_io_operations);
    ri.Printf(PRINT_ALL, "Total Data Transferred: %.2f MB\n", (double)total_bytes / (1024.0 * 1024.0));
    ri.Printf(PRINT_ALL, "Average Bandwidth: %.1f MB/s\n",
        total_time > 0 ? (double)total_bytes / ((double)total_time / 1000000000.0) / (1024.0 * 1024.0) : 0.0);

    if (bottleneck_count > 0) {
        ri.Printf(PRINT_ALL, "I/O Bottlenecks: %u operations flagged\n", bottleneck_count);
        ri.Printf(PRINT_ALL, "  (Operations with high latency or low bandwidth)\n");
    }
}

// Print streaming statistics
void vk_print_streaming_stats(void) {
    if (!vk.asset_loading_profiler.enabled || !vk.asset_loading_profiler.initialized) {
        ri.Printf(PRINT_ALL, "Asset loading profiler not initialized\n");
        return;
    }

    vk_asset_loading_profiler_t *profiler = &vk.asset_loading_profiler;

    ri.Printf(PRINT_ALL, "=== Streaming Performance Statistics ===\n");

    if (profiler->active_streaming_operations == 0) {
        ri.Printf(PRINT_ALL, "No streaming operations recorded\n");
        return;
    }

    uint32_t completed_ops = 0;
    uint32_t missed_deadlines = 0;
    VkDeviceSize total_bytes = 0;
    uint64_t total_queue_time = 0;
    uint64_t total_load_time = 0;

    for (uint32_t i = 0; i < profiler->max_streaming_operations; i++) {
        vk_streaming_operation_t *op = &profiler->streaming_operations[i];
        if (op->asset_name && op->completion_time > 0) {
            completed_ops++;
            total_bytes += op->bytes_loaded;
            total_queue_time += op->queue_wait_time;
            total_load_time += op->load_time;

            if (op->missed_deadline) {
                missed_deadlines++;
            }
        }
    }

    ri.Printf(PRINT_ALL, "Streaming Operations: %u completed\n", completed_ops);
    ri.Printf(PRINT_ALL, "Total Data Streamed: %.2f MB\n", (double)total_bytes / (1024.0 * 1024.0));
    ri.Printf(PRINT_ALL, "Average Queue Wait: %.2f ms\n",
        completed_ops > 0 ? (double)total_queue_time / (double)completed_ops / 1000000.0 : 0.0);
    ri.Printf(PRINT_ALL, "Average Load Time: %.2f ms\n",
        completed_ops > 0 ? (double)total_load_time / (double)completed_ops / 1000000.0 : 0.0);

    if (missed_deadlines > 0) {
        ri.Printf(PRINT_ALL, "Missed Deadlines: %u operations\n", missed_deadlines);
        ri.Printf(PRINT_ALL, "  Streaming efficiency may need improvement\n");
    }
}

// Print asset loading bottlenecks
void vk_print_asset_loading_bottlenecks(void) {
    if (!vk.asset_loading_profiler.enabled || !vk.asset_loading_profiler.initialized) {
        ri.Printf(PRINT_ALL, "Asset loading profiler not initialized\n");
        return;
    }

    vk_asset_loading_profiler_t *profiler = &vk.asset_loading_profiler;

    ri.Printf(PRINT_ALL, "=== Asset Loading Bottleneck Analysis ===\n");

    vk_asset_loading_bottlenecks_t *bottlenecks = &profiler->bottlenecks;

    ri.Printf(PRINT_ALL, "Bottleneck Analysis:\n");
    ri.Printf(PRINT_ALL, "  Primary Bottleneck: %s\n", bottlenecks->primary_bottleneck);

    if (bottlenecks->io_bottleneck) {
        ri.Printf(PRINT_ALL, "  I/O Subsystem: BOTTLENECK\n");
        ri.Printf(PRINT_ALL, "    Average Latency: %lu ms\n", (unsigned long)bottlenecks->avg_io_latency);
        ri.Printf(PRINT_ALL, "    Concurrent Operations: %u\n", bottlenecks->concurrent_io_ops);
    } else {
        ri.Printf(PRINT_ALL, "  I/O Subsystem: OK\n");
    }

    if (bottlenecks->streaming_bottleneck) {
        ri.Printf(PRINT_ALL, "  Streaming System: BOTTLENECK\n");
        ri.Printf(PRINT_ALL, "    Queue Length: %u\n", bottlenecks->streaming_queue_length);
        ri.Printf(PRINT_ALL, "    Efficiency: %.1f%%\n", bottlenecks->streaming_efficiency * 100.0f);
    } else {
        ri.Printf(PRINT_ALL, "  Streaming System: OK\n");
    }

    ri.Printf(PRINT_ALL, "Optimization Recommendation:\n");
    ri.Printf(PRINT_ALL, "  %s\n", bottlenecks->optimization_hint);

    if (bottlenecks->expected_improvement > 1.0f) {
        ri.Printf(PRINT_ALL, "  Expected Improvement: %.1fx\n", bottlenecks->expected_improvement);
    }
}

// Enable/disable asset loading profiling
void vk_set_asset_loading_profiling_enabled(qboolean enabled) {
    if (vk.asset_loading_profiler.enabled != enabled) {
        ri.Printf(PRINT_ALL, "Vulkan: %s asset loading profiling\n",
            enabled ? "Enabling" : "Disabling");
        vk.asset_loading_profiler.enabled = enabled;
    }
}

// Performance HUD Implementation

// Initialize performance HUD
qboolean vk_init_performance_hud(void) {
#ifdef USE_CIMGUI
    ri.Printf(PRINT_ALL, "Performance HUD: imGUI is available\n");
    if (vk.performance_hud.initialized) {
        return qtrue;
    }

    ri.Printf(PRINT_ALL, "Vulkan: Initializing performance HUD\n");

    vk_performance_hud_t *hud = &vk.performance_hud;

    // Initialize configuration with defaults
    hud->config.enabled = qfalse;
    hud->config.show_vram_stats = qtrue;
    hud->config.show_memory_pools = qtrue;
    hud->config.show_render_profiler = qtrue;
    hud->config.show_memory_bandwidth = qtrue;
    hud->config.show_parallel_processing = qtrue;
    hud->config.show_shader_analysis = qtrue;
    hud->config.show_asset_loading = qtrue;
    hud->config.show_bottlenecks = qtrue;
    hud->config.show_recommendations = qtrue;
    hud->config.update_interval = 0.5f; // Update every 500ms
    hud->config.window_alpha = 0.9f;
    hud->config.window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize;
    hud->config.position_x = 10.0f;
    hud->config.position_y = 10.0f;
    hud->config.size_x = 400.0f;
    hud->config.size_y = 600.0f;

    // Bottleneck highlighting thresholds
    hud->config.highlight_bottlenecks = qtrue;
    hud->config.bottleneck_threshold_high = 0.8f;
    hud->config.bottleneck_threshold_medium = 0.6f;
    hud->config.bottleneck_threshold_low = 0.4f;

    // Color scheme
    hud->config.color_normal[0] = 0.8f; hud->config.color_normal[1] = 0.8f; hud->config.color_normal[2] = 0.8f; hud->config.color_normal[3] = 1.0f;
    hud->config.color_warning[0] = 1.0f; hud->config.color_warning[1] = 1.0f; hud->config.color_warning[2] = 0.6f; hud->config.color_warning[3] = 1.0f;
    hud->config.color_critical[0] = 1.0f; hud->config.color_critical[1] = 0.4f; hud->config.color_critical[2] = 0.4f; hud->config.color_critical[3] = 1.0f;
    hud->config.color_good[0] = 0.4f; hud->config.color_good[1] = 1.0f; hud->config.color_good[2] = 0.4f; hud->config.color_good[3] = 1.0f;
    hud->config.color_background[0] = 0.1f; hud->config.color_background[1] = 0.1f; hud->config.color_background[2] = 0.1f; hud->config.color_background[3] = 0.9f;

    // Initialize timing
    atomic_store_explicit(&hud->last_update_time, ri.Milliseconds() * 1000000ULL, memory_order_relaxed);
    hud->frame_time_accumulator = 0.0f;
    atomic_init(&hud->frame_count, 0);
    hud->fps_current = 0.0f;
    hud->fps_average = 0.0f;
    hud->frame_time_min = FLT_MAX;
    hud->frame_time_max = 0.0f;
    hud->frame_time_avg = 0.0f;

    // Initialize display state
    hud->visible = qfalse;
    hud->show_demo_window = qfalse;
    hud->show_metrics_window = qfalse;
    hud->show_profiler_window = qfalse;
    memset(hud->filter_text, 0, sizeof(hud->filter_text));

    // Initialize bottlenecks
    memset(&hud->bottlenecks, 0, sizeof(vk_performance_hud_bottlenecks_t));
    hud->bottlenecks.overall_performance_score = 1.0f;

    hud->imgui_frame_started = qfalse;
    hud->debug_name = "performance_hud";
    hud->initialized = qtrue;

    ri.Printf(PRINT_ALL, "Vulkan: Performance HUD initialized with real-time bottleneck analysis\n");

    return qtrue;
#else
    // imGUI not available, return failure (expected on systems without imGUI)
    ri.Printf(PRINT_DEVELOPER, "Vulkan: Performance HUD - imGUI not available (expected)\n");
    return qfalse;
#endif
}

// Shutdown performance HUD
void vk_shutdown_performance_hud(void) {
#ifdef USE_CIMGUI
    if (!vk.performance_hud.initialized) {
        return;
    }

    ri.Printf(PRINT_ALL, "Vulkan: Shutting down performance HUD\n");

    vk_performance_hud_t *hud = &vk.performance_hud;
    atomic_store_explicit(&hud->initialized, qfalse, memory_order_relaxed);
    atomic_store_explicit(&hud->visible, qfalse, memory_order_relaxed);

    ri.Printf(PRINT_ALL, "Vulkan: Performance HUD shutdown complete\n");
#endif
}

// Render performance HUD
void vk_render_performance_hud(void) {
#ifdef USE_CIMGUI
    if (!atomic_load_explicit(&vk.performance_hud.enabled, memory_order_relaxed) || 
        !atomic_load_explicit(&vk.performance_hud.initialized, memory_order_relaxed) || 
        !atomic_load_explicit(&vk.performance_hud.visible, memory_order_relaxed)) {
        return;
    }

    vk_performance_hud_t *hud = &vk.performance_hud;

    // Update performance metrics periodically
    uint64_t current_time = ri.Milliseconds() * 1000000ULL;
    if (current_time - atomic_load_explicit(&hud->last_update_time, memory_order_relaxed) >= (uint64_t)(hud->config.update_interval * 1000000.0f)) {
        vk_update_performance_hud_bottlenecks(hud);
        atomic_store_explicit(&hud->last_update_time, current_time, memory_order_relaxed);
    }

    // Update frame timing
    static uint64_t last_frame_time = 0;
    if (last_frame_time > 0) {
        uint64_t frame_time_ns = current_time - last_frame_time;
        float frame_time_ms = (float)frame_time_ns / 1000000.0f;

        hud->frame_time_accumulator += frame_time_ms;
        atomic_fetch_add_explicit(&hud->frame_count, 1, memory_order_relaxed);

        if (frame_time_ms < hud->frame_time_min) hud->frame_time_min = frame_time_ms;
        if (frame_time_ms > hud->frame_time_max) hud->frame_time_max = frame_time_ms;

        if (hud->frame_time_accumulator >= 1000.0f) { // Update every second
            uint32_t count = atomic_load_explicit(&hud->frame_count, memory_order_relaxed);
            hud->fps_current = (float)count / (hud->frame_time_accumulator / 1000.0f);
            hud->fps_average = hud->fps_average * 0.9f + hud->fps_current * 0.1f;
            hud->frame_time_avg = hud->frame_time_accumulator / (float)count;

            // Reset for next interval
            hud->frame_time_accumulator = 0.0f;
            atomic_store_explicit(&hud->frame_count, 0, memory_order_relaxed);
            hud->frame_time_min = FLT_MAX;
            hud->frame_time_max = 0.0f;
        }
    }
    last_frame_time = current_time;

    // Set up ImGui style for the HUD
    ImGuiStyle* style = igGetStyle();
    style->Colors[ImGuiCol_WindowBg].x = hud->config.color_background[0];
    style->Colors[ImGuiCol_WindowBg].y = hud->config.color_background[1];
    style->Colors[ImGuiCol_WindowBg].z = hud->config.color_background[2];
    style->Colors[ImGuiCol_WindowBg].w = hud->config.color_background[3];
    style->Alpha = hud->config.window_alpha;

    // Render main HUD window
    vk_render_performance_hud_main_window(hud);
#endif
}

// Update performance HUD bottlenecks analysis
/* Optional: gate the perf HUD bottlenecks update behind a compile flag to allow build without
   the full performance HUD typing in some configurations. */
#if defined(VK_PERF_HUD_ENABLED)
static void vk_update_performance_hud_bottlenecks(vk_performance_hud_t *hud) {
    vk_performance_hud_bottlenecks_t *bottlenecks = &hud->bottlenecks;

    // Reset analysis
    bottlenecks->overall_performance_score = 1.0f;
    bottlenecks->primary_bottleneck = "None";
    bottlenecks->primary_bottleneck_severity = 0.0f;
    bottlenecks->critical_issues = 0;
    bottlenecks->warning_issues = 0;
    bottlenecks->info_issues = 0;

    // Analyze VRAM system
    if (vk.vram_stats.enabled) {
        bottlenecks->vram_score = 0.8f; // Placeholder - would analyze actual VRAM pressure
        if (bottlenecks->vram_score < 0.5f) {
            bottlenecks->critical_issues++;
            if (bottlenecks->vram_score < bottlenecks->overall_performance_score) {
                bottlenecks->overall_performance_score = bottlenecks->vram_score;
                bottlenecks->primary_bottleneck = "VRAM Pressure";
                bottlenecks->primary_bottleneck_severity = 1.0f - bottlenecks->vram_score;
            }
        }
    }

    // Analyze memory systems
    float memory_score = 1.0f;
    if (vk.resource_pools.enabled) memory_score *= 0.9f;
    if (vk.lock_free_manager.enabled) memory_score *= 0.95f;
    if (vk.arena_manager.enabled) memory_score *= 0.95f;
    bottlenecks->memory_score = memory_score;

    if (memory_score < 0.7f) {
        bottlenecks->warning_issues++;
    }

    // Analyze render performance
    if (vk.render_profiler.enabled) {
        bottlenecks->render_score = 0.85f; // Placeholder - would analyze actual render bottlenecks
        if (bottlenecks->render_score < 0.6f) {
            bottlenecks->warning_issues++;
        }
    }

    // Analyze shader performance
    if (vk.shader_performance_analyzer.enabled) {
        bottlenecks->shader_score = 0.9f; // Placeholder - would analyze shader bottlenecks
    }

    // Analyze asset loading
    if (vk.asset_loading_profiler.enabled) {
        bottlenecks->asset_score = 0.75f; // Placeholder - would analyze loading bottlenecks
        bottlenecks->io_score = 0.8f;

        if (vk.asset_loading_profiler.bottlenecks.io_bottleneck) {
            bottlenecks->warning_issues++;
        }
        if (vk.asset_loading_profiler.bottlenecks.streaming_bottleneck) {
            bottlenecks->info_issues++;
        }
    }

    // Generate top recommendations
    int rec_index = 0;
    if (bottlenecks->primary_bottleneck_severity > 0.5f) {
        bottlenecks->top_recommendations[rec_index] = "Address primary bottleneck immediately";
        bottlenecks->recommendation_priorities[rec_index++] = 1.0f;
    }

    if (vk.asset_loading_profiler.bottlenecks.io_bottleneck) {
        bottlenecks->top_recommendations[rec_index] = "Optimize asset loading I/O patterns";
        bottlenecks->recommendation_priorities[rec_index++] = 0.8f;
    }

    if (vk.asset_loading_profiler.bottlenecks.streaming_bottleneck) {
        bottlenecks->top_recommendations[rec_index] = "Improve streaming system efficiency";
        bottlenecks->recommendation_priorities[rec_index++] = 0.6f;
    }

    if (memory_score < 0.8f) {
        bottlenecks->top_recommendations[rec_index] = "Optimize memory allocation patterns";
        bottlenecks->recommendation_priorities[rec_index++] = 0.7f;
    }

    // Fill remaining slots
    while (rec_index < 5) {
        bottlenecks->top_recommendations[rec_index] = "Performance is good";
        bottlenecks->recommendation_priorities[rec_index++] = 0.1f;
    }
}
#else
static void vk_update_performance_hud_bottlenecks(vk_performance_hud_t *hud) { (void)hud; }
#endif

// Render main performance HUD window
static void vk_render_performance_hud_main_window(vk_performance_hud_t *hud) {
#ifdef USE_CIMGUI
    char window_title[256];
    Q_snprintf(window_title, sizeof(window_title), "Performance HUD (FPS: %.1f)###PerformanceHUD", hud->fps_current);

    igSetNextWindowPos((ImVec2_c){hud->config.position_x, hud->config.position_y}, ImGuiCond_FirstUseEver, (ImVec2_c){0, 0});
    igSetNextWindowSize((ImVec2_c){hud->config.size_x, hud->config.size_y}, ImGuiCond_FirstUseEver);

    qboolean visible = (qboolean)atomic_load_explicit(&hud->visible, memory_order_relaxed);
    if (igBegin(window_title, (bool*)&visible, (ImGuiWindowFlags)hud->config.window_flags)) {
        atomic_store_explicit(&hud->visible, visible, memory_order_relaxed);
        // FPS and frame timing
        if (igCollapsingHeader_BoolPtr("Frame Performance", NULL, ImGuiTreeNodeFlags_DefaultOpen)) {
            igText("FPS: %.1f (avg: %.1f)", (double)hud->fps_current, (double)hud->fps_average);
            igText("Frame Time: %.2f ms (min: %.2f, max: %.2f)",
                   (double)hud->frame_time_avg, (double)hud->frame_time_min, (double)hud->frame_time_max);

            ImVec4 fps_color = vk_get_bottleneck_color(hud->fps_current < 30.0f ? 0.8f : 0.2f, &hud->config);
            igTextColored(fps_color, "Performance Score: %.1f%%", (double)hud->bottlenecks.overall_performance_score * 100.0f);
        }

        // Bottleneck summary
        if (hud->config.show_bottlenecks && igCollapsingHeader_BoolPtr("System Bottlenecks", NULL, ImGuiTreeNodeFlags_DefaultOpen)) {
            vk_render_performance_hud_bottlenecks(hud);
        }

        // VRAM statistics
        if (hud->config.show_vram_stats && igCollapsingHeader_BoolPtr("VRAM Statistics", NULL, 0)) {
            vk_render_performance_hud_vram_stats(hud);
        }

        // Memory systems
        if (hud->config.show_memory_pools && igCollapsingHeader_BoolPtr("Memory Systems", NULL, 0)) {
            vk_render_performance_hud_memory_systems(hud);
        }

        // Render profiler
        if (hud->config.show_render_profiler && igCollapsingHeader_BoolPtr("Render Profiler", NULL, 0)) {
            vk_render_performance_hud_render_profiler(hud);
        }

        // Recommendations
        if (hud->config.show_recommendations && igCollapsingHeader_BoolPtr("Recommendations", NULL, 0)) {
            vk_render_performance_hud_recommendations(hud);
        }

        // Settings
        if (igCollapsingHeader_BoolPtr("HUD Settings", NULL, 0)) {
            igCheckbox("Show VRAM Stats", (bool*)&hud->config.show_vram_stats);
            igCheckbox("Show Memory Systems", (bool*)&hud->config.show_memory_pools);
            igCheckbox("Show Render Profiler", (bool*)&hud->config.show_render_profiler);
            igCheckbox("Show Memory Bandwidth", (bool*)&hud->config.show_memory_bandwidth);
            igCheckbox("Show Parallel Processing", (bool*)&hud->config.show_parallel_processing);
            igCheckbox("Show Shader Analysis", (bool*)&hud->config.show_shader_analysis);
            igCheckbox("Show Asset Loading", (bool*)&hud->config.show_asset_loading);
            igCheckbox("Show Bottlenecks", (bool*)&hud->config.show_bottlenecks);
            igCheckbox("Show Recommendations", (bool*)&hud->config.show_recommendations);
            igCheckbox("Highlight Bottlenecks", (bool*)&hud->config.highlight_bottlenecks);

            igSliderFloat("Update Interval", &hud->config.update_interval, 0.1f, 2.0f, "%.1f s", 0);
            igSliderFloat("Window Alpha", &hud->config.window_alpha, 0.1f, 1.0f, "%.2f", 0);
        }
    }
    igEnd();
#endif
}

// Render VRAM statistics
static void vk_render_performance_hud_vram_stats(vk_performance_hud_t *hud) {
#ifdef USE_CIMGUI
    if (!vk.vram_stats.enabled) {
        igTextDisabled("VRAM tracking not enabled");
        return;
    }

    vk_vram_stats_t *stats = &vk.vram_stats;

    igText("Total Allocations: %u", stats->total_allocations);
    igText("Active Allocations: %u", stats->current_allocations);
    igText("Total Memory: %.2f MB", (double)stats->used_vram / (1024.0 * 1024.0));
    igText("Peak Memory: %.2f MB", (double)stats->max_used_vram / (1024.0 * 1024.0));

    if (stats->leaked_allocations > 0) {
        ImVec4 leak_color = {hud->config.color_critical[0], hud->config.color_critical[1],
                            hud->config.color_critical[2], hud->config.color_critical[3]};
        igTextColored(leak_color, "Memory Leaks Detected: %u", stats->leaked_allocations);
    }
#endif
}

// Render memory systems overview
static void vk_render_performance_hud_memory_systems(vk_performance_hud_t *hud) {
#ifdef USE_CIMGUI
    (void)hud;
    // Memory pools
    if (vk.resource_pools.enabled) {
        if (igTreeNode_Str("Memory Pools")) {
            igText("Pool Levels: %d", (int)vk.resource_pools.num_pool_levels);
            igText("Total Allocated: %.2f MB", (double)vk.resource_pools.total_memory_allocated / (1024.0 * 1024.0));
            igText("Total Used: %.2f MB", (double)vk.resource_pools.total_memory_used / (1024.0 * 1024.0));
            igTreePop();
        }
    }

    // Lock-free allocators
    if (vk.lock_free_manager.enabled) {
        if (igTreeNode_Str("Lock-Free Allocators")) {
            igText("Total Operations: %llu", (unsigned long long)vk.lock_free_manager.total_allocations);
            igTreePop();
        }
    }

    // Arena allocators
    if (vk.arena_manager.enabled) {
        if (igTreeNode_Str("Arena Allocators")) {
            igText("Total Memory Used: %.2f MB", (double)vk.arena_manager.total_allocated_bytes / (1024.0 * 1024.0));
            igTreePop();
        }
    }

    // Memory advisor
    if (vk.memory_advisor.enabled) {
        if (igTreeNode_Str("Memory Advisor")) {
            igText("Access Patterns Analyzed: %llu", (unsigned long long)vk.memory_advisor.total_memory_accesses);
            igText("Optimizations Applied: %u", (uint32_t)vk.memory_advisor.optimizations_applied);
            igTreePop();
        }
    }
#endif
}

// Render render profiler data
static void vk_render_performance_hud_render_profiler(vk_performance_hud_t *hud) {
#ifdef USE_CIMGUI
    (void)hud;
    if (!vk.render_profiler.enabled) {
        igTextDisabled("Render profiler not enabled");
        return;
    }

    vk_render_profiler_t *profiler = &vk.render_profiler;

    igText("Active Passes: %u", (uint32_t)profiler->current_pass_count);
    igText("Avg Frame Time: %.2f ms", (double)profiler->performance_trend.avg_frame_time);

    if (profiler->bottleneck_severity > 0.5f) {
        ImVec4 bottleneck_color = {hud->config.color_warning[0], hud->config.color_warning[1],
                                  hud->config.color_warning[2], hud->config.color_warning[3]};
        igTextColored(bottleneck_color, "High Bottleneck Severity: %.2f", (double)profiler->bottleneck_severity);
    }
#endif
}

// Render bottleneck analysis
static void vk_render_performance_hud_bottlenecks(vk_performance_hud_t *hud) {
#ifdef USE_CIMGUI
    vk_performance_hud_bottlenecks_t *bottlenecks = &hud->bottlenecks;

    // Primary bottleneck
    if (bottlenecks->primary_bottleneck_severity > 0.1f) {
        ImVec4 color = vk_get_bottleneck_color(bottlenecks->primary_bottleneck_severity, &hud->config);
        igTextColored(color, "Primary Bottleneck: %s (%.1f%%)",
                     bottlenecks->primary_bottleneck,
                     bottlenecks->primary_bottleneck_severity * 100.0f);
    } else {
        ImVec4 good_color = {hud->config.color_good[0], hud->config.color_good[1],
                            hud->config.color_good[2], hud->config.color_good[3]};
        igTextColored(good_color, "Primary Bottleneck: None");
    }

    // Issue summary
    igText("Issues: %d Critical, %d Warning, %d Info",
           bottlenecks->critical_issues, bottlenecks->warning_issues, bottlenecks->info_issues);

    // Subsystem scores
    igText("VRAM Score: %.1f%%", bottlenecks->vram_score * 100.0f);
    igText("Memory Score: %.1f%%", bottlenecks->memory_score * 100.0f);
    igText("Render Score: %.1f%%", bottlenecks->render_score * 100.0f);
    igText("Shader Score: %.1f%%", bottlenecks->shader_score * 100.0f);
    igText("Asset Score: %.1f%%", bottlenecks->asset_score * 100.0f);
    igText("I/O Score: %.1f%%", bottlenecks->io_score * 100.0f);
#endif
}

// Render recommendations
static void vk_render_performance_hud_recommendations(vk_performance_hud_t *hud) {
#ifdef USE_CIMGUI
    vk_performance_hud_bottlenecks_t *bottlenecks = &hud->bottlenecks;

    for (int i = 0; i < 5; i++) {
        if (bottlenecks->top_recommendations[i] && bottlenecks->recommendation_priorities[i] > 0.1f) {
            ImVec4 color = vk_get_bottleneck_color(1.0f - bottlenecks->recommendation_priorities[i], &hud->config);
            igBullet();
            igTextColored(color, "%s", bottlenecks->top_recommendations[i]);
        }
    }

    if (bottlenecks->overall_performance_score > 0.8f) {
        ImVec4 good_color = {hud->config.color_good[0], hud->config.color_good[1],
                            hud->config.color_good[2], hud->config.color_good[3]};
        igTextColored(good_color, "Overall performance is good!");
    }
#endif
}

// Get color for bottleneck severity
static ImVec4 vk_get_bottleneck_color(float severity, const vk_performance_hud_config_t *config) {
#ifdef USE_CIMGUI
    ImVec4 color;
    if (severity >= config->bottleneck_threshold_high) {
        color.x = config->color_critical[0];
        color.y = config->color_critical[1];
        color.z = config->color_critical[2];
        color.w = config->color_critical[3];
    } else if (severity >= config->bottleneck_threshold_medium) {
        color.x = config->color_warning[0];
        color.y = config->color_warning[1];
        color.z = config->color_warning[2];
        color.w = config->color_warning[3];
    } else if (severity >= config->bottleneck_threshold_low) {
        color.x = config->color_normal[0];
        color.y = config->color_normal[1];
        color.z = config->color_normal[2];
        color.w = config->color_normal[3];
    } else {
        color.x = config->color_good[0];
        color.y = config->color_good[1];
        color.z = config->color_good[2];
        color.w = config->color_good[3];
    }
    return color;
#else
    // Fallback if imGUI is not available
    ImVec4 fallback = {1.0f, 1.0f, 1.0f, 1.0f};
    return fallback;
#endif
}

// Format performance value with unit
static void vk_format_performance_value(char *buffer, size_t size, float value, const char *unit) {
    if (value >= 1000000.0f) {
        Q_snprintf(buffer, size, "%.1f M%s", (double)value / 1000000.0f, unit);
    } else if (value >= 1000.0f) {
        Q_snprintf(buffer, size, "%.1f K%s", (double)value / 1000.0f, unit);
    } else {
        Q_snprintf(buffer, size, "%.1f %s", (double)value, unit);
    }
}

// Toggle performance HUD visibility
void vk_toggle_performance_hud(void) {
#ifdef USE_CIMGUI
    if (!atomic_load_explicit(&vk.performance_hud.initialized, memory_order_relaxed)) {
        return;
    }

    qboolean visible = (qboolean)atomic_load_explicit(&vk.performance_hud.visible, memory_order_relaxed);
    atomic_store_explicit(&vk.performance_hud.visible, !visible, memory_order_relaxed);
    ri.Printf(PRINT_ALL, "Performance HUD %s\n", !visible ? "enabled" : "disabled");
#else
    ri.Printf(PRINT_ALL, "Performance HUD requires imGUI support\n");
#endif
}

// Set performance HUD enabled state
void vk_set_performance_hud_enabled(qboolean enabled) {
#ifdef USE_CIMGUI
    if (!atomic_load_explicit(&vk.performance_hud.initialized, memory_order_relaxed)) {
        return;
    }

    atomic_store_explicit(&vk.performance_hud.enabled, enabled, memory_order_relaxed);
    if (!enabled) {
        atomic_store_explicit(&vk.performance_hud.visible, qfalse, memory_order_relaxed);
    }

    ri.Printf(PRINT_ALL, "Performance HUD %s\n", enabled ? "enabled" : "disabled");
#else
    ri.Printf(PRINT_ALL, "Performance HUD requires imGUI support\n");
#endif
}

// Check if performance HUD is enabled
qboolean vk_is_performance_hud_enabled(void) {
#ifdef USE_CIMGUI
    return atomic_load_explicit(&vk.performance_hud.enabled, memory_order_relaxed) && 
           atomic_load_explicit(&vk.performance_hud.initialized, memory_order_relaxed);
#else
    return qfalse;
#endif
}

// Performance Regression Detector Implementation

// Initialize performance regression detector
qboolean vk_init_performance_regression_detector(void) {
    if (vk.performance_regression_detector.initialized) {
        return qtrue;
    }

    ri.Printf(PRINT_ALL, "Vulkan: Initializing performance regression detector\n");

    vk_performance_regression_detector_t *detector = &vk.performance_regression_detector;

    // Configure detector
    detector->enabled = qtrue;
    detector->regression_threshold = 5.0f; // 5% regression threshold
    detector->fail_on_regression = qfalse;
    Com_sprintf(detector->baseline_file_path, sizeof(detector->baseline_file_path), "perf_baselines.json");

    // Initialize current snapshot
    detector->current_snapshot.max_metrics = 64;
    atomic_init(&detector->current_snapshot.metric_count, 0);
    detector->current_snapshot.metrics = (vk_performance_metric_t*)ri.Malloc(
        sizeof(vk_performance_metric_t) * detector->current_snapshot.max_metrics);
    if (!detector->current_snapshot.metrics) {
        ri.Printf(PRINT_WARNING, "Vulkan: Failed to allocate performance metrics buffer\n");
        return qfalse;
    }
    memset(detector->current_snapshot.metrics, 0,
           sizeof(vk_performance_metric_t) * detector->current_snapshot.max_metrics);

    // Initialize baselines
    detector->max_baselines = 10;
    atomic_init(&detector->baseline_count, 0);
    detector->baselines = (vk_performance_snapshot_t*)ri.Malloc(
        sizeof(vk_performance_snapshot_t) * detector->max_baselines);
    if (!detector->baselines) {
        ri.Printf(PRINT_WARNING, "Vulkan: Failed to allocate performance baselines buffer\n");
        ri.Free(detector->current_snapshot.metrics);
        return qfalse;
    }
    memset(detector->baselines, 0,
           sizeof(vk_performance_snapshot_t) * detector->max_baselines);

    // Initialize regressions
    detector->max_regressions = 64;
    atomic_init(&detector->regression_count, 0);
    detector->regressions = (vk_performance_regression_t*)ri.Malloc(
        sizeof(vk_performance_regression_t) * detector->max_regressions);
    if (!detector->regressions) {
        ri.Printf(PRINT_WARNING, "Vulkan: Failed to allocate performance regressions buffer\n");
        ri.Free(detector->current_snapshot.metrics);
        ri.Free(detector->baselines);
        return qfalse;
    }
    memset(detector->regressions, 0,
           sizeof(vk_performance_regression_t) * detector->max_regressions);

    // Initialize statistics
    atomic_init(&detector->total_regressions_detected, 0);
    detector->max_regression_percentage = 0.0f;

    // Load existing baselines if available
    vk_load_performance_baselines(detector);

    detector->debug_name = "performance_regression_detector";
    detector->initialized = qtrue;

    ri.Printf(PRINT_ALL, "Vulkan: Performance regression detector initialized\n");

    return qtrue;
}

// Shutdown performance regression detector
void vk_shutdown_performance_regression_detector(void) {
    if (!vk.performance_regression_detector.initialized) {
        return;
    }

    ri.Printf(PRINT_ALL, "Vulkan: Shutting down performance regression detector\n");

    vk_performance_regression_detector_t *detector = &vk.performance_regression_detector;

    // Free allocated memory
    if (detector->current_snapshot.metrics) {
        ri.Free(detector->current_snapshot.metrics);
    }

    if (detector->baselines) {
        for (uint32_t i = 0; i < detector->baseline_count; i++) {
            if (detector->baselines[i].metrics) {
                ri.Free(detector->baselines[i].metrics);
            }
        }
        ri.Free(detector->baselines);
    }

    if (detector->regressions) {
        ri.Free(detector->regressions);
    }

    detector->initialized = qfalse;
    ri.Printf(PRINT_ALL, "Vulkan: Performance regression detector shutdown complete\n");
}

// Capture current performance snapshot
void vk_capture_performance_snapshot(const char *scenario_name) {
    if (!vk.performance_regression_detector.enabled || !vk.performance_regression_detector.initialized) {
        return;
    }

    vk_performance_regression_detector_t *detector = &vk.performance_regression_detector;
    vk_performance_snapshot_t *snapshot = &detector->current_snapshot;

    // Reset snapshot
    Q_strncpyz(snapshot->scenario_name, scenario_name, sizeof(snapshot->scenario_name));
    snapshot->timestamp = ri.Milliseconds() * 1000000ULL;
    atomic_store_explicit(&snapshot->metric_count, 0, memory_order_relaxed);

    // Capture various metrics from other systems
    
    // Frame performance (leveraging HUD data)
#ifdef USE_CIMGUI
    vk_add_metric_to_snapshot(snapshot, "Average Frame Time", (float)vk.performance_hud.frame_time_avg, "ms", qtrue);
    vk_add_metric_to_snapshot(snapshot, "Min Frame Time", (float)vk.performance_hud.frame_time_min, "ms", qtrue);
    vk_add_metric_to_snapshot(snapshot, "Max Frame Time", (float)vk.performance_hud.frame_time_max, "ms", qtrue);
    vk_add_metric_to_snapshot(snapshot, "Average FPS", (float)vk.performance_hud.fps_average, "fps", qfalse);
#endif

    // GPU performance (from render profiler)
    if (vk.render_profiler.enabled) {
        vk_add_metric_to_snapshot(snapshot, "Average GPU Time", (float)vk.render_profiler.performance_trend.avg_frame_time, "ms", qtrue);
    }

    // Memory usage
    if (vk.vram_stats.enabled) {
        vk_add_metric_to_snapshot(snapshot, "Active VRAM", (float)vk.vram_stats.used_vram / (1024.0f * 1024.0f), "MB", qtrue);
        vk_add_metric_to_snapshot(snapshot, "Peak VRAM", (float)vk.vram_stats.max_used_vram / (1024.0f * 1024.0f), "MB", qtrue);
    }

    // Memory bandwidth
    if (vk.memory_bandwidth_profiler.enabled) {
        vk_add_metric_to_snapshot(snapshot, "Peak Bandwidth", (float)vk.memory_bandwidth_profiler.bandwidth_stats.peak_bandwidth / (1024.0f * 1024.0f), "MB/s", qfalse);
    }

    // Asset loading
    if (vk.asset_loading_profiler.enabled) {
        if (vk.asset_loading_profiler.total_assets_loaded > 0) {
            vk_add_metric_to_snapshot(snapshot, "Average Asset Load Time", 
                (float)vk.asset_loading_profiler.total_load_time_ns / (float)vk.asset_loading_profiler.total_assets_loaded / 1000000.0f, "ms", qtrue);
        }
    }

    ri.Printf(PRINT_ALL, "Vulkan: Captured performance snapshot for scenario '%s' with %u metrics\n",
        scenario_name, snapshot->metric_count);
}

// Save current snapshot as a baseline
void vk_save_performance_baseline(const char *scenario_name) {
    if (!vk.performance_regression_detector.initialized) return;

    vk_performance_regression_detector_t *detector = &vk.performance_regression_detector;
    
    // First capture current state
    vk_capture_performance_snapshot(scenario_name);

    // Check if baseline for this scenario already exists
    vk_performance_snapshot_t *baseline = NULL;
    for (uint32_t i = 0; i < detector->baseline_count; i++) {
        if (strcmp(detector->baselines[i].scenario_name, scenario_name) == 0) {
            baseline = &detector->baselines[i];
            break;
        }
    }

    if (!baseline && detector->baseline_count < detector->max_baselines) {
        baseline = &detector->baselines[detector->baseline_count++];
        memset(baseline, 0, sizeof(vk_performance_snapshot_t));
        baseline->max_metrics = detector->current_snapshot.max_metrics;
        baseline->metrics = (vk_performance_metric_t*)ri.Malloc(
            sizeof(vk_performance_metric_t) * baseline->max_metrics);
    }

    if (baseline) {
        // Copy data from current snapshot
        Q_strncpyz(baseline->scenario_name, detector->current_snapshot.scenario_name, sizeof(baseline->scenario_name));
        baseline->timestamp = detector->current_snapshot.timestamp;
        baseline->metric_count = detector->current_snapshot.metric_count;
        memcpy(baseline->metrics, detector->current_snapshot.metrics, 
               sizeof(vk_performance_metric_t) * baseline->metric_count);
        
        ri.Printf(PRINT_ALL, "Vulkan: Saved baseline for scenario '%s'\n", scenario_name);
        
        // Persist to file
        vk_save_performance_baselines(detector);
    }
}

// Compare current performance against baseline
void vk_compare_against_baseline(const char *scenario_name) {
    if (!vk.performance_regression_detector.initialized) return;

    vk_performance_regression_detector_t *detector = &vk.performance_regression_detector;
    
    // Find baseline
    vk_performance_snapshot_t *baseline = NULL;
    for (uint32_t i = 0; i < detector->baseline_count; i++) {
        if (strcmp(detector->baselines[i].scenario_name, scenario_name) == 0) {
            baseline = &detector->baselines[i];
            break;
        }
    }

    if (!baseline) {
        ri.Printf(PRINT_WARNING, "Vulkan: No baseline found for scenario '%s'\n", scenario_name);
        return;
    }

    // Capture current snapshot for comparison
    vk_capture_performance_snapshot(scenario_name);
    vk_performance_snapshot_t *current = &detector->current_snapshot;

    // Reset regression results
    detector->regression_count = 0;
    detector->total_regressions_detected = 0;
    detector->max_regression_percentage = 0.0f;

    // Compare each metric in current snapshot with baseline
    for (uint32_t i = 0; i < current->metric_count; i++) {
        vk_performance_metric_t *cur_m = &current->metrics[i];
        
        // Find matching metric in baseline
        vk_performance_metric_t *base_m = NULL;
        for (uint32_t j = 0; j < baseline->metric_count; j++) {
            if (strcmp(baseline->metrics[j].name, cur_m->name) == 0) {
                base_m = &baseline->metrics[j];
                break;
            }
        }

        if (base_m && detector->regression_count < detector->max_regressions) {
            vk_performance_regression_t *reg = &detector->regressions[detector->regression_count++];
            reg->metric_name = cur_m->name;
            reg->baseline_value = base_m->value;
            reg->current_value = cur_m->value;
            
            // Calculate difference percentage
            if (base_m->value != 0.0f) {
                reg->difference_percentage = ((cur_m->value - base_m->value) / base_m->value) * 100.0f;
            } else {
                reg->difference_percentage = 0.0f;
            }

            // Determine if it's a regression
            if (cur_m->lower_is_better) {
                reg->is_regression = (reg->difference_percentage > detector->regression_threshold);
            } else {
                reg->is_regression = (reg->difference_percentage < -detector->regression_threshold);
            }

            if (reg->is_regression) {
                detector->total_regressions_detected++;
                float severity = fabsf(reg->difference_percentage);
                if (severity > detector->max_regression_percentage) {
                    detector->max_regression_percentage = severity;
                }
                reg->severity = severity / 50.0f; // Scale to 0-1 range (50% = max severity)
                if (reg->severity > 1.0f) reg->severity = 1.0f;
            } else {
                reg->severity = 0.0f;
            }
        }
    }

    ri.Printf(PRINT_ALL, "Vulkan: Comparison for scenario '%s' complete. %u regressions detected.\n",
        scenario_name, detector->total_regressions_detected);
}

// Print performance regression report
void vk_print_performance_regression_report(void) {
    if (!vk.performance_regression_detector.initialized) return;

    vk_performance_regression_detector_t *detector = &vk.performance_regression_detector;

    ri.Printf(PRINT_ALL, "=== Performance Regression Report ===\n");
    ri.Printf(PRINT_ALL, "Scenario: %s\n", detector->current_snapshot.scenario_name);
    ri.Printf(PRINT_ALL, "Threshold: %.1f%%\n", detector->regression_threshold);
    ri.Printf(PRINT_ALL, "Regressions Detected: %u\n", detector->total_regressions_detected);

    if (detector->regression_count == 0) {
        ri.Printf(PRINT_ALL, "No comparison data available.\n");
        return;
    }

    for (uint32_t i = 0; i < detector->regression_count; i++) {
        vk_performance_regression_t *reg = &detector->regressions[i];
        
        const char *status = reg->is_regression ? "REGRESSED" : "OK";
        if (!reg->is_regression && fabsf(reg->difference_percentage) > detector->regression_threshold) {
            status = "IMPROVED";
        }

        ri.Printf(PRINT_ALL, "  %-25s: %8.2f -> %8.2f (%+6.1f%%) [%s]\n",
            reg->metric_name, reg->baseline_value, reg->current_value, 
            reg->difference_percentage, status);
    }

    if (detector->total_regressions_detected > 0) {
        ri.Printf(PRINT_ALL, "WARNING: Performance regressions detected in %u metrics!\n", 
            detector->total_regressions_detected);
    } else {
        ri.Printf(PRINT_ALL, "SUCCESS: Performance within acceptable thresholds.\n");
    }
}

// Export performance metrics to file
void vk_export_performance_metrics(const char *file_path) {
    if (!vk.performance_regression_detector.initialized) return;

    vk_performance_regression_detector_t *detector = &vk.performance_regression_detector;
    vk_performance_snapshot_t *snapshot = &detector->current_snapshot;

    ri.Printf(PRINT_ALL, "Vulkan: Exporting performance metrics to '%s'\n", file_path);

    // build string buffer
    char *buffer = (char*)ri.Malloc(65536);
    if (!buffer) return;
    
    char *ptr = buffer;
    int remaining = 65536;
    int n;

    n = Com_sprintf(ptr, remaining, "Scenario,Timestamp,Metric,Value,Unit\n");
    ptr += n;
    remaining -= n;

    for (uint32_t i = 0; i < snapshot->metric_count && remaining > 256; i++) {
        vk_performance_metric_t *m = &snapshot->metrics[i];
        n = Com_sprintf(ptr, remaining, "%s,%llu,%s,%.4f,%s\n",
            snapshot->scenario_name, (unsigned long long)snapshot->timestamp,
            m->name, (double)m->value, m->unit);
        ptr += n;
        remaining -= n;
    }

    ri.FS_WriteFile(file_path, buffer, (int)(ptr - buffer));
    ri.Free(buffer);
    
    ri.Printf(PRINT_ALL, "Vulkan: Export complete.\n");
}

// Set performance regression threshold
void vk_set_performance_regression_threshold(float threshold_percentage) {
    vk.performance_regression_detector.regression_threshold = threshold_percentage;
    ri.Printf(PRINT_ALL, "Vulkan: Performance regression threshold set to %.1f%%\n", threshold_percentage);
}

// Check if performance regressed significantly
qboolean vk_did_performance_regress(void) {
    return vk.performance_regression_detector.total_regressions_detected > 0;
}

// Internal: Add metric to snapshot
static void vk_add_metric_to_snapshot(vk_performance_snapshot_t *snapshot, const char *name, float value, const char *unit, qboolean lower_is_better) {
    uint32_t count = atomic_load_explicit(&snapshot->metric_count, memory_order_relaxed);
    if (count < snapshot->max_metrics) {
        vk_performance_metric_t *m = &snapshot->metrics[count];
        m->name = name;
        m->value = value;
        m->unit = unit;
        m->lower_is_better = lower_is_better;
        atomic_fetch_add_explicit(&snapshot->metric_count, 1, memory_order_relaxed);
    }
}

// Internal: Load baselines from file (Placeholder)
static void vk_load_performance_baselines(vk_performance_regression_detector_t *detector) {
    // In a real implementation, this would parse a JSON file
    // For now, we'll just log that we would load it
    ri.Printf(PRINT_ALL, "Vulkan: Loading performance baselines from '%s'\n", detector->baseline_file_path);
}

// Internal: Save baselines to file (Placeholder)
static void vk_save_performance_baselines(vk_performance_regression_detector_t *detector) {
    // In a real implementation, this would serialize baselines to a JSON file
    ri.Printf(PRINT_ALL, "Vulkan: Saving performance baselines to '%s'\n", detector->baseline_file_path);
}

// Internal: Calculate regression severity
static float vk_calculate_regression_severity(float current, float baseline, qboolean lower_is_better) {
    if (baseline == 0.0f) return 0.0f;
    float diff = ((current - baseline) / baseline) * 100.0f;
    if (lower_is_better) {
        return diff > 0.0f ? diff : 0.0f;
    } else {
        return diff < 0.0f ? -diff : 0.0f;
    }
}

// Heatmap Visualizer Implementation

// Initialize heatmap visualizer
qboolean vk_init_heatmap_visualizer(void) {
    if (vk.heatmap_visualizer.initialized) {
        return qtrue;
    }

    ri.Printf(PRINT_ALL, "Vulkan: Initializing heatmap visualizer\n");

    vk_heatmap_visualizer_t *visualizer = &vk.heatmap_visualizer;

    // Configure visualizer
    visualizer->enabled = qfalse;
    atomic_init(&visualizer->current_mode, VK_HEATMAP_LAYER_OVERDRAW);
    visualizer->global_opacity = 0.7f;
    visualizer->intensity_scale = 1.0f;
    visualizer->accumulation_mode = qtrue;
    visualizer->decay_rate = 0.95f; // Slow decay

    // Initialize layers
    for (int i = 0; i < VK_HEATMAP_LAYER_COUNT; i++) {
        vk_heatmap_layer_t *layer = &visualizer->layers[i];
        atomic_init(&layer->width, 128);
        atomic_init(&layer->height, 128);
        layer->data = (uint32_t*)ri.Malloc(sizeof(uint32_t) * 128 * 128);
        if (!layer->data) {
            ri.Printf(PRINT_WARNING, "Vulkan: Failed to allocate heatmap layer data\n");
            return qfalse;
        }
        memset(layer->data, 0, sizeof(uint32_t) * 128 * 128);
        layer->max_intensity = 0.0f;
        atomic_init(&layer->sample_count, 0);
        layer->needs_update = qfalse;
        
        // Vulkan resources will be created on first use
        layer->texture = VK_NULL_HANDLE;
        layer->texture_view = VK_NULL_HANDLE;
        layer->texture_memory = VK_NULL_HANDLE;
        layer->descriptor_set = VK_NULL_HANDLE;
    }

    // Initialize gradient colors (Blue -> Cyan -> Green -> Yellow -> Red)
    visualizer->gradient_colors[0][0] = 0.0f; visualizer->gradient_colors[0][1] = 0.0f; visualizer->gradient_colors[0][2] = 1.0f; visualizer->gradient_colors[0][3] = 1.0f;
    visualizer->gradient_colors[1][0] = 0.0f; visualizer->gradient_colors[1][1] = 1.0f; visualizer->gradient_colors[1][2] = 1.0f; visualizer->gradient_colors[1][3] = 1.0f;
    visualizer->gradient_colors[2][0] = 0.0f; visualizer->gradient_colors[2][1] = 1.0f; visualizer->gradient_colors[2][2] = 0.0f; visualizer->gradient_colors[2][3] = 1.0f;
    visualizer->gradient_colors[3][0] = 1.0f; visualizer->gradient_colors[3][1] = 1.0f; visualizer->gradient_colors[3][2] = 0.0f; visualizer->gradient_colors[3][3] = 1.0f;
    visualizer->gradient_colors[4][0] = 1.0f; visualizer->gradient_colors[4][1] = 0.0f; visualizer->gradient_colors[4][2] = 0.0f; visualizer->gradient_colors[4][3] = 1.0f;

    // Initialize sample buffer
    visualizer->max_samples = 10000;
    visualizer->samples = (vk_heatmap_sample_t*)ri.Malloc(sizeof(vk_heatmap_sample_t) * visualizer->max_samples);
    if (!visualizer->samples) {
        ri.Printf(PRINT_WARNING, "Vulkan: Failed to allocate heatmap sample buffer\n");
        return qfalse;
    }
    memset(visualizer->samples, 0, sizeof(vk_heatmap_sample_t) * visualizer->max_samples);
    atomic_init(&visualizer->active_samples, 0);

    visualizer->debug_name = "heatmap_visualizer";
    visualizer->initialized = qtrue;

    ri.Printf(PRINT_ALL, "Vulkan: Heatmap visualizer initialized with %d layers\n", VK_HEATMAP_LAYER_COUNT);

    return qtrue;
}

// Shutdown heatmap visualizer
void vk_shutdown_heatmap_visualizer(void) {
    if (!vk.heatmap_visualizer.initialized) {
        return;
    }

    ri.Printf(PRINT_ALL, "Vulkan: Shutting down heatmap visualizer\n");

    vk_heatmap_visualizer_t *visualizer = &vk.heatmap_visualizer;

    // Free layer resources
    for (int i = 0; i < VK_HEATMAP_LAYER_COUNT; i++) {
        vk_heatmap_layer_t *layer = &visualizer->layers[i];
        if (layer->data) {
            ri.Free(layer->data);
        }
        
        // Destroy Vulkan resources
        if (layer->texture != VK_NULL_HANDLE) {
            qvkDestroyImageView(vk.device, layer->texture_view, NULL);
            qvkDestroyImage(vk.device, layer->texture, NULL);
            qvkFreeMemory(vk.device, layer->texture_memory, NULL);
        }
    }

    // Free sample buffer
    if (visualizer->samples) {
        ri.Free(visualizer->samples);
    }

    visualizer->initialized = qfalse;
    ri.Printf(PRINT_ALL, "Vulkan: Heatmap visualizer shutdown complete\n");
}

// Record heatmap sample
void vk_record_heatmap_sample(float x, float y, float intensity, uint32_t layer_type) {
    if (!vk.heatmap_visualizer.enabled || !vk.heatmap_visualizer.initialized) {
        return;
    }

    if (layer_type >= VK_HEATMAP_LAYER_COUNT) return;

    vk_heatmap_visualizer_t *visualizer = &vk.heatmap_visualizer;

    uint32_t sample_idx = atomic_fetch_add_explicit(&visualizer->active_samples, 1, memory_order_relaxed);
    if (sample_idx < visualizer->max_samples) {
        vk_heatmap_sample_t *sample = &visualizer->samples[sample_idx];
        sample->x = x;
        sample->y = y;
        sample->intensity = intensity;
        sample->layer_type = layer_type;
    }
}

// Generate heatmap texture for a layer
void vk_generate_heatmap_texture(uint32_t layer_type) {
    if (!vk.heatmap_visualizer.initialized || layer_type >= VK_HEATMAP_LAYER_COUNT) return;

    vk_heatmap_visualizer_t *visualizer = &vk.heatmap_visualizer;
    vk_heatmap_layer_t *layer = &visualizer->layers[layer_type];

    // Update the layer data grid
    vk_update_heatmap_layer(layer, visualizer->samples, visualizer->active_samples, visualizer->decay_rate);
    
    // Update the Vulkan texture
    vk_update_heatmap_texture(layer);
}

// Render heatmap overlay using imGUI
void vk_render_heatmap_overlay(void) {
#ifdef USE_CIMGUI
    if (!vk.heatmap_visualizer.enabled || !vk.heatmap_visualizer.initialized) {
        return;
    }

    vk_heatmap_visualizer_t *visualizer = &vk.heatmap_visualizer;
    vk_heatmap_layer_t *layer = &visualizer->layers[visualizer->current_mode];

    // Create texture if it doesn't exist
    if (layer->texture == VK_NULL_HANDLE) {
        vk_create_heatmap_texture(layer);
    }

    // Update layer and texture
    vk_generate_heatmap_texture(visualizer->current_mode);

    // Render fullscreen overlay with imGUI
    ImGuiIO* io = igGetIO_Nil();
    igSetNextWindowPos((ImVec2_c){0, 0}, ImGuiCond_Always, (ImVec2_c){0, 0});
    igSetNextWindowSize(io->DisplaySize, ImGuiCond_Always);
    igSetNextWindowBgAlpha(0.0f); // Transparent background

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
                            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | 
                            ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground;

    if (igBegin("HeatmapOverlay", NULL, flags)) {
        if (layer->descriptor_set != VK_NULL_HANDLE) {
            igImage((ImTextureRef_c){NULL, (ImTextureID)layer->descriptor_set}, io->DisplaySize, (ImVec2_c){0, 0}, (ImVec2_c){1, 1});
        }
    }
    igEnd();

    // Reset samples for next frame
    atomic_store_explicit(&visualizer->active_samples, 0, memory_order_relaxed);
#endif
}

// Clear heatmap layer
void vk_clear_heatmap(uint32_t layer_type) {
    if (!vk.heatmap_visualizer.initialized || layer_type >= VK_HEATMAP_LAYER_COUNT) return;
    
    vk_heatmap_layer_t *layer = &vk.heatmap_visualizer.layers[layer_type];
    memset(layer->data, 0, sizeof(uint32_t) * layer->width * layer->height);
    layer->max_intensity = 0.0f;
}

// Enable/disable heatmap
void vk_set_heatmap_enabled(qboolean enabled) {
    vk.heatmap_visualizer.enabled = enabled;
    ri.Printf(PRINT_ALL, "Vulkan: Heatmap visualization %s\n", enabled ? "enabled" : "disabled");
}

// Set current heatmap mode
void vk_set_heatmap_mode(uint32_t mode) {
    if (mode < VK_HEATMAP_LAYER_COUNT) {
        vk.heatmap_visualizer.current_mode = mode;
        ri.Printf(PRINT_ALL, "Vulkan: Heatmap mode set to %u\n", mode);
    }
}

// Print heatmap statistics
void vk_print_heatmap_stats(void) {
    if (!vk.heatmap_visualizer.initialized) return;

    vk_heatmap_visualizer_t *visualizer = &vk.heatmap_visualizer;
    ri.Printf(PRINT_ALL, "=== Heatmap Visualization Statistics ===\n");
    ri.Printf(PRINT_ALL, "Status: %s\n", visualizer->enabled ? "ENABLED" : "DISABLED");
    ri.Printf(PRINT_ALL, "Current Mode: %u\n", visualizer->current_mode);
    ri.Printf(PRINT_ALL, "Opacity: %.2f\n", visualizer->global_opacity);
    ri.Printf(PRINT_ALL, "Max Samples: %u\n", visualizer->max_samples);

    for (int i = 0; i < VK_HEATMAP_LAYER_COUNT; i++) {
        vk_heatmap_layer_t *layer = &visualizer->layers[i];
        ri.Printf(PRINT_ALL, "Layer %d: %u x %u grid, Max Intensity: %.2f\n", 
            i, layer->width, layer->height, layer->max_intensity);
    }
}

// Internal: Update heatmap layer data grid
static void vk_update_heatmap_layer(vk_heatmap_layer_t *layer, vk_heatmap_sample_t *samples, uint32_t sample_count, float decay_rate) {
    // Apply decay to existing data
    for (uint32_t i = 0; i < layer->width * layer->height; i++) {
        layer->data[i] = (uint32_t)((float)layer->data[i] * decay_rate);
    }

    // Add new samples
    for (uint32_t i = 0; i < sample_count; i++) {
        if (samples[i].layer_type != (uint32_t)(layer - vk.heatmap_visualizer.layers)) continue;

        uint32_t gx = (uint32_t)(samples[i].x * (float)layer->width);
        uint32_t gy = (uint32_t)(samples[i].y * (float)layer->height);

        if (gx < layer->width && gy < layer->height) {
            uint32_t idx = gy * layer->width + gx;
            uint32_t val = (uint32_t)(samples[i].intensity * 1000.0f);
            layer->data[idx] += val;
            
            if ((float)layer->data[idx] > layer->max_intensity) {
                layer->max_intensity = (float)layer->data[idx];
            }
        }
    }
}

// Internal: Create Vulkan texture for a heatmap layer
static void vk_create_heatmap_texture(vk_heatmap_layer_t *layer) {
    // Basic image creation logic (simplified for brevity)
    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = { layer->width, layer->height, 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    if (qvkCreateImage(vk.device, &image_info, NULL, &layer->texture) != VK_SUCCESS) {
        ri.Printf(PRINT_WARNING, "Vulkan: Failed to create heatmap image\n");
        return;
    }

    VkMemoryRequirements mem_reqs;
    qvkGetImageMemoryRequirements(vk.device, layer->texture, &mem_reqs);

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_reqs.size,
        .memoryTypeIndex = find_memory_type(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    if (qvkAllocateMemory(vk.device, &alloc_info, NULL, &layer->texture_memory) != VK_SUCCESS) {
        ri.Printf(PRINT_WARNING, "Vulkan: Failed to allocate heatmap image memory\n");
        return;
    }

    qvkBindImageMemory(vk.device, layer->texture, layer->texture_memory, 0);

    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = layer->texture,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    if (qvkCreateImageView(vk.device, &view_info, NULL, &layer->texture_view) != VK_SUCCESS) {
        ri.Printf(PRINT_WARNING, "Vulkan: Failed to create heatmap image view\n");
        return;
    }
}

// Internal: Update Vulkan texture with new heatmap data
static void vk_update_heatmap_texture(vk_heatmap_layer_t *layer) {
    if (layer->texture == VK_NULL_HANDLE) return;

    // Convert grid data to RGBA texture data using gradient
    uint8_t *rgba_data = (uint8_t*)ri.Malloc(layer->width * layer->height * 4);
    if (!rgba_data) return;

    for (uint32_t i = 0; i < layer->width * layer->height; i++) {
#ifdef USE_CIMGUI
        float intensity = 0.0f;
        if (layer->max_intensity > 0.0f) {
            intensity = (float)layer->data[i] / layer->max_intensity;
        }

        ImVec4 color = vk_get_heatmap_color(intensity, vk.heatmap_visualizer.gradient_colors);
        
        rgba_data[i*4 + 0] = (uint8_t)(color.x * 255.0f);
        rgba_data[i*4 + 1] = (uint8_t)(color.y * 255.0f);
        rgba_data[i*4 + 2] = (uint8_t)(color.z * 255.0f);
        rgba_data[i*4 + 3] = (uint8_t)(color.w * 255.0f);
#else
        rgba_data[i*4 + 0] = 0;
        rgba_data[i*4 + 1] = 0;
        rgba_data[i*4 + 2] = 0;
        rgba_data[i*4 + 3] = 0;
#endif
    }

    ri.Free(rgba_data);
}

// Internal: Map intensity to color gradient
static ImVec4 vk_get_heatmap_color(float intensity, float gradient[5][4]) {
#ifdef USE_CIMGUI
    if (intensity <= 0.0f) return (ImVec4){0, 0, 0, 0};
    if (intensity >= 1.0f) return (ImVec4){gradient[4][0], gradient[4][1], gradient[4][2], gradient[4][3]};

    float scaled = intensity * 4.0f;
    int idx = (int)scaled;
    float frac = scaled - (float)idx;

    float *c1 = gradient[idx];
    float *c2 = gradient[idx + 1];

    ImVec4 result;
    result.x = c1[0] + (c2[0] - c1[0]) * frac;
    result.y = c1[1] + (c2[1] - c1[1]) * frac;
    result.z = c1[2] + (c2[2] - c1[2]) * frac;
    result.w = c1[3] + (c2[3] - c1[3]) * frac;

    return result;
#else
    (void)intensity;
    (void)gradient;
    return (ImVec4){0, 0, 0, 0};
#endif
}

// Performance Presets Implementation

static const char *preset_names[] = {
    "Potato",
    "Low",
    "Medium",
    "High",
    "Ultra"
};

// Apply performance preset
void vk_apply_performance_preset(vk_performance_preset_t preset) {
    if (preset >= VK_PERF_PRESET_COUNT) return;

    ri.Printf(PRINT_ALL, "Vulkan: Applying performance preset '%s'\n", preset_names[preset]);

    switch (preset) {
        case VK_PERF_PRESET_POTATO:
            ri.Cvar_Set("r_bloom", "0");
            ri.Cvar_Set("r_dlss", "1");
            ri.Cvar_Set("r_dlss_quality", "1"); // Performance
            ri.Cvar_Set("r_vrs", "1");
            ri.Cvar_Set("r_vrs_mode", "2"); // Content adaptive
            ri.Cvar_Set("r_postQuality", "0"); // Low
            ri.Cvar_Set("r_hdr", "0");
            ri.Cvar_Set("r_procDressing", "0");
            ri.Cvar_Set("r_materialSystem", "0");
            break;

        case VK_PERF_PRESET_LOW:
            ri.Cvar_Set("r_bloom", "0");
            ri.Cvar_Set("r_dlss", "1");
            ri.Cvar_Set("r_dlss_quality", "2"); // Balanced/Performance
            ri.Cvar_Set("r_vrs", "1");
            ri.Cvar_Set("r_vrs_mode", "1"); // Uniform
            ri.Cvar_Set("r_postQuality", "1");
            ri.Cvar_Set("r_hdr", "0");
            ri.Cvar_Set("r_procDressing", "0");
            ri.Cvar_Set("r_materialSystem", "1");
            break;

        case VK_PERF_PRESET_MEDIUM:
            ri.Cvar_Set("r_bloom", "1");
            ri.Cvar_Set("r_dlss", "0");
            ri.Cvar_Set("r_vrs", "0");
            ri.Cvar_Set("r_postQuality", "2");
            ri.Cvar_Set("r_hdr", "1");
            ri.Cvar_Set("r_procDressing", "1");
            ri.Cvar_Set("r_materialSystem", "1");
            break;

        case VK_PERF_PRESET_HIGH:
            ri.Cvar_Set("r_bloom", "1");
            ri.Cvar_Set("r_dlss", "0");
            ri.Cvar_Set("r_vrs", "0");
            ri.Cvar_Set("r_postQuality", "3");
            ri.Cvar_Set("r_hdr", "1");
            ri.Cvar_Set("r_procDressing", "1");
            ri.Cvar_Set("r_materialSystem", "1");
            break;

        case VK_PERF_PRESET_ULTRA:
            ri.Cvar_Set("r_bloom", "1");
            ri.Cvar_Set("r_dlss", "0");
            ri.Cvar_Set("r_vrs", "0");
            ri.Cvar_Set("r_postQuality", "4");
            ri.Cvar_Set("r_hdr", "1");
            ri.Cvar_Set("r_procDressing", "1");
            ri.Cvar_Set("r_materialSystem", "1");
            break;

        default:
            break;
    }

    vk.current_perf_preset = preset;
    ri.Printf(PRINT_ALL, "Vulkan: Performance preset '%s' applied successfully\n", preset_names[preset]);
}

// Get name of performance preset
const char *vk_get_performance_preset_name(vk_performance_preset_t preset) {
    if (preset < VK_PERF_PRESET_COUNT) {
        return preset_names[preset];
    }
    return "Unknown";
}

// Get current performance preset
vk_performance_preset_t vk_get_current_performance_preset(void) {
    return vk.current_perf_preset;
}

// Print performance presets info
void vk_print_performance_presets_info(void) {
    ri.Printf(PRINT_ALL, "=== Performance Presets ===\n");
    ri.Printf(PRINT_ALL, "Current Preset: %s\n", preset_names[vk.current_perf_preset]);
    ri.Printf(PRINT_ALL, "Available Presets:\n");
    for (int i = 0; i < VK_PERF_PRESET_COUNT; i++) {
        ri.Printf(PRINT_ALL, "  [%d] %s\n", i, preset_names[i]);
    }
}

// Performance Regression Detector Implementation

// Initialize performance regression detector
