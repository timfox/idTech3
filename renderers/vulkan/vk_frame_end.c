#include "tr_local.h"
#include "vk.h"
#include "vk_frame_end.h"
#include "vk_aa_policy.h"
#include "vk_descriptor_sets.h"
#include "vk_image_layout.h"
#include "vk_post_aa.h"
#include "vk_post_fog.h"
#include "vk_post_process_push.h"
#include "vk_postfx.h"
#include "vk_postfx_params.h"
#include "vk_reactive_mask.h"
#include "vk_render_pass.h"
#include "vk_scene_pass.h"
#include "vk_temporal.h"
#include "vk_upscale.h"
#include "vk_view_state.h"
#include "vk_volumetric_pass.h"
#ifdef USE_IMGUI
#include "inspector/vk_imgui.h"
#endif

static void vk_end_frame_update_gamma_target( void );
static void vk_end_frame_fill_gamma_push_constants( VkPostProcessPushConstants *push, uint32_t srcTexW, uint32_t srcTexH );

static void vk_end_frame_get_color_source_extent( VkImageView color_source, uint32_t *width, uint32_t *height )
{
	uint32_t w = 0u;
	uint32_t h = 0u;

	if ( color_source == vk.screenMap.color_image_view ) {
		w = vk.screenMapWidth;
		h = vk.screenMapHeight;
	} else if ( color_source == vk.swapchain_image_views[0] ) {
		if ( vk.swapchain_extent_valid ) {
			w = vk.swapchain_extent.width;
			h = vk.swapchain_extent.height;
		}
	} else if ( color_source == vk.color_image_view ||
		color_source == vk.fog_scene_image_view ||
		color_source == vk.smaa_output_image_view ||
		color_source == vk.taa_history_image_view[0] ||
		color_source == vk.taa_history_image_view[1] ) {
		w = vk.mainColorWidth;
		h = vk.mainColorHeight;
	}

	if ( w == 0u || h == 0u ) {
		w = vk_get_render_target_width();
		h = vk_get_render_target_height();
	}
	if ( w == 0u ) {
		w = 1u;
	}
	if ( h == 0u ) {
		h = 1u;
	}

	if ( width ) {
		*width = w;
	}
	if ( height ) {
		*height = h;
	}
}

static const char *vk_end_frame_render_pass_name( renderPass_t pass )
{
	switch ( pass ) {
	case RENDER_PASS_MAIN:
		return "main";
	case RENDER_PASS_SCREENMAP:
		return "screenmap";
	case RENDER_PASS_SUN_SHADOW:
		return "sun_shadow";
	case RENDER_PASS_POST_BLOOM:
		return "post_bloom";
	case RENDER_PASS_UI_OVERLAY:
		return "ui_overlay";
	case RENDER_PASS_CUBEMAP:
		return "cubemap";
	default:
		return "unknown_pass";
	}
}

static void vk_end_frame_validate_post_process_chain( const char *stage, VkImageView post_fog_src, VkImageView luminance_src )
{
	if ( !r_fboDebug || r_fboDebug->integer < 1 || !vk_post_fog_fbo_debug_throttle() ) {
		return;
	}

	if ( vk.inRenderPass ) {
		ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
			"[VK][fbo] %s: expected no active render pass before frame-end post chain, still in %s\n",
			stage ? stage : "frame_end",
			vk_end_frame_render_pass_name( vk.renderPassIndex ) );
	}

	if ( vk.fboActive && vk.color_image_view == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
			"[VK][fbo] %s: FBO path active but color_image_view is null\n",
			stage ? stage : "frame_end" );
	}

	if ( post_fog_src == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
			"[VK][fbo] %s: post-fog source is null (expected %s)\n",
			stage ? stage : "frame_end",
			vk_post_fog_source_name( vk_get_post_fog_source() ) );
	}

	if ( luminance_src == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
			"[VK][fbo] %s: luminance source is null (scene source=%s)\n",
			stage ? stage : "frame_end",
			vk_post_fog_source_name( vk.scene_post_fog_color_source ) );
	}

	if ( vk.uiOverlayActive && vk.ui_overlay_image_view == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
			"[VK][fbo] %s: uiOverlayActive=1 but ui_overlay_image_view is null\n",
			stage ? stage : "frame_end" );
	}
	if ( vk.uiOverlayContentValid && vk.ui_overlay_image_view == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
			"[VK][fbo] %s: uiOverlayContentValid=1 but ui_overlay_image_view is null\n",
			stage ? stage : "frame_end" );
	}
	if ( ( vk.uiOverlayActive || vk.uiOverlayContentValid ) && vk.render_pass.overlay_compose == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
			"[VK][fbo] %s: UI overlay content present but overlay_compose render pass is null\n",
			stage ? stage : "frame_end" );
	}

	{
		VkImageView expected = vk_post_chain_expected_gamma_source();
		ri.Printf( PRINT_DEVELOPER,
			"[VK][fbo] %s: writer=%s doneBloom=%d postAaAfterBloom=%d smaa=%d fxaa=%d taaHistory=%s expected=%s actual=%s\n",
			stage ? stage : "frame_end",
			vk_post_chain_last_writer_name(),
			backEnd.doneBloom ? 1 : 0,
			r_postAaAfterBloom ? r_postAaAfterBloom->integer : 0,
			vk.smaaActive ? 1 : 0,
			vk.fxaaActive ? 1 : 0,
			vk.temporal.hasValidTAAHistory ? "yes" : "no",
			vk_post_fog_source_name( expected ),
			vk_post_fog_source_name( post_fog_src ) );
	}
}

static void vk_end_frame_refresh_postfx_params_for_target( uint32_t width, uint32_t height )
{
	vk.renderWidth = width > 0 ? width : 1u;
	vk.renderHeight = height > 0 ? height : 1u;
	vk.renderScaleX = 1.0f;
	vk.renderScaleY = 1.0f;
	vk_update_postfx_params( vk.cmd_index );
}

