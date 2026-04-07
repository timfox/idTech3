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

#define VK_VOLUMETRIC_QUERY_SLOTS 16
#define VK_VOLUMETRIC_QUERY_COUNT (VK_VOLUMETRIC_QUERY_SLOTS * NUM_COMMAND_BUFFERS)

static void vk_fluid_simulation_pass( float delta_time );
static void vk_resolve_volumetric_depth_msaa( void );
static qboolean vk_begin_local_spot_shadow_render_pass( void );
static void vk_end_local_spot_shadow_render_pass( void );
static qboolean vk_begin_local_point_shadow_render_pass( int faceLayer );
static void vk_end_local_point_shadow_render_pass( void );
static const dlight_t *vk_get_volumetric_local_light( int local_index );
static qboolean vk_build_local_spot_shadow_view( const dlight_t *dl, int viewportX, int viewportY, int viewportSize, viewParms_t *shadowParms, float *outViewProj );
static qboolean vk_build_local_point_shadow_view( const dlight_t *dl, int face, int viewportSize, viewParms_t *shadowParms, float *outViewProj );
static qboolean vk_render_local_volumetric_shadow_view( const viewParms_t *shadowParms, qboolean pointShadow, int pointFaceLayer );
enum {
	VK_VOLUMETRY_COUNTER_NAN_OR_INF = 0,
	VK_VOLUMETRY_COUNTER_EXTINCTION_CLAMP = 1,
	VK_VOLUMETRY_COUNTER_VELOCITY_CLAMP = 2,
	VK_VOLUMETRY_COUNTER_DENSITY_CLAMP = 3,
	VK_VOLUMETRY_COUNTER_PRESSURE_SANITIZE = 4,
	VK_VOLUMETRY_COUNTER_TEMPORAL_REJECT = 5
};
enum {
	VK_VOLUMETRY_QUERY_FOG_START = 0,
	VK_VOLUMETRY_QUERY_AFTER_FLUID_SIM = 1,
	VK_VOLUMETRY_QUERY_AFTER_CLEAR = 2,
	VK_VOLUMETRY_QUERY_AFTER_GLOBAL_DENSITY = 3,
	VK_VOLUMETRY_QUERY_AFTER_VOLUME_DENSITY = 4,
	VK_VOLUMETRY_QUERY_AFTER_FLUID_DENSITY = 5,
	VK_VOLUMETRY_QUERY_AFTER_SUN = 6,
	VK_VOLUMETRY_QUERY_AFTER_LOCAL = 7,
	VK_VOLUMETRY_QUERY_AFTER_CLAMP0 = 8,
	VK_VOLUMETRY_QUERY_AFTER_CLAMP1 = 9,
	VK_VOLUMETRY_QUERY_AFTER_TEMPORAL = 10,
	VK_VOLUMETRY_QUERY_AFTER_COMPOSITE = 11,
	VK_VOLUMETRY_QUERY_FOG_END = 12,
	VK_VOLUMETRY_QUERY_USED = 13
};
static void vk_update_volumetric_perf_queries( void );
static void vk_update_fluid_auto_scale( void );
static void vk_write_volumetric_timestamp( uint32_t query_index, VkPipelineStageFlagBits stage );
static const float vk_local_shadow_flip_matrix[16] = {
	0, 0, -1, 0,
	-1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 0, 1
};

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

#ifdef USE_VBO
static void vk_release_vbo( void )
{
	if ( vk.vbo.vertex_buffer )
		qvkDestroyBuffer( vk.device, vk.vbo.vertex_buffer, NULL );
	vk.vbo.vertex_buffer = VK_NULL_HANDLE;

	if ( vk.vbo.buffer_memory )
		qvkFreeMemory( vk.device, vk.vbo.buffer_memory, NULL );
	vk.vbo.buffer_memory = VK_NULL_HANDLE;
}


qboolean vk_alloc_vbo( const byte *vbo_data, int vbo_size )
{
	VkMemoryRequirements vb_mem_reqs;
	VkMemoryAllocateInfo alloc_info;
	VkBufferCreateInfo desc;
	VkDeviceSize vertex_buffer_offset;
	VkDeviceSize allocationSize;
	uint32_t memory_type_bits;
	VkCommandBuffer command_buffer;
	VkBufferCopy copyRegion[1];
	VkDeviceSize uploadDone;

	vk_release_vbo();

	desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = NULL;

	// device-local buffer
	desc.size = vbo_size;
	desc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	VK_CHECK( qvkCreateBuffer( vk.device, &desc, NULL, &vk.vbo.vertex_buffer ) );

	// memory requirements
	qvkGetBufferMemoryRequirements( vk.device, vk.vbo.vertex_buffer, &vb_mem_reqs );
	vertex_buffer_offset = 0;
	allocationSize = vertex_buffer_offset + vb_mem_reqs.size;
	memory_type_bits = vb_mem_reqs.memoryTypeBits;

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = allocationSize;
	alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, memory_type_bits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.vbo.buffer_memory ) );
	qvkBindBufferMemory( vk.device, vk.vbo.vertex_buffer, vk.vbo.buffer_memory, vertex_buffer_offset );

	// staging buffers

#ifdef USE_UPLOAD_QUEUE
	vk_flush_staging_buffer( qfalse );
#endif
	// utilize existing staging buffer
	uploadDone = 0;
	while ( uploadDone < (VkDeviceSize) vbo_size ) {
		VkDeviceSize uploadSize = vk.staging_buffer.size;
		if ( uploadDone + uploadSize > (VkDeviceSize) vbo_size ) {
			uploadSize = (VkDeviceSize) vbo_size - uploadDone;
		}
		memcpy(vk.staging_buffer.ptr + 0, vbo_data + uploadDone, uploadSize);
		command_buffer = vk_begin_command_buffer();
		copyRegion[0].srcOffset = 0;
		copyRegion[0].dstOffset = uploadDone;
		copyRegion[0].size = uploadSize;
		qvkCmdCopyBuffer( command_buffer, vk.staging_buffer.handle, vk.vbo.vertex_buffer, 1, &copyRegion[0] );
		vk_end_command_buffer( command_buffer, __func__ );
		uploadDone += uploadSize;
	}

	SET_OBJECT_NAME( vk.vbo.vertex_buffer, "static VBO", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	SET_OBJECT_NAME( vk.vbo.buffer_memory, "static VBO memory", VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT );

	return qtrue;
}
#endif

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

	vk_destroy_pipelines( qtrue ); // reset counter

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

VkSampleCountFlagBits vk_get_main_rasterization_samples( void )
{
	return (VkSampleCountFlagBits)vkSamples;
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

	// vk_destroy_samplers();

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

	// Reset geometry buffers offsets for all command buffers (avoid stale offsets on next frame)
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		vk.tess[i].uniform_read_offset = 0;
		vk.tess[i].vertex_buffer_offset = 0;
		Com_Memset( vk.tess[i].buf_offset, 0, sizeof( vk.tess[i].buf_offset ) );
		Com_Memset( vk.tess[i].vbo_offset, 0, sizeof( vk.tess[i].vbo_offset ) );
	}

	Com_Memset( &vk.stats, 0, sizeof( vk.stats ) );
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

void vk_clear_color( const vec4_t color ) {

	VkClearAttachment attachment;
	VkClearRect clear_rect;

	if ( !vk.active )
		return;
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE || !vk.inRenderPass )
		return;

	attachment.colorAttachment = 0;
	attachment.clearValue.color.float32[0] = color[0];
	attachment.clearValue.color.float32[1] = color[1];
	attachment.clearValue.color.float32[2] = color[2];
	attachment.clearValue.color.float32[3] = color[3];
	attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	vk_get_scissor_rect( &clear_rect.rect );
	clear_rect.baseArrayLayer = 0;
	clear_rect.layerCount = 1;

	qvkCmdClearAttachments( vk.cmd->command_buffer, 1, &attachment, 1, &clear_rect );
}

void vk_set_color_write_mask( qboolean r, qboolean g, qboolean b, qboolean a )
{
	VkColorComponentFlags mask;

	if ( !vk.active || !vk.colorWriteMaskDynamic || !qvkCmdSetColorWriteMaskEXT )
		return;

	mask = 0;
	if ( r ) mask |= VK_COLOR_COMPONENT_R_BIT;
	if ( g ) mask |= VK_COLOR_COMPONENT_G_BIT;
	if ( b ) mask |= VK_COLOR_COMPONENT_B_BIT;
	if ( a ) mask |= VK_COLOR_COMPONENT_A_BIT;

	qvkCmdSetColorWriteMaskEXT( vk.cmd->command_buffer, 0, 1, &mask );
}

void vk_clear_depth( qboolean clear_stencil ) {

	VkClearAttachment attachment;
	VkClearRect clear_rect[1];

	if ( !vk.active )
		return;
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE || !vk.inRenderPass )
		return;

	if ( vk_world.dirty_depth_attachment == 0 )
		return;

	attachment.colorAttachment = 0;
#ifdef USE_REVERSED_DEPTH
	attachment.clearValue.depthStencil.depth = 0.0f;
#else
	attachment.clearValue.depthStencil.depth = 1.0f;
#endif
	attachment.clearValue.depthStencil.stencil = 0;
	if ( clear_stencil && glConfig.stencilBits > 0 ) {
		attachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	} else {
		attachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	}

	vk_get_scissor_rect( &clear_rect[0].rect );
	clear_rect[0].baseArrayLayer = 0;
	clear_rect[0].layerCount = 1;

	qvkCmdClearAttachments( vk.cmd->command_buffer, 1, &attachment, 1, clear_rect );
}

/* vk_bind_*, vk_alloc_storage, vk_tess_index, vk_*descriptor*, vk_draw_*: vk_draw_state.c */

void vk_begin_bloom_extract_render_pass( void )
{
	VkFramebuffer frameBuffer = vk.framebuffers.bloom_extract;

	//vk.renderPassIndex = RENDER_PASS_BLOOM_EXTRACT; // doesn't matter, we will use dedicated pipelines

	vk.renderWidth = gls.captureWidth;
	vk.renderHeight = gls.captureHeight;

	//vk.renderScaleX = (float)vk.renderWidth / (float)glConfig.vidWidth;
	//vk.renderScaleY = (float)vk.renderHeight / (float)glConfig.vidHeight;
	vk.renderScaleX = vk.renderScaleY = 1.0f;

	vk_begin_render_pass_tracked( vk.render_pass.bloom_extract, frameBuffer, qfalse, vk.renderWidth, vk.renderHeight );
}


void vk_begin_blur_render_pass( uint32_t index )
{
	VkFramebuffer frameBuffer = vk.framebuffers.blur[ index ];

	//vk.renderPassIndex = RENDER_PASS_BLOOM_EXTRACT; // doesn't matter, we will use dedicated pipelines

	vk.renderWidth = gls.captureWidth / ( 2 << ( index / 2 ) );
	vk.renderHeight = gls.captureHeight / ( 2 << ( index / 2 ) );

	//vk.renderScaleX = (float)vk.renderWidth / (float)glConfig.vidWidth;
	//vk.renderScaleY = (float)vk.renderHeight / (float)glConfig.vidHeight;
	vk.renderScaleX = vk.renderScaleY = 1.0f;

	vk_begin_render_pass_tracked( vk.render_pass.blur[ index ], frameBuffer, qfalse, vk.renderWidth, vk.renderHeight );
}


void vk_begin_ssao_render_pass( void )
{
	VkFramebuffer frameBuffer = vk.framebuffers.ssao;

	vk.renderWidth = glConfig.vidWidth;
	vk.renderHeight = glConfig.vidHeight;
	vk.renderScaleX = vk.renderScaleY = 1.0f;

	vk_begin_render_pass_tracked( vk.render_pass.ssao, frameBuffer, qfalse, vk.renderWidth, vk.renderHeight );
}


void vk_begin_ssao_blur_render_pass( void )
{
	VkFramebuffer frameBuffer = vk.framebuffers.ssao_blur;

	vk.renderWidth = glConfig.vidWidth;
	vk.renderHeight = glConfig.vidHeight;
	vk.renderScaleX = vk.renderScaleY = 1.0f;

	vk_begin_render_pass_tracked( vk.render_pass.ssao_blur, frameBuffer, qfalse, vk.renderWidth, vk.renderHeight );
}


void vk_begin_ssao_combine_render_pass( void )
{
	VkFramebuffer frameBuffer = vk.framebuffers.ssao_combine;

	vk.renderWidth = glConfig.vidWidth;
	vk.renderHeight = glConfig.vidHeight;
	vk.renderScaleX = vk.renderScaleY = 1.0f;

	vk_begin_render_pass_tracked( vk.render_pass.ssao_combine, frameBuffer, qfalse, vk.renderWidth, vk.renderHeight );
}


void vk_oit_pass( const struct drawSurfsCommand_s *cmd )
{
	VkImageCopy copy_region;

	if ( !r_oit || !r_oit->integer || !r_fbo || !r_fbo->integer || !vk.fboActive ||
		vk.render_pass.oit_accum == VK_NULL_HANDLE || vk.render_pass.oit_resolve == VK_NULL_HANDLE ||
		vk.framebuffers.oit_accum == VK_NULL_HANDLE || vk.framebuffers.oit_resolve == VK_NULL_HANDLE ||
		vk.oit_resolve_pipeline == VK_NULL_HANDLE || vk.oit_accum_pipeline == VK_NULL_HANDLE ||
		vk.oit_opaque_descriptor == VK_NULL_HANDLE || vk.oit_accum_descriptor == VK_NULL_HANDLE ||
		vk.oit_reveal_descriptor == VK_NULL_HANDLE || vk.oit_depth_descriptor == VK_NULL_HANDLE ||
		vk.fog_scene_image_view == VK_NULL_HANDLE || vk.oit_accum_image_view == VK_NULL_HANDLE ||
		vk.oit_reveal_image_view == VK_NULL_HANDLE )
		return;

	vk_end_render_pass();

	/* Copy opaque scene to fog_scene for resolve */
	Com_Memset( &copy_region, 0, sizeof( copy_region ) );
	copy_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copy_region.srcSubresource.layerCount = 1;
	copy_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copy_region.dstSubresource.layerCount = 1;
	copy_region.extent.width = glConfig.vidWidth;
	copy_region.extent.height = glConfig.vidHeight;
	copy_region.extent.depth = 1;
	record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT );
	record_image_layout_transition( vk.cmd->command_buffer, vk.fog_scene_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT );
	qvkCmdCopyImage( vk.cmd->command_buffer,
		vk.color_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		vk.fog_scene_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &copy_region );
	record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
	record_image_layout_transition( vk.cmd->command_buffer, vk.fog_scene_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );

	if ( vk.msaaActive ) {
		VkImageAspectFlags depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
		if ( glConfig.stencilBits > 0 ) {
			depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
		record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
		vk_resolve_volumetric_depth_msaa();
	}

	/* OIT accum pass: draw transparent surfaces (when oit_accum_pipeline ready) */
	vk_begin_render_pass_tracked( vk.render_pass.oit_accum, vk.framebuffers.oit_accum, qtrue, glConfig.vidWidth, glConfig.vidHeight );
	if ( vk.oit_accum_pipeline ) {
		backEnd.oitAccumPass = qtrue;
		backEnd.drawSurfFilter = 2;
		RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs );
		backEnd.oitAccumPass = qfalse;
		backEnd.drawSurfFilter = 0;
	}
	vk_end_render_pass();

	/* OIT resolve: composite opaque + accum to main color */
	record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT );
	vk_begin_render_pass_tracked( vk.render_pass.oit_resolve, vk.framebuffers.oit_resolve, qfalse, glConfig.vidWidth, glConfig.vidHeight );
	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.oit_resolve_pipeline );
	{
		VkDescriptorSet sets[3] = { vk.oit_opaque_descriptor, vk.oit_accum_descriptor, vk.oit_reveal_descriptor };
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_oit_resolve, 0, 3, sets, 0, NULL );
	}
	vk_set_fullscreen_viewport_scissor( glConfig.vidWidth, glConfig.vidHeight );
	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
	vk_end_render_pass();
	record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
	if ( vk.msaaActive ) {
		VkImageAspectFlags depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
		if ( glConfig.stencilBits > 0 ) {
			depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
		record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT );
	}

	/* Resume main pass for sun, flares, etc. */
	vk_begin_post_bloom_render_pass();
}


