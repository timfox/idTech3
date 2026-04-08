#include "tr_local.h"
#include "vk.h"
#include "vk_instance.h"
#include "vk_geometry.h"
#include "vk_postfx.h"
#include "vk_postfx_params.h"
#include "vk_post_fog.h"
#include "vk_skybox_hdr.h"
#include "vk_atmosphere.h"
#include "vk_image_layout.h"
#include "vk_render_pass.h"
#include "vk_scene_pass.h"
#include "vk_frame_end.h"
#include "vk_temporal.h"
#include "vk_view_state.h"
#include "vk_volumetric_fog_color.h"
#include "vk_volumetric_params.h"
#include "vk_volumetric_pass.h"
#include "vk_volumetric_internal.h"
#include "vk_util.h"
#include "vk_validation.h"
#include "vk_staging.h"
#include "vk_descriptors.h"
#include "vk_sync.h"
#include "vk_cmd.h"
#include "vk_device.h"
#include "vk_swapchain.h"
#include "vk_staging.h"
#include "vk_descriptors.h"
#include "vk_shader_modules.h"
#include "vk_pipelines_persistent.h"
#include "vk_framebuffers.h"
#include "vk_attachments.h"
#include "vk_resource_destroy.h"
#include "vk_descriptor_sets.h"
#include "vk_pipeline_helpers.h"
#include <math.h>

/* VK_EXT_extended_dynamic_state3: struct for pipeline creation (VK_DYNAMIC_STATE_*, PFN in vk.h) */
#ifndef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT 1000484000
#endif

#include "vk_fluidsim.h"
#include "vk_terrain.h"
#include <stddef.h>

#if defined( _DEBUG )
#define USE_VK_VALIDATION
#if defined( _WIN32 )
#include <windows.h> /* for win32 debug callback */
#endif
#endif

/* Vk_Pipeline_FragSpecData + vk_create_pipeline / vk_find_pipeline_ext: vk_create_pipeline.c */

typedef struct {
	float paniniAmount;
	float paniniD;
	float paniniS;
	float aspect;
	float fovXDeg;
	float paniniBorderMode;
	float paniniDebugMode;
	float brightness;
	float paniniZoom;
	float paniniPad0;
	float paniniPad1;
	float paniniPad2;
	float exposure;  /* per-frame exposure (eye adaptation or r_exposure) */
	float srcUVScaleBias[4]; // scale.xy, bias.xy
} VkPostProcessPushConstants;

static int vkSamples = VK_SAMPLE_COUNT_1_BIT;
static int vkMaxSamples = VK_SAMPLE_COUNT_1_BIT;

float vk_get_msaa_min_sample_shading( void )
{
	if ( !vk.msaaSampleShading ) {
		return 1.0f;
	}

	return Com_Clamp( 0.25f, 1.0f,
		r_msaa_sample_shading_rate ? r_msaa_sample_shading_rate->value : 0.5f );
}

/* vk_instance, vk_surface, vk_debug_callback: vk_instance.c; qvk* storage: vk_procs.c */


/*
static VkFlags get_composite_alpha( VkCompositeAlphaFlagsKHR flags )
{
	const VkCompositeAlphaFlagBitsKHR compositeFlags[] = {
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
	};
	int i;

	for ( i = 1; i < ARRAY_LEN( compositeFlags ); i++ ) {
		if ( flags & compositeFlags[i] ) {
			return compositeFlags[i];
		}
	}

	return compositeFlags[0];
}
*/


/* create_instance, vk_create_device, vk_destroy_instance, init_vulkan_library, deinit_* moved to vk_instance.c */
/* SPIR-V VkShaderModule creation: vk_shader_modules.c */
/* Descriptor pool alloc + attachment/volumetric writes: vk_descriptor_sets.c */

/* vk_release_geometry_buffers, vk_create_geometry_buffers, vk_create_storage_buffer moved to vk_geometry.c */
/* vk_alloc_vbo / vk_release_vbo: vk_vbo.c */

/* vk_create_gltf_buffers moved to vk_gltf.c; vk_create_shader_modules -> vk_shader_modules.c; vk_alloc_persistent_pipelines -> vk_pipelines_persistent.c */

void vk_update_post_process_pipelines( void )
{
	if ( vk.fboActive ) {
		// update gamma shader
		vk_create_post_process_pipeline( 0, 0, 0 );
		if ( vk.ui_overlay_image != VK_NULL_HANDLE ) {
			vk_create_post_process_pipeline( 22, glConfig.vidWidth, glConfig.vidHeight );
		}
		if ( vk.capture.image ) {
			// update capture pipeline
			vk_create_post_process_pipeline( 3, gls.captureWidth, gls.captureHeight );
		}
		if ( vk.smaaActive ) {
			vk_create_post_process_pipeline( 10, glConfig.vidWidth, glConfig.vidHeight );
			vk_create_post_process_pipeline( 11, glConfig.vidWidth, glConfig.vidHeight );
			vk_create_post_process_pipeline( 12, glConfig.vidWidth, glConfig.vidHeight );
		}
		vk_create_post_process_pipeline( 23, glConfig.vidWidth, glConfig.vidHeight );
		if ( r_bloom->integer ) {
			// update bloom shaders
			uint32_t width = gls.captureWidth;
			uint32_t height = gls.captureHeight;
			uint32_t i;

			vk_create_post_process_pipeline( 1, width, height ); // bloom extraction

			for ( i = 0; i < ARRAY_LEN( vk.blur_pipeline ); i += 2 ) {
				width /= 2;
				height /= 2;
				vk_create_blur_pipeline( i + 0, width, height, qtrue ); // horizontal
				vk_create_blur_pipeline( i + 1, width, height, qfalse ); // vertical
			}

			vk_create_post_process_pipeline( 2, glConfig.vidWidth, glConfig.vidHeight ); // bloom blending
		}

		if ( r_ssao && r_ssao->integer ) {
			vk_create_post_process_pipeline( 5, glConfig.vidWidth, glConfig.vidHeight );
			vk_create_post_process_pipeline( 21, glConfig.vidWidth, glConfig.vidHeight );
			vk_create_post_process_pipeline( 6, glConfig.vidWidth, glConfig.vidHeight );
			vk_create_post_process_pipeline( 7, glConfig.vidWidth, glConfig.vidHeight );
			vk_create_post_process_pipeline( 8, glConfig.vidWidth, glConfig.vidHeight );
			vk_create_post_process_pipeline( 9, glConfig.vidWidth, glConfig.vidHeight );
		}
		if ( r_oit && r_oit->integer ) {
			vk_create_post_process_pipeline( 20, glConfig.vidWidth, glConfig.vidHeight );
			vk_create_oit_accum_pipeline();
		}
		if ( PostFX_SSR_IsEnabled() ) {
			vk_create_post_process_pipeline( 13, glConfig.vidWidth, glConfig.vidHeight );
		}
		vk_create_atmosphere_pipeline();
	}
}




/* vk_create_framebuffers / vk_destroy_framebuffers: vk_framebuffers.c */

/* vk_destroy_swapchain moved to vk_swapchain.c */


static void vk_restart_swapchain( const char *funcname, VkResult res )
{
	(void)res;
	uint32_t i;

#ifdef _DEBUG
	ri.Printf( PRINT_WARNING, "%s(%s): restarting swapchain...\n", funcname, vk_result_string( res ) );
#else
	ri.Printf(PRINT_WARNING, "%s(): restarting swapchain...\n", funcname );
#endif

	vk_wait_idle();

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		qvkResetCommandBuffer( vk.tess[i].command_buffer, 0 );
	}

#ifdef USE_UPLOAD_QUEUE
	qvkResetCommandBuffer( vk.staging_command_buffer, 0 );
#endif

	vk_destroy_pipelines( qfalse );
	vk_destroy_framebuffers();
	vk_destroy_render_passes();
	vk_destroy_attachments();
	vk_destroy_swapchain();
	vk_destroy_sync_primitives();
#ifdef VK_CUBEMAP	
    vk_destroy_cubemap_prefilter();
#endif

	vk_select_surface_format( vk.physical_device, vk_surface );
	vk_setup_surface_formats( vk.physical_device );

	vk_create_sync_primitives();
	vk_create_swapchain( vk.physical_device, vk.device, vk_surface, vk.present_format, &vk.swapchain, qfalse );
	vk_create_attachments();
	vk_create_render_passes();
	vk_create_framebuffers();

	//vk_create_bloom_pipelines();
#ifdef VK_PBR_BRDFLUT
    vk_create_brdflut_pipeline();
#endif
#ifdef VK_CUBEMAP
    vk_create_cubemap_prefilter();
#endif
	vk_update_attachment_descriptors();
	vk_update_volumetric_descriptors();

	vk_update_post_process_pipelines();

#ifdef VK_PBR_BRDFLUT
	vk_create_brfdlut();
#endif

	vk_temporal_request_sticky_reset( VK_TEMPORAL_RESET_SWAPCHAIN_CHANGE );
}


static void vk_set_render_scale( void )
{
	if ( gls.windowWidth != glConfig.vidWidth || gls.windowHeight != glConfig.vidHeight )
	{
		if ( r_renderScale->integer > 0 )
		{
			int scaleMode = r_renderScale->integer - 1;
			if ( scaleMode & 1 )
			{
				// preserve aspect ratio (black bars on sides)
				float windowAspect = (float) gls.windowWidth / (float) gls.windowHeight;
				float renderAspect = (float) glConfig.vidWidth / (float) glConfig.vidHeight;
				if ( windowAspect >= renderAspect )
				{
					float scale = (float)gls.windowHeight / ( float ) glConfig.vidHeight;
					int bias = ( gls.windowWidth - scale * (float) glConfig.vidWidth ) / 2;
					vk.blitX0 += bias;
				}
				else
				{
					float scale = (float)gls.windowWidth / ( float ) glConfig.vidWidth;
					int bias = ( gls.windowHeight - scale * (float) glConfig.vidHeight ) / 2;
					vk.blitY0 += bias;
				}
			}
			// linear filtering
			if ( scaleMode & 2 )
				vk.blitFilter = GL_LINEAR;
			else
				vk.blitFilter = GL_NEAREST;
		}

		vk.windowAdjusted = qtrue;
	}

	if ( r_fbo->integer && r_ext_supersample->integer && !r_renderScale->integer )
	{
		vk.blitFilter = GL_LINEAR;
	}
}


