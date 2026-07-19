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
#include "vk_post_aa.h"
#include "vk_util.h"
#include "vk_device.h"
#include "vk_view_state.h"
#include "vk_deferred_gbuffer.h"
#include "vk_visibility_buffer.h"
#include "vk_reactive_mask.h"
#include "vk_ambient_visibility.h"
#include "vk_pass_registry.h"

static void vk_oit_validate_pass_break( const char *stage )
{
	if ( !r_fboDebug || r_fboDebug->integer < 1 || !vk_post_fog_fbo_debug_throttle() ) {
		return;
	}

	if ( vk.inRenderPass ) {
		ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
			"[VK][OIT] %s: expected no active render pass before OIT side pass, still in %d\n",
			stage ? stage : "pass_break", (int)vk.renderPassIndex );
	}

	if ( backEnd.drawSurfFilter && backEnd.drawSurfFilter != 2 ) {
		ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
			"[VK][OIT] %s: expected transparent drawSurfFilter 2 or 0, got %d\n",
			stage ? stage : "pass_break", backEnd.drawSurfFilter );
	}
}

/*
 * Full-framebuffer visibility after OIT color writes. Render-pass EXTERNAL deps use
 * BY_REGION which is insufficient when the next pass is a fullscreen resolve/stamp
 * that reads neighboring (or all) texels — races show up as horizontal scanline tears.
 */
