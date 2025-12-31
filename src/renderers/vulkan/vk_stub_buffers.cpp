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

// No direct definitions here; rely on real implementations or bridge wrappers
// for actual symbol resolution. Prototypes are provided above as WEAK.
WEAK void vk_destroy_swapchain(void);

#ifdef __cplusplus
}
#endif

