#include "tr_local.h"
#include "vk.h"
#include "vk_post_aa.h"
#include "vk_post_fog.h"
#include "vk_render_pass.h"

typedef struct {
	float threshold;
	float localContrast;
	int maxSearchSteps;
	float corner_rounding;
} SMAAPushConstants_t;

typedef struct {
	float invResolutionX;
	float invResolutionY;
	float subpixQuality;
	float edgeThreshold;
} FXAAPushConstants_t;

qboolean vk_post_aa_output_active( void )
{
	return vk.smaaActive || vk.fxaaActive;
}

static qboolean vk_run_smaa_pass( VkPipeline pipeline, VkRenderPass pass, VkFramebuffer framebuffer,
	VkDescriptorSet color_descriptor, VkDescriptorSet aux_descriptor, uint32_t width, uint32_t height )
{
	if ( !pipeline || pass == VK_NULL_HANDLE || framebuffer == VK_NULL_HANDLE ||
		vk.pipeline_layout_smaa == VK_NULL_HANDLE || color_descriptor == VK_NULL_HANDLE ) {
		return qfalse;
	}

	vk_begin_render_pass_tracked( pass, framebuffer, qfalse, width, height );
	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );

	{
		SMAAPushConstants_t pc;
		int preset = ( r_smaa_preset && r_smaa_preset->integer >= 1 && r_smaa_preset->integer <= 4 ) ? r_smaa_preset->integer : 0;
		if ( preset ) {
			static const float preset_threshold[5]  = { 0.0f, 0.15f, 0.1f, 0.08f, 0.05f };
			static const float preset_contrast[5]   = { 0.0f, 2.0f, 2.0f, 2.2f, 2.5f };
			static const int   preset_search[5]     = { 0, 8, 16, 24, 32 };
			pc.threshold = preset_threshold[preset];
			pc.localContrast = preset_contrast[preset];
			pc.maxSearchSteps = preset_search[preset];
		} else {
			pc.threshold = r_smaa_threshold ? r_smaa_threshold->value : 0.1f;
			pc.localContrast = r_smaa_local_contrast ? r_smaa_local_contrast->value : 2.0f;
			pc.maxSearchSteps = ( r_smaa_max_search_steps && r_smaa_max_search_steps->integer >= 8 && r_smaa_max_search_steps->integer <= 32 )
				? r_smaa_max_search_steps->integer : 16;
		}
		pc.corner_rounding = ( r_smaa_corner_rounding && r_smaa_corner_rounding->value >= 0.0f ) ?
			( r_smaa_corner_rounding->value <= 1.0f ? r_smaa_corner_rounding->value : 1.0f ) : 0.2f;
		qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout_smaa, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( pc ), &pc );
	}

	{
		VkDescriptorSet descriptor_sets[2];
		descriptor_sets[0] = color_descriptor;
		descriptor_sets[1] = ( aux_descriptor != VK_NULL_HANDLE ) ? aux_descriptor : color_descriptor;
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_smaa, 0, 2, descriptor_sets, 0, NULL );
	}

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

	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
	vk_end_render_pass();
	return qtrue;
}

static qboolean vk_smaa_passes( void )
{
	uint32_t w, h;

	if ( !vk.smaaActive ) {
		return qfalse;
	}
	if ( vk.color_image_view == VK_NULL_HANDLE || vk.smaa_output_image_view == VK_NULL_HANDLE ) {
		return qfalse;
	}
	w = ( glConfig.vidWidth > 0 ) ? (uint32_t)glConfig.vidWidth : 1u;
	h = ( glConfig.vidHeight > 0 ) ? (uint32_t)glConfig.vidHeight : 1u;

	if ( !vk_run_smaa_pass( vk.smaa_edge_pipeline, vk.render_pass.smaa_edge, vk.framebuffers.smaa_edge,
		vk.smaa_edge_descriptor, vk.smaa_edge_descriptor, w, h ) ) {
		return qfalse;
	}
	if ( !vk_run_smaa_pass( vk.smaa_blend_pipeline, vk.render_pass.smaa_blend, vk.framebuffers.smaa_blend,
		vk.smaa_edge_descriptor, vk.smaa_blend_descriptor, w, h ) ) {
		return qfalse;
	}
	if ( !vk_run_smaa_pass( vk.smaa_compose_pipeline, vk.render_pass.smaa_compose, vk.framebuffers.smaa_compose,
		vk.smaa_edge_descriptor, vk.smaa_compose_descriptor, w, h ) ) {
		return qfalse;
	}
	return qtrue;
}

