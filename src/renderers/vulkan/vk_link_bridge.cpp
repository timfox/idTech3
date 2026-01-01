// Plan D: Bridge wrappers removed. This file is deprecated; direct Vulkan usage is used.

#include "vk_link_bridge.h"
#include "vk.h"
#include "vk_link_bridge.h"
#include <stdint.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations of the real functions (they exist in existing modules)
// If they are not linked yet, these will resolve to the null implementations below.
extern "C" VkCommandBuffer vk_begin_command_buffer(void);
extern "C" void vk_end_command_buffer(VkCommandBuffer, const char*);
extern "C" void vk_destroy_swapchain(void);
extern "C" VkSurfaceFormatKHR vk_present_format;

// Bridge implementations
// bridge removed

// bridge removed

// bridge removed

// bridge removed

// bridge removed
// Implement CPU wait as a bridge-level helper
#include <stdint.h>
#include <stdbool.h>
extern VkSemaphoreTimelineWaitInfoKHR vkTimelineWaitInfo_placeholder;
 #ifndef VK_KHR_TIMELINE_SEMAPHORE
 #define VK_KHR_TIMELINE_SEMAPHORE 0
 #endif

// Best-effort CPU wait for a timeline value
qboolean vk_cpu_wait_for_timeline(uint64_t target_value, uint64_t timeout_ns) {
#ifdef VK_KHR_TIMELINE_SEMAPHORE
  if (vk.timeline_semaphore == VK_NULL_HANDLE || !qvkWaitSemaphoresKHR) return qtrue;
  VkSemaphoreWaitInfo waitInfo = {0};
  VkTimelineSemaphoreWaitInfoKHR timelineInfo = {0};
  waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO_KHR;
  waitInfo.pNext = &timelineInfo;
  waitInfo.semaphoreCount = 1;
  waitInfo.pSemaphores = &vk.timeline_semaphore;
  timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_WAIT_INFO_KHR;
  timelineInfo.waitSemaphoreValueCount = 1;
  timelineInfo.pWaitSemaphoreValues = &target_value;
  waitInfo.pNext = &timelineInfo;
  VkResult res = qvkWaitSemaphoresKHR(vk.device, &waitInfo, timeout_ns);
  return (res == VK_SUCCESS);
#else
  (void)target_value; (void)timeout_ns;
  return qtrue;
#endif
}

// CPU-side wait for a timeline value
#include <time.h>
/**
 * Wait for a timeline semaphore to reach the given target value.
 * If timeline semaphores are not available, returns true.
 */
qboolean vk_cpu_wait_for_timeline(uint64_t target_value, uint64_t timeout_ns) {
#ifdef VK_KHR_TIMELINE_SEMAPHORE
    if (vk.timeline_semaphore == VK_NULL_HANDLE || !qvkWaitSemaphoresKHR) {
        return qtrue;
    }
    VkSemaphoreWaitInfo waitInfo = {0};
    VkTimelineSemaphoreWaitInfoKHR timelineInfo = {0};
    waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO_KHR;
    waitInfo.pNext = &timelineInfo;
    waitInfo.semaphoreCount = 1;
    waitInfo.pSemaphores = &vk.timeline_semaphore;
    timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_WAIT_INFO_KHR;
    timelineInfo.pNext = NULL;
    timelineInfo.waitSemaphoreValueCount = 1;
    timelineInfo.pWaitSemaphoreValues = &target_value;
    timelineInfo.pNext = NULL;
    VkResult res = qvkWaitSemaphoresKHR(vk.device, &waitInfo, timeout_ns);
    return (res == VK_SUCCESS);
#else
    (void)target_value;
    (void)timeout_ns;
    return qtrue;
#endif
}

#ifdef __cplusplus
}
#endif