static void vk_oit_barrier_targets_for_sampling( const char *reason )
{
	VkMemoryBarrier mb;
	VkImageMemoryBarrier img[4];
	uint32_t imgCount = 0;

	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( &mb, 0, sizeof( mb ) );
	mb.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	mb.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	mb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	Com_Memset( img, 0, sizeof( img ) );
#define VK_OIT_BARRIER_IMG( handle, layout ) \
	do { \
		if ( ( handle ) != VK_NULL_HANDLE && imgCount < ARRAY_LEN( img ) ) { \
			img[imgCount].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER; \
			img[imgCount].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; \
			img[imgCount].dstAccessMask = VK_ACCESS_SHADER_READ_BIT; \
			img[imgCount].oldLayout = ( layout ); \
			img[imgCount].newLayout = ( layout ); \
			img[imgCount].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; \
			img[imgCount].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; \
			img[imgCount].image = ( handle ); \
			img[imgCount].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; \
			img[imgCount].subresourceRange.levelCount = 1; \
			img[imgCount].subresourceRange.layerCount = 1; \
			imgCount++; \
		} \
	} while ( 0 )

	VK_OIT_BARRIER_IMG( vk.oit_accum_image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	VK_OIT_BARRIER_IMG( vk.oit_reveal_image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	VK_OIT_BARRIER_IMG( vk.oit_moments_image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	VK_OIT_BARRIER_IMG( vk.oit_b0_image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
#undef VK_OIT_BARRIER_IMG

	qvkCmdPipelineBarrier( vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0, 1, &mb, 0, NULL, imgCount, imgCount ? img : NULL );

	vk_spine_note_barrier( VK_SPINE_RES_OIT_ACCUM, VK_SPINE_PASS_OIT_RESOLVE, reason );
	vk_spine_note_barrier( VK_SPINE_RES_OIT_REVEAL, VK_SPINE_PASS_OIT_RESOLVE, reason );
	vk_spine_note_barrier( VK_SPINE_RES_OIT_MOMENTS, VK_SPINE_PASS_OIT_RESOLVE, reason );
	vk_spine_note_barrier( VK_SPINE_RES_OIT_B0, VK_SPINE_PASS_OIT_RESOLVE, reason );

	if ( r_fboDebug && r_fboDebug->integer >= 2 && vk_post_fog_fbo_debug_throttle() ) {
		uint32_t w = 0, h = 0;
		vk_get_active_render_extent( &w, &h );
		ri.Printf( PRINT_DEVELOPER,
			"[VK][OIT] barrier→sample (%s): render=%ux%u mainColor=%ux%u imgs=%u frame=%u\n",
			reason ? reason : "unspecified", w, h,
			vk.mainColorWidth, vk.mainColorHeight, imgCount, vk.temporal.frameIndex );
	}
}

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

static void vk_postfx_draw_fullscreen_quad( void )
{
	vk_set_fullscreen_viewport_scissor( vk.renderWidth, vk.renderHeight );
	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
}

static void vk_postfx_run_blur_pass( uint32_t passIndex, VkDescriptorSet sourceDescriptor )
{
	vk_begin_blur_render_pass( passIndex );
	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.blur_pipeline[passIndex] );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.pipeline_layout_post_process, 0, 1, &sourceDescriptor, 0, NULL );
	vk_postfx_draw_fullscreen_quad();
	vk_end_render_pass();
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
	uint32_t width = gls.captureWidth / ( 2 << ( index / 2 ) );
	uint32_t height = gls.captureHeight / ( 2 << ( index / 2 ) );

	//vk.renderPassIndex = RENDER_PASS_BLOOM_EXTRACT; // doesn't matter, we will use dedicated pipelines

	vk_begin_postfx_render_pass( vk.render_pass.blur[ index ], frameBuffer,
		width, height, qfalse );
}

static qboolean vk_bloom_validate_step( const char *step, uint32_t w, uint32_t h, uint32_t mipIndex )
{
	uint32_t expectW;
	uint32_t expectH;

	if ( mipIndex >= ARRAY_LEN( vk.bloom_mip_extent ) ) {
		vk_pass_diag_stage( "bloom_dim_mismatch" );
		if ( r_fboDebug && r_fboDebug->integer >= 1 && vk_post_fog_fbo_debug_throttle() ) {
			ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
				"[VK][bloom] %s: mip index %u out of range\n",
				step ? step : "step", mipIndex );
		}
		return qfalse;
	}

	expectW = vk.bloom_mip_extent[mipIndex].width;
	expectH = vk.bloom_mip_extent[mipIndex].height;
	if ( expectW == 0 || expectH == 0 ) {
		return qtrue; /* extents not recorded yet; skip check */
	}
	if ( w != expectW || h != expectH ) {
		vk_pass_diag_stage( "bloom_dim_mismatch" );
		if ( r_fboDebug && r_fboDebug->integer >= 1 && vk_post_fog_fbo_debug_throttle() ) {
			ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
				"[VK][bloom] %s: dim mismatch mip%u got %ux%u expected %ux%u\n",
				step ? step : "step", mipIndex, w, h, expectW, expectH );
		}
		return qfalse;
	}
	return qtrue;
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
	qboolean mboit = ( r_oit && r_oit->integer == 2 );
	qboolean classify = ( r_oitClassify && r_oitClassify->integer );
	int bucket;
	int bucket_count;

	if ( !r_oit || !r_oit->integer || !r_fbo || !r_fbo->integer || !vk.fboActive ||
		vk.render_pass.oit_accum == VK_NULL_HANDLE || vk.render_pass.oit_resolve == VK_NULL_HANDLE ||
		vk.framebuffers.oit_accum == VK_NULL_HANDLE || vk.framebuffers.oit_resolve == VK_NULL_HANDLE ||
		vk.oit_resolve_pipeline == VK_NULL_HANDLE ||
		vk.oit_opaque_descriptor == VK_NULL_HANDLE || vk.oit_accum_descriptor == VK_NULL_HANDLE ||
		vk.oit_reveal_descriptor == VK_NULL_HANDLE || vk.oit_depth_descriptor == VK_NULL_HANDLE ||
		vk.fog_scene_image_view == VK_NULL_HANDLE || vk.oit_accum_image_view == VK_NULL_HANDLE ||
		vk.oit_reveal_image_view == VK_NULL_HANDLE )
		return;

	if ( mboit && ( vk.render_pass.oit_moments == VK_NULL_HANDLE ||
		vk.framebuffers.oit_moments == VK_NULL_HANDLE ||
		vk.oit_moments_pipeline == VK_NULL_HANDLE || vk.oit_accum_mboit_pipeline == VK_NULL_HANDLE ||
		vk.oit_moments_image_view == VK_NULL_HANDLE || vk.oit_b0_image_view == VK_NULL_HANDLE ||
		vk.oit_moments_descriptor == VK_NULL_HANDLE || vk.oit_b0_descriptor == VK_NULL_HANDLE ) )
		return;

	if ( !mboit && vk.oit_accum_pipeline == VK_NULL_HANDLE )
		return;

	if ( mboit ) {
		vk_spine_pass_begin( VK_SPINE_PASS_MBOIT_MOMENTS );
	} else {
		vk_spine_pass_begin( VK_SPINE_PASS_WBOIT_ACCUM );
	}

	vk_end_render_pass();
	vk_oit_validate_pass_break( "oit_pass_begin" );
	vk_get_active_render_extent( &fullWidth, &fullHeight );

	vk_reactive_mask_clear();

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

	bucket_count = classify ? 2 : 1;
	for ( bucket = 0; bucket < bucket_count; bucket++ ) {
		qboolean bucket_mboit = mboit;
		/* Bucket 0 = all (or alpha-blend); bucket 1 = additive (WBOIT only, no moments). */
		if ( classify ) {
			backEnd.oitBucketFilter = ( bucket == 0 ) ? 1 : 2;
			bucket_mboit = ( mboit && bucket == 0 ) ? qtrue : qfalse;
			if ( bucket == 1 ) {
				/* Composite bucket A into color, then use as base for additive. */
				vk_copy_color_to_fog_scene( fullWidth, fullHeight );
			}
		} else {
			backEnd.oitBucketFilter = 0;
		}

		if ( bucket_mboit ) {
			vk_begin_render_pass_tracked( vk.render_pass.oit_moments, vk.framebuffers.oit_moments, qtrue, fullWidth, fullHeight );
			backEnd.oitMomentsPass = qtrue;
			backEnd.drawSurfFilter = 2;
			RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs );
			backEnd.oitMomentsPass = qfalse;
			backEnd.drawSurfFilter = 0;
			vk_end_render_pass();
			vk_oit_barrier_targets_for_sampling( "post-mboit-moments" );
		}

		vk_begin_render_pass_tracked( vk.render_pass.oit_accum, vk.framebuffers.oit_accum, qtrue, fullWidth, fullHeight );
		if ( bucket_mboit ? vk.oit_accum_mboit_pipeline : vk.oit_accum_pipeline ) {
			backEnd.oitAccumPass = qtrue;
			backEnd.drawSurfFilter = 2;
			RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs );
			backEnd.oitAccumPass = qfalse;
			backEnd.drawSurfFilter = 0;
		}
		vk_end_render_pass();

		vk_oit_barrier_targets_for_sampling( bucket_mboit ? "post-mboit-accum" : "post-wboit-accum" );
		vk_reactive_mask_stamp_from_reveal();

		if ( bucket == 0 ) {
			if ( mboit ) {
				vk_spine_pass_end( VK_SPINE_PASS_MBOIT_MOMENTS );
				vk_spine_pass_begin( VK_SPINE_PASS_MBOIT_ACCUM );
				vk_spine_pass_end( VK_SPINE_PASS_MBOIT_ACCUM );
			} else {
				vk_spine_pass_end( VK_SPINE_PASS_WBOIT_ACCUM );
			}
			vk_spine_pass_begin( VK_SPINE_PASS_OIT_RESOLVE );
			vk_spine_expect_layout( VK_SPINE_RES_OIT_ACCUM, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_SPINE_PASS_OIT_RESOLVE, "oit_resolve" );
			vk_spine_expect_layout( VK_SPINE_RES_OIT_REVEAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_SPINE_PASS_OIT_RESOLVE, "oit_resolve" );
		}

		record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT );
		vk_postfx_set_render_extent( fullWidth, fullHeight );
		if ( r_fboDebug && r_fboDebug->integer >= 1 && vk_post_fog_fbo_debug_throttle() ) {
			ri.Printf( PRINT_DEVELOPER,
				"[VK][OIT] resolve: extent=%ux%u mainColor=%ux%u mboit=%d bucket=%d/%d frame=%u\n",
				fullWidth, fullHeight, vk.mainColorWidth, vk.mainColorHeight,
				(int)mboit, bucket + 1, bucket_count, vk.temporal.frameIndex );
		}
		vk_begin_render_pass_tracked( vk.render_pass.oit_resolve, vk.framebuffers.oit_resolve, qfalse, fullWidth, fullHeight );
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.oit_resolve_pipeline );
		{
			VkDescriptorSet moments_set = vk.oit_moments_descriptor != VK_NULL_HANDLE
				? vk.oit_moments_descriptor : vk.oit_reveal_descriptor;
			VkDescriptorSet b0_set = vk.oit_b0_descriptor != VK_NULL_HANDLE
				? vk.oit_b0_descriptor : vk.oit_reveal_descriptor;
			/* WBOIT: bind opaque depth on set 3 for transparent-depth debug views. */
			if ( !mboit && vk.oit_depth_descriptor != VK_NULL_HANDLE ) {
				moments_set = vk.oit_depth_descriptor;
			}
			VkDescriptorSet sets[5] = {
				vk.oit_opaque_descriptor,
				vk.oit_accum_descriptor,
				vk.oit_reveal_descriptor,
				moments_set,
				b0_set
			};
			int push_data[4];

			qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
				vk.pipeline_layout_oit_resolve, 0, 5, sets, 0, NULL );
			push_data[0] = ( r_oitDebug && r_oitDebug->integer > 0 ) ? r_oitDebug->integer : 0;
			push_data[1] = mboit ? 2 : 1;
			push_data[2] = 0;
			push_data[3] = 0;
			qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout_oit_resolve,
				VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( push_data ), push_data );
		}
		vk_postfx_draw_fullscreen_quad();
		vk_end_render_pass();
		record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
	}
	backEnd.oitBucketFilter = 0;
	vk_spine_pass_end( VK_SPINE_PASS_OIT_RESOLVE );

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
	vk_spine_expect_layout( VK_SPINE_RES_DEPTH, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_SPINE_PASS_SSR, "ssr_depth" );
	if ( vk.ssr_image != VK_NULL_HANDLE ) {
		vk_spine_note_layout( VK_SPINE_RES_SSR, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL );
	}

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
	vk_postfx_draw_fullscreen_quad();
	vk_end_render_pass();

	/* Copy ssr_image back to color (main FBO extent; not transient vk.renderWidth). */
	{
		const uint32_t ssr_rw = vk_get_render_target_width();
		const uint32_t ssr_rh = vk_get_render_target_height();

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
			region.extent.width = ssr_rw > 0u ? ssr_rw : 1u;
			region.extent.height = ssr_rh > 0u ? ssr_rh : 1u;
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
}
qboolean vk_ssao_pass( void )
{
	static qboolean warned_msaa = qfalse;

	if ( backEnd.doneSSAO || !r_ssao || !r_ssao->integer || !vk.fboActive || !backEnd.doneSurfaces ||
		vk_ambient_visibility_blocks_legacy_post() )
		return qfalse;
	/* Pass culling: skip expensive SSAO for menus, cinematics, no-world.
	 * Use doneWorldScene — HUD/weapon may set RDF_NOWORLDMODEL after the world view. */
	if ( !tr.world || !backEnd.doneWorldScene )
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
		const uint32_t ssaoTexW = vk_get_render_target_width();
		const uint32_t ssaoTexH = vk_get_render_target_height();
		const float depthIsReversed = 1.0f;
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
			push.misc[2] = ( ssaoTexW > 0u ) ? 1.0f / (float)ssaoTexW : 1.0f;
			if ( depthIsReversed > 0.5f )
				push.misc[2] = -push.misc[2];
			push.misc[3] = ( ssaoTexH > 0u ) ? 1.0f / (float)ssaoTexH : 1.0f;
		} else {
			push.params[0] = r_ssaoRadius->value;
			push.params[1] = r_ssaoBias->value;
			push.params[2] = r_ssaoIntensity->value;
			push.params[3] = r_ssaoPower->value;
			push.misc[0] = (float)r_ssaoSamples->integer;
			push.misc[1] = ( ssaoTexW > 0u ) ? 1.0f / (float)ssaoTexW : 1.0f;
			push.misc[2] = ( ssaoTexH > 0u ) ? 1.0f / (float)ssaoTexH : 1.0f;
			push.misc[3] = depthIsReversed;
			push.misc2[0] = (float)( r_ssaoMethod ? r_ssaoMethod->integer : 0 );
			push.misc2[1] = (float)( r_hbaoDirections ? r_hbaoDirections->integer : 8 );
			push.misc2[2] = (float)( r_hbaoSteps ? r_hbaoSteps->integer : 4 );
			push.misc2[3] = ( r_ssaoMaxDepthGradient && r_ssaoMaxDepthGradient->value > 0.0f ) ? r_ssaoMaxDepthGradient->value : 0.0f;
		}

		qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout_ssao, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( push ), &push );
		vk_postfx_draw_fullscreen_quad();
		vk_end_render_pass();

		vk_begin_ssao_blur_render_pass();
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.ssao_blur_pipeline );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_ssao, 0, 1, &vk.ssao_descriptor, 0, NULL );

		push.params[0] = ( r_ssaoBlurRadius && r_ssaoBlurRadius->integer >= 0 ) ? (float)r_ssaoBlurRadius->integer : 2.0f;
		push.params[1] = push.params[2] = push.params[3] = 0.0f;
		qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout_ssao, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( push ), &push );
		vk_postfx_draw_fullscreen_quad();
		vk_end_render_pass();

		vk_copy_color_to_fog_scene( ssaoTexW > 0u ? ssaoTexW : 1u,
			ssaoTexH > 0u ? ssaoTexH : 1u );
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
			vk_postfx_draw_fullscreen_quad();
			vk_end_render_pass();
		}
		record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0, 0 );
	}

	backEnd.doneSSAO = qtrue;
	return qtrue;
}