void vk_begin_ssr_render_pass( void )
{
	VkFramebuffer frameBuffer = vk.framebuffers.ssr;

	vk.renderWidth = glConfig.vidWidth;
	vk.renderHeight = glConfig.vidHeight;
	vk.renderScaleX = vk.renderScaleY = 1.0f;

	vk_begin_render_pass_tracked( vk.render_pass.ssr, frameBuffer, qfalse, vk.renderWidth, vk.renderHeight );
}


void vk_ssr_pass( void )
{
	typedef struct {
		float projection[16];
		float invProjection[16];
		float params[4];   /* maxDistance, stepSize, thickness, fadeEdge */
		float params2[4]; /* roughnessThreshold, intensity, pad, pad */
	} vk_ssr_push_t;

	VkImageAspectFlags depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	vk_ssr_push_t push;

	if ( !PostFX_SSR_IsEnabled() || !vk.fboActive || vk.render_pass.ssr == VK_NULL_HANDLE ||
		vk.ssr_pipeline == VK_NULL_HANDLE || vk.ssr_image == VK_NULL_HANDLE )
	{
		return;
	}

	if ( vk.renderPassIndex == RENDER_PASS_SCREENMAP || vk.renderPassIndex == RENDER_PASS_SUN_SHADOW )
	{
		return;
	}

	if ( !backEnd.doneSurfaces || !backEnd.doneBloom )
	{
		return;
	}

	if ( glConfig.stencilBits > 0 )
		depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;

	vk_end_render_pass();

	record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, 0, 0 );

	vk_begin_ssr_render_pass();
	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.ssr_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_ssr, 0, 2, vk.ssr_descriptor, 0, NULL );

	Com_Memcpy( push.projection, backEnd.viewParms.projectionMatrix, sizeof( push.projection ) );
	{
		float inv[16];
		if ( !vk_mat4_inverse( backEnd.viewParms.projectionMatrix, inv ) )
			Com_Memcpy( inv, backEnd.viewParms.projectionMatrix, sizeof( inv ) );
		Com_Memcpy( push.invProjection, inv, sizeof( push.invProjection ) );
	}
	push.params[0] = PostFX_SSR_GetMaxDistance();
	push.params[1] = PostFX_SSR_GetStepSize();
	push.params[2] = PostFX_SSR_GetThickness();
	push.params[3] = PostFX_SSR_GetFadeEdge();
	push.params2[0] = PostFX_SSR_GetRoughnessThreshold();
	push.params2[1] = PostFX_SSR_GetIntensity();
	push.params2[2] = PostFX_SSR_GetMaxDepthGradient();
	push.params2[3] = 0.0f;

	qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout_ssr, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( push ), &push );
	vk_set_fullscreen_viewport_scissor( vk.renderWidth, vk.renderHeight );
	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
	vk_end_render_pass();

	/* Copy ssr_image back to color */
	record_image_layout_transition( vk.cmd->command_buffer, vk.ssr_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 0 );
	record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );

	{
		VkImageCopy region;
		Com_Memset( &region, 0, sizeof( region ) );
		region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.srcSubresource.layerCount = 1;
		region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.dstSubresource.layerCount = 1;
		region.extent.width = vk.renderWidth;
		region.extent.height = vk.renderHeight;
		region.extent.depth = 1;
		qvkCmdCopyImage( vk.cmd->command_buffer, vk.ssr_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			vk.color_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );
	}

	record_image_layout_transition( vk.cmd->command_buffer, vk.ssr_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
	record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );

	record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0, 0 );
}


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