void vk_initialize( void )
{
	char buf[64], driver_version[64];
	const char *vendor_name;
	VkPhysicalDeviceProperties props;
	uint32_t major;
	uint32_t minor;
	uint32_t patch;
	uint32_t i;

		vk_init_vulkan_library();

	qvkGetDeviceQueue( vk.device, vk.queue_family_index, 0, &vk.queue );

	qvkGetPhysicalDeviceProperties( vk.physical_device, &props );

	vk.isV3DV = vk_device_is_v3dv( &props );
	if ( vk.isV3DV ) {
		cvar_t *r_rpi_profile = ri.Cvar_Get( "r_rpi_profile", "0", CVAR_ARCHIVE );
		if ( r_rpi_profile->integer ) {
			/* Apply RPi-friendly preset: Low quality + disable fluid sim */
			ri.Cvar_Set( "r_volumetricFog", "0" );
			ri.Cvar_Set( "r_ssao", "0" );
			ri.Cvar_Set( "r_bloom", "0" );
			ri.Cvar_Set( "r_ext_smaa", "0" );
			ri.Cvar_Set( "r_ssr", "0" );
			ri.Cvar_Set( "r_sharpen", "0.0" );
			ri.Cvar_Set( "r_exposure_auto", "0" );
			ri.Cvar_Set( "r_fogFluid", "0" );
			ri.Printf( PRINT_ALL, "[VK] Raspberry Pi (V3DV) detected: applied r_rpi_profile preset\n" );
		} else {
			ri.Printf( PRINT_ALL, S_COLOR_YELLOW "[VK] Raspberry Pi (V3DV) detected. For performance: set r_rpi_profile 1; vid_restart\n" S_COLOR_WHITE );
		}
	}

	vk.cmd = vk.tess + 0;
	vk_reset_motion_history();
	vk.adaptedExposure = 1.0f;
	VectorClear( vk.prevViewOrigin );
	VectorSet( vk.prevViewForward, 0.0f, 0.0f, -1.0f );  /* sentinel until first frame */
	vk.prevClientState = CA_UNINITIALIZED;
	Com_Memset( &vk.temporal, 0, sizeof( vk.temporal ) );
	vk.uniform_alignment = props.limits.minUniformBufferOffsetAlignment;
	vk.uniform_item_size = PAD( sizeof( vkUniform_t ), (size_t)vk.uniform_alignment );
#ifdef USE_VK_PBR	
	vk.uniform_camera_item_size = PAD( sizeof( vkUniformCamera_t ), (size_t)vk.uniform_alignment );
#endif
	// for flare visibility tests
	vk.storage_alignment = MAX( props.limits.minStorageBufferOffsetAlignment, sizeof( uint32_t ) );

	vk.maxAnisotropy = props.limits.maxSamplerAnisotropy;

	vk.blitFilter = GL_NEAREST;
	vk.windowAdjusted = qfalse;
	vk.blitX0 = vk.blitY0 = 0;
	vk.smaaActive = qfalse;
	vk.msaaActive = qfalse;

	vk_set_render_scale();

	if ( r_fbo->integer ) {
		vk.fboActive = qtrue;
		if ( r_ext_multisample->integer ) {
			vk.msaaActive = qtrue;
		}
		ri.Printf( PRINT_ALL, "...FBO enabled (HDR, post-process, gamma, PBR-ready)\n" );
		if ( r_fboDebug && r_fboDebug->integer >= 4 ) {
			ri.Printf( PRINT_ALL, S_COLOR_YELLOW "[VK][fbo] Troubleshooting: solid/wrong colors: r_oit 0 r_volumetricFog 0 r_exposure_auto 0 r_ext_smaa 0 r_ssao 0. Single-pixel noise: r_filmGrain 0. Death streaks: r_volumetricFog 0. Fog debug/gradient overlay: r_fogDebug 0 r_volumetricFogValidation 0. Then vid_restart\n" S_COLOR_WHITE );
		}
	} else {
		vk.fboActive = qfalse;
	}
	vk.smaaActive = (vk.fboActive && r_ext_smaa->integer) ? qtrue : qfalse;

	// multisampling

	vkMaxSamples = MIN( props.limits.sampledImageColorSampleCounts, props.limits.sampledImageDepthSampleCounts );

	if ( /*vk.fboActive &&*/ vk.msaaActive ) {
		VkSampleCountFlags mask = vkMaxSamples;
		int req = r_ext_multisample->integer;
		if ( req < 2 ) req = 2;
		else if ( req == 3 || req == 5 || req == 6 || req == 7 ) req = ( req <= 4 ) ? 4 : 8;
		else if ( req > 16 ) req = 16;
		vkSamples = MAX( log2pad( req, 1 ), VK_SAMPLE_COUNT_2_BIT );
		while ( (VkSampleCountFlags)vkSamples > mask )
				vkSamples >>= 1;
		vk.msaaSampleShading = ( r_msaa_sample_shading && r_msaa_sample_shading->integer ) ? qtrue : qfalse;
		if ( vk.msaaSampleShading ) {
			ri.Printf( PRINT_ALL, "...using %ix MSAA (sample shading %.2f)\n",
				vkSamples, vk_get_msaa_min_sample_shading() );
		} else {
			ri.Printf( PRINT_ALL, "...using %ix MSAA\n", vkSamples );
		}
	} else {
		vkSamples = VK_SAMPLE_COUNT_1_BIT;
		vk.msaaSampleShading = qfalse;
	}
	if ( vk.smaaActive ) {
			int p = r_smaa_preset && r_smaa_preset->integer >= 1 && r_smaa_preset->integer <= 4 ? r_smaa_preset->integer : 0;
			const char *preset_name = ( p == 1 ) ? "Low" : ( p == 2 ) ? "Medium" : ( p == 3 ) ? "High" : ( p == 4 ) ? "Ultra" : "Custom";
			float thresh = p ? ( p == 1 ? 0.15f : p == 2 ? 0.1f : p == 3 ? 0.08f : 0.05f ) : ( r_smaa_threshold ? r_smaa_threshold->value : 0.1f );
			int search = p ? ( p == 1 ? 8 : p == 2 ? 16 : p == 3 ? 24 : 32 ) : ( r_smaa_max_search_steps && r_smaa_max_search_steps->integer ? r_smaa_max_search_steps->integer : 16 );
			ri.Printf( PRINT_ALL, "...SMAA enabled (preset=%s, threshold %.2f, search %d)\n", preset_name, thresh, search );
	}
	if ( vk.msaaActive && vk.smaaActive ) {
		ri.Printf( PRINT_ALL, "...MSAA (geometry) + SMAA (alpha/transparency) for best edge quality\n" );
	}

	vk.screenMapSamples = MIN( vkMaxSamples, VK_SAMPLE_COUNT_4_BIT );

	{
		uint32_t screenMapScale = 2;
		if ( r_screenMapScale && r_screenMapScale->integer > 0 ) {
			screenMapScale = (uint32_t)r_screenMapScale->integer;
		}

		vk.screenMapWidth = ( glConfig.vidWidth + screenMapScale - 1 ) / screenMapScale;
		vk.screenMapHeight = ( glConfig.vidHeight + screenMapScale - 1 ) / screenMapScale;
	}

	if ( vk.screenMapWidth < 4 )
		vk.screenMapWidth = 4;

	if ( vk.screenMapHeight < 4 )
		vk.screenMapHeight = 4;

	ri.Printf( PRINT_ALL, "...screenMap size: %ux%u (r_screenMapScale %d)\n",
		vk.screenMapWidth, vk.screenMapHeight,
		( r_screenMapScale && r_screenMapScale->integer > 0 ) ? r_screenMapScale->integer : 2 );

	vk.defaults.geometry_size = VERTEX_BUFFER_SIZE;
	vk.defaults.staging_size = STAGING_BUFFER_SIZE;

	// get memory size & defaults
	{
		VkPhysicalDeviceMemoryProperties mem_props;
		VkDeviceSize maxDedicatedSize = 0;
		VkDeviceSize maxBARSize = 0;
		qvkGetPhysicalDeviceMemoryProperties( vk.physical_device, &mem_props );
		for ( i = 0; i < mem_props.memoryTypeCount; i++ ) {
			if ( mem_props.memoryTypes[i].propertyFlags == VK_MEMORY_HEAP_DEVICE_LOCAL_BIT ) {
				maxDedicatedSize = mem_props.memoryHeaps[mem_props.memoryTypes[i].heapIndex].size;
			}
			else if ( mem_props.memoryTypes[i].propertyFlags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT ) {
				if ( maxDedicatedSize == 0 || mem_props.memoryHeaps[mem_props.memoryTypes[i].heapIndex].size > maxDedicatedSize ) {
					maxDedicatedSize = mem_props.memoryHeaps[mem_props.memoryTypes[i].heapIndex].size;
				}
			}
			if ( mem_props.memoryTypes[i].propertyFlags == (VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ) {
				maxBARSize = mem_props.memoryHeaps[mem_props.memoryTypes[i].heapIndex].size;
			}
			else if ( (mem_props.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) == (VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ) {
				if ( maxBARSize == 0 ) {
					maxBARSize = mem_props.memoryHeaps[mem_props.memoryTypes[i].heapIndex].size;
				}
			}
		}

		if ( maxDedicatedSize != 0 ) {
			ri.Printf( PRINT_ALL, "...device memory size: %iMB\n", (int)((maxDedicatedSize + (1024 * 1024) - 1) / (1024 * 1024)) );
		}
		if ( maxBARSize != 0 ) {
			if ( maxBARSize >= 128 * 1024 * 1024 ) {
				// user larger buffers to avoid potential reallocations
				vk.defaults.geometry_size = VERTEX_BUFFER_SIZE_HI;
				vk.defaults.staging_size = STAGING_BUFFER_SIZE_HI;
			}
#ifdef _DEBUG
			ri.Printf( PRINT_ALL, "...BAR memory size: %iMB\n", (int)((maxBARSize + (1024 * 1024) - 1) / (1024 * 1024)) );
#endif
		}
	}

	// fill glConfig information

	// maxTextureSize should respect physical limits; we clamp to our MAX_TEXTURE_SIZE
	glConfig.maxTextureSize = MIN( props.limits.maxImageDimension2D, MAX_TEXTURE_SIZE );

	if ( glConfig.maxTextureSize > MAX_TEXTURE_SIZE )
		glConfig.maxTextureSize = MAX_TEXTURE_SIZE; // ResampleTexture() relies on that maximum

	// default chunk size, may be doubled on demand
	vk.image_chunk_size = IMAGE_CHUNK_SIZE;

	vk.maxLod = 1 + Q_log2( glConfig.maxTextureSize );

	if ( props.limits.maxPerStageDescriptorSamplers != 0xFFFFFFFF )
		glConfig.numTextureUnits = props.limits.maxPerStageDescriptorSamplers;
	else
		glConfig.numTextureUnits = props.limits.maxBoundDescriptorSets;
	if ( glConfig.numTextureUnits > MAX_TEXTURE_UNITS )
		glConfig.numTextureUnits = MAX_TEXTURE_UNITS;

	vk.maxBoundDescriptorSets = props.limits.maxBoundDescriptorSets;

#ifdef USE_VK_PBR
	// Decide PBR activation and print a clear reason if disabled.
	vk.pbrActive = qfalse;
	if ( r_pbr->integer ) {
		if ( !vk.fboActive ) {
			ri.Printf( PRINT_ALL, S_COLOR_YELLOW "PBR: disabled (requires r_fbo 1). Use: set r_fbo 1; vid_restart\n" S_COLOR_WHITE );
		} else if ( vk.maxBoundDescriptorSets < 10 ) {
			ri.Printf( PRINT_ALL, S_COLOR_YELLOW "PBR: disabled (insufficient descriptor sets: have %u, need >= 10)\n" S_COLOR_WHITE,
				(unsigned)vk.maxBoundDescriptorSets );
		} else {
			vk.pbrActive = qtrue;
			ri.Printf( PRINT_ALL, "PBR: enabled\n" );
		}
	} else {
		ri.Printf( PRINT_ALL, "PBR: disabled (r_pbr 0)\n" );
	}

#ifdef VK_CUBEMAP
	if ( vk.pbrActive && r_cubeMapping->integer )
		vk.cubemapActive = qtrue;
#endif
#endif

	glConfig.textureEnvAddAvailable = qtrue;
	if ( r_ext_texture_env_add->integer != 0 )
		glConfig.textureEnvAddAvailable = qtrue;
	else
		glConfig.textureEnvAddAvailable = qfalse;

	glConfig.textureCompression = TC_NONE;

	major = VK_VERSION_MAJOR(props.apiVersion);
	minor = VK_VERSION_MINOR(props.apiVersion);
	patch = VK_VERSION_PATCH(props.apiVersion);

	// decode driver version
	switch ( props.vendorID ) {
		case 0x10DE: // NVidia
			Com_sprintf( driver_version, sizeof( driver_version ), "%i.%i.%i.%i",
				(props.driverVersion >> 22) & 0x3FF,
				(props.driverVersion >> 14) & 0x0FF,
				(props.driverVersion >> 6) & 0x0FF,
				(props.driverVersion >> 0) & 0x03F );
			break;
#ifdef _WIN32
		case 0x8086: // Intel
			Com_sprintf( driver_version, sizeof( driver_version ), "%i.%i",
				(props.driverVersion >> 14),
				(props.driverVersion >> 0) & 0x3FFF );
			break;
#endif
		default:
			Com_sprintf( driver_version, sizeof( driver_version ), "%i.%i.%i",
				(props.driverVersion >> 22),
				(props.driverVersion >> 12) & 0x3FF,
				(props.driverVersion >> 0) & 0xFFF );
	}

	Com_sprintf( glConfig.version_string, sizeof( glConfig.version_string ), "API: %i.%i.%i, Driver: %s",
		major, minor, patch, driver_version );

#ifdef _WIN32
	// Intel iGPU drivers from 101.5333 to 101.6737 have a known bug that causes
	// VK_ERROR_DEVICE_LOST during vkQueueSubmit, see https://github.com/ec-/Quake3e/issues/312
	if ( props.vendorID == 0x8086 ) {
		uint32_t drvMajor = props.driverVersion >> 14;
		uint32_t drvMinor = props.driverVersion & 0x3FFF;
		if ( drvMajor == 101 && drvMinor >= 5333 && drvMinor <= 6737 ) {
			// NOTE: vk.driverNote is never read anywhere, so this warning
			// currently only populates an unused buffer. Print the guidance
			// to the console/log during initialization instead.
			Com_sprintf( vk.driverNote, sizeof( vk.driverNote ), S_COLOR_WARNING
				"\nWARNING: Intel driver %i.%i is known to cause Vulkan crashes.\n"
				"Consider updating to driver >= 101.6790 or downgrading to <= 101.5186.\n",
				drvMajor, drvMinor );
		}
	}
#endif

	vk.offscreenRender = qtrue;

	if ( props.vendorID == 0x1002 ) {
		vendor_name = "Advanced Micro Devices, Inc.";
	} else if ( props.vendorID == 0x106B ) {
		vendor_name = "Apple Inc.";
	} else if ( props.vendorID == 0x10DE ) {
		// https://github.com/SaschaWillems/Vulkan/issues/493
		// we can't render to offscreen presentation surfaces on nvidia
		vk.offscreenRender = qfalse;
		vendor_name = "NVIDIA";
	} else if ( props.vendorID == 0x14E4 ) {
		vendor_name = "Broadcom Inc.";
	} else if ( props.vendorID == 0x1AE0 ) {
		vendor_name = "Google Inc.";
	} else if ( props.vendorID == 0x8086 ) {
		vendor_name = "Intel Corporation";
	} else if ( props.vendorID == VK_VENDOR_ID_MESA ) {
		vendor_name = "MESA";
	} else {
		Com_sprintf( buf, sizeof( buf ), "VendorID: %04x", props.vendorID );
		vendor_name = buf;
	}

	Q_strncpyz( glConfig.vendor_string, vendor_name, sizeof( glConfig.vendor_string ) );
	Q_strncpyz( glConfig.renderer_string, vk_device_renderer_name( &props ), sizeof( glConfig.renderer_string ) );

	SET_OBJECT_NAME( (intptr_t)vk.device, glConfig.renderer_string, VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_EXT );

	// Structured Vulkan logging
	{
		cvar_t *logVerbosity = ri.Cvar_Get( "log_verbosity", "1", CVAR_ARCHIVE );
		if ( logVerbosity && logVerbosity->integer >= 1 ) {
			uint32_t vramMB = 0;
			VkPhysicalDeviceMemoryProperties mem_props;
			qvkGetPhysicalDeviceMemoryProperties( vk.physical_device, &mem_props );
			for ( i = 0; i < mem_props.memoryTypeCount; i++ ) {
				if ( mem_props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ) {
					uint32_t heapIdx = mem_props.memoryTypes[i].heapIndex;
					if ( heapIdx < mem_props.memoryHeapCount ) {
						VkDeviceSize size = mem_props.memoryHeaps[heapIdx].size;
						if ( size > vramMB ) {
							vramMB = (uint32_t)(size / (1024 * 1024));
						}
					}
				}
			}
			
			ri.Printf( PRINT_ALL, "[VK] VK_Init\n" );
			ri.Printf( PRINT_ALL, "[VK]   API Version : %d.%d.%d\n", major, minor, patch );
			ri.Printf( PRINT_ALL, "[VK]   Driver      : %s\n", driver_version );
			ri.Printf( PRINT_ALL, "[VK]   GPU         : %s\n", vk_device_renderer_name( &props ) );
			if ( vramMB > 0 ) {
				ri.Printf( PRINT_ALL, "[VK]   VRAM        : %u MB\n", vramMB );
			}
			ri.Printf( PRINT_ALL, "[VK]   Renderer    : vulkan\n" );
			
			if ( logVerbosity->integer >= 2 ) {
				VkPhysicalDeviceFeatures features;
				qvkGetPhysicalDeviceFeatures( vk.physical_device, &features );
				ri.Printf( PRINT_ALL, "[VK] VK_Features\n" );
				ri.Printf( PRINT_ALL, "[VK]   Geometry Shader     : %s\n",
					features.geometryShader ? "yes" : "no" );
				ri.Printf( PRINT_ALL, "[VK]   Tessellation Shader : %s\n",
					features.tessellationShader ? "yes" : "no" );
				ri.Printf( PRINT_ALL, "[VK]   Multi Viewport      : %s\n",
					features.multiViewport ? "yes" : "no" );
				ri.Printf( PRINT_ALL, "[VK]   Sampler Anisotropy  : %s\n",
					features.samplerAnisotropy ? "yes" : "no" );
#ifdef VK_KHR_RAY_TRACING_PIPELINE
				ri.Printf( PRINT_ALL, "[VK]   Ray Tracing         : available\n" );
#else
				ri.Printf( PRINT_ALL, "[VK]   Ray Tracing         : not available\n" );
#endif
				ri.Printf( PRINT_ALL, "[VK]   Compute Pipelines   : enabled\n" );
				ri.Printf( PRINT_ALL, "[VK]   First Person Rendering : enabled (r_firstPersonFov, r_firstPersonScale, r_firstPersonZNear)\n" );
			}
		}
	}

	// do early texture mode setup to avoid redundant descriptor updates in GL_SetDefaultState()
	vk.samplers.filter_min = -1;
	vk.samplers.filter_max = -1;
	vk.samplers.mip_lod_bias = -9999.0f;
	GL_TextureMode( r_textureMode->string );
	r_textureMode->modified = qfalse;

	//
	// Sync primitives.
	//
	vk_create_sync_primitives();

	//
	// Command pool.
	//
	{
		VkCommandPoolCreateInfo desc;

		desc.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		desc.queueFamilyIndex = vk.queue_family_index;

		VK_CHECK( qvkCreateCommandPool( vk.device, &desc, NULL, &vk.command_pool ) );

		SET_OBJECT_NAME( vk.command_pool, "command pool", VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_POOL_EXT );
	}

#ifdef USE_UPLOAD_QUEUE
	{
		VkCommandBufferAllocateInfo alloc_info;

		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.pNext = NULL;
		alloc_info.commandPool = vk.command_pool;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = 1;

		VK_CHECK( qvkAllocateCommandBuffers( vk.device, &alloc_info, &vk.staging_command_buffer ) );
	}
#endif

	//
	// Command buffers and color attachments.
	//
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ )
	{
		VkCommandBufferAllocateInfo alloc_info;

		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.pNext = NULL;
		alloc_info.commandPool = vk.command_pool;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = 1;

		VK_CHECK( qvkAllocateCommandBuffers( vk.device, &alloc_info, &vk.tess[i].command_buffer ) );

		//SET_OBJECT_NAME( vk.tess[i].command_buffer, va( "command buffer %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT );
	}

	vk.volumetric_query_pool = VK_NULL_HANDLE;
	vk.volumetric_timestamp_period_ns = props.limits.timestampPeriod;
	vk.volumetric_total_ms = 0.0f;
	vk.volumetric_fluid_ms = 0.0f;
	vk.fluid_dynamic_resolution_scale = 1.0f;
	vk.fluid_dynamic_pressure_iterations = ( r_fogFluidPressureIterations ) ? r_fogFluidPressureIterations->integer : 12;
	if ( vk.fluid_dynamic_pressure_iterations < 1 ) {
		vk.fluid_dynamic_pressure_iterations = 1;
	}
	Com_Memset( vk.volumetric_stage_ms, 0, sizeof( vk.volumetric_stage_ms ) );
	if ( qvkCreateQueryPool && qvkGetQueryPoolResults && qvkCmdWriteTimestamp && qvkCmdResetQueryPool &&
		props.limits.timestampComputeAndGraphics ) {
		VkQueryPoolCreateInfo query_desc;
		Com_Memset( &query_desc, 0, sizeof( query_desc ) );
		query_desc.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		query_desc.queryType = VK_QUERY_TYPE_TIMESTAMP;
		query_desc.queryCount = VK_VOLUMETRIC_QUERY_COUNT;
		if ( qvkCreateQueryPool( vk.device, &query_desc, NULL, &vk.volumetric_query_pool ) == VK_SUCCESS ) {
			SET_OBJECT_NAME( vk.volumetric_query_pool, "volumetric timestamp query pool", VK_DEBUG_REPORT_OBJECT_TYPE_QUERY_POOL_EXT );
			if ( qvkResetQueryPoolEXT )
				qvkResetQueryPoolEXT( vk.device, vk.volumetric_query_pool, 0, VK_VOLUMETRIC_QUERY_COUNT );
		} else {
			vk.volumetric_query_pool = VK_NULL_HANDLE;
		}
	}

	/* Occlusion query pool for entity culling */
	if ( qvkCreateQueryPool && qvkGetQueryPoolResults && qvkCmdBeginQuery && qvkCmdEndQuery && qvkCmdResetQueryPool ) {
		VkQueryPoolCreateInfo query_desc;
		Com_Memset( &query_desc, 0, sizeof( query_desc ) );
		query_desc.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		query_desc.queryType = VK_QUERY_TYPE_OCCLUSION;
		query_desc.queryCount = MAX_REFENTITIES;
		if ( qvkCreateQueryPool( vk.device, &query_desc, NULL, &vk.occlusion_query_pool ) == VK_SUCCESS ) {
			SET_OBJECT_NAME( vk.occlusion_query_pool, "occlusion query pool", VK_DEBUG_REPORT_OBJECT_TYPE_QUERY_POOL_EXT );
			if ( qvkResetQueryPoolEXT )
				qvkResetQueryPoolEXT( vk.device, vk.occlusion_query_pool, 0, MAX_REFENTITIES );
			vk_occlusion_seed_visibility_all_visible();  /* first frame: all visible */
			ri.Printf( PRINT_ALL, "GPU occlusion culling available (r_occlusionCulling 0=off 1=on)\n" );
		} else {
			vk.occlusion_query_pool = VK_NULL_HANDLE;
		}
	} else {
		vk.occlusion_query_pool = VK_NULL_HANDLE;
	}

	//
	// Descriptor pool.
	//
		{
			VkDescriptorPoolSize pool_size[5];
		VkDescriptorPoolCreateInfo desc;
		uint32_t j, maxSets;

		pool_size[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			pool_size[0].descriptorCount = MAX_DRAWIMAGES + NUM_COMMAND_BUFFERS + NUM_COMMAND_BUFFERS + NUM_COMMAND_BUFFERS + 3 + 6 + VK_NUM_BLOOM_PASSES * 2 + 32; // color[N], post_color[N], depth[N], screenmap, ssao, volumetric, bloom, SMAA, TAA
#ifdef USE_VK_PBR
        if ( vk.pbrActive )
            pool_size[0].descriptorCount += 2 + ( MAX_DRAWIMAGES * 9 ); // brdf-lut + irradiance | MAX_DRAWIMAGES * (physical, normal, emissive, clearcoat, sheen, anisotropy, transmission, subsurface, detail)
#endif

		pool_size[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		pool_size[1].descriptorCount = NUM_COMMAND_BUFFERS * 2; // main + camera

		//pool_size[2].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		//pool_size[2].descriptorCount = NUM_COMMAND_BUFFERS;

		pool_size[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
		pool_size[2].descriptorCount = 1 + NUM_COMMAND_BUFFERS * 2; // flare storage + IQM skin/morph

		pool_size[3].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			pool_size[3].descriptorCount = 22 + NUM_COMMAND_BUFFERS;	/* luminance[N] binding 1 */

		pool_size[4].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		pool_size[4].descriptorCount = 8 + NUM_COMMAND_BUFFERS;

		for ( j = 0, maxSets = 0; j < ARRAY_LEN( pool_size ); j++ ) {
			maxSets += pool_size[j].descriptorCount;
		}

		desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.maxSets = maxSets;
		desc.poolSizeCount = ARRAY_LEN( pool_size );
		desc.pPoolSizes = pool_size;

		VK_CHECK( qvkCreateDescriptorPool( vk.device, &desc, NULL, &vk.descriptor_pool ) );
	}

	//
	// Descriptor set layout.
	//
	vk_create_layout_binding( 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &vk.set_layout_sampler );
	vk_create_uniform_layout( &vk.set_layout_uniform );
	vk_create_layout_binding( 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, &vk.set_layout_storage );
	vk_create_layout_binding( 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, &vk.set_layout_postfx_uniform );
	//vk_create_layout_binding( 0, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT, &vk.set_layout_input );

		{
			VkDescriptorSetLayoutBinding compute_bindings[17];
		VkDescriptorSetLayoutCreateInfo compute_layout_desc;

		compute_bindings[0].binding = 0;
		compute_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		compute_bindings[0].descriptorCount = 1;
		compute_bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		compute_bindings[0].pImmutableSamplers = NULL;

		compute_bindings[1].binding = 1;
		compute_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		compute_bindings[1].descriptorCount = 1;
		compute_bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		compute_bindings[1].pImmutableSamplers = NULL;

		compute_bindings[2].binding = 2;
		compute_bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		compute_bindings[2].descriptorCount = 1;
		compute_bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		compute_bindings[2].pImmutableSamplers = NULL;

		compute_bindings[3].binding = 3;
		compute_bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		compute_bindings[3].descriptorCount = 1;
		compute_bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		compute_bindings[3].pImmutableSamplers = NULL;

		compute_bindings[4].binding = 4;
		compute_bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		compute_bindings[4].descriptorCount = 1;
		compute_bindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		compute_bindings[4].pImmutableSamplers = NULL;

		compute_bindings[5].binding = 5;
		compute_bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		compute_bindings[5].descriptorCount = 1;
		compute_bindings[5].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		compute_bindings[5].pImmutableSamplers = NULL;

			compute_bindings[6].binding = 6;
			compute_bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			compute_bindings[6].descriptorCount = 1;
			compute_bindings[6].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			compute_bindings[6].pImmutableSamplers = NULL;

			compute_bindings[7].binding = 7;
			compute_bindings[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			compute_bindings[7].descriptorCount = 1;
			compute_bindings[7].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			compute_bindings[7].pImmutableSamplers = NULL;

			compute_bindings[8].binding = 8;
			compute_bindings[8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			compute_bindings[8].descriptorCount = 1;
			compute_bindings[8].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			compute_bindings[8].pImmutableSamplers = NULL;

			compute_bindings[9].binding = 9;
			compute_bindings[9].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			compute_bindings[9].descriptorCount = 1;
			compute_bindings[9].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			compute_bindings[9].pImmutableSamplers = NULL;

			compute_bindings[10].binding = 10;
			compute_bindings[10].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			compute_bindings[10].descriptorCount = 1;
			compute_bindings[10].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			compute_bindings[10].pImmutableSamplers = NULL;

			compute_bindings[11].binding = 11;
			compute_bindings[11].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			compute_bindings[11].descriptorCount = 1;
			compute_bindings[11].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			compute_bindings[11].pImmutableSamplers = NULL;

			compute_bindings[12].binding = 12;
			compute_bindings[12].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			compute_bindings[12].descriptorCount = 1;
			compute_bindings[12].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			compute_bindings[12].pImmutableSamplers = NULL;

			compute_bindings[13].binding = 13;
			compute_bindings[13].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			compute_bindings[13].descriptorCount = 1;
			compute_bindings[13].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			compute_bindings[13].pImmutableSamplers = NULL;

			compute_bindings[14].binding = 14;
			compute_bindings[14].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			compute_bindings[14].descriptorCount = 1;
			compute_bindings[14].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			compute_bindings[14].pImmutableSamplers = NULL;

			compute_bindings[15].binding = 15;
			compute_bindings[15].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			compute_bindings[15].descriptorCount = 1;
			compute_bindings[15].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			compute_bindings[15].pImmutableSamplers = NULL;

			compute_bindings[16].binding = 16;
			compute_bindings[16].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			compute_bindings[16].descriptorCount = 1;
			compute_bindings[16].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			compute_bindings[16].pImmutableSamplers = NULL;

		compute_layout_desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		compute_layout_desc.pNext = NULL;
		compute_layout_desc.flags = 0;
		compute_layout_desc.bindingCount = ARRAY_LEN( compute_bindings );
		compute_layout_desc.pBindings = compute_bindings;

		VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &compute_layout_desc, NULL, &vk.volumetric_compute_layout ) );
	}

	{
		VkDescriptorSetLayoutBinding fluid_bindings[16];
		VkDescriptorSetLayoutCreateInfo fluid_layout_desc;

			for ( uint32_t fluid_binding_index = 0; fluid_binding_index < ARRAY_LEN( fluid_bindings ); fluid_binding_index++ ) {
				fluid_bindings[fluid_binding_index].binding = fluid_binding_index;
				fluid_bindings[fluid_binding_index].descriptorCount = 1;
				fluid_bindings[fluid_binding_index].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
				fluid_bindings[fluid_binding_index].pImmutableSamplers = NULL;
				fluid_bindings[fluid_binding_index].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			}

		fluid_bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		fluid_bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		fluid_bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		fluid_bindings[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		fluid_bindings[10].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		fluid_bindings[11].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		fluid_bindings[12].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		fluid_bindings[14].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		fluid_bindings[15].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

		Com_Memset( &fluid_layout_desc, 0, sizeof( fluid_layout_desc ) );
		fluid_layout_desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		fluid_layout_desc.bindingCount = ARRAY_LEN( fluid_bindings );
		fluid_layout_desc.pBindings = fluid_bindings;

		VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &fluid_layout_desc, NULL, &vk.volumetric_fluid_layout ) );
	}

	{
		VkDescriptorSetLayoutBinding cbt_bindings[4];
		VkDescriptorSetLayoutCreateInfo cbt_layout_desc;

		cbt_bindings[0].binding = 0;
		cbt_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		cbt_bindings[0].descriptorCount = 1;
		cbt_bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		cbt_bindings[0].pImmutableSamplers = NULL;

		cbt_bindings[1].binding = 1;
		cbt_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		cbt_bindings[1].descriptorCount = 1;
		cbt_bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		cbt_bindings[1].pImmutableSamplers = NULL;

		cbt_bindings[2].binding = 2;
		cbt_bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		cbt_bindings[2].descriptorCount = 1;
		cbt_bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		cbt_bindings[2].pImmutableSamplers = NULL;

		cbt_bindings[3].binding = 3;
		cbt_bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		cbt_bindings[3].descriptorCount = 1;
		cbt_bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		cbt_bindings[3].pImmutableSamplers = NULL;

		cbt_layout_desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		cbt_layout_desc.pNext = NULL;
		cbt_layout_desc.flags = 0;
		cbt_layout_desc.bindingCount = ARRAY_LEN( cbt_bindings );
		cbt_layout_desc.pBindings = cbt_bindings;

		VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &cbt_layout_desc, NULL, &vk.cbt_terrain_layout ) );
	}

	/* Vegetation wind: storage buffer for VegetationVertex (positionFlex + normalPhase) */
	if ( vk.modules.vegetation_wind_cs != VK_NULL_HANDLE ) {
		VkDescriptorSetLayoutBinding vegwind_binding;
		VkDescriptorSetLayoutCreateInfo vegwind_layout_desc;

		Com_Memset( &vegwind_binding, 0, sizeof( vegwind_binding ) );
		vegwind_binding.binding = 0;
		vegwind_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		vegwind_binding.descriptorCount = 1;
		vegwind_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

		Com_Memset( &vegwind_layout_desc, 0, sizeof( vegwind_layout_desc ) );
		vegwind_layout_desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		vegwind_layout_desc.bindingCount = 1;
		vegwind_layout_desc.pBindings = &vegwind_binding;

		VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &vegwind_layout_desc, NULL, &vk.vegwind_layout ) );
	}

			{
				VkDescriptorSetLayoutBinding composite_bindings[9];
			VkDescriptorSetLayoutCreateInfo composite_layout_desc;

		composite_bindings[0].binding = 0;
		composite_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		composite_bindings[0].descriptorCount = 1;
		composite_bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		composite_bindings[0].pImmutableSamplers = NULL;

		composite_bindings[1].binding = 1;
		composite_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		composite_bindings[1].descriptorCount = 1;
		composite_bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		composite_bindings[1].pImmutableSamplers = NULL;

		composite_bindings[2].binding = 2;
		composite_bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		composite_bindings[2].descriptorCount = 1;
		composite_bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		composite_bindings[2].pImmutableSamplers = NULL;

			composite_bindings[3].binding = 3;
			composite_bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			composite_bindings[3].descriptorCount = 1;
			composite_bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			composite_bindings[3].pImmutableSamplers = NULL;

			composite_bindings[4].binding = 4;
			composite_bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			composite_bindings[4].descriptorCount = 1;
			composite_bindings[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			composite_bindings[4].pImmutableSamplers = NULL;

			composite_bindings[5].binding = 5;
			composite_bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			composite_bindings[5].descriptorCount = 1;
			composite_bindings[5].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			composite_bindings[5].pImmutableSamplers = NULL;

			composite_bindings[6].binding = 6;
			composite_bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			composite_bindings[6].descriptorCount = 1;
			composite_bindings[6].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			composite_bindings[6].pImmutableSamplers = NULL;

			composite_bindings[7].binding = 7;
			composite_bindings[7].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			composite_bindings[7].descriptorCount = 1;
			composite_bindings[7].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			composite_bindings[7].pImmutableSamplers = NULL;

			composite_bindings[8].binding = 8;
			composite_bindings[8].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			composite_bindings[8].descriptorCount = 1;
			composite_bindings[8].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			composite_bindings[8].pImmutableSamplers = NULL;

			composite_layout_desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			composite_layout_desc.pNext = NULL;
			composite_layout_desc.flags = 0;
			composite_layout_desc.bindingCount = ARRAY_LEN( composite_bindings );
			composite_layout_desc.pBindings = composite_bindings;

				VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &composite_layout_desc, NULL, &vk.volumetric_composite_layout ) );
			}

			{
				VkDescriptorSetLayoutBinding resolve_bindings[2];
				VkDescriptorSetLayoutCreateInfo resolve_layout_desc;

				resolve_bindings[0].binding = 0;
				resolve_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				resolve_bindings[0].descriptorCount = 1;
				resolve_bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
				resolve_bindings[0].pImmutableSamplers = NULL;

				resolve_bindings[1].binding = 1;
				resolve_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				resolve_bindings[1].descriptorCount = 1;
				resolve_bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
				resolve_bindings[1].pImmutableSamplers = NULL;

				resolve_layout_desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
				resolve_layout_desc.pNext = NULL;
				resolve_layout_desc.flags = 0;
				resolve_layout_desc.bindingCount = ARRAY_LEN( resolve_bindings );
				resolve_layout_desc.pBindings = resolve_bindings;

				VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &resolve_layout_desc, NULL, &vk.volumetric_depth_resolve_layout ) );
			}

			/* Luminance pass for eye adaptation */
			{
				VkDescriptorSetLayoutBinding lum_bindings[2];
				VkDescriptorSetLayoutCreateInfo lum_layout_desc;

				lum_bindings[0].binding = 0;
				lum_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				lum_bindings[0].descriptorCount = 1;
				lum_bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
				lum_bindings[0].pImmutableSamplers = NULL;

				lum_bindings[1].binding = 1;
				lum_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				lum_bindings[1].descriptorCount = 1;
				lum_bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
				lum_bindings[1].pImmutableSamplers = NULL;

				Com_Memset( &lum_layout_desc, 0, sizeof( lum_layout_desc ) );
				lum_layout_desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
				lum_layout_desc.bindingCount = ARRAY_LEN( lum_bindings );
				lum_layout_desc.pBindings = lum_bindings;

				VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &lum_layout_desc, NULL, &vk.luminance_layout ) );
			}

			/* old composite/resolve block removed below */

	vk_create_volumetric_params_buffer();

	//
	// Pipeline layouts.
	//
	{
		VkDescriptorSetLayout set_layouts[VK_DESC_COUNT];
		VkPipelineLayoutCreateInfo desc;
		VkPushConstantRange push_range;

		push_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		push_range.offset = 0;
		push_range.size = 128; // current + previous MVP matrices

		// standard pipelines

		set_layouts[0] = vk.set_layout_uniform; // fog/dlight parameters
		set_layouts[1] = vk.set_layout_sampler; // diffuse
		set_layouts[2] = vk.set_layout_sampler; // lightmap / fog-only
		set_layouts[3] = vk.set_layout_sampler; // blend
		set_layouts[4] = vk.set_layout_sampler; // collapsed fog texture
#ifdef USE_VK_PBR
		set_layouts[5] = vk.set_layout_sampler; // brdf lut
		set_layouts[6] = vk.set_layout_sampler; // normalMap
		set_layouts[7] = vk.set_layout_sampler; // physicalMap
		set_layouts[8] = vk.set_layout_sampler; // prefiltered envmap
		set_layouts[9] = vk.set_layout_sampler; // deluxemap
		set_layouts[10] = vk.set_layout_sampler; // irradiance
		set_layouts[11] = vk.set_layout_sampler; // emissive
		set_layouts[12] = vk.set_layout_sampler; // clearcoat
		set_layouts[13] = vk.set_layout_sampler; // sheen
		set_layouts[14] = vk.set_layout_sampler; // anisotropy
		set_layouts[15] = vk.set_layout_sampler; // transmission
		set_layouts[16] = vk.set_layout_sampler; // subsurface
		set_layouts[17] = vk.set_layout_sampler; // detail
#endif
		desc.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.setLayoutCount = (vk.maxBoundDescriptorSets >= VK_DESC_COUNT) ? VK_DESC_COUNT : 4;
		desc.pSetLayouts = set_layouts;
		desc.pushConstantRangeCount = 1;
		desc.pPushConstantRanges = &push_range;

		VK_CHECK(qvkCreatePipelineLayout(vk.device, &desc, NULL, &vk.pipeline_layout));

		// flare test pipeline
		set_layouts[0] = vk.set_layout_storage; // dynamic storage buffer

		desc.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.setLayoutCount = 1;
		desc.pSetLayouts = set_layouts;
		desc.pushConstantRangeCount = 1;
		desc.pPushConstantRanges = &push_range;

		VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.pipeline_layout_storage ) );

		// post-processing pipeline
		set_layouts[0] = vk.set_layout_sampler;        // color sampler
		set_layouts[1] = vk.set_layout_sampler;        // depth / secondary sampler
		set_layouts[2] = vk.set_layout_postfx_uniform; // per-frame postfx params
		set_layouts[3] = vk.set_layout_sampler;        // grading LUT sampler

		push_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		push_range.offset = 0;
		push_range.size = sizeof( VkPostProcessPushConstants );

		desc.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.setLayoutCount = 4;
		desc.pSetLayouts = set_layouts;
		desc.pushConstantRangeCount = 1;
		desc.pPushConstantRanges = &push_range;

		VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.pipeline_layout_post_process ) );

		/* Blend pipeline: 4 sampler sets for texture0..texture3 (blend.frag). Must not reuse postfx_uniform. */
		set_layouts[0] = vk.set_layout_sampler;
		set_layouts[1] = vk.set_layout_sampler;
		set_layouts[2] = vk.set_layout_sampler;
		set_layouts[3] = vk.set_layout_sampler;
		desc.setLayoutCount = VK_NUM_BLOOM_PASSES;
		desc.pushConstantRangeCount = 0;
		desc.pPushConstantRanges = NULL;

		VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.pipeline_layout_blend ) );

		SET_OBJECT_NAME( vk.pipeline_layout, "pipeline layout - main", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );
		SET_OBJECT_NAME( vk.pipeline_layout_post_process, "pipeline layout - post-processing", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );
		SET_OBJECT_NAME( vk.pipeline_layout_blend, "pipeline layout - blend", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );

		{
			VkDescriptorSetLayout smaa_layouts[2];
			VkPipelineLayoutCreateInfo smaa_desc;
			VkPushConstantRange smaa_push_range;

			smaa_layouts[0] = vk.set_layout_sampler;
			smaa_layouts[1] = vk.set_layout_sampler;

			smaa_push_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			smaa_push_range.offset = 0;
			smaa_push_range.size = 16; /* threshold, localContrast, maxSearchSteps, pad */

			Com_Memset( &smaa_desc, 0, sizeof( smaa_desc ) );
			smaa_desc.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			smaa_desc.pNext = NULL;
			smaa_desc.flags = 0;
			smaa_desc.setLayoutCount = ARRAY_LEN( smaa_layouts );
			smaa_desc.pSetLayouts = smaa_layouts;
			smaa_desc.pushConstantRangeCount = 1;
			smaa_desc.pPushConstantRanges = &smaa_push_range;

			VK_CHECK( qvkCreatePipelineLayout( vk.device, &smaa_desc, NULL, &vk.pipeline_layout_smaa ) );
			SET_OBJECT_NAME( vk.pipeline_layout_smaa, "pipeline layout - smaa", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );
		}

		// ssao pipeline layout (depth sampler + push constants)
		set_layouts[0] = vk.set_layout_sampler;
		desc.setLayoutCount = 1;
		desc.pSetLayouts = set_layouts;
		push_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		push_range.offset = 0;
		push_range.size = 64; // ao push constants
		desc.pushConstantRangeCount = 1;
		desc.pPushConstantRanges = &push_range;
		VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.pipeline_layout_ssao ) );
		SET_OBJECT_NAME( vk.pipeline_layout_ssao, "pipeline layout - ssao", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );

		// ssao combine layout (color + ao samplers)
		set_layouts[0] = vk.set_layout_sampler;
		set_layouts[1] = vk.set_layout_sampler;
		desc.setLayoutCount = 2;
		desc.pSetLayouts = set_layouts;
		desc.pushConstantRangeCount = 0;
		desc.pPushConstantRanges = NULL;
		VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.pipeline_layout_ssao_combine ) );
		SET_OBJECT_NAME( vk.pipeline_layout_ssao_combine, "pipeline layout - ssao_combine", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );

		if ( r_oit && r_oit->integer && vk.fboActive ) {
			/* OIT resolve: set 0 = opaque, set 1 = accum, set 2 = revealage */
			set_layouts[0] = vk.set_layout_sampler;
			set_layouts[1] = vk.set_layout_sampler;
			set_layouts[2] = vk.set_layout_sampler;
			desc.setLayoutCount = 3;
			desc.pSetLayouts = set_layouts;
			desc.pushConstantRangeCount = 0;
			desc.pPushConstantRanges = NULL;
			VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.pipeline_layout_oit_resolve ) );
			SET_OBJECT_NAME( vk.pipeline_layout_oit_resolve, "pipeline layout - oit_resolve", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );

			/* OIT accum: set 0 = tex0, set 1 = opaque depth, push constants = mvp + prevMvp (128 bytes) */
			set_layouts[0] = vk.set_layout_sampler;
			set_layouts[1] = vk.set_layout_sampler;
			desc.setLayoutCount = 2;
			desc.pSetLayouts = set_layouts;
			push_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
			push_range.offset = 0;
			push_range.size = 128; /* 2 * mat4 */
			desc.pushConstantRangeCount = 1;
			desc.pPushConstantRanges = &push_range;
			VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.pipeline_layout_oit_accum ) );
			SET_OBJECT_NAME( vk.pipeline_layout_oit_accum, "pipeline layout - oit_accum", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );
		}

		// ssr layout (set 0: color, set 1: depth, push constants: 2 mat4 + 2 vec4 = 160 bytes)
		set_layouts[0] = vk.set_layout_sampler;
		set_layouts[1] = vk.set_layout_sampler;
		desc.setLayoutCount = 2;
		desc.pSetLayouts = set_layouts;
		push_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		push_range.offset = 0;
		push_range.size = 160;
		desc.pushConstantRangeCount = 1;
		desc.pPushConstantRanges = &push_range;
		VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.pipeline_layout_ssr ) );
		SET_OBJECT_NAME( vk.pipeline_layout_ssr, "pipeline layout - ssr", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );

		/* Atmosphere: push constants only (10 vec4s = 160 bytes) */
		desc.setLayoutCount = 0;
		desc.pSetLayouts = NULL;
		push_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		push_range.offset = 0;
		push_range.size = sizeof( vkAtmospherePushConstants_t );
		desc.pushConstantRangeCount = 1;
		desc.pPushConstantRanges = &push_range;
		VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.pipeline_layout_atmosphere ) );
		SET_OBJECT_NAME( vk.pipeline_layout_atmosphere, "pipeline layout - atmosphere", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );
	
