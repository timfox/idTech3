/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Device teardown: vk_shutdown, vk_wait_idle, vk_queue_wait_idle, vk_release_resources.
Split from vk.c.
===========================================================================
*/

#include "tr_local.h"
#include "vk_instance.h"
#include "vk_geometry.h"
#include "vk_texture_image.h"
#include "vk_resource_destroy.h"
#include "vk_framebuffers.h"
#include "vk_attachments.h"
#include "vk_swapchain.h"
#include "vk_staging.h"
#include "vk_sync.h"
#include "vk_skybox_hdr.h"
#include "vk_forward_plus.h"
#include "vk_deferred_gbuffer.h"
#include "vk_visibility_buffer.h"
#include "vk_vrcs.h"
#include "vk_rtx.h"
#include "vk_grtx.h"
#include "vk_pathtrace.h"
#include "vk_hybrid1.h"
#include "vk_raygun.h"
#include "vk_surfel_gi.h"
#include "vk_rcgi.h"
#include "vk_ambient_visibility.h"
#include "vk_dressi.h"
#include "vk_vdb.h"
#include "vk_pipeline_cache_disk.h"
#include "vk_fp64_points.h"

#ifdef USE_VBO
void vk_release_vbo( void );
#endif

void vk_shutdown( refShutdownCode_t code )
{
	int i, j, k, l, m;

	if ( qvkQueuePresentKHR == NULL ) { /* not fully initialized */
		goto __cleanup;
	}
	/* Device already lost: skip destroy trees to avoid recursive VK_CHECK/ri.Error spam.
	 * passDiag context was already printed by vk_report_device_lost_context. */
	if ( vk.device_lost ) {
		ri.Printf( PRINT_ALL, "[VK][device_lost] fast shutdown: skipping resource destroy tree\n" );
		goto __cleanup;
	}
	/* VUID-05137: ensure GPU finished before destroying resources */
	if ( qvkDeviceWaitIdle )
		qvkDeviceWaitIdle( vk.device );
	vk_rtx_shutdown();
	vk_grtx_shutdown();
	vk_pathtrace_shutdown();
	vk_hybrid1_shutdown();
	vk_raygun_shutdown();
	vk_surfel_gi_shutdown();
	vk_rcgi_shutdown();
	vk_ambient_visibility_shutdown();
	R_Dressi_Shutdown();
	vk_deferred_gbuffer_invalidate_runtime();
	vk_visibility_buffer_shutdown();
	vk_destroy_framebuffers();

	vk_destroy_pipelines( qtrue ); /* reset counter */

	vk_destroy_render_passes();

	vk_destroy_attachments();

	vk_destroy_swapchain();

#ifdef VK_CUBEMAP
	vk_destroy_cubemap_prefilter();

	image_t *img = tr.emptyCubemap;
	vk_destroy_image_resources( &img->handle, &img->view );

	for ( i = 0; i < tr.numCubemaps; i++ ) {
		image_t *cubemap_img = tr.cubemaps[ i ].prefiltered_image;
		vk_destroy_image_resources( &cubemap_img->handle, &cubemap_img->view );

		cubemap_img = tr.cubemaps[ i ].irradiance_image;
		vk_destroy_image_resources( &cubemap_img->handle, &cubemap_img->view );

		Com_Memset( &tr.cubemaps[ i ], 0, sizeof(cubemap_t) );
	}
#endif

	SkyboxHDR_Shutdown();

	if ( vk.pipelineCache != VK_NULL_HANDLE ) {
		vk_pipeline_cache_save();
		qvkDestroyPipelineCache( vk.device, vk.pipelineCache, NULL );
		vk.pipelineCache = VK_NULL_HANDLE;
	}
	if ( vk.volumetric_query_pool != VK_NULL_HANDLE ) {
		qvkDestroyQueryPool( vk.device, vk.volumetric_query_pool, NULL );
		vk.volumetric_query_pool = VK_NULL_HANDLE;
	}
	if ( vk.occlusion_query_pool != VK_NULL_HANDLE ) {
		qvkDestroyQueryPool( vk.device, vk.occlusion_query_pool, NULL );
		vk.occlusion_query_pool = VK_NULL_HANDLE;
	}

	if ( vk.command_pool != VK_NULL_HANDLE && qvkDestroyCommandPool != NULL ) {
		qvkDestroyCommandPool( vk.device, vk.command_pool, NULL );
		vk.command_pool = VK_NULL_HANDLE;
	}

	if ( vk.descriptor_pool != VK_NULL_HANDLE && qvkDestroyDescriptorPool != NULL ) {
		qvkDestroyDescriptorPool( vk.device, vk.descriptor_pool, NULL );
		vk.descriptor_pool = VK_NULL_HANDLE;
	}

	vk_forward_plus_on_descriptor_pool_destroyed();

	qvkDestroyDescriptorSetLayout(vk.device, vk.set_layout_sampler, NULL);
	qvkDestroyDescriptorSetLayout(vk.device, vk.set_layout_uniform, NULL);
	qvkDestroyDescriptorSetLayout(vk.device, vk.set_layout_storage, NULL);
	if ( vk.volumetric_compute_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.volumetric_compute_layout, NULL );
		vk.volumetric_compute_layout = VK_NULL_HANDLE;
	}
	if ( vk.volumetric_composite_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.volumetric_composite_layout, NULL );
		vk.volumetric_composite_layout = VK_NULL_HANDLE;
	}
	if ( vk.volumetric_depth_resolve_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.volumetric_depth_resolve_layout, NULL );
		vk.volumetric_depth_resolve_layout = VK_NULL_HANDLE;
	}
	if ( vk.luminance_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.luminance_layout, NULL );
		vk.luminance_layout = VK_NULL_HANDLE;
	}
	if ( vk.volumetric_fluid_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.volumetric_fluid_layout, NULL );
		vk.volumetric_fluid_layout = VK_NULL_HANDLE;
	}
	if ( vk.cbt_terrain_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.cbt_terrain_layout, NULL );
		vk.cbt_terrain_layout = VK_NULL_HANDLE;
	}
	if ( vk.set_layout_blend_layers != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.set_layout_blend_layers, NULL );
		vk.set_layout_blend_layers = VK_NULL_HANDLE;
	}
	vk.blend_layers_descriptor = VK_NULL_HANDLE;

	if ( vk.pipeline_layout != VK_NULL_HANDLE && qvkDestroyPipelineLayout != NULL ) {
		qvkDestroyPipelineLayout( vk.device, vk.pipeline_layout, NULL );
		vk.pipeline_layout = VK_NULL_HANDLE;
	}
	vk_forward_plus_destroy_graphics_layout();
	if ( vk.pipeline_layout_storage != VK_NULL_HANDLE && qvkDestroyPipelineLayout != NULL ) {
		qvkDestroyPipelineLayout( vk.device, vk.pipeline_layout_storage, NULL );
		vk.pipeline_layout_storage = VK_NULL_HANDLE;
	}
	if ( vk.pipeline_layout_post_process != VK_NULL_HANDLE && qvkDestroyPipelineLayout != NULL ) {
		qvkDestroyPipelineLayout( vk.device, vk.pipeline_layout_post_process, NULL );
		vk.pipeline_layout_post_process = VK_NULL_HANDLE;
	}
	if ( vk.pipeline_layout_taa != VK_NULL_HANDLE && qvkDestroyPipelineLayout != NULL ) {
		qvkDestroyPipelineLayout( vk.device, vk.pipeline_layout_taa, NULL );
		vk.pipeline_layout_taa = VK_NULL_HANDLE;
	}
	if ( vk.pipeline_layout_reactive_stamp != VK_NULL_HANDLE && qvkDestroyPipelineLayout != NULL ) {
		qvkDestroyPipelineLayout( vk.device, vk.pipeline_layout_reactive_stamp, NULL );
		vk.pipeline_layout_reactive_stamp = VK_NULL_HANDLE;
	}
	if ( vk.pipeline_layout_blend != VK_NULL_HANDLE && qvkDestroyPipelineLayout != NULL ) {
		qvkDestroyPipelineLayout( vk.device, vk.pipeline_layout_blend, NULL );
		vk.pipeline_layout_blend = VK_NULL_HANDLE;
	}
	if ( vk.pipeline_layout_smaa != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout(vk.device, vk.pipeline_layout_smaa, NULL);
		vk.pipeline_layout_smaa = VK_NULL_HANDLE;
	}
	qvkDestroyPipelineLayout(vk.device, vk.pipeline_layout_ssao, NULL);
	qvkDestroyPipelineLayout(vk.device, vk.pipeline_layout_ssao_combine, NULL);
	if ( vk.pipeline_layout_oit_resolve != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout(vk.device, vk.pipeline_layout_oit_resolve, NULL);
		vk.pipeline_layout_oit_resolve = VK_NULL_HANDLE;
	}
	if ( vk.pipeline_layout_oit_accum != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.pipeline_layout_oit_accum, NULL );
		vk.pipeline_layout_oit_accum = VK_NULL_HANDLE;
	}
	if ( vk.pipeline_layout_oit_moments != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.pipeline_layout_oit_moments, NULL );
		vk.pipeline_layout_oit_moments = VK_NULL_HANDLE;
	}
	if ( vk.pipeline_layout_oit_accum_mboit != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.pipeline_layout_oit_accum_mboit, NULL );
		vk.pipeline_layout_oit_accum_mboit = VK_NULL_HANDLE;
	}
	qvkDestroyPipelineLayout(vk.device, vk.pipeline_layout_ssr, NULL);
	VK_FP64_PointsShutdown();

	if ( vk.pipeline_layout_atmosphere != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.pipeline_layout_atmosphere, NULL );
		vk.pipeline_layout_atmosphere = VK_NULL_HANDLE;
	}
	qvkDestroyPipelineLayout(vk.device, vk.pipeline_layout_brdflut, NULL);

