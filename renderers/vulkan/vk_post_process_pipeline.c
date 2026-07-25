#include "tr_local.h"
#include "vk.h"
#include "vk_postfx.h"

#ifndef VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT
#define VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT 1000484004
#endif

#define VK_POST_PROCESS_CHECK( function_call ) do { \
	VkResult vk_post_process_res__ = ( function_call ); \
	if ( vk_post_process_res__ < 0 ) { \
		ri.Error( ERR_FATAL, "Vulkan: %s returned code %d", #function_call, (int)vk_post_process_res__ ); \
	} \
} while ( 0 )

#define VK_POST_PROCESS_SET_OBJECT_NAME( obj, objName, objType ) ( (void)(obj), (void)(objName), (void)(objType) )

static inline qboolean vk_post_process_hdr64_active( void )
{
	return vk.color_format == VK_FORMAT_R64G64B64A64_SFLOAT;
}

/* Fragment specialization constants 0..33 (34 entries); keep in sync with assignments in vk_create_post_process_pipeline. */
#define VK_POST_PROCESS_FRAG_SPEC_MAP_COUNT 34

typedef struct {
	float gamma;
	float preExposureScale;
	float greyscale;
	float bloom_threshold;
	float bloom_intensity;
	int bloom_threshold_mode;
	int bloom_modulate;
	int target_quantized;
	int depth_r;
	int depth_g;
	int depth_b;
	float exposure;
	float bloom_knee;
	int tonemap_mode;
	int apply_srgb_gamma;
	int post_debug;
	float vignette_intensity;
	float vignette_radius;
	float chromatic_aberration;
	float film_grain;
	int postprocess_enabled;
	float outline_strength;
	float outline_threshold;
	int film_look;
	float post_contrast;
	float post_saturation;
	float sharpen;
	float bloom_scatter;
	int bloom_energy_preserve;
	int bloom_firefly_clamp;
	float bloom_firefly_ratio;
	float bloom_firefly_absolute;
	int bloom_firefly_neighborhood;
	int bloom_firefly_debug;
} Vk_PostProcess_FragSpecData;

static void vk_post_process_set_shader_stage_desc( VkPipelineShaderStageCreateInfo *desc, VkShaderStageFlagBits stage, VkShaderModule shader_module, const char *entry )
{
	desc->sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	desc->pNext = NULL;
	desc->flags = 0;
	desc->stage = stage;
	desc->module = shader_module;
	desc->pName = entry;
	desc->pSpecializationInfo = NULL;
}

static qboolean vk_post_process_surface_format_color_depth( VkFormat format, int *r, int *g, int *b )
{
	switch ( format ) {
		case VK_FORMAT_R8G8B8A8_UNORM:
		case VK_FORMAT_R8G8B8A8_SRGB:
		case VK_FORMAT_B8G8R8A8_UNORM:
		case VK_FORMAT_B8G8R8A8_SRGB:
		case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
		case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
			*r = *g = *b = 8;
			return qtrue;
		case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
		case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
			*r = *g = *b = 10;
			return qtrue;
		case VK_FORMAT_B5G6R5_UNORM_PACK16:
		case VK_FORMAT_R5G6B5_UNORM_PACK16:
			*r = 5; *g = 6; *b = 5;
			return qtrue;
		case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
		case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
			*r = *g = *b = 4;
			return qtrue;
		/*
		 * Floating-point SceneHDR / bloom targets are not display-quantized.
		 * Report a high nominal depth so any accidental dither path is a no-op
		 * relative to float precision, and treat recognition as success so we
		 * do not spam "assume 8bpc" on every HDR pipeline create.
		 */
		case VK_FORMAT_R16_SFLOAT:
		case VK_FORMAT_R16G16_SFLOAT:
		case VK_FORMAT_R16G16B16_SFLOAT:
		case VK_FORMAT_R16G16B16A16_SFLOAT:
		case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
		case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
			*r = *g = *b = 16;
			return qtrue;
		case VK_FORMAT_R32_SFLOAT:
		case VK_FORMAT_R32G32_SFLOAT:
		case VK_FORMAT_R32G32B32_SFLOAT:
		case VK_FORMAT_R32G32B32A32_SFLOAT:
		case VK_FORMAT_R64_SFLOAT:
		case VK_FORMAT_R64G64_SFLOAT:
		case VK_FORMAT_R64G64B64_SFLOAT:
		case VK_FORMAT_R64G64B64A64_SFLOAT:
			*r = *g = *b = 16;
			return qtrue;
		default:
			*r = *g = *b = 8;
			return qfalse;
	}
}