#ifdef VK_PBR_BRDFLUT
		if( vk.pbrActive ) {
			desc.setLayoutCount = 1;
			desc.pSetLayouts = &vk.set_layout_sampler;
			desc.pushConstantRangeCount = 0;
			desc.pPushConstantRanges = NULL;
			VK_CHECK(qvkCreatePipelineLayout(vk.device, &desc, NULL, &vk.pipeline_layout_brdflut));
			SET_OBJECT_NAME(vk.pipeline_layout_brdflut, "pipeline layout - brdflut", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT);
		}
#endif	
	}

	vk.geometry_buffer_size_new = vk.defaults.geometry_size;
	vk_create_geometry_buffers( vk.geometry_buffer_size_new );
	vk.geometry_buffer_size_new = 0;

	vk_create_storage_buffer( MAX_FLARES * vk.storage_alignment );

	vk_create_shader_modules();

	{
		VkPipelineCacheCreateInfo ci;
		Com_Memset( &ci, 0, sizeof( ci ) );
		ci.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		VK_CHECK( qvkCreatePipelineCache( vk.device, &ci, NULL, &vk.pipelineCache ) );
	}

	vk.renderPassIndex = RENDER_PASS_MAIN; // default render pass

	// swapchain
	vk.initSwapchainLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	//vk.initSwapchainLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	vk_create_swapchain( vk.physical_device, vk.device, vk_surface, vk.present_format, &vk.swapchain, qtrue );

	// color/depth attachments
	vk_create_attachments();

	// renderpasses
	vk_create_render_passes();

	// framebuffers for each swapchain image
	vk_create_framebuffers();

