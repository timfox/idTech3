#ifndef VK_POST_AA_H
#define VK_POST_AA_H

#include "../common/vulkan/vulkan.h"

qboolean vk_post_aa_output_active( void );
void vk_post_scene_aa_apply( void );
/* Run SMAA/FXAA on an explicit color source (e.g. Temporal Reconstruction output for r_aaMode 5). */
qboolean vk_post_scene_aa_apply_from( VkImageView color_source );

#endif
