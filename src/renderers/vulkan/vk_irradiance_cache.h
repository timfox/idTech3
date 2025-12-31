#ifndef __VK_IRRADIANCE_CACHE_H__
#define __VK_IRRADIANCE_CACHE_H__

#include "vk.h"

#ifdef __cplusplus
extern "C" {
#endif
#define VK_IRRADIANCE_CACHE_SIZE 1024 // 1024x1024

typedef struct {
    VkImage image;
    VkImageView image_view;
    VkDescriptorSet descriptor_set;
    VkDescriptorSetLayout descriptor_set_layout;
    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
    VkCommandBuffer command_buffer;
    VkFence fence;
    VkSemaphore semaphore;
    VkCommandPool command_pool;
} vk_irradiance_cache_t;

extern vk_irradiance_cache_t vk_irradiance_cache;

void vk_irradiance_cache_init(VkDevice device);
void vk_irradiance_cache_destroy(VkDevice device);

#ifdef __cplusplus
}
#endif

#endif // __VK_IRRADIANCE_CACHE_H__