#ifdef USE_VBO
	vk_release_vbo();
	vk_release_stream_vbo();
#endif

	vk_clean_staging_buffer();

	vk_release_geometry_buffers();

	VDB_Shutdown();
	vk_forward_plus_shutdown();
	vk_vrcs_shutdown();
	vk_deferred_gbuffer_shutdown();
	vk_visibility_buffer_shutdown();

	vk_destroy_samplers();

	vk_destroy_sync_primitives();

	qvkDestroyBuffer( vk.device, vk.storage.buffer, NULL );
	qvkFreeMemory( vk.device, vk.storage.memory, NULL );

for (i = 0; i < 2; i++) {
        for (j = 0; j < 3; j++) {
            for (k = 0; k < 2; k++) {
                for (l = 0; l < 2; l++) {
                    for (m = 0; m < 2; m++) {
                        if (vk.modules.vert.gen[i][j][k][l][m] != VK_NULL_HANDLE) {
                            qvkDestroyShaderModule(vk.device, vk.modules.vert.gen[i][j][k][l][m], NULL);
                            vk.modules.vert.gen[i][j][k][l][m] = VK_NULL_HANDLE;
                        }
                        for ( int n = 0; n < 3; n++ ) {
                            if (vk.modules.vert.gen_gltf_gpu[i][j][k][l][m][n] != VK_NULL_HANDLE) {
                                qvkDestroyShaderModule(vk.device, vk.modules.vert.gen_gltf_gpu[i][j][k][l][m][n], NULL);
                                vk.modules.vert.gen_gltf_gpu[i][j][k][l][m][n] = VK_NULL_HANDLE;
                            }
                        }
                    }
                }
            }
        }
    }
    for (i = 0; i < 2; i++) {
        for (j = 0; j < 3; j++) {
            for (k = 0; k < 2; k++) {
                for (l = 0; l < 2; l++) {
                    if (vk.modules.frag.gen[i][j][k][l] != VK_NULL_HANDLE) {
                        qvkDestroyShaderModule(vk.device, vk.modules.frag.gen[i][j][k][l], NULL);
                        vk.modules.frag.gen[i][j][k][l] = VK_NULL_HANDLE;
                    }
                }
            }
        }
    }

	for ( i = 0; i < 2; i++ ) {
		if ( vk.modules.vert.light[i] != VK_NULL_HANDLE ) {
			qvkDestroyShaderModule( vk.device, vk.modules.vert.light[i], NULL );
			vk.modules.vert.light[i] = VK_NULL_HANDLE;
		}
		for ( j = 0; j < 2; j++ ) {
			if ( vk.modules.frag.light[i][j] != VK_NULL_HANDLE ) {
				qvkDestroyShaderModule( vk.device, vk.modules.frag.light[i][j], NULL );
				vk.modules.frag.light[i][j] = VK_NULL_HANDLE;
			}
		}
	}

	for ( i = 0; i < 2; i++ ) {
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				for ( m = 0; m < 2; m++ ) {
					qvkDestroyShaderModule( vk.device, vk.modules.vert.ident1[i][j][k][m], NULL );
					vk.modules.vert.ident1[i][j][k][m] = VK_NULL_HANDLE;
				}
				qvkDestroyShaderModule( vk.device, vk.modules.frag.ident1[i][j][k], NULL );
				vk.modules.frag.ident1[i][j][k] = VK_NULL_HANDLE;
			}
		}
	}

	for ( i = 0; i < 2; i++ ) {
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				for ( m = 0; m < 2; m++ ) {
					qvkDestroyShaderModule( vk.device, vk.modules.vert.fixed[i][j][k][m], NULL );
					vk.modules.vert.fixed[i][j][k][m] = VK_NULL_HANDLE;
				}
				qvkDestroyShaderModule( vk.device, vk.modules.frag.fixed[i][j][k], NULL );
				vk.modules.frag.fixed[i][j][k] = VK_NULL_HANDLE;
			}
		}
	}

	for ( i = 0; i < 2; i++ ) {
		for ( j = 0; j < 1; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				qvkDestroyShaderModule( vk.device, vk.modules.frag.ent[i][j][k], NULL );
				vk.modules.frag.ent[i][j][k] = VK_NULL_HANDLE;
			}
		}
	}

	qvkDestroyShaderModule( vk.device, vk.modules.frag.gen0_df, NULL );
	qvkDestroyShaderModule( vk.device, vk.modules.frag.ui_sdf_text, NULL );
	qvkDestroyShaderModule( vk.device, vk.modules.frag.ui_vector_text, NULL );
	qvkDestroyShaderModule( vk.device, vk.modules.frag.ui_vector_glyphlet_vert, NULL );
	qvkDestroyShaderModule( vk.device, vk.modules.frag.ui_vector_glyphlet_frag, NULL );
	qvkDestroyShaderModule( vk.device, vk.modules.frag.ui_subpixel_text, NULL );

	for ( i = 0; i < 2; i++ ) {
		if ( vk.modules.frag.flowmap[i] != VK_NULL_HANDLE ) {
			qvkDestroyShaderModule( vk.device, vk.modules.frag.flowmap[i], NULL );
			vk.modules.frag.flowmap[i] = VK_NULL_HANDLE;
		}
	}

	#define VK_DESTROY_SHADER_MODULE_FIELD(field) \
		do { \
			if ( (field) != VK_NULL_HANDLE ) { \
				qvkDestroyShaderModule( vk.device, (field), NULL ); \
				(field) = VK_NULL_HANDLE; \
			} \
		} while ( 0 )

	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.color_fs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.color_vs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.fog_vs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.fog_fs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.dot_vs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.dot_fs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.bloom_fs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.blur_fs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.blend_fs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.ssao_fs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.hbao_fs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.ssao_blur_fs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.ssao_combine_fs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.oit_accum_vs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.oit_accum_fs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.oit_moments_fs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.oit_accum_mboit_fs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.oit_resolve_fs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.reactive_stamp_reveal_fs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.ssao_debug_fs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.ssao_depth_debug_fs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.ssr_fs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.gamma_vs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.gamma_fs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.overlay_compose_fs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.atmosphere_fs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.smaa_edge_fs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.smaa_blend_fs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.smaa_compose_fs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.fxaa_fs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.lens_flare_fs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.fp64_points_native_vs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.fp64_points_native_fs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.fp64_points_emulated_vs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.fp64_points_emulated_fs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.fp64_points_single_vs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.fp64_points_single_fs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.taa_fs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.volumetric_fog_vs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.volumetric_fog_fs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.volumetric_fog_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.volumetric_depth_resolve_msaa_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.luminance_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.vegetation_wind_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.fluid_advect_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.fluid_divergence_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.fluid_pressure_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.fluid_gradient_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.deferred_gbuffer_fill_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.deferred_gbuffer_debug_fs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.deferred_lighting_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.deferred_lighting_vrcs_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.vrcs_sri_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.vrcs_pack_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.vrcs_deblock_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.deferred_lighting_composite_fs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.visibility_buffer_fill_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.visibility_buffer_debug_fs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.material_classify_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.ndgi_decompress_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.niv_shade_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.niv_composite_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.nslm_froxel_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.nist_refine_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.nist_composite_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.nvc_cache_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.nvc_restir_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.nvc_composite_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.fsa_importance_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.fsa_denoise_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.vfgi_decode_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.vfgi_composite_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.rf_transport_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.rf_decode_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.rf_composite_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.wpt_enqueue_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.wpt_wave_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.wpt_composite_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.mgs_prepare_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.mgs_splat_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.mgs_composite_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.vksplat_project_fwd_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.vksplat_tile_cull_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.vksplat_raster_fwd_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.vksplat_adam_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.curast_clear_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.curast_stage1_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.curast_resolve_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.graph_bfs_expand_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.arc_blanc_htilde_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.arc_blanc_fft_1d_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.arc_blanc_extract_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.arc_blanc_combine_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.arc_blanc_velocity_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.arc_blanc_velocity_accum_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.mimir_clear_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.mimir_brownian_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.mimir_splat_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.iris_clear_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.iris_spd_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.iris_compose_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.iris_overlay_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.wsp_clear_tiles_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.wsp_prepare_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.wsp_tile_bin_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.wsp_tile_draw_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.wsp_composite_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.cbt_terrain_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.terrain_vs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.terrain_fs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.filtercube_vs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.filtercube_gm );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.irradiancecube_fs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.prefilterenvmap_fs );

	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.brdflut_fs );

	#undef VK_DESTROY_SHADER_MODULE_FIELD

