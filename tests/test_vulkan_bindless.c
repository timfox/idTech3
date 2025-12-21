/*
=============================================================================
Vulkan Bindless Resource Tests

Tests for bindless texture and buffer systems.
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

// Test bindless texture registration framework
TEST(bindless_texture_framework) {
	test_vk_device_t test_device;
	
	if (!test_vk_create_test_device(&test_device)) {
		return;
	}

	// Test image creation (prerequisite for bindless textures)
	VkImage image;
	VkDeviceMemory memory;
	VkImageView image_view;
	
	qboolean result = test_vk_create_test_image(&test_device, 256, 256,
		VK_FORMAT_R8G8B8A8_UNORM, &image, &memory, &image_view);
	
	ASSERT_EQ(result, qtrue);
	ASSERT_NOT_NULL((void*)image);
	ASSERT_NOT_NULL((void*)image_view);

	// Cleanup
	PFN_vkDestroyImageView pfn_vkDestroyImageView = NULL;
	PFN_vkDestroyImage pfn_vkDestroyImage = NULL;
	PFN_vkFreeMemory pfn_vkFreeMemory = NULL;
	pfn_vkDestroyImageView = vkDestroyImageView;
	pfn_vkDestroyImage = vkDestroyImage;
	pfn_vkFreeMemory = vkFreeMemory;
	
	if (pfn_vkDestroyImageView && pfn_vkDestroyImage && pfn_vkFreeMemory) {
		pfn_vkDestroyImageView(test_device.device, image_view, NULL);
		pfn_vkDestroyImage(test_device.device, image, NULL);
		pfn_vkFreeMemory(test_device.device, memory, NULL);
	}

	test_vk_destroy_test_device(&test_device);
}

// Test bindless buffer registration
TEST(bindless_buffer_framework) {
	test_vk_device_t test_device;
	
	if (!test_vk_create_test_device(&test_device)) {
		return;
	}

	// Test buffer creation (prerequisite for bindless buffers)
	VkBuffer buffer;
	VkDeviceMemory memory;
	
	qboolean result = test_vk_create_test_buffer(&test_device, 1024,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &buffer, &memory);
	
	ASSERT_EQ(result, qtrue);
	ASSERT_NOT_NULL((void*)buffer);

	// Cleanup
	PFN_vkDestroyBuffer pfn_vkDestroyBuffer = NULL;
	PFN_vkFreeMemory pfn_vkFreeMemory = NULL;
	pfn_vkDestroyBuffer = vkDestroyBuffer;
	pfn_vkFreeMemory = vkFreeMemory;
	
	if (pfn_vkDestroyBuffer && pfn_vkFreeMemory) {
		pfn_vkDestroyBuffer(test_device.device, buffer, NULL);
		pfn_vkFreeMemory(test_device.device, memory, NULL);
	}

	test_vk_destroy_test_device(&test_device);
}

int main(void) {
	test_count = 0;
	test_passed = 0;
	test_failed = 0;

	test_bindless_texture_framework();
	test_bindless_buffer_framework();

	printf("\nBindless tests: %d passed, %d failed out of %d total\n",
		test_passed, test_failed, test_count);

	return (test_failed == 0) ? 0 : 1;
}

#else

int main(void) {
	printf("Bindless tests skipped (USE_VULKAN not defined)\n");
	return 0;
}

#endif

