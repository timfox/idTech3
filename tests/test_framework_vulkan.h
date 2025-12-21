/*
=============================================================================
Vulkan Test Framework Utilities

Helper functions for creating Vulkan test environments and resources.
=============================================================================
*/

#ifndef TEST_FRAMEWORK_VULKAN_H
#define TEST_FRAMEWORK_VULKAN_H

#ifdef USE_VULKAN

#include "../src/renderers/renderercommon/vulkan/vulkan.h"
#include "test_framework.h"

// Test Vulkan device structure
typedef struct {
	VkInstance instance;
	VkPhysicalDevice physical_device;
	VkDevice device;
	VkQueue queue;
	uint32_t queue_family_index;
	qboolean initialized;
} test_vk_device_t;

// Initialize minimal Vulkan device for testing
qboolean test_vk_create_test_device(test_vk_device_t *test_device);

// Cleanup test device
void test_vk_destroy_test_device(test_vk_device_t *test_device);

// Create test buffer
qboolean test_vk_create_test_buffer(test_vk_device_t *test_device, VkDeviceSize size,
	VkBufferUsageFlags usage, VkBuffer *buffer, VkDeviceMemory *memory);

// Create test image
qboolean test_vk_create_test_image(test_vk_device_t *test_device, uint32_t width, uint32_t height,
	VkFormat format, VkImage *image, VkDeviceMemory *memory, VkImageView *image_view);

// Check Vulkan result and assert on failure
void test_vk_check_result(VkResult result, const char *operation);

// Helper to get memory type index
uint32_t test_vk_find_memory_type(test_vk_device_t *test_device, uint32_t type_filter,
	VkMemoryPropertyFlags properties);

#endif // USE_VULKAN

#endif // TEST_FRAMEWORK_VULKAN_H

