/*
=============================================================================
Vulkan Async Compute Tests

Tests for async compute queue and synchronization.
=============================================================================
*/

#include "test_framework.h"
#include "test_framework_vulkan.h"

#ifdef USE_VULKAN

#include <stdlib.h>

// Mock functions
void Com_Printf(const char *fmt, ...);
void Com_Error(errorParm_t level, const char *error, ...);
void *ri_Malloc(size_t size);
void ri_Free(void *ptr);

void Com_Printf(const char *fmt, ...) { (void)fmt; }
void Com_Error(errorParm_t level, const char *error, ...) { (void)level; (void)error; exit(1); }
void *ri_Malloc(size_t size) { return malloc(size); }
void ri_Free(void *ptr) { free(ptr); }

// Test compute queue creation
TEST(compute_queue_creation) {
	test_vk_device_t test_device;
	
	if (!test_vk_create_test_device(&test_device)) {
		return;
	}

	// Verify queue was created
	ASSERT_NOT_NULL((void*)test_device.queue);
	ASSERT_NE(test_device.queue_family_index, ~0U);

	test_vk_destroy_test_device(&test_device);
}

// Test compute command buffer creation
TEST(compute_command_buffer) {
	test_vk_device_t test_device;
	
	if (!test_vk_create_test_device(&test_device)) {
		return;
	}

	// Create command pool for compute
	PFN_vkCreateCommandPool pfn_vkCreateCommandPool = NULL;
	PFN_vkAllocateCommandBuffers pfn_vkAllocateCommandBuffers = NULL;
	PFN_vkDestroyCommandPool pfn_vkDestroyCommandPool = NULL;
	PFN_vkFreeCommandBuffers pfn_vkFreeCommandBuffers = NULL;
	// Load function pointers (simplified)
	pfn_vkCreateCommandPool = vkCreateCommandPool;
	pfn_vkAllocateCommandBuffers = vkAllocateCommandBuffers;
	pfn_vkDestroyCommandPool = vkDestroyCommandPool;
	pfn_vkFreeCommandBuffers = vkFreeCommandBuffers;

	if (pfn_vkCreateCommandPool && pfn_vkAllocateCommandBuffers) {
		VkCommandPoolCreateInfo pool_info = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.pNext = NULL,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = test_device.queue_family_index
		};

		VkCommandPool command_pool;
		VkResult res = pfn_vkCreateCommandPool(test_device.device, &pool_info, NULL, &command_pool);
		ASSERT_EQ(res, VK_SUCCESS);

		VkCommandBufferAllocateInfo alloc_info = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.pNext = NULL,
			.commandPool = command_pool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1
		};

		VkCommandBuffer cmd_buffer;
		res = pfn_vkAllocateCommandBuffers(test_device.device, &alloc_info, &cmd_buffer);
		ASSERT_EQ(res, VK_SUCCESS);

		// Cleanup
		if (pfn_vkFreeCommandBuffers && pfn_vkDestroyCommandPool) {
			pfn_vkFreeCommandBuffers(test_device.device, command_pool, 1, &cmd_buffer);
			pfn_vkDestroyCommandPool(test_device.device, command_pool, NULL);
		}
	}

	test_vk_destroy_test_device(&test_device);
}

int main(void) {
	test_count = 0;
	test_passed = 0;
	test_failed = 0;

	test_compute_queue_creation();
	test_compute_command_buffer();

	printf("\nAsync compute tests: %d passed, %d failed out of %d total\n",
		test_passed, test_failed, test_count);

	return (test_failed == 0) ? 0 : 1;
}

#else

int main(void) {
	printf("Async compute tests skipped (USE_VULKAN not defined)\n");
	return 0;
}

#endif

