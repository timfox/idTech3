// Lightweight bridge-focus: provide concrete definitions for core Vulkan bridge symbols
// to ensure linkage even when the real implementations are still being wired up.

#include "vk.h"

// C linkage wrappers for bridge symbols
#ifdef __cplusplus
extern "C" {
#endif

// Real entry points (weak fallbacks are provided elsewhere; these are strong definitions here)
VkCommandBuffer vk_begin_command_buffer(void) { return VK_NULL_HANDLE; }
void vk_end_command_buffer(VkCommandBuffer cb, const char* location) { (void)cb; (void)location; }
void vk_destroy_swapchain(void) { }
VkSurfaceFormatKHR vk_present_format = {};

// Bridge helpers
VkCommandBuffer vk_begin_command_buffer_bridge(void) { return vk_begin_command_buffer(); }
void vk_end_command_buffer_bridge(VkCommandBuffer cb, const char* location) { vk_end_command_buffer(cb, location); }
void vk_destroy_swapchain_bridge(void) { vk_destroy_swapchain(); }
VkSurfaceFormatKHR vk_present_format_bridge(void) { return vk_present_format; }

#ifdef __cplusplus
}
#endif

