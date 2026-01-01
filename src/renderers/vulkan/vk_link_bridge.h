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
// CPU-side wait for a timeline value (see Plan A)
// Returns qtrue on success or if timeline is not available.
qboolean vk_cpu_wait_for_timeline(uint64_t target_value, uint64_t timeout_ns);
bool vk_calibrated_supported(void);
void vk_calibrated_begin_frame(uint32_t frameIndex, const char* label);
void vk_calibrated_end_frame(uint32_t frameIndex, const char* label, uint64_t duration_ns);
void vk_calibrated_begin_pass(const char* name);
void vk_calibrated_end_pass(const char* name, uint64_t duration_ns);

#ifdef __cplusplus
}
 #endif

#endif // VK_LINK_BRIDGE_H