static qboolean vk_post_process_format_is_srgb( VkFormat format )
{
	switch ( format ) {
		case VK_FORMAT_R8G8B8A8_SRGB:
		case VK_FORMAT_B8G8R8A8_SRGB:
		case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
			return qtrue;
		default:
			return qfalse;
	}
}

void vk_create_post_process_pipeline( int program_index, uint32_t width, uint32_t height )
{
	VkPipelineShaderStageCreateInfo shader_stages[2];
	VkPipelineVertexInputStateCreateInfo vertex_input_state;
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
	VkPipelineRasterizationStateCreateInfo rasterization_state;
	VkPipelineDepthStencilStateCreateInfo depth_stencil_state;
	VkPipelineViewportStateCreateInfo viewport_state;
	VkPipelineDynamicStateCreateInfo dynamic_state;
	VkDynamicState dynamic_state_array[3] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	uint32_t dynamic_state_count = 2;
	if ( vk.colorWriteMaskDynamic ) {
		dynamic_state_array[dynamic_state_count++] = VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT;
	}
	VkPipelineMultisampleStateCreateInfo multisample_state;
	VkPipelineColorBlendStateCreateInfo blend_state;
	VkPipelineColorBlendAttachmentState attachment_blend_state;
	VkPipelineColorBlendAttachmentState attachment_blend_states[2];
	VkGraphicsPipelineCreateInfo create_info;
	VkViewport viewport;
	VkRect2D scissor;
	VkSpecializationMapEntry spec_entries[VK_POST_PROCESS_FRAG_SPEC_MAP_COUNT];
	VkSpecializationInfo frag_spec_info;
	VkPipeline *pipeline;
	VkShaderModule fsmodule;
	VkRenderPass renderpass;
	VkPipelineLayout layout;
	VkFormat target_format;
	VkSampleCountFlagBits samples;
	const char *pipeline_name;
	qboolean blend;
	qboolean alpha_composite = qfalse;

	Vk_PostProcess_FragSpecData frag_spec_data;

	switch ( program_index ) {
		case 1:
			pipeline = &vk.bloom_extract_pipeline;
			fsmodule = vk.modules.bloom_fs;
			renderpass = vk.render_pass.bloom_extract;
			layout = vk.pipeline_layout_post_process;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "bloom extraction pipeline";
			target_format = vk.capture_format;
			blend = qfalse;
			break;
		case 5:
			pipeline = &vk.ssao_pipeline;
			fsmodule = vk.modules.ssao_fs;
			renderpass = vk.render_pass.ssao;
			layout = vk.pipeline_layout_ssao;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "ssao pipeline";
			target_format = vk.capture_format;
			blend = qfalse;
			break;
		case 21:
			pipeline = &vk.hbao_pipeline;
			fsmodule = vk.modules.hbao_fs;
			renderpass = vk.render_pass.ssao;
			layout = vk.pipeline_layout_ssao;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "hbao pipeline";
			target_format = vk.capture_format;
			blend = qfalse;
			break;
		case 6:
			pipeline = &vk.ssao_blur_pipeline;
			fsmodule = vk.modules.ssao_blur_fs;
			renderpass = vk.render_pass.ssao_blur;
			layout = vk.pipeline_layout_ssao;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "ssao blur pipeline";
			target_format = vk.capture_format;
			blend = qfalse;
			break;
		case 7:
			pipeline = &vk.ssao_combine_pipeline;
			fsmodule = vk.modules.ssao_combine_fs;
			renderpass = vk.render_pass.ssao_combine;
			layout = vk.pipeline_layout_ssao_combine;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "ssao combine pipeline";
			target_format = vk.color_format;
			blend = qfalse;
			break;
		case 8:
			pipeline = &vk.ssao_debug_pipeline;
			fsmodule = vk.modules.ssao_debug_fs;
			renderpass = vk.render_pass.ssao_combine;
			layout = vk.pipeline_layout_post_process;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "ssao debug pipeline";
			target_format = vk.capture_format;
			blend = qfalse;
			break;
		case 9:
			pipeline = &vk.ssao_depth_debug_pipeline;
			fsmodule = vk.modules.ssao_depth_debug_fs;
			renderpass = vk.render_pass.ssao_combine;
			layout = vk.pipeline_layout_post_process;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "ssao depth debug pipeline";
			target_format = vk.capture_format;
			blend = qfalse;
			break;
		case 20:
			if ( vk.pipeline_layout_oit_resolve == VK_NULL_HANDLE ) {
				return;
			}
			pipeline = &vk.oit_resolve_pipeline;
			fsmodule = vk.modules.oit_resolve_fs;
			renderpass = vk.render_pass.oit_resolve;
			layout = vk.pipeline_layout_oit_resolve;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "oit resolve pipeline";
			target_format = vk.color_format;
			blend = qfalse;
			break;
		case 2:
			pipeline = &vk.bloom_blend_pipeline;
			fsmodule = vk.modules.blend_fs;
			renderpass = vk.render_pass.post_bloom;
			layout = vk.pipeline_layout_blend;
			samples = vk_get_main_rasterization_samples();
			pipeline_name = "bloom blend pipeline";
			target_format = vk.color_format;
			blend = qtrue;
			break;
		case 3:
			pipeline = &vk.capture_pipeline;
			fsmodule = vk.modules.gamma_fs;
			renderpass = vk.render_pass.capture;
			layout = vk.pipeline_layout_post_process;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "capture buffer pipeline";
			target_format = vk.capture_format;
			blend = qfalse;
			break;
		case 10:
			pipeline = &vk.smaa_edge_pipeline;
			fsmodule = vk_post_process_hdr64_active() ? vk.modules.smaa_edge_fs_hdr64 : vk.modules.smaa_edge_fs;
			renderpass = vk.render_pass.smaa_edge;
			layout = vk.pipeline_layout_smaa;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "smaa edge pipeline";
			target_format = vk.color_format;
			blend = qfalse;
			break;
		case 11:
			pipeline = &vk.smaa_blend_pipeline;
			fsmodule = vk_post_process_hdr64_active() ? vk.modules.smaa_blend_fs_hdr64 : vk.modules.smaa_blend_fs;
			renderpass = vk.render_pass.smaa_blend;
			layout = vk.pipeline_layout_smaa;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "smaa blend pipeline";
			target_format = vk.color_format;
			blend = qfalse;
			break;
		case 12:
			pipeline = &vk.smaa_compose_pipeline;
			fsmodule = vk_post_process_hdr64_active() ? vk.modules.smaa_compose_fs_hdr64 : vk.modules.smaa_compose_fs;
			renderpass = vk.render_pass.smaa_compose;
			layout = vk.pipeline_layout_smaa;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "smaa compose pipeline";
			target_format = vk.color_format;
			blend = qfalse;
			break;
		case 24:
			pipeline = &vk.fxaa_pipeline;
			fsmodule = vk.modules.fxaa_fs;
			renderpass = vk.render_pass.smaa_compose;
			layout = vk.pipeline_layout_post_process;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "fxaa pipeline";
			target_format = vk.color_format;
			blend = qfalse;
			break;
		case 26:
			pipeline = &vk.spatial_adaptive_pipeline;
			fsmodule = vk.modules.spatial_adaptive_fs;
			renderpass = vk.render_pass.taa;
			layout = vk.pipeline_layout_smaa;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "spatial adaptive SS pipeline";
			target_format = vk.color_format;
			blend = qfalse;
			break;
		case 25:
			pipeline = &vk.lens_flare_pipeline;
			fsmodule = vk.modules.lens_flare_fs;
			renderpass = vk.render_pass.post_bloom;
			layout = vk.pipeline_layout_blend;
			samples = vk_get_main_rasterization_samples();
			pipeline_name = "lens flare pipeline";
			target_format = vk.color_format;
			blend = qtrue;
			break;
		case 13:
			pipeline = &vk.ssr_pipeline;
			fsmodule = vk_post_process_hdr64_active() ? vk.modules.ssr_fs_hdr64 : vk.modules.ssr_fs;
			renderpass = vk.render_pass.ssr;
			layout = vk.pipeline_layout_ssr;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "ssr pipeline";
			target_format = vk.color_format;
			blend = qfalse;
			break;
		case 22:
			pipeline = &vk.overlay_compose_pipeline;
			fsmodule = vk.modules.overlay_compose_fs;
			renderpass = vk.render_pass.overlay_compose;
			layout = vk.pipeline_layout_post_process;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "overlay compose pipeline";
			target_format = vk.present_format.format;
			blend = qtrue;
			alpha_composite = qtrue;
			break;
		case 23:
			pipeline = &vk.taa_pipeline;
			fsmodule = vk.modules.taa_fs;
			renderpass = vk.render_pass.taa;
			layout = vk.pipeline_layout_taa;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "taa resolve pipeline";
			target_format = vk.color_format;
			blend = qfalse;
			break;
		case 27:
			pipeline = &vk.weapon_taa_pipeline;
			fsmodule = vk.modules.weapon_taa_fs;
			renderpass = vk.render_pass.taa;
			layout = vk.pipeline_layout_weapon_taa;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "weapon taa resolve pipeline";
			target_format = vk.color_format;
			blend = qfalse;
			break;
		case 28:
			pipeline = &vk.weapon_taa_composite_pipeline;
			fsmodule = vk.modules.weapon_taa_composite_fs;
			renderpass = vk.render_pass.post_bloom;
			layout = vk.pipeline_layout_weapon_composite;
			samples = vk_get_main_rasterization_samples();
			pipeline_name = "weapon taa composite pipeline";
			target_format = vk.color_format;
			blend = qfalse;
			break;
		case 29:
			pipeline = &vk.weapon_bloom_extract_pipeline;
			fsmodule = vk.modules.weapon_bloom_extract_fs;
			renderpass = vk.render_pass.bloom_extract;
			layout = vk.pipeline_layout_weapon_bloom != VK_NULL_HANDLE ?
				vk.pipeline_layout_weapon_bloom : vk.pipeline_layout_weapon_composite;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "weapon bloom extract pipeline";
			target_format = vk.capture_format;
			blend = qfalse;
			break;
#ifdef VK_PBR_BRDFLUT
		case 4:
			pipeline = &vk.brdflut_pipeline;
			fsmodule = vk.modules.brdflut_fs;
			renderpass = vk.render_pass.brdflut;
			layout = vk.pipeline_layout_brdflut;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "brdf LUT pipeline";
			target_format = vk.capture_format;
			blend = qfalse;
			break;
#endif
		default:
			pipeline = &vk.gamma_pipeline;
			fsmodule = vk.modules.gamma_fs;
			renderpass = vk.render_pass.gamma;
			layout = vk.pipeline_layout_post_process;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "gamma-correction pipeline";
			target_format = vk.present_format.format;
			blend = qfalse;
			break;
	}

	if ( program_index != 22 ) {
		alpha_composite = qfalse;
	}

	if ( *pipeline != VK_NULL_HANDLE ) {
		vk_wait_idle();
		qvkDestroyPipeline( vk.device, *pipeline, NULL );
		*pipeline = VK_NULL_HANDLE;
	}

	vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_state.pNext = NULL;
	vertex_input_state.flags = 0;
	vertex_input_state.vertexBindingDescriptionCount = 0;
	vertex_input_state.pVertexBindingDescriptions = NULL;
	vertex_input_state.vertexAttributeDescriptionCount = 0;
	vertex_input_state.pVertexBindingDescriptions = NULL;

	vk_post_process_set_shader_stage_desc( shader_stages + 0, VK_SHADER_STAGE_VERTEX_BIT, vk.modules.gamma_vs, "main" );
	vk_post_process_set_shader_stage_desc( shader_stages + 1, VK_SHADER_STAGE_FRAGMENT_BIT, fsmodule, "main" );

	frag_spec_data.gamma = 1.0f / ( r_gamma->value );
	frag_spec_data.preExposureScale = ( r_pre_exposure_scale && r_pre_exposure_scale->value > 0.0f ) ?
		r_pre_exposure_scale->value : 1.0f;
	frag_spec_data.greyscale = r_greyscale->value;
	frag_spec_data.bloom_threshold = r_bloom_threshold->value;
	frag_spec_data.bloom_intensity = r_bloom_intensity->value;
	frag_spec_data.bloom_threshold_mode = r_bloom_threshold_mode->integer;
	frag_spec_data.bloom_modulate = r_bloom_modulate->integer;
	frag_spec_data.exposure = r_exposure ? r_exposure->value : 1.0f;
	frag_spec_data.bloom_knee = r_bloomKnee ? r_bloomKnee->value : 0.5f;
	frag_spec_data.tonemap_mode = r_tonemap ? r_tonemap->integer : 2;
	/*
	 * Display encode + dither belong only on integer present/capture targets.
	 * SceneHDR float destinations must stay scene-linear: treating "not sRGB"
	 * as "apply manual gamma" previously sRGB-encoded into FP16/FP32 buffers.
	 * target_quantized is a property of the attachment, not of r_dither, so
	 * toggling r_dither at runtime still works through the PostFX uniform.
	 */
	if ( vk_format_is_float( target_format ) ) {
		frag_spec_data.apply_srgb_gamma = 0;
		frag_spec_data.target_quantized = 0;
	} else {
		frag_spec_data.apply_srgb_gamma = vk_post_process_format_is_srgb( target_format ) ? 0 : 1;
		frag_spec_data.target_quantized = 1;
	}
	frag_spec_data.post_debug = r_post_debug ? r_post_debug->integer : 0;
	frag_spec_data.vignette_intensity = PostFX_GetVignetteIntensity();
	frag_spec_data.vignette_radius = PostFX_GetVignetteRadius();
	frag_spec_data.chromatic_aberration = PostFX_GetChromaticAberration();
	frag_spec_data.film_grain = PostFX_GetFilmGrain();
	frag_spec_data.postprocess_enabled = ( r_post && r_post->integer ) ? 1 : 0;
	frag_spec_data.film_look = PostFX_GetFilmLook();
	frag_spec_data.outline_strength = r_outline ? r_outline->value : 0.0f;
	frag_spec_data.outline_threshold = r_outlineThreshold ? r_outlineThreshold->value : 0.15f;
	{
		cvar_t *r_post_contrast = ri.Cvar_Get( "r_post_contrast", "1.0", CVAR_ARCHIVE_ND );
		cvar_t *r_post_saturation = ri.Cvar_Get( "r_post_saturation", "1.0", CVAR_ARCHIVE_ND );
		frag_spec_data.post_contrast = ( r_post_contrast && r_post_contrast->value > 0.0f ) ? r_post_contrast->value : 1.0f;
		frag_spec_data.post_saturation = ( r_post_saturation && r_post_saturation->value >= 0.0f ) ? r_post_saturation->value : 1.0f;
	}
	frag_spec_data.sharpen = PostFX_GetSharpen();
	{
		cvar_t *r_bloom_scatter = ri.Cvar_Get( "r_bloom_scatter", "0.72", CVAR_ARCHIVE_ND );
		cvar_t *r_bloom_energy = ri.Cvar_Get( "r_bloom_energyPreserve", "1", CVAR_ARCHIVE_ND );
		frag_spec_data.bloom_scatter = ( r_bloom_scatter && r_bloom_scatter->value > 0.0f ) ? r_bloom_scatter->value : 0.72f;
		frag_spec_data.bloom_energy_preserve = ( r_bloom_energy && r_bloom_energy->integer ) ? 1 : 0;
	}
	{
		cvar_t *ffClamp = ri.Cvar_Get( "r_bloomFireflyClamp", "1", 0 );
		cvar_t *ffRatio = ri.Cvar_Get( "r_bloomFireflyRatio", "4.0", 0 );
		cvar_t *ffAbs = ri.Cvar_Get( "r_bloomFireflyAbsolute", "0.25", 0 );
		cvar_t *ffNeigh = ri.Cvar_Get( "r_bloomFireflyNeighborhood", "1", 0 );
		cvar_t *ffDbg = ri.Cvar_Get( "r_bloomFireflyDebug", "0", 0 );
		frag_spec_data.bloom_firefly_clamp = ( ffClamp && ffClamp->integer ) ? 1 : 0;
		frag_spec_data.bloom_firefly_ratio = ( ffRatio && ffRatio->value > 0.0f ) ? ffRatio->value : 4.0f;
		frag_spec_data.bloom_firefly_absolute = ( ffAbs && ffAbs->value >= 0.0f ) ? ffAbs->value : 0.25f;
		frag_spec_data.bloom_firefly_neighborhood = ffNeigh ? ffNeigh->integer : 1;
		frag_spec_data.bloom_firefly_debug = ffDbg ? ffDbg->integer : 0;
	}

	/*
	 * Quantize for the image this pipeline actually writes. Capture is always
	 * RGBA8 even when the swapchain is 10-bit; using present_format there
	 * leaves the later 8-bit store undithered and reintroduces banding.
	 * Float SceneHDR targets are recognized above and must not warn as 8bpc.
	 */
	if ( !vk_post_process_surface_format_color_depth( target_format, &frag_spec_data.depth_r, &frag_spec_data.depth_g, &frag_spec_data.depth_b ) ) {
		ri.Printf( PRINT_DEVELOPER, "Format %s not recognized for dither depth; assuming 8bpc\n", vk_format_string( target_format ) );
	}

	spec_entries[0].constantID = 0;
	spec_entries[0].offset = offsetof( Vk_PostProcess_FragSpecData, gamma );
	spec_entries[0].size = sizeof( frag_spec_data.gamma );
	spec_entries[1].constantID = 1;
	spec_entries[1].offset = offsetof( Vk_PostProcess_FragSpecData, preExposureScale );
	spec_entries[1].size = sizeof( frag_spec_data.preExposureScale );
	spec_entries[2].constantID = 2;
	spec_entries[2].offset = offsetof( Vk_PostProcess_FragSpecData, greyscale );
	spec_entries[2].size = sizeof( frag_spec_data.greyscale );
	spec_entries[3].constantID = 3;
	spec_entries[3].offset = offsetof( Vk_PostProcess_FragSpecData, bloom_threshold );
	spec_entries[3].size = sizeof( frag_spec_data.bloom_threshold );
	spec_entries[4].constantID = 4;
	spec_entries[4].offset = offsetof( Vk_PostProcess_FragSpecData, bloom_intensity );
	spec_entries[4].size = sizeof( frag_spec_data.bloom_intensity );
	spec_entries[5].constantID = 5;
	spec_entries[5].offset = offsetof( Vk_PostProcess_FragSpecData, bloom_threshold_mode );
	spec_entries[5].size = sizeof( frag_spec_data.bloom_threshold_mode );
	spec_entries[6].constantID = 6;
	spec_entries[6].offset = offsetof( Vk_PostProcess_FragSpecData, bloom_modulate );
	spec_entries[6].size = sizeof( frag_spec_data.bloom_modulate );
	spec_entries[7].constantID = 7;
	spec_entries[7].offset = offsetof( Vk_PostProcess_FragSpecData, target_quantized );
	spec_entries[7].size = sizeof( frag_spec_data.target_quantized );
	spec_entries[8].constantID = 8;
	spec_entries[8].offset = offsetof( Vk_PostProcess_FragSpecData, depth_r );
	spec_entries[8].size = sizeof( frag_spec_data.depth_r );
	spec_entries[9].constantID = 9;
	spec_entries[9].offset = offsetof( Vk_PostProcess_FragSpecData, depth_g );
	spec_entries[9].size = sizeof( frag_spec_data.depth_g );
	spec_entries[10].constantID = 10;
	spec_entries[10].offset = offsetof( Vk_PostProcess_FragSpecData, depth_b );
	spec_entries[10].size = sizeof( frag_spec_data.depth_b );
	spec_entries[11].constantID = 11;
	spec_entries[11].offset = offsetof( Vk_PostProcess_FragSpecData, exposure );
	spec_entries[11].size = sizeof( frag_spec_data.exposure );
	spec_entries[12].constantID = 12;
	spec_entries[12].offset = offsetof( Vk_PostProcess_FragSpecData, bloom_knee );
	spec_entries[12].size = sizeof( frag_spec_data.bloom_knee );
	spec_entries[13].constantID = 13;
	spec_entries[13].offset = offsetof( Vk_PostProcess_FragSpecData, tonemap_mode );
	spec_entries[13].size = sizeof( frag_spec_data.tonemap_mode );
	spec_entries[14].constantID = 14;
	spec_entries[14].offset = offsetof( Vk_PostProcess_FragSpecData, apply_srgb_gamma );
	spec_entries[14].size = sizeof( frag_spec_data.apply_srgb_gamma );
	spec_entries[15].constantID = 15;
	spec_entries[15].offset = offsetof( Vk_PostProcess_FragSpecData, post_debug );
	spec_entries[15].size = sizeof( frag_spec_data.post_debug );
	spec_entries[16].constantID = 16;
	spec_entries[16].offset = offsetof( Vk_PostProcess_FragSpecData, vignette_intensity );
	spec_entries[16].size = sizeof( frag_spec_data.vignette_intensity );
	spec_entries[17].constantID = 17;
	spec_entries[17].offset = offsetof( Vk_PostProcess_FragSpecData, vignette_radius );
	spec_entries[17].size = sizeof( frag_spec_data.vignette_radius );
	spec_entries[18].constantID = 18;
	spec_entries[18].offset = offsetof( Vk_PostProcess_FragSpecData, chromatic_aberration );
	spec_entries[18].size = sizeof( frag_spec_data.chromatic_aberration );
	spec_entries[19].constantID = 19;
	spec_entries[19].offset = offsetof( Vk_PostProcess_FragSpecData, film_grain );
	spec_entries[19].size = sizeof( frag_spec_data.film_grain );
	spec_entries[20].constantID = 20;
	spec_entries[20].offset = offsetof( Vk_PostProcess_FragSpecData, postprocess_enabled );
	spec_entries[20].size = sizeof( frag_spec_data.postprocess_enabled );
	spec_entries[21].constantID = 21;
	spec_entries[21].offset = offsetof( Vk_PostProcess_FragSpecData, outline_strength );
	spec_entries[21].size = sizeof( frag_spec_data.outline_strength );
	spec_entries[22].constantID = 22;
	spec_entries[22].offset = offsetof( Vk_PostProcess_FragSpecData, outline_threshold );
	spec_entries[22].size = sizeof( frag_spec_data.outline_threshold );
	spec_entries[23].constantID = 23;
	spec_entries[23].offset = offsetof( Vk_PostProcess_FragSpecData, film_look );
	spec_entries[23].size = sizeof( frag_spec_data.film_look );
	spec_entries[24].constantID = 24;
	spec_entries[24].offset = offsetof( Vk_PostProcess_FragSpecData, post_contrast );
	spec_entries[24].size = sizeof( frag_spec_data.post_contrast );
	spec_entries[25].constantID = 25;
	spec_entries[25].offset = offsetof( Vk_PostProcess_FragSpecData, post_saturation );
	spec_entries[25].size = sizeof( frag_spec_data.post_saturation );
	spec_entries[26].constantID = 26;
	spec_entries[26].offset = offsetof( Vk_PostProcess_FragSpecData, sharpen );
	spec_entries[26].size = sizeof( frag_spec_data.sharpen );
	spec_entries[27].constantID = 27;
	spec_entries[27].offset = offsetof( Vk_PostProcess_FragSpecData, bloom_scatter );
	spec_entries[27].size = sizeof( frag_spec_data.bloom_scatter );
	spec_entries[28].constantID = 28;
	spec_entries[28].offset = offsetof( Vk_PostProcess_FragSpecData, bloom_energy_preserve );
	spec_entries[28].size = sizeof( frag_spec_data.bloom_energy_preserve );
	spec_entries[29].constantID = 29;
	spec_entries[29].offset = offsetof( Vk_PostProcess_FragSpecData, bloom_firefly_clamp );
	spec_entries[29].size = sizeof( frag_spec_data.bloom_firefly_clamp );
	spec_entries[30].constantID = 30;
	spec_entries[30].offset = offsetof( Vk_PostProcess_FragSpecData, bloom_firefly_ratio );
	spec_entries[30].size = sizeof( frag_spec_data.bloom_firefly_ratio );
	spec_entries[31].constantID = 31;
	spec_entries[31].offset = offsetof( Vk_PostProcess_FragSpecData, bloom_firefly_absolute );
	spec_entries[31].size = sizeof( frag_spec_data.bloom_firefly_absolute );
	spec_entries[32].constantID = 32;
	spec_entries[32].offset = offsetof( Vk_PostProcess_FragSpecData, bloom_firefly_neighborhood );
	spec_entries[32].size = sizeof( frag_spec_data.bloom_firefly_neighborhood );
	spec_entries[33].constantID = 33;
	spec_entries[33].offset = offsetof( Vk_PostProcess_FragSpecData, bloom_firefly_debug );
	spec_entries[33].size = sizeof( frag_spec_data.bloom_firefly_debug );

	_Static_assert( sizeof( spec_entries ) / sizeof( spec_entries[0] ) >= (size_t)VK_POST_PROCESS_FRAG_SPEC_MAP_COUNT,
		"vk_create_post_process_pipeline: spec_entries[] smaller than VK_POST_PROCESS_FRAG_SPEC_MAP_COUNT" );
	frag_spec_info.mapEntryCount = VK_POST_PROCESS_FRAG_SPEC_MAP_COUNT;
	frag_spec_info.pMapEntries = spec_entries;
	frag_spec_info.dataSize = sizeof( frag_spec_data );
	frag_spec_info.pData = &frag_spec_data;

	shader_stages[1].pSpecializationInfo = &frag_spec_info;
	if ( program_index >= 5 ) {
		shader_stages[1].pSpecializationInfo = NULL;
	}

	input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_state.pNext = NULL;
	input_assembly_state.flags = 0;
	input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	input_assembly_state.primitiveRestartEnable = VK_FALSE;

	if ( program_index == 0 ) {
		viewport.x = 0.0f + vk.blitX0;
		viewport.y = 0.0f + vk.blitY0;
		viewport.width = gls.windowWidth - vk.blitX0 * 2;
		viewport.height = gls.windowHeight - vk.blitY0 * 2;
	} else {
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = width;
		viewport.height = height;
	}

	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	scissor.offset.x = viewport.x;
	scissor.offset.y = viewport.y;
	scissor.extent.width = viewport.width;
	scissor.extent.height = viewport.height;

	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.pNext = NULL;
	viewport_state.flags = 0;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = &viewport;
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = &scissor;

	dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.pNext = NULL;
	dynamic_state.flags = 0;
	dynamic_state.dynamicStateCount = dynamic_state_count;
	dynamic_state.pDynamicStates = dynamic_state_array;

	rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization_state.pNext = NULL;
	rasterization_state.flags = 0;
	rasterization_state.depthClampEnable = VK_FALSE;
	rasterization_state.rasterizerDiscardEnable = VK_FALSE;
	rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
	rasterization_state.cullMode = VK_CULL_MODE_NONE;
	rasterization_state.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterization_state.depthBiasEnable = VK_FALSE;
	rasterization_state.depthBiasConstantFactor = 0.0f;
	rasterization_state.depthBiasClamp = 0.0f;
	rasterization_state.depthBiasSlopeFactor = 0.0f;
	rasterization_state.lineWidth = 1.0f;

	multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_state.pNext = NULL;
	multisample_state.flags = 0;
	multisample_state.rasterizationSamples = samples;
	multisample_state.sampleShadingEnable = VK_FALSE;
	multisample_state.minSampleShading = 1.0f;
	multisample_state.pSampleMask = NULL;
	multisample_state.alphaToCoverageEnable = VK_FALSE;
	multisample_state.alphaToOneEnable = VK_FALSE;

	Com_Memset( &attachment_blend_state, 0, sizeof( attachment_blend_state ) );
	attachment_blend_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	if ( blend ) {
		attachment_blend_state.blendEnable = VK_TRUE;
		if ( alpha_composite ) {
			attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
			attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			attachment_blend_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			attachment_blend_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			attachment_blend_state.colorBlendOp = VK_BLEND_OP_ADD;
			attachment_blend_state.alphaBlendOp = VK_BLEND_OP_ADD;
		} else {
			attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
			attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
		}
	} else {
		attachment_blend_state.blendEnable = VK_FALSE;
	}

	if ( program_index == 7 ) {
		attachment_blend_state.blendEnable = VK_TRUE;
		attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
		attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		attachment_blend_state.colorBlendOp = VK_BLEND_OP_ADD;
		attachment_blend_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		attachment_blend_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		attachment_blend_state.alphaBlendOp = VK_BLEND_OP_ADD;
	}

	blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state.pNext = NULL;
	blend_state.flags = 0;
	blend_state.logicOpEnable = VK_FALSE;
	blend_state.logicOp = VK_LOGIC_OP_COPY;
	/* VUID-07609: blend attachmentCount must match subpass colorAttachmentCount */
	if ( renderpass == vk.render_pass.post_bloom && vk.fboActive ) {
		Com_Memcpy( attachment_blend_states, &attachment_blend_state, sizeof( attachment_blend_state ) );
		Com_Memcpy( attachment_blend_states + 1, &attachment_blend_state, sizeof( attachment_blend_state ) );
		blend_state.attachmentCount = 2;
		blend_state.pAttachments = attachment_blend_states;
	} else {
		blend_state.attachmentCount = 1;
		blend_state.pAttachments = &attachment_blend_state;
	}
	blend_state.blendConstants[0] = 0.0f;
	blend_state.blendConstants[1] = 0.0f;
	blend_state.blendConstants[2] = 0.0f;
	blend_state.blendConstants[3] = 0.0f;

	Com_Memset( &depth_stencil_state, 0, sizeof( depth_stencil_state ) );
	depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil_state.pNext = NULL;
	depth_stencil_state.flags = 0;
	depth_stencil_state.depthTestEnable = VK_FALSE;
	depth_stencil_state.depthWriteEnable = VK_FALSE;
	depth_stencil_state.depthCompareOp = VK_COMPARE_OP_NEVER;
	depth_stencil_state.depthBoundsTestEnable = VK_FALSE;
	depth_stencil_state.stencilTestEnable = VK_FALSE;
	depth_stencil_state.minDepthBounds = 0.0f;
	depth_stencil_state.maxDepthBounds = 1.0f;

	create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	create_info.pNext = NULL;
	create_info.flags = 0;
	create_info.stageCount = 2;
	create_info.pStages = shader_stages;
	create_info.pVertexInputState = &vertex_input_state;
	create_info.pInputAssemblyState = &input_assembly_state;
	create_info.pTessellationState = NULL;
	create_info.pViewportState = &viewport_state;
	create_info.pRasterizationState = &rasterization_state;
	create_info.pMultisampleState = &multisample_state;
	create_info.pDepthStencilState = &depth_stencil_state;
	create_info.pColorBlendState = &blend_state;
	create_info.pDynamicState = &dynamic_state;
	create_info.layout = layout;
	create_info.renderPass = renderpass;
	create_info.subpass = 0;
	create_info.basePipelineHandle = VK_NULL_HANDLE;
	create_info.basePipelineIndex = -1;

	VK_POST_PROCESS_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &create_info, NULL, pipeline ) );

	VK_POST_PROCESS_SET_OBJECT_NAME( *pipeline, pipeline_name, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
}