static qboolean vk_begin_local_spot_shadow_render_pass( void )
{
	VkImageAspectFlags depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;

	if ( !vk.fboActive || vk.render_pass.local_spot_shadow == VK_NULL_HANDLE ||
		vk.framebuffers.local_spot_shadow == VK_NULL_HANDLE ||
		vk.local_spot_shadow_atlas_image == VK_NULL_HANDLE ||
		vk.local_spot_shadow_atlas_size == 0 )
	{
		return qfalse;
	}

	if ( glConfig.stencilBits > 0 ) {
		depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	vk_end_render_pass();

	record_image_layout_transition( vk.cmd->command_buffer, vk.local_spot_shadow_atlas_image, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT );

	vk.renderPassIndex = RENDER_PASS_SUN_SHADOW;
	vk.renderWidth = vk.local_spot_shadow_atlas_size;
	vk.renderHeight = vk.local_spot_shadow_atlas_size;
	vk.renderScaleX = vk.renderScaleY = 1.0f;
	vk_begin_render_pass_tracked( vk.render_pass.local_spot_shadow, vk.framebuffers.local_spot_shadow, qtrue, vk.renderWidth, vk.renderHeight );

	return qtrue;
}

static void vk_end_local_spot_shadow_render_pass( void )
{
	VkImageAspectFlags depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;

	if ( !vk.fboActive || vk.local_spot_shadow_atlas_image == VK_NULL_HANDLE ) {
		return;
	}

	if ( glConfig.stencilBits > 0 ) {
		depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	vk_end_render_pass();

	record_image_layout_transition( vk.cmd->command_buffer, vk.local_spot_shadow_atlas_image, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
}

static qboolean vk_begin_local_point_shadow_render_pass( int faceLayer )
{
	VkImageAspectFlags depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;

	if ( !vk.fboActive || vk.render_pass.local_point_shadow == VK_NULL_HANDLE ||
		vk.local_point_shadow_array_image == VK_NULL_HANDLE ||
		vk.local_point_shadow_face_size == 0 ||
		faceLayer < 0 || faceLayer >= (int)( vk.local_point_shadow_capacity * 6 ) ||
		vk.framebuffers.local_point_shadow[faceLayer] == VK_NULL_HANDLE )
	{
		return qfalse;
	}

	if ( glConfig.stencilBits > 0 ) {
		depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	vk_end_render_pass();

	record_image_layout_transition( vk.cmd->command_buffer, vk.local_point_shadow_array_image, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT );

	vk.renderPassIndex = RENDER_PASS_SUN_SHADOW;
	vk.renderWidth = vk.local_point_shadow_face_size;
	vk.renderHeight = vk.local_point_shadow_face_size;
	vk.renderScaleX = vk.renderScaleY = 1.0f;
	vk_begin_render_pass_tracked( vk.render_pass.local_point_shadow, vk.framebuffers.local_point_shadow[faceLayer], qtrue, vk.renderWidth, vk.renderHeight );

	return qtrue;
}

static void vk_end_local_point_shadow_render_pass( void )
{
	VkImageAspectFlags depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;

	if ( !vk.fboActive || vk.local_point_shadow_array_image == VK_NULL_HANDLE ) {
		return;
	}

	if ( glConfig.stencilBits > 0 ) {
		depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	vk_end_render_pass();

	record_image_layout_transition( vk.cmd->command_buffer, vk.local_point_shadow_array_image, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
}


static void vk_begin_volumetric_render_pass( void )
{
	VkFramebuffer frameBuffer = vk.framebuffers.volumetric[ vk.cmd->swapchain_image_index ];

	vk.renderWidth = glConfig.vidWidth;
	vk.renderHeight = glConfig.vidHeight;
	vk.renderScaleX = vk.renderScaleY = 1.0f;

	vk_begin_render_pass_tracked( vk.render_pass.volumetric, frameBuffer, qfalse, vk.renderWidth, vk.renderHeight );
}

static void vk_resolve_volumetric_depth_msaa( void )
{
	if ( !vk.msaaActive || vk.volumetric_depth_resolve_pipeline == VK_NULL_HANDLE ||
		vk.volumetric_depth_resolve_descriptor == VK_NULL_HANDLE ||
		vk.depth_image == VK_NULL_HANDLE || vk.volumetric_depth_image == VK_NULL_HANDLE )
	{
		return;
	}

	record_image_layout_transition( vk.cmd->command_buffer, vk.volumetric_depth_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.volumetric_depth_resolve_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
		vk.volumetric_depth_resolve_pipeline_layout, 0, 1, &vk.volumetric_depth_resolve_descriptor, 0, NULL );
	qvkCmdDispatch( vk.cmd->command_buffer, ( glConfig.vidWidth + 7 ) / 8, ( glConfig.vidHeight + 7 ) / 8, 1 );

	record_image_layout_transition( vk.cmd->command_buffer, vk.volumetric_depth_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
}

static void vk_set_volumetric_pass_params( float stage, float x, float y, float z )
{
	if ( !vk.volumetric_params_ptr ) {
		return;
	}

	volumetric_params_t *params = (volumetric_params_t *)vk.volumetric_params_ptr;
	params->passParams[0] = stage;
	params->passParams[1] = x;
	params->passParams[2] = y;
	params->passParams[3] = z;
}

static void vk_write_volumetric_timestamp( uint32_t query_index, VkPipelineStageFlagBits stage )
{
	if ( vk.volumetric_query_pool == VK_NULL_HANDLE || !qvkCmdWriteTimestamp ) {
		return;
	}
	if ( !r_volumetricFogPerfTimers || !r_volumetricFogPerfTimers->integer ) {
		return;
	}
	if ( query_index >= VK_VOLUMETRY_QUERY_USED ) {
		return;
	}
	const uint32_t query_base = vk.cmd_index * VK_VOLUMETRIC_QUERY_SLOTS;
	qvkCmdWriteTimestamp( vk.cmd->command_buffer, stage, vk.volumetric_query_pool, query_base + query_index );
}

static void vk_update_fluid_auto_scale( void )
{
	float min_resolution;
	float adjust_rate;
	int min_iterations;
	int base_iterations;
	float target_ms;
	qboolean autoscale_enabled;

	base_iterations = ( r_fogFluidPressureIterations ) ? r_fogFluidPressureIterations->integer : 12;
	if ( base_iterations < 1 ) {
		base_iterations = 1;
	} else if ( base_iterations > VK_FLUID_MAX_PRESSURE_ITERATIONS ) {
		base_iterations = VK_FLUID_MAX_PRESSURE_ITERATIONS;
	}
	min_iterations = ( r_fogFluidAutoScaleMinIterations ) ? r_fogFluidAutoScaleMinIterations->integer : 6;
	if ( min_iterations < 1 ) {
		min_iterations = 1;
	} else if ( min_iterations > base_iterations ) {
		min_iterations = base_iterations;
	}

	min_resolution = ( r_fogFluidAutoScaleMinResolution ) ? r_fogFluidAutoScaleMinResolution->value : 0.45f;
	if ( min_resolution < 0.125f ) {
		min_resolution = 0.125f;
	} else if ( min_resolution > 1.0f ) {
		min_resolution = 1.0f;
	}
	adjust_rate = ( r_fogFluidAutoScaleRate ) ? r_fogFluidAutoScaleRate->value : 0.08f;
	if ( adjust_rate < 0.01f ) {
		adjust_rate = 0.01f;
	} else if ( adjust_rate > 1.0f ) {
		adjust_rate = 1.0f;
	}
	target_ms = ( r_fogFluidTargetMs ) ? r_fogFluidTargetMs->value : 1.2f;
	if ( target_ms < 0.1f ) {
		target_ms = 0.1f;
	} else if ( target_ms > 8.0f ) {
		target_ms = 8.0f;
	}

	autoscale_enabled = ( r_fogFluidAutoScale && r_fogFluidAutoScale->integer &&
		r_fogFluid && r_fogFluid->integer && vk.volumetric_fluid_ms > 0.0f ) ? qtrue : qfalse;

	if ( vk.fluid_dynamic_resolution_scale <= 0.0f || vk.fluid_dynamic_resolution_scale > 1.0f ) {
		vk.fluid_dynamic_resolution_scale = 1.0f;
	}
	if ( vk.fluid_dynamic_pressure_iterations <= 0 ) {
		vk.fluid_dynamic_pressure_iterations = base_iterations;
	}

	if ( !autoscale_enabled ) {
		vk.fluid_dynamic_resolution_scale = 1.0f;
		vk.fluid_dynamic_pressure_iterations = base_iterations;
		return;
	}

	if ( vk.volumetric_fluid_ms > target_ms * 1.05f ) {
		// Budget miss: reduce effective resolution first, then iterations.
		vk.fluid_dynamic_resolution_scale -= adjust_rate;
		if ( vk.fluid_dynamic_resolution_scale < min_resolution ) {
			vk.fluid_dynamic_resolution_scale = min_resolution;
			if ( vk.fluid_dynamic_pressure_iterations > min_iterations ) {
				vk.fluid_dynamic_pressure_iterations--;
			}
		}
	} else if ( vk.volumetric_fluid_ms < target_ms * 0.75f ) {
		// Headroom: recover iterations first, then resolution.
		if ( vk.fluid_dynamic_pressure_iterations < base_iterations ) {
			vk.fluid_dynamic_pressure_iterations++;
		} else {
			vk.fluid_dynamic_resolution_scale += adjust_rate;
		}
	}

	if ( vk.fluid_dynamic_resolution_scale < min_resolution ) {
		vk.fluid_dynamic_resolution_scale = min_resolution;
	} else if ( vk.fluid_dynamic_resolution_scale > 1.0f ) {
		vk.fluid_dynamic_resolution_scale = 1.0f;
	}
	if ( vk.fluid_dynamic_pressure_iterations < min_iterations ) {
		vk.fluid_dynamic_pressure_iterations = min_iterations;
	} else if ( vk.fluid_dynamic_pressure_iterations > base_iterations ) {
		vk.fluid_dynamic_pressure_iterations = base_iterations;
	}
}

static void vk_update_volumetric_perf_queries( void )
{
	uint64_t query_values[ VK_VOLUMETRIC_QUERY_SLOTS ];
	/* Read from the frame that just completed (cmd_index was advanced at end of previous frame) */
	const uint32_t read_slot = ( vk.cmd_index + NUM_COMMAND_BUFFERS - 1 ) % NUM_COMMAND_BUFFERS;
	const uint32_t query_base = read_slot * VK_VOLUMETRIC_QUERY_SLOTS;
	const VkResult query_result = qvkGetQueryPoolResults( vk.device,
		vk.volumetric_query_pool,
		query_base,
		VK_VOLUMETRY_QUERY_USED,
		sizeof( query_values ),
		query_values,
		sizeof( uint64_t ),
		VK_QUERY_RESULT_64_BIT );
	if ( query_result != VK_SUCCESS ) {
		return;
	}

	const double to_ms = (double)vk.volumetric_timestamp_period_ns * 1e-6;
	for ( int i = 0; i < VK_VOLUMETRY_QUERY_USED - 1; i++ ) {
		const uint64_t t0 = query_values[i];
		const uint64_t t1 = query_values[i + 1];
		vk.volumetric_stage_ms[i] = ( t1 >= t0 ) ? (float)( (double)( t1 - t0 ) * to_ms ) : 0.0f;
	}
	vk.volumetric_fluid_ms = vk.volumetric_stage_ms[ VK_VOLUMETRY_QUERY_AFTER_FLUID_SIM - VK_VOLUMETRY_QUERY_FOG_START ];
	vk.volumetric_total_ms = ( query_values[ VK_VOLUMETRY_QUERY_FOG_END ] >= query_values[ VK_VOLUMETRY_QUERY_FOG_START ] ) ?
		(float)( (double)( query_values[ VK_VOLUMETRY_QUERY_FOG_END ] - query_values[ VK_VOLUMETRY_QUERY_FOG_START ] ) * to_ms ) : 0.0f;

	vk_update_fluid_auto_scale();

	if ( r_volumetricFogPerfTimers && r_volumetricFogPerfTimers->integer ) {
		int print_interval = ( r_volumetricFogPerfPrintInterval ) ? r_volumetricFogPerfPrintInterval->integer : 120;
		if ( print_interval < 1 ) {
			print_interval = 1;
		}
		if ( ( vk.volumetric_frame % (uint32_t)print_interval ) == 0u ) {
			ri.Printf( PRINT_DEVELOPER,
				"[VK][fog][perf] total=%.3fms fluid=%.3fms clear=%.3f global=%.3f volume=%.3f fluidInject=%.3f sun=%.3f local=%.3f temporal=%.3f scale=%.3f iters=%d\n",
				vk.volumetric_total_ms,
				vk.volumetric_fluid_ms,
				vk.volumetric_stage_ms[ VK_VOLUMETRY_QUERY_AFTER_CLEAR - VK_VOLUMETRY_QUERY_FOG_START ],
				vk.volumetric_stage_ms[ VK_VOLUMETRY_QUERY_AFTER_GLOBAL_DENSITY - VK_VOLUMETRY_QUERY_FOG_START ],
				vk.volumetric_stage_ms[ VK_VOLUMETRY_QUERY_AFTER_VOLUME_DENSITY - VK_VOLUMETRY_QUERY_FOG_START ],
				vk.volumetric_stage_ms[ VK_VOLUMETRY_QUERY_AFTER_FLUID_DENSITY - VK_VOLUMETRY_QUERY_FOG_START ],
				vk.volumetric_stage_ms[ VK_VOLUMETRY_QUERY_AFTER_SUN - VK_VOLUMETRY_QUERY_FOG_START ],
				vk.volumetric_stage_ms[ VK_VOLUMETRY_QUERY_AFTER_LOCAL - VK_VOLUMETRY_QUERY_FOG_START ],
				vk.volumetric_stage_ms[ VK_VOLUMETRY_QUERY_AFTER_TEMPORAL - VK_VOLUMETRY_QUERY_FOG_START ],
				vk.fluid_dynamic_resolution_scale,
				vk.fluid_dynamic_pressure_iterations );
		}
	}
}

static void vk_volumetric_stage_barrier( VkImage image )
{
	if ( image == VK_NULL_HANDLE ) {
		return;
	}
	record_image_layout_transition( vk.cmd->command_buffer, image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
}

static void vk_fluid_simulation_pass( float delta_time )
{
	enum {
		VK_FLUID_OP_ADVECT_VELOCITY = 0,
		VK_FLUID_OP_ADVECT_DENSITY = 1,
		VK_FLUID_OP_DIVERGENCE = 2,
		VK_FLUID_OP_PRESSURE = 3,
		VK_FLUID_OP_GRADIENT = 4
	};

	if ( !r_fogFluid || !r_fogFluid->integer ) {
		return;
	}
	if ( vk.volumetric_fluid_pipeline_layout == VK_NULL_HANDLE || vk.volumetric_fluid_descriptor == VK_NULL_HANDLE ) {
		return;
	}
	if ( vk.volumetric_fluid_advect_pipeline == VK_NULL_HANDLE ||
		vk.volumetric_fluid_divergence_pipeline == VK_NULL_HANDLE ||
		vk.volumetric_fluid_pressure_pipeline == VK_NULL_HANDLE ||
		vk.volumetric_fluid_gradient_pipeline == VK_NULL_HANDLE ) {
		return;
	}
	if ( vk.fluid_width == 0 || vk.fluid_height == 0 ) {
		return;
	}
	if ( vk.volumetric_params_ptr == NULL ) {
		return;
	}
	/* Guard: ensure all fluid images exist before dispatching (avoids Bus error on some drivers) */
	if ( vk.fluid_velocity_images[0] == VK_NULL_HANDLE || vk.fluid_velocity_images[1] == VK_NULL_HANDLE ||
	     vk.fluid_density_images[0] == VK_NULL_HANDLE || vk.fluid_density_images[1] == VK_NULL_HANDLE ||
	     vk.fluid_pressure_images[0] == VK_NULL_HANDLE || vk.fluid_pressure_images[1] == VK_NULL_HANDLE ||
	     vk.fluid_divergence_image == VK_NULL_HANDLE ) {
		return;
	}

	volumetric_params_t *params_rw = (volumetric_params_t *)vk.volumetric_params_ptr;
	if ( params_rw->fluidParams1[3] < 0.5f ) {
		return;
	}

	uint32_t active_width = (uint32_t)MAX( 1, (int)( params_rw->fluidParams2[0] + 0.5f ) );
	uint32_t active_height = (uint32_t)MAX( 1, (int)( params_rw->fluidParams2[1] + 0.5f ) );
	if ( active_width > vk.fluid_width ) {
		active_width = vk.fluid_width;
	}
	if ( active_height > vk.fluid_height ) {
		active_height = vk.fluid_height;
	}
	if ( active_width == 0 || active_height == 0 ) {
		return;
	}
	vk.fluid_active_width = active_width;
	vk.fluid_active_height = active_height;
	const uint32_t groups_x = ( active_width + 15 ) / 16;
	const uint32_t groups_y = ( active_height + 15 ) / 16;
	uint32_t vel_read = vk.fluid_velocity_index & 1u;
	uint32_t vel_write = vel_read ^ 1u;
	uint32_t den_read = vk.fluid_density_index & 1u;
	uint32_t den_write = den_read ^ 1u;
	uint32_t pressure_read = vk.fluid_pressure_index & 1u;
	uint32_t pressure_write = pressure_read ^ 1u;
	int pressure_iterations = (int)params_rw->fluidParams1[2];

	if ( pressure_iterations < 1 ) {
		pressure_iterations = 1;
	} else if ( pressure_iterations > VK_FLUID_MAX_PRESSURE_ITERATIONS ) {
		pressure_iterations = VK_FLUID_MAX_PRESSURE_ITERATIONS;
	}

	if ( delta_time <= 0.0f ) {
		delta_time = 1.0f / 60.0f;
	}

	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
		vk.volumetric_fluid_pipeline_layout, 0, 1, &vk.volumetric_fluid_descriptor, 0, NULL );

	// Semi-Lagrangian velocity advection with external forces.
	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.volumetric_fluid_advect_pipeline );
	vk_set_volumetric_pass_params( (float)VK_FLUID_OP_ADVECT_VELOCITY, (float)vel_read, (float)vel_write, 0.0f );
	qvkCmdDispatch( vk.cmd->command_buffer, groups_x, groups_y, 1 );
	vk_volumetric_stage_barrier( vk.fluid_velocity_images[vel_write] );

	// Divergence from advected velocity.
	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.volumetric_fluid_divergence_pipeline );
	vk_set_volumetric_pass_params( (float)VK_FLUID_OP_DIVERGENCE, (float)vel_write, 0.0f, 0.0f );
	qvkCmdDispatch( vk.cmd->command_buffer, groups_x, groups_y, 1 );
	vk_volumetric_stage_barrier( vk.fluid_divergence_image );

	// Jacobi pressure solve.
	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.volumetric_fluid_pressure_pipeline );
	for ( int i = 0; i < pressure_iterations; i++ ) {
		vk_set_volumetric_pass_params( (float)VK_FLUID_OP_PRESSURE, (float)pressure_read, (float)pressure_write, 0.0f );
		qvkCmdDispatch( vk.cmd->command_buffer, groups_x, groups_y, 1 );
		vk_volumetric_stage_barrier( vk.fluid_pressure_images[pressure_write] );

		const uint32_t tmp = pressure_read;
		pressure_read = pressure_write;
		pressure_write = tmp;
	}

	// Projection: subtract pressure gradient from velocity.
	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.volumetric_fluid_gradient_pipeline );
	vk_set_volumetric_pass_params( (float)VK_FLUID_OP_GRADIENT, (float)vel_write, (float)vel_read, (float)pressure_read );
	qvkCmdDispatch( vk.cmd->command_buffer, groups_x, groups_y, 1 );
	vk_volumetric_stage_barrier( vk.fluid_velocity_images[vel_read] );

	// Semi-Lagrangian density advection using projected velocity.
	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.volumetric_fluid_advect_pipeline );
	vk_set_volumetric_pass_params( (float)VK_FLUID_OP_ADVECT_DENSITY, (float)den_read, (float)den_write, (float)vel_read );
	qvkCmdDispatch( vk.cmd->command_buffer, groups_x, groups_y, 1 );
	vk_volumetric_stage_barrier( vk.fluid_density_images[den_write] );

	vk.fluid_velocity_index = vel_read;
	vk.fluid_density_index = den_write;
	vk.fluid_pressure_index = pressure_read;
	params_rw->fluidParams2[0] = (float)vk.fluid_active_width;
	params_rw->fluidParams2[1] = (float)vk.fluid_active_height;
	params_rw->fluidParams2[2] = (float)vk.fluid_velocity_index;
	params_rw->fluidParams2[3] = (float)vk.fluid_density_index;
}

static const dlight_t *vk_get_volumetric_local_light( int local_index )
{
	int current = 0;

	if ( local_index < 0 ) {
		return NULL;
	}

	for ( int i = 0; i < (int)backEnd.viewParms.num_dlights; i++ ) {
		const dlight_t *dl = &backEnd.viewParms.dlights[i];
		if ( dl->radius <= 0.001f ) {
			continue;
		}
		if ( current == local_index ) {
			return dl;
		}
		current++;
	}

	return NULL;
}

static qboolean vk_build_local_shadow_view_axes(
	const vec3_t origin,
	const vec3_t forward_in,
	const vec3_t up_hint,
	float fov_degrees,
	float max_distance,
	int viewportX,
	int viewportY,
	int viewportWidth,
	int viewportHeight,
	viewParms_t *shadowParms,
	float *outViewProj )
{
	vec3_t forward;
	vec3_t upRef;
	vec3_t right;
	vec3_t up;
	float viewerMatrix[16];
	float lightView[16];
	float nearPlane = ( r_znear ) ? r_znear->value : 8.0f;

	if ( !shadowParms || !outViewProj ) {
		return qfalse;
	}
	if ( viewportWidth <= 0 || viewportHeight <= 0 ) {
		return qfalse;
	}

	if ( nearPlane < 1.0f ) {
		nearPlane = 1.0f;
	}
	if ( max_distance <= nearPlane + 1.0f ) {
		return qfalse;
	}

	VectorCopy( forward_in, forward );
	if ( VectorNormalize( forward ) <= 0.0f ) {
		return qfalse;
	}

	VectorCopy( up_hint, upRef );
	if ( VectorLengthSquared( upRef ) < 1e-6f || fabsf( DotProduct( forward, upRef ) ) > 0.95f ) {
		VectorSet( upRef, 0.0f, 0.0f, 1.0f );
		if ( fabsf( DotProduct( forward, upRef ) ) > 0.95f ) {
			VectorSet( upRef, 0.0f, 1.0f, 0.0f );
		}
	}

	CrossProduct( upRef, forward, right );
	if ( VectorNormalize( right ) <= 0.0f ) {
		return qfalse;
	}
	CrossProduct( forward, right, up );
	VectorNormalize( up );

	*shadowParms = backEnd.viewParms;
	VectorCopy( origin, shadowParms->or.origin );
	VectorCopy( origin, shadowParms->pvsOrigin );
	VectorCopy( forward, shadowParms->or.axis[0] );
	VectorCopy( right, shadowParms->or.axis[1] );
	VectorCopy( up, shadowParms->or.axis[2] );
	shadowParms->portalView = PV_NONE;
	shadowParms->targetCube = NULL;
	shadowParms->targetCubeLayer = 0;
	shadowParms->viewportX = viewportX;
	shadowParms->viewportY = viewportY;
	shadowParms->viewportWidth = viewportWidth;
	shadowParms->viewportHeight = viewportHeight;
	shadowParms->scissorX = viewportX;
	shadowParms->scissorY = viewportY;
	shadowParms->scissorWidth = viewportWidth;
	shadowParms->scissorHeight = viewportHeight;
	shadowParms->zFar = max_distance;
	shadowParms->fovX = fov_degrees;
	shadowParms->fovY = fov_degrees;
	R_SetupProjection( shadowParms, nearPlane, qfalse );

	viewerMatrix[0] = shadowParms->or.axis[0][0];
	viewerMatrix[4] = shadowParms->or.axis[0][1];
	viewerMatrix[8] = shadowParms->or.axis[0][2];
	viewerMatrix[12] = -origin[0] * viewerMatrix[0] + -origin[1] * viewerMatrix[4] + -origin[2] * viewerMatrix[8];
	viewerMatrix[1] = shadowParms->or.axis[1][0];
	viewerMatrix[5] = shadowParms->or.axis[1][1];
	viewerMatrix[9] = shadowParms->or.axis[1][2];
	viewerMatrix[13] = -origin[0] * viewerMatrix[1] + -origin[1] * viewerMatrix[5] + -origin[2] * viewerMatrix[9];
	viewerMatrix[2] = shadowParms->or.axis[2][0];
	viewerMatrix[6] = shadowParms->or.axis[2][1];
	viewerMatrix[10] = shadowParms->or.axis[2][2];
	viewerMatrix[14] = -origin[0] * viewerMatrix[2] + -origin[1] * viewerMatrix[6] + -origin[2] * viewerMatrix[10];
	viewerMatrix[3] = 0.0f;
	viewerMatrix[7] = 0.0f;
	viewerMatrix[11] = 0.0f;
	viewerMatrix[15] = 1.0f;

	myGlMultMatrix( viewerMatrix, vk_local_shadow_flip_matrix, lightView );
	Matrix16Identity( shadowParms->world.modelMatrix );
	Com_Memcpy( shadowParms->world.modelViewMatrix, lightView, sizeof( lightView ) );
	VectorCopy( origin, shadowParms->world.viewOrigin );
	VectorClear( shadowParms->world.origin );
	AxisCopy( axisDefault, shadowParms->world.axis );

	myGlMultMatrix( lightView, shadowParms->projectionMatrix, outViewProj );
	return qtrue;
}

static qboolean vk_build_local_spot_shadow_view( const dlight_t *dl, int viewportX, int viewportY, int viewportSize, viewParms_t *shadowParms, float *outViewProj )
{
	vec3_t forward;
	vec3_t up = { 0.0f, 0.0f, 1.0f };

	if ( !dl || !shadowParms || !outViewProj ) {
		return qfalse;
	}

	VectorSubtract( dl->origin2, dl->origin, forward );
	if ( VectorNormalize( forward ) <= 0.001f ) {
		VectorSet( forward, 0.0f, 0.0f, -1.0f );
	}

	return vk_build_local_shadow_view_axes( dl->origin, forward, up, 70.0f, dl->radius,
		viewportX, viewportY, viewportSize, viewportSize, shadowParms, outViewProj );
}

static qboolean vk_build_local_point_shadow_view( const dlight_t *dl, int face, int viewportSize, viewParms_t *shadowParms, float *outViewProj )
{
	vec3_t forward = { 0.0f, 0.0f, -1.0f };
	vec3_t up = { 0.0f, 0.0f, 1.0f };

	if ( !dl || !shadowParms || !outViewProj ) {
		return qfalse;
	}

	switch ( face ) {
		case 0: VectorSet( forward, 1.0f, 0.0f, 0.0f ); VectorSet( up, 0.0f, 0.0f, 1.0f ); break; // +X
		case 1: VectorSet( forward, -1.0f, 0.0f, 0.0f ); VectorSet( up, 0.0f, 0.0f, 1.0f ); break; // -X
		case 2: VectorSet( forward, 0.0f, 1.0f, 0.0f ); VectorSet( up, 0.0f, 0.0f, 1.0f ); break; // +Y
		case 3: VectorSet( forward, 0.0f, -1.0f, 0.0f ); VectorSet( up, 0.0f, 0.0f, 1.0f ); break; // -Y
		case 4: VectorSet( forward, 0.0f, 0.0f, 1.0f ); VectorSet( up, 0.0f, -1.0f, 0.0f ); break; // +Z
		case 5: VectorSet( forward, 0.0f, 0.0f, -1.0f ); VectorSet( up, 0.0f, 1.0f, 0.0f ); break; // -Z
		default: return qfalse;
	}

	return vk_build_local_shadow_view_axes( dl->origin, forward, up, 90.0f, dl->radius,
		0, 0, viewportSize, viewportSize, shadowParms, outViewProj );
}

static qboolean vk_render_local_volumetric_shadow_view( const viewParms_t *shadowParms, qboolean pointShadow, int pointFaceLayer )
{
	renderPass_t saved_pass;
	uint32_t saved_width;
	uint32_t saved_height;
	float saved_scale_x;
	float saved_scale_y;

	if ( !shadowParms || backEnd.refdef.numDrawSurfs <= 0 || !backEnd.refdef.drawSurfs ) {
		return qfalse;
	}
	saved_pass = vk.renderPassIndex;
	saved_width = vk.renderWidth;
	saved_height = vk.renderHeight;
	saved_scale_x = vk.renderScaleX;
	saved_scale_y = vk.renderScaleY;

	if ( pointShadow ) {
		if ( !vk_begin_local_point_shadow_render_pass( pointFaceLayer ) ) {
			vk.renderPassIndex = saved_pass;
			vk.renderWidth = saved_width;
			vk.renderHeight = saved_height;
			vk.renderScaleX = saved_scale_x;
			vk.renderScaleY = saved_scale_y;
			return qfalse;
		}
	} else if ( !vk_begin_local_spot_shadow_render_pass() ) {
		vk.renderPassIndex = saved_pass;
		vk.renderWidth = saved_width;
		vk.renderHeight = saved_height;
		vk.renderScaleX = saved_scale_x;
		vk.renderScaleY = saved_scale_y;
		return qfalse;
	}

	RB_RenderVolumetricShadowView( shadowParms, backEnd.refdef.drawSurfs, backEnd.refdef.numDrawSurfs );
	if ( pointShadow ) {
		vk_end_local_point_shadow_render_pass();
	} else {
		vk_end_local_spot_shadow_render_pass();
	}
	vk.renderPassIndex = saved_pass;
	vk.renderWidth = saved_width;
	vk.renderHeight = saved_height;
	vk.renderScaleX = saved_scale_x;
	vk.renderScaleY = saved_scale_y;
	return qtrue;
}

static void vk_volumetric_compute_pass( void )
{
	enum {
		VK_VOLUMETRIC_STAGE_CLEAR = 0,
		VK_VOLUMETRIC_STAGE_GLOBAL_DENSITY = 1,
		VK_VOLUMETRIC_STAGE_VOLUME_DENSITY = 2,
		VK_VOLUMETRIC_STAGE_LOCAL_LIGHT = 3,
		VK_VOLUMETRIC_STAGE_SUN_LIGHT = 4,
		VK_VOLUMETRIC_STAGE_CLAMP_LEVEL0 = 5,
		VK_VOLUMETRIC_STAGE_CLAMP_LEVEL1 = 6,
		VK_VOLUMETRIC_STAGE_TEMPORAL = 7,
		VK_VOLUMETRIC_STAGE_FLUID_DENSITY = 8
	};

	if ( vk.volumetric_compute_pipeline == VK_NULL_HANDLE || !vk.froxel_width || !vk.froxel_height || !vk.froxel_slices ||
		vk.froxel_extinction_image == VK_NULL_HANDLE || vk.froxel_clamp_image == VK_NULL_HANDLE )
	{
		if ( r_fogDebug && r_fogDebug->integer >= 1 ) {
			ri.Printf( PRINT_WARNING, "[VK][fog] skipping compute pass: pipeline=0x%llx dims=%ux%ux%u ext=0x%llx clamp=0x%llx\n",
				(unsigned long long)(uintptr_t)vk.volumetric_compute_pipeline, vk.froxel_width, vk.froxel_height, vk.froxel_slices,
				(unsigned long long)(uintptr_t)vk.froxel_extinction_image,
				(unsigned long long)(uintptr_t)vk.froxel_clamp_image );
		}
		return;
	}

	const uint32_t groups_x = ( vk.froxel_width + 7 ) / 8;
	const uint32_t groups_y = ( vk.froxel_height + 7 ) / 8;
	const uint32_t groups_z = ( vk.froxel_slices + 3 ) / 4;
	const uint32_t clamp0_width = ( vk.froxel_width + 1 ) / 2;
	const uint32_t clamp0_height = ( vk.froxel_height + 1 ) / 2;
	const uint32_t clamp1_width = ( clamp0_width + 1 ) / 2;
	const uint32_t clamp1_height = ( clamp0_height + 1 ) / 2;
	const uint32_t clamp0_groups_x = ( clamp0_width + 7 ) / 8;
	const uint32_t clamp0_groups_y = ( clamp0_height + 7 ) / 8;
	const uint32_t clamp1_groups_x = ( clamp1_width + 7 ) / 8;
	const uint32_t clamp1_groups_y = ( clamp1_height + 7 ) / 8;
	const uint32_t clamp_groups_z = ( vk.froxel_slices + 3 ) / 4;
	int local_light_count = 0;
	volumetric_params_t *params_rw = (volumetric_params_t *)vk.volumetric_params_ptr;
	qboolean use_local_shadows = ( r_fog_shadows && r_fog_shadows->integer ) ? qtrue : qfalse;
	int spot_shadow_slot = 0;
	int point_shadow_slot = 0;
	int spot_shadow_ready_count = 0;
	int point_shadow_ready_count = 0;

	vk_fluid_simulation_pass( params_rw ? params_rw->fluidParams0[0] : (1.0f / 60.0f) );
	vk_write_volumetric_timestamp( VK_VOLUMETRY_QUERY_AFTER_FLUID_SIM, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.volumetric_compute_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.volumetric_compute_pipeline_layout, 0, 1, &vk.volumetric_compute_descriptor, 0, NULL );

	if ( vk.volumetric_params_ptr ) {
		const volumetric_params_t *params = (const volumetric_params_t *)vk.volumetric_params_ptr;
		local_light_count = (int)( params->volumeCounts[1] + 0.5f );
		if ( local_light_count < 0 ) {
			local_light_count = 0;
		} else if ( local_light_count > VK_VOLUMETRIC_MAX_LIGHTS ) {
			local_light_count = VK_VOLUMETRIC_MAX_LIGHTS;
		}
	}
	vk_volumetric_validation_state.local_light_count = (uint32_t)local_light_count;

	if ( params_rw ) {
		for ( int i = 0; i < VK_VOLUMETRIC_MAX_LIGHTS; i++ ) {
			Matrix16Identity( params_rw->localSpotShadowMatrix[i] );
			for ( int face = 0; face < 6; face++ ) {
				Matrix16Identity( params_rw->localPointShadowMatrix[i][face] );
			}
			params_rw->localShadowAtlasUv[i][0] = 1.0f;
			params_rw->localShadowAtlasUv[i][1] = 1.0f;
			params_rw->localShadowAtlasUv[i][2] = 0.0f;
			params_rw->localShadowAtlasUv[i][3] = 0.0f;
			params_rw->lightExtra[i][3] = -1.0f;
		}
	}

	if ( r_fogDebug && r_fogDebug->integer >= 1 && ( vk.volumetric_frame % 120u ) == 0u ) {
		ri.Printf( PRINT_ALL,
			"[VK][fog] screen=%dx%d froxel=%ux%ux%u groups=%ux%ux%u volImg=0x%llx volView=0x%llx compSet=0x%llx fragSet=0x%llx\n",
			glConfig.vidWidth, glConfig.vidHeight,
			vk.froxel_width, vk.froxel_height, vk.froxel_slices,
			groups_x, groups_y, groups_z,
			(unsigned long long)(uintptr_t)vk.froxel_volume_image,
			(unsigned long long)(uintptr_t)vk.froxel_volume_view,
				(unsigned long long)(uintptr_t)vk.volumetric_compute_descriptor,
				(unsigned long long)(uintptr_t)vk.volumetric_composite_descriptor );
	}

	vk_set_volumetric_pass_params( (float)VK_VOLUMETRIC_STAGE_CLEAR, 0.0f, 0.0f, 0.0f );
	qvkCmdDispatch( vk.cmd->command_buffer, groups_x, groups_y, groups_z );
	vk_volumetric_stage_barrier( vk.froxel_volume_image );
	vk_volumetric_stage_barrier( vk.froxel_light_image );
	vk_volumetric_stage_barrier( vk.froxel_extinction_image );
	vk_write_volumetric_timestamp( VK_VOLUMETRY_QUERY_AFTER_CLEAR, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	vk_set_volumetric_pass_params( (float)VK_VOLUMETRIC_STAGE_GLOBAL_DENSITY, 0.0f, 0.0f, 0.0f );
	qvkCmdDispatch( vk.cmd->command_buffer, groups_x, groups_y, groups_z );
	vk_volumetric_stage_barrier( vk.froxel_light_image );
	vk_volumetric_stage_barrier( vk.froxel_extinction_image );
	vk_write_volumetric_timestamp( VK_VOLUMETRY_QUERY_AFTER_GLOBAL_DENSITY, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	vk_set_volumetric_pass_params( (float)VK_VOLUMETRIC_STAGE_VOLUME_DENSITY, 0.0f, 0.0f, 0.0f );
	qvkCmdDispatch( vk.cmd->command_buffer, groups_x, groups_y, groups_z );
	vk_volumetric_stage_barrier( vk.froxel_light_image );
	vk_volumetric_stage_barrier( vk.froxel_extinction_image );
	vk_write_volumetric_timestamp( VK_VOLUMETRY_QUERY_AFTER_VOLUME_DENSITY, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	vk_set_volumetric_pass_params( (float)VK_VOLUMETRIC_STAGE_FLUID_DENSITY, 0.0f, 0.0f, 0.0f );
	qvkCmdDispatch( vk.cmd->command_buffer, groups_x, groups_y, groups_z );
	vk_volumetric_stage_barrier( vk.froxel_extinction_image );
	vk_volumetric_stage_barrier( vk.froxel_light_image );
	vk_write_volumetric_timestamp( VK_VOLUMETRY_QUERY_AFTER_FLUID_DENSITY, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	vk_set_volumetric_pass_params( (float)VK_VOLUMETRIC_STAGE_SUN_LIGHT, 0.0f, 0.0f, 0.0f );
	qvkCmdDispatch( vk.cmd->command_buffer, groups_x, groups_y, groups_z );
	vk_volumetric_stage_barrier( vk.froxel_volume_image );
	vk_volumetric_stage_barrier( vk.froxel_extinction_image );
	vk_write_volumetric_timestamp( VK_VOLUMETRY_QUERY_AFTER_SUN, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	for ( int i = 0; i < local_light_count; i++ ) {
		const dlight_t *dl = vk_get_volumetric_local_light( i );
		qboolean shadow_ready = qfalse;

		if ( use_local_shadows && params_rw && dl ) {
			if ( dl->linear ) {
				viewParms_t shadowView;
				float shadowViewProj[16];
				const uint32_t atlas_size = MAX( vk.local_spot_shadow_atlas_size, 1u );
				const uint32_t tile_size = MAX( vk.local_spot_shadow_tile_size, 1u );
				const uint32_t grid = MAX( 1u, atlas_size / tile_size );

				if ( spot_shadow_slot < (int)vk.local_spot_shadow_capacity ) {
					const int slot = spot_shadow_slot++;
					const int tile_x = ( slot % (int)grid ) * (int)tile_size;
					const int tile_y = ( slot / (int)grid ) * (int)tile_size;

					if ( vk_build_local_spot_shadow_view( dl, tile_x, tile_y, (int)tile_size, &shadowView, shadowViewProj ) &&
						vk_render_local_volumetric_shadow_view( &shadowView, qfalse, -1 ) )
					{
						Com_Memcpy( params_rw->localSpotShadowMatrix[i], shadowViewProj, sizeof( shadowViewProj ) );
						params_rw->localShadowAtlasUv[i][0] = (float)tile_size / (float)atlas_size;
						params_rw->localShadowAtlasUv[i][1] = (float)tile_size / (float)atlas_size;
						params_rw->localShadowAtlasUv[i][2] = (float)tile_x / (float)atlas_size;
						params_rw->localShadowAtlasUv[i][3] = (float)tile_y / (float)atlas_size;
						params_rw->lightExtra[i][3] = (float)slot;
						shadow_ready = qtrue;
						spot_shadow_ready_count++;
					}
				}
			} else {
				if ( point_shadow_slot < (int)vk.local_point_shadow_capacity ) {
					const int point_slot = point_shadow_slot++;
					qboolean all_faces_rendered = qtrue;

					for ( int face = 0; face < 6; face++ ) {
						viewParms_t shadowView;
						float shadowViewProj[16];
						const int layer = point_slot * 6 + face;

						if ( !vk_build_local_point_shadow_view( dl, face, (int)vk.local_point_shadow_face_size, &shadowView, shadowViewProj ) ||
							!vk_render_local_volumetric_shadow_view( &shadowView, qtrue, layer ) )
						{
							all_faces_rendered = qfalse;
							break;
						}

						Com_Memcpy( params_rw->localPointShadowMatrix[i][face], shadowViewProj, sizeof( shadowViewProj ) );
					}

					if ( all_faces_rendered ) {
						params_rw->lightExtra[i][3] = (float)point_slot;
						shadow_ready = qtrue;
						point_shadow_ready_count++;
					}
				}
			}
		}

		// passParams.y carries local light index for the STAGE_LOCAL_LIGHT dispatch.
		vk_set_volumetric_pass_params( (float)VK_VOLUMETRIC_STAGE_LOCAL_LIGHT, (float)i, shadow_ready ? 1.0f : 0.0f, 0.0f );
		qvkCmdDispatch( vk.cmd->command_buffer, groups_x, groups_y, groups_z );
		vk_volumetric_stage_barrier( vk.froxel_volume_image );
	}
	vk_write_volumetric_timestamp( VK_VOLUMETRY_QUERY_AFTER_LOCAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	vk_volumetric_validation_state.local_shadow_ready_spot = (uint32_t)spot_shadow_ready_count;
	vk_volumetric_validation_state.local_shadow_ready_point = (uint32_t)point_shadow_ready_count;

	vk_set_volumetric_pass_params( (float)VK_VOLUMETRIC_STAGE_CLAMP_LEVEL0, 0.0f, 0.0f, 0.0f );
	qvkCmdDispatch( vk.cmd->command_buffer, clamp0_groups_x, clamp0_groups_y, clamp_groups_z );
	vk_volumetric_stage_barrier( vk.froxel_clamp_image );
	vk_write_volumetric_timestamp( VK_VOLUMETRY_QUERY_AFTER_CLAMP0, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	vk_set_volumetric_pass_params( (float)VK_VOLUMETRIC_STAGE_CLAMP_LEVEL1, 0.0f, 0.0f, 0.0f );
	qvkCmdDispatch( vk.cmd->command_buffer, clamp1_groups_x, clamp1_groups_y, clamp_groups_z );
	vk_volumetric_stage_barrier( vk.froxel_light_image );
	vk_write_volumetric_timestamp( VK_VOLUMETRY_QUERY_AFTER_CLAMP1, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	vk_set_volumetric_pass_params( (float)VK_VOLUMETRIC_STAGE_TEMPORAL, 0.0f, 0.0f, 0.0f );
	qvkCmdDispatch( vk.cmd->command_buffer, groups_x, groups_y, groups_z );
	vk_write_volumetric_timestamp( VK_VOLUMETRY_QUERY_AFTER_TEMPORAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	record_image_layout_transition( vk.cmd->command_buffer, vk.froxel_volume_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
	record_image_layout_transition( vk.cmd->command_buffer, vk.froxel_extinction_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
	record_image_layout_transition( vk.cmd->command_buffer, vk.volumetric_telemetry_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
	if ( r_fogDebug && r_fogDebug->integer >= 1 && ( vk.volumetric_frame % 120u ) == 0u ) {
		ri.Printf( PRINT_ALL, "[VK][fog] transition froxelVolume/extinction to SHADER_READ_ONLY_OPTIMAL for composite\n" );
	}
}

static void vk_copy_froxel_history( void )
{
	if ( !vk.froxel_volume_image || !vk.froxel_history_image || !vk.froxel_width || !vk.froxel_height || !vk.froxel_slices ) {
		return;
	}

	VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	VkCommandBuffer cmd = vk.cmd->command_buffer;

	VkImageCopy copy;
	Com_Memset( &copy, 0, sizeof( copy ) );
	copy.srcSubresource.aspectMask = aspect;
	copy.srcSubresource.mipLevel = 0;
	copy.srcSubresource.baseArrayLayer = 0;
	copy.srcSubresource.layerCount = 1;
	copy.dstSubresource = copy.srcSubresource;
	copy.extent.width = vk.froxel_width;
	copy.extent.height = vk.froxel_height;
	copy.extent.depth = vk.froxel_slices;

	record_image_layout_transition( cmd, vk.froxel_volume_image, aspect,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 0 );
	record_image_layout_transition( cmd, vk.froxel_history_image, aspect,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );

	qvkCmdCopyImage( cmd,
		vk.froxel_volume_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		vk.froxel_history_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &copy );

	record_image_layout_transition( cmd, vk.froxel_volume_image, aspect,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
	record_image_layout_transition( cmd, vk.froxel_history_image, aspect,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0 );
	if ( r_fogDebug && r_fogDebug->integer >= 1 && ( vk.volumetric_frame % 120u ) == 0u ) {
		ri.Printf( PRINT_ALL,
			"[VK][fog] history copy vol=0x%llx READ_ONLY->TRANSFER_SRC->READ_ONLY hist=0x%llx GENERAL->TRANSFER_DST->GENERAL\n",
			(unsigned long long)(uintptr_t)vk.froxel_volume_image,
			(unsigned long long)(uintptr_t)vk.froxel_history_image );
	}

	vk.has_prev_volumetric = qtrue;
}

static void vk_volumetric_composite_pass( void )
{
	if ( vk.volumetric_composite_pipeline == VK_NULL_HANDLE || vk.render_pass.volumetric == VK_NULL_HANDLE ) {
		return;
	}

	vk_begin_volumetric_render_pass();

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.volumetric_composite_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.volumetric_composite_pipeline_layout, 0, 1, &vk.volumetric_composite_descriptor, 0, NULL );

	VkViewport viewport;
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = vk.renderWidth > 0 ? (float) vk.renderWidth : (float)glConfig.vidWidth;
	viewport.height = vk.renderHeight > 0 ? (float) vk.renderHeight : (float)glConfig.vidHeight;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor;
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	uint32_t scissorWidth = ( viewport.width > 0.0f ) ? (uint32_t) viewport.width : (uint32_t) glConfig.vidWidth;
	uint32_t scissorHeight = ( viewport.height > 0.0f ) ? (uint32_t) viewport.height : (uint32_t) glConfig.vidHeight;
	scissor.extent.width = scissorWidth;
	scissor.extent.height = scissorHeight;

	qvkCmdSetViewport( vk.cmd->command_buffer, 0, 1, &viewport );
	qvkCmdSetScissor( vk.cmd->command_buffer, 0, 1, &scissor );

	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
	vk_end_render_pass();
}

typedef struct {
	float threshold;
	float localContrast;
	int maxSearchSteps;
	float corner_rounding;
} SMAAPushConstants_t;

static void vk_run_smaa_pass( VkPipeline pipeline, VkRenderPass pass, VkFramebuffer framebuffer, VkDescriptorSet color_descriptor, VkDescriptorSet aux_descriptor, uint32_t width, uint32_t height )
{
	if ( !pipeline || pass == VK_NULL_HANDLE || framebuffer == VK_NULL_HANDLE || vk.pipeline_layout_smaa == VK_NULL_HANDLE || color_descriptor == VK_NULL_HANDLE ) {
		return;
	}

	vk_begin_render_pass_tracked( pass, framebuffer, qfalse, width, height );

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );

	{
		SMAAPushConstants_t pc;
		int preset = ( r_smaa_preset && r_smaa_preset->integer >= 1 && r_smaa_preset->integer <= 4 ) ? r_smaa_preset->integer : 0;
		if ( preset ) {
			/* Quality presets: threshold, localContrast, maxSearchSteps */
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

	VkDescriptorSet descriptor_sets[2];
	descriptor_sets[0] = color_descriptor;
	descriptor_sets[1] = ( aux_descriptor != VK_NULL_HANDLE ) ? aux_descriptor : color_descriptor;

	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_smaa, 0, 2, descriptor_sets, 0, NULL );

	VkViewport viewport;
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)width;
	viewport.height = (float)height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor;
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = width;
	scissor.extent.height = height;

	qvkCmdSetViewport( vk.cmd->command_buffer, 0, 1, &viewport );
	qvkCmdSetScissor( vk.cmd->command_buffer, 0, 1, &scissor );

	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );

	vk_end_render_pass();
}

static void vk_smaa_passes( void )
{
	uint32_t w, h;

	if ( !vk.smaaActive ) {
		return;
	}
	if ( vk.color_image_view == VK_NULL_HANDLE || vk.smaa_output_image_view == VK_NULL_HANDLE ) {
		return;
	}
	w = ( glConfig.vidWidth > 0 ) ? (uint32_t)glConfig.vidWidth : 1u;
	h = ( glConfig.vidHeight > 0 ) ? (uint32_t)glConfig.vidHeight : 1u;

	/* Edge: set0=scene(color_image). Blend: set0=scene(texelSize), set1=edges. Compose: set0=scene, set1=blend. */
	vk_run_smaa_pass( vk.smaa_edge_pipeline, vk.render_pass.smaa_edge, vk.framebuffers.smaa_edge, vk.smaa_edge_descriptor, vk.smaa_edge_descriptor, w, h );
	vk_run_smaa_pass( vk.smaa_blend_pipeline, vk.render_pass.smaa_blend, vk.framebuffers.smaa_blend, vk.smaa_edge_descriptor, vk.smaa_blend_descriptor, w, h );
	vk_run_smaa_pass( vk.smaa_compose_pipeline, vk.render_pass.smaa_compose, vk.framebuffers.smaa_compose, vk.smaa_edge_descriptor, vk.smaa_compose_descriptor, w, h );
}

void vk_reset_volumetric_history( void )
{
	vk.has_prev_volumetric = qfalse;
	vk.volumetric_frame = 0;
	vk_prev_matrices_valid = qfalse;
	vk_prev_volumetric_time_valid = qfalse;
	vk_volumetric_noise_time = 0.0f;
	vk_near_static_view_frames = 0;
	Com_Memset( &vk_volumetric_validation_state, 0, sizeof( vk_volumetric_validation_state ) );
}

void vk_volumetric_fog_pass( void )
{
	/* Atmosphere: only when we have a 3D world. Skip for menus, videos, RDF_NOWORLDMODEL
	 * (depth is cleared to far; drawing sky over full screen would cover UI). */
	if ( tr.world && !( tr.refdef.rdflags & RDF_NOWORLDMODEL ) ) {
		vk_atmosphere_pass();
	}

	if ( backEnd.doneFog ) {
		return;
	}

	{
		int tier = 0;
		cvar_t *tierCvar = ri.Cvar_Get( "r_volumetricFogTier", "0", 0 );
		if ( tierCvar ) tier = tierCvar->integer;
		if ( tier >= 2 || tier == 4 || !r_volumetricFog->integer || !vk.fboActive ||
			!tr.world || ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) ) {
			vk_volumetric_skip_cleanup( "volumetric skipped (tier/off/no-world)",
				VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
			return;
		}
	}
	if ( vk.froxel_volume_image == VK_NULL_HANDLE || vk.froxel_history_image == VK_NULL_HANDLE ||
		vk.froxel_extinction_image == VK_NULL_HANDLE || vk.froxel_clamp_image == VK_NULL_HANDLE ||
		vk.fog_scene_image == VK_NULL_HANDLE || vk.motion_vector_image == VK_NULL_HANDLE )
	{
		if ( r_fogDebug && r_fogDebug->integer >= 1 ) {
			ri.Printf( PRINT_WARNING, "[VK][fog] skipping fog pass: resources missing vol=0x%llx hist=0x%llx ext=0x%llx clamp=0x%llx scene=0x%llx motion=0x%llx\n",
				(unsigned long long)(uintptr_t)vk.froxel_volume_image,
				(unsigned long long)(uintptr_t)vk.froxel_history_image,
				(unsigned long long)(uintptr_t)vk.froxel_extinction_image,
				(unsigned long long)(uintptr_t)vk.froxel_clamp_image,
				(unsigned long long)(uintptr_t)vk.fog_scene_image,
				(unsigned long long)(uintptr_t)vk.motion_vector_image );
		}
		vk_volumetric_skip_cleanup( "volumetric skipped (missing resources)",
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
		return;
	}
	if ( vk.msaaActive &&
		( vk.volumetric_depth_image == VK_NULL_HANDLE ||
		  vk.volumetric_depth_view == VK_NULL_HANDLE ||
		  vk.volumetric_depth_resolve_pipeline == VK_NULL_HANDLE ||
		  vk.volumetric_depth_resolve_descriptor == VK_NULL_HANDLE ) )
	{
		if ( r_fogDebug && r_fogDebug->integer >= 1 ) {
			ri.Printf( PRINT_WARNING, "[VK][fog] skipping fog pass: MSAA depth resolve path incomplete image=0x%llx view=0x%llx pipeline=0x%llx descriptor=0x%llx\n",
				(unsigned long long)(uintptr_t)vk.volumetric_depth_image,
				(unsigned long long)(uintptr_t)vk.volumetric_depth_view,
				(unsigned long long)(uintptr_t)vk.volumetric_depth_resolve_pipeline,
				(unsigned long long)(uintptr_t)vk.volumetric_depth_resolve_descriptor );
		}
		vk_volumetric_skip_cleanup( "volumetric skipped (MSAA depth resolve missing)",
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
		return;
	}

	VkImageAspectFlags depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	if ( glConfig.stencilBits > 0 ) {
		depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );

	vk_resolve_volumetric_depth_msaa();
	vk_update_volumetric_params();

	/* Skip volumetrics when view is nearly static (death cam) to avoid gradient/streak artifacts */
	if ( r_volumetricFogSkipStatic && r_volumetricFogSkipStatic->integer &&
		vk_near_static_view_frames >= 30 ) {
		vk_volumetric_skip_cleanup( "volumetric skipped (static view, death cam)",
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
		return;
	}

	vk_volumetric_validation_state.telemetry_nan_or_inf = 0;
	vk_volumetric_validation_state.telemetry_extinction_clamp_hits = 0;
	vk_volumetric_validation_state.telemetry_velocity_clamp_hits = 0;
	vk_volumetric_validation_state.telemetry_density_clamp_hits = 0;
	vk_volumetric_validation_state.telemetry_pressure_sanitize_hits = 0;
	vk_volumetric_validation_state.telemetry_temporal_rejects = 0;

	if ( vk.volumetric_query_pool != VK_NULL_HANDLE &&
		r_volumetricFogPerfTimers && r_volumetricFogPerfTimers->integer &&
		qvkCmdResetQueryPool )
	{
		const uint32_t query_base = vk.cmd_index * VK_VOLUMETRIC_QUERY_SLOTS;
		qvkCmdResetQueryPool( vk.cmd->command_buffer, vk.volumetric_query_pool, query_base, VK_VOLUMETRY_QUERY_USED );
	}
	vk_write_volumetric_timestamp( VK_VOLUMETRY_QUERY_FOG_START, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	if ( vk.volumetric_telemetry_image != VK_NULL_HANDLE ) {
		VkImageSubresourceRange telemetry_clear_range;
		VkClearColorValue telemetry_clear_value;

		record_image_layout_transition( vk.cmd->command_buffer, vk.volumetric_telemetry_image, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT );

		Com_Memset( &telemetry_clear_range, 0, sizeof( telemetry_clear_range ) );
		telemetry_clear_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		telemetry_clear_range.levelCount = 1;
		telemetry_clear_range.layerCount = 1;
		Com_Memset( &telemetry_clear_value, 0, sizeof( telemetry_clear_value ) );
		qvkCmdClearColorImage( vk.cmd->command_buffer, vk.volumetric_telemetry_image, VK_IMAGE_LAYOUT_GENERAL, &telemetry_clear_value, 1, &telemetry_clear_range );

		record_image_layout_transition( vk.cmd->command_buffer, vk.volumetric_telemetry_image, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	}

	record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT );
	record_image_layout_transition( vk.cmd->command_buffer, vk.fog_scene_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT );
	{
		VkImageCopy copy_region;
		Com_Memset( &copy_region, 0, sizeof( copy_region ) );
		copy_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copy_region.srcSubresource.layerCount = 1;
		copy_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copy_region.dstSubresource.layerCount = 1;
		copy_region.extent.width = glConfig.vidWidth;
		copy_region.extent.height = glConfig.vidHeight;
		copy_region.extent.depth = 1;
		qvkCmdCopyImage( vk.cmd->command_buffer,
			vk.color_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			vk.fog_scene_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &copy_region );
	}
	record_image_layout_transition( vk.cmd->command_buffer, vk.fog_scene_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
	record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT );
	if ( r_fogDebug && r_fogDebug->integer >= 1 && ( vk.volumetric_frame % 120u ) == 0u ) {
		ri.Printf( PRINT_ALL, "[VK][fog] copied scene color src=0x%llx dst=0x%llx for explicit composite\n",
			(unsigned long long)(uintptr_t)vk.color_image,
			(unsigned long long)(uintptr_t)vk.fog_scene_image );
	}

	record_image_layout_transition( vk.cmd->command_buffer, vk.froxel_volume_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	record_image_layout_transition( vk.cmd->command_buffer, vk.froxel_extinction_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	if ( r_fogDebug && r_fogDebug->integer >= 1 && ( vk.volumetric_frame % 120u ) == 0u ) {
		ri.Printf( PRINT_ALL, "[VK][fog] transition froxelVolume/extinction SHADER_READ_ONLY_OPTIMAL->GENERAL (fragment->compute)\n" );
	}

	vk_volumetric_compute_pass();

	if ( r_volumetricFogTemporalWeight->value > 0.0f ) {
		vk_copy_froxel_history();
	} else {
		vk.has_prev_volumetric = qfalse;
	}

	vk_volumetric_composite_pass();
	vk_write_volumetric_timestamp( VK_VOLUMETRY_QUERY_AFTER_COMPOSITE, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT );

	/* Render pass finalLayout=SHADER_READ_ONLY transitions color_image automatically on end. */

	if ( vk.smaaActive && tr.world && !( tr.refdef.rdflags & RDF_NOWORLDMODEL ) ) {
		vk_barrier_post_fog_source_for_sampling( vk.color_image_view, "volumetric pre-SMAA" );
		vk_smaa_passes();
		vk_set_scene_post_fog_source( vk.smaa_output_image_view ? vk.smaa_output_image_view : vk.color_image_view );
		vk_update_post_fog_descriptors( vk.smaa_output_image_view ? vk.smaa_output_image_view : vk.color_image_view );
	} else {
		vk_set_scene_post_fog_source( vk.color_image_view );
		vk_update_post_fog_descriptors( vk.color_image_view );
	}

	// Restore depth layout for the next frame's main render pass clears/attachments.
	record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT );
	vk_write_volumetric_timestamp( VK_VOLUMETRY_QUERY_FOG_END, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT );

	if ( r_volumetricFogValidation && r_volumetricFogValidation->integer > 0 ) {
		int interval = ( r_volumetricFogValidationPrintInterval ) ? r_volumetricFogValidationPrintInterval->integer : 120;
		const volumetric_params_t *params = (const volumetric_params_t *)vk.volumetric_params_ptr;
		if ( interval < 1 ) {
			interval = 1;
		}
		if ( ( vk.volumetric_frame % (uint32_t)interval ) == 0u && params ) {
			ri.Printf( PRINT_ALL,
				"[VK][fog][validate] frame=%u hasHistory=%.0f cameraCut=%.0f forcedCuts=%u totalCuts=%u localShadows spot=%u point=%u lights=%u msaa=%s depthResolve=%s motion=%s\n",
				vk.volumetric_frame,
				params->miscParams[3],
				params->temporalParams[3],
				vk_volumetric_validation_state.forced_camera_cut_events,
				vk_volumetric_validation_state.camera_cut_events,
				vk_volumetric_validation_state.local_shadow_ready_spot,
				vk_volumetric_validation_state.local_shadow_ready_point,
				vk_volumetric_validation_state.local_light_count,
				vk.msaaActive ? "on" : "off",
				( vk.msaaActive && vk.volumetric_depth_image != VK_NULL_HANDLE ) ? "on" : "off",
				( vk.motion_vector_image != VK_NULL_HANDLE ) ? "on" : "off" );
		}
	}

	backEnd.doneFog = qtrue;
}

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

/*
 * SSAO/HBAO pass. Runs before bloom so AO darkens the scene before bloom extraction.
 * Skip for menus/cinematics (RDF_NOWORLDMODEL) and when MSAA is active.
 */
qboolean vk_ssao_pass( void )
{
	static qboolean warned_msaa = qfalse;

	if ( backEnd.doneSSAO || !r_ssao || !r_ssao->integer || !vk.fboActive || !backEnd.doneSurfaces )
		return qfalse;
	/* Pass culling: skip expensive SSAO for menus, cinematics, no-world */
	if ( !tr.world || ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) )
		return qfalse;
	if ( vk.msaaActive ) {
		if ( !warned_msaa ) {
			ri.Printf( PRINT_WARNING, "Vulkan: SSAO disabled while MSAA is enabled (no depth resolve yet)\n" );
			warned_msaa = qtrue;
		}
		return qfalse;
	}

	{
		typedef struct {
			float projInfo[4];
			float params[4];
			float misc[4];
			float misc2[4];
		} vk_ssao_push_t;

		vk_ssao_push_t push;
		VkImageAspectFlags depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
#ifdef USE_REVERSED_DEPTH
		const float depthIsReversed = 1.0f;
#else
		const float depthIsReversed = 0.0f;
#endif
		if ( glConfig.stencilBits > 0 )
			depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;

		vk_end_render_pass();

		record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, 0, 0 );

		vk_begin_ssao_render_pass();
		if ( r_ssaoMethod && r_ssaoMethod->integer && vk.hbao_pipeline != VK_NULL_HANDLE ) {
			qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.hbao_pipeline );
		} else {
			qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.ssao_pipeline );
		}
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_ssao, 0, 1, &vk.depth_descriptor[vk.cmd_index], 0, NULL );

		push.projInfo[0] = ( backEnd.viewParms.projectionMatrix[0] != 0.0f ) ? 1.0f / backEnd.viewParms.projectionMatrix[0] : 1.0f;
		push.projInfo[1] = ( backEnd.viewParms.projectionMatrix[5] != 0.0f ) ? 1.0f / backEnd.viewParms.projectionMatrix[5] : 1.0f;
		push.projInfo[2] = backEnd.viewParms.projectionMatrix[10];
		push.projInfo[3] = backEnd.viewParms.projectionMatrix[14];

		if ( r_ssaoMethod && r_ssaoMethod->integer ) {
			push.params[0] = ( r_ssaoRadius->value > 0.0f ) ? r_ssaoRadius->value / 100.0f : 0.1f;
			push.params[1] = ( r_ssaoBias->value > 0.0f ) ? r_ssaoBias->value * 0.01f : 0.04f;
			push.params[2] = r_ssaoIntensity->value;
			push.params[3] = r_ssaoPower->value;
			push.misc[0] = (float)( r_hbaoDirections ? r_hbaoDirections->integer : 8 );
			push.misc[1] = (float)( r_hbaoSteps ? r_hbaoSteps->integer : 8 );
			push.misc[2] = ( glConfig.vidWidth > 0 ) ? 1.0f / (float)glConfig.vidWidth : 1.0f;
			if ( depthIsReversed > 0.5f )
				push.misc[2] = -push.misc[2];
			push.misc[3] = ( glConfig.vidHeight > 0 ) ? 1.0f / (float)glConfig.vidHeight : 1.0f;
		} else {
			push.params[0] = r_ssaoRadius->value;
			push.params[1] = r_ssaoBias->value;
			push.params[2] = r_ssaoIntensity->value;
			push.params[3] = r_ssaoPower->value;
			push.misc[0] = (float)r_ssaoSamples->integer;
			push.misc[1] = ( glConfig.vidWidth > 0 ) ? 1.0f / (float)glConfig.vidWidth : 1.0f;
			push.misc[2] = ( glConfig.vidHeight > 0 ) ? 1.0f / (float)glConfig.vidHeight : 1.0f;
			push.misc[3] = depthIsReversed;
			push.misc2[0] = (float)( r_ssaoMethod ? r_ssaoMethod->integer : 0 );
			push.misc2[1] = (float)( r_hbaoDirections ? r_hbaoDirections->integer : 8 );
			push.misc2[2] = (float)( r_hbaoSteps ? r_hbaoSteps->integer : 4 );
			push.misc2[3] = ( r_ssaoMaxDepthGradient && r_ssaoMaxDepthGradient->value > 0.0f ) ? r_ssaoMaxDepthGradient->value : 0.0f;
		}

		qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout_ssao, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( push ), &push );
		vk_set_fullscreen_viewport_scissor( vk.renderWidth, vk.renderHeight );
		qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
		vk_end_render_pass();

		vk_begin_ssao_blur_render_pass();
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.ssao_blur_pipeline );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_ssao, 0, 1, &vk.ssao_descriptor, 0, NULL );

		push.params[0] = ( r_ssaoBlurRadius && r_ssaoBlurRadius->integer >= 0 ) ? (float)r_ssaoBlurRadius->integer : 2.0f;
		push.params[1] = push.params[2] = push.params[3] = 0.0f;
		qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout_ssao, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( push ), &push );
		vk_set_fullscreen_viewport_scissor( vk.renderWidth, vk.renderHeight );
		qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
		vk_end_render_pass();

		if ( vk.fog_scene_image != VK_NULL_HANDLE && vk.color_image != VK_NULL_HANDLE ) {
			VkImageCopy copy_region;
			uint32_t copy_w = ( glConfig.vidWidth > 0 ) ? (uint32_t)glConfig.vidWidth : 1u;
			uint32_t copy_h = ( glConfig.vidHeight > 0 ) ? (uint32_t)glConfig.vidHeight : 1u;
			Com_Memset( &copy_region, 0, sizeof( copy_region ) );
			copy_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copy_region.srcSubresource.layerCount = 1;
			copy_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copy_region.dstSubresource.layerCount = 1;
			copy_region.extent.width = copy_w;
			copy_region.extent.height = copy_h;
			copy_region.extent.depth = 1;
			record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT );
			record_image_layout_transition( vk.cmd->command_buffer, vk.fog_scene_image, VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT );
			qvkCmdCopyImage( vk.cmd->command_buffer,
				vk.color_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				vk.fog_scene_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &copy_region );
			record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
			record_image_layout_transition( vk.cmd->command_buffer, vk.fog_scene_image, VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
		}
		if ( vk.fog_scene_image_view != VK_NULL_HANDLE && vk.ssao_scene_descriptor != VK_NULL_HANDLE && vk.ssao_blur_descriptor != VK_NULL_HANDLE ) {
			vk_begin_ssao_combine_render_pass();
			if ( r_ssaoDebugView && r_ssaoDebugView->integer == 2 ) {
				qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.ssao_depth_debug_pipeline );
				qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.depth_descriptor[vk.cmd_index], 0, NULL );
			} else {
				qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
					( r_ssaoDebugView && r_ssaoDebugView->integer ) ? vk.ssao_debug_pipeline : vk.ssao_combine_pipeline );
				if ( r_ssaoDebugView && r_ssaoDebugView->integer ) {
					qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.ssao_blur_descriptor, 0, NULL );
				} else {
					VkDescriptorSet ssao_combine_sets[2] = { vk.ssao_scene_descriptor, vk.ssao_blur_descriptor };
					qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_ssao_combine, 0, 2, ssao_combine_sets, 0, NULL );
				}
			}
			vk_set_fullscreen_viewport_scissor( vk.renderWidth, vk.renderHeight );
			qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
			vk_end_render_pass();
		}
		record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0, 0 );
	}

	backEnd.doneSSAO = qtrue;
	return qtrue;
}


qboolean vk_bloom( void )
{
	uint32_t i;
	const qboolean canBlitDownsample = vk_format_has_features( vk.physical_device, vk.bloom_format,
		VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT );

	if ( vk.renderPassIndex == RENDER_PASS_SCREENMAP || vk.renderPassIndex == RENDER_PASS_SUN_SHADOW )
	{
		return qfalse;
	}

	if ( backEnd.doneBloom || !backEnd.doneSurfaces || !vk.fboActive )
	{
		return qfalse;
	}
	/* Pass culling: skip bloom for menus/cinematics (no 3D world) */
	if ( !tr.world || ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) )
	{
		return qfalse;
	}

	vk_end_render_pass(); // end main/post-bloom continuation
	if ( !backEnd.doneFog ) {
		vk_volumetric_fog_pass();
	}

	/* Ensure color_image is ready for sampling before bloom extract */
	vk_barrier_post_fog_source_for_sampling( vk.color_image_view, "pre-bloom-extract" );

	// bloom extraction
	vk_begin_bloom_extract_render_pass();
	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.bloom_extract_pipeline );
	{
		VkDescriptorSet bloom_sets[3] = {
			vk.color_descriptor[vk.cmd_index],
			vk.depth_descriptor[vk.cmd_index],
			vk.postfx_params_descriptor[vk.cmd_index]
		};
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 3, bloom_sets, 0, NULL );
	}
	vk_set_fullscreen_viewport_scissor( vk.renderWidth, vk.renderHeight );
	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
	vk_end_render_pass();

	if ( canBlitDownsample ) {
		// Split pipeline: downsample first, then blur at same resolution.
		for ( i = 0; i < VK_NUM_BLOOM_PASSES * 2; i += 2 ) {
			VkImageBlit region;
			const uint32_t level = i / 2;
			const uint32_t srcIndex = ( i == 0 ) ? 0 : i;
			const uint32_t dstIndex = i + 2;
			const uint32_t srcWidth = MAX( 1u, gls.captureWidth / ( 1u << level ) );
			const uint32_t srcHeight = MAX( 1u, gls.captureHeight / ( 1u << level ) );
			const uint32_t dstWidth = MAX( 1u, srcWidth / 2u );
			const uint32_t dstHeight = MAX( 1u, srcHeight / 2u );
			VkCommandBuffer cmd = vk.cmd->command_buffer;

			record_image_layout_transition( cmd, vk.bloom_image[srcIndex], VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 0 );
			record_image_layout_transition( cmd, vk.bloom_image[dstIndex], VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );

			Com_Memset( &region, 0, sizeof( region ) );
			region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.srcSubresource.layerCount = 1;
			region.srcOffsets[1].x = srcWidth;
			region.srcOffsets[1].y = srcHeight;
			region.srcOffsets[1].z = 1;
			region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.dstSubresource.layerCount = 1;
			region.dstOffsets[1].x = dstWidth;
			region.dstOffsets[1].y = dstHeight;
			region.dstOffsets[1].z = 1;

			qvkCmdBlitImage( cmd, vk.bloom_image[srcIndex], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				vk.bloom_image[dstIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_LINEAR );

			record_image_layout_transition( cmd, vk.bloom_image[srcIndex], VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
			record_image_layout_transition( cmd, vk.bloom_image[dstIndex], VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );

			// horizontal blur: downsampled source -> ping image
			vk_begin_blur_render_pass( i + 0 );
			qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.blur_pipeline[i+0] );
			qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.bloom_image_descriptor[dstIndex], 0, NULL );
			qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
			vk_end_render_pass();

			// vertical blur: ping image -> final image for this level
			vk_begin_blur_render_pass( i + 1 );
			qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.blur_pipeline[i+1] );
			qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.bloom_image_descriptor[i+1], 0, NULL );
			qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
			vk_end_render_pass();
		}
	} else {
		// Fallback to legacy downsample+blur in one pass if blit features are unavailable.
		for ( i = 0; i < VK_NUM_BLOOM_PASSES * 2; i += 2 ) {
			vk_begin_blur_render_pass( i + 0 );
			qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.blur_pipeline[i+0] );
			qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.bloom_image_descriptor[i+0], 0, NULL );
			qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
			vk_end_render_pass();

			vk_begin_blur_render_pass( i + 1 );
			qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.blur_pipeline[i+1] );
			qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.bloom_image_descriptor[i+1], 0, NULL );
			qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
			vk_end_render_pass();
		}
	}

	vk_begin_post_bloom_render_pass(); // begin post-bloom
	{
		VkDescriptorSet dset[VK_NUM_BLOOM_PASSES];

		for ( i = 0; i < VK_NUM_BLOOM_PASSES; i++ )
		{
			dset[i] = vk.bloom_image_descriptor[(i+1)*2];
		}

		// blend downscaled buffers to main fbo
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.bloom_blend_pipeline );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_blend, 0, ARRAY_LEN(dset), dset, 0, NULL );
		vk_set_fullscreen_viewport_scissor( vk.renderWidth, vk.renderHeight );
		qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
	}

	// invalidate pipeline state cache
	//vk.cmd->last_pipeline = VK_NULL_HANDLE;

	if ( vk.cmd->last_pipeline != VK_NULL_HANDLE )
	{
		// restore last pipeline
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.cmd->last_pipeline );

		vk_update_mvp( NULL );

		// force depth range and viewport/scissor updates
		vk.cmd->depth_range = DEPTH_RANGE_COUNT;

		uint32_t offsets[VK_DESC_UNIFORM_COUNT], offset_count;

		// restore clobbered descriptor sets
		for ( i = 0; i < VK_NUM_BLOOM_PASSES; i++ ) {
			if ( vk.cmd->descriptor_set.current[i] != VK_NULL_HANDLE ) {
				if ( i == VK_DESC_UNIFORM /*|| i == VK_DESC_STORAGE*/ ) {
					offset_count = 0;

					offsets[offset_count++] = vk.cmd->descriptor_set.offset[VK_DESC_UNIFORM_MAIN_BINDING];
					offsets[offset_count++] = vk.cmd->descriptor_set.offset[VK_DESC_UNIFORM_CAMERA_BINDING];
					offsets[offset_count++] = vk.cmd->descriptor_set.offset[VK_DESC_UNIFORM_IQM_SKIN_BINDING];
					offsets[offset_count++] = vk.cmd->descriptor_set.offset[VK_DESC_UNIFORM_IQM_MORPH_BINDING];

					qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout, i, 1, &vk.cmd->descriptor_set.current[i], offset_count, offsets );
				}
				else
					qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout, i, 1, &vk.cmd->descriptor_set.current[i], 0, NULL );
			}
		}
	}

	backEnd.doneBloom = qtrue;

	return qtrue;
}

