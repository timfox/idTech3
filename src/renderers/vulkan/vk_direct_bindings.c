/*
=============================================================================
Direct Vulkan Helper Bindings (Plan D baseline)
=============================================================================
Minimal stub implementations for direct Vulkan API access.
These are weak symbols that can be overridden by real implementations
in vk_command_buffers.cpp and vk_swapchain.cpp.

Note: Real implementations are provided in:
- vk_command_buffers.cpp: vk_begin_command_buffer, vk_end_command_buffer
- vk_swapchain.cpp: vk_destroy_swapchain
=============================================================================
*/

#include <stdint.h>
#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

// Stub implementations - real versions are in vk_command_buffers.cpp
VkCommandBuffer vk_begin_command_buffer(void) {
    // Real implementation in vk_command_buffers.cpp allocates and begins recording
    return VK_NULL_HANDLE;
}

void vk_end_command_buffer(VkCommandBuffer cb, const char* location) {
    // Real implementation in vk_command_buffers.cpp ends recording and submits
    (void)cb;
    (void)location;
}

// Stub implementation - real version is in vk_swapchain.cpp
void vk_destroy_swapchain(void) {
    // Real implementation in vk_swapchain.cpp destroys swapchain and related resources
}

#ifdef __cplusplus
}
#endif

#ifdef __GNUC__
#define WEAK __attribute__((weak))
#else
#define WEAK
#endif

// Weak symbol for present format - can be overridden by real implementation
WEAK VkSurfaceFormatKHR vk_present_format = {};

