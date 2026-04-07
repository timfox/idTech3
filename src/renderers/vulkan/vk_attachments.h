#ifndef VK_ATTACHMENTS_H
#define VK_ATTACHMENTS_H

void vk_create_attachments( void );
void vk_destroy_attachments( void );

void vk_create_depth_only_image_view( VkImage image, VkFormat format, VkImageViewType view_type,
	uint32_t base_array_layer, uint32_t layer_count, VkImageView *out_view, const char *name );

/* Lifecycled with attachments; definitions remain in vk.c */
void vk_create_volumetric_params_buffer( void );
void vk_destroy_volumetric_params_buffer( void );
void vk_create_postfx_params_buffers( void );
void vk_destroy_postfx_params_buffers( void );

#endif
