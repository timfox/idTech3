// Lightweight weak stubs for core Vulkan binding symbols to ease incremental linking.
// These are intended to be overridden by real implementations when available.

#include <vulkan/vulkan.h>

#ifdef __GNUC__
#define WEAK __attribute__((weak))
#else
#define WEAK
#endif

extern "C" {
WEAK VkCommandBuffer vk_begin_command_buffer(void);
WEAK void vk_end_command_buffer(VkCommandBuffer cb, const char* location);
WEAK void vk_destroy_swapchain(void);
WEAK VkSurfaceFormatKHR vk_present_format;
}

// Weak default implementations (enforce C linkage)
extern "C" {
WEAK VkCommandBuffer vk_begin_command_buffer(void) { return VK_NULL_HANDLE; }
WEAK void vk_end_command_buffer(VkCommandBuffer cb, const char* location) { (void)cb; (void)location; }
WEAK void vk_destroy_swapchain(void) { }
WEAK VkSurfaceFormatKHR vk_present_format = {};
}

// Bridge wrappers are implemented in dedicated bridge file (vk_bridge_focus.cpp)