#ifdef VK_CUBEMAP
	vk_create_cubemap_prefilter();
#endif

	// preallocate staging buffer
	if ( vk.defaults.staging_size == STAGING_BUFFER_SIZE_HI ) {
		vk_alloc_staging_buffer( vk.defaults.staging_size );
	}

	vk.active = qtrue;
}


void vk_create_pipelines( void )
{
	vk_alloc_persistent_pipelines();

	vk.pipelines_world_base = vk.pipelines_count;

	//vk_create_bloom_pipelines();
#ifdef VK_PBR_BRDFLUT
    vk_create_brdflut_pipeline();
#endif
	vk_create_volumetric_pipelines();
}

#ifdef VK_PBR_BRDFLUT
void vk_create_brdflut_pipeline( void )
{
    if( !vk.pbrActive )
        return;
    uint32_t size = 512;
    vk_create_post_process_pipeline( 4, size, size );
}
#endif

/* vk_create_volumetric_pipelines + helpers: vk_volumetric_pipelines.c */



typedef struct {
	float positionFlex[4];
	float normalPhase[4];
} vegwind_vertex_t;

static vegwind_vertex_t vegwind_staging[VEGWIND_MAX_VERTS];
static int vegwind_staging_count;

void vk_vegetation_clear_staging( void )
{
	vegwind_staging_count = 0;
}