static void vk_end_frame_bind_post_process_sets( VkDescriptorSet set0, VkDescriptorSet set1, VkDescriptorSet set2, VkDescriptorSet set3 )
{
	VkDescriptorSet sets[4] = { set0, set1, set2, set3 };

	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.pipeline_layout_post_process, 0, 4, sets, 0, NULL );
}

static void vk_end_frame_bind_taa_sets( VkDescriptorSet set0, VkDescriptorSet set1, VkDescriptorSet set2,
	VkDescriptorSet set3, VkDescriptorSet set4, VkDescriptorSet set5 )
{
	VkDescriptorSet sets[6] = { set0, set1, set2, set3, set4, set5 };

	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.pipeline_layout_taa, 0, 6, sets, 0, NULL );
}

static qboolean vk_end_frame_gamma_chain_ready( void )
{
	if ( vk.cmd == NULL || vk.cmd->command_buffer == VK_NULL_HANDLE ||
		vk.cmd_index >= NUM_COMMAND_BUFFERS ||
		vk.swapchain_image_count == 0 ||
		vk.gamma_pipeline == VK_NULL_HANDLE || vk.post_color_descriptor[vk.cmd_index] == VK_NULL_HANDLE ||
		vk.render_pass.gamma == VK_NULL_HANDLE ||
		vk.cmd->swapchain_image_index >= vk.swapchain_image_count ||
		vk.cmd->swapchain_image_index >= MAX_SWAPCHAIN_IMAGES ||
		vk.framebuffers.gamma[ vk.cmd->swapchain_image_index ] == VK_NULL_HANDLE ||
		vk.renderWidth == 0 || vk.renderHeight == 0 ) {
		return qfalse;
	}

	return qtrue;
}

static VkImageView vk_end_frame_choose_viable_color_source( VkImageView preferred, const char *reason )
{
	VkImageView candidates[6];
	uint32_t candidateCount = 0;
	uint32_t i;

	if ( preferred != VK_NULL_HANDLE ) {
		candidates[candidateCount++] = preferred;
	}
	if ( vk_get_post_fog_source() != VK_NULL_HANDLE ) {
		candidates[candidateCount++] = vk_get_post_fog_source();
	}
	if ( vk.scene_post_fog_color_source != VK_NULL_HANDLE ) {
		candidates[candidateCount++] = vk.scene_post_fog_color_source;
	}
	if ( vk.post_fog_color_source != VK_NULL_HANDLE ) {
		candidates[candidateCount++] = vk.post_fog_color_source;
	}
	if ( vk.color_image_view != VK_NULL_HANDLE ) {
		candidates[candidateCount++] = vk.color_image_view;
	}
	if ( vk.temporal.hasValidTAAHistory ) {
		VkImageView taaView = vk.taa_history_image_view[ vk.temporal.taaHistoryIndex & 1u ];
		if ( taaView != VK_NULL_HANDLE ) {
			candidates[candidateCount++] = taaView;
		}
	}

	for ( i = 0; i < candidateCount; i++ ) {
		if ( vk_post_fog_source_image( candidates[i] ) != VK_NULL_HANDLE ) {
			if ( r_fboDebug && r_fboDebug->integer >= 1 && vk_post_fog_fbo_debug_throttle() ) {
				ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
					"[VK][fbo] viable color source (%s): %s\n",
					reason ? reason : "unspecified",
					vk_post_fog_source_name( candidates[i] ) );
			}
			return candidates[i];
		}
	}

	if ( r_fboDebug && r_fboDebug->integer >= 1 && vk_post_fog_fbo_debug_throttle() ) {
		ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
			"[VK][fbo] viable color source lookup failed (%s): preferred=%s post=%s scene=%s color=%s taaValid=%d\n",
			reason ? reason : "unspecified",
			vk_post_fog_source_name( preferred ),
			vk_post_fog_source_name( vk.post_fog_color_source ),
			vk_post_fog_source_name( vk.scene_post_fog_color_source ),
			vk_post_fog_source_name( vk.color_image_view ),
			vk.temporal.hasValidTAAHistory ? 1 : 0 );
	}

	return VK_NULL_HANDLE;
}

