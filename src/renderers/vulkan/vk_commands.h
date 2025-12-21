#ifndef __VK_COMMANDS_H__
#define __VK_COMMANDS_H__

#include <vulkan/vulkan.h>
#include <stdint.h>
#include "vk.h"


// Function prototypes for command buffer and pool management
VkCommandPool VK_CreateCommandPool(VkDevice device, uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags);
void VK_DestroyCommandPool(VkDevice device, VkCommandPool commandPool);
VkCommandBuffer* VK_AllocateCommandBuffers(VkDevice device, VkCommandPool commandPool, VkCommandBufferLevel level, uint32_t commandBufferCount);
void VK_FreeCommandBuffers(VkDevice device, VkCommandPool commandPool, uint32_t commandBufferCount, VkCommandBuffer* commandBuffers);

#endif // __VK_COMMANDS_H__