void vk_vegetation_add_from_tess( int oldVertexCount, int newVertexCount )
{
	int i, n, count;
	float flex, phase;

	if ( newVertexCount <= oldVertexCount || vegwind_staging_count >= VEGWIND_MAX_VERTS )
		return;

	n = newVertexCount - oldVertexCount;
	count = VEGWIND_MAX_VERTS - vegwind_staging_count;
	if ( n > count )
		n = count;

	for ( i = 0; i < n; i++ ) {
		int v = oldVertexCount + i;
		vegwind_vertex_t *dst = &vegwind_staging[vegwind_staging_count + i];

		dst->positionFlex[0] = tess.xyz[v][0];
		dst->positionFlex[1] = tess.xyz[v][1];
		dst->positionFlex[2] = tess.xyz[v][2];
		/* flexibility: 0=rigid root, 1=flexible tip; use normal Y for grass (up=flexible) */
		flex = tess.normal[v][1];
		dst->positionFlex[3] = ( flex > 0.0f ) ? Com_Clamp( 0.0f, 1.0f, flex ) : 0.5f;

		dst->normalPhase[0] = tess.normal[v][0];
		dst->normalPhase[1] = tess.normal[v][1];
		dst->normalPhase[2] = tess.normal[v][2];
		/* phase offset for variation (hash from position) */
		phase = ( tess.xyz[v][0] * 12.9898f + tess.xyz[v][2] * 78.233f );
		dst->normalPhase[3] = phase - floorf( phase );
	}

	vegwind_staging_count += n;
}

void vk_vegetation_wind_dispatch( void )
{
	typedef struct {
		float windDirection[4];
		float windParams[4];
		float gustParams[4];
		float timeParams[4];
		uint32_t vertexCount;
		uint32_t pad0;
		uint32_t pad1;
		uint32_t pad2;
	} vegwind_push_t;

	vegwind_push_t push;
	uint32_t groupCount;
	const uint32_t localSize = 64;

	if ( !PostFX_VegWind_IsEnabled() || vk.vegwind_pipeline == VK_NULL_HANDLE ||
		vk.vegwind_descriptor == VK_NULL_HANDLE )
	{
		return;
	}
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE )
		return;

	vk_end_render_pass();

	PostFX_VegWind_GetWindDir( &push.windDirection[0], &push.windDirection[1], &push.windDirection[2] );
	push.windDirection[3] = PostFX_VegWind_GetWindStrength();
	push.windParams[0] = PostFX_VegWind_GetPrimaryFreq();
	push.windParams[1] = PostFX_VegWind_GetPrimaryAmp();
	push.windParams[2] = PostFX_VegWind_GetDetailFreq();
	push.windParams[3] = PostFX_VegWind_GetDetailAmp();
	push.gustParams[0] = PostFX_VegWind_GetGustFreq();
	push.gustParams[1] = PostFX_VegWind_GetGustAmp();
	push.gustParams[2] = 0.0f;
	push.gustParams[3] = 0.0f;
	push.timeParams[0] = (float)ri.Milliseconds() / 1000.0f;
	push.timeParams[1] = 0.0f;
	push.timeParams[2] = 0.0f;
	push.timeParams[3] = 0.0f;
	push.vertexCount = vegwind_staging_count;
	push.pad0 = push.pad1 = push.pad2 = 0;

	if ( push.vertexCount > 0 && vk.vegwind_vertex_buffer != VK_NULL_HANDLE ) {
		void *ptr;
		VkDeviceSize uploadSize = (VkDeviceSize)push.vertexCount * VEGWIND_VERTEX_STRIDE;
		if ( qvkMapMemory( vk.device, vk.vegwind_vertex_memory, 0, uploadSize, 0, &ptr ) == VK_SUCCESS ) {
			Com_Memcpy( ptr, vegwind_staging, (size_t)uploadSize );
			qvkUnmapMemory( vk.device, vk.vegwind_vertex_memory );
		}
	}

	groupCount = ( push.vertexCount > 0 ) ? ( ( push.vertexCount + localSize - 1 ) / localSize ) : 1;

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.vegwind_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.pipeline_layout_vegwind, 0, 1, &vk.vegwind_descriptor, 0, NULL );
	qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout_vegwind, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( push ), &push );
	qvkCmdDispatch( vk.cmd->command_buffer, groupCount, 1, 1 );
}

/* vk_shutdown, vk_wait_idle, vk_queue_wait_idle, vk_release_resources: vk_shutdown.c */

VkSampleCountFlagBits vk_get_main_rasterization_samples( void )
{
	return (VkSampleCountFlagBits)vkSamples;
}

