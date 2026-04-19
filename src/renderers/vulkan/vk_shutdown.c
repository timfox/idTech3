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

#ifdef USE_VBO
void vk_release_vbo( void );
#endif

void vk_shutdown( refShutdownCode_t code )
{
#ifdef USE_VK_PBR
	int i, j, k, l, m;
#else
	int i, j, k, l;
#endif

	if ( qvkQueuePresentKHR == NULL ) { /* not fully initialized */
		goto __cleanup;
	}
	/* VUID-05137: ensure GPU finished before destroying resources */
	if ( !vk.device_lost && qvkDeviceWaitIdle )
		qvkDeviceWaitIdle( vk.device );
	/* Always run full destroy sequence for VUID-05137 compliance.
	 * When device_lost, destroy calls may return VK_ERROR_DEVICE_LOST but we still attempt them. */
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

	vk_forward_plus_destroy_descriptor_layout();

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

	if ( vk.pipeline_layout != VK_NULL_HANDLE && qvkDestroyPipelineLayout != NULL ) {
		qvkDestroyPipelineLayout( vk.device, vk.pipeline_layout, NULL );
		vk.pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.pipeline_layout_storage != VK_NULL_HANDLE && qvkDestroyPipelineLayout != NULL ) {
		qvkDestroyPipelineLayout( vk.device, vk.pipeline_layout_storage, NULL );
		vk.pipeline_layout_storage = VK_NULL_HANDLE;
	}
	if ( vk.pipeline_layout_post_process != VK_NULL_HANDLE && qvkDestroyPipelineLayout != NULL ) {
		qvkDestroyPipelineLayout( vk.device, vk.pipeline_layout_post_process, NULL );
		vk.pipeline_layout_post_process = VK_NULL_HANDLE;
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
	qvkDestroyPipelineLayout(vk.device, vk.pipeline_layout_ssr, NULL);
	if ( vk.pipeline_layout_atmosphere != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.pipeline_layout_atmosphere, NULL );
		vk.pipeline_layout_atmosphere = VK_NULL_HANDLE;
	}
#ifdef USE_VK_PBR
	qvkDestroyPipelineLayout(vk.device, vk.pipeline_layout_brdflut, NULL);
#endif

#ifdef USE_VBO
	vk_release_vbo();
#endif

	vk_clean_staging_buffer();

	vk_release_geometry_buffers();

	vk_forward_plus_shutdown();

	vk_destroy_samplers();

	vk_destroy_sync_primitives();

	qvkDestroyBuffer( vk.device, vk.storage.buffer, NULL );
	qvkFreeMemory( vk.device, vk.storage.memory, NULL );

#ifdef USE_VK_PBR
for (i = 0; i < 2; i++) {
        for (j = 0; j < 3; j++) {
            for (k = 0; k < 2; k++) {
                for (l = 0; l < 2; l++) {
                    for (m = 0; m < 2; m++) {
                        if (vk.modules.vert.gen[i][j][k][l][m] != VK_NULL_HANDLE) {
                            qvkDestroyShaderModule(vk.device, vk.modules.vert.gen[i][j][k][l][m], NULL);
                            vk.modules.vert.gen[i][j][k][l][m] = VK_NULL_HANDLE;
                        }
                        for ( int n = 0; n < 2; n++ ) {
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
#else
	for ( i = 0; i < 3; i++ ) {
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				for ( l = 0; l < 2; l++ ) {
					if ( vk.modules.vert.gen[i][j][k][l] != VK_NULL_HANDLE ) {
						qvkDestroyShaderModule( vk.device, vk.modules.vert.gen[i][j][k][l], NULL );
						vk.modules.vert.gen[i][j][k][l] = VK_NULL_HANDLE;
					}
				}
			}
		}
	}
	for ( i = 0; i < 3; i++ ) {
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				if ( vk.modules.frag.gen[i][j][k] != VK_NULL_HANDLE ) {
					qvkDestroyShaderModule( vk.device, vk.modules.frag.gen[i][j][k], NULL );
					vk.modules.frag.gen[i][j][k] = VK_NULL_HANDLE;
				}
			}
		}
	}
#endif

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

#ifdef USE_VK_PBR
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
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				qvkDestroyShaderModule( vk.device, vk.modules.frag.ent[i][j][k], NULL );
				vk.modules.frag.ent[i][j][k] = VK_NULL_HANDLE;
			}
		}
	}
#else
	for ( i = 0; i < 2; i++ ) {
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				qvkDestroyShaderModule( vk.device, vk.modules.vert.ident1[i][j][k], NULL );
				vk.modules.vert.ident1[i][j][k] = VK_NULL_HANDLE;
			}
			qvkDestroyShaderModule( vk.device, vk.modules.frag.ident1[i][j], NULL );
			vk.modules.frag.ident1[i][j] = VK_NULL_HANDLE;
		}
	}

	for ( i = 0; i < 2; i++ ) {
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				qvkDestroyShaderModule( vk.device, vk.modules.vert.fixed[i][j][k], NULL );
				vk.modules.vert.fixed[i][j][k] = VK_NULL_HANDLE;
			}
			qvkDestroyShaderModule( vk.device, vk.modules.frag.fixed[i][j], NULL );
			vk.modules.frag.fixed[i][j] = VK_NULL_HANDLE;
		}
	}

	for ( i = 0; i < 1; i++ ) {
		for ( j = 0; j < 2; j++ ) {
			qvkDestroyShaderModule( vk.device, vk.modules.frag.ent[i][j], NULL );
			vk.modules.frag.ent[i][j] = VK_NULL_HANDLE;
		}
	}
#endif

	qvkDestroyShaderModule( vk.device, vk.modules.frag.gen0_df, NULL );

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
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.oit_resolve_fs );
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
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.cbt_terrain_cs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.terrain_vs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.terrain_fs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.filtercube_vs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.filtercube_gm );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.irradiancecube_fs );
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.prefilterenvmap_fs );

#ifdef USE_VK_PBR
	VK_DESTROY_SHADER_MODULE_FIELD( vk.modules.brdflut_fs );
#endif

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

	Com_Memset( &vk_world, 0, sizeof( vk_world ) );

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		vk.tess[i].uniform_read_offset = 0;
		vk.tess[i].vertex_buffer_offset = 0;
		Com_Memset( vk.tess[i].buf_offset, 0, sizeof( vk.tess[i].buf_offset ) );
		Com_Memset( vk.tess[i].vbo_offset, 0, sizeof( vk.tess[i].vbo_offset ) );
	}

	Com_Memset( &vk.stats, 0, sizeof( vk.stats ) );
}