static qboolean vk_fxaa_pass( void )
{
	uint32_t w, h;
	FXAAPushConstants_t pc;

	if ( !vk.fxaaActive ) {
		return qfalse;
	}
	if ( vk.color_image_view == VK_NULL_HANDLE || vk.smaa_output_image_view == VK_NULL_HANDLE ||
		vk.fxaa_pipeline == VK_NULL_HANDLE || vk.render_pass.smaa_compose == VK_NULL_HANDLE ||
		vk.framebuffers.smaa_compose == VK_NULL_HANDLE || vk.pipeline_layout_post_process == VK_NULL_HANDLE ||
		vk.color_descriptor[vk.cmd_index] == VK_NULL_HANDLE ) {
		return qfalse;
	}

	w = ( glConfig.vidWidth > 0 ) ? (uint32_t)glConfig.vidWidth : 1u;
	h = ( glConfig.vidHeight > 0 ) ? (uint32_t)glConfig.vidHeight : 1u;

	vk_begin_render_pass_tracked( vk.render_pass.smaa_compose, vk.framebuffers.smaa_compose, qfalse, w, h );
	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.fxaa_pipeline );

	pc.invResolutionX = 1.0f / (float)w;
	pc.invResolutionY = 1.0f / (float)h;
	pc.subpixQuality = r_fxaa_subpix ? r_fxaa_subpix->value : 0.75f;
	pc.edgeThreshold = r_fxaa_edgeThreshold ? r_fxaa_edgeThreshold->value : 0.166f;
	qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout_post_process, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( pc ), &pc );

	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.pipeline_layout_post_process, 0, 1, &vk.color_descriptor[vk.cmd_index], 0, NULL );

	{
		VkViewport viewport;
		VkRect2D scissor;

		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)w;
		viewport.height = (float)h;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		scissor.offset.x = 0;
		scissor.offset.y = 0;
		scissor.extent.width = w;
		scissor.extent.height = h;

		qvkCmdSetViewport( vk.cmd->command_buffer, 0, 1, &viewport );
		qvkCmdSetScissor( vk.cmd->command_buffer, 0, 1, &scissor );
	}

	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
	vk_end_render_pass();
	return qtrue;
}

void vk_post_scene_aa_apply( void )
{
	VkImageView aa_output;
	qboolean aa_ran = qfalse;

	if ( !tr.world || ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) ) {
		vk_set_scene_post_fog_source( vk.color_image_view );
		vk_update_post_fog_descriptors( vk.color_image_view );
		return;
	}

	if ( !vk.smaaActive && !vk.fxaaActive ) {
		vk_set_scene_post_fog_source( vk.color_image_view );
		vk_update_post_fog_descriptors( vk.color_image_view );
		return;
	}

	vk_barrier_post_fog_source_for_sampling( vk.color_image_view, "pre-post-AA" );

	if ( vk.smaaActive ) {
		aa_ran = vk_smaa_passes();
	} else {
		aa_ran = vk_fxaa_pass();
	}

	aa_output = ( aa_ran && vk.smaa_output_image_view ) ? vk.smaa_output_image_view : vk.color_image_view;
	vk_set_scene_post_fog_source( aa_output );
	vk_update_post_fog_descriptors( aa_output );
	if ( aa_ran && aa_output == vk.smaa_output_image_view ) {
		vk_set_post_chain_last_writer( "post_aa" );
	} else {
		vk_set_post_chain_last_writer( "bloom" );
	}
}
