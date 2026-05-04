/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Vk_Pipeline_Def → VkPipeline factory, pipeline table alloc/gen/lookup.
Split from vk.c.
===========================================================================
*/

#include "tr_local.h"
#include "vk_create_pipeline.h"
#include "vk_pipeline_helpers.h"

/* r_hdr 3: 64-bit (RGBA64F) uses dvec4 fragment output; select HDR64 shaders when active */
static inline qboolean vk_hdr64_active( void )
{
	return vk.color_format == VK_FORMAT_R64G64B64A64_SFLOAT;
}

#ifdef USE_VK_PBR
static VkVertexInputBindingDescription bindings[10];
static VkVertexInputAttributeDescription attribs[10];
#else
static VkVertexInputBindingDescription bindings[8];
static VkVertexInputAttributeDescription attribs[8];
#endif
static uint32_t num_binds;
static uint32_t num_attrs;

static void push_bind( uint32_t binding, uint32_t stride )
{
	bindings[ num_binds ].binding = binding;
	bindings[ num_binds ].stride = stride;
	bindings[ num_binds ].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	num_binds++;
}

static void push_attr( uint32_t location, uint32_t binding, VkFormat format )
{
	attribs[ num_attrs ].location = location;
	attribs[ num_attrs ].binding = binding;
	attribs[ num_attrs ].format = format;
	attribs[ num_attrs ].offset = 0;
	num_attrs++;
}


#ifdef USE_VK_PBR
static VkShaderModule *vk_select_pbr_gen_vert( const Vk_Pipeline_Def *def, int use_pbr, int tx, int cl, int env )
{
	const int fog = def->fog_stage ? 1 : 0;
	if ( def->pbr_vert_mode && use_pbr ) {
		return &vk.modules.vert.gen_gltf_gpu[use_pbr][tx][cl][env][fog];
	}
	return &vk.modules.vert.gen[use_pbr][tx][cl][env][fog];
}
#endif