static qboolean vk_end_frame_record_emergency_present( VkImageView color_source )
{
	VkImage srcImage;
	VkImage dstImage;
	uint32_t srcWidth, srcHeight;
	uint32_t dstWidth, dstHeight;

	if ( !vk.fboActive || vk.cmd == NULL || vk.cmd->command_buffer == VK_NULL_HANDLE ||
		vk.swapchain_image_count == 0 || vk.cmd->swapchain_image_index >= vk.swapchain_image_count ||
		vk.cmd->swapchain_image_index >= MAX_SWAPCHAIN_IMAGES ) {
		return qfalse;
	}

	color_source = vk_end_frame_choose_viable_color_source( color_source, "emergency_present" );
	srcImage = vk_post_fog_source_image( color_source );
	dstImage = vk.swapchain_images[ vk.cmd->swapchain_image_index ];
	if ( srcImage == VK_NULL_HANDLE || dstImage == VK_NULL_HANDLE ) {
		return qfalse;
	}

	vk_end_frame_get_color_source_extent( color_source, &srcWidth, &srcHeight );
	dstWidth = vk.renderWidth > 0 ? vk.renderWidth :
		( vk.swapchain_extent_valid ? vk.swapchain_extent.width : ( gls.windowWidth > 0 ? (uint32_t)gls.windowWidth : 0u ) );
	dstHeight = vk.renderHeight > 0 ? vk.renderHeight :
		( vk.swapchain_extent_valid ? vk.swapchain_extent.height : ( gls.windowHeight > 0 ? (uint32_t)gls.windowHeight : 0u ) );

	if ( srcWidth == 0 || srcHeight == 0 || dstWidth == 0 || dstHeight == 0 ) {
		return qfalse;
	}

	record_image_layout_transition( vk.cmd->command_buffer, srcImage, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 0 );
	record_image_layout_transition( vk.cmd->command_buffer, dstImage, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );

	if ( vk.blitEnabled ) {
		VkImageBlit region;

		Com_Memset( &region, 0, sizeof( region ) );
		region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.srcSubresource.layerCount = 1;
		region.srcOffsets[1].x = (int32_t)srcWidth;
		region.srcOffsets[1].y = (int32_t)srcHeight;
		region.srcOffsets[1].z = 1;
		region.dstSubresource = region.srcSubresource;
		region.dstOffsets[1].x = (int32_t)dstWidth;
		region.dstOffsets[1].y = (int32_t)dstHeight;
		region.dstOffsets[1].z = 1;

		qvkCmdBlitImage( vk.cmd->command_buffer,
			srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &region, VK_FILTER_LINEAR );
	} else {
		VkImageCopy region;

		if ( srcWidth != dstWidth || srcHeight != dstHeight ) {
			record_image_layout_transition( vk.cmd->command_buffer, srcImage, VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
			record_image_layout_transition( vk.cmd->command_buffer, dstImage, VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 0, 0 );
			return qfalse;
		}

		Com_Memset( &region, 0, sizeof( region ) );
		region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.srcSubresource.layerCount = 1;
		region.dstSubresource = region.srcSubresource;
		region.extent.width = srcWidth;
		region.extent.height = srcHeight;
		region.extent.depth = 1;

		qvkCmdCopyImage( vk.cmd->command_buffer,
			srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &region );
	}

	record_image_layout_transition( vk.cmd->command_buffer, srcImage, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
	record_image_layout_transition( vk.cmd->command_buffer, dstImage, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 0, 0 );

	if ( r_fboDebug && r_fboDebug->integer >= 1 && vk_post_fog_fbo_debug_throttle() ) {
		ri.Printf( PRINT_WARNING,
			"[VK][fbo] emergency present fallback copied %s to swapchain image %u (%ux%u -> %ux%u, mode=%s)\n",
			vk_post_fog_source_name( color_source ),
			(unsigned int)vk.cmd->swapchain_image_index,
			(unsigned int)srcWidth, (unsigned int)srcHeight,
			(unsigned int)dstWidth, (unsigned int)dstHeight,
			vk.blitEnabled ? "blit" : "copy" );
	}

	return qtrue;
}

static qboolean vk_end_frame_try_repair_gamma_chain( VkImageView *gamma_src )
{
	if ( !vk.fboActive || vk.cmd == NULL || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		return qfalse;
	}

	vk_update_attachment_descriptors();
	vk_update_post_process_pipelines();
	vk_end_frame_update_gamma_target();
	vk_end_frame_refresh_postfx_params_for_target( vk.renderWidth, vk.renderHeight );

	if ( gamma_src ) {
		*gamma_src = vk_end_frame_choose_viable_color_source(
			*gamma_src != VK_NULL_HANDLE ? *gamma_src : vk_get_post_fog_source(),
			"repair_gamma_chain" );
	}

	if ( gamma_src && *gamma_src != VK_NULL_HANDLE ) {
		vk_update_post_fog_descriptors( *gamma_src );
		vk_update_color_descriptor_image( *gamma_src );
	}

	if ( r_fboDebug && r_fboDebug->integer >= 1 && vk_post_fog_fbo_debug_throttle() ) {
		const char *reason = "ready";

		if ( vk.cmd == NULL ) {
			reason = "no-cmd";
		} else if ( vk.cmd->command_buffer == VK_NULL_HANDLE ) {
			reason = "no-command-buffer";
		} else if ( vk.cmd_index >= NUM_COMMAND_BUFFERS ) {
			reason = "cmd-index-range";
		} else if ( vk.swapchain_image_count == 0 ) {
			reason = "no-swapchain-images";
		} else if ( vk.cmd->swapchain_image_index >= vk.swapchain_image_count ) {
			reason = "swapchain-index-range";
		} else if ( vk.cmd->swapchain_image_index >= MAX_SWAPCHAIN_IMAGES ) {
			reason = "framebuffer-index-range";
		} else if ( vk.gamma_pipeline == VK_NULL_HANDLE ) {
			reason = "no-gamma-pipeline";
		} else if ( vk.post_color_descriptor[vk.cmd_index] == VK_NULL_HANDLE ) {
			reason = "no-post-color-descriptor";
		} else if ( vk.render_pass.gamma == VK_NULL_HANDLE ) {
			reason = "no-gamma-renderpass";
		} else if ( vk.framebuffers.gamma[ vk.cmd->swapchain_image_index ] == VK_NULL_HANDLE ) {
			reason = "no-gamma-framebuffer";
		} else if ( vk.renderWidth == 0 || vk.renderHeight == 0 ) {
			reason = "zero-render-size";
		}

		ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
			"[VK][fbo] gamma chain self-heal attempted: source=%s ready=%s reason=%s\n",
			gamma_src ? vk_post_fog_source_name( *gamma_src ) : "null",
			vk_end_frame_gamma_chain_ready() ? "yes" : "no",
			reason );
	}

	return vk_end_frame_gamma_chain_ready();
}

static void vk_end_frame_draw_fullscreen_quad( uint32_t width, uint32_t height )
{
	vk_set_fullscreen_viewport_scissor( width, height );
	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
}

static void vk_end_frame_begin_post_process_pass( VkRenderPass renderPass, VkFramebuffer framebuffer,
	uint32_t width, uint32_t height, VkPipeline pipeline )
{
	vk_begin_render_pass_tracked( renderPass, framebuffer, qfalse, width, height );
	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );
}

