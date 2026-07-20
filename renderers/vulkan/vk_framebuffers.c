/*
===========================================================================
VkFramebuffer creation and destruction (main, gamma, postfx, shadows, etc.).
Extracted from vk.c for modularization.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_postfx.h"
#include "vk_framebuffers.h"

static uint32_t vk_fullres_framebuffer_width( void )
{
	if ( vk.mainColorWidth > 0u ) {
		return vk.mainColorWidth;
	}
	if ( glConfig.vidWidth > 0 ) {
		return (uint32_t)glConfig.vidWidth;
	}
	if ( gls.windowWidth > 0 ) {
		return (uint32_t)gls.windowWidth;
	}
	return 1u;
}

static uint32_t vk_fullres_framebuffer_height( void )
{
	if ( vk.mainColorHeight > 0u ) {
		return vk.mainColorHeight;
	}
	if ( glConfig.vidHeight > 0 ) {
		return (uint32_t)glConfig.vidHeight;
	}
	if ( gls.windowHeight > 0 ) {
		return (uint32_t)gls.windowHeight;
	}
	return 1u;
}

void vk_create_framebuffers( void )
{
	VkImageView framebuffer_attachments[7];
	VkFramebufferCreateInfo desc;
	uint32_t n;
	const uint32_t fullresWidth = vk_fullres_framebuffer_width();
	const uint32_t fullresHeight = vk_fullres_framebuffer_height();

	desc.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.pAttachments = framebuffer_attachments;
	desc.layers = 1;

	for ( n = 0; (uint32_t) n < vk.swapchain_image_count; n++ )
	{
		desc.renderPass = vk.render_pass.main;
		desc.attachmentCount = 2;
		if ( !vk.fboActive )
		{
			desc.width = gls.windowWidth;
			desc.height = gls.windowHeight;
			framebuffer_attachments[0] = vk.swapchain_image_views[n];
			framebuffer_attachments[1] = vk.depth_image_view;
			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.main[n] ) );

			SET_OBJECT_NAME( vk.framebuffers.main[n], va( "framebuffer - main %i", n ), VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
		}
		else
		{
			// same framebuffer configuration for main and post-bloom render passes
			if ( n == 0 )
			{
				desc.width = fullresWidth;
				desc.height = fullresHeight;
				framebuffer_attachments[0] = vk.color_image_view;
				framebuffer_attachments[1] = vk.depth_image_view;
				framebuffer_attachments[2] = vk.motion_vector_view;
				desc.attachmentCount = 3;
				if ( vk.deferredGbufferDirectExport )
				{
					desc.attachmentCount = 5;
					framebuffer_attachments[3] = vk.deferred_gbuffer_normal_view;
					framebuffer_attachments[4] = vk.deferred_gbuffer_material_view;
					if ( vk.visibilityBufferDirectExport &&
						vk.visibility_buffer_ids_view != VK_NULL_HANDLE &&
						vk.visibility_buffer_bary_view != VK_NULL_HANDLE )
					{
						desc.attachmentCount = 7;
						framebuffer_attachments[5] = vk.visibility_buffer_ids_view;
						framebuffer_attachments[6] = vk.visibility_buffer_bary_view;
					}
				}
				if ( vk.msaaActive )
				{
					desc.attachmentCount = 5;
					framebuffer_attachments[3] = vk.msaa_image_view;
					framebuffer_attachments[4] = vk.motion_vector_msaa_view;
				}
				VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.main[n] ) );
				SET_OBJECT_NAME( vk.framebuffers.main[n], "framebuffer - main", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
			}
			else
			{
				vk.framebuffers.main[n] = vk.framebuffers.main[0];
			}
			if ( vk.render_pass.atmosphere != VK_NULL_HANDLE && n == 0 ) {
				desc.renderPass = vk.render_pass.atmosphere;
				desc.attachmentCount = 2;
				desc.width = fullresWidth;
				desc.height = fullresHeight;
				framebuffer_attachments[0] = vk.msaaActive ? vk.msaa_image_view : vk.color_image_view;
				framebuffer_attachments[1] = vk.depth_image_view;
				VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.atmosphere[0] ) );
				SET_OBJECT_NAME( vk.framebuffers.atmosphere[0], "framebuffer - atmosphere", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
			}
			if ( vk.render_pass.atmosphere != VK_NULL_HANDLE ) {
				vk.framebuffers.atmosphere[n] = vk.framebuffers.atmosphere[0];
			} else {
				vk.framebuffers.atmosphere[n] = VK_NULL_HANDLE;
			}

			// gamma correction: use swapchain extent so framebuffer matches swapchain image dimensions
			desc.renderPass = vk.render_pass.gamma;
			desc.attachmentCount = 1;
			desc.width = vk.swapchain_extent_valid ? vk.swapchain_extent.width : (uint32_t)gls.windowWidth;
			desc.height = vk.swapchain_extent_valid ? vk.swapchain_extent.height : (uint32_t)gls.windowHeight;
			if ( desc.width == 0 ) desc.width = (uint32_t)glConfig.vidWidth;
			if ( desc.height == 0 ) desc.height = (uint32_t)glConfig.vidHeight;
			framebuffer_attachments[0] = vk.swapchain_image_views[n];
			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.gamma[n] ) );

			SET_OBJECT_NAME( vk.framebuffers.gamma[n], "framebuffer - gamma-correction", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );

			desc.renderPass = vk.render_pass.overlay_compose;
			desc.attachmentCount = 1;
			framebuffer_attachments[0] = vk.swapchain_image_views[n];
			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.overlay_compose[n] ) );
			SET_OBJECT_NAME( vk.framebuffers.overlay_compose[n], "framebuffer - overlay compose", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
		}
	}

	if ( vk.fboActive )
	{
		if ( vk.render_pass.ui_overlay != VK_NULL_HANDLE && vk.ui_overlay_image_view != VK_NULL_HANDLE ) {
			desc.renderPass = vk.render_pass.ui_overlay;
			desc.width = fullresWidth;
			desc.height = fullresHeight;
			desc.attachmentCount = 3;
			framebuffer_attachments[0] = vk.ui_overlay_image_view;
			framebuffer_attachments[1] = vk.depth_image_view;
			framebuffer_attachments[2] = vk.motion_vector_view;
			if ( vk.msaaActive ) {
				desc.attachmentCount = 5;
				framebuffer_attachments[3] = vk.ui_overlay_msaa_image_view;
				framebuffer_attachments[4] = vk.motion_vector_msaa_view;
			}
			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.ui_overlay[0] ) );
			SET_OBJECT_NAME( vk.framebuffers.ui_overlay[0], "framebuffer - ui overlay", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
			for ( n = 1; (uint32_t)n < vk.swapchain_image_count; n++ ) {
				vk.framebuffers.ui_overlay[n] = vk.framebuffers.ui_overlay[0];
			}
		}

		// screenmap
		desc.renderPass = vk.render_pass.screenmap;
		desc.attachmentCount = 3;
		desc.width = vk.screenMapWidth;
		desc.height = vk.screenMapHeight;
		framebuffer_attachments[0] = vk.screenMap.color_image_view;
		framebuffer_attachments[1] = vk.screenMap.depth_image_view;
		framebuffer_attachments[2] = vk.screenMap.motion_image_view;
		if ( vk.screenMapSamples > VK_SAMPLE_COUNT_1_BIT )
		{
			desc.attachmentCount = 5;
			framebuffer_attachments[2] = vk.screenMap.motion_image_view;
			framebuffer_attachments[3] = vk.screenMap.color_image_view_msaa;
			framebuffer_attachments[4] = vk.screenMap.motion_image_view_msaa;
		}
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.screenmap ) );
		SET_OBJECT_NAME( vk.framebuffers.screenmap, "framebuffer - screenmap", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );

		// sun shadow map framebuffer (single-cascade, resolved depth path)
		if ( vk.sun_shadow_image != VK_NULL_HANDLE && vk.sun_shadow_color_image != VK_NULL_HANDLE ) {
			desc.renderPass = vk.render_pass.sun_shadow;
			desc.attachmentCount = 2;
			desc.width = vk.sun_shadow_width;
			desc.height = vk.sun_shadow_height;
			framebuffer_attachments[0] = vk.sun_shadow_color_view;
			framebuffer_attachments[1] = vk.sun_shadow_view;
			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.sun_shadow ) );
			SET_OBJECT_NAME( vk.framebuffers.sun_shadow, "framebuffer - sun shadow", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
		}

		if ( vk.local_spot_shadow_atlas_image != VK_NULL_HANDLE &&
			vk.local_spot_shadow_color_image != VK_NULL_HANDLE &&
			vk.render_pass.local_spot_shadow != VK_NULL_HANDLE )
		{
			desc.renderPass = vk.render_pass.local_spot_shadow;
			desc.attachmentCount = 2;
			desc.width = vk.local_spot_shadow_atlas_size;
			desc.height = vk.local_spot_shadow_atlas_size;
			framebuffer_attachments[0] = vk.local_spot_shadow_color_view;
			framebuffer_attachments[1] = vk.local_spot_shadow_atlas_view;
			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.local_spot_shadow ) );
			SET_OBJECT_NAME( vk.framebuffers.local_spot_shadow, "framebuffer - local spot shadow", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
		}

		if ( vk.local_point_shadow_array_image != VK_NULL_HANDLE &&
			vk.local_point_shadow_color_array_image != VK_NULL_HANDLE &&
			vk.render_pass.local_point_shadow != VK_NULL_HANDLE )
		{
			const uint32_t point_layers = vk.local_point_shadow_capacity * 6;
			desc.renderPass = vk.render_pass.local_point_shadow;
			desc.attachmentCount = 2;
			desc.width = vk.local_point_shadow_face_size;
			desc.height = vk.local_point_shadow_face_size;

			for ( uint32_t layer = 0; layer < point_layers && layer < ARRAY_LEN( vk.framebuffers.local_point_shadow ); layer++ ) {
				framebuffer_attachments[0] = vk.local_point_shadow_color_face_views[layer];
				framebuffer_attachments[1] = vk.local_point_shadow_face_views[layer];
				VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.local_point_shadow[layer] ) );
			}
		}

	#ifdef VK_CUBEMAP
	if ( vk.cubemapActive )
	{
		// cubemap
		desc.renderPass = vk.render_pass.cubemap;
		desc.attachmentCount = 2;
		desc.width = REF_CUBEMAP_SIZE;
		desc.height = REF_CUBEMAP_SIZE;

		framebuffer_attachments[1] = vk.cubeMap.depth_image_view;

		if ( vk.msaaActive )
			desc.attachmentCount = 3;

		for ( int j = 0; j < 6; j++  )
		{
			framebuffer_attachments[0] = vk.cubeMap.color_image_view[j+1];

			if ( vk.msaaActive ) {
				framebuffer_attachments[2] = vk.cubeMap.color_image_view_msaa[0];
			}

			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.cubemap[j] ) );
			SET_OBJECT_NAME( vk.framebuffers.cubemap[j], va( "framebuffer - cubemap face %d", j ), VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
		}
	}
	#endif

	if ( vk.capture.image != VK_NULL_HANDLE )
	{
		framebuffer_attachments[0] = vk.capture.image_view;

		desc.renderPass = vk.render_pass.capture;
		desc.pAttachments = framebuffer_attachments;
		desc.attachmentCount = 1;
		desc.width = gls.captureWidth;
		desc.height = gls.captureHeight;

		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.capture ) );
		SET_OBJECT_NAME( vk.framebuffers.capture, "framebuffer - capture", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
	}

	if ( vk.smaaActive || vk.fxaaActive )
	{
		desc.width = fullresWidth;
		desc.height = fullresHeight;
		desc.attachmentCount = 1;

		if ( vk.smaaActive ) {
			desc.renderPass = vk.render_pass.smaa_edge;
			framebuffer_attachments[0] = vk.smaa_edge_image_view;
			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.smaa_edge ) );
			SET_OBJECT_NAME( vk.framebuffers.smaa_edge, "framebuffer - smaa edge", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );

			desc.renderPass = vk.render_pass.smaa_blend;
			framebuffer_attachments[0] = vk.smaa_blend_image_view;
			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.smaa_blend ) );
			SET_OBJECT_NAME( vk.framebuffers.smaa_blend, "framebuffer - smaa blend", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
		}

		desc.renderPass = vk.render_pass.smaa_compose;
		framebuffer_attachments[0] = vk.smaa_output_image_view;
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.smaa_compose ) );
		SET_OBJECT_NAME( vk.framebuffers.smaa_compose, vk.smaaActive ? "framebuffer - smaa compose" : "framebuffer - fxaa",
			VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
	}

	/* TAA history FBs: independent of SMAA so r_taa / r_upscale 2 work without r_ext_smaa. */
	if ( vk.render_pass.taa != VK_NULL_HANDLE &&
		vk.taa_history_image_view[0] != VK_NULL_HANDLE &&
		vk.taa_history_image_view[1] != VK_NULL_HANDLE ) {
		desc.width = fullresWidth;
		desc.height = fullresHeight;
		desc.attachmentCount = 1;
		desc.renderPass = vk.render_pass.taa;
		framebuffer_attachments[0] = vk.taa_history_image_view[0];
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.taa[0] ) );
		SET_OBJECT_NAME( vk.framebuffers.taa[0], "framebuffer - taa history 0", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
		framebuffer_attachments[0] = vk.taa_history_image_view[1];
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.taa[1] ) );
		SET_OBJECT_NAME( vk.framebuffers.taa[1], "framebuffer - taa history 1", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
	}

	if ( r_bloom->integer )
	{
		uint32_t width = gls.captureWidth;
		uint32_t height = gls.captureHeight;

		// bloom color extraction
		desc.renderPass = vk.render_pass.bloom_extract;
		desc.width = width;
		desc.height = height;

		desc.attachmentCount = 1;
		framebuffer_attachments[0] = vk.bloom_image_view[0];

		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.bloom_extract ) );

		SET_OBJECT_NAME( vk.framebuffers.bloom_extract, "framebuffer - bloom extraction", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );

		for ( n = 0; n < ARRAY_LEN( vk.framebuffers.blur ); n += 2 )
		{
			width /= 2;
			height /= 2;

			desc.renderPass = vk.render_pass.blur[n];
			desc.width = width;
			desc.height = height;

			desc.attachmentCount = 1;

			framebuffer_attachments[0] = vk.bloom_image_view[n+0+1];
			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.blur[n+0] ) );

			framebuffer_attachments[0] = vk.bloom_image_view[n+1+1];
			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.blur[n+1] ) );

			SET_OBJECT_NAME( vk.framebuffers.blur[n+0], va( "framebuffer - blur %i", n+0 ), VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
			SET_OBJECT_NAME( vk.framebuffers.blur[n+1], va( "framebuffer - blur %i", n+1 ), VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
		}
	}

	if ( r_ssao && r_ssao->integer )
	{
		// ssao
		desc.renderPass = vk.render_pass.ssao;
		desc.attachmentCount = 1;
		desc.width = fullresWidth;
		desc.height = fullresHeight;
		framebuffer_attachments[0] = vk.ssao_image_view;
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.ssao ) );
		SET_OBJECT_NAME( vk.framebuffers.ssao, "framebuffer - ssao", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );

		desc.renderPass = vk.render_pass.ssao_blur;
		framebuffer_attachments[0] = vk.ssao_blur_image_view;
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.ssao_blur ) );
		SET_OBJECT_NAME( vk.framebuffers.ssao_blur, "framebuffer - ssao_blur", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );

		desc.renderPass = vk.render_pass.ssao_combine;
		framebuffer_attachments[0] = vk.color_image_view;
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.ssao_combine ) );
		SET_OBJECT_NAME( vk.framebuffers.ssao_combine, "framebuffer - ssao_combine", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
	}

	if ( r_oit && r_oit->integer && vk.render_pass.oit_accum != VK_NULL_HANDLE &&
		vk.oit_accum_image_view && vk.oit_reveal_image_view )
	{
		desc.renderPass = vk.render_pass.oit_accum;
		desc.attachmentCount = ( vk_get_main_rasterization_samples() == VK_SAMPLE_COUNT_1_BIT && vk.depth_image_view ) ? 3 : 2;
		desc.width = fullresWidth;
		desc.height = fullresHeight;
		framebuffer_attachments[0] = vk.oit_accum_image_view;
		framebuffer_attachments[1] = vk.oit_reveal_image_view;
		if ( desc.attachmentCount == 3 )
			framebuffer_attachments[2] = vk.depth_image_view;
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.oit_accum ) );
		SET_OBJECT_NAME( vk.framebuffers.oit_accum, "framebuffer - oit_accum", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );

		if ( r_oit->integer == 2 && vk.render_pass.oit_moments != VK_NULL_HANDLE &&
			vk.oit_moments_image_view && vk.oit_b0_image_view )
		{
			desc.renderPass = vk.render_pass.oit_moments;
			desc.attachmentCount = ( vk_get_main_rasterization_samples() == VK_SAMPLE_COUNT_1_BIT && vk.depth_image_view ) ? 3 : 2;
			framebuffer_attachments[0] = vk.oit_moments_image_view;
			framebuffer_attachments[1] = vk.oit_b0_image_view;
			if ( desc.attachmentCount == 3 )
				framebuffer_attachments[2] = vk.depth_image_view;
			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.oit_moments ) );
			SET_OBJECT_NAME( vk.framebuffers.oit_moments, "framebuffer - oit_moments", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
		}

		/* Resolve RP has a single color attachment — reset count left at 2/3 by accum/moments. */
		desc.renderPass = vk.render_pass.oit_resolve;
		desc.attachmentCount = 1;
		desc.width = fullresWidth;
		desc.height = fullresHeight;
		framebuffer_attachments[0] = vk.color_image_view;
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.oit_resolve ) );
		SET_OBJECT_NAME( vk.framebuffers.oit_resolve, "framebuffer - oit_resolve", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
		/* Single authoritative bump after images + all OIT FBs exist. */
		vk.oitAttachmentGeneration++;
		vk.oitUnhealthy = qfalse;
	}

	if ( vk.render_pass.reactive_stamp != VK_NULL_HANDLE && vk.reactive_mask_view != VK_NULL_HANDLE ) {
		desc.renderPass = vk.render_pass.reactive_stamp;
		desc.attachmentCount = 1;
		desc.width = fullresWidth;
		desc.height = fullresHeight;
		framebuffer_attachments[0] = vk.reactive_mask_view;
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.reactive_stamp ) );
		SET_OBJECT_NAME( vk.framebuffers.reactive_stamp, "framebuffer - reactive_stamp",
			VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
	}

	if ( PostFX_SSR_IsEnabled() && vk.render_pass.ssr != VK_NULL_HANDLE && vk.ssr_image_view )
	{
		desc.renderPass = vk.render_pass.ssr;
		desc.attachmentCount = 1;
		desc.width = fullresWidth;
		desc.height = fullresHeight;
		framebuffer_attachments[0] = vk.ssr_image_view;
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.ssr ) );
		SET_OBJECT_NAME( vk.framebuffers.ssr, "framebuffer - ssr", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
	}

	if ( vk.render_pass.volumetric != VK_NULL_HANDLE ) {
		desc.renderPass = vk.render_pass.volumetric;
		desc.attachmentCount = 1;
		desc.width = fullresWidth;
		desc.height = fullresHeight;
		framebuffer_attachments[0] = vk.color_image_view;
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.volumetric[0] ) );
		SET_OBJECT_NAME( vk.framebuffers.volumetric[0], "framebuffer - volumetric fog", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
		for ( n = 1; (uint32_t)n < vk.swapchain_image_count; n++ ) {
			vk.framebuffers.volumetric[n] = vk.framebuffers.volumetric[0];
		}
	}

	#ifdef VK_PBR_BRDFLUT
	if( vk.pbrActive )
	{
		desc.renderPass = vk.render_pass.brdflut;
		desc.width = desc.height = 512;
		desc.attachmentCount = 1;
		framebuffer_attachments[0] = vk.brdflut_image_view;
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.brdflut ) );
		SET_OBJECT_NAME( vk.framebuffers.brdflut, va( "framebuffer - brdf LUT" ), VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
	}
	#endif
	}
}


