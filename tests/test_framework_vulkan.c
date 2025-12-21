/*
=============================================================================
Vulkan Test Framework Utilities Implementation
=============================================================================
*/

#include "test_framework_vulkan.h"
#include "test_framework.h"

#ifdef USE_VULKAN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

// Mock implementations for standalone tests
// Note: Com_Printf should be defined in test_framework.h or each test file
// We don't define it here to avoid multiple definition errors
// Com_Memset is already defined as a macro in q_shared.h (maps to memset)

// Minimal Vulkan instance creation for testing
static qboolean test_vk_create_instance(VkInstance *instance) {
	VkApplicationInfo app_info = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pNext = NULL,
		.pApplicationName = "idtech3_test",
		.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
		.pEngineName = "idtech3",
		.engineVersion = VK_MAKE_VERSION(1, 0, 0),
		.apiVersion = VK_API_VERSION_1_0
	};

	VkInstanceCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.pApplicationInfo = &app_info,
		.enabledLayerCount = 0,
		.ppEnabledLayerNames = NULL,
		.enabledExtensionCount = 0,
		.ppEnabledExtensionNames = NULL
	};

	// Load Vulkan library and get vkCreateInstance
	// Note: This is simplified - full implementation would use platform-specific library loading
	PFN_vkCreateInstance pfn_vkCreateInstance = NULL;
	// For testing, we assume Vulkan is already loaded
	// In a real implementation, this would load the library dynamically
	// For now, use the global vkCreateInstance from Vulkan loader
	pfn_vkCreateInstance = vkCreateInstance;

	if (!pfn_vkCreateInstance) {
		return qfalse;
	}

	VkResult result = pfn_vkCreateInstance(&create_info, NULL, instance);
	return (result == VK_SUCCESS);
}

qboolean test_vk_create_test_device(test_vk_device_t *test_device) {
	if (!test_device) {
		return qfalse;
	}

	Com_Memset(test_device, 0, sizeof(*test_device));

	// Create instance
	if (!test_vk_create_instance(&test_device->instance)) {
		return qfalse;
	}

	// Enumerate physical devices
	uint32_t device_count = 0;
	PFN_vkEnumeratePhysicalDevices pfn_vkEnumeratePhysicalDevices = NULL;
	// Load function pointer (simplified)
	pfn_vkEnumeratePhysicalDevices = vkEnumeratePhysicalDevices;
	
	if (!pfn_vkEnumeratePhysicalDevices) {
		return qfalse;
	}

	VkResult res = pfn_vkEnumeratePhysicalDevices(test_device->instance, &device_count, NULL);
	if (res != VK_SUCCESS || device_count == 0) {
		return qfalse;
	}

	VkPhysicalDevice *devices = (VkPhysicalDevice*)malloc(device_count * sizeof(VkPhysicalDevice));
	res = pfn_vkEnumeratePhysicalDevices(test_device->instance, &device_count, devices);
	if (res != VK_SUCCESS) {
		free(devices);
		return qfalse;
	}

	// Select first device
	test_device->physical_device = devices[0];
	free(devices);

	// Get queue family properties
	PFN_vkGetPhysicalDeviceQueueFamilyProperties pfn_vkGetPhysicalDeviceQueueFamilyProperties = NULL;
	// Load function pointer (simplified)
	pfn_vkGetPhysicalDeviceQueueFamilyProperties = vkGetPhysicalDeviceQueueFamilyProperties;

	uint32_t queue_family_count = 0;
	pfn_vkGetPhysicalDeviceQueueFamilyProperties(test_device->physical_device, &queue_family_count, NULL);
	if (queue_family_count == 0) {
		return qfalse;
	}

	VkQueueFamilyProperties *queue_props = (VkQueueFamilyProperties*)malloc(
		queue_family_count * sizeof(VkQueueFamilyProperties));
	pfn_vkGetPhysicalDeviceQueueFamilyProperties(test_device->physical_device, &queue_family_count, queue_props);

	// Find graphics queue
	test_device->queue_family_index = ~0U;
	for (uint32_t i = 0; i < queue_family_count; i++) {
		if (queue_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			test_device->queue_family_index = i;
			break;
		}
	}
	free(queue_props);

	if (test_device->queue_family_index == ~0U) {
		return qfalse;
	}

	// Create device
	const float queue_priority = 1.0f;
	VkDeviceQueueCreateInfo queue_info = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.queueFamilyIndex = test_device->queue_family_index,
		.queueCount = 1,
		.pQueuePriorities = &queue_priority
	};

	VkPhysicalDeviceFeatures features = {0};
	VkDeviceCreateInfo device_info = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.queueCreateInfoCount = 1,
		.pQueueCreateInfos = &queue_info,
		.enabledLayerCount = 0,
		.ppEnabledLayerNames = NULL,
		.enabledExtensionCount = 0,
		.ppEnabledExtensionNames = NULL,
		.pEnabledFeatures = &features
	};

	PFN_vkCreateDevice pfn_vkCreateDevice = NULL;
	// Load function pointer (simplified)
	pfn_vkCreateDevice = vkCreateDevice;

	res = pfn_vkCreateDevice(test_device->physical_device, &device_info, NULL, &test_device->device);
	if (res != VK_SUCCESS) {
		return qfalse;
	}

	// Get queue
	PFN_vkGetDeviceQueue pfn_vkGetDeviceQueue = NULL;
	// Load function pointer (simplified)
	pfn_vkGetDeviceQueue = vkGetDeviceQueue;

	pfn_vkGetDeviceQueue(test_device->device, test_device->queue_family_index, 0, &test_device->queue);

	test_device->initialized = qtrue;
	return qtrue;
}

