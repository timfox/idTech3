#include "vk_irradiance_cache.h"

void vk_irradiance_cache_init(VkDevice device) {
    // Initialize the irradiance cache
    memset(&vk_irradiance_cache, 0, sizeof(vk_irradiance_cache_t));

    // Create the irradiance cache image
    vk_create_image(&vk_irradiance_cache.image, 1024, 1024, 1);

    // Create the irradiance cache image view
    vk_create_image_view(&vk_irradiance_cache.image, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT);
    
    // Create the irradiance cache descriptor set
    vk_create_descriptor_set(&vk_irradiance_cache.descriptor_set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    // Create the irradiance cache pipeline
    vk_create_pipeline(&vk_irradiance_cache.pipeline, VK_SHADER_STAGE_COMPUTE_BIT);

    // Create the irradiance cache command buffer
    vk_create_command_buffer(&vk_irradiance_cache.command_buffer);
    
    // Create the irradiance cache fence
    vk_create_fence(&vk_irradiance_cache.fence);

    // Create the irradiance cache semaphore
    vk_create_semaphore(&vk_irradiance_cache.semaphore);

    // Create the irradiance cache command pool
    vk_create_command_pool(&vk_irradiance_cache.command_pool);
}

void vk_irradiance_cache_destroy(VkDevice device) {
    // Destroy the irradiance cache
    vk_destroy_image(&vk_irradiance_cache.image);
    vk_destroy_image_view(&vk_irradiance_cache.image);
    vk_destroy_descriptor_set(&vk_irradiance_cache.descriptor_set);
    vk_destroy_pipeline(&vk_irradiance_cache.pipeline);
    vk_destroy_command_buffer(&vk_irradiance_cache.command_buffer);
    vk_destroy_fence(&vk_irradiance_cache.fence);
    vk_destroy_semaphore(&vk_irradiance_cache.semaphore);
    vk_destroy_command_pool(&vk_irradiance_cache.command_pool);
}