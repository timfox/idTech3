#include "tr_local.h"
#include "vk.h"
#include "vk_postfx.h"
#include "vk_render_pass.h"
#include "vk_validation.h"

void vk_set_fullscreen_viewport_scissor( uint32_t width, uint32_t height )
{
	VkViewport viewport;
	VkRect2D scissor;

	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)width;
	viewport.height = (float)height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = width;
	scissor.extent.height = height;

	qvkCmdSetViewport( vk.cmd->command_buffer, 0, 1, &viewport );
	qvkCmdSetScissor( vk.cmd->command_buffer, 0, 1, &scissor );
}

void vk_begin_render_pass_tracked( VkRenderPass renderPass, VkFramebuffer frameBuffer, qboolean clearValues, uint32_t width, uint32_t height )
{
	VkRenderPassBeginInfo render_pass_begin_info;
	VkClearValue clear_values[5];

	if ( width == 0 ) {
		width = 1u;
	}
	if ( height == 0 ) {
		height = 1u;
	}

	render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_begin_info.pNext = NULL;
	render_pass_begin_info.renderPass = renderPass;
	render_pass_begin_info.framebuffer = frameBuffer;
	render_pass_begin_info.renderArea.offset.x = 0;
	render_pass_begin_info.renderArea.offset.y = 0;
	render_pass_begin_info.renderArea.extent.width = width;
	render_pass_begin_info.renderArea.extent.height = height;

	if ( clearValues ) {
		uint32_t clear_count = 2;

		Com_Memset( clear_values, 0, sizeof( clear_values ) );
		clear_values[0].color.float32[0] = 0.0f;
		clear_values[0].color.float32[1] = 0.0f;
		clear_values[0].color.float32[2] = 0.0f;
		clear_values[0].color.float32[3] = 1.0f;
		if ( vk.renderPassIndex == RENDER_PASS_UI_OVERLAY ) {
			clear_values[0].color.float32[3] = 0.0f;
		}
#ifndef USE_REVERSED_DEPTH
		clear_values[1].depthStencil.depth = 1.0;
#endif
		if ( vk.renderPassIndex == RENDER_PASS_MAIN || vk.renderPassIndex == RENDER_PASS_POST_BLOOM ||
			vk.renderPassIndex == RENDER_PASS_UI_OVERLAY ) {
			if ( vk.fboActive ) {
				clear_values[2].color.float32[0] = 0.0f;
				clear_values[2].color.float32[1] = 0.0f;
				clear_values[2].color.float32[2] = 0.0f;
				clear_values[2].color.float32[3] = 0.0f;
				clear_count = vk.msaaActive ? 5 : 3;
			} else {
				clear_count = vk.msaaActive ? 3 : 2;
			}
		} else if ( vk.renderPassIndex == RENDER_PASS_SCREENMAP ) {
			clear_values[2].color.float32[0] = 0.0f;
			clear_values[2].color.float32[1] = 0.0f;
			clear_values[2].color.float32[2] = 0.0f;
			clear_values[2].color.float32[3] = 0.0f;
			clear_count = ( vk.screenMapSamples > VK_SAMPLE_COUNT_1_BIT ) ? 5 : 3;
		} else if ( vk.renderPassIndex == RENDER_PASS_SUN_SHADOW ) {
			clear_count = 2;
		} else {
			clear_count = vk.msaaActive ? 3 : 2;
		}
		if ( renderPass == vk.render_pass.oit_accum ) {
			clear_values[0].color.float32[0] = 0.0f;
			clear_values[0].color.float32[1] = 0.0f;
			clear_values[0].color.float32[2] = 0.0f;
			clear_values[0].color.float32[3] = 0.0f;
			clear_values[1].color.float32[0] = 1.0f;
			clear_values[1].color.float32[1] = 1.0f;
			clear_values[1].color.float32[2] = 1.0f;
			clear_values[1].color.float32[3] = 1.0f;
#ifndef USE_REVERSED_DEPTH
			clear_values[2].depthStencil.depth = 1.0f;
#endif
			clear_count = vk.msaaActive ? 2 : 3;
		}
		render_pass_begin_info.clearValueCount = clear_count;
		render_pass_begin_info.pClearValues = clear_values;

		vk_world.dirty_depth_attachment = 0;
	} else {
		render_pass_begin_info.clearValueCount = 0;
		render_pass_begin_info.pClearValues = NULL;
	}

	qvkCmdBeginRenderPass( vk.cmd->command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE );
	vk.inRenderPass = qtrue;

	vk.cmd->last_pipeline = VK_NULL_HANDLE;
	vk.cmd->depth_range = DEPTH_RANGE_COUNT;
}

