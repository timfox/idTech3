#ifndef VK_LINK_BRIDGE_H
#define VK_LINK_BRIDGE_H

// Bridging wrappers to ensure C-linkage for critical Vulkan helper symbols.
 #ifdef __cplusplus
extern "C" {
 #endif

// Forward declarations of bridging wrappers
VkCommandBuffer vk_begin_command_buffer_bridge(void);
void vk_end_command_buffer_bridge(VkCommandBuffer cb, const char* location);
void vk_destroy_swapchain_bridge(void);
VkSurfaceFormatKHR vk_present_format_bridge(void);

 #ifdef __cplusplus
}
 #endif

#endif // VK_LINK_BRIDGE_H