enum Target { IRRADIANCE = 0, PREFILTEREDENV = 1 };

typedef struct {
	uint32_t target;

	VkFormat format;
	uint32_t size;
	uint32_t mipLevels;
	
	VkRenderPass		renderpass;
	VkPipeline			pipeline;
	VkPipelineLayout	pipeline_layout;

	struct {
		VkShaderModule	*vs_module;
		VkShaderModule	*gm_module;
		VkShaderModule	*fs_module;	
	} shaders;

	struct {
		VkImage			image;
		VkImageView		view;
		VkDeviceMemory	memory;
		VkFramebuffer	framebuffer;
	} offscreen;
} filterDef;

static filterDef prefilters[2];

static uint32_t vk_pow2_floor_u32( uint32_t v )
{
	uint32_t p = 1;
	while ( ( p << 1 ) && ( ( p << 1 ) <= v ) ) {
		p <<= 1;
	}
	return p;
}

static uint32_t vk_ibl_size_from_cvar( const cvar_t *cv, uint32_t defValue, uint32_t minValue, uint32_t maxValue )
{
	uint32_t v = defValue;
	if ( cv && cv->integer > 0 ) {
		v = (uint32_t)cv->integer;
	}
	if ( v < minValue ) v = minValue;
	if ( v > maxValue ) v = maxValue;

	// Prefer power-of-two sizes (required for full mip chains).
	v = vk_pow2_floor_u32( v );
	if ( v < minValue ) v = minValue;
	if ( v > maxValue ) v = vk_pow2_floor_u32( maxValue );
	return v;
}

