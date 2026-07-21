/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Device/swapchain/renderer bootstrap after logical device creation.
Extracted from vk.c for incremental modularization.
===========================================================================
*/

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
#include "vk_pass_registry.h"
#include "vk_forward_plus.h"
#include "vk_deferred_gbuffer.h"
#include "vk_visibility_buffer.h"
#include "vk_vrcs.h"
#include "vk_view_state.h"
#include "vk_sim_render_profile.h"
#include "vk_fp64_points.h"
#include "vk_volumetric_fog_color.h"
#include "vk_volumetric_pass.h"
#include "vk_volumetric_internal.h"
#include "vk_util.h"
#include "vk_lens_flare_push.h"
#include "vk_validation.h"
#include "vk_staging.h"
#include "vk_descriptors.h"
#include "vk_sync.h"
#include "vk_cmd.h"
#include "vk_device.h"
#include "vk_swapchain.h"
#include "vk_post_process_push.h"
#include "vk_shader_modules.h"
#include "vk_pipelines_persistent.h"
#include "vk_framebuffers.h"
#include "vk_attachments.h"
#include "vk_resource_destroy.h"
#include "vk_descriptor_sets.h"
#include "vk_rtx.h"
#include "vk_grtx.h"
#include "vk_pathtrace.h"
#include "vk_hybrid1.h"
#include "vk_raygun.h"
#include "vk_surfel_gi.h"
#include "vk_rcgi.h"
#include "vk_ambient_visibility.h"
#include "vk_raster_gi.h"
#include "vk_gpu_particles.h"
#include "vk_deferred_decals.h"
#include "vk_distortion.h"
#include "vk_selective_sun_shadow.h"
#include "vk_selective_reflection.h"
#include "vk_pipeline_cache_disk.h"
#include "vk_pipeline_helpers.h"
#include "vk_raster_samples.h"
#include "vk_fluidsim.h"
#include "vk_terrain.h"
#include <math.h>
#include <stddef.h>

#ifndef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT 1000484000
#endif

#if defined( _DEBUG )
#define USE_VK_VALIDATION
#if defined( _WIN32 )
#include <windows.h>
#endif
#endif

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

	if ( r_simRenderProfileAutoApply && r_simRenderProfileAutoApply->integer &&
		r_simRenderProfile && r_simRenderProfile->integer > 0 ) {
		VK_ApplySimRenderProfile( r_simRenderProfile->integer );
	}

	vk.cmd = vk.tess + 0;
	vk_reset_motion_history();
	vk.adaptedExposure = 1.0f;
	VectorClear( vk.prevViewOrigin );
	VectorSet( vk.prevViewForward, 0.0f, 0.0f, -1.0f );  /* sentinel until first frame */
	vk.prevClientState = CA_UNINITIALIZED;
	Com_Memset( &vk.temporal, 0, sizeof( vk.temporal ) );
	Com_Memset( &vk.forward_plus, 0, sizeof( vk.forward_plus ) );
#ifdef USE_VK_PBR
	vk.set_layout_forward_plus = VK_NULL_HANDLE;
