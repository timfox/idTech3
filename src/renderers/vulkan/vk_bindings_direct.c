// Direct bindings consolidated into a single C file
#include <stdint.h>
#include <vulkan/vulkan.h>

/** Provide concrete bindings with C linkage. These are the single source of truth. **/
VkCommandBuffer vk_begin_command_buffer(void) { return VK_NULL_HANDLE; }
void vk_end_command_buffer(VkCommandBuffer cb, const char* location) { (void)cb; (void)location; }
void vk_destroy_swapchain(void) { }
VkSurfaceFormatKHR vk_present_format = {};