static void vk_create_prefilter_renderpass( filterDef *def ) 
{
	VkAttachmentReference	color_attachment_ref;
	VkSubpassDependency		deps[2];
	VkAttachmentDescription	attachment;
	VkRenderPassCreateInfo	desc;
	VkSubpassDescription	subpass;

	// Color attachment
	Com_Memset( &attachment, 0, sizeof( attachment ) );
	attachment.format = def->format;
	attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	Com_Memset( &subpass, 0, sizeof( subpass ) );
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;

	// subpass dependencies
	Com_Memset( &deps, 0, sizeof( deps ) );

	deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	deps[0].dstSubpass = 0;
	deps[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	deps[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	deps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
	
	deps[1].srcSubpass = 0;
	deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	deps[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	deps[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	deps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	desc.attachmentCount = 1;
	desc.pAttachments = &attachment;
	desc.subpassCount = 1;
	desc.pSubpasses = &subpass;
	desc.dependencyCount = 2;
	desc.pDependencies = deps;

	VK_CHECK( qvkCreateRenderPass( vk.device, &desc, NULL, &def->renderpass ) );
}

static void vk_create_prefilter_framebuffer( filterDef *def ) {
	VkCommandBuffer			command_buffer;
	VkMemoryRequirements	memory_requirements;
	VkMemoryAllocateInfo	alloc_info;

	// create offscreen image to copy from
	{
		VkImageCreateInfo desc;

		Com_Memset( &desc, 0, sizeof( desc ) );

		desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		desc.imageType = VK_IMAGE_TYPE_2D;
		desc.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		desc.format = def->format;
		desc.extent.width = def->size;
		desc.extent.height = def->size;
		desc.extent.depth = 1;
		desc.mipLevels = 1;
		desc.arrayLayers = 6;
		desc.samples = VK_SAMPLE_COUNT_1_BIT;
		desc.tiling = VK_IMAGE_TILING_OPTIMAL;
		desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		desc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VK_CHECK( qvkCreateImage( vk.device, &desc, NULL, &def->offscreen.image ) );
	}

	qvkGetImageMemoryRequirements( vk.device, def->offscreen.image, &memory_requirements);
	
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = memory_requirements.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
			
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &def->offscreen.memory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, def->offscreen.image, def->offscreen.memory, 0 ) );

	// create image view
	{
		VkImageViewCreateInfo desc;

		Com_Memset( &desc, 0, sizeof( desc ) );

		desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		desc.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		desc.format = def->format;
		desc.flags = 0;
		desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		desc.subresourceRange.baseMipLevel = 0;
		desc.subresourceRange.levelCount = 1;
		desc.subresourceRange.baseArrayLayer = 0;
		desc.subresourceRange.layerCount = 6;
		desc.image = def->offscreen.image;
		VK_CHECK( qvkCreateImageView( vk.device, &desc, NULL, &def->offscreen.view ) );
	}

	// create framebuffer
	{
		VkFramebufferCreateInfo desc;

		Com_Memset( &desc, 0, sizeof( desc ) );

		desc.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		desc.renderPass = def->renderpass;
		desc.attachmentCount = 1;
		desc.pAttachments = &def->offscreen.view;
		desc.width = def->size;
		desc.height = def->size;
		desc.layers = 6;
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &def->offscreen.framebuffer));
	}


	command_buffer = vk_begin_command_buffer();
	record_image_layout_transition( command_buffer, def->offscreen.image, VK_IMAGE_ASPECT_COLOR_BIT, 
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
		0, 0 );

	vk_end_command_buffer( command_buffer, __func__  );
}

