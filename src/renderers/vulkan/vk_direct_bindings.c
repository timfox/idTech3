// Direct Vulkan helper bindings (Plan D baseline, minimal no-ops)
#include <stdint.h>
#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif
VkCommandBuffer vk_begin_command_buffer(void) { return VK_NULL_HANDLE; }
void vk_end_command_buffer(VkCommandBuffer cb, const char* location) { (void)cb; (void)location; }
void vk_destroy_swapchain(void) { }
#ifdef __cplusplus
}
#endif

#ifdef __GNUC__
#define WEAK __attribute__((weak))
#else
#define WEAK
#endif
WEAK VkSurfaceFormatKHR vk_present_format = {};

// Direct Vulkan helper bindings (Plan D baseline, minimal no-ops)
#include <stdint.h>
#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif
VkCommandBuffer vk_begin_command_buffer(void) { return VK_NULL_HANDLE; }
void vk_end_command_buffer(VkCommandBuffer cb, const char* location) { (void)cb; (void)location; }
void vk_destroy_swapchain(void) { }
#ifdef __cplusplus
}
#endif

#ifdef __GNUC__
#define WEAK __attribute__((weak))
#else
#define WEAK
#endif
WEAK VkSurfaceFormatKHR vk_present_format = {};

