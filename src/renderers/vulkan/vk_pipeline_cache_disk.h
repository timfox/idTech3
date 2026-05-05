#pragma once

#include "../common/vulkan/vulkan.h"

void vk_pipeline_cache_create( const VkPhysicalDeviceProperties *props );
void vk_pipeline_cache_save( void );
