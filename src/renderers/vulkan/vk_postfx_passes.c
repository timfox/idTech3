/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Bloom/SSAO/OIT/SSR: render-pass starters, OIT composite, SSR resolve,
SSAO/HBAO pass, and vk_bloom. Split from vk.c.
===========================================================================
*/

#include "tr_local.h"
#include "vk_image_layout.h"
#include "vk_render_pass.h"
#include "vk_scene_pass.h"
#include "vk_postfx.h"
#include "vk_post_fog.h"
#include "vk_volumetric_pass.h"
#include "vk_volumetric_internal.h"
#include "vk_util.h"
#include "vk_device.h"

static void vk_postfx_set_render_extent( uint32_t width, uint32_t height )
{
	vk.renderWidth = width;
	vk.renderHeight = height;
	vk.renderScaleX = 1.0f;
	vk.renderScaleY = 1.0f;
}

static void vk_begin_postfx_render_pass(
	VkRenderPass renderPass,
	VkFramebuffer frameBuffer,
	uint32_t width,
	uint32_t height,
	qboolean clear )
{
	vk_postfx_set_render_extent( width, height );
	vk_begin_render_pass_tracked( renderPass, frameBuffer, clear, vk.renderWidth, vk.renderHeight );
}

static void vk_begin_fullres_postfx_render_pass( VkRenderPass renderPass, VkFramebuffer frameBuffer, qboolean clear )
{
	uint32_t width = 0;
	uint32_t height = 0;

	vk_get_active_render_extent( &width, &height );
	vk_begin_postfx_render_pass( renderPass, frameBuffer, width, height, clear );
}