void test_vk_destroy_test_device(test_vk_device_t *test_device) {
	if (!test_device || !test_device->initialized) {
		return;
	}

	PFN_vkDestroyDevice pfn_vkDestroyDevice = NULL;
	PFN_vkDestroyInstance pfn_vkDestroyInstance = NULL;
	// Load function pointers (simplified)
	pfn_vkDestroyDevice = vkDestroyDevice;
	pfn_vkDestroyInstance = vkDestroyInstance;

	if (test_device->device != VK_NULL_HANDLE) {
		pfn_vkDestroyDevice(test_device->device, NULL);
		test_device->device = VK_NULL_HANDLE;
	}

	if (test_device->instance != VK_NULL_HANDLE) {
		pfn_vkDestroyInstance(test_device->instance, NULL);
		test_device->instance = VK_NULL_HANDLE;
	}

	Com_Memset(test_device, 0, sizeof(*test_device));
}

qboolean test_vk_create_test_buffer(test_vk_device_t *test_device, VkDeviceSize size,
	VkBufferUsageFlags usage, VkBuffer *buffer, VkDeviceMemory *memory) {
	if (!test_device || !test_device->initialized || !buffer || !memory) {
		return qfalse;
	}

	PFN_vkCreateBuffer pfn_vkCreateBuffer = NULL;
	PFN_vkGetBufferMemoryRequirements pfn_vkGetBufferMemoryRequirements = NULL;
	PFN_vkAllocateMemory pfn_vkAllocateMemory = NULL;
	PFN_vkBindBufferMemory pfn_vkBindBufferMemory = NULL;
	// Load function pointers (simplified)
	pfn_vkCreateBuffer = vkCreateBuffer;
	pfn_vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
	pfn_vkAllocateMemory = vkAllocateMemory;
	pfn_vkBindBufferMemory = vkBindBufferMemory;

	VkBufferCreateInfo buffer_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.size = size,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = NULL
	};

	VkResult res = pfn_vkCreateBuffer(test_device->device, &buffer_info, NULL, buffer);
	if (res != VK_SUCCESS) {
		return qfalse;
	}

	VkMemoryRequirements mem_requirements;
	pfn_vkGetBufferMemoryRequirements(test_device->device, *buffer, &mem_requirements);

	VkMemoryAllocateInfo alloc_info = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = NULL,
		.allocationSize = mem_requirements.size,
		.memoryTypeIndex = test_vk_find_memory_type(test_device, mem_requirements.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
	};

	res = pfn_vkAllocateMemory(test_device->device, &alloc_info, NULL, memory);
	if (res != VK_SUCCESS) {
		return qfalse;
	}

	res = pfn_vkBindBufferMemory(test_device->device, *buffer, *memory, 0);
	return (res == VK_SUCCESS);
}