static qboolean vk_bloom_resources_ready( const char **outReason )
{
	uint32_t i;

	if ( outReason ) {
		*outReason = NULL;
	}
	if ( vk.color_image == VK_NULL_HANDLE || vk.color_image_view == VK_NULL_HANDLE ) {
		if ( outReason ) {
			*outReason = "null color source image/view";
		}
		return qfalse;
	}
	if ( vk.render_pass.bloom_extract == VK_NULL_HANDLE || vk.framebuffers.bloom_extract == VK_NULL_HANDLE ) {
		if ( outReason ) {
			*outReason = "missing bloom extract pass/framebuffer";
		}
		return qfalse;
	}
	if ( vk.bloom_extract_pipeline == VK_NULL_HANDLE || vk.bloom_blend_pipeline == VK_NULL_HANDLE ) {
		if ( outReason ) {
			*outReason = "missing bloom extract/blend pipeline";
		}
		return qfalse;
	}
	if ( gls.captureWidth < 1 || gls.captureHeight < 1 ) {
		if ( outReason ) {
			*outReason = "invalid bloom capture dimensions";
		}
		return qfalse;
	}
	if ( vk.bloom_capture_extent.width > 0 && vk.bloom_capture_extent.height > 0 &&
		( (uint32_t)gls.captureWidth != vk.bloom_capture_extent.width ||
			(uint32_t)gls.captureHeight != vk.bloom_capture_extent.height ) ) {
		if ( outReason ) {
			*outReason = "bloom capture extent drift vs attachments";
		}
		return qfalse;
	}
	for ( i = 0; i < 1u + VK_NUM_BLOOM_PASSES * 2u; i++ ) {
		if ( vk.bloom_image[i] == VK_NULL_HANDLE || vk.bloom_image_view[i] == VK_NULL_HANDLE ||
			vk.bloom_image_descriptor[i] == VK_NULL_HANDLE ) {
			if ( outReason ) {
				*outReason = "incomplete bloom attachment chain";
			}
			return qfalse;
		}
	}
	for ( i = 0; i < VK_NUM_BLOOM_PASSES * 2u; i++ ) {
		if ( vk.render_pass.blur[i] == VK_NULL_HANDLE || vk.framebuffers.blur[i] == VK_NULL_HANDLE ||
			vk.blur_pipeline[i] == VK_NULL_HANDLE ) {
			if ( outReason ) {
				*outReason = "incomplete bloom blur pass chain";
			}
			return qfalse;
		}
	}
	if ( vk.render_pass.post_bloom == VK_NULL_HANDLE ) {
		if ( outReason ) {
			*outReason = "missing post-bloom render pass";
		}
		return qfalse;
	}
	return qtrue;
}