static void vk_copy_color_to_fog_scene( uint32_t width, uint32_t height )
{
	VkImageCopy copy_region;

	if ( vk.fog_scene_image == VK_NULL_HANDLE || vk.color_image == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( &copy_region, 0, sizeof( copy_region ) );
	copy_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copy_region.srcSubresource.layerCount = 1;
	copy_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copy_region.dstSubresource.layerCount = 1;
	copy_region.extent.width = width;
	copy_region.extent.height = height;
	copy_region.extent.depth = 1;

	record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT );
	record_image_layout_transition( vk.cmd->command_buffer, vk.fog_scene_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT );
	qvkCmdCopyImage( vk.cmd->command_buffer,
		vk.color_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		vk.fog_scene_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &copy_region );
	record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
	record_image_layout_transition( vk.cmd->command_buffer, vk.fog_scene_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
}

void vk_begin_bloom_extract_render_pass( void )
{
	VkFramebuffer frameBuffer = vk.framebuffers.bloom_extract;

	//vk.renderPassIndex = RENDER_PASS_BLOOM_EXTRACT; // doesn't matter, we will use dedicated pipelines

	vk_begin_postfx_render_pass( vk.render_pass.bloom_extract, frameBuffer,
		gls.captureWidth, gls.captureHeight, qfalse );
}


void vk_begin_blur_render_pass( uint32_t index )
{
	VkFramebuffer frameBuffer = vk.framebuffers.blur[ index ];

	//vk.renderPassIndex = RENDER_PASS_BLOOM_EXTRACT; // doesn't matter, we will use dedicated pipelines

	vk_begin_postfx_render_pass( vk.render_pass.blur[ index ], frameBuffer,
		gls.captureWidth / ( 2 << ( index / 2 ) ),
		gls.captureHeight / ( 2 << ( index / 2 ) ),
		qfalse );
}


void vk_begin_ssao_render_pass( void )
{
	VkFramebuffer frameBuffer = vk.framebuffers.ssao;
	vk_begin_fullres_postfx_render_pass( vk.render_pass.ssao, frameBuffer, qfalse );
}


void vk_begin_ssao_blur_render_pass( void )
{
	VkFramebuffer frameBuffer = vk.framebuffers.ssao_blur;
	vk_begin_fullres_postfx_render_pass( vk.render_pass.ssao_blur, frameBuffer, qfalse );
}


void vk_begin_ssao_combine_render_pass( void )
{
	VkFramebuffer frameBuffer = vk.framebuffers.ssao_combine;
	vk_begin_fullres_postfx_render_pass( vk.render_pass.ssao_combine, frameBuffer, qfalse );
}


void vk_oit_pass( const struct drawSurfsCommand_s *cmd )
{
	uint32_t fullWidth = 0;
	uint32_t fullHeight = 0;

	if ( !r_oit || !r_oit->integer || !r_fbo || !r_fbo->integer || !vk.fboActive ||
		vk.render_pass.oit_accum == VK_NULL_HANDLE || vk.render_pass.oit_resolve == VK_NULL_HANDLE ||
		vk.framebuffers.oit_accum == VK_NULL_HANDLE || vk.framebuffers.oit_resolve == VK_NULL_HANDLE ||
		vk.oit_resolve_pipeline == VK_NULL_HANDLE || vk.oit_accum_pipeline == VK_NULL_HANDLE ||
		vk.oit_opaque_descriptor == VK_NULL_HANDLE || vk.oit_accum_descriptor == VK_NULL_HANDLE ||
		vk.oit_reveal_descriptor == VK_NULL_HANDLE || vk.oit_depth_descriptor == VK_NULL_HANDLE ||
		vk.fog_scene_image_view == VK_NULL_HANDLE || vk.oit_accum_image_view == VK_NULL_HANDLE ||
		vk.oit_reveal_image_view == VK_NULL_HANDLE )
		return;

	vk_end_render_pass();
	vk_get_active_render_extent( &fullWidth, &fullHeight );

	/* Copy opaque scene to fog_scene for resolve */
	vk_copy_color_to_fog_scene( fullWidth, fullHeight );

	if ( vk.msaaActive ) {
		VkImageAspectFlags depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
		if ( glConfig.stencilBits > 0 ) {
			depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
		record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
		vk_resolve_volumetric_depth_msaa();
	}

	/* OIT accum pass: draw transparent surfaces (when oit_accum_pipeline ready) */
	vk_begin_render_pass_tracked( vk.render_pass.oit_accum, vk.framebuffers.oit_accum, qtrue, fullWidth, fullHeight );
	if ( vk.oit_accum_pipeline ) {
		backEnd.oitAccumPass = qtrue;
		backEnd.drawSurfFilter = 2;
		RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs );
		backEnd.oitAccumPass = qfalse;
		backEnd.drawSurfFilter = 0;
	}
	vk_end_render_pass();

	/* OIT resolve: composite opaque + accum to main color */
	record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT );
	vk_postfx_set_render_extent( fullWidth, fullHeight );
	vk_begin_render_pass_tracked( vk.render_pass.oit_resolve, vk.framebuffers.oit_resolve, qfalse, fullWidth, fullHeight );
	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.oit_resolve_pipeline );
	{
		VkDescriptorSet sets[3] = { vk.oit_opaque_descriptor, vk.oit_accum_descriptor, vk.oit_reveal_descriptor };
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_oit_resolve, 0, 3, sets, 0, NULL );
	}
	vk_set_fullscreen_viewport_scissor( vk.renderWidth, vk.renderHeight );
	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
	vk_end_render_pass();
	record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
	if ( vk.msaaActive ) {
		VkImageAspectFlags depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
		if ( glConfig.stencilBits > 0 ) {
			depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
		record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT );
	}

	/* Resume main pass for sun, flares, etc. */
	vk_begin_post_bloom_render_pass();
}


void vk_begin_ssr_render_pass( void )
{
	VkFramebuffer frameBuffer = vk.framebuffers.ssr;
	vk_begin_fullres_postfx_render_pass( vk.render_pass.ssr, frameBuffer, qfalse );
}


