// Lightweight stubs for Vulkan command-buffer helpers to enable smoke tests
// These are weakly-linked fallbacks; when real implementations are present they
// will override these symbols.

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __GNUC__
#define WEAK __attribute__((weak))
#else
#define WEAK
#endif

// Forward declarations to ensure linkage if the real ones exist elsewhere
WEAK VkCommandBuffer vk_begin_command_buffer(void);
WEAK void vk_end_command_buffer(VkCommandBuffer cb, const char* location);
WEAK void vk_destroy_swapchain(void);
WEAK VkSurfaceFormatKHR vk_present_format; // weakly-linked variable; real symbol provided elsewhere


#ifdef __cplusplus
}
#endif

// Lightweight weak bridges to ensure symbols exist for linking if real bridge is missing
WEAK VkCommandBuffer vk_begin_command_buffer_bridge(void) { return VK_NULL_HANDLE; }
WEAK void vk_end_command_buffer_bridge(VkCommandBuffer cb, const char* location) { (void)cb; (void)location; }
WEAK void vk_destroy_swapchain_bridge(void) { }
WEAK VkSurfaceFormatKHR vk_present_format_bridge(void) { VkSurfaceFormatKHR f = {}; return f; }

