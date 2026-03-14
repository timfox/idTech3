#include "tr_local.h"
#include "vk.h"
#include "vk_frame_end.h"
#include "vk_image_layout.h"
#include "vk_post_fog.h"
#include "vk_postfx.h"
#include "vk_postfx_params.h"
#include "vk_render_pass.h"
#include "vk_temporal.h"
#include "vk_volumetric_pass.h"

typedef struct {
	float paniniAmount;
	float paniniD;
	float paniniS;
	float aspect;
	float fovXDeg;
	float paniniBorderMode;
	float paniniDebugMode;
	float brightness;
	float paniniZoom;
	float paniniPad0;
	float paniniPad1;
	float paniniPad2;
	float exposure;
	float srcUVScaleBias[4];
} VkPostProcessPushConstants;

void vk_end_frame_record_capture_if_needed( void )
{
	if ( !backEnd.screenshotMask || !vk.capture.image ) {
		return;
	}

	{
		VkImageView capture_src = vk.color_image_view;
		uint32_t cap_w = ( gls.captureWidth > 0 ) ? (uint32_t)gls.captureWidth : 1u;
		uint32_t cap_h = ( gls.captureHeight > 0 ) ? (uint32_t)gls.captureHeight : 1u;

		vk_end_render_pass();

		if ( capture_src == VK_NULL_HANDLE ||
			vk.render_pass.capture == VK_NULL_HANDLE ||
			vk.framebuffers.capture == VK_NULL_HANDLE ) {
			return;
		}

		vk_barrier_post_fog_source_for_sampling( capture_src, "vk_end_frame pre-capture" );
		if ( r_fboDebug && r_fboDebug->integer >= 2 && vk_post_fog_fbo_debug_throttle() ) {
			ri.Printf( PRINT_DEVELOPER, "[VK][fbo] capture source -> %s view=0x%llx\n",
				vk_post_fog_source_name( capture_src ),
				(unsigned long long)(uintptr_t)capture_src );
		}

		vk_begin_render_pass_tracked( vk.render_pass.capture, vk.framebuffers.capture, qfalse, cap_w, cap_h );
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.capture_pipeline );
		{
			VkDescriptorSet capture_sets[4] = {
				vk.color_descriptor[vk.cmd_index],
				vk.depth_descriptor[vk.cmd_index],
				vk.postfx_params_descriptor[vk.cmd_index],
				PostFX_GetLUTImage()->descriptor
			};
			qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 4, capture_sets, 0, NULL );
		}
		vk_set_fullscreen_viewport_scissor( cap_w, cap_h );
		qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
	}
}