static void vk_create_prefilter_pipeline( filterDef *def ) 
{
	VkPipelineShaderStageCreateInfo			shader_stages[3];
	VkPipelineVertexInputStateCreateInfo	vertex_input_state = {0};
	VkPipelineInputAssemblyStateCreateInfo	input_assembly_state;
	VkPipelineViewportStateCreateInfo		viewport_state = {0};
	VkPipelineRasterizationStateCreateInfo	rasterization_state = {0};
	VkPipelineMultisampleStateCreateInfo	multisample_state = {0};
	VkPipelineDepthStencilStateCreateInfo	depth_stencil_state = {0};
	VkPipelineColorBlendAttachmentState		attachment_blend_state = {0};
	VkPipelineColorBlendStateCreateInfo		blend_state = {0};
	VkPipelineDynamicStateCreateInfo		dynamic_state;
	VkDynamicState							dynamic_state_array[3] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	uint32_t								prefilter_dyn_count = 2;
	if ( vk.colorWriteMaskDynamic ) {
		dynamic_state_array[prefilter_dyn_count++] = VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT;
	}
	VkGraphicsPipelineCreateInfo			create_info = {0};
	VkPipelineLayoutCreateInfo				pipeline_layout;
	VkPushConstantRange						push_range;

	push_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	push_range.offset = 0;

	pipeline_layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout.pNext = NULL;
	pipeline_layout.flags = 0;
	pipeline_layout.setLayoutCount = 1;
	pipeline_layout.pSetLayouts = &vk.set_layout_sampler;

	if ( def->target == PREFILTEREDENV ) {
		push_range.size = sizeof(float);
		pipeline_layout.pushConstantRangeCount = 1;
		pipeline_layout.pPushConstantRanges = &push_range;
	} else {
		pipeline_layout.pushConstantRangeCount = 0;
		pipeline_layout.pPushConstantRanges = NULL;
	}

	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pipeline_layout, NULL, &def->pipeline_layout ) );
	
	input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_state.pNext = NULL;
    input_assembly_state.flags = 0;
    input_assembly_state.primitiveRestartEnable = VK_FALSE;	
	input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
	rasterization_state.cullMode = VK_CULL_MODE_NONE;
	rasterization_state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterization_state.lineWidth = 1.0f;

	attachment_blend_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	attachment_blend_state.blendEnable = VK_FALSE;

	blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state.attachmentCount = 1;
	blend_state.pAttachments = &attachment_blend_state;

	depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil_state.depthTestEnable = VK_FALSE;
	depth_stencil_state.depthWriteEnable = VK_FALSE;
	depth_stencil_state.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depth_stencil_state.front = depth_stencil_state.back;
	depth_stencil_state.back.compareOp = VK_COMPARE_OP_ALWAYS;

	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.viewportCount = 1;
	viewport_state.scissorCount = 1;

	multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
				
	dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.pNext = NULL;
    dynamic_state.flags = 0;  
	dynamic_state.dynamicStateCount = prefilter_dyn_count;
    dynamic_state.pDynamicStates = dynamic_state_array;
	
	vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_state.vertexBindingDescriptionCount = 0;
	vertex_input_state.pVertexBindingDescriptions = NULL;
	vertex_input_state.vertexAttributeDescriptionCount = 0;
	vertex_input_state.pVertexAttributeDescriptions = NULL;

	vk_set_shader_stage_desc( shader_stages + 0, VK_SHADER_STAGE_VERTEX_BIT, *def->shaders.vs_module, "main" );
	vk_set_shader_stage_desc( shader_stages + 1, VK_SHADER_STAGE_FRAGMENT_BIT, *def->shaders.fs_module, "main" );
	vk_set_shader_stage_desc( shader_stages + 2, VK_SHADER_STAGE_GEOMETRY_BIT, *def->shaders.gm_module, "main" );

	create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	create_info.layout = def->pipeline_layout;
	create_info.renderPass = def->renderpass;
	create_info.pInputAssemblyState = &input_assembly_state;
	create_info.pVertexInputState = &vertex_input_state;
	create_info.pRasterizationState = &rasterization_state;
	create_info.pColorBlendState = &blend_state;
	create_info.pMultisampleState = &multisample_state;
	create_info.pViewportState = &viewport_state;
	create_info.pDepthStencilState = &depth_stencil_state;
	create_info.pDynamicState = &dynamic_state;
	create_info.stageCount = ARRAY_LEN(shader_stages);
	create_info.pStages = shader_stages;

	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &create_info, NULL, &def->pipeline ) );	
}

