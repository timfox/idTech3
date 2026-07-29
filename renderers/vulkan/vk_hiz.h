#pragma once

/*
 * Raster Ultra 1.6 — Hi-Z depth pyramid for conservative occlusion.
 * Distinct from r_forwardPlusHiZ (tile probe padding only).
 */

#ifdef USE_VULKAN

void vk_hiz_register_cvars( void );
void vk_hiz_init( void );
void vk_hiz_shutdown( void );
void vk_hiz_on_resize( void );
void vk_hiz_on_camera_cut( void );

qboolean vk_hiz_active( void );
qboolean vk_hiz_ready( void );

typedef struct vkHizPyramidSampleInfo_s {
	VkImageView view;
	VkImageLayout layout;
	uint32_t width;
	uint32_t height;
	uint32_t levels;
	qboolean ready;
} vkHizPyramidSampleInfo_t;

qboolean vk_hiz_get_pyramid_sample_info( vkHizPyramidSampleInfo_t *out );

/* Build / refresh pyramid from current depth (after depth prepass when available). */
void vk_hiz_build( void );

/*
 * Conservative AABB occlusion test in view space / NDC.
 * Returns qtrue if possibly visible (must draw). Uses visible-last-frame bias.
 */
qboolean vk_hiz_aabb_visible( const vec3_t mins, const vec3_t maxs,
	qboolean wasVisibleLastFrame, uint32_t visibleAge );

void vk_hiz_status_f( void );

#endif /* USE_VULKAN */