void vk_end_frame_prepare_post_process( VkImageView *post_fog_src, VkImageView *luminance_src )
{
	if ( post_fog_src ) {
		*post_fog_src = VK_NULL_HANDLE;
	}
	if ( luminance_src ) {
		*luminance_src = VK_NULL_HANDLE;
	}

	vk_end_render_pass();

	if ( !backEnd.doneFog ) {
		vk_volumetric_fog_pass();
	} else {
		vk_log_post_fog_rebind( "end_frame volumetric skipped (scene+2D on color_image)", vk.color_image_view );
		vk_update_post_fog_descriptors( vk.color_image_view );
	}

	if ( post_fog_src ) {
		*post_fog_src = vk_get_post_fog_source();
	}
	if ( luminance_src ) {
		*luminance_src = vk_get_luminance_source();
	}

	if ( post_fog_src && *post_fog_src != VK_NULL_HANDLE ) {
		vk_update_post_fog_descriptors( *post_fog_src );
		vk_barrier_post_fog_source_for_sampling( *post_fog_src, "vk_end_frame pre-luminance/gamma" );
	}
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
	push->paniniPad2 = 0.0f;

	{
		cvar_t *r_exposure_auto_var = ri.Cvar_Get( "r_exposure_auto", "0", 0 );
		float expVal = ( r_exposure && r_exposure->value > 0.0f ) ? r_exposure->value : 1.0f;
		if ( r_exposure_auto_var && r_exposure_auto_var->integer ) {
			expVal = vk.adaptedExposure > 0.0f ? vk.adaptedExposure : expVal;
		}
		if ( ( !tr.world || ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) ) && expVal < 1.0f ) {
			expVal = 1.0f;
		}
		push->exposure = expVal;
	}

	if ( !tr.world || ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) ) {
		push->brightness = 1.0f;
		push->exposure = 1.0f;
		push->paniniPad1 = 1.0f;
	}

	srcRect.offset.x = 0;
	srcRect.offset.y = 0;
	srcRect.extent.width = srcTexW;
	srcRect.extent.height = srcTexH;

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
	VkImageView gamma_src = ( post_fog_src != VK_NULL_HANDLE ) ? post_fog_src : vk.color_image_view;

	/*
	 * Consume exposure as part of the frame-end post chain rather than the
	 * scene/2D transition path. This keeps adaptation tied to gamma, not to
	 * whichever subsystem first prepared temporal state for the frame.
	 */
	vk_temporal_update_auto_exposure();
	vk_end_frame_update_gamma_target();

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
		ri.Printf( PRINT_DEVELOPER,
			"[VK][fbo] gamma pipeline=0x%llx color_desc=0x%llx layout=0x%llx rp=0x%llx fb=0x%llx\n",
			(unsigned long long)(uintptr_t)vk.gamma_pipeline,
			(unsigned long long)(uintptr_t)vk.post_color_descriptor[vk.cmd_index],
			(unsigned long long)(uintptr_t)vk.pipeline_layout_post_process,
			(unsigned long long)(uintptr_t)vk.render_pass.gamma,
			(unsigned long long)(uintptr_t)vk.framebuffers.gamma[ vk.cmd->swapchain_image_index ] );
	}

	if ( r_fboDebug && r_fboDebug->integer >= 1 && vk_post_fog_fbo_debug_throttle() ) {
		VkImageView expected = vk_get_post_fog_source();
		if ( gamma_src != expected || gamma_src != vk.post_fog_color_source ) {
			ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
				"[VK][fbo] gamma source mismatch: gamma_src=%s post_fog_color_source=%s expected=%s\n",
				vk_post_fog_source_name( gamma_src ),
				vk_post_fog_source_name( vk.post_fog_color_source ),
				vk_post_fog_source_name( expected ) );
		}
	}

	if ( vk.gamma_pipeline == VK_NULL_HANDLE || vk.post_color_descriptor[vk.cmd_index] == VK_NULL_HANDLE ||
		vk.render_pass.gamma == VK_NULL_HANDLE ||
		vk.cmd->swapchain_image_index >= MAX_SWAPCHAIN_IMAGES ||
		vk.framebuffers.gamma[ vk.cmd->swapchain_image_index ] == VK_NULL_HANDLE ||
		vk.renderWidth == 0 || vk.renderHeight == 0 ) {
		if ( vk_post_fog_fbo_debug_throttle() ) {
			ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW "[VK][fbo] gamma pass skipped: missing pipeline/descriptor/renderpass/framebuffer or zero size (%dx%d)\n",
				vk.renderWidth, vk.renderHeight );
		}
		return;
	}

	if ( gamma_src == VK_NULL_HANDLE ) {
		if ( vk_post_fog_fbo_debug_throttle() ) {
			ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW "[VK][fbo] gamma pass skipped: no valid color source (post_fog or color_image)\n" );
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

	vk_begin_render_pass_tracked( vk.render_pass.gamma, vk.framebuffers.gamma[ vk.cmd->swapchain_image_index ], qfalse, vk.renderWidth, vk.renderHeight );
	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.gamma_pipeline );
	{
		VkDescriptorSet gamma_sets[3] = {
			vk.post_color_descriptor[vk.cmd_index],
			vk.depth_descriptor[vk.cmd_index],
			vk.postfx_params_descriptor[vk.cmd_index]
		};
		VkDescriptorSet gamma_lut_set = PostFX_GetLUTImage()->descriptor;
		VkDescriptorSet gamma_bind_sets[4] = {
			gamma_sets[0],
			gamma_sets[1],
			gamma_sets[2],
			gamma_lut_set
		};
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 4, gamma_bind_sets, 0, NULL );
	}

	{
		VkPostProcessPushConstants push;
		uint32_t srcTexW = ( glConfig.vidWidth > 0 ) ? (uint32_t)glConfig.vidWidth : 1u;
		uint32_t srcTexH = ( glConfig.vidHeight > 0 ) ? (uint32_t)glConfig.vidHeight : 1u;

		vk_end_frame_fill_gamma_push_constants( &push, srcTexW, srcTexH );
		qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout_post_process, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( push ), &push );
	}

	vk_set_fullscreen_viewport_scissor( vk.renderWidth, vk.renderHeight );
	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
	vk_end_render_pass();

	if ( vk.uiOverlayActive &&
		vk.ui_overlay_image_view != VK_NULL_HANDLE &&
		vk.overlay_compose_pipeline != VK_NULL_HANDLE &&
		vk.render_pass.overlay_compose != VK_NULL_HANDLE &&
		vk.framebuffers.overlay_compose[ vk.cmd->swapchain_image_index ] != VK_NULL_HANDLE ) {
		vk_update_color_descriptor_image( vk.ui_overlay_image_view );
		vk_begin_render_pass_tracked( vk.render_pass.overlay_compose,
			vk.framebuffers.overlay_compose[ vk.cmd->swapchain_image_index ],
			qfalse, vk.renderWidth, vk.renderHeight );
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.overlay_compose_pipeline );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			vk.pipeline_layout_post_process, 0, 1, &vk.post_color_descriptor[vk.cmd_index], 0, NULL );
		vk_set_fullscreen_viewport_scissor( vk.renderWidth, vk.renderHeight );
		qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
		vk_end_render_pass();
		vk_update_color_descriptor_image( gamma_src );
	}
}