void vk_end_render_pass_tracked( void )
{
	if ( !vk.inRenderPass ) {
		return;
	}
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		vk.inRenderPass = qfalse;
		return;
	}

	qvkCmdEndRenderPass( vk.cmd->command_buffer );
	vk.inRenderPass = qfalse;
}
void vk_create_render_passes( void )
{
	VkSampleCountFlagBits vkSamples = vk_get_main_rasterization_samples();
	VkAttachmentDescription attachments[5]; // color resolve | depth | motion resolve | msaa color | msaa motion
	VkAttachmentReference colorResolveRefs[2];
	VkAttachmentReference colorResolveRef;
	VkAttachmentReference colorRefs[2];
	VkAttachmentReference colorRef0;
	VkAttachmentReference depthRef0;
	VkSubpassDescription subpass;
	VkSubpassDependency deps[3];
	VkRenderPassCreateInfo desc;
	VkFormat depth_format;
	VkDevice device;
	const qboolean fboActive = vk.fboActive;
	uint32_t i;

	depth_format = vk.depth_format;
	device = vk.device;

	if ( !fboActive )
	{
		// presentation
		attachments[0].flags = 0;
		attachments[0].format = vk.present_format.format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
#ifdef USE_BUFFER_CLEAR
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
#else
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;	// Assuming this will be completely overwritten
#endif
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;		// needed for presentation
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = vk.initSwapchainLayout;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	}
	else
	{
		// resolve/color buffer
		attachments[0].flags = 0;
		attachments[0].format = vk.color_format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;

		/* Always clear FBO color to avoid solid/wrong colors from uninitialized or stale content
		 * (fixes r_fbo 1 solid rapidly-changing color bug). */
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;

		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;   // needed for next render pass
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}

	// depth buffer
	attachments[1].flags = 0;
	attachments[1].format = depth_format;
	attachments[1].samples = vkSamples;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Need empty depth buffer before use
	attachments[1].stencilLoadOp = glConfig.stencilBits ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[1].stencilStoreOp = glConfig.stencilBits ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	colorRefs[0].attachment = 0;
	colorRefs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorRefs[1].attachment = VK_ATTACHMENT_UNUSED;
	colorRefs[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	depthRef0.attachment = 1;
	depthRef0.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	Com_Memset( colorResolveRefs, 0, sizeof( colorResolveRefs ) );
	colorResolveRefs[0].attachment = VK_ATTACHMENT_UNUSED;
	colorResolveRefs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorResolveRefs[1].attachment = VK_ATTACHMENT_UNUSED;
	colorResolveRefs[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	if ( fboActive ) {
		// velocity buffer used for per-pixel reprojection.
		attachments[2].flags = 0;
		attachments[2].format = VK_FORMAT_R16G16_SFLOAT;
		attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[2].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		attachments[2].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		colorRefs[1].attachment = 2;
	}

	Com_Memset( &subpass, 0, sizeof( subpass ) );
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = fboActive ? 2 : 1;
	subpass.pColorAttachments = colorRefs;
	subpass.pDepthStencilAttachment = &depthRef0;
	subpass.pResolveAttachments = NULL;

	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.pAttachments = attachments;
	desc.pSubpasses = &subpass;

	desc.subpassCount = 1;
	desc.attachmentCount = fboActive ? 3 : 2;

	if ( vk.msaaActive )
	{
		attachments[3].flags = 0;
		attachments[3].format = vk.color_format;
		attachments[3].samples = vkSamples;
#ifdef USE_BUFFER_CLEAR
		attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
#else
		attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
#endif
		attachments[3].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[3].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[3].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[3].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachments[3].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		if ( fboActive ) {
			attachments[4].flags = 0;
			attachments[4].format = VK_FORMAT_R16G16_SFLOAT;
			attachments[4].samples = vkSamples;
#ifdef USE_BUFFER_CLEAR
			attachments[4].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
#else
			attachments[4].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
#endif
			attachments[4].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[4].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[4].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[4].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			attachments[4].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			desc.attachmentCount = 5;

			colorRefs[0].attachment = 3; // msaa scene color attachment
			colorRefs[1].attachment = 4; // msaa motion attachment

			colorResolveRefs[0].attachment = 0; // scene resolve
			colorResolveRefs[1].attachment = 2; // motion resolve
			subpass.pResolveAttachments = colorResolveRefs;
		} else {
			desc.attachmentCount = 3;

			colorRefs[0].attachment = 2; // msaa image attachment
			colorResolveRefs[0].attachment = 0; // resolve image attachment
			subpass.pResolveAttachments = &colorResolveRefs[0];
		}
	}

	// subpass dependencies

	Com_Memset( &deps, 0, sizeof( deps ) );

	deps[2].srcSubpass = VK_SUBPASS_EXTERNAL;
	deps[2].dstSubpass = 0;
	deps[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;	// What pipeline stage is waiting on the dependency
	deps[2].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;	// What pipeline stage is waiting on the dependency
	deps[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;			// What access scopes are influence the dependency
	deps[2].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;			// What access scopes are waiting on the dependency
	deps[2].dependencyFlags = 0;

	if ( !fboActive )
	{
		desc.dependencyCount = 1;
		desc.pDependencies = &deps[2];

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.main ) );
		SET_OBJECT_NAME( vk.render_pass.main, "render pass - main", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

		return;
	}

	desc.dependencyCount = 2;
	desc.pDependencies = &deps[0];

	deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	deps[0].dstSubpass = 0;
	deps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;			// What pipeline stage must have completed for the dependency
	deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;	// What pipeline stage is waiting on the dependency
	deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;						// What access scopes are influence the dependency
	deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; // What access scopes are waiting on the dependency
	deps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;					// Only need the current fragment (or tile) synchronized, not the whole framebuffer

	deps[1].srcSubpass = 0;
	deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;	// Fragment data has been written
	deps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;			// Don't start shading until data is available
	deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;			// Waiting for color data to be written
	deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;						// Don't read things from the shader before ready
	deps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;					// Only need the current fragment (or tile) synchronized, not the whole framebuffer

	VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.main ) );
	SET_OBJECT_NAME( vk.render_pass.main, "render pass - main", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

	// Post-main continuation pass used by 3D -> 2D split (volumetric fog runs here even when bloom is off).
	{

		// post-bloom pass
		// color buffer
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // load from previous pass
		if ( fboActive ) {
			attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		}
		 // depth buffer
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		if ( vk.msaaActive ) {
			// msaa render target
			attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			attachments[3].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			if ( fboActive ) {
				attachments[4].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
				attachments[4].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			}
		}
		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.post_bloom ) );
		SET_OBJECT_NAME( vk.render_pass.post_bloom, "render pass - post_bloom", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
	}

	if ( r_bloom->integer ) {
		// bloom extraction, using resolved/main fbo as a source
		desc.attachmentCount = 1;

		colorRef0.attachment = 0;
		colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		Com_Memset( &subpass, 0, sizeof( subpass ) );
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef0;

		attachments[0].flags = 0;
		attachments[0].format = vk.bloom_format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;	// Assuming this will be completely overwritten
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;		// needed for next render pass
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.bloom_extract ) );
		SET_OBJECT_NAME( vk.render_pass.bloom_extract, "render pass - bloom_extract", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

		for ( i = 0; i < ARRAY_LEN( vk.render_pass.blur ); i++ )
		{
			VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.blur[i] ) );
			SET_OBJECT_NAME( vk.render_pass.blur[i], va( "render pass - blur %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
		}
	}

	if ( r_ssao && r_ssao->integer && fboActive )
	{
		// ssao render pass
		desc.attachmentCount = 1;

		colorRef0.attachment = 0;
		colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		Com_Memset( &subpass, 0, sizeof( subpass ) );
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef0;

		attachments[0].flags = 0;
		attachments[0].format = vk.ssao_format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.ssao ) );
		SET_OBJECT_NAME( vk.render_pass.ssao, "render pass - ssao", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.ssao_blur ) );
		SET_OBJECT_NAME( vk.render_pass.ssao_blur, "render pass - ssao_blur", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

		// ssao combine pass (write back to main color)
		attachments[0].flags = 0;
		attachments[0].format = vk.color_format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.ssao_combine ) );
		SET_OBJECT_NAME( vk.render_pass.ssao_combine, "render pass - ssao_combine", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
	}

	if ( r_oit && r_oit->integer && fboActive )
	{
		/* OIT accumulation pass:
		 *  RT0 = weighted color accumulation
		 *  RT1 = revealage product
		 *  Depth attachment (when MSAA off): hardware depth-test transparents against opaque scene.
		 *  MSAA path uses resolved depth sampling in shader. */
		desc.attachmentCount = ( vkSamples == VK_SAMPLE_COUNT_1_BIT ) ? 3 : 2;
		colorRef0.attachment = 0;
		colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		colorRefs[0] = colorRef0;
		colorRefs[1].attachment = 1;
		colorRefs[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		Com_Memset( &subpass, 0, sizeof( subpass ) );
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 2;
		subpass.pColorAttachments = colorRefs;
		attachments[0].flags = 0;
		attachments[0].format = VK_FORMAT_R16G16B16A16_SFLOAT;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		attachments[1].flags = 0;
		attachments[1].format = VK_FORMAT_R16_SFLOAT;
		attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		attachments[1].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		if ( vkSamples == VK_SAMPLE_COUNT_1_BIT ) {
			attachments[2].flags = 0;
			attachments[2].format = depth_format;
			attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[2].stencilLoadOp = glConfig.stencilBits ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[2].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			attachments[2].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			depthRef0.attachment = 2;
			depthRef0.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			subpass.pDepthStencilAttachment = &depthRef0;
		} else {
			subpass.pDepthStencilAttachment = NULL;
		}
		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.oit_accum ) );
		SET_OBJECT_NAME( vk.render_pass.oit_accum, "render pass - oit_accum", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

		/* OIT resolve pass: composite opaque + accum to main color */
		desc.attachmentCount = 1;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef0;
		subpass.pDepthStencilAttachment = NULL;
		attachments[0].format = vk.color_format;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.oit_resolve ) );
		SET_OBJECT_NAME( vk.render_pass.oit_resolve, "render pass - oit_resolve", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
	}

	if ( PostFX_SSR_IsEnabled() && fboActive )
	{
		// ssr render pass (output to ssr_image, same format as color)
		desc.attachmentCount = 1;

		colorRef0.attachment = 0;
		colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		Com_Memset( &subpass, 0, sizeof( subpass ) );
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef0;

		attachments[0].flags = 0;
		attachments[0].format = vk.color_format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.ssr ) );
		SET_OBJECT_NAME( vk.render_pass.ssr, "render pass - ssr", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
	}

	if ( vk.fboActive )
	{
		Com_Memset( &subpass, 0, sizeof( subpass ) );
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef0;

		attachments[0].flags = 0;
		attachments[0].format = vk.color_format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		/* finalLayout SHADER_READ_ONLY so gamma/luminance can sample after vk_end_render_pass */
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.pAttachments = attachments;
		desc.attachmentCount = 1;
		desc.pSubpasses = &subpass;
		desc.subpassCount = 1;
		desc.dependencyCount = 0;
		desc.pDependencies = NULL;

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.volumetric ) );
		SET_OBJECT_NAME( vk.render_pass.volumetric, "render pass - volumetric fog", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
	}

	// capture render pass
	if ( vk.capture.image )
	{
		Com_Memset( &subpass, 0, sizeof( subpass ) );

		attachments[0].flags = 0;
		attachments[0].format = vk.capture_format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // this will be completely overwritten
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;   // needed for next render pass
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

		colorRef0.attachment = 0;
		colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef0;

		desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.pAttachments = attachments;
		desc.attachmentCount = 1;
		desc.pSubpasses = &subpass;
		desc.subpassCount = 1;

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.capture ) );
		SET_OBJECT_NAME( vk.render_pass.capture, "render pass - capture", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
	}

	colorRef0.attachment = 0;
	colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	desc.attachmentCount = 1;

	Com_Memset( &subpass, 0, sizeof( subpass ) );
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef0;

	// gamma post-processing
	attachments[0].flags = 0;
	attachments[0].format = vk.present_format.format;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE; // needed for presentation
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = vk.initSwapchainLayout;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	desc.dependencyCount = 1;
	desc.pDependencies = &deps[2];

	VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.gamma ) );
	SET_OBJECT_NAME( vk.render_pass.gamma, "render pass - gamma", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

	// LDR UI overlay composite on top of the tonemapped swapchain image.
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.overlay_compose ) );
	SET_OBJECT_NAME( vk.render_pass.overlay_compose, "render pass - overlay compose", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

	// Separate 2D/UI overlay target. Keep attachment compatibility with post_bloom so
	// existing 2D draw pipelines can render here while preserving alpha coverage.
	{
		VkAttachmentDescription ui_attachments[5];
		VkAttachmentReference ui_color_refs[2];
		VkAttachmentReference ui_depth_ref;
		VkAttachmentReference ui_resolve_refs[2];
		VkSubpassDescription ui_subpass;
		VkRenderPassCreateInfo ui_desc;

		Com_Memcpy( ui_attachments, attachments, sizeof( ui_attachments ) );
		ui_attachments[0].format = vk.color_format;
		ui_attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		ui_attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		ui_attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		ui_attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		ui_attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		ui_attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		ui_attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		ui_attachments[1].format = depth_format;
		ui_attachments[1].samples = vkSamples;
		ui_attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		ui_attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		ui_attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		ui_attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		ui_attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		ui_attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		ui_attachments[2].format = VK_FORMAT_R16G16_SFLOAT;
		ui_attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
		ui_attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		ui_attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		ui_attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		ui_attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		ui_attachments[2].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		ui_attachments[2].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		ui_color_refs[0].attachment = 0;
		ui_color_refs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		ui_color_refs[1].attachment = 2;
		ui_color_refs[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		ui_depth_ref.attachment = 1;
		ui_depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		Com_Memset( &ui_subpass, 0, sizeof( ui_subpass ) );
		ui_subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		ui_subpass.colorAttachmentCount = 2;
		ui_subpass.pColorAttachments = ui_color_refs;
		ui_subpass.pDepthStencilAttachment = &ui_depth_ref;

		Com_Memset( &ui_desc, 0, sizeof( ui_desc ) );
		ui_desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		ui_desc.pNext = NULL;
		ui_desc.flags = 0;
		ui_desc.pAttachments = ui_attachments;
		ui_desc.attachmentCount = 3;
		ui_desc.pSubpasses = &ui_subpass;
		ui_desc.subpassCount = 1;
		ui_desc.dependencyCount = 2;
		ui_desc.pDependencies = deps;

		if ( vk.msaaActive ) {
			ui_attachments[3].format = vk.color_format;
			ui_attachments[3].samples = vkSamples;
			ui_attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			ui_attachments[3].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			ui_attachments[3].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			ui_attachments[3].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			ui_attachments[3].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			ui_attachments[3].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			ui_attachments[4].format = VK_FORMAT_R16G16_SFLOAT;
			ui_attachments[4].samples = vkSamples;
			ui_attachments[4].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			ui_attachments[4].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			ui_attachments[4].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			ui_attachments[4].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			ui_attachments[4].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			ui_attachments[4].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			ui_color_refs[0].attachment = 3;
			ui_color_refs[1].attachment = 4;

			ui_resolve_refs[0].attachment = 0;
			ui_resolve_refs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			ui_resolve_refs[1].attachment = 2;
			ui_resolve_refs[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			ui_subpass.pResolveAttachments = ui_resolve_refs;
			ui_desc.attachmentCount = 5;
		}

		VK_CHECK( qvkCreateRenderPass( device, &ui_desc, NULL, &vk.render_pass.ui_overlay ) );
		SET_OBJECT_NAME( vk.render_pass.ui_overlay, "render pass - ui overlay", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
	}

	/* Atmosphere pass: additive fullscreen overlay for sky (depth test culls geometry) */
	{
		VkAttachmentDescription atm_att[2];
		VkAttachmentReference atm_color_ref, atm_depth_ref;
		VkSubpassDescription atm_subpass;
		VkSubpassDependency atm_deps[2];
		VkRenderPassCreateInfo atm_desc;

		Com_Memset( atm_att, 0, sizeof( atm_att ) );
		atm_att[0].format = vk.color_format;
		atm_att[0].samples = vkSamples;
		atm_att[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		atm_att[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		atm_att[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		atm_att[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		atm_att[1].format = vk.depth_format;
		atm_att[1].samples = vkSamples;
		atm_att[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		atm_att[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		atm_att[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		atm_att[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
		/* Main/post-bloom leave the shared depth buffer in attachment layout.
		 * Let the render pass perform the internal transition to read-only depth
		 * for the subpass, then return it to attachment layout on exit. */
		atm_att[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		atm_att[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		atm_color_ref.attachment = 0;
		atm_color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		atm_depth_ref.attachment = 1;
		atm_depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

		Com_Memset( &atm_subpass, 0, sizeof( atm_subpass ) );
		atm_subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		atm_subpass.colorAttachmentCount = 1;
		atm_subpass.pColorAttachments = &atm_color_ref;
		atm_subpass.pDepthStencilAttachment = &atm_depth_ref;

		Com_Memset( atm_deps, 0, sizeof( atm_deps ) );
		atm_deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		atm_deps[0].dstSubpass = 0;
		atm_deps[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		atm_deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		atm_deps[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		atm_deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		atm_deps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		atm_deps[1].srcSubpass = 0;
		atm_deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		atm_deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		atm_deps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
		atm_deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		atm_deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;
		atm_deps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		Com_Memset( &atm_desc, 0, sizeof( atm_desc ) );
		atm_desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		atm_desc.attachmentCount = 2;
		atm_desc.pAttachments = atm_att;
		atm_desc.subpassCount = 1;
		atm_desc.pSubpasses = &atm_subpass;
		atm_desc.dependencyCount = 2;
		atm_desc.pDependencies = atm_deps;
		VK_CHECK( qvkCreateRenderPass( device, &atm_desc, NULL, &vk.render_pass.atmosphere ) );
		SET_OBJECT_NAME( vk.render_pass.atmosphere, "render pass - atmosphere", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
	}

	if ( vk.smaaActive )
	{
		VkAttachmentDescription smaaAttachment;
		VkAttachmentReference smaaColorRef;
		VkSubpassDescription smaaSubpass;
		VkSubpassDependency smaaDeps[2];
		VkRenderPassCreateInfo smaaDesc;
		VkRenderPass *smaaPasses[3];
		const char *smaaNames[3];

		smaaPasses[0] = &vk.render_pass.smaa_edge;
		smaaPasses[1] = &vk.render_pass.smaa_blend;
		smaaPasses[2] = &vk.render_pass.smaa_compose;

		smaaNames[0] = "render pass - smaa edge";
		smaaNames[1] = "render pass - smaa blend";
		smaaNames[2] = "render pass - smaa compose";

		Com_Memset( &smaaAttachment, 0, sizeof( smaaAttachment ) );
		smaaAttachment.flags = 0;
		smaaAttachment.format = vk.color_format;
		smaaAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		smaaAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		smaaAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		smaaAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		smaaAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		smaaAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		smaaAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		smaaColorRef.attachment = 0;
		smaaColorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		Com_Memset( &smaaSubpass, 0, sizeof( smaaSubpass ) );
		smaaSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		smaaSubpass.colorAttachmentCount = 1;
		smaaSubpass.pColorAttachments = &smaaColorRef;

		Com_Memset( smaaDeps, 0, sizeof( smaaDeps ) );
		smaaDeps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		smaaDeps[0].dstSubpass = 0;
		smaaDeps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		smaaDeps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		smaaDeps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		smaaDeps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		smaaDeps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		smaaDeps[1].srcSubpass = 0;
		smaaDeps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		smaaDeps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		smaaDeps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		smaaDeps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		smaaDeps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		smaaDeps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		Com_Memset( &smaaDesc, 0, sizeof( smaaDesc ) );
		smaaDesc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		smaaDesc.pNext = NULL;
		smaaDesc.flags = 0;
		smaaDesc.pAttachments = &smaaAttachment;
		smaaDesc.attachmentCount = 1;
		smaaDesc.pSubpasses = &smaaSubpass;
		smaaDesc.subpassCount = 1;
		smaaDesc.pDependencies = smaaDeps;
		smaaDesc.dependencyCount = 2;

		for ( i = 0; i < ARRAY_LEN( smaaPasses ); i++ )
		{
			VK_CHECK( qvkCreateRenderPass( device, &smaaDesc, NULL, smaaPasses[i] ) );
			SET_OBJECT_NAME( *smaaPasses[i], smaaNames[i], VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
		}
	}

	if ( vk.fboActive )
	{
		VkAttachmentDescription taaAttachment;
		VkAttachmentReference taaColorRef;
		VkSubpassDescription taaSubpass;
		VkSubpassDependency taaDeps[2];
		VkRenderPassCreateInfo taaDesc;

		Com_Memset( &taaAttachment, 0, sizeof( taaAttachment ) );
		taaAttachment.flags = 0;
		taaAttachment.format = vk.color_format;
		taaAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		taaAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		taaAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		taaAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		taaAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		taaAttachment.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		taaAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		taaColorRef.attachment = 0;
		taaColorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		Com_Memset( &taaSubpass, 0, sizeof( taaSubpass ) );
		taaSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		taaSubpass.colorAttachmentCount = 1;
		taaSubpass.pColorAttachments = &taaColorRef;

		Com_Memset( taaDeps, 0, sizeof( taaDeps ) );
		taaDeps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		taaDeps[0].dstSubpass = 0;
		taaDeps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		taaDeps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		taaDeps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		taaDeps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		taaDeps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		taaDeps[1].srcSubpass = 0;
		taaDeps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		taaDeps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		taaDeps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		taaDeps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		taaDeps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		taaDeps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		Com_Memset( &taaDesc, 0, sizeof( taaDesc ) );
		taaDesc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		taaDesc.pNext = NULL;
		taaDesc.flags = 0;
		taaDesc.pAttachments = &taaAttachment;
		taaDesc.attachmentCount = 1;
		taaDesc.pSubpasses = &taaSubpass;
		taaDesc.subpassCount = 1;
		taaDesc.pDependencies = taaDeps;
		taaDesc.dependencyCount = 2;

		VK_CHECK( qvkCreateRenderPass( device, &taaDesc, NULL, &vk.render_pass.taa ) );
		SET_OBJECT_NAME( vk.render_pass.taa, "render pass - taa", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
	}

	// screenmap
	desc.dependencyCount = 2;
	desc.pDependencies = &deps[0];

	// screenmap resolve/color buffer
	attachments[0].flags = 0;
	attachments[0].format = vk.base_format.format;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
#ifdef USE_BUFFER_CLEAR
	if ( vk.screenMapSamples > VK_SAMPLE_COUNT_1_BIT )
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	else
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
#else
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // Assuming this will be completely overwritten
#endif
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;   // needed for next render pass
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// screenmap depth buffer
	attachments[1].flags = 0;
	attachments[1].format = depth_format;
	attachments[1].samples = vk.screenMapSamples;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Need empty depth buffer before use
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// screenmap dummy motion buffer so shaders with location=1 output match the subpass
	attachments[2].flags = 0;
	attachments[2].format = VK_FORMAT_R16G16_SFLOAT;
	attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[2].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	attachments[2].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	colorRef0.attachment = 0;
	colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorRefs[1].attachment = 2;
	colorRefs[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	depthRef0.attachment = 1;
	depthRef0.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	Com_Memset( &subpass, 0, sizeof( subpass ) );
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 2;
	subpass.pColorAttachments = colorRefs;
	subpass.pDepthStencilAttachment = &depthRef0;

	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.pAttachments = attachments;
	desc.pSubpasses = &subpass;
	desc.subpassCount = 1;
	desc.attachmentCount = 3;
	desc.dependencyCount = 2;
	desc.pDependencies = deps;

	if ( vk.screenMapSamples > VK_SAMPLE_COUNT_1_BIT ) {
		attachments[3].flags = 0;
		attachments[3].format = vk.base_format.format;
		attachments[3].samples = vk.screenMapSamples;
#ifdef USE_BUFFER_CLEAR
		attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
#else
		attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
#endif
		attachments[3].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[3].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[3].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[3].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachments[3].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		attachments[4].flags = 0;
		attachments[4].format = VK_FORMAT_R16G16_SFLOAT;
		attachments[4].samples = vk.screenMapSamples;
#ifdef USE_BUFFER_CLEAR
		attachments[4].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
#else
		attachments[4].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
#endif
		attachments[4].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[4].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[4].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[4].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachments[4].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		desc.attachmentCount = 5;

		colorRefs[0].attachment = 3; // screenmap msaa image attachment
		colorRefs[1].attachment = 4; // screenmap msaa motion attachment

		colorResolveRefs[0].attachment = 0; // screenmap resolve image attachment
		colorResolveRefs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		colorResolveRefs[1].attachment = VK_ATTACHMENT_UNUSED;
		colorResolveRefs[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		subpass.pResolveAttachments = colorResolveRefs;
	}

	VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.screenmap ) );

	SET_OBJECT_NAME( vk.render_pass.screenmap, "render pass - screenmap", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

	// sun shadow (always 1x depth/color so compute sampling is never MSAA)
	attachments[0].flags = 0;
	attachments[0].format = vk.color_format;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	attachments[1].flags = 0;
	attachments[1].format = depth_format;
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	colorRef0.attachment = 0;
	colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	depthRef0.attachment = 1;
	depthRef0.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	Com_Memset( &subpass, 0, sizeof( subpass ) );
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef0;
	subpass.pDepthStencilAttachment = &depthRef0;
	subpass.pResolveAttachments = NULL;

	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.pAttachments = attachments;
	desc.attachmentCount = 2;
	desc.pSubpasses = &subpass;
	desc.subpassCount = 1;
	desc.dependencyCount = 2;
	desc.pDependencies = deps;

	VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.sun_shadow ) );
	SET_OBJECT_NAME( vk.render_pass.sun_shadow, "render pass - sun shadow", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

	VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.local_spot_shadow ) );
	SET_OBJECT_NAME( vk.render_pass.local_spot_shadow, "render pass - local spot shadow", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

	VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.local_point_shadow ) );
	SET_OBJECT_NAME( vk.render_pass.local_point_shadow, "render pass - local point shadow", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

#ifdef VK_PBR_BRDFLUT
    if( vk.pbrActive )
    {
    #ifdef VK_CUBEMAP 
        if ( vk.cubemapActive ) 
        {   			
			desc.attachmentCount = 2;
			attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[1].samples = vkSamples;

			colorRef0.attachment = 0;
			colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			depthRef0.attachment = 1;
			depthRef0.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			Com_Memset( &subpass, 0, sizeof( subpass ) );
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.colorAttachmentCount = 1;
			subpass.pColorAttachments = &colorRef0;
			subpass.pDepthStencilAttachment = &depthRef0;

			if ( vk.msaaActive ) {
				desc.attachmentCount = 3;
				attachments[2].samples = vkSamples;
				attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE; 

				colorRef0.attachment = 2; // msaa image attachment
				colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

				colorResolveRef.attachment = 0; // resolve image attachment
				colorResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

				subpass.pResolveAttachments = &colorResolveRef;
			}

            VK_CHECK(qvkCreateRenderPass(device, &desc, NULL, &vk.render_pass.cubemap));
            SET_OBJECT_NAME(vk.render_pass.cubemap, "render pass - cubemap", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT);
        }  
    #endif

        deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        deps[0].dstSubpass = 0;
        deps[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        deps[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        deps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
       
		deps[1].srcSubpass = 0;
        deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        deps[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        deps[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        deps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        
		attachments[0].format = VK_FORMAT_R16G16_SFLOAT;
        attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        
		colorRef0.attachment = 0;
        colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        
		Com_Memset(&subpass, 0, sizeof(subpass));
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef0;
        subpass.pDepthStencilAttachment = VK_NULL_HANDLE;

        Com_Memset(&desc, 0, sizeof(desc));
        desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        desc.pNext = NULL;
        desc.flags = 0;
        desc.pAttachments = attachments;
        desc.pSubpasses = &subpass;
        desc.subpassCount = 1;
        desc.attachmentCount = 1;
        desc.dependencyCount = 2;
        desc.pDependencies = deps;
        VK_CHECK(qvkCreateRenderPass(device, &desc, NULL, &vk.render_pass.brdflut));
        SET_OBJECT_NAME(vk.render_pass.brdflut, "render pass - brdf lut", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT);
    }
#endif
}