__cleanup:
	if ( vk.device != VK_NULL_HANDLE ) {
		qvkDestroyDevice( vk.device, NULL );
	}

	vk_deinit_device_functions();

	Com_Memset( &vk, 0, sizeof( vk ) );
	Com_Memset( &vk_world, 0, sizeof( vk_world ) );

	if ( code != REF_KEEP_CONTEXT ) {
		vk_destroy_instance();
		vk_deinit_instance_functions();
	}
}

void vk_wait_idle( void )
{
	if ( vk.device == VK_NULL_HANDLE || qvkDeviceWaitIdle == NULL ) {
		return;
	}
	if ( vk.device_lost ) {
		return;
	}
	VK_CHECK( qvkDeviceWaitIdle( vk.device ) );
}

void vk_queue_wait_idle( void )
{
	if ( vk.queue == VK_NULL_HANDLE || qvkQueueWaitIdle == NULL ) {
		return;
	}
	if ( vk.device_lost ) {
		return;
	}
	VK_CHECK( qvkQueueWaitIdle( vk.queue ) );
}

void vk_release_resources( void ) {
	int i, j;

	if ( vk.device == VK_NULL_HANDLE ) {
		return;  /* Vulkan never initialized (e.g. VKimp_Init failed) */
	}
	if ( vk.device_lost ) {
		return;  /* GPU lost; skip cleanup to avoid recursive VK_ERROR_DEVICE_LOST */
	}
	vk_wait_idle();

	vk_image_free_chunks();

	vk_clean_staging_buffer();

	for ( i = vk.pipelines_world_base; (uint32_t) i < vk.pipelines_count; i++ ) {
		for ( j = 0; j < RENDER_PASS_COUNT; j++ ) {
			if ( vk.pipelines[i].handle[j] != VK_NULL_HANDLE ) {
				qvkDestroyPipeline( vk.device, vk.pipelines[i].handle[j], NULL );
				vk.pipelines[i].handle[j] = VK_NULL_HANDLE;
				vk.pipeline_create_count--;
			}
		}
		Com_Memset( &vk.pipelines[i], 0, sizeof( vk.pipelines[0] ) );
	}
	vk.pipelines_count = vk.pipelines_world_base;

	VK_CHECK( qvkResetDescriptorPool( vk.device, vk.descriptor_pool, 0 ) );
	vk_forward_plus_on_descriptor_pool_destroyed();

	Com_Memset( &vk_world, 0, sizeof( vk_world ) );

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		vk.tess[i].uniform_read_offset = 0;
		vk.tess[i].vertex_buffer_offset = 0;
		Com_Memset( vk.tess[i].buf_offset, 0, sizeof( vk.tess[i].buf_offset ) );
		Com_Memset( vk.tess[i].vbo_offset, 0, sizeof( vk.tess[i].vbo_offset ) );
	}

	Com_Memset( &vk.stats, 0, sizeof( vk.stats ) );
}