qboolean vk_bloom( void )
{
	uint32_t i;
	const char *skipReason = NULL;
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
	/* Pass culling: skip bloom for menus/cinematics (no 3D world).
	 * Use doneWorldScene — HUD/weapon may set RDF_NOWORLDMODEL after the world view. */
	if ( !tr.world || !backEnd.doneWorldScene )
	{
		return qfalse;
	}

	if ( !vk_bloom_resources_ready( &skipReason ) ) {
		vk_pass_diag_stage( "bloom_skip_invalid" );
		if ( r_fboDebug && r_fboDebug->integer >= 1 && vk_post_fog_fbo_debug_throttle() ) {
			ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
				"[VK][bloom] skip: %s (capture=%dx%d threshold=%.3f intensity=%.3f)\n",
				skipReason ? skipReason : "unknown",
				gls.captureWidth, gls.captureHeight,
				r_bloom_threshold ? r_bloom_threshold->value : 0.0f,
				r_bloom_intensity ? r_bloom_intensity->value : 0.0f );
		}
		return qfalse;
	}

	vk_assert_ui_pass_consistency( "vk_bloom" );

	vk_pass_diag_stage( "bloom_enter" );
	vk_spine_expect_layout( VK_SPINE_RES_HDR_COLOR, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_SPINE_PASS_BLOOM, "bloom_extract" );
	if ( r_fboDebug && r_fboDebug->integer >= 1 && vk_post_fog_fbo_debug_throttle() ) {
		ri.Printf( PRINT_DEVELOPER,
			"[VK][bloom] enter: source=%s capture=%dx%d render=%ux%u threshold=%.3f intensity=%.3f\n",
			vk_post_fog_source_name( vk.color_image_view ),
			gls.captureWidth, gls.captureHeight,
			vk.renderWidth, vk.renderHeight,
			r_bloom_threshold ? r_bloom_threshold->value : 0.0f,
			r_bloom_intensity ? r_bloom_intensity->value : 0.0f );
	}

	vk_end_render_pass(); // end main/post-bloom continuation
	vk_deferred_gbuffer_draw_debug();
	vk_visibility_buffer_draw_debug();
	if ( !backEnd.doneFog ) {
		vk_volumetric_fog_pass();
	}

	/* Ensure color_image is ready for sampling before bloom extract */
	vk_barrier_post_fog_source_for_sampling( vk.color_image_view, "pre-bloom-extract" );
	vk_spine_note_layout( VK_SPINE_RES_HDR_COLOR, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	vk_spine_note_barrier( VK_SPINE_RES_HDR_COLOR, VK_SPINE_PASS_BLOOM, "pre-bloom-extract" );
	vk_pass_diag_stage( "bloom_extract" );

	if ( !vk_bloom_validate_step( "extract", (uint32_t)gls.captureWidth, (uint32_t)gls.captureHeight, 0 ) ) {
		/* Stay out of bloom chain; resume post-bloom for 2D/HUD continuation. */
		vk_begin_post_bloom_render_pass();
		backEnd.doneBloom = qtrue;
		return qfalse;
	}

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
	vk_postfx_draw_fullscreen_quad();
	vk_end_render_pass();

	if ( canBlitDownsample ) {
		// Split pipeline: downsample first, then blur at same resolution.
		vk_pass_diag_stage( "bloom_downsample_blur" );
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

			if ( !vk_bloom_validate_step( "blit_src", srcWidth, srcHeight, srcIndex ) ||
				!vk_bloom_validate_step( "blit_dst", dstWidth, dstHeight, dstIndex ) ) {
				break;
			}

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

			{
				const uint32_t blurW = MAX( 1u, gls.captureWidth / ( 2u << ( ( i + 0 ) / 2 ) ) );
				const uint32_t blurH = MAX( 1u, gls.captureHeight / ( 2u << ( ( i + 0 ) / 2 ) ) );
				const uint32_t blurMip = ( i + 0 ) + 1;
				if ( !vk_bloom_validate_step( "blur_h", blurW, blurH, blurMip ) ||
					!vk_bloom_validate_step( "blur_v", blurW, blurH, blurMip + 1 ) ) {
					break;
				}
			}

			// horizontal blur: downsampled source -> ping image
			vk_postfx_run_blur_pass( i + 0, vk.bloom_image_descriptor[dstIndex] );

			// vertical blur: ping image -> final image for this level
			vk_postfx_run_blur_pass( i + 1, vk.bloom_image_descriptor[i+1] );
		}
	} else {
		// Fallback to legacy downsample+blur in one pass if blit features are unavailable.
		vk_pass_diag_stage( "bloom_legacy_blur" );
		for ( i = 0; i < VK_NUM_BLOOM_PASSES * 2; i += 2 ) {
			const uint32_t blurW = MAX( 1u, gls.captureWidth / ( 2u << ( i / 2 ) ) );
			const uint32_t blurH = MAX( 1u, gls.captureHeight / ( 2u << ( i / 2 ) ) );
			if ( !vk_bloom_validate_step( "legacy_blur_h", blurW, blurH, i + 1 ) ||
				!vk_bloom_validate_step( "legacy_blur_v", blurW, blurH, i + 2 ) ) {
				break;
			}
			vk_postfx_run_blur_pass( i + 0, vk.bloom_image_descriptor[i+0] );
			vk_postfx_run_blur_pass( i + 1, vk.bloom_image_descriptor[i+1] );
		}
	}

	vk_begin_post_bloom_render_pass(); // begin post-bloom
	vk_pass_diag_stage( "bloom_blend" );
	{
		VkDescriptorSet dset[VK_NUM_BLOOM_PASSES];

		for ( i = 0; i < VK_NUM_BLOOM_PASSES; i++ )
		{
			dset[i] = vk.bloom_image_descriptor[(i+1)*2];
		}

		// blend downscaled buffers to main fbo
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.bloom_blend_pipeline );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_blend, 0, ARRAY_LEN(dset), dset, 0, NULL );
		vk_postfx_draw_fullscreen_quad();
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
					offsets[offset_count++] = vk.cmd->descriptor_set.offset[VK_DESC_UNIFORM_GLTF_TOPO_BINDING];

					qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout, i, 1, &vk.cmd->descriptor_set.current[i], offset_count, offsets );
				}
				else
					qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout, i, 1, &vk.cmd->descriptor_set.current[i], 0, NULL );
			}
		}
	}

	/* Bloom writes back to color_image; refresh post chain source and re-AA when enabled. */
	if ( r_bloom && r_bloom->integer && vk.color_image_view != VK_NULL_HANDLE ) {
		vk_pass_diag_stage( "bloom_refresh_post" );
		vk_barrier_post_fog_source_for_sampling( vk.color_image_view, "post-bloom refresh post-fog source" );
		vk_set_scene_post_fog_source( vk.color_image_view );
		vk_update_post_fog_descriptors( vk.color_image_view );
		vk_set_post_chain_last_writer( "bloom" );
		if ( r_postAaAfterBloom && r_postAaAfterBloom->integer && vk_post_aa_output_active() ) {
			vk_post_scene_aa_apply();
		}
	}

	backEnd.doneBloom = qtrue;
	if ( Q_stricmp( vk_post_chain_last_writer_name(), "post_aa" ) ) {
		vk_set_post_chain_last_writer( "bloom" );
	}
	vk_pass_diag_stage( "bloom_exit" );
	if ( r_fboDebug && r_fboDebug->integer >= 1 && vk_post_fog_fbo_debug_throttle() ) {
		ri.Printf( PRINT_DEVELOPER,
			"[VK][bloom] exit: postFog=%s activePass=%s continuation=%s\n",
			vk_post_fog_source_name( vk_get_post_fog_source() ),
			vk.passDiag.lastBegunPass[0] ? vk.passDiag.lastBegunPass : "(none)",
			vk.passDiag.inContinuationPass ? "yes" : "no" );
	}

	return qtrue;
}