#endif
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
	vk.fxaaActive = qfalse;
	vk.lensFlareActive = qfalse;
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
	vk.smaaActive = (vk.fboActive && r_ext_smaa->integer && !r_ext_fxaa->integer) ? qtrue : qfalse;
	vk.fxaaActive = (vk.fboActive && r_ext_fxaa->integer && !r_ext_smaa->integer) ? qtrue : qfalse;
	if ( r_ext_fxaa->integer && r_ext_smaa->integer ) {
		ri.Printf( PRINT_WARNING, "Warning: r_ext_fxaa and r_ext_smaa both enabled; using SMAA only.\n" );
		vk.fxaaActive = qfalse;
	}

	// multisampling
	vk_raster_samples_configure( &props, vk.msaaActive );
	if ( vk.smaaActive ) {
			int p = r_smaa_preset && r_smaa_preset->integer >= 1 && r_smaa_preset->integer <= 4 ? r_smaa_preset->integer : 0;
			const char *preset_name = ( p == 1 ) ? "Low" : ( p == 2 ) ? "Medium" : ( p == 3 ) ? "High" : ( p == 4 ) ? "Ultra" : "Custom";
			float thresh = p ? ( p == 1 ? 0.15f : p == 2 ? 0.1f : p == 3 ? 0.08f : 0.05f ) : ( r_smaa_threshold ? r_smaa_threshold->value : 0.1f );
			int search = p ? ( p == 1 ? 8 : p == 2 ? 16 : p == 3 ? 24 : 32 ) : ( r_smaa_max_search_steps && r_smaa_max_search_steps->integer ? r_smaa_max_search_steps->integer : 16 );
			ri.Printf( PRINT_ALL, "...SMAA enabled (preset=%s, threshold %.2f, search %d)\n", preset_name, thresh, search );
	}
	if ( vk.fxaaActive ) {
		ri.Printf( PRINT_ALL, "...FXAA enabled (subpix %.2f, edgeThreshold %.3f)\n",
			r_fxaa_subpix ? r_fxaa_subpix->value : 0.75f,
			r_fxaa_edgeThreshold ? r_fxaa_edgeThreshold->value : 0.166f );
	}
	vk.lensFlareActive = ( vk.fboActive && r_lensFlare && r_lensFlare->integer ) ? qtrue : qfalse;
	if ( vk.lensFlareActive ) {
		ri.Printf( PRINT_ALL, "...Lens flare enabled (strength %.2f)\n",
			r_lensFlareStrength ? r_lensFlareStrength->value : 1.0f );
	}
	if ( vk.msaaActive && vk.smaaActive ) {
		ri.Printf( PRINT_ALL, "...MSAA (geometry) + SMAA (alpha/transparency) for best edge quality\n" );
	}
	if ( vk.msaaActive && vk.fxaaActive ) {
		ri.Printf( PRINT_ALL, "...MSAA (geometry) + FXAA (post-process) for simulation-style edge quality\n" );
	}

	vk.screenMapSamples = MIN( vk_get_main_rasterization_max_samples(), VK_SAMPLE_COUNT_4_BIT );

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
	if ( vk.pbrActive && r_cubeMapping->integer ) {
		ri.Printf( PRINT_WARNING,
			"PBR IBL: runtime cubemap convolution is disabled on Vulkan; using fallback IBL.\n" );
	}
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
#ifdef USE_VULKAN_RTX
				if ( vk.rtxAvailable ) {
					ri.Printf( PRINT_ALL, "[VK]   Ray Tracing         : enabled (KHR AS + RT pipeline + buffer device address)\n" );
				} else {
					ri.Printf( PRINT_ALL, "[VK]   Ray Tracing         : build on, GPU/extensions unavailable\n" );
				}
