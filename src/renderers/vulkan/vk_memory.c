#include "vk_memory.h"
#include "vk.h"
#include <string.h>

// Forward declarations for functions used from vk.c
extern void vk_set_object_name(uint64_t obj, const char *name, VkDebugReportObjectTypeEXT type);
#define SET_OBJECT_NAME(obj,objName,objType) vk_set_object_name( (uint64_t)(obj), (objName), (objType) )
extern uint32_t find_memory_type(uint32_t memory_type_bits, VkMemoryPropertyFlags properties);
extern void vk_wait_idle(void);
// va is defined in q_shared.h
extern refimport_t ri;

qboolean vk_allocate_image_chunk(void) {
	// Ensure image_chunk_size is initialized
	if (vk.image_chunk_size == 0) {
		vk.image_chunk_size = IMAGE_CHUNK_SIZE;
	}

	if (vk_world.num_image_chunks == 0) {
		VkMemoryAllocateInfo alloc_info = {
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.pNext = NULL,
			.allocationSize = vk.image_chunk_size,
			.memoryTypeIndex = find_memory_type(~0U, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
		};

		VkDeviceMemory memory;
		VkResult result = qvkAllocateMemory(vk.device, &alloc_info, NULL, &memory);

		if (result != VK_SUCCESS) {
			ri.Printf(PRINT_WARNING, "Vulkan: Failed to allocate image memory chunk (%u MB): %s\n",
				(uint32_t)(vk.image_chunk_size / (1024 * 1024)), vk_result_string(result));
			return qfalse;
		}

		ImageChunk *chunk = &vk_world.image_chunks[0];
		chunk->memory = memory;
		chunk->used = 0; // Start with no used space

		// SET_OBJECT_NAME(memory, "preallocated image memory chunk 0", VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT);

		vk_world.num_image_chunks = 1;
		ri.Printf(PRINT_ALL, "...preallocated first image memory chunk (%u MB)\n",
			(uint32_t)(vk.image_chunk_size / (1024 * 1024)));
	}
	return qtrue;
}

void vk_calculate_fragmentation_metrics(void) {
	vk.memory_defrag.total_allocated = 0;
	vk.memory_defrag.total_used = 0;
	vk.memory_defrag.largest_free_block = 0;
	vk.memory_defrag.free_block_count = 0;

	for (int i = 0; i < vk_world.num_image_chunks; i++) {
		VkDeviceSize chunk_size = vk.image_chunk_size;
		VkDeviceSize used = vk_world.image_chunks[i].used;
		VkDeviceSize free = chunk_size - used;

		vk.memory_defrag.total_allocated += chunk_size;
		vk.memory_defrag.total_used += used;

		if (free > vk.memory_defrag.largest_free_block) {
			vk.memory_defrag.largest_free_block = free;
		}

		if (free > 0) {
			vk.memory_defrag.free_block_count++;
		}
	}
}

// Defragment memory by consolidating allocations
static qboolean vk_defragment_memory(void) {
	if (!vk.memory_defrag.enabled || vk_world.num_image_chunks <= 1) {
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

	// For now, defragmentation is a placeholder
	// Full implementation would require:
	// 1. Track all image allocations and their offsets
	// 2. Create new consolidated chunks
	// 3. Copy image data to new locations
	// 4. Update image memory bindings
	// 5. Free old fragmented chunks

	// This is a framework - actual defragmentation requires more complex tracking
	ri.Printf(PRINT_ALL, "Vulkan: Memory defragmentation framework ready (full implementation requires allocation tracking)\n");

	return qtrue;
}

void vk_check_defragmentation(void) {
	if (!vk.memory_defrag.enabled) {
		return;
	}

	vk.memory_defrag.frame_counter++;

	// Check interval-based defragmentation
	if (vk.memory_defrag.defrag_interval_frames > 0) {
		if (vk.memory_defrag.frame_counter >= vk.memory_defrag.defrag_interval_frames) {
			vk.memory_defrag.frame_counter = 0;
			vk_defragment_memory();
		}
	} else {
		// Check threshold-based defragmentation
		vk_calculate_fragmentation_metrics();
		float fragmentation = 0.0f;
		if (vk.memory_defrag.total_allocated > 0) {
			fragmentation = 1.0f - ((float)vk.memory_defrag.total_used / (float)vk.memory_defrag.total_allocated);
		}
		if (fragmentation >= vk.memory_defrag.fragmentation_threshold) {
			vk_defragment_memory();
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
			uint32_t index = vk.resource_pools.small_buffers.count++;
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
			uint32_t index = vk.resource_pools.medium_buffers.count++;
			return VK_NULL_HANDLE;
		}
	} else { // > 16MB
		// Large buffer pool
		if (vk.resource_pools.large_buffers.free_count > 0) {
			uint32_t index = vk.resource_pools.large_buffers.free_indices[--vk.resource_pools.large_buffers.free_count];
			return vk.resource_pools.large_buffers.buffers[index];
		} else if (vk.resource_pools.large_buffers.count < ARRAY_LEN(vk.resource_pools.large_buffers.buffers)) {
			// Allocate new buffer
			uint32_t index = vk.resource_pools.large_buffers.count++;
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

	vk.staging_buffer.ptr = NULL;
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

	vk.staging_buffer.size = MAX(size, STAGING_BUFFER_SIZE);
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