void vk_destroy_framebuffers( void ) {
	uint32_t n;

	for ( n = 0; n < vk.swapchain_image_count; n++ ) {
		if ( vk.framebuffers.main[n] != VK_NULL_HANDLE ) {
			if ( !vk.fboActive || n == 0 ) {
				qvkDestroyFramebuffer( vk.device, vk.framebuffers.main[n], NULL );
			}
			vk.framebuffers.main[n] = VK_NULL_HANDLE;
		}
		if ( vk.framebuffers.atmosphere[n] != VK_NULL_HANDLE ) {
			if ( n == 0 ) {
				qvkDestroyFramebuffer( vk.device, vk.framebuffers.atmosphere[n], NULL );
			}
			vk.framebuffers.atmosphere[n] = VK_NULL_HANDLE;
		}
		if ( vk.framebuffers.gamma[n] != VK_NULL_HANDLE ) {
			qvkDestroyFramebuffer( vk.device, vk.framebuffers.gamma[n], NULL );
			vk.framebuffers.gamma[n] = VK_NULL_HANDLE;
		}
	}

	if ( vk.framebuffers.bloom_extract != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.bloom_extract, NULL );
		vk.framebuffers.bloom_extract = VK_NULL_HANDLE;
	}

	if ( vk.framebuffers.ssao != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.ssao, NULL );
		vk.framebuffers.ssao = VK_NULL_HANDLE;
	}

	if ( vk.framebuffers.ssao_blur != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.ssao_blur, NULL );
		vk.framebuffers.ssao_blur = VK_NULL_HANDLE;
	}

	if ( vk.framebuffers.ssao_combine != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.ssao_combine, NULL );
		vk.framebuffers.ssao_combine = VK_NULL_HANDLE;
	}
	if ( vk.framebuffers.oit_accum != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.oit_accum, NULL );
		vk.framebuffers.oit_accum = VK_NULL_HANDLE;
	}
	if ( vk.framebuffers.oit_moments != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.oit_moments, NULL );
		vk.framebuffers.oit_moments = VK_NULL_HANDLE;
	}
	if ( vk.framebuffers.oit_resolve != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.oit_resolve, NULL );
		vk.framebuffers.oit_resolve = VK_NULL_HANDLE;
	}
	if ( vk.framebuffers.reactive_stamp != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.reactive_stamp, NULL );
		vk.framebuffers.reactive_stamp = VK_NULL_HANDLE;
	}

	if ( vk.framebuffers.ssr != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.ssr, NULL );
		vk.framebuffers.ssr = VK_NULL_HANDLE;
	}

	if ( vk.framebuffers.volumetric[0] != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.volumetric[0], NULL );
		for ( n = 0; (uint32_t)n < vk.swapchain_image_count; n++ ) {
			vk.framebuffers.volumetric[n] = VK_NULL_HANDLE;
		}
	}

	if ( vk.framebuffers.screenmap != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.screenmap, NULL );
		vk.framebuffers.screenmap = VK_NULL_HANDLE;
	}
	if ( vk.framebuffers.sun_shadow != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.sun_shadow, NULL );
		vk.framebuffers.sun_shadow = VK_NULL_HANDLE;
	}
	if ( vk.framebuffers.local_spot_shadow != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.local_spot_shadow, NULL );
		vk.framebuffers.local_spot_shadow = VK_NULL_HANDLE;
	}
	for ( n = 0; n < ARRAY_LEN( vk.framebuffers.local_point_shadow ); n++ ) {
		if ( vk.framebuffers.local_point_shadow[n] != VK_NULL_HANDLE ) {
			qvkDestroyFramebuffer( vk.device, vk.framebuffers.local_point_shadow[n], NULL );
			vk.framebuffers.local_point_shadow[n] = VK_NULL_HANDLE;
		}
	}

	if ( vk.framebuffers.capture != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.capture, NULL );
		vk.framebuffers.capture = VK_NULL_HANDLE;
	}
	if ( vk.framebuffers.ui_overlay[0] != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.ui_overlay[0], NULL );
		for ( n = 0; (uint32_t)n < vk.swapchain_image_count; n++ ) {
			vk.framebuffers.ui_overlay[n] = VK_NULL_HANDLE;
		}
	}

	if ( vk.framebuffers.smaa_edge != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.smaa_edge, NULL );
		vk.framebuffers.smaa_edge = VK_NULL_HANDLE;
	}

	if ( vk.framebuffers.smaa_blend != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.smaa_blend, NULL );
		vk.framebuffers.smaa_blend = VK_NULL_HANDLE;
	}

	if ( vk.framebuffers.smaa_compose != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.smaa_compose, NULL );
		vk.framebuffers.smaa_compose = VK_NULL_HANDLE;
	}
	for ( n = 0; n < 2; n++ ) {
		if ( vk.framebuffers.taa[n] != VK_NULL_HANDLE ) {
			qvkDestroyFramebuffer( vk.device, vk.framebuffers.taa[n], NULL );
			vk.framebuffers.taa[n] = VK_NULL_HANDLE;
		}
	}

	for ( n = 0; n < ARRAY_LEN( vk.framebuffers.blur ); n++ ) {
		if ( vk.framebuffers.blur[n] != VK_NULL_HANDLE ) {
			qvkDestroyFramebuffer( vk.device, vk.framebuffers.blur[n], NULL );
			vk.framebuffers.blur[n] = VK_NULL_HANDLE;
		}
	}

#ifdef VK_PBR_BRDFLUT
    if ( vk.framebuffers.brdflut != VK_NULL_HANDLE ) {
        qvkDestroyFramebuffer( vk.device, vk.framebuffers.brdflut, NULL );
        vk.framebuffers.brdflut = VK_NULL_HANDLE;
    }
#endif

#ifdef VK_CUBEMAP
    for ( n = 0; n < ARRAY_LEN( vk.framebuffers.cubemap ); n++ ) {
        if ( vk.framebuffers.cubemap[n] != VK_NULL_HANDLE ) {
            qvkDestroyFramebuffer( vk.device, vk.framebuffers.cubemap[n], NULL );
            vk.framebuffers.cubemap[n] = VK_NULL_HANDLE;
        }
    }
#endif
	for ( n = 0; n < ARRAY_LEN( vk.framebuffers.overlay_compose ); n++ ) {
		if ( vk.framebuffers.overlay_compose[n] != VK_NULL_HANDLE ) {
			qvkDestroyFramebuffer( vk.device, vk.framebuffers.overlay_compose[n], NULL );
			vk.framebuffers.overlay_compose[n] = VK_NULL_HANDLE;
		}
	}
}
