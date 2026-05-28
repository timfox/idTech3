/*
===========================================================================
SPIR-V VkShaderModule creation and vk_create_shader_modules().
Extracted from vk.c for incremental modularization.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_shader_modules.h"

static VkShaderModule vk_shader_module_from_spirv( const uint8_t *bytes, int count )
{
	VkShaderModuleCreateInfo desc;
	VkShaderModule module;

	if ( count % 4 != 0 ) {
		ri.Error( ERR_FATAL, "Vulkan: SPIR-V binary buffer size is not a multiple of 4" );
	}

	desc.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.codeSize = (size_t)count;
	desc.pCode = (const uint32_t *)bytes;

	VK_CHECK( qvkCreateShaderModule( vk.device, &desc, NULL, &module ) );

	return module;
}

#include "shaders/spirv/shader_data.c"
#define SHADER_MODULE(name) vk_shader_module_from_spirv( name, (int)sizeof( name ) )

#include "shaders/spirv/shader_binding.c"

void vk_create_shader_modules( void )
{
	int i, j, k;

	vk.modules.frag.gen0_df = SHADER_MODULE( frag_tx0_df );
	SET_OBJECT_NAME( vk.modules.frag.gen0_df, "single-texture df fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.frag.ui_sdf_text = SHADER_MODULE( frag_ui_sdf_text_frag_spv );
	SET_OBJECT_NAME( vk.modules.frag.ui_sdf_text, "ui sdf text fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.frag.ent[0][0][0] = SHADER_MODULE( frag_tx0_ent );
	vk.modules.frag.ent[0][0][1] = SHADER_MODULE( frag_tx0_ent_fog );
	vk.modules.frag.ent[1][0][0] = SHADER_MODULE( frag_pbr_tx0_ent );
	vk.modules.frag.ent[1][0][1] = SHADER_MODULE( frag_pbr_tx0_ent_fog );

	for ( i = 0; i < 2; i++ ) {
		const char *sh[] = { "", "pbr" };

		for ( j = 0; j < 1; j++ ) {
			const char *tx[] = { "single" /*, "double" */};
			const char *fog[] = { "", "+fog" };
			for ( k = 0; k < 2; k++ ) {
				const char *s = va( "%s-%s-texture entity-color%s fragment module", sh[i], tx[j], fog[k] );
				SET_OBJECT_NAME( vk.modules.frag.ent[i][j][k], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
			}
		}
	}

	vk_bind_generated_shaders();

	vk.modules.vert.light[0] = SHADER_MODULE( vert_light );
	vk.modules.vert.light[1] = SHADER_MODULE( vert_light_fog );
	SET_OBJECT_NAME( vk.modules.vert.light[0], "light vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.vert.light[1], "light fog vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.frag.light[0][0] = SHADER_MODULE( frag_light );
	vk.modules.frag.light[0][1] = SHADER_MODULE( frag_light_fog );
	vk.modules.frag.light[1][0] = SHADER_MODULE( frag_light_line );
	vk.modules.frag.light[1][1] = SHADER_MODULE( frag_light_line_fog );
	SET_OBJECT_NAME( vk.modules.frag.light[0][0], "light fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.frag.light[0][1], "light fog fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.frag.light[1][0], "linear light fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.frag.light[1][1], "linear light fog fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.color_fs = SHADER_MODULE( color_frag_spv );
	vk.modules.color_vs = SHADER_MODULE( color_vert_spv );

	SET_OBJECT_NAME( vk.modules.color_vs, "single-color vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.color_fs, "single-color fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.fog_vs = SHADER_MODULE( fog_vert_spv );
	vk.modules.fog_fs = SHADER_MODULE( fog_frag_spv );

	SET_OBJECT_NAME( vk.modules.fog_vs, "fog-only vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.fog_fs, "fog-only fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.dot_vs = SHADER_MODULE( dot_vert_spv );
	vk.modules.dot_fs = SHADER_MODULE( dot_frag_spv );

	SET_OBJECT_NAME( vk.modules.dot_vs, "dot vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.dot_fs, "dot fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.bloom_fs = SHADER_MODULE( bloom_frag_spv );
	vk.modules.blur_fs = SHADER_MODULE( blur_frag_spv );
	vk.modules.blend_fs = SHADER_MODULE( blend_frag_spv );
	vk.modules.ssao_fs = SHADER_MODULE( ssao_frag_spv );
	vk.modules.hbao_fs = SHADER_MODULE( hbao_frag_spv );
	vk.modules.ssao_blur_fs = SHADER_MODULE( ssao_blur_frag_spv );
	vk.modules.ssao_combine_fs = SHADER_MODULE( ssao_combine_frag_spv );
	vk.modules.oit_accum_vs = SHADER_MODULE( oit_accum_vert_spv );
	vk.modules.oit_accum_fs = SHADER_MODULE( oit_accum_frag_spv );
	vk.modules.oit_resolve_fs = SHADER_MODULE( oit_resolve_frag_spv );
	vk.modules.ssao_debug_fs = SHADER_MODULE( ssao_debug_frag_spv );
	vk.modules.ssao_depth_debug_fs = SHADER_MODULE( ssao_depth_debug_frag_spv );
	vk.modules.ssr_fs = SHADER_MODULE( ssr_frag_spv );

	SET_OBJECT_NAME( vk.modules.bloom_fs, "bloom extraction fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.blur_fs, "gaussian blur fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.blend_fs, "final bloom blend fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.ssao_fs, "ssao fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.hbao_fs, "hbao fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.ssao_blur_fs, "ssao blur fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.ssao_combine_fs, "ssao combine fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.ssao_debug_fs, "ssao debug fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.ssao_depth_debug_fs, "ssao depth debug fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.ssr_fs, "ssr fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.gamma_fs = SHADER_MODULE( gamma_frag_spv );
	vk.modules.overlay_compose_fs = SHADER_MODULE( overlay_compose_frag_spv );
	vk.modules.gamma_vs = SHADER_MODULE( gamma_vert_spv );
	vk.modules.atmosphere_fs = SHADER_MODULE( atmosphere_frag_spv );

	SET_OBJECT_NAME( vk.modules.gamma_fs, "gamma post-processing fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.overlay_compose_fs, "overlay compose fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.gamma_vs, "gamma post-processing vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.atmosphere_fs, "atmosphere fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.smaa_edge_fs = SHADER_MODULE( smaa_edge_frag_spv );
	vk.modules.smaa_blend_fs = SHADER_MODULE( smaa_blend_frag_spv );
	vk.modules.smaa_compose_fs = SHADER_MODULE( smaa_compose_frag_spv );
	vk.modules.taa_fs = SHADER_MODULE( taa_frag_spv );

	SET_OBJECT_NAME( vk.modules.smaa_edge_fs, "smaa edge fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.smaa_blend_fs, "smaa blend fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.smaa_compose_fs, "smaa compose fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.taa_fs, "taa fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

#ifdef VK_PBR_BRDFLUT
	vk.modules.brdflut_fs = SHADER_MODULE( brdflut_frag_spv );
	SET_OBJECT_NAME( vk.modules.brdflut_fs, "brdf LUT fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
#endif

	vk.modules.filtercube_vs = SHADER_MODULE( filtercube_vert_spv );
	SET_OBJECT_NAME( vk.modules.filtercube_vs, "filter cube vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	vk.modules.prefilterenvmap_fs = SHADER_MODULE( prefilterenvmap_frag_spv );
	SET_OBJECT_NAME( vk.modules.prefilterenvmap_fs, "prefilter env map fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	vk.modules.irradiancecube_fs = SHADER_MODULE( irradiancecube_frag_spv );
	SET_OBJECT_NAME( vk.modules.irradiancecube_fs, "irradiance cube fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	vk.modules.filtercube_gm = SHADER_MODULE( filtercube_geom_spv );
	SET_OBJECT_NAME( vk.modules.filtercube_gm, "filter cube geometry shader", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
}
