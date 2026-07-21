#pragma once

#include "../common/vulkan/vulkan.h"

/* OpenGL-style column-major projection -> Vulkan clip (Y flip on m22/m23 stack row). */
void vk_get_projection_matrix_vk( const float *projection_matrix, float *projection_vk );

/* Effective color pass resolution (r_renderScale / FBO); falls back to glConfig. */
uint32_t vk_get_render_target_width( void );
uint32_t vk_get_render_target_height( void );

void vk_reset_scene_src_rect_tracking( void );
qboolean vk_get_scene_src_rect( VkRect2D *out_rect );
void vk_begin_motion_frame( void );
/* Copy morphActiveWeight -> morphGpuWeightPrev for all refdef entities (GPU morph motion vectors). */
void vk_snap_gpu_morph_weights_for_motion( void );
void vk_get_scissor_rect( VkRect2D *r );
void vk_update_depth_range( Vk_Depth_Range depth_range );
void vk_update_mvp( const float *m );
void vk_read_mvp_transform( float *mvp );
void vk_read_prev_mvp_transform( float *prev_mvp );
void vk_print_viewmodel_projection_f( void );
