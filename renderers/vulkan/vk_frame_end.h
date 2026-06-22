#pragma once

#include "../common/vulkan/vulkan.h"

void vk_end_frame_record_capture_if_needed( void );
void vk_end_frame_prepare_post_process( VkImageView *post_fog_src, VkImageView *luminance_src );
void vk_end_frame_record_taa_pass( VkImageView *post_fog_src, VkImageView *luminance_src );
void vk_end_frame_record_luminance_pass( VkImageView luminance_src );
void vk_end_frame_record_gamma_pass( VkImageView post_fog_src );