#if 0
static void record_buffer_memory_barrier(VkCommandBuffer cb, VkBuffer buffer, VkDeviceSize size, VkDeviceSize offset,
		VkPipelineStageFlags src_stages, VkPipelineStageFlags dst_stages,
		VkAccessFlags src_access, VkAccessFlags dst_access) {

	VkBufferMemoryBarrier barrier;
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.pNext = NULL;
	barrier.srcAccessMask = src_access;
	barrier.dstAccessMask = dst_access;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.buffer = buffer;
	barrier.offset = offset;
	barrier.size = size;

	qvkCmdPipelineBarrier( cb, src_stages, dst_stages, 0, 0, NULL, 1, &barrier, 0, NULL );
}
#endif

/* vk_create_image / vk_upload_* / vk_update_descriptor_set / vk_destroy_image_resources: vk_texture_image.c */

/* vk_set_shader_stage_desc, vk_create_atmosphere_pipeline, vk_create_oit_accum_pipeline, vk_create_blur_pipeline: vk_pipeline_helpers.c */
/* vk_create_pipeline, vk_find_pipeline_ext, vk_get_pipeline_def: vk_create_pipeline.c */



/* vk_occlusion_* / vk_reset_occlusion_visibility: vk_occlusion.c */
/* vk_clear_color, vk_clear_depth, vk_set_color_write_mask: vk_clear_attachments.c */

/* vk_bind_*, vk_alloc_storage, vk_tess_index, vk_*descriptor*, vk_draw_*: vk_draw_state.c */

/* Bloom, SSAO, OIT, SSR passes: vk_postfx_passes.c */