void vk_end_frame_record_capture_if_needed( void )
{
	VkImage srcImage;
	uint32_t cap_w;
	uint32_t cap_h;
	uint32_t src_w;
	uint32_t src_h;
	VkImageBlit region;

	if ( !backEnd.screenshotMask || !vk.capture.image ) {
		return;
	}
	if ( ri.CL_IsMinimized() || vk.swapchain_image_count == 0 ) {
		return;
	}
	if ( vk.cmd->swapchain_image_index >= vk.swapchain_image_count ) {
		return;
	}

	vk_end_render_pass();

	srcImage = vk.swapchain_images[ vk.cmd->swapchain_image_index ];
	if ( srcImage == VK_NULL_HANDLE ) {
		return;
	}

	cap_w = ( gls.captureWidth > 0 ) ? (uint32_t)gls.captureWidth : 1u;
	cap_h = ( gls.captureHeight > 0 ) ? (uint32_t)gls.captureHeight : 1u;
	src_w = ( vk.renderWidth > 0 ) ? vk.renderWidth : cap_w;
	src_h = ( vk.renderHeight > 0 ) ? vk.renderHeight : cap_h;

	/* Gamma (+ overlay compose) left the swapchain in PRESENT_SRC. Blit that
	   LDR result into the capture buffer so screenshots match what was shown. */
	record_image_layout_transition( vk.cmd->command_buffer, srcImage,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 0 );
	record_image_layout_transition( vk.cmd->command_buffer, vk.capture.image,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );

	Com_Memset( &region, 0, sizeof( region ) );
	region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.srcSubresource.layerCount = 1;
	region.dstSubresource = region.srcSubresource;
	region.srcOffsets[1].x = (int32_t)src_w;
	region.srcOffsets[1].y = (int32_t)src_h;
	region.srcOffsets[1].z = 1;
	region.dstOffsets[1].x = (int32_t)cap_w;
	region.dstOffsets[1].y = (int32_t)cap_h;
	region.dstOffsets[1].z = 1;

	qvkCmdBlitImage( vk.cmd->command_buffer,
		srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		vk.capture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &region, VK_FILTER_LINEAR );

	record_image_layout_transition( vk.cmd->command_buffer, vk.capture.image,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 0 );
	record_image_layout_transition( vk.cmd->command_buffer, srcImage,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 0, 0 );
}

void vk_end_frame_prepare_post_process( VkImageView *post_fog_src, VkImageView *luminance_src )
{
	if ( post_fog_src ) {
		*post_fog_src = VK_NULL_HANDLE;
	}
	if ( luminance_src ) {
		*luminance_src = VK_NULL_HANDLE;
	}

	/*
	 * Close any open scene/UI draw pass before post. Keep uiOverlayContentValid so
	 * gamma can still alpha-composite the LDR HUD/menu overlay after tonemap.
	 * uiOverlayActive only means "currently recording into ui_overlay".
	 */
	if ( vk.inRenderPass &&
		( vk.renderPassIndex == RENDER_PASS_UI_OVERLAY || vk.uiOverlayActive ) ) {
		vk_end_render_pass();
		vk.uiOverlayActive = qfalse;
		vk_pass_diag_stage( "prepare_post_leave_ui_overlay" );
	} else {
		vk_end_render_pass();
	}

	if ( !backEnd.doneFog ) {
		vk_volumetric_fog_pass();
	} else {
		vk_log_post_fog_rebind( "end_frame volumetric skipped (scene+2D on color_image)", vk.color_image_view );
		vk_update_post_fog_descriptors( vk.color_image_view );
	}

	if ( post_fog_src ) {
		*post_fog_src = vk_end_frame_choose_viable_color_source( vk_get_post_fog_source(), "prepare_post_process" );
	}
	if ( luminance_src ) {
		*luminance_src = vk_end_frame_choose_viable_color_source( vk_get_luminance_source(), "prepare_post_process_luminance" );
	}

	if ( post_fog_src && *post_fog_src != VK_NULL_HANDLE ) {
		vk_update_post_fog_descriptors( *post_fog_src );
		vk_barrier_post_fog_source_for_sampling( *post_fog_src, "vk_end_frame pre-luminance/gamma" );
	}

	vk_end_frame_validate_post_process_chain( "prepare_post_process",
		post_fog_src ? *post_fog_src : VK_NULL_HANDLE,
		luminance_src ? *luminance_src : VK_NULL_HANDLE );
}