#else
				ri.Printf( PRINT_ALL, "[VK]   Ray Tracing         : not built (enable USE_VULKAN_RTX)\n" );
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
			VkDescriptorPoolSize pool_size[6];
		VkDescriptorPoolCreateInfo desc;
		uint32_t j, maxSets;

		pool_size[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			pool_size[0].descriptorCount = MAX_DRAWIMAGES + NUM_COMMAND_BUFFERS + NUM_COMMAND_BUFFERS + NUM_COMMAND_BUFFERS + 3 + 6 + VK_NUM_BLOOM_PASSES * 2 + 32 + NUM_COMMAND_BUFFERS + 1; // + TAA reactive[N] + reactive stamp reveal
#ifdef USE_VK_PBR
        if ( vk.pbrActive )
            pool_size[0].descriptorCount += 2 + ( MAX_DRAWIMAGES * 9 ) + 24; // brdf-lut + irradiance | PBR maps | blend layer arrays (3×8)
#endif

		pool_size[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		pool_size[1].descriptorCount = NUM_COMMAND_BUFFERS * 2; // main + camera

		//pool_size[2].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		//pool_size[2].descriptorCount = NUM_COMMAND_BUFFERS;

		pool_size[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
		pool_size[2].descriptorCount = 1 + NUM_COMMAND_BUFFERS * 3; // flare storage + IQM skin/morph + glTF topo SSBO

		pool_size[3].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			pool_size[3].descriptorCount = 22 + NUM_COMMAND_BUFFERS + 4;	/* + Forward+ reactive mask storage */

		pool_size[4].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		pool_size[4].descriptorCount = 8 + NUM_COMMAND_BUFFERS;

		pool_size[5].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		pool_size[5].descriptorCount = 12; /* forward+ + CBT draw commands SSBO */

		for ( j = 0, maxSets = 0; j < ARRAY_LEN( pool_size ); j++ ) {
			maxSets += pool_size[j].descriptorCount;
		}

		desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.maxSets = maxSets;
		desc.poolSizeCount = ARRAY_LEN( pool_size );
		desc.pPoolSizes = pool_size;

		vk_forward_plus_on_descriptor_pool_destroyed();
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

	vk_forward_plus_create_set_layout();

#ifdef USE_VK_PBR
	/* Material-blend layer arrays: set 19, bindings 0..2 with descriptorCount 8 each. */
	{
		VkDescriptorSetLayoutBinding blend_bindings[3];
		VkDescriptorSetLayoutCreateInfo blend_layout_desc;
		int bi;
		for ( bi = 0; bi < 3; bi++ ) {
			Com_Memset( &blend_bindings[bi], 0, sizeof( blend_bindings[bi] ) );
			blend_bindings[bi].binding = (uint32_t)bi;
			blend_bindings[bi].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			blend_bindings[bi].descriptorCount = 8;
			blend_bindings[bi].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		}
		Com_Memset( &blend_layout_desc, 0, sizeof( blend_layout_desc ) );
		blend_layout_desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		blend_layout_desc.bindingCount = 3;
		blend_layout_desc.pBindings = blend_bindings;
		VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &blend_layout_desc, NULL, &vk.set_layout_blend_layers ) );
	}
#endif

		{
			VkDescriptorSetLayoutBinding compute_bindings[19];
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

			compute_bindings[17].binding = 17;
			compute_bindings[17].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			compute_bindings[17].descriptorCount = 1;
			compute_bindings[17].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			compute_bindings[17].pImmutableSamplers = NULL;

			compute_bindings[18].binding = 18;
			compute_bindings[18].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			compute_bindings[18].descriptorCount = 1;
			compute_bindings[18].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			compute_bindings[18].pImmutableSamplers = NULL;

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
				VkDescriptorSetLayoutBinding composite_bindings[12];
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

			composite_bindings[9].binding = 9;
			composite_bindings[9].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			composite_bindings[9].descriptorCount = 1;
			composite_bindings[9].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			composite_bindings[9].pImmutableSamplers = NULL;

			composite_bindings[10].binding = 10;
			composite_bindings[10].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			composite_bindings[10].descriptorCount = 1;
			composite_bindings[10].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			composite_bindings[10].pImmutableSamplers = NULL;

			composite_bindings[11].binding = 11;
			composite_bindings[11].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			composite_bindings[11].descriptorCount = 1;
			composite_bindings[11].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			composite_bindings[11].pImmutableSamplers = NULL;

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

		push_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		push_range.offset = 0;
		/* vkMvpPushConstants_t: 2x mat4 + 8 float pad (256 B); must be >= minPushConstantSize (commonly 256). */
		push_range.size = sizeof( float ) * 40;

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
		set_layouts[18] = vk.set_layout_forward_plus;
		set_layouts[19] = vk.set_layout_blend_layers;
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

		set_layouts[0] = vk.set_layout_sampler;
		set_layouts[1] = vk.set_layout_sampler;
		set_layouts[2] = vk.set_layout_postfx_uniform;
		set_layouts[3] = vk.set_layout_sampler;
		set_layouts[4] = vk.set_layout_sampler;
		set_layouts[5] = vk.set_layout_sampler;
		set_layouts[6] = vk.set_layout_sampler;
		desc.setLayoutCount = 7;
		desc.pSetLayouts = set_layouts;
		VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.pipeline_layout_taa ) );
		SET_OBJECT_NAME( vk.pipeline_layout_taa, "pipeline layout - taa", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );

		/* Blend pipeline: 4 sampler sets for texture0..texture3 (blend.frag). Must not reuse postfx_uniform. */
		set_layouts[0] = vk.set_layout_sampler;
		set_layouts[1] = vk.set_layout_sampler;
		set_layouts[2] = vk.set_layout_sampler;
		set_layouts[3] = vk.set_layout_sampler;
		desc.setLayoutCount = VK_NUM_BLOOM_PASSES;
		push_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		push_range.offset = 0;
		push_range.size = sizeof( VkLensFlarePushConstants );
		desc.pushConstantRangeCount = 1;
		desc.pPushConstantRanges = &push_range;

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
			/* 32 bytes: SMAA uses 16; spatial adaptive SS push uses 24. */
			smaa_push_range.size = 32;

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
			/* OIT resolve: set 0 = opaque, set 1 = accum, set 2 = revealage,
			 * set 3 = moments, set 4 = b0 (placeholders when WBOIT).
			 * Push: debugMode + oitMode. */
			set_layouts[0] = vk.set_layout_sampler;
			set_layouts[1] = vk.set_layout_sampler;
			set_layouts[2] = vk.set_layout_sampler;
			set_layouts[3] = vk.set_layout_sampler;
			set_layouts[4] = vk.set_layout_sampler;
			desc.setLayoutCount = 5;
			desc.pSetLayouts = set_layouts;
			push_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			push_range.offset = 0;
			push_range.size = 16; /* ivec4 */
			desc.pushConstantRangeCount = 1;
			desc.pPushConstantRanges = &push_range;
			VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.pipeline_layout_oit_resolve ) );
			SET_OBJECT_NAME( vk.pipeline_layout_oit_resolve, "pipeline layout - oit_resolve", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );

			/* OIT accum: set 0 = tex0, set 1 = opaque depth, set 2 = Forward+ lights,
			 * push = mvp + prevMvp + model (192 bytes). Requires Forward+ set layout. */
			set_layouts[0] = vk.set_layout_sampler;
			set_layouts[1] = vk.set_layout_sampler;
			desc.setLayoutCount = 2;
			if ( vk.set_layout_forward_plus != VK_NULL_HANDLE ) {
				set_layouts[2] = vk.set_layout_forward_plus;
				desc.setLayoutCount = 3;
			} else {
				ri.Printf( PRINT_WARNING, "[VK][OIT] Forward+ set layout missing; OIT accum unbound set2 — enable PBR/Forward+\n" );
			}
			desc.pSetLayouts = set_layouts;
			push_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
			push_range.offset = 0;
			push_range.size = 192; /* 3 * mat4 */
			desc.pushConstantRangeCount = 1;
			desc.pPushConstantRanges = &push_range;
			VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.pipeline_layout_oit_accum ) );
			SET_OBJECT_NAME( vk.pipeline_layout_oit_accum, "pipeline layout - oit_accum", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );

			if ( r_oit->integer == 2 ) {
				/* MBOIT pass 1: set 0 = tex0, set 1 = opaque depth */
				set_layouts[0] = vk.set_layout_sampler;
				set_layouts[1] = vk.set_layout_sampler;
				desc.setLayoutCount = 2;
				desc.pSetLayouts = set_layouts;
				push_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
				push_range.offset = 0;
				push_range.size = 192;
				desc.pushConstantRangeCount = 1;
				desc.pPushConstantRanges = &push_range;
				VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.pipeline_layout_oit_moments ) );
				SET_OBJECT_NAME( vk.pipeline_layout_oit_moments, "pipeline layout - oit_moments", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );

				/* MBOIT pass 2: set 0 = tex0, set 1 = depth, set 2 = moments, set 3 = b0,
				 * set 4 = Forward+ lights (when available). Push = mvp + prevMvp + model. */
				set_layouts[0] = vk.set_layout_sampler;
				set_layouts[1] = vk.set_layout_sampler;
				set_layouts[2] = vk.set_layout_sampler;
				set_layouts[3] = vk.set_layout_sampler;
				desc.setLayoutCount = 4;
				if ( vk.set_layout_forward_plus != VK_NULL_HANDLE ) {
					set_layouts[4] = vk.set_layout_forward_plus;
					desc.setLayoutCount = 5;
				} else {
					ri.Printf( PRINT_WARNING, "[VK][OIT] Forward+ set layout missing; MBOIT accum unbound set4 — enable PBR/Forward+\n" );
				}
				desc.pSetLayouts = set_layouts;
				push_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
				push_range.offset = 0;
				push_range.size = 192;
				desc.pushConstantRangeCount = 1;
				desc.pPushConstantRanges = &push_range;
				VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.pipeline_layout_oit_accum_mboit ) );
				SET_OBJECT_NAME( vk.pipeline_layout_oit_accum_mboit, "pipeline layout - oit_accum_mboit", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );
			}
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

	vk_forward_plus_init();
	vk_deferred_gbuffer_init();
	vk_visibility_buffer_init();
	vk_vrcs_init();

	vk_pipeline_cache_create( &props );

	vk.renderPassIndex = RENDER_PASS_MAIN; // default render pass

	// swapchain
	vk.initSwapchainLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	//vk.initSwapchainLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	vk_create_swapchain( vk.physical_device, vk.device, vk_surface, vk.present_format, &vk.swapchain, qtrue );

	// color/depth attachments
	vk_create_attachments();

	// renderpasses
	vk_create_render_passes();

	VK_FP64_PointsInit();
	VK_FP64_PointsCreatePipelines();

	// framebuffers for each swapchain image
	vk_create_framebuffers();

	vk_rtx_init();
	vk_grtx_init();
	vk_pathtrace_init();
	vk_hybrid1_init();
	vk_raygun_init();
	vk_surfel_gi_init();
	vk_rcgi_init();
	vk_ambient_visibility_init();
	vk_raster_gi_init();
	vk_gpu_particles_init();
	vk_deferred_decals_init();
	vk_distortion_init();
	vk_shs_init();
	vk_shr_init();
	vk_spine_registry_init();

#ifdef VK_CUBEMAP
	vk_create_cubemap_prefilter();
#endif

	// preallocate staging buffer
	if ( vk.defaults.staging_size == STAGING_BUFFER_SIZE_HI ) {
		vk_alloc_staging_buffer( vk.defaults.staging_size );
	}

	vk.active = qtrue;
}