VkPipeline vk_create_pipeline( const Vk_Pipeline_Def *def, renderPass_t renderPassIndex, uint32_t def_index ) {
	(void)def_index; // unused parameter
	VkShaderModule *vs_module = NULL;
	VkShaderModule *fs_module = NULL;
	//int32_t vert_spec_data[1]; // clippping
	//VkSpecializationInfo vert_spec_info;
    struct Vk_Pipeline_FragSpecData frag_spec_data;

#ifdef USE_VK_PBR
	/* ADD_FRAG_SPEC: 11 base (0..10) + 30 PBR (constant_id 11..40, includes lightmap_scale/srgb at 32..33) = 41 entries.
	 * Was 38 → stack smash / SIGABRT in debug when vk_create_pipelines ran after VarInfo. */
	VkSpecializationMapEntry spec_entries[48];
	_Static_assert( sizeof( spec_entries ) / sizeof( spec_entries[0] ) >= 41u,
		"vk_create_pipeline: spec_entries[] too small for PBR fragment specialization map" );
#else
	VkSpecializationMapEntry spec_entries[12];
	_Static_assert( sizeof( spec_entries ) / sizeof( spec_entries[0] ) >= 11u,
		"vk_create_pipeline: spec_entries[] too small for non-PBR fragment specialization map" );
#endif
	
	VkSpecializationInfo frag_spec_info;
	VkPipelineVertexInputStateCreateInfo vertex_input_state;
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
	VkPipelineRasterizationStateCreateInfo rasterization_state;
	VkPipelineViewportStateCreateInfo viewport_state;
	VkPipelineMultisampleStateCreateInfo multisample_state;
	VkPipelineDepthStencilStateCreateInfo depth_stencil_state;
	VkPipelineColorBlendStateCreateInfo blend_state;
	VkPipelineColorBlendAttachmentState attachment_blend_state;
	VkPipelineColorBlendAttachmentState attachment_blend_states[2];
	VkPipelineDynamicStateCreateInfo dynamic_state;
	VkDynamicState dynamic_state_array[3] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	uint32_t main_dynamic_state_count = 2;
	if ( vk.colorWriteMaskDynamic ) {
		dynamic_state_array[main_dynamic_state_count++] = VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT;
	}
	VkGraphicsPipelineCreateInfo create_info;
	VkPipeline pipeline;
	VkPipelineShaderStageCreateInfo shader_stages[2];
	VkBool32 alphaToCoverage = VK_FALSE;
	VkBool32 main_motion_target = VK_FALSE;
	unsigned int atest_bits;
	unsigned int state_bits = def->state_bits;

#ifdef USE_VK_PBR
	const int use_pbr = def->vk_pbr_flags ? 1 : 0;

	switch ( def->shader_type ) {

		case TYPE_SIGNLE_TEXTURE_LIGHTING:
			vs_module = &vk.modules.vert.light[0];
			fs_module = vk_hdr64_active() ? &vk.modules.frag.light_hdr64[0][0] : &vk.modules.frag.light[0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_LIGHTING_LINEAR:
			vs_module = &vk.modules.vert.light[0];
			fs_module = vk_hdr64_active() ? &vk.modules.frag.light_hdr64[1][0] : &vk.modules.frag.light[1][0];
			break;

		case TYPE_SIGNLE_TEXTURE_DF:
			state_bits |= GLS_DEPTHMASK_TRUE;
			vs_module = &vk.modules.vert.ident1[use_pbr][0][0][0];
			fs_module = &vk.modules.frag.gen0_df;
			break;

		case TYPE_SIGNLE_TEXTURE_FIXED_COLOR:
			vs_module = &vk.modules.vert.fixed[use_pbr][0][0][0];
			fs_module = vk_hdr64_active() ? &vk.modules.frag.fixed_hdr64[use_pbr][0][0] : &vk.modules.frag.fixed[use_pbr][0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_FIXED_COLOR_ENV:
			vs_module = &vk.modules.vert.fixed[use_pbr][0][1][0];
			fs_module = vk_hdr64_active() ? &vk.modules.frag.fixed_hdr64[use_pbr][0][0] : &vk.modules.frag.fixed[use_pbr][0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_ENT_COLOR:
			vs_module = &vk.modules.vert.fixed[use_pbr][0][0][0];
			fs_module = vk_hdr64_active() ? &vk.modules.frag.ent_hdr64[use_pbr][0][0] : &vk.modules.frag.ent[use_pbr][0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_ENT_COLOR_ENV:
			vs_module = &vk.modules.vert.fixed[use_pbr][0][1][0];
			fs_module = vk_hdr64_active() ? &vk.modules.frag.ent_hdr64[use_pbr][0][0] : &vk.modules.frag.ent[use_pbr][0][0];
			break;

		case TYPE_SIGNLE_TEXTURE:
			if ( def->hasFlowmap && !use_pbr ) {
				const int fog = def->fog_stage ? 1 : 0;
				vs_module = &vk.modules.vert.gen[0][0][0][0][fog];
				fs_module = vk_hdr64_active() ? &vk.modules.frag.flowmap_hdr64[fog] : &vk.modules.frag.flowmap[fog];
			} else {
				vs_module = vk_select_pbr_gen_vert( def, use_pbr, 0, 0, 0 );
				fs_module = vk_hdr64_active() ? &vk.modules.frag.gen_hdr64[use_pbr][0][0][0] : &vk.modules.frag.gen[use_pbr][0][0][0];
			}
			break;

		case TYPE_SIGNLE_TEXTURE_ENV:
			vs_module = vk_select_pbr_gen_vert( def, use_pbr, 0, 0, 1 );
			fs_module = vk_hdr64_active() ? &vk.modules.frag.gen_hdr64[use_pbr][0][0][0] : &vk.modules.frag.gen[use_pbr][0][0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_IDENTITY:
			vs_module = &vk.modules.vert.ident1[use_pbr][0][0][0];
			fs_module = vk_hdr64_active() ? &vk.modules.frag.ident1_hdr64[use_pbr][0][0] : &vk.modules.frag.ident1[use_pbr][0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_IDENTITY_ENV:
			vs_module = &vk.modules.vert.ident1[use_pbr][0][1][0];
			fs_module = vk_hdr64_active() ? &vk.modules.frag.ident1_hdr64[use_pbr][0][0] : &vk.modules.frag.ident1[use_pbr][0][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY:
		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY:
			vs_module = &vk.modules.vert.ident1[use_pbr][1][0][0];
			fs_module = vk_hdr64_active() ? &vk.modules.frag.ident1_hdr64[use_pbr][1][0] : &vk.modules.frag.ident1[use_pbr][1][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV:
		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV:
			vs_module = &vk.modules.vert.ident1[use_pbr][1][1][0];
			fs_module = vk_hdr64_active() ? &vk.modules.frag.ident1_hdr64[use_pbr][1][0] : &vk.modules.frag.ident1[use_pbr][1][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR:
			vs_module = &vk.modules.vert.fixed[use_pbr][1][0][0];
			fs_module = vk_hdr64_active() ? &vk.modules.frag.fixed_hdr64[use_pbr][1][0] : &vk.modules.frag.fixed[use_pbr][1][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV:
			vs_module = &vk.modules.vert.fixed[use_pbr][1][1][0];
			fs_module = vk_hdr64_active() ? &vk.modules.frag.fixed_hdr64[use_pbr][1][0] : &vk.modules.frag.fixed[use_pbr][1][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL2:
		case TYPE_MULTI_TEXTURE_ADD2_1_1:
		case TYPE_MULTI_TEXTURE_ADD2:
			vs_module = vk_select_pbr_gen_vert( def, use_pbr, 1, 0, 0 );
			fs_module = vk_hdr64_active() ? &vk.modules.frag.gen_hdr64[use_pbr][1][0][0] : &vk.modules.frag.gen[use_pbr][1][0][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL2_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_ENV:
			vs_module = vk_select_pbr_gen_vert( def, use_pbr, 1, 0, 1 );
			fs_module = vk_hdr64_active() ? &vk.modules.frag.gen_hdr64[use_pbr][1][0][0] : &vk.modules.frag.gen[use_pbr][1][0][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL3:
		case TYPE_MULTI_TEXTURE_ADD3_1_1:
		case TYPE_MULTI_TEXTURE_ADD3:
			vs_module = vk_select_pbr_gen_vert( def, use_pbr, 2, 0, 0 );
			fs_module = vk_hdr64_active() ? &vk.modules.frag.gen_hdr64[use_pbr][2][0][0] : &vk.modules.frag.gen[use_pbr][2][0][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL3_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_ENV:
			vs_module = vk_select_pbr_gen_vert( def, use_pbr, 2, 0, 1 );
			fs_module = vk_hdr64_active() ? &vk.modules.frag.gen_hdr64[use_pbr][2][0][0] : &vk.modules.frag.gen[use_pbr][2][0][0];
			break;

		case TYPE_BLEND2_ADD:
		case TYPE_BLEND2_MUL:
		case TYPE_BLEND2_ALPHA:
		case TYPE_BLEND2_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_MIX_ALPHA:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA:
			vs_module = vk_select_pbr_gen_vert( def, use_pbr, 1, 1, 0 );
			fs_module = vk_hdr64_active() ? &vk.modules.frag.gen_hdr64[use_pbr][1][1][0] : &vk.modules.frag.gen[use_pbr][1][1][0];
			break;

		case TYPE_BLEND2_ADD_ENV:
		case TYPE_BLEND2_MUL_ENV:
		case TYPE_BLEND2_ALPHA_ENV:
		case TYPE_BLEND2_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND2_MIX_ALPHA_ENV:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV:
			vs_module = vk_select_pbr_gen_vert( def, use_pbr, 1, 1, 1 );
			fs_module = vk_hdr64_active() ? &vk.modules.frag.gen_hdr64[use_pbr][1][1][0] : &vk.modules.frag.gen[use_pbr][1][1][0];
			break;

		case TYPE_BLEND3_ADD:
		case TYPE_BLEND3_MUL:
		case TYPE_BLEND3_ALPHA:
		case TYPE_BLEND3_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_MIX_ALPHA:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA:
			vs_module = vk_select_pbr_gen_vert( def, use_pbr, 2, 1, 0 );
			fs_module = vk_hdr64_active() ? &vk.modules.frag.gen_hdr64[use_pbr][2][1][0] : &vk.modules.frag.gen[use_pbr][2][1][0];
			break;

		case TYPE_BLEND3_ADD_ENV:
		case TYPE_BLEND3_MUL_ENV:
		case TYPE_BLEND3_ALPHA_ENV:
		case TYPE_BLEND3_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV:
			vs_module = vk_select_pbr_gen_vert( def, use_pbr, 2, 1, 1 );
			fs_module = vk_hdr64_active() ? &vk.modules.frag.gen_hdr64[use_pbr][2][1][0] : &vk.modules.frag.gen[use_pbr][2][1][0];
			break;

		case TYPE_COLOR_BLACK:
		case TYPE_COLOR_WHITE:
		case TYPE_COLOR_GREEN:
		case TYPE_COLOR_RED:
			vs_module = &vk.modules.color_vs;
			fs_module = vk_hdr64_active() ? &vk.modules.color_fs_hdr64 : &vk.modules.color_fs;
			break;

		case TYPE_FOG_ONLY:
			vs_module = &vk.modules.fog_vs;
			fs_module = vk_hdr64_active() ? &vk.modules.fog_fs_hdr64 : &vk.modules.fog_fs;
			break;

		case TYPE_DOT:
			vs_module = &vk.modules.dot_vs;
			fs_module = vk_hdr64_active() ? &vk.modules.dot_fs_hdr64 : &vk.modules.dot_fs;
			break;

		case TYPE_OCCLUSION_BBOX:
			vs_module = &vk.modules.color_vs;
			fs_module = vk_hdr64_active() ? &vk.modules.color_fs_hdr64 : &vk.modules.color_fs;
			break;

		default:
			ri.Error(ERR_DROP, "create_pipeline: unknown shader type %i\n", def->shader_type);
			return 0;
	}
#else
	switch ( def->shader_type ) {

		case TYPE_SIGNLE_TEXTURE_LIGHTING:
			vs_module = &vk.modules.vert.light[0];
			fs_module = &vk.modules.frag.light[0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_LIGHTING_LINEAR:
			vs_module = &vk.modules.vert.light[0];
			fs_module = &vk.modules.frag.light[1][0];
			break;

		case TYPE_SIGNLE_TEXTURE_DF:
			state_bits |= GLS_DEPTHMASK_TRUE;
			vs_module = &vk.modules.vert.ident1[0][0][0];
			fs_module = &vk.modules.frag.gen0_df;
			break;

		case TYPE_SIGNLE_TEXTURE_FIXED_COLOR:
			vs_module = &vk.modules.vert.fixed[0][0][0];
			fs_module = &vk.modules.frag.fixed[0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_FIXED_COLOR_ENV:
			vs_module = &vk.modules.vert.fixed[0][1][0];
			fs_module = &vk.modules.frag.fixed[0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_ENT_COLOR:
			vs_module = &vk.modules.vert.fixed[0][0][0];
			fs_module = &vk.modules.frag.ent[0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_ENT_COLOR_ENV:
			vs_module = &vk.modules.vert.fixed[0][1][0];
			fs_module = &vk.modules.frag.ent[0][0];
			break;

		case TYPE_SIGNLE_TEXTURE:
			if ( def->hasFlowmap ) {
				const int fog = def->fog_stage ? 1 : 0;
				vs_module = &vk.modules.vert.gen[0][0][0][fog];
				fs_module = &vk.modules.frag.flowmap[fog];
			} else {
				vs_module = &vk.modules.vert.gen[0][0][0][0];
				fs_module = &vk.modules.frag.gen[0][0][0];
			}
			break;

		case TYPE_SIGNLE_TEXTURE_ENV:
			vs_module = &vk.modules.vert.gen[0][0][1][0];
			fs_module = &vk.modules.frag.gen[0][0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_IDENTITY:
			vs_module = &vk.modules.vert.ident1[0][0][0];
			fs_module = &vk.modules.frag.ident1[0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_IDENTITY_ENV:
			vs_module = &vk.modules.vert.ident1[0][1][0];
			fs_module = &vk.modules.frag.ident1[0][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY:
		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY:
			vs_module = &vk.modules.vert.ident1[1][0][0];
			fs_module = &vk.modules.frag.ident1[1][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV:
		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV:
			vs_module = &vk.modules.vert.ident1[1][1][0];
			fs_module = &vk.modules.frag.ident1[1][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR:
			vs_module = &vk.modules.vert.fixed[1][0][0];
			fs_module = &vk.modules.frag.fixed[1][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV:
			vs_module = &vk.modules.vert.fixed[1][1][0];
			fs_module = &vk.modules.frag.fixed[1][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL2:
		case TYPE_MULTI_TEXTURE_ADD2_1_1:
		case TYPE_MULTI_TEXTURE_ADD2:
			vs_module = &vk.modules.vert.gen[1][0][0][0];
			fs_module = &vk.modules.frag.gen[1][0][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL2_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_ENV:
			vs_module = &vk.modules.vert.gen[1][0][1][0];
			fs_module = &vk.modules.frag.gen[1][0][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL3:
		case TYPE_MULTI_TEXTURE_ADD3_1_1:
		case TYPE_MULTI_TEXTURE_ADD3:
			vs_module = &vk.modules.vert.gen[2][0][0][0];
			fs_module = &vk.modules.frag.gen[2][0][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL3_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_ENV:
			vs_module = &vk.modules.vert.gen[2][0][1][0];
			fs_module = &vk.modules.frag.gen[2][0][0];
			break;

		case TYPE_BLEND2_ADD:
		case TYPE_BLEND2_MUL:
		case TYPE_BLEND2_ALPHA:
		case TYPE_BLEND2_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_MIX_ALPHA:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA:
			vs_module = &vk.modules.vert.gen[1][1][0][0];
			fs_module = &vk.modules.frag.gen[1][1][0];
			break;

		case TYPE_BLEND2_ADD_ENV:
		case TYPE_BLEND2_MUL_ENV:
		case TYPE_BLEND2_ALPHA_ENV:
		case TYPE_BLEND2_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND2_MIX_ALPHA_ENV:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV:
			vs_module = &vk.modules.vert.gen[1][1][1][0];
			fs_module = &vk.modules.frag.gen[1][1][0];
			break;

		case TYPE_BLEND3_ADD:
		case TYPE_BLEND3_MUL:
		case TYPE_BLEND3_ALPHA:
		case TYPE_BLEND3_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_MIX_ALPHA:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA:
			vs_module = &vk.modules.vert.gen[2][1][0][0];
			fs_module = &vk.modules.frag.gen[2][1][0];
			break;

		case TYPE_BLEND3_ADD_ENV:
		case TYPE_BLEND3_MUL_ENV:
		case TYPE_BLEND3_ALPHA_ENV:
		case TYPE_BLEND3_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV:
			vs_module = &vk.modules.vert.gen[2][1][1][0];
			fs_module = &vk.modules.frag.gen[2][1][0];
			break;

		case TYPE_COLOR_BLACK:
		case TYPE_COLOR_WHITE:
		case TYPE_COLOR_GREEN:
		case TYPE_COLOR_RED:
			vs_module = &vk.modules.color_vs;
			fs_module = &vk.modules.color_fs;
			break;

		case TYPE_FOG_ONLY:
			vs_module = &vk.modules.fog_vs;
			fs_module = &vk.modules.fog_fs;
			break;

		case TYPE_DOT:
			vs_module = &vk.modules.dot_vs;
			fs_module = &vk.modules.dot_fs;
			break;

		case TYPE_OCCLUSION_BBOX:
			vs_module = &vk.modules.color_vs;
			fs_module = &vk.modules.color_fs;
			break;

		default:
			ri.Error(ERR_DROP, "create_pipeline: unknown shader type %i\n", def->shader_type);
			return 0;
	}
#endif
	

	if ( def->fog_stage ) {
		switch ( def->shader_type ) {
			case TYPE_FOG_ONLY:
			case TYPE_DOT:
			case TYPE_SIGNLE_TEXTURE_DF:
			case TYPE_OCCLUSION_BBOX:
			case TYPE_COLOR_BLACK:
			case TYPE_COLOR_WHITE:
			case TYPE_COLOR_GREEN:
			case TYPE_COLOR_RED:
				break;
			default:
				// switch to fogged modules
				vs_module++;
				fs_module++;
				break;
		}
	}

	vk_set_shader_stage_desc(shader_stages+0, VK_SHADER_STAGE_VERTEX_BIT, *vs_module, "main");
	vk_set_shader_stage_desc(shader_stages+1, VK_SHADER_STAGE_FRAGMENT_BIT, *fs_module, "main");

	//Com_Memset( vert_spec_data, 0, sizeof( vert_spec_data ) );
	Com_Memset( &frag_spec_data, 0, sizeof( frag_spec_data ) );

	//vert_spec_data[0] = def->clipping_plane ? 1 : 0;

	// fragment shader specialization data
	atest_bits = state_bits & GLS_ATEST_BITS;
	switch ( atest_bits ) {
        case GLS_ATEST_GT_0:
            frag_spec_data.alpha_test_func = 1; // not equal
            frag_spec_data.alpha_test_value = 0.0f;
            break;
        case GLS_ATEST_LT_80:
            frag_spec_data.alpha_test_func = 2; // less than
            frag_spec_data.alpha_test_value = 0.5f;
            break;
        case GLS_ATEST_GE_80:
            frag_spec_data.alpha_test_func = 3; // greater or equal
            frag_spec_data.alpha_test_value = 0.5f;
            break;
        default:
            frag_spec_data.alpha_test_func = 0;
            frag_spec_data.alpha_test_value = 0.0f;
            break;
	};

	// depth fragment threshold
	frag_spec_data.depth_fragment = 0.85f;
#if 0
	if ( r_ext_alpha_to_coverage->integer && vk_get_main_rasterization_samples() != VK_SAMPLE_COUNT_1_BIT && frag_spec_data.alpha_test_func ) {
		frag_spec_data.alpha_to_coverage = 1;
		alphaToCoverage = VK_TRUE;
	}
#endif

    // constant color
    switch ( def->shader_type ) {
        default: frag_spec_data.color_mode = 0; break;
		case TYPE_COLOR_WHITE: frag_spec_data.color_mode = 1; break;
        case TYPE_COLOR_GREEN: frag_spec_data.color_mode = 2; break;
        case TYPE_COLOR_RED:   frag_spec_data.color_mode = 3; break;
    }

    // abs lighting
    switch ( def->shader_type ) {
		case TYPE_SIGNLE_TEXTURE_LIGHTING:
		case TYPE_SIGNLE_TEXTURE_LIGHTING_LINEAR:
            frag_spec_data.abs_light = def->abs_light ? 1 : 0;
        default:
        break;
    }

	// multutexture mode
	switch ( def->shader_type ) {
		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY:
		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV:
		case TYPE_MULTI_TEXTURE_MUL2:
		case TYPE_MULTI_TEXTURE_MUL2_ENV:
		case TYPE_MULTI_TEXTURE_MUL3:
		case TYPE_MULTI_TEXTURE_MUL3_ENV:
		case TYPE_BLEND2_MUL:
		case TYPE_BLEND2_MUL_ENV:
		case TYPE_BLEND3_MUL:
		case TYPE_BLEND3_MUL_ENV:
			frag_spec_data.tex_mode = 0;
			break;

		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY:
		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR:
		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_1_1:
		case TYPE_MULTI_TEXTURE_ADD2_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_1_1:
		case TYPE_MULTI_TEXTURE_ADD3_1_1_ENV:
			frag_spec_data.tex_mode = 1;
			break;

		case TYPE_MULTI_TEXTURE_ADD2:
		case TYPE_MULTI_TEXTURE_ADD2_ENV:
		case TYPE_MULTI_TEXTURE_ADD3:
		case TYPE_MULTI_TEXTURE_ADD3_ENV:
		case TYPE_BLEND2_ADD:
		case TYPE_BLEND2_ADD_ENV:
		case TYPE_BLEND3_ADD:
		case TYPE_BLEND3_ADD_ENV:
			frag_spec_data.tex_mode = 2;
			break;

		case TYPE_BLEND2_ALPHA:
		case TYPE_BLEND2_ALPHA_ENV:
		case TYPE_BLEND3_ALPHA:
		case TYPE_BLEND3_ALPHA_ENV:
			frag_spec_data.tex_mode = 3;
			break;

		case TYPE_BLEND2_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_ONE_MINUS_ALPHA_ENV:
			frag_spec_data.tex_mode = 4;
			break;

		case TYPE_BLEND2_MIX_ALPHA:
		case TYPE_BLEND2_MIX_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ALPHA:
		case TYPE_BLEND3_MIX_ALPHA_ENV:
			frag_spec_data.tex_mode = 5;
			break;

		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV:
			frag_spec_data.tex_mode = 6;
			break;

		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV:
			frag_spec_data.tex_mode = 7;
			break;

		default:
			break;
	}

	frag_spec_data.identity_color = ((float)def->color.rgb) / 255.0;
	frag_spec_data.identity_alpha = ((float)def->color.alpha) / 255.0;

	if ( def->fog_stage ) {
		frag_spec_data.acff = def->acff;
	} else {
		frag_spec_data.acff = 0;
	}

	//
	// vertex module specialization data
	//
#if 0
	spec_entries[0].constantID = 0; // clip_plane
	spec_entries[0].offset = 0 * sizeof( int32_t );
	spec_entries[0].size = sizeof( int32_t );

	vert_spec_info.mapEntryCount = 1;
	vert_spec_info.pMapEntries = spec_entries + 0;
	vert_spec_info.dataSize = 1 * sizeof( int32_t );
	vert_spec_info.pData = &vert_spec_data[0];
	shader_stages[0].pSpecializationInfo = &vert_spec_info;
#endif
	shader_stages[0].pSpecializationInfo = NULL;

	//
	// fragment module specialization data
	//
	Com_Memset( spec_entries, 0, sizeof( spec_entries ) );
	int spec_entry_count = 0;
#define ADD_FRAG_SPEC(cid, field) do { \
		spec_entries[spec_entry_count].constantID = (cid); \
		spec_entries[spec_entry_count].offset = offsetof( struct Vk_Pipeline_FragSpecData, field ); \
		spec_entries[spec_entry_count].size = sizeof( frag_spec_data.field ); \
		spec_entry_count++; \
	} while ( 0 )

	ADD_FRAG_SPEC( 0, alpha_test_func );
	ADD_FRAG_SPEC( 1, alpha_test_value );
	ADD_FRAG_SPEC( 2, depth_fragment );
	ADD_FRAG_SPEC( 3, alpha_to_coverage );
	ADD_FRAG_SPEC( 4, color_mode );
	ADD_FRAG_SPEC( 5, abs_light );
	ADD_FRAG_SPEC( 6, tex_mode );
	ADD_FRAG_SPEC( 7, discard_mode );
	ADD_FRAG_SPEC( 8, identity_color );
	ADD_FRAG_SPEC( 9, identity_alpha );
	ADD_FRAG_SPEC( 10, acff );

#ifdef USE_VK_PBR
	ADD_FRAG_SPEC( 11, specularScale_x );
	ADD_FRAG_SPEC( 12, specularScale_y );
	ADD_FRAG_SPEC( 13, specularScale_z );
	ADD_FRAG_SPEC( 14, specularScale_w );
	ADD_FRAG_SPEC( 15, normalScale_x );
	ADD_FRAG_SPEC( 16, normalScale_y );
	ADD_FRAG_SPEC( 17, normalScale_z );
	ADD_FRAG_SPEC( 18, normalScale_w );
	/* constant_id order must match gen_frag.tmpl (19..40) */
	ADD_FRAG_SPEC( 19, normal_texture_set );
	ADD_FRAG_SPEC( 20, physical_texture_set );
	ADD_FRAG_SPEC( 21, env_texture_set );
	ADD_FRAG_SPEC( 22, lightmap_texture_set );
	ADD_FRAG_SPEC( 23, irradiance_texture_set );
	ADD_FRAG_SPEC( 24, emissive_texture_set );
	ADD_FRAG_SPEC( 25, clearcoat_texture_set );
	ADD_FRAG_SPEC( 26, sheen_texture_set );
	ADD_FRAG_SPEC( 27, anisotropy_texture_set );
	ADD_FRAG_SPEC( 28, transmission_texture_set );
	ADD_FRAG_SPEC( 29, subsurface_texture_set );
	ADD_FRAG_SPEC( 30, deluxe_mapping );
	ADD_FRAG_SPEC( 31, deluxe_specular_scale );
	ADD_FRAG_SPEC( 32, lightmap_scale );
	ADD_FRAG_SPEC( 33, lightmap_srgb_decode );
	ADD_FRAG_SPEC( 34, detail_texture_set );
	ADD_FRAG_SPEC( 35, detail_scale );
	ADD_FRAG_SPEC( 36, pom_height_source );
	ADD_FRAG_SPEC( 37, pom_enabled );
	ADD_FRAG_SPEC( 38, pom_max_steps );
	ADD_FRAG_SPEC( 39, parallax_bias_shader );
	ADD_FRAG_SPEC( 40, forward_plus_shade_strength );

	// only use w value, specgloss maps are not supported
	frag_spec_data.specularScale_x = def->specularScale[0];
	frag_spec_data.specularScale_y = def->specularScale[1];
	frag_spec_data.specularScale_z = def->specularScale[2];
	frag_spec_data.specularScale_w = def->specularScale[3];

	frag_spec_data.normalScale_x = def->normalScale[0];
	frag_spec_data.normalScale_y = def->normalScale[1];
	frag_spec_data.normalScale_z = def->normalScale[2];
	frag_spec_data.normalScale_w = def->normalScale[3];

	frag_spec_data.normal_texture_set = -1;
	frag_spec_data.physical_texture_set = -1;
	frag_spec_data.env_texture_set = -1;
	frag_spec_data.lightmap_texture_set = -1;
	frag_spec_data.irradiance_texture_set = -1;
	frag_spec_data.emissive_texture_set = -1;
	frag_spec_data.clearcoat_texture_set = -1;
	frag_spec_data.sheen_texture_set = -1;
	frag_spec_data.anisotropy_texture_set = -1;
	frag_spec_data.transmission_texture_set = -1;
	frag_spec_data.subsurface_texture_set = -1;
	frag_spec_data.detail_texture_set = -1;
	frag_spec_data.detail_scale = ( r_detail_scale && r_detail_scale->value > 0.0f ) ? r_detail_scale->value : 4.0f;

	frag_spec_data.pom_height_source = 0;
	frag_spec_data.pom_enabled = 0;
	frag_spec_data.pom_max_steps = 16;
	frag_spec_data.parallax_bias_shader = def->parallaxBias;
	frag_spec_data.forward_plus_shade_strength = ( r_forwardPlusShade && r_forwardPlusShade->value > 0.0f )
		? Com_Clamp( 0.0f, 4.0f, r_forwardPlusShade->value ) : 0.0f;

	if ( def->vk_pbr_flags & PBR_HAS_NORMALMAP )
		frag_spec_data.normal_texture_set = 0;

	if ( def->vk_pbr_flags & PBR_HAS_SPECULARMAP )
		frag_spec_data.physical_texture_set = 1;
	else if ( def->vk_pbr_flags & PBR_HAS_PHYSICALMAP )
		frag_spec_data.physical_texture_set = 0;

	if ( vk.cubemapActive )
		frag_spec_data.env_texture_set = 0;

	if ( def->vk_pbr_flags & PBR_HAS_LIGHTMAP )
		frag_spec_data.lightmap_texture_set = def->lightmap_bundle;

	if ( ( def->vk_pbr_flags & PBR_HAS_NORMALMAP ) && r_pom && r_pom->integer ) {
		const qboolean heightFromOrm = ( ( def->vk_pbr_flags & PBR_HAS_PHYSICALMAP ) != 0 ) &&
			( frag_spec_data.physical_texture_set == 0 );
		const qboolean heightFromNormal = ( def->pom_height_source == 1 );

		if ( heightFromOrm || heightFromNormal ) {
			frag_spec_data.pom_enabled = 1;
			frag_spec_data.pom_max_steps = r_pomSteps ? Com_Clamp( 4, 64, r_pomSteps->integer ) : 16;
			frag_spec_data.pom_height_source = heightFromOrm ? 0 : 1;
		}
	}

	if ( def->vk_pbr_flags & PBR_HAS_IRRADIANCE )
		frag_spec_data.irradiance_texture_set = 0;

	if ( def->vk_pbr_flags & PBR_HAS_EMISSIVE )
		frag_spec_data.emissive_texture_set = 0;

	if ( def->vk_pbr_flags & PBR_HAS_CLEARCOAT )
		frag_spec_data.clearcoat_texture_set = 0;

	if ( def->vk_pbr_flags & PBR_HAS_SHEEN )
		frag_spec_data.sheen_texture_set = 0;

	if ( def->vk_pbr_flags & PBR_HAS_ANISOTROPY )
		frag_spec_data.anisotropy_texture_set = 0;

	if ( def->vk_pbr_flags & PBR_HAS_TRANSMISSION )
		frag_spec_data.transmission_texture_set = 0;

	if ( def->vk_pbr_flags & PBR_HAS_SUBSURFACE )
		frag_spec_data.subsurface_texture_set = 0;

	if ( def->vk_pbr_flags & PBR_HAS_DETAILMAP )
		frag_spec_data.detail_texture_set = 0;
	if ( r_deluxeMapping->integer )
	{
		// deluxe_texture_set = 0: use approx + scale
		frag_spec_data.deluxe_mapping = 0;
		frag_spec_data.deluxe_specular_scale = r_deluxeSpecular->value;

		// enabled+: use deluxe map
		if ( def->vk_pbr_flags & (PBR_HAS_DELUXEMAP0 | PBR_HAS_DELUXEMAP1) )
			frag_spec_data.deluxe_mapping = 1;
	}
	else
	{
		// use approx + default scale
		// perhaps when r_specularMapping = 0 set scale to 0 to disable it?
		frag_spec_data.deluxe_mapping = -1;
		frag_spec_data.deluxe_specular_scale = 1.0f;
	}
#endif

	frag_spec_data.lightmap_scale = ( r_hdr_lightmap_scale && r_hdr_lightmap_scale->value > 0.0f ) ?
		r_hdr_lightmap_scale->value : 1.0f;

	frag_spec_data.lightmap_srgb_decode = ( r_lightmap_srgb_decode && r_lightmap_srgb_decode->integer && r_hdr && r_hdr->integer > 0 ) ? 1 : 0;

	if ( spec_entry_count > (int)( sizeof( spec_entries ) / sizeof( spec_entries[0] ) ) ) {
		ri.Error( ERR_FATAL, "vk_create_pipeline: fragment specialization map overflow (%d > %u)",
			spec_entry_count, (unsigned int)( sizeof( spec_entries ) / sizeof( spec_entries[0] ) ) );
	}

	frag_spec_info.mapEntryCount = spec_entry_count;
	frag_spec_info.pMapEntries = spec_entries;
	frag_spec_info.dataSize = sizeof( frag_spec_data );
	frag_spec_info.pData = &frag_spec_data;
	shader_stages[1].pSpecializationInfo = &frag_spec_info;
#undef ADD_FRAG_SPEC


	//
	// Vertex input
	//
	num_binds = num_attrs = 0;
	qboolean has_normal = qfalse;

	switch ( def->shader_type ) {

		case TYPE_FOG_ONLY:
		case TYPE_DOT:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_COLOR_BLACK:
		case TYPE_COLOR_WHITE:
		case TYPE_COLOR_GREEN:
		case TYPE_COLOR_RED:
		case TYPE_OCCLUSION_BBOX:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_SIGNLE_TEXTURE_DF:
		case TYPE_SIGNLE_TEXTURE_IDENTITY:
		case TYPE_SIGNLE_TEXTURE_FIXED_COLOR:
		case TYPE_SIGNLE_TEXTURE_ENT_COLOR:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			break;

		case TYPE_SIGNLE_TEXTURE:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			break;

		case TYPE_SIGNLE_TEXTURE_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			//push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			//push_attr( 2, 2, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );

			has_normal = qtrue;
			break;

		case TYPE_SIGNLE_TEXTURE_IDENTITY_ENV:
		case TYPE_SIGNLE_TEXTURE_FIXED_COLOR_ENV:
		case TYPE_SIGNLE_TEXTURE_ENT_COLOR_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );

			has_normal = qtrue;
			break;

		case TYPE_SIGNLE_TEXTURE_LIGHTING:
		case TYPE_SIGNLE_TEXTURE_LIGHTING_LINEAR:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( vec2_t ) );					// st0 array
			push_bind( 2, sizeof( vec4_t ) );					// normals array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 2, 2, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY:
		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR:
		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			break;

		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );

			has_normal = qtrue;
			break;

		case TYPE_MULTI_TEXTURE_MUL2:
		case TYPE_MULTI_TEXTURE_ADD2_1_1:
		case TYPE_MULTI_TEXTURE_ADD2:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			break;

		case TYPE_MULTI_TEXTURE_MUL2_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			//push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			//push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );

			has_normal = qtrue;
			break;

		case TYPE_MULTI_TEXTURE_MUL3:
		case TYPE_MULTI_TEXTURE_ADD3_1_1:
		case TYPE_MULTI_TEXTURE_ADD3:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 4, sizeof( vec2_t ) );					// st2 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 4, 4, VK_FORMAT_R32G32_SFLOAT );
			break;

		case TYPE_MULTI_TEXTURE_MUL3_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			//push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 4, sizeof( vec2_t ) );					// st2 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			//push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 4, 4, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );

			has_normal = qtrue;
			break;

		case TYPE_BLEND2_ADD:
		case TYPE_BLEND2_MUL:
		case TYPE_BLEND2_ALPHA:
		case TYPE_BLEND2_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_MIX_ALPHA:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color0 array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 6, sizeof( color4ub_t ) );				// color1 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 6, 6, VK_FORMAT_R8G8B8A8_UNORM );
			break;

		case TYPE_BLEND2_ADD_ENV:
		case TYPE_BLEND2_MUL_ENV:
		case TYPE_BLEND2_ALPHA_ENV:
		case TYPE_BLEND2_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND2_MIX_ALPHA_ENV:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color0 array
			//push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_bind( 6, sizeof( color4ub_t ) );				// color1 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			//push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 6, 6, VK_FORMAT_R8G8B8A8_UNORM );

			has_normal = qtrue;
			break;

		case TYPE_BLEND3_ADD:
		case TYPE_BLEND3_MUL:
		case TYPE_BLEND3_ALPHA:
		case TYPE_BLEND3_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_MIX_ALPHA:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color0 array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 4, sizeof( vec2_t ) );					// st2 array
			push_bind( 6, sizeof( color4ub_t ) );				// color1 array
			push_bind( 7, sizeof( color4ub_t ) );				// color2 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 4, 4, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 6, 6, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 7, 7, VK_FORMAT_R8G8B8A8_UNORM );
			break;

		case TYPE_BLEND3_ADD_ENV:
		case TYPE_BLEND3_MUL_ENV:
		case TYPE_BLEND3_ALPHA_ENV:
		case TYPE_BLEND3_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color0 array
			//push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 4, sizeof( vec2_t ) );					// st2 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_bind( 6, sizeof( color4ub_t ) );				// color1 array
			push_bind( 7, sizeof( color4ub_t ) );				// color2 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			//push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 4, 4, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 6, 6, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 7, 7, VK_FORMAT_R8G8B8A8_UNORM );

			has_normal = qtrue;
			break;

		default:
                        ri.Error( ERR_DROP, "%s: invalid shader type - %i", __func__, def->shader_type );
			break;
	}

 #ifdef USE_VK_PBR  
    if( def->vk_pbr_flags ){   

		if ( !has_normal )
		{
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
		}

		if ( def->pbr_vert_mode ) {
			push_bind( 15, 4 ); /* glTF joint indices */
			push_attr( 15, 15, VK_FORMAT_R8G8B8A8_UINT );
			push_bind( 16, sizeof( vec4_t ) ); /* glTF joint weights */
			push_attr( 16, 16, VK_FORMAT_R32G32B32A32_SFLOAT );
		}

        push_bind( 8, sizeof( vec4_t ) );						// tangent
        push_attr( 8, 8, VK_FORMAT_R32G32B32A32_SFLOAT );

        push_bind( 9, sizeof(vec4_t) );							// lightdir
        push_attr( 9, 9, VK_FORMAT_R32G32B32A32_SFLOAT );
    }
#endif

	vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_state.pNext = NULL;
	vertex_input_state.flags = 0;
	vertex_input_state.pVertexBindingDescriptions = bindings;
	vertex_input_state.pVertexAttributeDescriptions = attribs;
	vertex_input_state.vertexBindingDescriptionCount = num_binds;
	vertex_input_state.vertexAttributeDescriptionCount = num_attrs;

	//
	// Primitive assembly.
	//
	input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_state.pNext = NULL;
	input_assembly_state.flags = 0;
	input_assembly_state.primitiveRestartEnable = VK_FALSE;

	switch ( def->primitives ) {
		case LINE_LIST: input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST; break;
		case POINT_LIST: input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; break;
		case TRIANGLE_STRIP: input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; break;
		default: input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; break;
	}

	//
	// Viewport.
	//
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.pNext = NULL;
	viewport_state.flags = 0;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = NULL; // dynamic viewport state
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = NULL; // dynamic scissor state

	//
	// Rasterization.
	//
	rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization_state.pNext = NULL;
	rasterization_state.flags = 0;
	rasterization_state.depthClampEnable = VK_FALSE;
	rasterization_state.rasterizerDiscardEnable = VK_FALSE;
	if ( def->shader_type == TYPE_DOT ) {
		rasterization_state.polygonMode = VK_POLYGON_MODE_POINT;
	} else {
		rasterization_state.polygonMode = (state_bits & GLS_POLYMODE_LINE) ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
	}

	switch ( def->face_culling ) {
		case CT_TWO_SIDED:
			rasterization_state.cullMode = VK_CULL_MODE_NONE;
			break;
		case CT_FRONT_SIDED:
			rasterization_state.cullMode = (def->mirror ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_BACK_BIT);
			break;
		case CT_BACK_SIDED:
			rasterization_state.cullMode = (def->mirror ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_FRONT_BIT);
			break;
		default:
			ri.Error( ERR_DROP, "create_pipeline: invalid face culling mode %i\n", def->face_culling );
			break;
	}

	rasterization_state.frontFace = VK_FRONT_FACE_CLOCKWISE; // Q3 defaults to clockwise vertex order

	 // depth bias state
	if ( def->polygon_offset ) {
		rasterization_state.depthBiasEnable = VK_TRUE;
		rasterization_state.depthBiasClamp = 0.0f;
		if ( def->shadow_phase == SHADOW_EDGES ) {
			/* Shadow volumes: push geometry forward (toward camera) to avoid z-fight with
			 * object silhouette. Back direction caused large black triangles. */
			cvar_t *sv_factor = ri.Cvar_Get( "r_shadowVolumeOffsetFactor", "1", CVAR_ARCHIVE_ND );
			cvar_t *sv_units = ri.Cvar_Get( "r_shadowVolumeOffsetUnits", "1", CVAR_ARCHIVE_ND );
			float factor = sv_factor ? sv_factor->value : 1.0f;
			float units = sv_units ? sv_units->value : 1.0f;
#ifdef USE_REVERSED_DEPTH
			rasterization_state.depthBiasConstantFactor = units;
			rasterization_state.depthBiasSlopeFactor = factor;
#else
			rasterization_state.depthBiasConstantFactor = -units;
			rasterization_state.depthBiasSlopeFactor = -factor;
#endif
		} else {
#ifdef USE_REVERSED_DEPTH
			rasterization_state.depthBiasConstantFactor = -r_offsetUnits->value;
			rasterization_state.depthBiasSlopeFactor = -r_offsetFactor->value;
#else
			rasterization_state.depthBiasConstantFactor = r_offsetUnits->value;
			rasterization_state.depthBiasSlopeFactor = r_offsetFactor->value;
#endif
		}
	} else {
		rasterization_state.depthBiasEnable = VK_FALSE;
		rasterization_state.depthBiasClamp = 0.0f;
		rasterization_state.depthBiasConstantFactor = 0.0f;
		rasterization_state.depthBiasSlopeFactor = 0.0f;
	}

	if ( def->line_width )
		rasterization_state.lineWidth = (float)def->line_width;
	else
		rasterization_state.lineWidth = 1.0f;

	multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_state.pNext = NULL;
	multisample_state.flags = 0;

	if ( renderPassIndex == RENDER_PASS_SCREENMAP ) {
		multisample_state.rasterizationSamples = vk.screenMapSamples;
	} else if ( renderPassIndex == RENDER_PASS_SUN_SHADOW ) {
		multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	} else {
		multisample_state.rasterizationSamples = vk_get_main_rasterization_samples();
	}

	multisample_state.sampleShadingEnable = ( vk.msaaSampleShading && multisample_state.rasterizationSamples != VK_SAMPLE_COUNT_1_BIT ) ? VK_TRUE : VK_FALSE;
	multisample_state.minSampleShading = vk_get_msaa_min_sample_shading();
	multisample_state.pSampleMask = NULL;
	multisample_state.alphaToCoverageEnable = alphaToCoverage;
	multisample_state.alphaToOneEnable = VK_FALSE;

	Com_Memset( &depth_stencil_state, 0, sizeof( depth_stencil_state ) );

	depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil_state.pNext = NULL;
	depth_stencil_state.flags = 0;
	depth_stencil_state.depthTestEnable = (state_bits & GLS_DEPTHTEST_DISABLE) ? VK_FALSE : VK_TRUE;
	depth_stencil_state.depthWriteEnable = ( def->shader_type == TYPE_OCCLUSION_BBOX ) ? VK_FALSE : ( (state_bits & GLS_DEPTHMASK_TRUE) ? VK_TRUE : VK_FALSE );
#ifdef USE_REVERSED_DEPTH
	depth_stencil_state.depthCompareOp = (state_bits & GLS_DEPTHFUNC_EQUAL) ? VK_COMPARE_OP_EQUAL : VK_COMPARE_OP_GREATER_OR_EQUAL;
#else
	depth_stencil_state.depthCompareOp = (state_bits & GLS_DEPTHFUNC_EQUAL) ? VK_COMPARE_OP_EQUAL : VK_COMPARE_OP_LESS_OR_EQUAL;
#endif
	depth_stencil_state.depthBoundsTestEnable = VK_FALSE;
	depth_stencil_state.stencilTestEnable = (def->shadow_phase != SHADOW_DISABLED) ? VK_TRUE : VK_FALSE;

	if (def->shadow_phase == SHADOW_EDGES) {
		depth_stencil_state.front.failOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.passOp = (def->face_culling == CT_FRONT_SIDED) ? VK_STENCIL_OP_INCREMENT_AND_CLAMP : VK_STENCIL_OP_DECREMENT_AND_CLAMP;
		depth_stencil_state.front.depthFailOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.compareOp = VK_COMPARE_OP_ALWAYS;
		depth_stencil_state.front.compareMask = 255;
		depth_stencil_state.front.writeMask = 255;
		depth_stencil_state.front.reference = 0;

		depth_stencil_state.back = depth_stencil_state.front;

	} else if (def->shadow_phase == SHADOW_FS_QUAD) {
		depth_stencil_state.front.failOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.passOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.depthFailOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.compareOp = VK_COMPARE_OP_NOT_EQUAL;
		depth_stencil_state.front.compareMask = 255;
		depth_stencil_state.front.writeMask = 255;
		depth_stencil_state.front.reference = 0;

		depth_stencil_state.back = depth_stencil_state.front;
	}

	depth_stencil_state.minDepthBounds = 0.0f;
	depth_stencil_state.maxDepthBounds = 1.0f;

	Com_Memset(&attachment_blend_state, 0, sizeof(attachment_blend_state));
	attachment_blend_state.blendEnable = (state_bits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)) ? VK_TRUE : VK_FALSE;

	if (def->shadow_phase == SHADOW_EDGES || def->shader_type == TYPE_SIGNLE_TEXTURE_DF || def->shader_type == TYPE_OCCLUSION_BBOX)
		attachment_blend_state.colorWriteMask = 0;
	else
		attachment_blend_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	if (attachment_blend_state.blendEnable) {
		switch (state_bits & GLS_SRCBLEND_BITS) {
			case GLS_SRCBLEND_ZERO:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
				break;
			case GLS_SRCBLEND_ONE:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
				break;
			case GLS_SRCBLEND_DST_COLOR:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
				break;
			case GLS_SRCBLEND_SRC_ALPHA:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_DST_ALPHA:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ALPHA_SATURATE:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
				break;
			default:
				ri.Error( ERR_DROP, "create_pipeline: invalid src blend state bits\n" );
				break;
		}
		switch (state_bits & GLS_DSTBLEND_BITS) {
			case GLS_DSTBLEND_ZERO:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
				break;
			case GLS_DSTBLEND_ONE:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
				break;
			case GLS_DSTBLEND_SRC_COLOR:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
				break;
			case GLS_DSTBLEND_SRC_ALPHA:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_DST_ALPHA:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
				break;
			default:
				ri.Error( ERR_DROP, "create_pipeline: invalid dst blend state bits\n" );
				break;
		}

		attachment_blend_state.srcAlphaBlendFactor = attachment_blend_state.srcColorBlendFactor;
		attachment_blend_state.dstAlphaBlendFactor = attachment_blend_state.dstColorBlendFactor;
		attachment_blend_state.colorBlendOp = VK_BLEND_OP_ADD;
		attachment_blend_state.alphaBlendOp = VK_BLEND_OP_ADD;
		if ( renderPassIndex == RENDER_PASS_UI_OVERLAY &&
			attachment_blend_state.srcColorBlendFactor == VK_BLEND_FACTOR_SRC_ALPHA &&
			attachment_blend_state.dstColorBlendFactor == VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA ) {
			/* Preserve overlay coverage so the final composite can alpha-blend on top of the
			 * already-tonemapped scene. RGB stays premultiplied; alpha tracks accumulated coverage. */
			attachment_blend_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			attachment_blend_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		}

		if ( def->allow_discard ) {
			// try to reduce pixel fillrate for transparent surfaces, this yields 1..10% fps increase when multisampling in enabled
			if ( attachment_blend_state.srcColorBlendFactor == VK_BLEND_FACTOR_SRC_ALPHA && attachment_blend_state.dstColorBlendFactor == VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA ) {
				frag_spec_data.discard_mode = 1;
			} else if ( attachment_blend_state.srcColorBlendFactor == VK_BLEND_FACTOR_ONE && attachment_blend_state.dstColorBlendFactor == VK_BLEND_FACTOR_ONE ) {
				frag_spec_data.discard_mode = 2;
			}
		}
	}

	main_motion_target = ( vk.fboActive &&
		( renderPassIndex == RENDER_PASS_MAIN || renderPassIndex == RENDER_PASS_POST_BLOOM ||
			renderPassIndex == RENDER_PASS_UI_OVERLAY ) ) ? VK_TRUE : VK_FALSE;
	if ( renderPassIndex == RENDER_PASS_SCREENMAP ) {
		main_motion_target = VK_TRUE;
	}
	attachment_blend_states[0] = attachment_blend_state;
	Com_Memset( &attachment_blend_states[1], 0, sizeof( attachment_blend_states[1] ) );
	attachment_blend_states[1].blendEnable = VK_FALSE;
	attachment_blend_states[1].colorWriteMask = 0;
	if ( main_motion_target &&
		def->shader_type != TYPE_DOT &&
		def->shader_type != TYPE_SIGNLE_TEXTURE_DF &&
		def->shadow_phase == SHADOW_DISABLED &&
		depth_stencil_state.depthTestEnable == VK_TRUE &&
		depth_stencil_state.depthWriteEnable == VK_TRUE )
	{
		attachment_blend_states[1].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT;
	}

	if ( r_vk_pipeline_debug && r_vk_pipeline_debug->integer ) {
		ri.Printf( PRINT_DEVELOPER, "vk pipeline def#%u render_pass=%u shader=%u fog=%d state=0x%x allow_discard=%d discard_mode=%d\n",
			def_index, renderPassIndex, def->shader_type, def->fog_stage, def->state_bits, def->allow_discard, frag_spec_data.discard_mode );
#ifdef USE_VK_PBR
		if ( def->vk_pbr_flags ) {
			ri.Printf( PRINT_DEVELOPER, "vk pipeline PBR spec consts [19=%d 20=%d 21=%d 22=%d 23=%d 24=%d 25=%d 26=%d 27=%d 28=%d 29=%d]\n",
				frag_spec_data.normal_texture_set,
				frag_spec_data.physical_texture_set,
				frag_spec_data.env_texture_set,
				frag_spec_data.lightmap_texture_set,
				frag_spec_data.irradiance_texture_set,
				frag_spec_data.emissive_texture_set,
				frag_spec_data.clearcoat_texture_set,
				frag_spec_data.sheen_texture_set,
				frag_spec_data.anisotropy_texture_set,
				frag_spec_data.transmission_texture_set,
				frag_spec_data.subsurface_texture_set );
		}
#endif
	}

	blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state.pNext = NULL;
	blend_state.flags = 0;
	blend_state.logicOpEnable = VK_FALSE;
	blend_state.logicOp = VK_LOGIC_OP_COPY;
	blend_state.attachmentCount = main_motion_target ? 2 : 1;
	blend_state.pAttachments = attachment_blend_states;
	blend_state.blendConstants[0] = 0.0f;
	blend_state.blendConstants[1] = 0.0f;
	blend_state.blendConstants[2] = 0.0f;
	blend_state.blendConstants[3] = 0.0f;

	dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.pNext = NULL;
	dynamic_state.flags = 0;
	dynamic_state.dynamicStateCount = main_dynamic_state_count;
	dynamic_state.pDynamicStates = dynamic_state_array;

	create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	create_info.pNext = NULL;
	create_info.flags = 0;
	create_info.stageCount = ARRAY_LEN(shader_stages);
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

	if ( def->shader_type == TYPE_DOT )
		create_info.layout = vk.pipeline_layout_storage;
	else
		create_info.layout = vk.pipeline_layout;

	if ( renderPassIndex == RENDER_PASS_SCREENMAP )
		create_info.renderPass = vk.render_pass.screenmap;
	else if ( renderPassIndex == RENDER_PASS_SUN_SHADOW )
		create_info.renderPass = vk.render_pass.sun_shadow;
	else if ( renderPassIndex == RENDER_PASS_POST_BLOOM )
		create_info.renderPass = vk.render_pass.post_bloom;
	else if ( renderPassIndex == RENDER_PASS_UI_OVERLAY )
		create_info.renderPass = vk.render_pass.ui_overlay;
	else if ( renderPassIndex == RENDER_PASS_CUBEMAP )
		create_info.renderPass = vk.render_pass.cubemap;
	else
		create_info.renderPass = vk.render_pass.main;

	create_info.subpass = 0;
	create_info.basePipelineHandle = VK_NULL_HANDLE;
	create_info.basePipelineIndex = -1;

	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, vk.pipelineCache, 1, &create_info, NULL, &pipeline ) );

	// SET_OBJECT_NAME( pipeline, va( "pipeline def#%i, pass#%i", def_index, renderPassIndex ), VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );

	vk.pipeline_create_count++;

	return pipeline;
}


static uint32_t vk_alloc_pipeline( const Vk_Pipeline_Def *def ) {
	VK_Pipeline_t *pipeline;
	if ( vk.pipelines_count >= MAX_VK_PIPELINES ) {
		ri.Error( ERR_DROP, "alloc_pipeline: MAX_VK_PIPELINES reached" );
		return 0;
	} else {
		int j;
		pipeline = &vk.pipelines[ vk.pipelines_count ];
		pipeline->def = *def;
		for ( j = 0; j < RENDER_PASS_COUNT; j++ ) {
			pipeline->handle[j] = VK_NULL_HANDLE;
		}
		return vk.pipelines_count++;
	}
}


VkPipeline vk_gen_pipeline( uint32_t index ) {
	if ( index < vk.pipelines_count ) {
		VK_Pipeline_t *pipeline = vk.pipelines + index;
		const renderPass_t pass = vk.renderPassIndex;
		if ( pipeline->handle[ pass ] == VK_NULL_HANDLE ) {
			pipeline->handle[ pass ] = vk_create_pipeline( &pipeline->def, pass, index );
		}
		return pipeline->handle[ pass ];
	} else {
		ri.Error( ERR_FATAL, "vk_gen_pipeline(%i): NULL pipeline", index );
		return VK_NULL_HANDLE;
	}
}






uint32_t vk_find_pipeline_ext( uint32_t base, const Vk_Pipeline_Def *def, qboolean use ) {
	const Vk_Pipeline_Def *cur_def;
	uint32_t index;

	for ( index = base; index < vk.pipelines_count; index++ ) {
		cur_def = &vk.pipelines[ index ].def;
		if ( memcmp( cur_def, def, sizeof( *def ) ) == 0 ) {
			goto found;
		}
	}

	index = vk_alloc_pipeline( def );
found:

	if ( use )
		vk_gen_pipeline( index );

	return index;
}


void vk_get_pipeline_def( uint32_t pipeline, Vk_Pipeline_Def *def ) {
	if ( pipeline >= vk.pipelines_count ) {
		Com_Memset( def, 0, sizeof( *def ) );
	} else {
		Com_Memcpy( def, &vk.pipelines[ pipeline ].def, sizeof( *def ) );
	}
}