void vk_end_frame_record_taa_pass( VkImageView *post_fog_src, VkImageView *luminance_src )
{
	VkImageView taa_src;
	VkImageView resolved_view;
	uint32_t taaWidth;
	uint32_t taaHeight;
	uint32_t readIndex;
	uint32_t writeIndex;
	qboolean allow_taa;
	qboolean taa_wanted;

	if ( post_fog_src == NULL || luminance_src == NULL ) {
		return;
	}

	taa_src = ( *post_fog_src != VK_NULL_HANDLE ) ? *post_fog_src : vk.color_image_view;
	/* Prefer doneWorldScene over the last refdef (HUD/weapon may set RDF_NOWORLDMODEL). */
	allow_taa = ( tr.world != NULL ) &&
		backEnd.doneWorldScene &&
		( backEnd.viewParms.portalView == PV_NONE ) &&
		( vk.temporal.firstPersonProjectionThisFrame == vk.temporal.firstPersonProjectionLastFrame ) &&
		!vk_temporal_has_reason( VK_TEMPORAL_RESET_CAMERA_CUT | VK_TEMPORAL_RESET_MISSING_PREV_DATA |
			VK_TEMPORAL_RESET_RENDERER_INIT | VK_TEMPORAL_RESET_SWAPCHAIN_CHANGE |
			VK_TEMPORAL_RESET_RENDER_SIZE_CHANGE | VK_TEMPORAL_RESET_WORLD_CHANGE |
			VK_TEMPORAL_RESET_CLIENT_STATE_CHANGE ) &&
		/* Death cam / AFK: high stationary TAA feedback smears sky/fog into streaks. */
		!vk_temporal_near_static_streak_guard();
	taa_wanted = ( r_taa && r_taa->integer ) ? qtrue : qfalse;
	/*
	 * Hybrid1 owns separate SVGF / channel histories — do not auto-force world
	 * taa_history as an RT denoiser. Optional presentation AA stays under r_aaMode / r_taa.
	 */
	if ( !taa_wanted && R_Upscale_WantTemporal() ) {
		taa_wanted = qtrue;
	}

	if ( !allow_taa ||
		!taa_wanted ||
		vk.cmd == NULL || vk.cmd->command_buffer == VK_NULL_HANDLE ||
		taa_src == VK_NULL_HANDLE ||
		vk.taa_pipeline == VK_NULL_HANDLE ||
		vk.pipeline_layout_taa == VK_NULL_HANDLE ||
		vk.render_pass.taa == VK_NULL_HANDLE ||
		vk.post_color_descriptor[vk.cmd_index] == VK_NULL_HANDLE ||
		vk.depth_descriptor[vk.cmd_index] == VK_NULL_HANDLE ||
		vk.postfx_params_descriptor[vk.cmd_index] == VK_NULL_HANDLE ||
		vk.framebuffers.taa[0] == VK_NULL_HANDLE ||
		vk.framebuffers.taa[1] == VK_NULL_HANDLE ||
		vk.taa_history_descriptor[0] == VK_NULL_HANDLE ||
		vk.taa_history_descriptor[1] == VK_NULL_HANDLE ) {
		if ( !allow_taa || !taa_wanted ) {
			vk_reset_taa_history();
		}
		/* Mode 5: if temporal skipped on a world frame, still apply deferred SMAA cleanup.
		 * Menus/cinematics keep color_image sharp (no world temporal path). */
		if ( allow_taa &&
			vk_aa_policy_wants_temporal_cleanup_smaa() && vk.smaaActive && taa_src != VK_NULL_HANDLE ) {
			if ( vk_post_scene_aa_apply_from( taa_src ) ) {
				*post_fog_src = vk_get_post_fog_source();
				*luminance_src = *post_fog_src;
			}
		}
		/* Even if TAA was skipped, flush any deferred weapon onto the HDR source. */
		RB_FlushDeferredWeaponAfterTaa( post_fog_src, luminance_src );
		return;
	}

	readIndex = vk.temporal.taaHistoryIndex & 1u;
	writeIndex = 1u - readIndex;

	vk_barrier_post_fog_source_for_sampling( taa_src, "vk_end_frame pre-taa (current)" );
	if ( r_taaMotionVectors && r_taaMotionVectors->integer && vk.motion_vector_image != VK_NULL_HANDLE ) {
		vk_barrier_motion_vector_for_sampling( "vk_end_frame pre-taa (motion)" );
	}
	if ( vk_reactive_mask_active() ) {
		vk_barrier_reactive_mask_for_sampling( "vk_end_frame pre-taa (reactive)" );
	}
	if ( vk.temporal.hasValidTAAHistory ) {
		vk_barrier_post_fog_source_for_sampling( vk.taa_history_image_view[readIndex], "vk_end_frame pre-taa (history)" );
	}
	vk_update_color_descriptor_image( taa_src );
	vk_get_active_render_extent( &taaWidth, &taaHeight );
	vk_end_frame_refresh_postfx_params_for_target( taaWidth, taaHeight );

	vk_end_frame_begin_post_process_pass( vk.render_pass.taa, vk.framebuffers.taa[writeIndex],
		taaWidth, taaHeight, vk.taa_pipeline );
	{
		VkDescriptorSet reactive_set = vk.taa_reactive_descriptor[vk.cmd_index];
		VkDescriptorSet depth_set = vk.depth_descriptor[vk.cmd_index];
		if ( reactive_set == VK_NULL_HANDLE ) {
			reactive_set = vk.taa_motion_descriptor[vk.cmd_index]; /* should not happen */
		}
		/* MSAA: sample resolved R32F depth (same binding as OIT), not the MSAA attachment. */
		if ( vk.msaaActive && vk.volumetric_depth_view != VK_NULL_HANDLE &&
			vk.oit_depth_descriptor != VK_NULL_HANDLE ) {
			depth_set = vk.oit_depth_descriptor;
		}
		vk_end_frame_bind_taa_sets(
			vk.post_color_descriptor[vk.cmd_index],
			depth_set,
			vk.postfx_params_descriptor[vk.cmd_index],
			vk.taa_history_descriptor[readIndex],
			vk.taa_motion_descriptor[vk.cmd_index],
			reactive_set );
	}
	vk_end_frame_draw_fullscreen_quad( taaWidth, taaHeight );
	vk_end_render_pass();

	resolved_view = vk.taa_history_image_view[writeIndex];
	vk.temporal.taaHistoryIndex = writeIndex;
	vk.temporal.hasValidTAAHistory = qtrue;
	*post_fog_src = resolved_view;
	*luminance_src = resolved_view;
	vk_set_scene_post_fog_source( resolved_view );
	vk_update_post_fog_descriptors( resolved_view );
	vk_set_post_chain_last_writer( "taa" );

	/* Mode 5 / r_temporalSmaaCleanup: light SMAA after Temporal Reconstruction. */
	if ( vk_aa_policy_wants_temporal_cleanup_smaa() && vk.smaaActive ) {
		if ( vk_post_scene_aa_apply_from( resolved_view ) ) {
			*post_fog_src = vk_get_post_fog_source();
			*luminance_src = *post_fog_src;
		}
	}

	/* Weapon/view-model after world TAA — never contaminate history. */
	RB_FlushDeferredWeaponAfterTaa( post_fog_src, luminance_src );
}