qboolean vk_begin_sun_shadow_render_pass( void )
{
	VkImageAspectFlags depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;

	if ( !vk.fboActive || vk.render_pass.sun_shadow == VK_NULL_HANDLE || vk.framebuffers.sun_shadow == VK_NULL_HANDLE ||
		vk.sun_shadow_image == VK_NULL_HANDLE || vk.sun_shadow_width == 0 || vk.sun_shadow_height == 0 )
	{
		return qfalse;
	}

	if ( glConfig.stencilBits > 0 ) {
		depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	vk_end_render_pass();

	record_image_layout_transition( vk.cmd->command_buffer, vk.sun_shadow_image, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT );

	vk.renderPassIndex = RENDER_PASS_SUN_SHADOW;
	vk.renderWidth = vk.sun_shadow_width;
	vk.renderHeight = vk.sun_shadow_height;
	vk.renderScaleX = vk.renderScaleY = 1.0f;
	vk_begin_render_pass_tracked( vk.render_pass.sun_shadow, vk.framebuffers.sun_shadow, qtrue, vk.renderWidth, vk.renderHeight );

	if ( r_fogDebug && r_fogDebug->integer >= 1 && ( vk.volumetric_frame % 120u ) == 0u ) {
		ri.Printf( PRINT_ALL, "[VK][fog] sun shadow pass begin %ux%u image=0x%llx\n",
			vk.sun_shadow_width, vk.sun_shadow_height,
			(unsigned long long)(uintptr_t)vk.sun_shadow_image );
	}

	return qtrue;
}

void vk_end_sun_shadow_render_pass( void )
{
	VkImageAspectFlags depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;

	if ( !vk.fboActive || vk.sun_shadow_image == VK_NULL_HANDLE ) {
		return;
	}

	if ( glConfig.stencilBits > 0 ) {
		depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	vk_end_render_pass();

	record_image_layout_transition( vk.cmd->command_buffer, vk.sun_shadow_image, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	if ( r_fogDebug && r_fogDebug->integer >= 1 && ( vk.volumetric_frame % 120u ) == 0u ) {
		ri.Printf( PRINT_ALL, "[VK][fog] sun shadow pass end image=0x%llx ATTACHMENT->READ_ONLY\n",
			(unsigned long long)(uintptr_t)vk.sun_shadow_image );
	}

	vk_begin_main_render_pass();
}

/* Local volumetric shadows + froxel compute + composite + SMAA: vk_volumetric_pass_compute.c */

static void vk_begin_screenmap_render_pass( void )
{
	VkFramebuffer frameBuffer = vk.framebuffers.screenmap;

	if ( frameBuffer == VK_NULL_HANDLE || vk.render_pass.screenmap == VK_NULL_HANDLE )
		return;

	vk.renderPassIndex = RENDER_PASS_SCREENMAP;

	vk.renderWidth = vk.screenMapWidth;
	vk.renderHeight = vk.screenMapHeight;

	vk.renderScaleX = (float)vk.renderWidth / (float)glConfig.vidWidth;
	vk.renderScaleY = (float)vk.renderHeight / (float)glConfig.vidHeight;

	vk_begin_render_pass_tracked( vk.render_pass.screenmap, frameBuffer, qtrue, vk.renderWidth, vk.renderHeight );
}

#ifdef VK_CUBEMAP
void vk_begin_cubemap_render_pass( void )
{
    VkFramebuffer frameBuffer = vk.framebuffers.cubemap[backEnd.viewParms.targetCubeLayer];

    vk.renderPassIndex = RENDER_PASS_CUBEMAP;

    vk.renderWidth = REF_CUBEMAP_SIZE;
    vk.renderHeight = REF_CUBEMAP_SIZE;
    vk.renderScaleX = vk.renderScaleY = 1.0f;

    vk_begin_render_pass_tracked(vk.render_pass.cubemap, frameBuffer, qtrue, vk.renderWidth, vk.renderHeight);
}

#endif
#ifdef VK_PBR_BRDFLUT
void vk_create_brfdlut( void )
{
    if( !vk.pbrActive )
        return;

    VkRenderPassBeginInfo   begin_info;
    VkClearValue            clear_values[1];
    VkCommandBuffer         command_buffer;
    VkViewport              viewport;
    VkRect2D                scissor_rect;
    uint32_t                size;

    command_buffer = vk_begin_command_buffer();
    size = 512;
    
    begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.pNext = NULL;
    begin_info.renderPass = vk.render_pass.brdflut;
    begin_info.framebuffer = vk.framebuffers.brdflut;
    begin_info.renderArea.offset.x = 0;
    begin_info.renderArea.offset.y = 0;
    begin_info.renderArea.extent.width = size;
    begin_info.renderArea.extent.height = size;

    Com_Memset( clear_values, 0, sizeof( clear_values ) );
    clear_values[0].color.float32[3] = 1.0f;

    begin_info.clearValueCount = 1;
    begin_info.pClearValues = clear_values;	
	
    Com_Memset( &viewport, 0, sizeof( viewport ) );
    viewport.width = viewport.height = (float)size;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    Com_Memset( &scissor_rect, 0, sizeof( scissor_rect ) );
    scissor_rect.extent.width = scissor_rect.extent.height = size;

    qvkCmdBeginRenderPass( command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE );
    qvkCmdSetScissor( command_buffer, 0, 1, &scissor_rect );
    qvkCmdSetViewport( command_buffer, 0, 1, &viewport ); 
    qvkCmdBindPipeline( command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.brdflut_pipeline );
    qvkCmdBindDescriptorSets( command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_brdflut, 0, 1, &vk.brdflut_image_descriptor, 0, NULL );
    qvkCmdDraw( command_buffer, 4, 1, 0, 0 );	
    qvkCmdEndRenderPass( command_buffer );

    vk_end_command_buffer( command_buffer, __func__  );
}
#endif

void vk_validate_pbr_ibl_resources( void )
{
#ifdef USE_VK_PBR
	if ( !vk.pbrActive ) {
		return;
	}

	{
		const qboolean brdfLutReady = ( vk.brdflut_image != VK_NULL_HANDLE &&
			vk.brdflut_image_view != VK_NULL_HANDLE &&
			vk.brdflut_image_descriptor != VK_NULL_HANDLE );
		const qboolean emptyCubemapReady = ( tr.emptyCubemap != NULL &&
			tr.emptyCubemap->descriptor != VK_NULL_HANDLE );

		ri.Printf( PRINT_ALL, "[VK] PBR IBL: BRDF LUT %s, empty cubemap fallback %s\n",
			brdfLutReady ? "ready" : "missing",
			emptyCubemapReady ? "ready" : "missing" );

#ifdef VK_CUBEMAP
		ri.Printf( PRINT_ALL, "[VK] PBR IBL: runtime cubemap path %s\n",
			vk.cubemapActive ? "enabled" : "disabled" );
		if ( vk.cubemapActive && tr.numCubemaps == 0 ) {
			ri.Printf( PRINT_ALL, "[VK] PBR IBL: no map cubemaps loaded at startup, using fallback until available\n" );
		}
#endif

		if ( !brdfLutReady ) {
			ri.Printf( PRINT_WARNING, "PBR IBL: BRDF LUT resources are incomplete, split-sum specular may fallback\n" );
		}
		if ( !emptyCubemapReady ) {
			ri.Printf( PRINT_WARNING, "PBR IBL: empty cubemap fallback is missing\n" );
		}
	}
#endif
}

void vk_end_render_pass( void )
{
	vk_end_render_pass_tracked();

//	vk.renderPassIndex = RENDER_PASS_MAIN;
}


static qboolean vk_find_screenmap_drawsurfs( void )
{
	const void *curCmd = &backEndData->commands.cmds;
	const drawBufferCommand_t *db_cmd;
	const drawSurfsCommand_t *ds_cmd;

	for ( ;; ) {
		curCmd = PADP( curCmd, sizeof(void *) );
		switch ( *(const int *)curCmd ) {
			case RC_DRAW_BUFFER:
				db_cmd = (const drawBufferCommand_t *)curCmd;
				curCmd = (const void *)(db_cmd + 1);
				break;
			case RC_DRAW_SURFS:
				ds_cmd = (const drawSurfsCommand_t *)curCmd;
				return ds_cmd->refdef.needScreenMap;
			default:
				return qfalse;
		}
	}
}


#ifndef UINT64_MAX
#define UINT64_MAX 0xFFFFFFFFFFFFFFFFULL
#endif

void vk_begin_frame( void )
{
	VkCommandBufferBeginInfo begin_info;
	VkResult res;

	if ( vk.device_lost ) {
		return;
	}
	if ( vk.frame_count++ ) // might happen during stereo rendering
		return;

	/* Ensure render pass state is clean; avoids stale vk.inRenderPass from
	 * previous frame error/early-exit causing draws outside a render pass. */
	vk.inRenderPass = qfalse;

	if (PostFX_NeedsPipelineUpdate()) {
		vk_update_post_process_pipelines();
	}

	vk_begin_motion_frame();
	vk.sun_shadow_valid = qfalse;
	vk.temporal.preparedThisFrame = qfalse;
	vk.uiOverlayActive = qfalse;

#ifdef USE_UPLOAD_QUEUE
	vk_flush_staging_buffer( qtrue );
#endif

	vk.cmd = &vk.tess[ vk.cmd_index ];

	if ( vk.cmd->waitForFence ) {
		vk.cmd->waitForFence = qfalse;
		res = qvkWaitForFences( vk.device, 1, &vk.cmd->rendering_finished_fence, VK_FALSE, 1e10 );
		if ( res != VK_SUCCESS ) {
			if ( res == VK_ERROR_DEVICE_LOST ) {
				vk.device_lost = qtrue;
				ri.Error( ERR_FATAL, "Vulkan: %s returned %s (GPU lost)", "vkWaitForFences", vk_result_string( res ) );
			}
			else {
				ri.Error( ERR_FATAL, "Vulkan: %s returned %s", "vkWaitForFences", vk_result_string( res ) );
			}
		}
		VK_CHECK( qvkResetFences( vk.device, 1, &vk.cmd->rendering_finished_fence ) );
		if ( vk.volumetric_query_pool != VK_NULL_HANDLE &&
			r_volumetricFogPerfTimers && r_volumetricFogPerfTimers->integer ) {
			vk_update_volumetric_perf_queries();
		}
		if ( r_occlusionCulling && r_occlusionCulling->integer ) {
			vk_occlusion_readback();
		}
	}

	if ( !ri.CL_IsMinimized() && !vk.cmd->swapchain_image_acquired ) {
		qboolean retry = qfalse;
_retry:
		res = qvkAcquireNextImageKHR( vk.device, vk.swapchain, 1 * 1000000000ULL, vk.cmd->image_acquired, VK_NULL_HANDLE, &vk.cmd->swapchain_image_index );
		// when running via RDP: "Application has already acquired the maximum number of images (0x2)"
		// probably caused by "device lost" errors
		if ( res < 0 ) {
			if ( res == VK_ERROR_OUT_OF_DATE_KHR && retry == qfalse ) {
				// swapchain re-creation needed
				retry = qtrue;
				vk_restart_swapchain( __func__, res );
				goto _retry;
			} else {
				ri.Error( ERR_FATAL, "vkAcquireNextImageKHR returned %s", vk_result_string( res ) );
			}
		}
		vk.cmd->swapchain_image_acquired = qtrue;
	}

	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.pNext = NULL;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	begin_info.pInheritanceInfo = NULL;

	VK_CHECK( qvkBeginCommandBuffer( vk.cmd->command_buffer, &begin_info ) );
	vk_reset_post_fog_frame_state();

	/* VUID-09401: reset volumetric queries before first use when host reset ext unavailable */
	if ( vk.volumetric_query_pool != VK_NULL_HANDLE && !qvkResetQueryPoolEXT && qvkCmdResetQueryPool ) {
		const uint32_t qbase = vk.cmd_index * VK_VOLUMETRIC_QUERY_SLOTS;
		qvkCmdResetQueryPool( vk.cmd->command_buffer, vk.volumetric_query_pool, qbase, VK_VOLUMETRY_QUERY_USED );
	}

		/* Default color write mask (all enabled) when using VK_EXT_extended_dynamic_state3 */
		if ( vk.colorWriteMaskDynamic && qvkCmdSetColorWriteMaskEXT )
			vk_set_color_write_mask( qtrue, qtrue, qtrue, qtrue );

	// Ensure visibility of geometry buffers writes.
	//record_buffer_memory_barrier( vk.cmd->command_buffer, vk.cmd->vertex_buffer, vk.geometry_buffer_size, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT );

#if 0
	// add explicit layout transition dependency
	if ( vk.fboActive ) {
		record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
	} else {
		record_image_layout_transition( vk.cmd->command_buffer, vk.swapchain_images[ vk.swapchain_image_index ], VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 0, 0 );
	}
#endif

	if ( vk.cmd->vertex_buffer_offset > vk.stats.vertex_buffer_max ) {
		vk.stats.vertex_buffer_max = vk.cmd->vertex_buffer_offset;
	}

	if ( vk.stats.push_size > vk.stats.push_size_max ) {
		vk.stats.push_size_max = vk.stats.push_size;
	}

	vk.cmd->last_pipeline = VK_NULL_HANDLE;

	backEnd.screenMapDone = qfalse;

	if ( PostFX_VegWind_IsEnabled() ) {
		vk_vegetation_wind_dispatch();
	}

	if ( vk_find_screenmap_drawsurfs() ) {
		vk_begin_screenmap_render_pass();
	} else {
vk_begin_main_render_pass();
}


	// dynamic vertex buffer layout
	vk.cmd->uniform_read_offset = 0;
	vk.cmd->vertex_buffer_offset = 0;
	Com_Memset( vk.cmd->vertex_buffer_ptr, 0, 64 );
	vk.cmd->vertex_buffer_offset = PAD( 64, 32 );
	vk_reset_iqm_storage_offsets();
	Com_Memset( vk.cmd->buf_offset, 0, sizeof( vk.cmd->buf_offset ) );
	Com_Memset( vk.cmd->vbo_offset, 0, sizeof( vk.cmd->vbo_offset ) );
	vk.cmd->curr_index_buffer = VK_NULL_HANDLE;
	vk.cmd->curr_index_offset = 0;
	vk.cmd->num_indexes = 0;

	Com_Memset( &vk.cmd->descriptor_set, 0, sizeof( vk.cmd->descriptor_set ) );
	vk.cmd->descriptor_set.start = ~0U;
	//vk.cmd->descriptor_set.end = 0;

	Com_Memset( &vk.cmd->scissor_rect, 0, sizeof( vk.cmd->scissor_rect ) );

	// other stats
	vk.stats.push_size = 0;
}

void vk_prepare_frame_temporal_state( void )
{
	qboolean reset_taa = qfalse;

	if ( vk.temporal.preparedThisFrame ) {
		return;
	}

	if ( r_taa && r_taa->modified ) {
		r_taa->modified = qfalse;
		reset_taa = qtrue;
	}
	if ( r_taa_feedbackStationary && r_taa_feedbackStationary->modified ) {
		r_taa_feedbackStationary->modified = qfalse;
		reset_taa = qtrue;
	}
	if ( r_taa_feedbackMotion && r_taa_feedbackMotion->modified ) {
		r_taa_feedbackMotion->modified = qfalse;
		reset_taa = qtrue;
	}
	if ( r_taa_sharpen && r_taa_sharpen->modified ) {
		r_taa_sharpen->modified = qfalse;
		reset_taa = qtrue;
	}
	if ( reset_taa ) {
		vk_reset_taa_history();
	}

	vk_temporal_begin_frame();
	vk_update_postfx_params( vk.cmd_index );
	vk.temporal.preparedThisFrame = qtrue;
}


static void vk_resize_geometry_buffer( void )
{
	int i;

	vk_end_render_pass();

	VK_CHECK( qvkEndCommandBuffer( vk.cmd->command_buffer ) );

	qvkResetCommandBuffer( vk.cmd->command_buffer, 0 );

	vk_wait_idle();

	vk_release_geometry_buffers();

	vk_create_geometry_buffers( vk.geometry_buffer_size_new );
	vk.geometry_buffer_size_new = 0;

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ )
		vk_update_uniform_descriptor( vk.tess[ i ].uniform_descriptor, vk.tess[ i ].vertex_buffer );

	ri.Printf( PRINT_DEVELOPER, "...geometry buffer resized to %iK\n", (int)( vk.geometry_buffer_size / 1024 ) );
}


void vk_end_frame( void )
{
#ifdef USE_UPLOAD_QUEUE
	VkSemaphore waits[2], signals[2];
	const VkPipelineStageFlags wait_dst_stage_mask[2] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
#else
	const VkPipelineStageFlags wait_dst_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
#endif
	VkSubmitInfo submit_info;

	if ( vk.frame_count == 0 )
		return;

	vk.frame_count = 0;

	if ( vk.geometry_buffer_size_new )
	{
		vk_resize_geometry_buffer();
		// issue: one frame may be lost during video recording
		// solution: re-record all commands again? (might be complicated though)
		return;
	}

	vk_prepare_frame_temporal_state();

	if ( vk.fboActive )
	{
		vk.cmd->last_pipeline = VK_NULL_HANDLE; // do not restore clobbered descriptors in vk_bloom()

		if ( PostFX_SSR_IsEnabled() )
		{
			vk_ssr_pass();
		}

		if ( r_bloom->integer )
		{
			vk_bloom();
		}

		/* SSAO runs in RB_FinishBloom before bloom; only run here if not yet done (e.g. no RC_FINISHBLOOM). */
		if ( r_ssao && r_ssao->integer && !backEnd.doneSSAO )
		{
			vk_ssao_pass();
		}

			vk_end_frame_record_capture_if_needed();

			if ( !ri.CL_IsMinimized() )
			{
				VkImageView post_fog_src;
				VkImageView luminance_src;

				vk_end_frame_prepare_post_process( &post_fog_src, &luminance_src );
				vk_end_frame_record_taa_pass( &post_fog_src, &luminance_src );
				vk_end_frame_record_luminance_pass( luminance_src );
				vk_end_frame_record_gamma_pass( post_fog_src );
			}
		}
	else
	{
		/* r_fbo 0 path: we started the main render pass in vk_start_frame().
		 * There is no post-process chain, so ensure the main render pass is
		 * terminated before ending the command buffer to avoid validation errors
		 * or undefined behavior on submission. */
		vk_end_render_pass();
	}

	VK_CHECK( qvkEndCommandBuffer( vk.cmd->command_buffer ) );

	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = NULL;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &vk.cmd->command_buffer;
	if ( !ri.CL_IsMinimized() ) {
#ifdef USE_UPLOAD_QUEUE
		if ( vk.image_uploaded != VK_NULL_HANDLE ) {
			waits[0] = vk.cmd->image_acquired;
			waits[1] = vk.image_uploaded;
			submit_info.waitSemaphoreCount = 2;
			submit_info.pWaitSemaphores = &waits[0];
			submit_info.pWaitDstStageMask = &wait_dst_stage_mask[0];
			signals[0] = vk.swapchain_rendering_finished[ vk.cmd->swapchain_image_index ];
			signals[1] = vk.cmd->rendering_finished2;
			submit_info.signalSemaphoreCount = 2;
			submit_info.pSignalSemaphores = &signals[0];

			vk.rendering_finished = vk.cmd->rendering_finished2;
			vk.image_uploaded = VK_NULL_HANDLE;
		} else if ( vk.rendering_finished != VK_NULL_HANDLE ) {
			waits[0] = vk.cmd->image_acquired;
			waits[1] = vk.rendering_finished;
			submit_info.waitSemaphoreCount = 2;
			submit_info.pWaitSemaphores = &waits[0];
			submit_info.pWaitDstStageMask = &wait_dst_stage_mask[0];
			signals[0] = vk.swapchain_rendering_finished[ vk.cmd->swapchain_image_index ];
			signals[1] = vk.cmd->rendering_finished2;
			submit_info.signalSemaphoreCount = 2;
			submit_info.pSignalSemaphores = &signals[0];

			vk.rendering_finished = vk.cmd->rendering_finished2;
		} else {
			submit_info.waitSemaphoreCount = 1;
			submit_info.pWaitSemaphores = &vk.cmd->image_acquired;
			submit_info.pWaitDstStageMask = &wait_dst_stage_mask[0];
			submit_info.signalSemaphoreCount = 1;
			submit_info.pSignalSemaphores = &vk.swapchain_rendering_finished[ vk.cmd->swapchain_image_index ];
		}
#else
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &vk.cmd->image_acquired;
		submit_info.pWaitDstStageMask = &wait_dst_stage_mask;
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &vk.swapchain_rendering_finished[ vk.cmd->swapchain_image_index ];
#endif
	} else {
		submit_info.waitSemaphoreCount = 0;
		submit_info.pWaitSemaphores = NULL;
		submit_info.pWaitDstStageMask = NULL;
		submit_info.signalSemaphoreCount = 0;
		submit_info.pSignalSemaphores = NULL;
	}

	{
		VkResult sub_res = qvkQueueSubmit( vk.queue, 1, &submit_info, vk.cmd->rendering_finished_fence );
		if ( sub_res != VK_SUCCESS ) {
			if ( sub_res == VK_ERROR_DEVICE_LOST ) {
				vk.device_lost = qtrue;
			}
			ri.Error( ERR_FATAL, "Vulkan: qvkQueueSubmit returned %s", vk_result_string( sub_res ) );
		}
	}
	vk.cmd->waitForFence = qtrue;
	vk_temporal_commit_frame_state();

	// presentation may take undefined time to complete, we can't measure it in a reliable way
	backEnd.pc.msec = ri.Milliseconds() - backEnd.pc.msec;

	vk.renderPassIndex = RENDER_PASS_MAIN;
}


void vk_present_frame( void )
{
	VkPresentInfoKHR present_info;
	VkResult res;
	VkExtent2D new_extent;
	qboolean new_extent_valid;

	if ( ri.CL_IsMinimized() || !vk.cmd->swapchain_image_acquired ) {
		return;
	}

	if ( gls.windowWidth == 0 || gls.windowHeight == 0 ) {
		return;
	}

	if ( vk.swapchain_extent_valid && ( vk.swapchain_extent.width == 0 || vk.swapchain_extent.height == 0 ) ) {
		return;
	}

	if ( !vk.cmd->waitForFence ) {
		// nothing has been submitted this frame due to geometry buffer overflow?
		return;
	}

	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.pNext = NULL;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &vk.swapchain_rendering_finished[ vk.cmd->swapchain_image_index ];
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &vk.swapchain;
	present_info.pImageIndices = &vk.cmd->swapchain_image_index;
	present_info.pResults = NULL;

	vk.cmd->swapchain_image_acquired = qfalse;

	res = qvkQueuePresentKHR( vk.queue, &present_info );
	switch ( res ) {
		case VK_SUCCESS:
			break;
		case VK_SUBOPTIMAL_KHR:
			new_extent_valid = vk_query_surface_extent( vk.physical_device, vk_surface, &new_extent );
			vk_log_swapchain_recreation( res, &vk.swapchain_extent, new_extent_valid ? &new_extent : NULL );
			if ( new_extent_valid && ( !vk.swapchain_extent_valid ||
					new_extent.width != vk.swapchain_extent.width ||
					new_extent.height != vk.swapchain_extent.height ) ) {
				vk_restart_swapchain( __func__, res );
				return;
			}
			break;
		case VK_ERROR_OUT_OF_DATE_KHR:
			new_extent_valid = vk_query_surface_extent( vk.physical_device, vk_surface, &new_extent );
			vk_log_swapchain_recreation( res, &vk.swapchain_extent, new_extent_valid ? &new_extent : NULL );
			vk_restart_swapchain( __func__, res );
			return;
		case VK_ERROR_DEVICE_LOST:
			// we can ignore that
			ri.Printf( PRINT_DEVELOPER, "vkQueuePresentKHR: device lost\n" );
			break;
		default:
			// or we don't
			ri.Error( ERR_FATAL, "vkQueuePresentKHR returned %s", vk_result_string( res ) );
	}

	// pickup next command buffer for rendering
	vk.cmd_index++;
	vk.cmd_index %= NUM_COMMAND_BUFFERS;
	vk.cmd = &vk.tess[ vk.cmd_index ];
}


static qboolean is_bgr( VkFormat format ) {
	switch ( format ) {
		case VK_FORMAT_B8G8R8A8_UNORM:
		case VK_FORMAT_B8G8R8A8_SNORM:
		case VK_FORMAT_B8G8R8A8_UINT:
		case VK_FORMAT_B8G8R8A8_SINT:
		case VK_FORMAT_B8G8R8A8_SRGB:
		case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
			return qtrue;
		default:
			return qfalse;
	}
}


void vk_read_pixels( byte *buffer, uint32_t width, uint32_t height )
{
	VkCommandBuffer command_buffer;
	VkDeviceMemory memory;
	VkMemoryRequirements memory_requirements;
	VkMemoryPropertyFlags memory_reqs;
	VkMemoryPropertyFlags memory_flags;
	VkMemoryAllocateInfo alloc_info;
	VkImageSubresource subresource;
	VkSubresourceLayout layout;
	VkImageCreateInfo desc;
	VkImage srcImage;
	VkImageLayout srcImageLayout;
	VkImage dstImage;
	byte *buffer_ptr;
	byte *data;
	uint32_t pixel_width;
	uint32_t i, n;
	qboolean invalidate_ptr;

	if ( vk.device_lost ) {
		return;
	}
	VK_CHECK( qvkWaitForFences( vk.device, 1, &vk.cmd->rendering_finished_fence, VK_FALSE, 1e12 ) );

	if ( vk.fboActive ) {
		if ( vk.capture.image ) {
			// dedicated capture buffer
			srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			srcImage = vk.capture.image;
		} else {
			srcImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			srcImage = vk.color_image;
		}
	} else {
		srcImageLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		srcImage = vk.swapchain_images[ vk.cmd->swapchain_image_index ];
	}

	Com_Memset( &desc, 0, sizeof( desc ) );

	// Create image in host visible memory to serve as a destination for framebuffer pixels.
	desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.imageType = VK_IMAGE_TYPE_2D;
	desc.format = vk.capture_format;
	desc.extent.width = width;
	desc.extent.height = height;
	desc.extent.depth = 1;
	desc.mipLevels = 1;
	desc.arrayLayers = 1;
	desc.samples = VK_SAMPLE_COUNT_1_BIT;
	desc.tiling = VK_IMAGE_TILING_LINEAR;
	desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = NULL;
	desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VK_CHECK( qvkCreateImage( vk.device, &desc, NULL, &dstImage ) );

	qvkGetImageMemoryRequirements( vk.device, dstImage, &memory_requirements );

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = memory_requirements.size;

	// host_cached bit is desirable for fast reads
	memory_reqs = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
	alloc_info.memoryTypeIndex = vk_find_memory_type2( vk.physical_device, memory_requirements.memoryTypeBits, memory_reqs, &memory_flags );
	if ( alloc_info.memoryTypeIndex == ~0U ) {
		// try less explicit flags, without host_coherent
		memory_reqs = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
		alloc_info.memoryTypeIndex = vk_find_memory_type2( vk.physical_device, memory_requirements.memoryTypeBits, memory_reqs, &memory_flags );
		if ( alloc_info.memoryTypeIndex == ~0U ) {
			// slowest case
			memory_reqs = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			alloc_info.memoryTypeIndex = vk_find_memory_type2( vk.physical_device, memory_requirements.memoryTypeBits, memory_reqs, &memory_flags );
			if ( alloc_info.memoryTypeIndex == ~0U ) {
				ri.Error( ERR_FATAL, "%s(): failed to find matching memory type for image capture", __func__ );
			}
		}
	}

	if ( memory_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT ) {
		invalidate_ptr = qfalse;
	} else {
		 // according to specification - must be performed if host_coherent is not set
		invalidate_ptr = qtrue;
	}

	VK_CHECK(qvkAllocateMemory(vk.device, &alloc_info, NULL, &memory));
	VK_CHECK(qvkBindImageMemory(vk.device, dstImage, memory, 0));

	command_buffer = vk_begin_command_buffer();

	if ( srcImageLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL ) {
		record_image_layout_transition( command_buffer, srcImage,
			VK_IMAGE_ASPECT_COLOR_BIT,
			srcImageLayout,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			0, 0);
	}

	record_image_layout_transition( command_buffer, dstImage,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );

	// vk_end_command_buffer( command_buffer, __func__  );

	// command_buffer = vk_begin_command_buffer();

	if ( vk.blitEnabled ) {
		VkImageBlit region;

		region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.srcSubresource.mipLevel = 0;
		region.srcSubresource.baseArrayLayer = 0;
		region.srcSubresource.layerCount = 1;
		region.srcOffsets[0].x = 0;
		region.srcOffsets[0].y = 0;
		region.srcOffsets[0].z = 0;
		region.srcOffsets[1].x = width;
		region.srcOffsets[1].y = height;
		region.srcOffsets[1].z = 1;
		region.dstSubresource = region.srcSubresource;
		region.dstOffsets[0] = region.srcOffsets[0];
		region.dstOffsets[1] = region.srcOffsets[1];

		qvkCmdBlitImage( command_buffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_NEAREST );

	} else {
		VkImageCopy region;

		region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.srcSubresource.mipLevel = 0;
		region.srcSubresource.baseArrayLayer = 0;
		region.srcSubresource.layerCount = 1;
		region.srcOffset.x = 0;
		region.srcOffset.y = 0;
		region.srcOffset.z = 0;
		region.dstSubresource = region.srcSubresource;
		region.dstOffset = region.srcOffset;
		region.extent.width = width;
		region.extent.height = height;
		region.extent.depth = 1;

		qvkCmdCopyImage( command_buffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );
	}

	vk_end_command_buffer( command_buffer, __func__ );

	// Copy data from destination image to memory buffer.
	subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresource.mipLevel = 0;
	subresource.arrayLayer = 0;

	qvkGetImageSubresourceLayout( vk.device, dstImage, &subresource, &layout );

	VK_CHECK( qvkMapMemory( vk.device, memory, 0, VK_WHOLE_SIZE, 0, (void**)&data ) );

	if ( invalidate_ptr )
	{
		VkMappedMemoryRange range;
		range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range.pNext = NULL;
		range.memory = memory;
		range.size = VK_WHOLE_SIZE;
		range.offset = 0;
		qvkInvalidateMappedMemoryRanges( vk.device, 1, &range );
	}

	data += layout.offset;

	switch ( vk.capture_format ) {
		case VK_FORMAT_B4G4R4A4_UNORM_PACK16: pixel_width = 2; break;
		case VK_FORMAT_R16G16B16A16_UNORM: pixel_width = 8; break;
		default: pixel_width = 4; break;
	}

	buffer_ptr = buffer + width * (height - 1) * 3;
	for ( i = 0; i < height; i++ ) {
		switch ( pixel_width ) {
			case 2: {
				uint16_t *src = (uint16_t*)data;
				for ( n = 0; n < width; n++ ) {
					buffer_ptr[n*3+0] = ((src[n]>>12)&0xF)<<4;
					buffer_ptr[n*3+1] = ((src[n]>>8)&0xF)<<4;
					buffer_ptr[n*3+2] = ((src[n]>>4)&0xF)<<4;
				}
			} break;

			case 4: {
				for ( n = 0; n < width; n++ ) {
					Com_Memcpy( &buffer_ptr[n*3], &data[n*4], 3 );
					//buffer_ptr[n*3+0] = data[n*4+0];
					//buffer_ptr[n*3+1] = data[n*4+1];
					//buffer_ptr[n*3+2] = data[n*4+2];
				}
			} break;

			case 8: {
				const uint16_t *src = (uint16_t*)data;
				for ( n = 0; n < width; n++ ) {
					buffer_ptr[n*3+0] = src[n*4+0]>>8;
					buffer_ptr[n*3+1] = src[n*4+1]>>8;
					buffer_ptr[n*3+2] = src[n*4+2]>>8;
				}
			} break;
		}
		buffer_ptr -= width * 3;
		data += layout.rowPitch;
	}

	if ( is_bgr( vk.capture_format ) ) {
		buffer_ptr = buffer;
		for ( i = 0; i < width * height; i++ ) {
			byte tmp = buffer_ptr[0];
			buffer_ptr[0] = buffer_ptr[2];
			buffer_ptr[2] = tmp;
			buffer_ptr += 3;
		}
	}

	qvkDestroyImage( vk.device, dstImage, NULL );
	qvkFreeMemory( vk.device, memory, NULL );

	// restore previous layout
	if ( srcImageLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL ) {
		command_buffer = vk_begin_command_buffer();

		record_image_layout_transition( command_buffer, srcImage,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			srcImageLayout, 0, 0 );

		vk_end_command_buffer( command_buffer, "restore layout" );
	}
}

void vk_destroy_volumetric_params_buffer( void )
{
	if ( vk.volumetric_params_ptr ) {
		qvkUnmapMemory( vk.device, vk.volumetric_params_memory );
		vk.volumetric_params_ptr = NULL;
	}

	if ( vk.volumetric_params_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.volumetric_params_buffer, NULL );
		vk.volumetric_params_buffer = VK_NULL_HANDLE;
		vk.volumetric_params_buffer_size = 0;
	}

	if ( vk.volumetric_params_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.volumetric_params_memory, NULL );
		vk.volumetric_params_memory = VK_NULL_HANDLE;
	}
	vk_reset_volumetric_history();
	vk_temporal_request_sticky_reset( VK_TEMPORAL_RESET_MISSING_PREV_DATA );
}

