// Single source of truth for direct Vulkan bindings
#include <stdint.h>
#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif
VkCommandBuffer vk_begin_command_buffer(void) { return VK_NULL_HANDLE; }
void vk_end_command_buffer(VkCommandBuffer cb, const char* location) { (void)cb; (void)location; }
void vk_destroy_swapchain(void) { }
VkSurfaceFormatKHR vk_present_format = {};
#ifdef __cplusplus
}
#endif