void vk_ssr_pass( void )
{
	typedef struct {
		float projection[16];
		float invProjection[16];
		float params[4];   /* maxDistance, stepSize, thickness, fadeEdge */
		float params2[4]; /* roughnessThreshold (Fresnel blend), intensity, maxDepthGradient, fresnelExponent */
	} vk_ssr_push_t;

	VkImageAspectFlags depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	vk_ssr_push_t push;

	if ( !PostFX_SSR_IsEnabled() || !vk.fboActive || vk.render_pass.ssr == VK_NULL_HANDLE ||
		vk.ssr_pipeline == VK_NULL_HANDLE || vk.ssr_image == VK_NULL_HANDLE )
	{
		return;
	}

	if ( vk.renderPassIndex == RENDER_PASS_SCREENMAP || vk.renderPassIndex == RENDER_PASS_SUN_SHADOW )
	{
		return;
	}

	if ( !backEnd.doneSurfaces || !backEnd.doneBloom )
	{
		return;
	}

	if ( glConfig.stencilBits > 0 )
		depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;

	vk_end_render_pass();

	record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, 0, 0 );

	vk_begin_ssr_render_pass();
	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.ssr_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_ssr, 0, 2, vk.ssr_descriptor, 0, NULL );

	Com_Memcpy( push.projection, backEnd.viewParms.projectionMatrix, sizeof( push.projection ) );
	{
		float inv[16];
		if ( !vk_mat4_inverse( backEnd.viewParms.projectionMatrix, inv ) )
			Com_Memcpy( inv, backEnd.viewParms.projectionMatrix, sizeof( inv ) );
		Com_Memcpy( push.invProjection, inv, sizeof( push.invProjection ) );
	}
	push.params[0] = PostFX_SSR_GetMaxDistance();
	push.params[1] = PostFX_SSR_GetStepSize();
	push.params[2] = PostFX_SSR_GetThickness();
	push.params[3] = PostFX_SSR_GetFadeEdge();
	push.params2[0] = PostFX_SSR_GetRoughnessThreshold();
	push.params2[1] = PostFX_SSR_GetIntensity();
	push.params2[2] = PostFX_SSR_GetMaxDepthGradient();
	push.params2[3] = PostFX_SSR_GetFresnelExponent();

	qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout_ssr, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( push ), &push );
	vk_set_fullscreen_viewport_scissor( vk.renderWidth, vk.renderHeight );
	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
	vk_end_render_pass();

	/* Copy ssr_image back to color */
	record_image_layout_transition( vk.cmd->command_buffer, vk.ssr_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 0 );
	record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );

	{
		VkImageCopy region;
		Com_Memset( &region, 0, sizeof( region ) );
		region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.srcSubresource.layerCount = 1;
		region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.dstSubresource.layerCount = 1;
		region.extent.width = vk.renderWidth;
		region.extent.height = vk.renderHeight;
		region.extent.depth = 1;
		qvkCmdCopyImage( vk.cmd->command_buffer, vk.ssr_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			vk.color_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );
	}

	record_image_layout_transition( vk.cmd->command_buffer, vk.ssr_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
	record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );

	record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0, 0 );
}
qboolean vk_ssao_pass( void )
{
	static qboolean warned_msaa = qfalse;

	if ( backEnd.doneSSAO || !r_ssao || !r_ssao->integer || !vk.fboActive || !backEnd.doneSurfaces )
		return qfalse;
	/* Pass culling: skip expensive SSAO for menus, cinematics, no-world */
	if ( !tr.world || ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) )
		return qfalse;
	if ( vk.msaaActive ) {
		if ( !warned_msaa ) {
			ri.Printf( PRINT_WARNING, "Vulkan: SSAO disabled while MSAA is enabled (no depth resolve yet)\n" );
			warned_msaa = qtrue;
		}
		return qfalse;
	}

	{
		typedef struct {
			float projInfo[4];
			float params[4];
			float misc[4];
			float misc2[4];
		} vk_ssao_push_t;

		vk_ssao_push_t push;
		VkImageAspectFlags depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
#ifdef USE_REVERSED_DEPTH
		const float depthIsReversed = 1.0f;
#else
		const float depthIsReversed = 0.0f;
#endif
		if ( glConfig.stencilBits > 0 )
			depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;

		vk_end_render_pass();

		record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, 0, 0 );

		vk_begin_ssao_render_pass();
		if ( r_ssaoMethod && r_ssaoMethod->integer && vk.hbao_pipeline != VK_NULL_HANDLE ) {
			qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.hbao_pipeline );
		} else {
			qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.ssao_pipeline );
		}
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_ssao, 0, 1, &vk.depth_descriptor[vk.cmd_index], 0, NULL );

		push.projInfo[0] = ( backEnd.viewParms.projectionMatrix[0] != 0.0f ) ? 1.0f / backEnd.viewParms.projectionMatrix[0] : 1.0f;
		push.projInfo[1] = ( backEnd.viewParms.projectionMatrix[5] != 0.0f ) ? 1.0f / backEnd.viewParms.projectionMatrix[5] : 1.0f;
		push.projInfo[2] = backEnd.viewParms.projectionMatrix[10];
		push.projInfo[3] = backEnd.viewParms.projectionMatrix[14];

		if ( r_ssaoMethod && r_ssaoMethod->integer ) {
			push.params[0] = ( r_ssaoRadius->value > 0.0f ) ? r_ssaoRadius->value / 100.0f : 0.1f;
			push.params[1] = ( r_ssaoBias->value > 0.0f ) ? r_ssaoBias->value * 0.01f : 0.04f;
			push.params[2] = r_ssaoIntensity->value;
			push.params[3] = r_ssaoPower->value;
			push.misc[0] = (float)( r_hbaoDirections ? r_hbaoDirections->integer : 8 );
			push.misc[1] = (float)( r_hbaoSteps ? r_hbaoSteps->integer : 8 );
			push.misc[2] = ( vk.renderWidth > 0 ) ? 1.0f / (float)vk.renderWidth : 1.0f;
			if ( depthIsReversed > 0.5f )
				push.misc[2] = -push.misc[2];
			push.misc[3] = ( vk.renderHeight > 0 ) ? 1.0f / (float)vk.renderHeight : 1.0f;
		} else {
			push.params[0] = r_ssaoRadius->value;
			push.params[1] = r_ssaoBias->value;
			push.params[2] = r_ssaoIntensity->value;
			push.params[3] = r_ssaoPower->value;
			push.misc[0] = (float)r_ssaoSamples->integer;
			push.misc[1] = ( vk.renderWidth > 0 ) ? 1.0f / (float)vk.renderWidth : 1.0f;
			push.misc[2] = ( vk.renderHeight > 0 ) ? 1.0f / (float)vk.renderHeight : 1.0f;
			push.misc[3] = depthIsReversed;
			push.misc2[0] = (float)( r_ssaoMethod ? r_ssaoMethod->integer : 0 );
			push.misc2[1] = (float)( r_hbaoDirections ? r_hbaoDirections->integer : 8 );
			push.misc2[2] = (float)( r_hbaoSteps ? r_hbaoSteps->integer : 4 );
			push.misc2[3] = ( r_ssaoMaxDepthGradient && r_ssaoMaxDepthGradient->value > 0.0f ) ? r_ssaoMaxDepthGradient->value : 0.0f;
		}

		qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout_ssao, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( push ), &push );
		vk_set_fullscreen_viewport_scissor( vk.renderWidth, vk.renderHeight );
		qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
		vk_end_render_pass();

		vk_begin_ssao_blur_render_pass();
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.ssao_blur_pipeline );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_ssao, 0, 1, &vk.ssao_descriptor, 0, NULL );

		push.params[0] = ( r_ssaoBlurRadius && r_ssaoBlurRadius->integer >= 0 ) ? (float)r_ssaoBlurRadius->integer : 2.0f;
		push.params[1] = push.params[2] = push.params[3] = 0.0f;
		qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout_ssao, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( push ), &push );
		vk_set_fullscreen_viewport_scissor( vk.renderWidth, vk.renderHeight );
		qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
		vk_end_render_pass();

		vk_copy_color_to_fog_scene( vk.renderWidth > 0 ? (uint32_t)vk.renderWidth : 1u,
			vk.renderHeight > 0 ? (uint32_t)vk.renderHeight : 1u );
		if ( vk.fog_scene_image_view != VK_NULL_HANDLE && vk.ssao_scene_descriptor != VK_NULL_HANDLE && vk.ssao_blur_descriptor != VK_NULL_HANDLE ) {
			vk_begin_ssao_combine_render_pass();
			if ( r_ssaoDebugView && r_ssaoDebugView->integer == 2 ) {
				qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.ssao_depth_debug_pipeline );
				qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.depth_descriptor[vk.cmd_index], 0, NULL );
			} else {
				qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
					( r_ssaoDebugView && r_ssaoDebugView->integer ) ? vk.ssao_debug_pipeline : vk.ssao_combine_pipeline );
				if ( r_ssaoDebugView && r_ssaoDebugView->integer ) {
					qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.ssao_blur_descriptor, 0, NULL );
				} else {
					VkDescriptorSet ssao_combine_sets[2] = { vk.ssao_scene_descriptor, vk.ssao_blur_descriptor };
					qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_ssao_combine, 0, 2, ssao_combine_sets, 0, NULL );
				}
			}
			vk_set_fullscreen_viewport_scissor( vk.renderWidth, vk.renderHeight );
			qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
			vk_end_render_pass();
		}
		record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0, 0 );
	}

	backEnd.doneSSAO = qtrue;
	return qtrue;
}