qboolean test_vk_create_test_image(test_vk_device_t *test_device, uint32_t width, uint32_t height,
	VkFormat format, VkImage *image, VkDeviceMemory *memory, VkImageView *image_view) {
	if (!test_device || !test_device->initialized || !image || !memory || !image_view) {
		return qfalse;
	}

	PFN_vkCreateImage pfn_vkCreateImage = NULL;
	PFN_vkGetImageMemoryRequirements pfn_vkGetImageMemoryRequirements = NULL;
	PFN_vkAllocateMemory pfn_vkAllocateMemory = NULL;
	PFN_vkBindImageMemory pfn_vkBindImageMemory = NULL;
	PFN_vkCreateImageView pfn_vkCreateImageView = NULL;
	// Load function pointers (simplified)
	pfn_vkCreateImage = vkCreateImage;
	pfn_vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
	pfn_vkAllocateMemory = vkAllocateMemory;
	pfn_vkBindImageMemory = vkBindImageMemory;
	pfn_vkCreateImageView = vkCreateImageView;

	VkImageCreateInfo image_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = format,
		.extent = {width, height, 1},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_SAMPLED_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = NULL,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};

	VkResult res = pfn_vkCreateImage(test_device->device, &image_info, NULL, image);
	if (res != VK_SUCCESS) {
		return qfalse;
	}

	VkMemoryRequirements mem_requirements;
	pfn_vkGetImageMemoryRequirements(test_device->device, *image, &mem_requirements);

	VkMemoryAllocateInfo alloc_info = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = NULL,
		.allocationSize = mem_requirements.size,
		.memoryTypeIndex = test_vk_find_memory_type(test_device, mem_requirements.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
	};

	res = pfn_vkAllocateMemory(test_device->device, &alloc_info, NULL, memory);
	if (res != VK_SUCCESS) {
		return qfalse;
	}

	res = pfn_vkBindImageMemory(test_device->device, *image, *memory, 0);
	if (res != VK_SUCCESS) {
		return qfalse;
	}

	VkImageViewCreateInfo view_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.image = *image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = format,
		.components = {
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY
		},
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		}
	};

	res = pfn_vkCreateImageView(test_device->device, &view_info, NULL, image_view);
	return (res == VK_SUCCESS);
}

void test_vk_check_result(VkResult result, const char *operation) {
	if (result != VK_SUCCESS) {
		test_count++;
		Com_Printf("FAIL: %s:%d: Vulkan operation '%s' failed with result: %d\n", 
			__func__, __LINE__, operation ? operation : "unknown", result);
		test_failed++;
	} else {
		test_count++;
		test_passed++;
	}
}

uint32_t test_vk_find_memory_type(test_vk_device_t *test_device, uint32_t type_filter,
	VkMemoryPropertyFlags properties) {
	if (!test_device || !test_device->initialized) {
		return ~0U;
	}

	PFN_vkGetPhysicalDeviceMemoryProperties pfn_vkGetPhysicalDeviceMemoryProperties = NULL;
	// Load function pointer (simplified)
	pfn_vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;

	VkPhysicalDeviceMemoryProperties mem_props;
	pfn_vkGetPhysicalDeviceMemoryProperties(test_device->physical_device, &mem_props);

	for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
		if ((type_filter & (1 << i)) && (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	return ~0U;
}

#endif // USE_VULKAN

