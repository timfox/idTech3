#ifndef __VK_DESCRIPTORS_H__
#define __VK_DESCRIPTORS_H__

#include "vk.h"

// Descriptor management functions
qboolean vk_create_descriptor_pool(void);
void vk_init_descriptors(void);
void vk_update_attachment_descriptors(void);
void vk_update_uniform_descriptor(VkDescriptorSet descriptor, VkBuffer buffer);

// Sampler management (moved from vk.c)
VkSampler vk_find_sampler(const Vk_Sampler_Def *def);

#endif // __VK_DESCRIPTORS_H__