qboolean vk_bloom( void )
{
	uint32_t i;
	const qboolean canBlitDownsample = vk_format_has_features( vk.physical_device, vk.bloom_format,
		VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT );

	if ( vk.renderPassIndex == RENDER_PASS_SCREENMAP || vk.renderPassIndex == RENDER_PASS_SUN_SHADOW )
	{
		return qfalse;
	}

	if ( backEnd.doneBloom || !backEnd.doneSurfaces || !vk.fboActive )
	{
		return qfalse;
	}
	/* Pass culling: skip bloom for menus/cinematics (no 3D world) */
	if ( !tr.world || ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) )
	{
		return qfalse;
	}

	vk_end_render_pass(); // end main/post-bloom continuation
	if ( !backEnd.doneFog ) {
		vk_volumetric_fog_pass();
	}

	/* Ensure color_image is ready for sampling before bloom extract */
	vk_barrier_post_fog_source_for_sampling( vk.color_image_view, "pre-bloom-extract" );

	// bloom extraction
	vk_begin_bloom_extract_render_pass();
	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.bloom_extract_pipeline );
	{
		VkDescriptorSet bloom_sets[3] = {
			vk.color_descriptor[vk.cmd_index],
			vk.depth_descriptor[vk.cmd_index],
			vk.postfx_params_descriptor[vk.cmd_index]
		};
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 3, bloom_sets, 0, NULL );
	}
	vk_set_fullscreen_viewport_scissor( vk.renderWidth, vk.renderHeight );
	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
	vk_end_render_pass();

	if ( canBlitDownsample ) {
		// Split pipeline: downsample first, then blur at same resolution.
		for ( i = 0; i < VK_NUM_BLOOM_PASSES * 2; i += 2 ) {
			VkImageBlit region;
			const uint32_t level = i / 2;
			const uint32_t srcIndex = ( i == 0 ) ? 0 : i;
			const uint32_t dstIndex = i + 2;
			const uint32_t srcWidth = MAX( 1u, gls.captureWidth / ( 1u << level ) );
			const uint32_t srcHeight = MAX( 1u, gls.captureHeight / ( 1u << level ) );
			const uint32_t dstWidth = MAX( 1u, srcWidth / 2u );
			const uint32_t dstHeight = MAX( 1u, srcHeight / 2u );
			VkCommandBuffer cmd = vk.cmd->command_buffer;

			record_image_layout_transition( cmd, vk.bloom_image[srcIndex], VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 0 );
			record_image_layout_transition( cmd, vk.bloom_image[dstIndex], VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );

			Com_Memset( &region, 0, sizeof( region ) );
			region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.srcSubresource.layerCount = 1;
			region.srcOffsets[1].x = srcWidth;
			region.srcOffsets[1].y = srcHeight;
			region.srcOffsets[1].z = 1;
			region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.dstSubresource.layerCount = 1;
			region.dstOffsets[1].x = dstWidth;
			region.dstOffsets[1].y = dstHeight;
			region.dstOffsets[1].z = 1;

			qvkCmdBlitImage( cmd, vk.bloom_image[srcIndex], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				vk.bloom_image[dstIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_LINEAR );

			record_image_layout_transition( cmd, vk.bloom_image[srcIndex], VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
			record_image_layout_transition( cmd, vk.bloom_image[dstIndex], VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );

			// horizontal blur: downsampled source -> ping image
			vk_begin_blur_render_pass( i + 0 );
			qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.blur_pipeline[i+0] );
			qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.bloom_image_descriptor[dstIndex], 0, NULL );
			qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
			vk_end_render_pass();

			// vertical blur: ping image -> final image for this level
			vk_begin_blur_render_pass( i + 1 );
			qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.blur_pipeline[i+1] );
			qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.bloom_image_descriptor[i+1], 0, NULL );
			qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
			vk_end_render_pass();
		}
	} else {
		// Fallback to legacy downsample+blur in one pass if blit features are unavailable.
		for ( i = 0; i < VK_NUM_BLOOM_PASSES * 2; i += 2 ) {
			vk_begin_blur_render_pass( i + 0 );
			qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.blur_pipeline[i+0] );
			qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.bloom_image_descriptor[i+0], 0, NULL );
			qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
			vk_end_render_pass();

			vk_begin_blur_render_pass( i + 1 );
			qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.blur_pipeline[i+1] );
			qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.bloom_image_descriptor[i+1], 0, NULL );
			qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
			vk_end_render_pass();
		}
	}

	vk_begin_post_bloom_render_pass(); // begin post-bloom
	{
		VkDescriptorSet dset[VK_NUM_BLOOM_PASSES];

		for ( i = 0; i < VK_NUM_BLOOM_PASSES; i++ )
		{
			dset[i] = vk.bloom_image_descriptor[(i+1)*2];
		}

		// blend downscaled buffers to main fbo
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.bloom_blend_pipeline );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_blend, 0, ARRAY_LEN(dset), dset, 0, NULL );
		vk_set_fullscreen_viewport_scissor( vk.renderWidth, vk.renderHeight );
		qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
	}

	// invalidate pipeline state cache
	//vk.cmd->last_pipeline = VK_NULL_HANDLE;

	if ( vk.cmd->last_pipeline != VK_NULL_HANDLE )
	{
		// restore last pipeline
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.cmd->last_pipeline );

		vk_update_mvp( NULL );

		// force depth range and viewport/scissor updates
		vk.cmd->depth_range = DEPTH_RANGE_COUNT;

		uint32_t offsets[VK_DESC_UNIFORM_COUNT], offset_count;

		// restore clobbered descriptor sets
		for ( i = 0; i < VK_NUM_BLOOM_PASSES; i++ ) {
			if ( vk.cmd->descriptor_set.current[i] != VK_NULL_HANDLE ) {
				if ( i == VK_DESC_UNIFORM /*|| i == VK_DESC_STORAGE*/ ) {
					offset_count = 0;

					offsets[offset_count++] = vk.cmd->descriptor_set.offset[VK_DESC_UNIFORM_MAIN_BINDING];
					offsets[offset_count++] = vk.cmd->descriptor_set.offset[VK_DESC_UNIFORM_CAMERA_BINDING];
					offsets[offset_count++] = vk.cmd->descriptor_set.offset[VK_DESC_UNIFORM_IQM_SKIN_BINDING];
					offsets[offset_count++] = vk.cmd->descriptor_set.offset[VK_DESC_UNIFORM_IQM_MORPH_BINDING];

					qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout, i, 1, &vk.cmd->descriptor_set.current[i], offset_count, offsets );
				}
				else
					qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout, i, 1, &vk.cmd->descriptor_set.current[i], 0, NULL );
			}
		}
	}

	backEnd.doneBloom = qtrue;

	return qtrue;
}