void vk_create_volumetric_params_buffer( void )
{
	if ( vk.volumetric_params_buffer != VK_NULL_HANDLE ) {
		return;
	}

	VkBufferCreateInfo desc;
	VkMemoryRequirements mem_req;
	VkMemoryAllocateInfo alloc_info;

	desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.size = sizeof( volumetric_params_t );
	desc.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = NULL;

	VK_CHECK( qvkCreateBuffer( vk.device, &desc, NULL, &vk.volumetric_params_buffer ) );

	qvkGetBufferMemoryRequirements( vk.device, vk.volumetric_params_buffer, &mem_req );

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.volumetric_params_memory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.volumetric_params_buffer, vk.volumetric_params_memory, 0 ) );

	vk.volumetric_params_buffer_size = mem_req.size;

	VK_CHECK( qvkMapMemory( vk.device, vk.volumetric_params_memory, 0, vk.volumetric_params_buffer_size, 0, &vk.volumetric_params_ptr ) );
	vk_reset_volumetric_history();
	vk_temporal_request_sticky_reset( VK_TEMPORAL_RESET_MISSING_PREV_DATA );
}

void vk_destroy_postfx_params_buffers( void )
{
	uint32_t i;

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		if ( vk.postfx_params_memory[i] != VK_NULL_HANDLE ) {
			if ( vk.postfx_params_ptr[i] ) {
				qvkUnmapMemory( vk.device, vk.postfx_params_memory[i] );
				vk.postfx_params_ptr[i] = NULL;
			}
			qvkFreeMemory( vk.device, vk.postfx_params_memory[i], NULL );
			vk.postfx_params_memory[i] = VK_NULL_HANDLE;
		}
		if ( vk.postfx_params_buffer[i] != VK_NULL_HANDLE ) {
			qvkDestroyBuffer( vk.device, vk.postfx_params_buffer[i], NULL );
			vk.postfx_params_buffer[i] = VK_NULL_HANDLE;
		}
		vk.postfx_params_descriptor[i] = VK_NULL_HANDLE;
	}
}

void vk_create_postfx_params_buffers( void )
{
	uint32_t i;

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		VkBufferCreateInfo desc;
		VkMemoryRequirements mem_req;
		VkMemoryAllocateInfo alloc_info;

		if ( vk.postfx_params_buffer[i] != VK_NULL_HANDLE ) {
			continue;
		}

		Com_Memset( &desc, 0, sizeof( desc ) );
		desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		desc.size = sizeof( VkPostFXParams );
		desc.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VK_CHECK( qvkCreateBuffer( vk.device, &desc, NULL, &vk.postfx_params_buffer[i] ) );
		qvkGetBufferMemoryRequirements( vk.device, vk.postfx_params_buffer[i], &mem_req );

		Com_Memset( &alloc_info, 0, sizeof( alloc_info ) );
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize = mem_req.size;
		alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device,
			mem_req.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

		VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.postfx_params_memory[i] ) );
		VK_CHECK( qvkBindBufferMemory( vk.device, vk.postfx_params_buffer[i], vk.postfx_params_memory[i], 0 ) );
		VK_CHECK( qvkMapMemory( vk.device, vk.postfx_params_memory[i], 0, mem_req.size, 0, &vk.postfx_params_ptr[i] ) );
		Com_Memset( vk.postfx_params_ptr[i], 0, sizeof( VkPostFXParams ) );
	}
}

/* Cubemap prefilter / vk_generate_cubemaps: vk_cubemap_prefilter.c */
