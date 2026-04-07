/*
===========================================================================
Teardown for VkRenderPass handles and long-lived VkPipelines (split from vk.c).
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_resource_destroy.h"

void vk_destroy_render_passes( void )
{
	uint32_t i;

	if ( vk.render_pass.main != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.main, NULL );
		vk.render_pass.main = VK_NULL_HANDLE;
	}

	if ( vk.render_pass.bloom_extract != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.bloom_extract, NULL );
		vk.render_pass.bloom_extract = VK_NULL_HANDLE;
	}

	for ( i = 0; i < ARRAY_LEN( vk.render_pass.blur ); i++ ) {
		if ( vk.render_pass.blur[i] != VK_NULL_HANDLE ) {
			qvkDestroyRenderPass( vk.device, vk.render_pass.blur[i], NULL );
			vk.render_pass.blur[i] = VK_NULL_HANDLE;
		}
	}

	if ( vk.render_pass.post_bloom != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.post_bloom, NULL );
		vk.render_pass.post_bloom = VK_NULL_HANDLE;
	}
	if ( vk.render_pass.ui_overlay != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.ui_overlay, NULL );
		vk.render_pass.ui_overlay = VK_NULL_HANDLE;
	}

	if ( vk.render_pass.ssao != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.ssao, NULL );
		vk.render_pass.ssao = VK_NULL_HANDLE;
	}

	if ( vk.render_pass.ssao_blur != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.ssao_blur, NULL );
		vk.render_pass.ssao_blur = VK_NULL_HANDLE;
	}

	if ( vk.render_pass.ssao_combine != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.ssao_combine, NULL );
		vk.render_pass.ssao_combine = VK_NULL_HANDLE;
	}
	if ( vk.render_pass.oit_accum != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.oit_accum, NULL );
		vk.render_pass.oit_accum = VK_NULL_HANDLE;
	}
	if ( vk.render_pass.oit_resolve != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.oit_resolve, NULL );
		vk.render_pass.oit_resolve = VK_NULL_HANDLE;
	}

	if ( vk.render_pass.ssr != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.ssr, NULL );
		vk.render_pass.ssr = VK_NULL_HANDLE;
	}

	if ( vk.render_pass.volumetric != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.volumetric, NULL );
		vk.render_pass.volumetric = VK_NULL_HANDLE;
	}
	if ( vk.render_pass.atmosphere != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.atmosphere, NULL );
		vk.render_pass.atmosphere = VK_NULL_HANDLE;
	}

	if ( vk.render_pass.screenmap != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.screenmap, NULL );
		vk.render_pass.screenmap = VK_NULL_HANDLE;
	}

	if ( vk.render_pass.sun_shadow != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.sun_shadow, NULL );
		vk.render_pass.sun_shadow = VK_NULL_HANDLE;
	}
	if ( vk.render_pass.local_spot_shadow != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.local_spot_shadow, NULL );
		vk.render_pass.local_spot_shadow = VK_NULL_HANDLE;
	}
	if ( vk.render_pass.local_point_shadow != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.local_point_shadow, NULL );
		vk.render_pass.local_point_shadow = VK_NULL_HANDLE;
	}

	if ( vk.render_pass.gamma != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.gamma, NULL );
		vk.render_pass.gamma = VK_NULL_HANDLE;
	}
	if ( vk.render_pass.overlay_compose != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.overlay_compose, NULL );
		vk.render_pass.overlay_compose = VK_NULL_HANDLE;
	}

	if ( vk.render_pass.capture != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.capture, NULL );
		vk.render_pass.capture = VK_NULL_HANDLE;
	}

#ifdef VK_PBR_BRDFLUT
    if ( vk.render_pass.brdflut != VK_NULL_HANDLE ) {
        qvkDestroyRenderPass( vk.device, vk.render_pass.brdflut, NULL );
        vk.render_pass.brdflut = VK_NULL_HANDLE;
    }
#endif

#ifdef VK_CUBEMAP
    if ( vk.render_pass.cubemap != VK_NULL_HANDLE ) {
        qvkDestroyRenderPass( vk.device, vk.render_pass.cubemap, NULL );
        vk.render_pass.cubemap = VK_NULL_HANDLE;
    }
#endif
	if ( vk.render_pass.smaa_edge != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.smaa_edge, NULL );
		vk.render_pass.smaa_edge = VK_NULL_HANDLE;
	}
	if ( vk.render_pass.smaa_blend != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.smaa_blend, NULL );
		vk.render_pass.smaa_blend = VK_NULL_HANDLE;
	}
	if ( vk.render_pass.smaa_compose != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.smaa_compose, NULL );
		vk.render_pass.smaa_compose = VK_NULL_HANDLE;
	}
	if ( vk.render_pass.taa != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.taa, NULL );
		vk.render_pass.taa = VK_NULL_HANDLE;
	}
}

void vk_destroy_pipelines( qboolean resetCounter )
{
	uint32_t i, j;

	for ( i = 0; i < vk.pipelines_count; i++ ) {
		for ( j = 0; j < RENDER_PASS_COUNT; j++ ) {
			if ( vk.pipelines[i].handle[j] != VK_NULL_HANDLE ) {
				qvkDestroyPipeline( vk.device, vk.pipelines[i].handle[j], NULL );
				vk.pipelines[i].handle[j] = VK_NULL_HANDLE;
				vk.pipeline_create_count--;
			}
		}
	}

	if ( resetCounter ) {
		Com_Memset( &vk.pipelines, 0, sizeof( vk.pipelines ) );
		vk.pipelines_count = 0;
	}

	if ( vk.gamma_pipeline ) {
		qvkDestroyPipeline( vk.device, vk.gamma_pipeline, NULL );
		vk.gamma_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.overlay_compose_pipeline ) {
		qvkDestroyPipeline( vk.device, vk.overlay_compose_pipeline, NULL );
		vk.overlay_compose_pipeline = VK_NULL_HANDLE;
	}

	if ( vk.capture_pipeline ) {
		qvkDestroyPipeline( vk.device, vk.capture_pipeline, NULL );
		vk.capture_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.ssr_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.ssr_pipeline, NULL );
		vk.ssr_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.atmosphere_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.atmosphere_pipeline, NULL );
		vk.atmosphere_pipeline = VK_NULL_HANDLE;
	}

	if ( vk.bloom_extract_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.bloom_extract_pipeline, NULL );
		vk.bloom_extract_pipeline = VK_NULL_HANDLE;
	}

	if ( vk.bloom_blend_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.bloom_blend_pipeline, NULL );
		vk.bloom_blend_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.smaa_edge_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.smaa_edge_pipeline, NULL );
		vk.smaa_edge_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.smaa_blend_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.smaa_blend_pipeline, NULL );
		vk.smaa_blend_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.smaa_compose_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.smaa_compose_pipeline, NULL );
		vk.smaa_compose_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.taa_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.taa_pipeline, NULL );
		vk.taa_pipeline = VK_NULL_HANDLE;
	}

	if ( vk.ssao_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.ssao_pipeline, NULL );
		vk.ssao_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.hbao_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.hbao_pipeline, NULL );
		vk.hbao_pipeline = VK_NULL_HANDLE;
	}

	if ( vk.ssao_blur_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.ssao_blur_pipeline, NULL );
		vk.ssao_blur_pipeline = VK_NULL_HANDLE;
	}

	if ( vk.ssao_combine_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.ssao_combine_pipeline, NULL );
		vk.ssao_combine_pipeline = VK_NULL_HANDLE;
	}

	if ( vk.ssao_debug_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.ssao_debug_pipeline, NULL );
		vk.ssao_debug_pipeline = VK_NULL_HANDLE;
	}

	if ( vk.ssao_depth_debug_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.ssao_depth_debug_pipeline, NULL );
		vk.ssao_depth_debug_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.oit_resolve_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.oit_resolve_pipeline, NULL );
		vk.oit_resolve_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.oit_accum_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.oit_accum_pipeline, NULL );
		vk.oit_accum_pipeline = VK_NULL_HANDLE;
	}

#ifdef VK_PBR_BRDFLUT
    if( vk.brdflut_pipeline != VK_NULL_HANDLE ) {
        qvkDestroyPipeline( vk.device, vk.brdflut_pipeline, NULL );
        vk.brdflut_pipeline = VK_NULL_HANDLE;
    }
#endif

	for ( i = 0; i < ARRAY_LEN( vk.blur_pipeline ); i++ ) {
		if ( vk.blur_pipeline[i] != VK_NULL_HANDLE ) {
			qvkDestroyPipeline( vk.device, vk.blur_pipeline[i], NULL );
			vk.blur_pipeline[i] = VK_NULL_HANDLE;
		}
	}

	if ( vk.volumetric_depth_resolve_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.volumetric_depth_resolve_pipeline, NULL );
		vk.volumetric_depth_resolve_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.luminance_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.luminance_pipeline, NULL );
		vk.luminance_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.volumetric_fluid_advect_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.volumetric_fluid_advect_pipeline, NULL );
		vk.volumetric_fluid_advect_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.volumetric_fluid_divergence_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.volumetric_fluid_divergence_pipeline, NULL );
		vk.volumetric_fluid_divergence_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.volumetric_fluid_pressure_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.volumetric_fluid_pressure_pipeline, NULL );
		vk.volumetric_fluid_pressure_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.volumetric_fluid_gradient_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.volumetric_fluid_gradient_pipeline, NULL );
		vk.volumetric_fluid_gradient_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.volumetric_compute_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.volumetric_compute_pipeline, NULL );
		vk.volumetric_compute_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.volumetric_composite_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.volumetric_composite_pipeline, NULL );
		vk.volumetric_composite_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.volumetric_compute_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.volumetric_compute_pipeline_layout, NULL );
		vk.volumetric_compute_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.volumetric_composite_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.volumetric_composite_pipeline_layout, NULL );
		vk.volumetric_composite_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.volumetric_depth_resolve_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.volumetric_depth_resolve_pipeline_layout, NULL );
		vk.volumetric_depth_resolve_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.luminance_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.luminance_pipeline_layout, NULL );
		vk.luminance_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.volumetric_fluid_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.volumetric_fluid_pipeline_layout, NULL );
		vk.volumetric_fluid_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.cbt_terrain_compute_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.cbt_terrain_compute_pipeline, NULL );
		vk.cbt_terrain_compute_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.cbt_terrain_compute_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.cbt_terrain_compute_layout, NULL );
		vk.cbt_terrain_compute_layout = VK_NULL_HANDLE;
	}
	if ( vk.vegwind_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.vegwind_pipeline, NULL );
		vk.vegwind_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.pipeline_layout_vegwind != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.pipeline_layout_vegwind, NULL );
		vk.pipeline_layout_vegwind = VK_NULL_HANDLE;
	}
	if ( vk.vegwind_vertex_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.vegwind_vertex_buffer, NULL );
		vk.vegwind_vertex_buffer = VK_NULL_HANDLE;
	}
	if ( vk.vegwind_vertex_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.vegwind_vertex_memory, NULL );
		vk.vegwind_vertex_memory = VK_NULL_HANDLE;
	}
	if ( vk.vegwind_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.vegwind_layout, NULL );
		vk.vegwind_layout = VK_NULL_HANDLE;
	}
}