void vk_create_cubemap_prefilter( void )
{
	if ( !vk.cubemapActive )
		return;

	uint32_t	i;
	filterDef	*def;

	Com_Memset( &prefilters, 0, sizeof( prefilters ) );

	for ( i = 0; i < PREFILTEREDENV + 1; i++ ) 
	{
		def = &prefilters[i];

		def->target = i;
		def->shaders.vs_module = &vk.modules.filtercube_vs;
		def->shaders.gm_module = &vk.modules.filtercube_gm;

		switch ( def->target ) {
			case IRRADIANCE:
				def->format = VK_FORMAT_R32G32B32A32_SFLOAT;
				def->size = vk_ibl_size_from_cvar( r_pbr_iblIrradianceSize, 64, 16, (uint32_t)MIN( glConfig.maxTextureSize, 1024 ) );
				def->shaders.fs_module = &vk.modules.irradiancecube_fs;
				def->mipLevels = (uint32_t)(floor(log2(def->size))) + 1;
				break;
			case PREFILTEREDENV:
				def->format = VK_FORMAT_R16G16B16A16_SFLOAT;
				def->size = vk_ibl_size_from_cvar( r_pbr_iblPrefilterSize, 256, 32, (uint32_t)MIN( glConfig.maxTextureSize, 2048 ) );
				def->shaders.fs_module = &vk.modules.prefilterenvmap_fs;
				def->mipLevels = (uint32_t)(floor(log2(def->size))) + 1;
				break;
		};

		vk_create_prefilter_renderpass( def );
		vk_create_prefilter_framebuffer( def );
		vk_create_prefilter_pipeline( def );
	}
}

