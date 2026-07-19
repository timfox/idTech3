#include "tr_local.h"
#include "vk.h"
#include "vk_aa_policy.h"
#include "vk_post_aa.h"
#include "vk_post_fog.h"
#include "vk_pass_registry.h"
#include "vk_render_pass.h"
#include "vk_view_state.h"

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
	w = vk_get_render_target_width();
	h = vk_get_render_target_height();
	if ( w < 1u ) {
		w = 1u;
	}
	if ( h < 1u ) {
		h = 1u;
	}

	vk_spine_pass_begin( VK_SPINE_PASS_SMAA );
	vk_spine_expect_layout( VK_SPINE_RES_HDR_COLOR, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_SPINE_PASS_SMAA, "smaa_edge_input" );
	if ( !vk_run_smaa_pass( vk.smaa_edge_pipeline, vk.render_pass.smaa_edge, vk.framebuffers.smaa_edge,
		vk.smaa_edge_descriptor, vk.smaa_edge_descriptor, w, h ) ) {
		vk_spine_pass_end( VK_SPINE_PASS_SMAA );
		return qfalse;
	}
	if ( !vk_run_smaa_pass( vk.smaa_blend_pipeline, vk.render_pass.smaa_blend, vk.framebuffers.smaa_blend,
		vk.smaa_edge_descriptor, vk.smaa_blend_descriptor, w, h ) ) {
		vk_spine_pass_end( VK_SPINE_PASS_SMAA );
		return qfalse;
	}
	if ( !vk_run_smaa_pass( vk.smaa_compose_pipeline, vk.render_pass.smaa_compose, vk.framebuffers.smaa_compose,
		vk.smaa_edge_descriptor, vk.smaa_compose_descriptor, w, h ) ) {
		vk_spine_pass_end( VK_SPINE_PASS_SMAA );
		return qfalse;
	}
	vk_spine_pass_end( VK_SPINE_PASS_SMAA );
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

	w = vk_get_render_target_width();
	h = vk_get_render_target_height();
	if ( w < 1u ) {
		w = 1u;
	}
	if ( h < 1u ) {
		h = 1u;
	}

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

static void vk_update_smaa_edge_source( VkImageView color_source )
{
	VkDescriptorImageInfo info;
	VkWriteDescriptorSet desc;
	Vk_Sampler_Def sd;

	if ( color_source == VK_NULL_HANDLE || vk.smaa_edge_descriptor == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
	sd.max_lod_1_0 = qtrue;
	sd.noAnisotropy = qtrue;

	Com_Memset( &info, 0, sizeof( info ) );
	info.sampler = vk_find_sampler( &sd );
	info.imageView = color_source;
	info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	desc.dstSet = vk.smaa_edge_descriptor;
	desc.dstBinding = 0;
	desc.descriptorCount = 1;
	desc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	desc.pImageInfo = &info;
	qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
}

qboolean vk_post_scene_aa_apply_from( VkImageView color_source )
{
	VkImageView aa_output;
	qboolean aa_ran = qfalse;

	if ( color_source == VK_NULL_HANDLE ) {
		return qfalse;
	}
	if ( !vk.smaaActive && !vk.fxaaActive ) {
		return qfalse;
	}

	vk_barrier_post_fog_source_for_sampling( color_source, "pre-post-AA-from" );
	if ( vk.smaaActive ) {
		vk_update_smaa_edge_source( color_source );
		aa_ran = vk_smaa_passes();
	} else {
		vk_update_color_descriptor_image( color_source );
		aa_ran = vk_fxaa_pass();
	}

	aa_output = ( aa_ran && vk.smaa_output_image_view ) ? vk.smaa_output_image_view : color_source;
	vk_set_scene_post_fog_source( aa_output );
	vk_update_post_fog_descriptors( aa_output );
	if ( aa_ran && aa_output == vk.smaa_output_image_view ) {
		vk_set_post_chain_last_writer( "post_aa" );
	}
	return aa_ran;
}

void vk_post_scene_aa_apply( void )
{
	/*
	 * Mode 5: defer SMAA until after Temporal Reconstruction. Do not rewrite
	 * the post-fog source — volumetrics / bloom may already point at fog_scene.
	 */
	if ( vk_aa_policy_wants_temporal_cleanup_smaa() && r_taa && r_taa->integer ) {
		return;
	}

	/* Menu / cinematic: 2D is authored into color_image (post_bloom fallback).
	 * Keep it sharp — skip spatial AA so main-menu / connect UI stays crisp. */
	if ( !tr.world || !backEnd.doneWorldScene ) {
		vk_set_scene_post_fog_source( vk.color_image_view );
		vk_update_post_fog_descriptors( vk.color_image_view );
		return;
	}

	if ( !vk_post_scene_aa_apply_from( vk.color_image_view ) ) {
		vk_set_scene_post_fog_source( vk.color_image_view );
		vk_update_post_fog_descriptors( vk.color_image_view );
		vk_set_post_chain_last_writer( "bloom" );
	}
}