void vk_end_frame_record_luminance_pass( VkImageView luminance_src )
{
	cvar_t *exp_auto = ri.Cvar_Get( "r_exposure_auto", "0", 0 );
	cvar_t *fbo_cinematic = ri.Cvar_Get( "r_fboCinematic", "1", 0 );
	int clientState = ri.CL_GetState ? ri.CL_GetState() : CA_ACTIVE;
	qboolean allow_cinematic_luminance = ( clientState == CA_ACTIVE && tr.world != NULL ) ||
		( clientState == CA_CINEMATIC && fbo_cinematic && fbo_cinematic->integer );

	if ( vk.cmd == NULL || vk.cmd->command_buffer == VK_NULL_HANDLE ||
		exp_auto == NULL || !exp_auto->integer ||
		r_hdr == NULL || !r_hdr->integer ||
		!allow_cinematic_luminance ||
		vk.luminance_pipeline == VK_NULL_HANDLE ||
		vk.luminance_descriptor[vk.cmd_index] == VK_NULL_HANDLE ||
		vk.luminance_image_view == VK_NULL_HANDLE ||
		vk.luminance_staging_buffer == VK_NULL_HANDLE ||
		luminance_src == VK_NULL_HANDLE ||
		luminance_src == vk.luminance_image_view ) {
		return;
	}

	vk_update_luminance_descriptor_image( luminance_src );
	vk_barrier_post_fog_source_for_sampling( luminance_src, "vk_end_frame pre-luminance (scene only)" );
	record_image_layout_transition( vk.cmd->command_buffer, vk.luminance_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.luminance_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
		vk.luminance_pipeline_layout, 0, 1, &vk.luminance_descriptor[vk.cmd_index], 0, NULL );
	{
		VkLuminancePushConstants meter;
		cvar_t *lowPercent = ri.Cvar_Get( "r_autoExposure_lowPercent", "0.02", 0 );
		cvar_t *highPercent = ri.Cvar_Get( "r_autoExposure_highPercent", "0.01", 0 );
		cvar_t *centerWeight = ri.Cvar_Get( "r_autoExposure_centerWeight", "0.60", 0 );
		meter.lowPercent = Com_Clamp( 0.0f, 0.45f, lowPercent ? lowPercent->value : 0.02f );
		meter.highPercent = Com_Clamp( 0.0f, 0.45f, highPercent ? highPercent->value : 0.01f );
		meter.centerWeight = Com_Clamp( 0.0f, 1.5f, centerWeight ? centerWeight->value : 0.60f );
		meter.reserved = 0.0f;
		qvkCmdPushConstants( vk.cmd->command_buffer, vk.luminance_pipeline_layout,
			VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( meter ), &meter );
	}
	qvkCmdDispatch( vk.cmd->command_buffer, 1, 1, 1 );

	record_image_layout_transition( vk.cmd->command_buffer, vk.luminance_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT );

	{
		VkBufferImageCopy region;
		Com_Memset( &region, 0, sizeof( region ) );
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset.x = 0;
		region.imageOffset.y = 0;
		region.imageOffset.z = 0;
		region.imageExtent.width = 1;
		region.imageExtent.height = 1;
		region.imageExtent.depth = 1;
		qvkCmdCopyImageToBuffer( vk.cmd->command_buffer, vk.luminance_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			vk.luminance_staging_buffer, 1, &region );
	}

	record_image_layout_transition( vk.cmd->command_buffer, vk.luminance_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
}

static void vk_end_frame_update_gamma_target( void )
{
	if ( vk.swapchain_extent_valid && vk.swapchain_extent.width > 0 && vk.swapchain_extent.height > 0 ) {
		vk.renderWidth = vk.swapchain_extent.width;
		vk.renderHeight = vk.swapchain_extent.height;
	} else {
		vk.renderWidth = ( gls.windowWidth > 0 ) ? gls.windowWidth : glConfig.vidWidth;
		vk.renderHeight = ( gls.windowHeight > 0 ) ? gls.windowHeight : glConfig.vidHeight;
	}

	vk.renderScaleX = 1.0f;
	vk.renderScaleY = 1.0f;
}

static void vk_end_frame_fill_gamma_push_constants( VkPostProcessPushConstants *push, uint32_t srcTexW, uint32_t srcTexH )
{
	VkRect2D srcRect;

	Com_Memset( push, 0, sizeof( *push ) );

	{
		int lensPreset = r_paniniLensPreset ? r_paniniLensPreset->integer : 0;
		float presetAmount = r_panini ? r_panini->value : 0.0f;
		float presetD = r_panini_d ? r_panini_d->value : 1.0f;
		float presetS = r_panini_s ? r_panini_s->value : 0.25f;
		float presetFov = r_panini_theta ? r_panini_theta->value : 90.0f;
		float presetZoom = r_panini_zoom ? r_panini_zoom->value : 1.0f;
		float presetBright = r_paniniBrightness ? r_paniniBrightness->value : 1.0f;

		switch ( lensPreset ) {
			case 1: presetAmount = 1.0f; presetD = 1.0f; presetS = 0.2f; presetFov = 120.0f; presetZoom = 1.15f; presetBright = 1.2f; break;
			case 2: presetAmount = 1.0f; presetD = 1.2f; presetS = 0.35f; presetFov = 150.0f; presetZoom = 1.25f; presetBright = 1.25f; break;
			case 3: presetAmount = 0.0f; presetD = 0.0f; presetS = 0.0f; presetFov = 90.0f; presetZoom = 1.0f; break;
			case 4: presetAmount = 0.4f; presetD = 0.3f; presetS = 0.05f; presetFov = 84.0f; presetZoom = 1.0f; break;
			case 5: presetAmount = 0.2f; presetD = 0.15f; presetS = 0.02f; presetFov = 63.0f; presetZoom = 1.0f; break;
			case 6: presetAmount = 1.0f; presetD = 1.5f; presetS = 0.5f; presetFov = 170.0f; presetZoom = 1.4f; presetBright = 1.3f; break;
			case 7: presetAmount = 0.8f; presetD = 0.8f; presetS = 0.15f; presetFov = 110.0f; presetZoom = 1.1f; break;
			default: break;
		}

		push->paniniAmount = presetAmount;
		push->paniniD = presetD;
		push->paniniS = presetS;
		push->fovXDeg = backEnd.viewParms.fovX > 1.0f ? backEnd.viewParms.fovX : presetFov;
		push->paniniZoom = presetZoom;
		push->brightness = presetBright;
	}

	push->aspect = vk.renderHeight > 0 ? ( (float)vk.renderWidth / (float)vk.renderHeight ) : 1.0f;
	push->paniniBorderMode = r_panini_border ? (float)r_panini_border->integer : 0.0f;
	push->paniniDebugMode = r_panini_debug ? (float)r_panini_debug->integer : 0.0f;
	push->paniniPad0 = (float)backEnd.refdef.time * 0.001f;
	push->paniniPad1 = 0.0f;
	/* Default: apply Panini. Skip when console/UI just drew into the color buffer
	   unless r_panini_console is enabled (1 = warp console too). */
	push->paniniMask = 1.0f;
	if ( backEnd.drawConsole && ( !r_panini_console || !r_panini_console->integer ) ) {
		push->paniniMask = 0.0f;
	}

	{
		cvar_t *r_exposure_auto_var = ri.Cvar_Get( "r_exposure_auto", "0", 0 );
		float expVal = ( r_exposure && r_exposure->value > 0.0f ) ? r_exposure->value : 1.0f;
		if ( r_exposure_auto_var && r_exposure_auto_var->integer ) {
			expVal = vk.adaptedExposure > 0.0f ? vk.adaptedExposure : expVal;
		}
		/* Use doneWorldScene — tr.refdef may be a later HUD/weapon NOWORLDMODEL scene. */
		if ( ( !tr.world || !backEnd.doneWorldScene ) && expVal < 1.0f ) {
			expVal = 1.0f;
		}
		push->exposure = expVal;
	}

	if ( !tr.world || !backEnd.doneWorldScene ) {
		push->brightness = 1.0f;
		push->exposure = 1.0f;
		push->paniniPad1 = 1.0f;
		/* Menus / no-world UI should stay undistorted. */
		if ( !r_panini_console || !r_panini_console->integer ) {
			push->paniniMask = 0.0f;
		}
		if ( r_fboDebug && r_fboDebug->integer >= 1 ) {
			static int s_gammaNoWorldLog;
			if ( s_gammaNoWorldLog++ < 3 ) {
				ri.Printf( PRINT_ALL, "Vulkan: gamma no-world path (doneWorldScene=%d world=%p)\n",
					(int)backEnd.doneWorldScene, (void *)tr.world );
			}
		}
	} else if ( r_fboDebug && r_fboDebug->integer >= 1 ) {
		static int s_gammaWorldLog;
		if ( s_gammaWorldLog++ < 3 ) {
			ri.Printf( PRINT_ALL, "Vulkan: gamma world path (doneWorldScene=1 exposure=%.3f)\n",
				push->exposure );
		}
	}

	if ( !vk_get_scene_src_rect( &srcRect ) ) {
		srcRect.offset.x = 0;
		srcRect.offset.y = 0;
		srcRect.extent.width = srcTexW;
		srcRect.extent.height = srcTexH;
	}

	if ( (int32_t)srcRect.offset.x < 0 ) srcRect.offset.x = 0;
	if ( (int32_t)srcRect.offset.y < 0 ) srcRect.offset.y = 0;
	{
		const uint32_t offX = (uint32_t)srcRect.offset.x;
		const uint32_t offY = (uint32_t)srcRect.offset.y;
		if ( offX + srcRect.extent.width > srcTexW ) {
			srcRect.extent.width = ( srcTexW > offX ) ? ( srcTexW - offX ) : 0u;
		}
		if ( offY + srcRect.extent.height > srcTexH ) {
			srcRect.extent.height = ( srcTexH > offY ) ? ( srcTexH - offY ) : 0u;
		}
	}
	if ( srcRect.extent.width == 0 || srcRect.extent.height == 0 ) {
		srcRect.offset.x = 0;
		srcRect.offset.y = 0;
		srcRect.extent.width = srcTexW > 0 ? srcTexW : 1u;
		srcRect.extent.height = srcTexH > 0 ? srcTexH : 1u;
	}

	push->srcUVScaleBias[0] = (float)srcRect.extent.width / (float)srcTexW;
	push->srcUVScaleBias[1] = (float)srcRect.extent.height / (float)srcTexH;
	push->srcUVScaleBias[2] = (float)srcRect.offset.x / (float)srcTexW;
	push->srcUVScaleBias[3] = (float)srcRect.offset.y / (float)srcTexH;
}

void vk_end_frame_record_gamma_pass( VkImageView post_fog_src )
{
	VkImageView expected_src = vk_post_chain_expected_gamma_source();
	VkImageView preferred = ( post_fog_src != VK_NULL_HANDLE ) ? post_fog_src : vk.color_image_view;
	VkImageView gamma_src;

	/* Prefer expected writer when viable so bloom/AA/TAA intent wins over stale handles. */
	if ( expected_src != VK_NULL_HANDLE && vk_post_fog_source_image( expected_src ) != VK_NULL_HANDLE ) {
		preferred = expected_src;
	}
	gamma_src = vk_end_frame_choose_viable_color_source( preferred, "gamma_pass_start" );

	/*
	 * Consume exposure as part of the frame-end post chain rather than the
	 * scene/2D transition path. This keeps adaptation tied to gamma, not to
	 * whichever subsystem first prepared temporal state for the frame.
	 */
	vk_temporal_update_auto_exposure();
	vk_end_frame_update_gamma_target();
	vk_end_frame_refresh_postfx_params_for_target( vk.renderWidth, vk.renderHeight );

	if ( r_fboDebug && r_fboDebug->integer >= 2 && vk_post_fog_fbo_debug_throttle() ) {
		const float sx = ( glConfig.vidWidth > 0 ) ? (float)vk.renderWidth / (float)glConfig.vidWidth : 1.0f;
		const float sy = ( glConfig.vidHeight > 0 ) ? (float)vk.renderHeight / (float)glConfig.vidHeight : 1.0f;
		ri.Printf( PRINT_DEVELOPER,
			"[VK][fbo] gamma target=%dx%d source=%dx%d window=%dx%d src=%s scale=%.3fx%.3f\n",
			vk.renderWidth, vk.renderHeight,
			glConfig.vidWidth, glConfig.vidHeight,
			gls.windowWidth, gls.windowHeight,
			vk_post_fog_source_name( post_fog_src ),
			sx, sy );
	}
	if ( r_fboDebug && r_fboDebug->integer >= 3 && vk_post_fog_fbo_debug_throttle() ) {
		const unsigned int gamma_index = ( vk.cmd != NULL ) ? (unsigned int)vk.cmd->swapchain_image_index : 0u;
		const unsigned long long gamma_fb = ( vk.cmd != NULL && vk.cmd->swapchain_image_index < MAX_SWAPCHAIN_IMAGES ) ?
			(unsigned long long)(uintptr_t)vk.framebuffers.gamma[ vk.cmd->swapchain_image_index ] : 0ULL;

		ri.Printf( PRINT_DEVELOPER,
			"[VK][fbo] gamma pipeline=0x%llx color_desc=0x%llx layout=0x%llx rp=0x%llx fb=0x%llx idx=%u count=%u\n",
			(unsigned long long)(uintptr_t)vk.gamma_pipeline,
			(unsigned long long)(uintptr_t)vk.post_color_descriptor[vk.cmd_index],
			(unsigned long long)(uintptr_t)vk.pipeline_layout_post_process,
			(unsigned long long)(uintptr_t)vk.render_pass.gamma,
			gamma_fb,
			gamma_index,
			(unsigned int)vk.swapchain_image_count );
	}

	if ( r_fboDebug && r_fboDebug->integer >= 1 && vk_post_fog_fbo_debug_throttle() ) {
		if ( gamma_src != expected_src || gamma_src != vk.post_fog_color_source ) {
			ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
				"[VK][fbo] gamma source mismatch: gamma_src=%s post_fog_color_source=%s expected=%s writer=%s\n",
				vk_post_fog_source_name( gamma_src ),
				vk_post_fog_source_name( vk.post_fog_color_source ),
				vk_post_fog_source_name( expected_src ),
				vk_post_chain_last_writer_name() );
		}
	}

	vk_end_frame_validate_post_process_chain( "gamma_pass", gamma_src, vk_get_luminance_source() );

	if ( !vk_end_frame_gamma_chain_ready() ) {
		if ( !vk_end_frame_try_repair_gamma_chain( &gamma_src ) ) {
			gamma_src = vk_end_frame_choose_viable_color_source( gamma_src, "gamma_pass_repair_failed" );
			if ( vk_end_frame_record_emergency_present( gamma_src ) ) {
				return;
			}
			if ( vk_post_fog_fbo_debug_throttle() ) {
				ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW "[VK][fbo] gamma pass skipped: missing pipeline/descriptor/renderpass/framebuffer or zero size (%dx%d), and emergency present fallback was unavailable\n",
					vk.renderWidth, vk.renderHeight );
			}
			return;
		}
	}

	if ( gamma_src == VK_NULL_HANDLE ) {
		gamma_src = vk_end_frame_choose_viable_color_source( vk_get_post_fog_source(), "gamma_pass_null_source" );
	}

	if ( gamma_src == VK_NULL_HANDLE ) {
		if ( vk_end_frame_record_emergency_present( vk_end_frame_choose_viable_color_source( vk.color_image_view, "gamma_pass_last_resort" ) ) ) {
			return;
		}
		if ( vk_post_fog_fbo_debug_throttle() ) {
			ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW "[VK][fbo] gamma pass skipped: no valid color source (post_fog or color_image), and emergency present fallback was unavailable\n" );
		}
		return;
	}

	vk_barrier_post_fog_source_for_sampling( gamma_src, "vk_end_frame pre-gamma (gamma_src)" );
	vk_update_color_descriptor_image( gamma_src );

	if ( vk.depth_image != VK_NULL_HANDLE ) {
		VkImageAspectFlags da = VK_IMAGE_ASPECT_DEPTH_BIT;
		if ( glConfig.stencilBits > 0 ) {
			da |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
		record_depth_image_layout_transition( vk.cmd->command_buffer, da,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, 0, 0 );
	}

	vk_end_frame_begin_post_process_pass( vk.render_pass.gamma,
		vk.framebuffers.gamma[ vk.cmd->swapchain_image_index ],
		vk.renderWidth, vk.renderHeight, vk.gamma_pipeline );
	vk_end_frame_bind_post_process_sets(
		vk.post_color_descriptor[vk.cmd_index],
		vk.depth_descriptor[vk.cmd_index],
		vk.postfx_params_descriptor[vk.cmd_index],
		PostFX_GetLUTImage()->descriptor );

	{
		VkPostProcessPushConstants push;
		uint32_t srcTexW;
		uint32_t srcTexH;

		vk_end_frame_get_color_source_extent( gamma_src, &srcTexW, &srcTexH );
		vk_end_frame_fill_gamma_push_constants( &push, srcTexW, srcTexH );
		qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout_post_process, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( push ), &push );
	}

	vk_end_frame_draw_fullscreen_quad( vk.renderWidth, vk.renderHeight );
	vk_end_render_pass();

	if ( vk.depth_image != VK_NULL_HANDLE ) {
		VkImageAspectFlags da = VK_IMAGE_ASPECT_DEPTH_BIT;
		if ( glConfig.stencilBits > 0 ) {
			da |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
		record_depth_image_layout_transition( vk.cmd->command_buffer, da,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0, 0 );
	}

#ifdef USE_IMGUI
	VkImgui_RecordOverlayPass();
#endif

	/*
	 * Compose LDR UI/HUD after tonemap. Prefer uiOverlayContentValid over
	 * uiOverlayActive: FinishBloom / post_bloom / prepare_post clear the active
	 * recording flag but the overlay image still holds this frame's StretchPics.
	 */
	if ( vk.uiOverlayContentValid &&
		vk.ui_overlay_image_view != VK_NULL_HANDLE &&
		vk.overlay_compose_pipeline != VK_NULL_HANDLE &&
		vk.render_pass.overlay_compose != VK_NULL_HANDLE &&
		vk.overlay_color_descriptor[vk.cmd_index] != VK_NULL_HANDLE &&
		vk.framebuffers.overlay_compose[ vk.cmd->swapchain_image_index ] != VK_NULL_HANDLE ) {
		vk_end_frame_begin_post_process_pass( vk.render_pass.overlay_compose,
			vk.framebuffers.overlay_compose[ vk.cmd->swapchain_image_index ],
			vk.renderWidth, vk.renderHeight, vk.overlay_compose_pipeline );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			vk.pipeline_layout_post_process, 0, 1, &vk.overlay_color_descriptor[vk.cmd_index], 0, NULL );
		vk_end_frame_draw_fullscreen_quad( vk.renderWidth, vk.renderHeight );
		vk_end_render_pass();
	}
}
