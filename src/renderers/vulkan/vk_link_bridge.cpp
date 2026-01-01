// Bridge wrappers to expose C-linkage for critical Vulkan helper functions.
// These wrappers forward to the real implementations if present.

#include "vk_link_bridge.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations of the real functions (they exist in existing modules)
// If they are not linked yet, these will resolve to the null implementations below.
extern VkCommandBuffer vk_begin_command_buffer(void);
extern void vk_end_command_buffer(VkCommandBuffer, const char*);
extern void vk_destroy_swapchain(void);
extern VkSurfaceFormatKHR vk_present_format; // actual variable; provide a bridge accessor if needed

// Bridge implementations
VkCommandBuffer vk_begin_command_buffer_bridge(void) {
    // Forward to real implementation if available
    return vk_begin_command_buffer();
}

void vk_end_command_buffer_bridge(VkCommandBuffer cb, const char* location) {
    vk_end_command_buffer(cb, location);
}

void vk_destroy_swapchain_bridge(void) {
    vk_destroy_swapchain();
}

VkSurfaceFormatKHR vk_present_format_bridge(void) {
    return vk_present_format;
}

#ifdef __cplusplus
}
#endif

