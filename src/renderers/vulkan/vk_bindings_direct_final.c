// Single source of truth for direct Vulkan bindings
#include <stdint.h>
#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

// These functions are implemented in vk_command_buffers.cpp and vk_swapchain.cpp
extern VkCommandBuffer vk_begin_command_buffer(void);
extern void vk_end_command_buffer(VkCommandBuffer cb, const char* location);
extern void vk_destroy_swapchain(void);
extern VkSurfaceFormatKHR vk_present_format;

#ifdef __cplusplus
}
#endif
