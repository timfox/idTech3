#pragma once

#include "../common/vulkan/vulkan.h"
#include "tr_common.h"

/* Vulkan validation layer: debug report callback and error consumption.
 * Used during instance creation and by tr_init for startup validation messages.
 */

/* Callback for VkDebugReportCallbackEXT. Pass to vkCreateDebugReportCallbackEXT. */
VKAPI_ATTR VkBool32 VKAPI_CALL vk_validation_debug_callback(
	VkDebugReportFlagsEXT flags,
	VkDebugReportObjectTypeEXT object_type,
	uint64_t object,
	size_t location,
	int32_t message_code,
	const char *layer_prefix,
	const char *message,
	void *user_data );
