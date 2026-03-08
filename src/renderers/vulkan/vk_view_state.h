#pragma once

#include "../common/vulkan/vulkan.h"

void vk_reset_scene_src_rect_tracking( void );
void vk_begin_motion_frame( void );
void vk_get_scissor_rect( VkRect2D *r );
void vk_update_depth_range( Vk_Depth_Range depth_range );
