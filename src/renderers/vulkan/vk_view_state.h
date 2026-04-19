#pragma once

#include "../common/vulkan/vulkan.h"

/* OpenGL-style column-major projection -> Vulkan clip (Y flip on m22/m23 stack row). */
void vk_get_projection_matrix_vk( const float *projection_matrix, float *projection_vk );

void vk_reset_scene_src_rect_tracking( void );
qboolean vk_get_scene_src_rect( VkRect2D *out_rect );
void vk_begin_motion_frame( void );
void vk_get_scissor_rect( VkRect2D *r );
void vk_update_depth_range( Vk_Depth_Range depth_range );
