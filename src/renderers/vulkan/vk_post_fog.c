/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Post-fog color source selection and descriptor helpers for the Vulkan
FBO pipeline. Centralizes logic for luminance/gamma sampling source
when volumetrics are skipped or SMAA is applied.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_post_fog.h"

/*
===============
vk_post_fog_fbo_debug_throttle
===============
Throttle r_fboDebug 2/3 per-frame logs to at most once per second.
*/
qboolean vk_post_fog_fbo_debug_throttle( void )
{
	static unsigned int last_ms = 0;
	unsigned int now = ri.Milliseconds();
	if ( now - last_ms < 1000 )
		return qfalse;
	last_ms = now;
	return qtrue;
}

const char *vk_post_fog_source_name( VkImageView color_source )
{
	if ( color_source == vk.color_image_view ) {
		return "color_image";
	}
	if ( color_source == vk.smaa_output_image_view ) {
		return "smaa_output";
	}
	if ( color_source == vk.fog_scene_image_view ) {
		return "fog_scene";
	}
	if ( color_source == VK_NULL_HANDLE ) {
		return "null";
	}
	return "unknown_view";
}

VkImage vk_post_fog_source_image( VkImageView color_source )
{
	if ( color_source == vk.color_image_view ) {
		return vk.color_image;
	}
	if ( color_source == vk.smaa_output_image_view ) {
		return vk.smaa_output_image;
	}
	if ( color_source == vk.fog_scene_image_view ) {
		return vk.fog_scene_image;
	}
	return VK_NULL_HANDLE;
}

void vk_log_post_fog_rebind( const char *reason, VkImageView color_source )
{
	if ( r_fboDebug && r_fboDebug->integer >= 1 && vk_post_fog_fbo_debug_throttle() ) {
		ri.Printf( PRINT_DEVELOPER, "[VK][fbo] %s -> %s view=0x%llx\n",
			reason ? reason : "post-fog source rebind",
			vk_post_fog_source_name( color_source ),
			(unsigned long long)(uintptr_t)color_source );
	}
}

/*
 * Centralized post-fog source selection for luminance/gamma passes.
 */
VkImageView vk_get_post_fog_source( void )
{
	if ( vk.post_fog_color_source != VK_NULL_HANDLE ) {
		return vk.post_fog_color_source;
	}
	return vk.color_image_view;
}