void vk_destroy_cubemap_prefilter( void ){

	uint32_t	i;
	filterDef	*def;

	for ( i = 0; i < PREFILTEREDENV + 1; i++ ) 
	{
		def = &prefilters[i];

		qvkDestroyRenderPass( vk.device, def->renderpass, NULL );
		qvkDestroyFramebuffer( vk.device, def->offscreen.framebuffer, NULL );
		qvkFreeMemory( vk.device, def->offscreen.memory, NULL );
		qvkDestroyImageView( vk.device, def->offscreen.view, NULL );
		qvkDestroyImage( vk.device, def->offscreen.image, NULL );
		def->offscreen.image = VK_NULL_HANDLE;
		def->offscreen.view = VK_NULL_HANDLE;
		qvkDestroyPipeline( vk.device, def->pipeline, NULL );
		qvkDestroyPipelineLayout( vk.device, def->pipeline_layout, NULL );
	}

	Com_Memset( &prefilters, 0, sizeof( prefilters ) );
}

void vk_clear_cube_color( image_t *image, VkClearColorValue color ) 
{
	VkCommandBuffer			command_buffer;
	VkImageSubresourceRange desc;

	desc.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
	desc.baseMipLevel   = 0;
	desc.levelCount     = VK_REMAINING_MIP_LEVELS; //6
	desc.baseArrayLayer = 0;
	desc.layerCount     = VK_REMAINING_ARRAY_LAYERS; //image->layers;

	command_buffer = vk_begin_command_buffer();

	record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, 
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );

	qvkCmdClearColorImage( command_buffer, image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &desc );	
		
	record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, 
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		0, 0 );

	vk_end_command_buffer( command_buffer, __func__ );
}

static void vk_copy_to_cubemap( filterDef *def, VkImage *image, uint32_t mipLevel, uint32_t size, VkCommandBuffer command_buffer ) 
{	
	VkImageCopy region;
	
	// change image layout for all offsceen faces to transfer source
	record_image_layout_transition( command_buffer, def->offscreen.image, VK_IMAGE_ASPECT_COLOR_BIT, 
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 
		0, 0);

	Com_Memset( &region, 0, sizeof( VkImageCopy ) );
	region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.srcSubresource.baseArrayLayer = 0;
	region.srcSubresource.mipLevel = 0;
	region.srcSubresource.layerCount = 6;
	region.srcOffset.x = 0;
	region.srcOffset.y = 0;
	region.srcOffset.z = 0;



	region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.dstSubresource.baseArrayLayer = 0;
	region.dstSubresource.mipLevel = mipLevel;
	region.dstSubresource.layerCount = 6;
	region.dstOffset.x = 0;
	region.dstOffset.y = 0;
	region.dstOffset.z = 0;

	region.extent.width = region.extent.height = size;
	region.extent.depth = 1;

	qvkCmdCopyImage( command_buffer, def->offscreen.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );

	record_image_layout_transition( command_buffer, def->offscreen.image, VK_IMAGE_ASPECT_COLOR_BIT, 
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
		0, 0 );
}

static void vk_create_readback_buffer( VkDeviceSize size, VkBuffer *buffer, VkDeviceMemory *memory, void **data ) {
	VkBufferCreateInfo buffer_desc = { 0 };
	buffer_desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_desc.size = size;
	buffer_desc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	buffer_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK( qvkCreateBuffer( vk.device, &buffer_desc, NULL, buffer ) );

	VkMemoryRequirements mem_reqs;
	qvkGetBufferMemoryRequirements( vk.device, *buffer, &mem_reqs );

	VkMemoryAllocateInfo alloc_info = { 0 };
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = mem_reqs.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, memory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, *buffer, *memory, 0 ) );

	VK_CHECK( qvkMapMemory( vk.device, *memory, 0, size, 0, data ) );
}

static void vk_destroy_readback_buffer( VkBuffer buffer, VkDeviceMemory memory ) {
	qvkUnmapMemory( vk.device, memory );
	qvkDestroyBuffer( vk.device, buffer, NULL );
	qvkFreeMemory( vk.device, memory, NULL );
}

#define SH_C0 0.28209479177387814347f // 1/2*sqrt(1/pi)
#define SH_C1 0.48860251190291992159f // sqrt(3/(4*pi))
#define SH_C2 1.09254843059207907054f // 1/2*sqrt(15/pi)
#define SH_C3 0.31539156525252000603f // 1/4*sqrt(5/pi)
#define SH_C4 0.54627421529603953527f // 1/4*sqrt(15/pi)

static float SH_Basis( int index, const vec3_t dir ) {
	float x = dir[0];
	float y = dir[1];
	float z = dir[2];

	switch ( index ) {
		case 0: return SH_C0;
		case 1: return SH_C1 * y;
		case 2: return SH_C1 * z;
		case 3: return SH_C1 * x;
		case 4: return SH_C2 * x * y;
		case 5: return SH_C2 * y * z;
		case 6: return SH_C3 * ( 3.0f * z * z - 1.0f );
		case 7: return SH_C2 * x * z;
		case 8: return SH_C4 * ( x * x - y * y );
		default: return 0.0f;
	}
}

static void get_cube_dir( int face, float x, float y, vec3_t dir ) {
	switch ( face ) {
		case 0: dir[0] =  1.0f; dir[1] = -y;    dir[2] = -x;    break; // +X
		case 1: dir[0] = -1.0f; dir[1] = -y;    dir[2] =  x;    break; // -X
		case 2: dir[0] =  x;    dir[1] =  1.0f; dir[2] =  y;    break; // +Y
		case 3: dir[0] =  x;    dir[1] = -1.0f; dir[2] = -y;    break; // -Y
		case 4: dir[0] =  x;    dir[1] = -y;    dir[2] =  1.0f; break; // +Z
		case 5: dir[0] = -x;    dir[1] = -y;    dir[2] = -1.0f; break; // -Z
		default: VectorClear( dir ); break;
	}
}

static qboolean vk_extract_sh_coeffs( const image_t *irradiance_image, vec4_t shCoeffs[9] )
{
	int i;
	uint32_t f, x, y;
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;
	void *data;
	float *pixels;
	VkCommandBuffer command_buffer;

	if ( !irradiance_image || !shCoeffs )
	{
		return qfalse;
	}

	if ( irradiance_image->internalFormat != VK_FORMAT_R32G32B32A32_SFLOAT ) {
		ri.Printf( PRINT_WARNING, "vk_extract_sh_coeffs: unsupported irradiance format %s\n",
			vk_format_string( (VkFormat)irradiance_image->internalFormat ) );
		return qfalse;
	}

	uint32_t size = irradiance_image->width;
	if ( size == 0 ) {
		ri.Printf( PRINT_WARNING, "vk_extract_sh_coeffs: irradiance image has invalid size 0\n" );
		return qfalse;
	}
	uint32_t bufferSize = size * size * 6 * 4 * sizeof( float );

	for ( i = 0; i < 9; i++ )
	{
		VectorClear( shCoeffs[i] );
		shCoeffs[i][3] = 0.0f;
	}

	// Create staging buffer for readback
	vk_create_readback_buffer( bufferSize, &stagingBuffer, &stagingMemory, &data );

	command_buffer = vk_begin_command_buffer();

	// Transition image to transfer src
	record_image_layout_transition( command_buffer, irradiance_image->handle, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 0 );

	// Copy image to buffer
	VkBufferImageCopy region = { 0 };
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 6;
	region.imageExtent.width = size;
	region.imageExtent.height = size;
	region.imageExtent.depth = 1;

	qvkCmdCopyImageToBuffer( command_buffer, irradiance_image->handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer, 1, &region );

	// Transition image back to shader read only
	record_image_layout_transition( command_buffer, irradiance_image->handle, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );

	vk_end_command_buffer( command_buffer, "sh extraction" );

	pixels = (float *)data;

	float totalWeight = 0.0f;
	for ( f = 0; f < 6; f++ )
	{
		for ( y = 0; y < size; y++ )
		{
			for ( x = 0; x < size; x++ )
			{
				float u = ( (float)x + 0.5f ) / (float)size * 2.0f - 1.0f;
				float v = ( (float)y + 0.5f ) / (float)size * 2.0f - 1.0f;
				float weight = 4.0f / powf( 1.0f + u * u + v * v, 1.5f );
				
				vec3_t dir;
				get_cube_dir( (int)f, u, v, dir );
				VectorNormalize( dir );
				
				const size_t pixel_index =
					( (size_t)f * (size_t)size * (size_t)size ) +
					( (size_t)y * (size_t)size ) +
					(size_t)x;
				float *pixel = &pixels[ pixel_index * 4 ];
				vec3_t color = { pixel[0], pixel[1], pixel[2] };
				
				for ( i = 0; i < 9; i++ )
				{
					float basis = SH_Basis( i, dir );
					shCoeffs[i][0] += color[0] * basis * weight;
					shCoeffs[i][1] += color[1] * basis * weight;
					shCoeffs[i][2] += color[2] * basis * weight;
				}
				totalWeight += weight;
			}
		}
	}

	// Normalize
	if ( totalWeight <= 0.0f ) {
		vk_destroy_readback_buffer( stagingBuffer, stagingMemory );
		return qfalse;
	}

	float norm = ( 4.0f * M_PI ) / totalWeight;
	for ( i = 0; i < 9; i++ )
	{
		shCoeffs[i][0] *= norm;
		shCoeffs[i][1] *= norm;
		shCoeffs[i][2] *= norm;
	}

	vk_destroy_readback_buffer( stagingBuffer, stagingMemory );
	return qtrue;
}

void vk_generate_cubemaps( cubemap_t *cube ) 
{
	VkRenderPassBeginInfo	begin_info = {0};
	VkViewport				viewport;
	VkRect2D				scissor_rect;
	VkClearValue			clear_values[1];
	VkCommandBuffer			command_buffer;

	image_t		*cubemap = NULL;
	uint32_t	i, j;
	filterDef	*def;

	if ( !cube ) {
		ri.Printf( PRINT_WARNING, "vk_generate_cubemaps: called with NULL cubemap\n" );
		return;
	}

	vk_end_render_pass();

	command_buffer = vk_begin_command_buffer();
	record_image_layout_transition( command_buffer, vk.cubeMap.color_image, VK_IMAGE_ASPECT_COLOR_BIT, 
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
		0, 0 );
	vk_end_command_buffer( command_buffer, __func__  );

	for ( i = 0; i < PREFILTEREDENV + 1; i++ ) 
	{
		def = &prefilters[i];

		switch ( def->target ) {
			case IRRADIANCE: cubemap = cube->irradiance_image; break;
			case PREFILTEREDENV: cubemap = cube->prefiltered_image; break;
			default: cubemap = NULL; break;
		};
		if ( !cubemap ) {
			ri.Printf( PRINT_WARNING, "vk_generate_cubemaps: missing cubemap image for target %d (%s)\n",
				def->target, cube->name[0] ? cube->name : "<unnamed>" );
			continue;
		}

		Com_Memset( clear_values, 0, sizeof( clear_values ) );
		clear_values[0].color.float32[0] = 0.75f;
		clear_values[0].color.float32[1] = 0.75f;
		clear_values[0].color.float32[2] = 0.75f;
		clear_values[0].color.float32[3] = 0.0f;

		begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		begin_info.renderPass = def->renderpass;
		begin_info.framebuffer = def->offscreen.framebuffer;
		begin_info.renderArea.extent.width = def->size;
		begin_info.renderArea.extent.height = def->size;
		begin_info.clearValueCount = 1;
		begin_info.pClearValues = clear_values;

		Com_Memset( &viewport, 0, sizeof( viewport ) );
		viewport.width = viewport.height = (float)def->size;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		Com_Memset( &scissor_rect, 0, sizeof( scissor_rect ) );
		scissor_rect.extent.width = scissor_rect.extent.height = def->size;

		// change image layout for all cubemap faces to transfer destination
		record_image_layout_transition( vk.cmd->command_buffer, cubemap->handle, VK_IMAGE_ASPECT_COLOR_BIT, 
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
			0, 0 );
			
		for ( j = 0; j < def->mipLevels; j++ ) {
			qvkCmdSetViewport( vk.cmd->command_buffer, 0, 1, &viewport );
			qvkCmdSetScissor( vk.cmd->command_buffer, 0, 1, &scissor_rect );

			// render scene from cube face's point of view
			qvkCmdBeginRenderPass(vk.cmd->command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);

			if ( def->target == PREFILTEREDENV ) {
				float roughness = (float)j / (float)(def->mipLevels - 1);
				qvkCmdPushConstants( vk.cmd->command_buffer, def->pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(roughness), &roughness );
			}

			qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, def->pipeline );
			qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, def->pipeline_layout, 0, 1, &vk.cubeMap.color_descriptor, 0, NULL );
			qvkCmdDraw( vk.cmd->command_buffer, 3, 1, 0, 0 );
			qvkCmdEndRenderPass( vk.cmd->command_buffer );

			vk_copy_to_cubemap( def, &cubemap->handle, j, (uint32_t)viewport.width, vk.cmd->command_buffer );
		
			viewport.width /= 2;
			viewport.height /= 2;
		}

		record_image_layout_transition( vk.cmd->command_buffer, cubemap->handle, VK_IMAGE_ASPECT_COLOR_BIT, 
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
	}

#ifdef USE_VK_PBR
	if ( r_pbr_shExtract && r_pbr_shExtract->integer && vk.pbrActive && cube && cube->irradiance_image ) {
		R_ResetCubemapSH( cube );
		if ( vk_extract_sh_coeffs( cube->irradiance_image, cube->shCoeffs ) ) {
			cube->hasSHCoeffs = qtrue;
			ri.Printf( PRINT_DEVELOPER, "PBR: extracted SH coeffs for cubemap '%s'\n", cube->name );
		}
	}
#endif

	command_buffer = vk_begin_command_buffer();
	record_image_layout_transition( command_buffer, vk.cubeMap.color_image, VK_IMAGE_ASPECT_COLOR_BIT, 
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
		0, 0 );
	vk_end_command_buffer( command_buffer, __func__  );

	vk_begin_main_render_pass();
}
