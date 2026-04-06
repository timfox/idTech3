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
#include <math.h>

/* VK_EXT_extended_dynamic_state3: struct for pipeline creation (VK_DYNAMIC_STATE_*, PFN in vk.h) */
#ifndef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT 1000484000
#endif

/* r_hdr 3: 64-bit (RGBA64F) uses dvec4 fragment output; select HDR64 shaders when active */
static inline qboolean vk_hdr64_active( void )
{
	return vk.color_format == VK_FORMAT_R64G64B64A64_SFLOAT;
}

/* GPU occlusion culling: visibility from previous frame (1=visible, 0=occluded) */
uint64_t vk_entity_occlusion_visibility[MAX_REFENTITIES];
static uint32_t vk_occlusion_last_entity_count;

#include "vk_fluidsim.h"
#include "vk_terrain.h"
#include <stddef.h>

#if defined( _DEBUG )
#define USE_VK_VALIDATION
#if defined( _WIN32 )
#include <windows.h> /* for win32 debug callback */
#endif
#endif


struct Vk_Pipeline_FragSpecData {
	int32_t alpha_test_func;
	float   alpha_test_value;
	float   depth_fragment;
	int32_t alpha_to_coverage;
	int32_t color_mode;
	int32_t abs_light;
	int32_t tex_mode;
	int32_t discard_mode;
	float   identity_color;
	float	identity_alpha;
	int32_t	acff;
#ifdef USE_VK_PBR
	float   specularScale_x;	// use ubo for this
	float   specularScale_y;
	float   specularScale_z;
	float   specularScale_w;
	float   normalScale_x;
	float   normalScale_y;
	float   normalScale_z;
	float   normalScale_w;		// ..
	/* Order must match gen_frag.tmpl specialization constant_id 19..35 (SPIR-V layout). */
	int32_t normal_texture_set;
	int32_t physical_texture_set;
	int32_t env_texture_set;
	int32_t lightmap_texture_set;
	int32_t irradiance_texture_set;
	int32_t emissive_texture_set;
	int32_t clearcoat_texture_set;
	int32_t sheen_texture_set;
	int32_t anisotropy_texture_set;
	int32_t transmission_texture_set;
	int32_t subsurface_texture_set;
	int32_t deluxe_mapping;
	float deluxe_specular_scale;
	float lightmap_scale;
	int32_t lightmap_srgb_decode;  /* 1: sRGB->linear when BSP lightmaps gamma-encoded */
	int32_t detail_texture_set;
	float   detail_scale;
#endif
};

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

#define VEGWIND_MAX_VERTS 16384
#define VEGWIND_VERTEX_STRIDE 32  /* positionFlex + normalPhase */

static int vkSamples = VK_SAMPLE_COUNT_1_BIT;
static int vkMaxSamples = VK_SAMPLE_COUNT_1_BIT;

static float vk_get_msaa_min_sample_shading( void )
{
	if ( !vk.msaaSampleShading ) {
		return 1.0f;
	}

	return Com_Clamp( 0.25f, 1.0f,
		r_msaa_sample_shading_rate ? r_msaa_sample_shading_rate->value : 0.5f );
}

/* vk_instance, vk_surface, vk_debug_callback defined in vk_instance.c */

//
// Vulkan API function pointers (defined here, assigned by vk_instance.c init)
//
PFN_vkCreateInstance								qvkCreateInstance;
PFN_vkEnumerateInstanceExtensionProperties		qvkEnumerateInstanceExtensionProperties;

PFN_vkCreateDevice								qvkCreateDevice;
PFN_vkDestroyInstance							qvkDestroyInstance;
PFN_vkEnumerateDeviceExtensionProperties			qvkEnumerateDeviceExtensionProperties;
PFN_vkEnumeratePhysicalDevices					qvkEnumeratePhysicalDevices;
PFN_vkGetDeviceProcAddr							qvkGetDeviceProcAddr;
PFN_vkGetPhysicalDeviceFeatures					qvkGetPhysicalDeviceFeatures;
PFN_vkGetPhysicalDeviceFormatProperties			qvkGetPhysicalDeviceFormatProperties;
PFN_vkGetPhysicalDeviceMemoryProperties			qvkGetPhysicalDeviceMemoryProperties;
PFN_vkGetPhysicalDeviceProperties				qvkGetPhysicalDeviceProperties;
PFN_vkGetPhysicalDeviceQueueFamilyProperties		qvkGetPhysicalDeviceQueueFamilyProperties;
PFN_vkDestroySurfaceKHR							qvkDestroySurfaceKHR;
PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR	qvkGetPhysicalDeviceSurfaceCapabilitiesKHR;
PFN_vkGetPhysicalDeviceSurfaceFormatsKHR			qvkGetPhysicalDeviceSurfaceFormatsKHR;
PFN_vkGetPhysicalDeviceSurfacePresentModesKHR	qvkGetPhysicalDeviceSurfacePresentModesKHR;
PFN_vkGetPhysicalDeviceSurfaceSupportKHR			qvkGetPhysicalDeviceSurfaceSupportKHR;
#ifdef USE_VK_VALIDATION
PFN_vkCreateDebugReportCallbackEXT				qvkCreateDebugReportCallbackEXT;
PFN_vkDestroyDebugReportCallbackEXT				qvkDestroyDebugReportCallbackEXT;
#endif
PFN_vkAllocateCommandBuffers						qvkAllocateCommandBuffers;
PFN_vkAllocateDescriptorSets						qvkAllocateDescriptorSets;
PFN_vkAllocateMemory								qvkAllocateMemory;
PFN_vkBeginCommandBuffer							qvkBeginCommandBuffer;
PFN_vkBindBufferMemory							qvkBindBufferMemory;
PFN_vkBindImageMemory							qvkBindImageMemory;
PFN_vkCmdBeginRenderPass								qvkCmdBeginRenderPass;
PFN_vkCmdBindDescriptorSets						qvkCmdBindDescriptorSets;
PFN_vkCmdBindIndexBuffer							qvkCmdBindIndexBuffer;
PFN_vkCmdBindPipeline							qvkCmdBindPipeline;
PFN_vkCmdBindVertexBuffers						qvkCmdBindVertexBuffers;
PFN_vkCmdBlitImage								qvkCmdBlitImage;
PFN_vkCmdClearAttachments						qvkCmdClearAttachments;
PFN_vkCmdCopyBuffer								qvkCmdCopyBuffer;
PFN_vkCmdCopyBufferToImage						qvkCmdCopyBufferToImage;
PFN_vkCmdCopyImage								qvkCmdCopyImage;
PFN_vkCmdCopyImageToBuffer						qvkCmdCopyImageToBuffer;
PFN_vkCmdDraw									qvkCmdDraw;
PFN_vkCmdDrawIndexed								qvkCmdDrawIndexed;
PFN_vkCmdDispatch								qvkCmdDispatch;
PFN_vkCmdEndRenderPass									qvkCmdEndRenderPass;
PFN_vkCmdNextSubpass								qvkCmdNextSubpass;
PFN_vkCmdPipelineBarrier									qvkCmdPipelineBarrier;
PFN_vkCmdPushConstants									qvkCmdPushConstants;
PFN_vkCmdSetDepthBias							qvkCmdSetDepthBias;
PFN_vkCmdSetScissor										qvkCmdSetScissor;
PFN_vkCmdSetViewport										qvkCmdSetViewport;
PFN_vkCmdSetColorWriteMaskEXT						qvkCmdSetColorWriteMaskEXT;
PFN_vkCmdWriteTimestamp							qvkCmdWriteTimestamp;
PFN_vkCmdResetQueryPool							qvkCmdResetQueryPool;
PFN_vkCmdBeginQuery								qvkCmdBeginQuery;
PFN_vkCmdEndQuery								qvkCmdEndQuery;
PFN_vkCreateBuffer								qvkCreateBuffer;
PFN_vkCreateCommandPool							qvkCreateCommandPool;
PFN_vkCreateDescriptorPool						qvkCreateDescriptorPool;
PFN_vkCreateDescriptorSetLayout					qvkCreateDescriptorSetLayout;
PFN_vkCreateFence								qvkCreateFence;
PFN_vkCreateFramebuffer							qvkCreateFramebuffer;
PFN_vkCreateComputePipelines						qvkCreateComputePipelines;
PFN_vkCreateGraphicsPipelines					qvkCreateGraphicsPipelines;
PFN_vkCreateImage								qvkCreateImage;
PFN_vkCreateImageView							qvkCreateImageView;
PFN_vkCreatePipelineLayout						qvkCreatePipelineLayout;
PFN_vkCreatePipelineCache						qvkCreatePipelineCache;
PFN_vkCreateQueryPool							qvkCreateQueryPool;
PFN_vkCreateRenderPass							qvkCreateRenderPass;
PFN_vkCreateSampler								qvkCreateSampler;
PFN_vkCreateSemaphore							qvkCreateSemaphore;
PFN_vkCreateShaderModule							qvkCreateShaderModule;
PFN_vkDestroyBuffer								qvkDestroyBuffer;
PFN_vkDestroyCommandPool							qvkDestroyCommandPool;
PFN_vkDestroyDescriptorPool						qvkDestroyDescriptorPool;
PFN_vkDestroyDescriptorSetLayout					qvkDestroyDescriptorSetLayout;
PFN_vkDestroyDevice								qvkDestroyDevice;
PFN_vkDestroyFence								qvkDestroyFence;
PFN_vkDestroyFramebuffer							qvkDestroyFramebuffer;
PFN_vkDestroyImage								qvkDestroyImage;
PFN_vkDestroyImageView							qvkDestroyImageView;
PFN_vkDestroyPipeline							qvkDestroyPipeline;
PFN_vkDestroyPipelineCache						qvkDestroyPipelineCache;
PFN_vkDestroyPipelineLayout						qvkDestroyPipelineLayout;
PFN_vkDestroyQueryPool							qvkDestroyQueryPool;
PFN_vkDestroyRenderPass							qvkDestroyRenderPass;
PFN_vkDestroySampler								qvkDestroySampler;
PFN_vkDestroySemaphore							qvkDestroySemaphore;
PFN_vkDestroyShaderModule						qvkDestroyShaderModule;
PFN_vkDeviceWaitIdle								qvkDeviceWaitIdle;
PFN_vkEndCommandBuffer							qvkEndCommandBuffer;
PFN_vkFlushMappedMemoryRanges					qvkFlushMappedMemoryRanges;
PFN_vkFreeCommandBuffers							qvkFreeCommandBuffers;
PFN_vkFreeDescriptorSets							qvkFreeDescriptorSets;
PFN_vkFreeMemory									qvkFreeMemory;
PFN_vkGetBufferMemoryRequirements				qvkGetBufferMemoryRequirements;
PFN_vkGetDeviceQueue								qvkGetDeviceQueue;
PFN_vkGetImageMemoryRequirements					qvkGetImageMemoryRequirements;
PFN_vkGetImageSubresourceLayout					qvkGetImageSubresourceLayout;
PFN_vkInvalidateMappedMemoryRanges				qvkInvalidateMappedMemoryRanges;
PFN_vkMapMemory									qvkMapMemory;
PFN_vkQueueSubmit								qvkQueueSubmit;
PFN_vkQueueWaitIdle								qvkQueueWaitIdle;
PFN_vkResetCommandBuffer							qvkResetCommandBuffer;
PFN_vkResetDescriptorPool						qvkResetDescriptorPool;
PFN_vkResetFences								qvkResetFences;
PFN_vkGetQueryPoolResults						qvkGetQueryPoolResults;
PFN_vkResetQueryPoolEXT							qvkResetQueryPoolEXT;
PFN_vkUnmapMemory								qvkUnmapMemory;
PFN_vkUpdateDescriptorSets							qvkUpdateDescriptorSets;
PFN_vkWaitForFences								qvkWaitForFences;
PFN_vkAcquireNextImageKHR						qvkAcquireNextImageKHR;
PFN_vkCreateSwapchainKHR							qvkCreateSwapchainKHR;
PFN_vkDestroySwapchainKHR						qvkDestroySwapchainKHR;
PFN_vkGetSwapchainImagesKHR						qvkGetSwapchainImagesKHR;
PFN_vkQueuePresentKHR							qvkQueuePresentKHR;

PFN_vkGetBufferMemoryRequirements2KHR			qvkGetBufferMemoryRequirements2KHR;
PFN_vkGetImageMemoryRequirements2KHR				qvkGetImageMemoryRequirements2KHR;

PFN_vkDebugMarkerSetObjectNameEXT				qvkDebugMarkerSetObjectNameEXT;

PFN_vkCmdClearColorImage								qvkCmdClearColorImage;

////////////////////////////////////////////////////////////////////////////

// forward declaration
VkPipeline create_pipeline( const Vk_Pipeline_Def *def, renderPass_t renderPassIndex, uint32_t def_index );
static uint32_t vk_alloc_pipeline( const Vk_Pipeline_Def *def );
static VkPipeline vk_gen_pipeline( uint32_t index );
uint32_t vk_find_pipeline_ext( uint32_t base, const Vk_Pipeline_Def *def, qboolean use );

#define VK_VOLUMETRIC_QUERY_SLOTS 16
#define VK_VOLUMETRIC_QUERY_COUNT (VK_VOLUMETRIC_QUERY_SLOTS * NUM_COMMAND_BUFFERS)

static void vk_create_froxel_images( void );
static void vk_update_volumetric_descriptors( void );
static void vk_create_sun_shadow_resources( void );
static void vk_destroy_sun_shadow_resources( void );
static void vk_create_local_shadow_resources( void );
static void vk_destroy_local_shadow_resources( void );
static void vk_create_volumetric_pipelines( void );
static void vk_create_volumetric_params_buffer( void );
static void vk_destroy_volumetric_params_buffer( void );
static void vk_create_postfx_params_buffers( void );
static void vk_destroy_postfx_params_buffers( void );
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

static VkShaderModule SHADER_MODULE(const uint8_t *bytes, const int count) {
	VkShaderModuleCreateInfo desc;
	VkShaderModule module;

	if ( count % 4 != 0 ) {
		ri.Error( ERR_FATAL, "Vulkan: SPIR-V binary buffer size is not a multiple of 4" );
	}

	desc.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.codeSize = count;
	desc.pCode = (const uint32_t*)bytes;

	VK_CHECK(qvkCreateShaderModule(vk.device, &desc, NULL, &module));

	return module;
}


void vk_update_attachment_descriptors( void ) {
	uint32_t i;

	if ( vk.color_image_view )
	{
		VkDescriptorImageInfo info;
		VkWriteDescriptorSet desc;
		Vk_Sampler_Def sd;

		Com_Memset( &sd, 0, sizeof( sd ) );
		// Post-process source should stay linear-filtered; Panini magnifies edges and nearest exacerbates aliasing.
		sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
		sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sd.max_lod_1_0 = qtrue;
		sd.noAnisotropy = qtrue;

		info.sampler = vk_find_sampler( &sd );
		info.imageView = vk.color_image_view;
		info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		desc.dstBinding = 0;
		desc.dstArrayElement = 0;
		desc.descriptorCount = 1;
		desc.pNext = NULL;
		desc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		desc.pImageInfo = &info;
		desc.pBufferInfo = NULL;
		desc.pTexelBufferView = NULL;
		for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
			desc.dstSet = vk.color_descriptor[i];
			qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
			if ( vk.post_color_descriptor[i] != VK_NULL_HANDLE ) {
				VkDescriptorImageInfo post_info;
				VkWriteDescriptorSet post_desc;
				Vk_Sampler_Def post_sd;

				Com_Memset( &post_sd, 0, sizeof( post_sd ) );
				post_sd.gl_mag_filter = post_sd.gl_min_filter = GL_LINEAR;
				post_sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
				post_sd.max_lod_1_0 = qtrue;
				post_sd.noAnisotropy = qtrue;

				post_info.sampler = vk_find_sampler( &post_sd );
				post_info.imageView = vk.color_image_view;
				post_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				post_desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				post_desc.dstSet = vk.post_color_descriptor[i];
				post_desc.dstBinding = 0;
				post_desc.dstArrayElement = 0;
				post_desc.descriptorCount = 1;
				post_desc.pNext = NULL;
				post_desc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				post_desc.pImageInfo = &post_info;
				post_desc.pBufferInfo = NULL;
				post_desc.pTexelBufferView = NULL;

				qvkUpdateDescriptorSets( vk.device, 1, &post_desc, 0, NULL );
			}
		}
		/* Ensure post-fog and luminance descriptors are initialized for gamma/eye-adaptation. */
		vk_update_post_fog_descriptors( vk.color_image_view );

		// screenmap
		sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
		/* screenMap UVs routinely go out of range on reflective surfaces.
		 * Clamp-to-edge smears the screen border across water/refraction planes,
		 * which shows up as the wide bright/dark bands in-game. */
		sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		sd.max_lod_1_0 = qfalse;
		sd.noAnisotropy = qtrue;

		info.sampler = vk_find_sampler( &sd );

		info.imageView = vk.screenMap.color_image_view;
		desc.dstSet = vk.screenMap.color_descriptor;

		qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

		if ( r_ssao && r_ssao->integer )
		{
			// depth sampling for SSAO (use depth-only view when available for VUID-01976)
			sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
			sd.max_lod_1_0 = qtrue;
			sd.noAnisotropy = qtrue;
			info.sampler = vk_find_sampler( &sd );
			info.imageView = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
			info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
				desc.dstSet = vk.depth_descriptor[i];
				qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
			}

			// ssao output
			sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
			sd.max_lod_1_0 = qtrue;
			sd.noAnisotropy = qtrue;
			info.sampler = vk_find_sampler( &sd );
			info.imageView = vk.ssao_image_view;
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			desc.dstSet = vk.ssao_descriptor;
			qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

			// ssao blur output
			info.imageView = vk.ssao_blur_image_view;
			desc.dstSet = vk.ssao_blur_descriptor;
			qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
			if ( vk.fog_scene_image_view ) {
				info.imageView = vk.fog_scene_image_view;
				desc.dstSet = vk.ssao_scene_descriptor;
				qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
			}
		}
		if ( r_oit && r_oit->integer ) {
			if ( vk.fog_scene_image_view ) {
				info.imageView = vk.fog_scene_image_view;
				desc.dstSet = vk.oit_opaque_descriptor;
				qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
			}
			if ( vk.oit_accum_image_view ) {
				info.imageView = vk.oit_accum_image_view;
				desc.dstSet = vk.oit_accum_descriptor;
				qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
			}
			if ( vk.oit_reveal_image_view ) {
				info.imageView = vk.oit_reveal_image_view;
				desc.dstSet = vk.oit_reveal_descriptor;
				qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
			}
			if ( vk.oit_depth_descriptor ) {
				VkImageView depth_view = VK_NULL_HANDLE;
				VkImageLayout depth_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
				sd.max_lod_1_0 = qtrue;
				sd.noAnisotropy = qtrue;
				info.sampler = vk_find_sampler( &sd );
				if ( vk.msaaActive && vk.volumetric_depth_view != VK_NULL_HANDLE ) {
					depth_view = vk.volumetric_depth_view;
					depth_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				} else {
					depth_view = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
					depth_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
				}
				if ( depth_view != VK_NULL_HANDLE ) {
					info.imageView = depth_view;
					info.imageLayout = depth_layout;
					desc.dstSet = vk.oit_depth_descriptor;
					qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
				}
			}
		}

		if ( PostFX_SSR_IsEnabled() && vk.ssr_image_view )
		{
			// ssr set 0: color texture
			sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
			sd.max_lod_1_0 = qtrue;
			sd.noAnisotropy = qtrue;
			info.sampler = vk_find_sampler( &sd );
			info.imageView = vk.color_image_view;
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			desc.dstSet = vk.ssr_descriptor[0];
			qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

			// ssr set 1: depth texture (use depth-only view when available for VUID-01976)
			sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
			info.sampler = vk_find_sampler( &sd );
			info.imageView = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
			info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			desc.dstSet = vk.ssr_descriptor[1];
			qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
		}

		// bloom images
		if ( r_bloom->integer )
		{
			uint32_t j;

			sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
			sd.max_lod_1_0 = qtrue;
			sd.noAnisotropy = qtrue;
			info.sampler = vk_find_sampler( &sd );
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			for ( j = 0; j < ARRAY_LEN( vk.bloom_image_descriptor ); j++ )
			{
				info.imageView = vk.bloom_image_view[j];
				desc.dstSet = vk.bloom_image_descriptor[j];

				qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
			}
		}

		if ( vk.smaaActive )
		{
			sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
			sd.max_lod_1_0 = qtrue;
			sd.noAnisotropy = qtrue;
			info.sampler = vk_find_sampler( &sd );
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			if ( vk.smaa_edge_image_view )
			{
				info.imageView = vk.color_image_view;
				desc.dstSet = vk.smaa_edge_descriptor;
				qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
			}
			if ( vk.smaa_edge_image_view )
			{
				info.imageView = vk.smaa_edge_image_view;
				desc.dstSet = vk.smaa_blend_descriptor;
				qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
			}
			if ( vk.smaa_blend_image_view )
			{
				info.imageView = vk.smaa_blend_image_view;
				desc.dstSet = vk.smaa_compose_descriptor;
				qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
			}
		}
		for ( i = 0; i < 2; i++ ) {
			if ( vk.taa_history_image_view[i] ) {
				info.imageView = vk.taa_history_image_view[i];
				desc.dstSet = vk.taa_history_descriptor[i];
				qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
			}
		}

#ifdef VK_PBR_BRDFLUT
		if( vk.pbrActive )
		{
			// brdf
			info.imageView = vk.brdflut_image_view;
			desc.dstSet = vk.brdflut_image_descriptor;
			qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );	
		
			// cubemap
			info.imageView = vk.cubeMap.color_image_view[0];
			desc.dstSet = vk.cubeMap.color_descriptor;
			qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );	
		}
#endif
	}
}

static void vk_update_volumetric_descriptors( void )
{
	VkDescriptorBufferInfo params_buffer;
	VkImageView volumetric_depth_view;
	VkImageLayout volumetric_depth_layout;

	if ( vk.volumetric_params_buffer == VK_NULL_HANDLE ) {
		return;
	}

	params_buffer.buffer = vk.volumetric_params_buffer;
	params_buffer.offset = 0;
	params_buffer.range = sizeof( volumetric_params_t );

	volumetric_depth_view = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
	volumetric_depth_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	if ( vk.msaaActive && vk.volumetric_depth_view != VK_NULL_HANDLE ) {
		volumetric_depth_view = vk.volumetric_depth_view;
		volumetric_depth_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}

	if ( vk.volumetric_compute_descriptor != VK_NULL_HANDLE &&
		vk.froxel_volume_view && vk.froxel_history_view && vk.froxel_light_view &&
		vk.froxel_extinction_view && vk.froxel_clamp_view &&
		volumetric_depth_view && vk.fog_noise_view && vk.sun_shadow_sample_view &&
		vk.local_spot_shadow_atlas_sample_view && vk.local_point_shadow_array_sample_view && vk.motion_vector_view &&
		vk.fluid_velocity_views[0] && vk.fluid_velocity_views[1] &&
		vk.fluid_density_views[0] && vk.fluid_density_views[1] &&
		vk.volumetric_telemetry_view )
	{
		VkDescriptorImageInfo storage_info[5];
		VkDescriptorImageInfo telemetry_info;
		VkDescriptorImageInfo depth_info;
		VkDescriptorImageInfo noise_info;
		VkDescriptorImageInfo shadow_info;
		VkDescriptorImageInfo local_spot_shadow_info;
		VkDescriptorImageInfo local_point_shadow_info;
		VkDescriptorImageInfo motion_info;
		VkDescriptorImageInfo fluid_info[4];
		VkWriteDescriptorSet writes[17];
		Vk_Sampler_Def depth_sd;
		Vk_Sampler_Def noise_sd;
		Vk_Sampler_Def shadow_sd;
		Vk_Sampler_Def motion_sd;
		Vk_Sampler_Def fluid_sd;

		Com_Memset( storage_info, 0, sizeof( storage_info ) );
		storage_info[0].imageView = vk.froxel_volume_view;
		storage_info[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		storage_info[1].imageView = vk.froxel_history_view;
		storage_info[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		storage_info[2].imageView = vk.froxel_light_view;
		storage_info[2].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		storage_info[3].imageView = vk.froxel_extinction_view;
		storage_info[3].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		storage_info[4].imageView = vk.froxel_clamp_view;
		storage_info[4].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		Com_Memset( &depth_sd, 0, sizeof( depth_sd ) );
		depth_sd.gl_mag_filter = depth_sd.gl_min_filter = GL_NEAREST;
		depth_sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		depth_sd.noAnisotropy = qtrue;
		vk.froxel_depth_sampler = vk_find_sampler( &depth_sd );

		Com_Memset( &depth_info, 0, sizeof( depth_info ) );
		depth_info.sampler = vk.froxel_depth_sampler;
		depth_info.imageView = volumetric_depth_view;
		depth_info.imageLayout = volumetric_depth_layout;
		Com_Memset( &noise_sd, 0, sizeof( noise_sd ) );
		noise_sd.gl_mag_filter = noise_sd.gl_min_filter = GL_LINEAR;
		noise_sd.address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		noise_sd.noAnisotropy = qtrue;
		vk.fog_noise_sampler = vk_find_sampler( &noise_sd );

		Com_Memset( &noise_info, 0, sizeof( noise_info ) );
		noise_info.sampler = vk.fog_noise_sampler;
		noise_info.imageView = vk.fog_noise_view;
		noise_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		Com_Memset( &shadow_sd, 0, sizeof( shadow_sd ) );
		shadow_sd.gl_mag_filter = shadow_sd.gl_min_filter = GL_NEAREST;
		shadow_sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		shadow_sd.noAnisotropy = qtrue;
		vk.sun_shadow_sampler = vk_find_sampler( &shadow_sd );

		Com_Memset( &shadow_info, 0, sizeof( shadow_info ) );
		shadow_info.sampler = vk.sun_shadow_sampler;
		shadow_info.imageView = vk.sun_shadow_sample_view;
		shadow_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

		Com_Memset( &local_spot_shadow_info, 0, sizeof( local_spot_shadow_info ) );
		local_spot_shadow_info.sampler = vk.sun_shadow_sampler;
		local_spot_shadow_info.imageView = vk.local_spot_shadow_atlas_sample_view;
		local_spot_shadow_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

		Com_Memset( &local_point_shadow_info, 0, sizeof( local_point_shadow_info ) );
		local_point_shadow_info.sampler = vk.sun_shadow_sampler;
		local_point_shadow_info.imageView = vk.local_point_shadow_array_sample_view;
		local_point_shadow_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

		Com_Memset( &motion_sd, 0, sizeof( motion_sd ) );
		motion_sd.gl_mag_filter = motion_sd.gl_min_filter = GL_LINEAR;
		motion_sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		motion_sd.noAnisotropy = qtrue;

		Com_Memset( &motion_info, 0, sizeof( motion_info ) );
		motion_info.sampler = vk_find_sampler( &motion_sd );
		motion_info.imageView = vk.motion_vector_view;
		motion_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		Com_Memset( &telemetry_info, 0, sizeof( telemetry_info ) );
		telemetry_info.imageView = vk.volumetric_telemetry_view;
		telemetry_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		Com_Memset( &fluid_sd, 0, sizeof( fluid_sd ) );
		fluid_sd.gl_mag_filter = fluid_sd.gl_min_filter = GL_LINEAR;
		fluid_sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		fluid_sd.noAnisotropy = qtrue;
		const VkSampler fluid_sampler = vk_find_sampler( &fluid_sd );

		Com_Memset( fluid_info, 0, sizeof( fluid_info ) );
		fluid_info[0].sampler = fluid_sampler;
		fluid_info[0].imageView = vk.fluid_velocity_views[0];
		fluid_info[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		fluid_info[1].sampler = fluid_sampler;
		fluid_info[1].imageView = vk.fluid_velocity_views[1];
		fluid_info[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		fluid_info[2].sampler = fluid_sampler;
		fluid_info[2].imageView = vk.fluid_density_views[0];
		fluid_info[2].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		fluid_info[3].sampler = fluid_sampler;
		fluid_info[3].imageView = vk.fluid_density_views[1];
		fluid_info[3].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		Com_Memset( writes, 0, sizeof( writes ) );
		for ( int i = 0; i < 2; i++ ) {
			writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[i].dstSet = vk.volumetric_compute_descriptor;
			writes[i].dstBinding = i;
			writes[i].descriptorCount = 1;
			writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			writes[i].pImageInfo = &storage_info[i];
		}

		writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[2].dstSet = vk.volumetric_compute_descriptor;
		writes[2].dstBinding = 2;
		writes[2].descriptorCount = 1;
		writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[2].pImageInfo = &depth_info;

		writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[3].dstSet = vk.volumetric_compute_descriptor;
		writes[3].dstBinding = 3;
		writes[3].descriptorCount = 1;
		writes[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[3].pBufferInfo = &params_buffer;

		writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[4].dstSet = vk.volumetric_compute_descriptor;
		writes[4].dstBinding = 4;
		writes[4].descriptorCount = 1;
		writes[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[4].pImageInfo = &noise_info;

		writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[5].dstSet = vk.volumetric_compute_descriptor;
		writes[5].dstBinding = 5;
		writes[5].descriptorCount = 1;
		writes[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[5].pImageInfo = &shadow_info;

		writes[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[6].dstSet = vk.volumetric_compute_descriptor;
		writes[6].dstBinding = 6;
		writes[6].descriptorCount = 1;
		writes[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[6].pImageInfo = &storage_info[2];

		writes[7].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[7].dstSet = vk.volumetric_compute_descriptor;
		writes[7].dstBinding = 7;
		writes[7].descriptorCount = 1;
		writes[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[7].pImageInfo = &storage_info[3];

		writes[8].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[8].dstSet = vk.volumetric_compute_descriptor;
		writes[8].dstBinding = 8;
		writes[8].descriptorCount = 1;
		writes[8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[8].pImageInfo = &storage_info[4];

		writes[9].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[9].dstSet = vk.volumetric_compute_descriptor;
		writes[9].dstBinding = 9;
		writes[9].descriptorCount = 1;
		writes[9].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[9].pImageInfo = &motion_info;

		writes[10].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[10].dstSet = vk.volumetric_compute_descriptor;
		writes[10].dstBinding = 10;
		writes[10].descriptorCount = 1;
		writes[10].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[10].pImageInfo = &local_spot_shadow_info;

		writes[11].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[11].dstSet = vk.volumetric_compute_descriptor;
		writes[11].dstBinding = 11;
		writes[11].descriptorCount = 1;
		writes[11].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[11].pImageInfo = &local_point_shadow_info;

		for ( int i = 0; i < 4; i++ ) {
			writes[12 + i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[12 + i].dstSet = vk.volumetric_compute_descriptor;
			writes[12 + i].dstBinding = 12 + i;
			writes[12 + i].descriptorCount = 1;
			writes[12 + i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writes[12 + i].pImageInfo = &fluid_info[i];
		}

		writes[16].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[16].dstSet = vk.volumetric_compute_descriptor;
		writes[16].dstBinding = 16;
		writes[16].descriptorCount = 1;
		writes[16].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[16].pImageInfo = &telemetry_info;

		qvkUpdateDescriptorSets( vk.device, ARRAY_LEN( writes ), writes, 0, NULL );
	}

	if ( vk.volumetric_composite_descriptor != VK_NULL_HANDLE &&
		vk.fog_scene_image_view && volumetric_depth_view && vk.froxel_volume_view && vk.froxel_extinction_view &&
		vk.motion_vector_view && vk.local_spot_shadow_atlas_sample_view && vk.local_point_shadow_array_sample_view &&
		vk.volumetric_telemetry_view )
	{
		VkDescriptorImageInfo composite_info[8];
		VkWriteDescriptorSet writes[9];
		Vk_Sampler_Def color_sd;
		Vk_Sampler_Def volume_sd;
		Vk_Sampler_Def motion_sd;
		Vk_Sampler_Def shadow_sd;
		Vk_Sampler_Def telemetry_sd;

		Com_Memset( &color_sd, 0, sizeof( color_sd ) );
		color_sd.gl_mag_filter = color_sd.gl_min_filter = GL_LINEAR;
		color_sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		color_sd.noAnisotropy = qtrue;

		Com_Memset( &volume_sd, 0, sizeof( volume_sd ) );
		volume_sd.gl_mag_filter = volume_sd.gl_min_filter = GL_LINEAR;
		volume_sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		volume_sd.noAnisotropy = qtrue;
		vk.froxel_sampler = vk_find_sampler( &volume_sd );

		Com_Memset( &motion_sd, 0, sizeof( motion_sd ) );
		motion_sd.gl_mag_filter = motion_sd.gl_min_filter = GL_LINEAR;
		motion_sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		motion_sd.noAnisotropy = qtrue;

		Com_Memset( &shadow_sd, 0, sizeof( shadow_sd ) );
		shadow_sd.gl_mag_filter = shadow_sd.gl_min_filter = GL_NEAREST;
		shadow_sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		shadow_sd.noAnisotropy = qtrue;

		Com_Memset( &telemetry_sd, 0, sizeof( telemetry_sd ) );
		telemetry_sd.gl_mag_filter = telemetry_sd.gl_min_filter = GL_NEAREST;
		telemetry_sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		telemetry_sd.noAnisotropy = qtrue;

		Com_Memset( composite_info, 0, sizeof( composite_info ) );

		// sceneColor (binding 0)
		composite_info[0].sampler = vk_find_sampler( &color_sd );
		composite_info[0].imageView = vk.fog_scene_image_view;
		composite_info[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		// depthTexture (binding 1)
		composite_info[1].sampler = vk.froxel_depth_sampler;
		composite_info[1].imageView = volumetric_depth_view;
		composite_info[1].imageLayout = volumetric_depth_layout;

		// froxelScattering (binding 2)
		composite_info[2].sampler = vk.froxel_sampler;
		composite_info[2].imageView = vk.froxel_volume_view;
		composite_info[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		// froxelExtinction (binding 3)
		composite_info[3].sampler = vk.froxel_sampler;
		composite_info[3].imageView = vk.froxel_extinction_view;
		composite_info[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		// motionTexture (binding 5)
		composite_info[4].sampler = vk_find_sampler( &motion_sd );
		composite_info[4].imageView = vk.motion_vector_view;
		composite_info[4].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		// localSpotShadowMap (binding 6)
		composite_info[5].sampler = vk_find_sampler( &shadow_sd );
		composite_info[5].imageView = vk.local_spot_shadow_atlas_sample_view;
		composite_info[5].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

		// localPointShadowMap (binding 7)
		composite_info[6].sampler = vk_find_sampler( &shadow_sd );
		composite_info[6].imageView = vk.local_point_shadow_array_sample_view;
		composite_info[6].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

		// telemetryTexture (binding 8)
		composite_info[7].sampler = vk_find_sampler( &telemetry_sd );
		composite_info[7].imageView = vk.volumetric_telemetry_view;
		composite_info[7].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		Com_Memset( writes, 0, sizeof( writes ) );
		for ( int i = 0; i < 4; i++ ) {
			writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[i].dstSet = vk.volumetric_composite_descriptor;
			writes[i].dstBinding = i;
			writes[i].descriptorCount = 1;
			writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writes[i].pImageInfo = &composite_info[i];
		}

		writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[4].dstSet = vk.volumetric_composite_descriptor;
		writes[4].dstBinding = 4;
		writes[4].descriptorCount = 1;
		writes[4].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[4].pBufferInfo = &params_buffer;

		for ( int i = 0; i < 3; i++ ) {
			writes[5 + i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[5 + i].dstSet = vk.volumetric_composite_descriptor;
			writes[5 + i].dstBinding = 5 + i;
			writes[5 + i].descriptorCount = 1;
			writes[5 + i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writes[5 + i].pImageInfo = &composite_info[4 + i];
		}

		writes[8].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[8].dstSet = vk.volumetric_composite_descriptor;
		writes[8].dstBinding = 8;
		writes[8].descriptorCount = 1;
		writes[8].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[8].pImageInfo = &composite_info[7];

		qvkUpdateDescriptorSets( vk.device, ARRAY_LEN( writes ), writes, 0, NULL );
	}

	if ( vk.volumetric_depth_resolve_descriptor != VK_NULL_HANDLE &&
		vk.msaaActive && vk.depth_image_view && vk.volumetric_depth_view )
	{
		VkDescriptorImageInfo resolve_info[2];
		VkWriteDescriptorSet resolve_writes[2];
		Vk_Sampler_Def depth_sd;

		if ( vk.froxel_depth_sampler == VK_NULL_HANDLE ) {
			Com_Memset( &depth_sd, 0, sizeof( depth_sd ) );
			depth_sd.gl_mag_filter = depth_sd.gl_min_filter = GL_NEAREST;
			depth_sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			depth_sd.noAnisotropy = qtrue;
			vk.froxel_depth_sampler = vk_find_sampler( &depth_sd );
		}

		Com_Memset( resolve_info, 0, sizeof( resolve_info ) );
		resolve_info[0].sampler = vk.froxel_depth_sampler;
		resolve_info[0].imageView = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
		resolve_info[0].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		resolve_info[1].imageView = vk.volumetric_depth_view;
		resolve_info[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		Com_Memset( resolve_writes, 0, sizeof( resolve_writes ) );
		resolve_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		resolve_writes[0].dstSet = vk.volumetric_depth_resolve_descriptor;
		resolve_writes[0].dstBinding = 0;
		resolve_writes[0].descriptorCount = 1;
		resolve_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		resolve_writes[0].pImageInfo = &resolve_info[0];

		resolve_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		resolve_writes[1].dstSet = vk.volumetric_depth_resolve_descriptor;
		resolve_writes[1].dstBinding = 1;
		resolve_writes[1].descriptorCount = 1;
		resolve_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		resolve_writes[1].pImageInfo = &resolve_info[1];

		qvkUpdateDescriptorSets( vk.device, ARRAY_LEN( resolve_writes ), resolve_writes, 0, NULL );
	}

	if ( vk.volumetric_fluid_descriptor != VK_NULL_HANDLE &&
		vk.fluid_velocity_views[0] && vk.fluid_velocity_views[1] &&
		vk.fluid_density_views[0] && vk.fluid_density_views[1] &&
		vk.fluid_pressure_views[0] && vk.fluid_pressure_views[1] &&
		vk.fluid_divergence_view && vk.volumetric_params_buffer != VK_NULL_HANDLE &&
		vk.volumetric_telemetry_view )
	{
		VkDescriptorImageInfo info[15];
		VkWriteDescriptorSet writes[16];
		Vk_Sampler_Def sd_linear;
		Vk_Sampler_Def sd_nearest;
			VkSampler sampler_linear;
			VkSampler sampler_nearest;

		Com_Memset( &sd_linear, 0, sizeof( sd_linear ) );
		sd_linear.gl_mag_filter = sd_linear.gl_min_filter = GL_LINEAR;
		sd_linear.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sd_linear.noAnisotropy = qtrue;

		Com_Memset( &sd_nearest, 0, sizeof( sd_nearest ) );
		sd_nearest.gl_mag_filter = sd_nearest.gl_min_filter = GL_NEAREST;
		sd_nearest.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sd_nearest.noAnisotropy = qtrue;

		sampler_linear = vk_find_sampler( &sd_linear );
		sampler_nearest = vk_find_sampler( &sd_nearest );

		Com_Memset( info, 0, sizeof( info ) );
		info[0].sampler = sampler_linear;
		info[0].imageView = vk.fluid_velocity_views[0];
		info[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		info[1].sampler = sampler_linear;
		info[1].imageView = vk.fluid_velocity_views[1];
		info[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		info[2].imageView = vk.fluid_velocity_views[0];
		info[2].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		info[3].imageView = vk.fluid_velocity_views[1];
		info[3].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		info[4].sampler = sampler_linear;
		info[4].imageView = vk.fluid_density_views[0];
		info[4].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		info[5].sampler = sampler_linear;
		info[5].imageView = vk.fluid_density_views[1];
		info[5].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		info[6].imageView = vk.fluid_density_views[0];
		info[6].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		info[7].imageView = vk.fluid_density_views[1];
		info[7].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		info[8].sampler = sampler_nearest;
		info[8].imageView = vk.fluid_pressure_views[0];
		info[8].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		info[9].sampler = sampler_nearest;
		info[9].imageView = vk.fluid_pressure_views[1];
		info[9].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		info[10].imageView = vk.fluid_pressure_views[0];
		info[10].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		info[11].imageView = vk.fluid_pressure_views[1];
		info[11].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		info[12].imageView = vk.fluid_divergence_view;
		info[12].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		info[13].sampler = sampler_nearest;
		info[13].imageView = vk.fluid_divergence_view;
		info[13].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		info[14].imageView = vk.volumetric_telemetry_view;
		info[14].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		Com_Memset( writes, 0, sizeof( writes ) );
		for ( uint32_t i = 0; i < 15; i++ ) {
			writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[i].dstSet = vk.volumetric_fluid_descriptor;
			writes[i].dstBinding = ( i == 14 ) ? 15 : i;
			writes[i].descriptorCount = 1;
			writes[i].descriptorType =
				(i == 2 || i == 3 || i == 6 || i == 7 || i == 10 || i == 11 || i == 12 || i == 14) ?
				VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writes[i].pImageInfo = &info[i];
		}

		writes[15].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[15].dstSet = vk.volumetric_fluid_descriptor;
		writes[15].dstBinding = 14;
		writes[15].descriptorCount = 1;
		writes[15].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[15].pBufferInfo = &params_buffer;

		qvkUpdateDescriptorSets( vk.device, ARRAY_LEN( writes ), writes, 0, NULL );
	}

	if ( r_fogDebug && r_fogDebug->integer >= 1 && ( vk.volumetric_frame % 120u ) == 0u ) {
		ri.Printf( PRINT_ALL,
			"[VK][fog] descriptors computeSet=0x%llx storage=GENERAL history=GENERAL light=GENERAL ext=GENERAL clamp=GENERAL noiseView=0x%llx sunShadow=0x%llx localSpot=0x%llx localPoint=0x%llx motionView=0x%llx depthView=0x%llx compositeSet=0x%llx sceneView=0x%llx\n",
			(unsigned long long)(uintptr_t)vk.volumetric_compute_descriptor,
			(unsigned long long)(uintptr_t)vk.fog_noise_view,
			(unsigned long long)(uintptr_t)vk.sun_shadow_sample_view,
			(unsigned long long)(uintptr_t)vk.local_spot_shadow_atlas_sample_view,
			(unsigned long long)(uintptr_t)vk.local_point_shadow_array_sample_view,
			(unsigned long long)(uintptr_t)vk.motion_vector_view,
			(unsigned long long)(uintptr_t)volumetric_depth_view,
			(unsigned long long)(uintptr_t)vk.volumetric_composite_descriptor,
			(unsigned long long)(uintptr_t)vk.fog_scene_image_view );
	}
}


void vk_init_descriptors( void )
{
	VkDescriptorSetAllocateInfo alloc;
	VkDescriptorBufferInfo info;
	VkWriteDescriptorSet desc;
	uint32_t i;

	vk_create_postfx_params_buffers();

	alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc.pNext = NULL;
	alloc.descriptorPool = vk.descriptor_pool;
	alloc.descriptorSetCount = 1;
	alloc.pSetLayouts = &vk.set_layout_storage;

	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.storage.descriptor ) );

	info.buffer = vk.storage.buffer;
	info.offset = 0;
	info.range = sizeof( uint32_t );

	desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	desc.dstSet = vk.storage.descriptor;
	desc.dstBinding = 0;
	desc.dstArrayElement = 0;
	desc.descriptorCount = 1;
	desc.pNext = NULL;
	desc.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
	desc.pImageInfo = NULL;
	desc.pBufferInfo = &info;
	desc.pTexelBufferView = NULL;

	qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

	// allocated and update descriptor set
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ )
	{
		alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc.pNext = NULL;
		alloc.descriptorPool = vk.descriptor_pool;
		alloc.descriptorSetCount = 1;
		alloc.pSetLayouts = &vk.set_layout_uniform;

		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.tess[i].uniform_descriptor ) );

		vk_update_uniform_descriptor( vk.tess[ i ].uniform_descriptor, vk.tess[ i ].vertex_buffer );

		SET_OBJECT_NAME( vk.tess[ i ].uniform_descriptor, va( "uniform descriptor %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
	}

	if ( vk.color_image_view )
	{
		alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc.pNext = NULL;
		alloc.descriptorPool = vk.descriptor_pool;
		alloc.descriptorSetCount = 1;
		alloc.pSetLayouts = &vk.set_layout_sampler;

			for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
				VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.color_descriptor[i] ) );
				VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.post_color_descriptor[i] ) );
			}
		for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ )
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.depth_descriptor[i] ) );

		if ( r_ssao && r_ssao->integer ) {
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.ssao_descriptor ) );
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.ssao_blur_descriptor ) );
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.ssao_scene_descriptor ) );
		}
		if ( r_oit && r_oit->integer ) {
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.oit_opaque_descriptor ) );
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.oit_accum_descriptor ) );
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.oit_reveal_descriptor ) );
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.oit_depth_descriptor ) );
		}

		if ( PostFX_SSR_IsEnabled() ) {
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.ssr_descriptor[0] ) );
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.ssr_descriptor[1] ) );
		}

		if ( r_bloom->integer )
		{
			for ( i = 0; i < ARRAY_LEN( vk.bloom_image_descriptor ); i++ )
			{
				VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.bloom_image_descriptor[i] ) );
			}
		}

		if ( vk.smaaActive )
		{
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.smaa_edge_descriptor ) );
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.smaa_blend_descriptor ) );
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.smaa_compose_descriptor ) );
		}
		for ( i = 0; i < 2; i++ ) {
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.taa_history_descriptor[i] ) );
		}

		alloc.descriptorSetCount = 1;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.screenMap.color_descriptor ) ); // screenmap

#ifdef VK_PBR_BRDFLUT
		if( vk.pbrActive )
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.brdflut_image_descriptor ) );
#endif

		// cubemap
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.cubeMap.color_descriptor ) );

		alloc.pSetLayouts = &vk.set_layout_postfx_uniform;
		for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.postfx_params_descriptor[i] ) );
		}

		alloc.pSetLayouts = &vk.volumetric_compute_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.volumetric_compute_descriptor ) );

		alloc.pSetLayouts = &vk.volumetric_composite_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.volumetric_composite_descriptor ) );

			if ( vk.volumetric_depth_resolve_layout != VK_NULL_HANDLE ) {
				alloc.pSetLayouts = &vk.volumetric_depth_resolve_layout;
				VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.volumetric_depth_resolve_descriptor ) );
			}
			if ( vk.luminance_layout != VK_NULL_HANDLE ) {
				alloc.pSetLayouts = &vk.luminance_layout;
				for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ )
					VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.luminance_descriptor[i] ) );
			}
			if ( vk.volumetric_fluid_layout != VK_NULL_HANDLE ) {
				alloc.pSetLayouts = &vk.volumetric_fluid_layout;
				VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.volumetric_fluid_descriptor ) );
			}

			for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
				VkDescriptorBufferInfo postfx_info;
				VkWriteDescriptorSet postfx_desc;

				postfx_info.buffer = vk.postfx_params_buffer[i];
				postfx_info.offset = 0;
				postfx_info.range = sizeof( VkPostFXParams );

				Com_Memset( &postfx_desc, 0, sizeof( postfx_desc ) );
				postfx_desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				postfx_desc.dstSet = vk.postfx_params_descriptor[i];
				postfx_desc.dstBinding = 0;
				postfx_desc.descriptorCount = 1;
				postfx_desc.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				postfx_desc.pBufferInfo = &postfx_info;
				qvkUpdateDescriptorSets( vk.device, 1, &postfx_desc, 0, NULL );
			}

			vk_update_attachment_descriptors();
			vk_update_volumetric_descriptors();
		}
	}


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

/* vk_create_gltf_buffers moved to vk_gltf.c */

#include "shaders/spirv/shader_data.c"
#define SHADER_MODULE(name) SHADER_MODULE(name,sizeof(name))

#include "shaders/spirv/shader_binding.c"

static void vk_create_shader_modules( void )
{	
	int i, j, k;

	// specialized depth-fragment shader
	vk.modules.frag.gen0_df = SHADER_MODULE( frag_tx0_df );
	SET_OBJECT_NAME( vk.modules.frag.gen0_df, "single-texture df fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.frag.ent[0][0][0] = SHADER_MODULE( frag_tx0_ent );
	vk.modules.frag.ent[0][0][1] = SHADER_MODULE( frag_tx0_ent_fog );
	vk.modules.frag.ent[1][0][0] = SHADER_MODULE( frag_pbr_tx0_ent );
	vk.modules.frag.ent[1][0][1] = SHADER_MODULE( frag_pbr_tx0_ent_fog );
	//vk.modules.frag.ent[1][0] = SHADER_MODULE( frag_tx1_ent );
	//vk.modules.frag.ent[1][1] = SHADER_MODULE( frag_tx1_ent_fog );

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

#ifdef USE_VK_PBR
	vk_bind_generated_shaders();
#else
	vk.modules.vert.gen[0][0][0][0] = SHADER_MODULE( vert_tx0 );
	vk.modules.vert.gen[0][0][0][1] = SHADER_MODULE( vert_tx0_fog );
	vk.modules.vert.gen[0][0][1][0] = SHADER_MODULE( vert_tx0_env );
	vk.modules.vert.gen[0][0][1][1] = SHADER_MODULE( vert_tx0_env_fog );

	vk.modules.vert.gen[1][0][0][0] = SHADER_MODULE( vert_tx1 );
	vk.modules.vert.gen[1][0][0][1] = SHADER_MODULE( vert_tx1_fog );
	vk.modules.vert.gen[1][0][1][0] = SHADER_MODULE( vert_tx1_env );
	vk.modules.vert.gen[1][0][1][1] = SHADER_MODULE( vert_tx1_env_fog );

	vk.modules.vert.gen[1][1][0][0] = SHADER_MODULE( vert_tx1_cl );
	vk.modules.vert.gen[1][1][0][1] = SHADER_MODULE( vert_tx1_cl_fog );
	vk.modules.vert.gen[1][1][1][0] = SHADER_MODULE( vert_tx1_cl_env );
	vk.modules.vert.gen[1][1][1][1] = SHADER_MODULE( vert_tx1_cl_env_fog );

	vk.modules.vert.gen[2][0][0][0] = SHADER_MODULE( vert_tx2 );
	vk.modules.vert.gen[2][0][0][1] = SHADER_MODULE( vert_tx2_fog );
	vk.modules.vert.gen[2][0][1][0] = SHADER_MODULE( vert_tx2_env );
	vk.modules.vert.gen[2][0][1][1] = SHADER_MODULE( vert_tx2_env_fog );

	vk.modules.vert.gen[2][1][0][0] = SHADER_MODULE( vert_tx2_cl );
	vk.modules.vert.gen[2][1][0][1] = SHADER_MODULE( vert_tx2_cl_fog );
	vk.modules.vert.gen[2][1][1][0] = SHADER_MODULE( vert_tx2_cl_env );
	vk.modules.vert.gen[2][1][1][1] = SHADER_MODULE( vert_tx2_cl_env_fog );

	for ( i = 0; i < 3; i++ ) {
		const char *tx[] = { "single", "double", "triple" };
		const char *cl[] = { "", "+cl" };
		const char *env[] = { "", "+env" };
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				for ( l = 0; l < 2; l++ ) {
					const char *s = va( "%s-texture%s%s%s vertex module", tx[i], cl[j], env[k], fog[l] );
					SET_OBJECT_NAME( vk.modules.vert.gen[i][j][k][l], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
				}
			}
		}
	}

	// fixed-color (1.0) shader modules
	vk.modules.vert.ident1[0][0][0] = SHADER_MODULE( vert_tx0_ident1 );
	vk.modules.vert.ident1[0][0][1] = SHADER_MODULE( vert_tx0_ident1_fog );
	vk.modules.vert.ident1[0][1][0] = SHADER_MODULE( vert_tx0_ident1_env );
	vk.modules.vert.ident1[0][1][1] = SHADER_MODULE( vert_tx0_ident1_env_fog );
	vk.modules.vert.ident1[1][0][0] = SHADER_MODULE( vert_tx1_ident1 );
	vk.modules.vert.ident1[1][0][1] = SHADER_MODULE( vert_tx1_ident1_fog );
	vk.modules.vert.ident1[1][1][0] = SHADER_MODULE( vert_tx1_ident1_env );
	vk.modules.vert.ident1[1][1][1] = SHADER_MODULE( vert_tx1_ident1_env_fog );
	for ( i = 0; i < 2; i++ ) {
		const char *tx[] = { "single", "double" };
		const char *env[] = { "", "+env" };
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				const char *s = va( "%s-texture identity%s%s vertex module", tx[i], env[j], fog[k] );
				SET_OBJECT_NAME( vk.modules.vert.ident1[i][j][k], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
			}
		}
	}

	vk.modules.frag.ident1[0][0] = SHADER_MODULE( frag_tx0_ident1 );
	vk.modules.frag.ident1[0][1] = SHADER_MODULE( frag_tx0_ident1_fog );
	vk.modules.frag.ident1[1][0] = SHADER_MODULE( frag_tx1_ident1 );
	vk.modules.frag.ident1[1][1] = SHADER_MODULE( frag_tx1_ident1_fog );
	for ( i = 0; i < 2; i++ ) {
		const char *tx[] = { "single", "double" };
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			const char *s = va( "%s-texture identity%s fragment module", tx[i], fog[j] );
			SET_OBJECT_NAME( vk.modules.frag.ident1[i][j], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
		}
	}

	vk.modules.vert.fixed[0][0][0] = SHADER_MODULE( vert_tx0_fixed );
	vk.modules.vert.fixed[0][0][1] = SHADER_MODULE( vert_tx0_fixed_fog );
	vk.modules.vert.fixed[0][1][0] = SHADER_MODULE( vert_tx0_fixed_env );
	vk.modules.vert.fixed[0][1][1] = SHADER_MODULE( vert_tx0_fixed_env_fog );
	vk.modules.vert.fixed[1][0][0] = SHADER_MODULE( vert_tx1_fixed );
	vk.modules.vert.fixed[1][0][1] = SHADER_MODULE( vert_tx1_fixed_fog );
	vk.modules.vert.fixed[1][1][0] = SHADER_MODULE( vert_tx1_fixed_env );
	vk.modules.vert.fixed[1][1][1] = SHADER_MODULE( vert_tx1_fixed_env_fog );
	for ( i = 0; i < 2; i++ ) {
		const char *tx[] = { "single", "double" };
		const char *env[] = { "", "+env" };
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				const char *s = va( "%s-texture fixed-color%s%s vertex module", tx[i], env[j], fog[k] );
				SET_OBJECT_NAME( vk.modules.vert.fixed[i][j][k], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
			}
		}
	}

	vk.modules.frag.fixed[0][0] = SHADER_MODULE( frag_tx0_fixed );
	vk.modules.frag.fixed[0][1] = SHADER_MODULE( frag_tx0_fixed_fog );
	vk.modules.frag.fixed[1][0] = SHADER_MODULE( frag_tx1_fixed );
	vk.modules.frag.fixed[1][1] = SHADER_MODULE( frag_tx1_fixed_fog );
	for ( i = 0; i < 2; i++ ) {
		const char *tx[] = { "single", "double" };
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			const char *s = va( "%s-texture fixed-color%s fragment module", tx[i], fog[j] );
			SET_OBJECT_NAME( vk.modules.frag.fixed[i][j], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
		}
	}

	vk.modules.frag.ent[0][0] = SHADER_MODULE( frag_tx0_ent );
	vk.modules.frag.ent[0][1] = SHADER_MODULE( frag_tx0_ent_fog );
	//vk.modules.frag.ent[1][0] = SHADER_MODULE( frag_tx1_ent );
	//vk.modules.frag.ent[1][1] = SHADER_MODULE( frag_tx1_ent_fog );
	for ( i = 0; i < 1; i++ ) {
		const char *tx[] = { "single" /*, "double" */};
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			const char *s = va( "%s-texture entity-color%s fragment module", tx[i], fog[j] );
			SET_OBJECT_NAME( vk.modules.frag.ent[i][j], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
		}
	}
#endif

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
    vk.modules.brdflut_fs = SHADER_MODULE(brdflut_frag_spv);
    SET_OBJECT_NAME(vk.modules.brdflut_fs, "brdf LUT fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
#endif

	vk.modules.filtercube_vs = SHADER_MODULE(filtercube_vert_spv);
    SET_OBJECT_NAME(vk.modules.filtercube_vs, "filter cube vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
    vk.modules.prefilterenvmap_fs = SHADER_MODULE(prefilterenvmap_frag_spv);
    SET_OBJECT_NAME(vk.modules.prefilterenvmap_fs, "prefilter env map fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
    vk.modules.irradiancecube_fs = SHADER_MODULE(irradiancecube_frag_spv);
    SET_OBJECT_NAME(vk.modules.irradiancecube_fs, "irradiance cube fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
    vk.modules.filtercube_gm = SHADER_MODULE(filtercube_geom_spv);
    SET_OBJECT_NAME(vk.modules.filtercube_gm, "filter cube geometry shader", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
}


static void vk_alloc_persistent_pipelines( void )
{
	unsigned int state_bits;
	Vk_Pipeline_Def def;

	// skybox
	{
		Com_Memset(&def, 0, sizeof(def));
		def.shader_type = TYPE_SIGNLE_TEXTURE_FIXED_COLOR;
		def.color.rgb = tr.identityLightByte;
		def.color.alpha = tr.identityLightByte;
		def.face_culling = CT_FRONT_SIDED;
		def.polygon_offset = qfalse;
		def.mirror = qfalse;
		vk.skybox_pipeline = vk_find_pipeline_ext( 0, &def, qtrue );
	}

	// stencil shadows (polygon offset pushes volume forward to reduce thin black lines)
	{
		cullType_t cull_types[2] = { CT_FRONT_SIDED, CT_BACK_SIDED };
		qboolean mirror_flags[2] = { qfalse, qtrue };
		int i, j;

		Com_Memset(&def, 0, sizeof(def));
		def.polygon_offset = qtrue;
		def.state_bits = 0;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.shadow_phase = SHADOW_EDGES;

		for (i = 0; i < 2; i++) {
			def.face_culling = cull_types[i];
			for (j = 0; j < 2; j++) {
				def.mirror = mirror_flags[j];
				vk.shadow_volume_pipelines[i][j] = vk_find_pipeline_ext( 0, &def, r_shadows->integer ? qtrue: qfalse );
			}
		}
	}
	{
		Com_Memset( &def, 0, sizeof( def ) );
		def.face_culling = CT_FRONT_SIDED;
		def.polygon_offset = qfalse;
		def.state_bits = GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.mirror = qfalse;
		def.shadow_phase = SHADOW_FS_QUAD;
		def.primitives = TRIANGLE_STRIP;
		vk.shadow_finish_pipeline = vk_find_pipeline_ext( 0, &def, r_shadows->integer ? qtrue: qfalse );
	}

	// fog and dlights
	{
		unsigned int fog_state_bits[2] = {
			GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_EQUAL, // fogPass == FP_EQUAL
			GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA // fogPass == FP_LE
		};
		unsigned int dlight_state_bits[2] = {
			GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL,	// modulated
			GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL			// additive
		};
		qboolean polygon_offset[2] = { qfalse, qtrue };
		int i, j, k;
#ifdef USE_PMLIGHT
		int l;
#endif

		Com_Memset(&def, 0, sizeof(def));
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.mirror = qfalse;

		for ( i = 0; i < 2; i++ ) {
			unsigned fog_state = fog_state_bits[ i ];
			unsigned dlight_state = dlight_state_bits[ i ];

			for ( j = 0; j < 3; j++ ) {
				def.face_culling = j; // cullType_t value

				for ( k = 0; k < 2; k++ ) {
					def.polygon_offset = polygon_offset[ k ];
#ifdef USE_FOG_ONLY
					def.shader_type = TYPE_FOG_ONLY;
#else
					def.shader_type = TYPE_SIGNLE_TEXTURE;
#endif
					def.state_bits = fog_state;
					vk.fog_pipelines[ i ][ j ][ k ] = vk_find_pipeline_ext( 0, &def, qtrue );

					def.shader_type = TYPE_SIGNLE_TEXTURE;
					def.state_bits = dlight_state;
#ifdef USE_LEGACY_DLIGHTS
#ifdef USE_PMLIGHT
					vk.dlight_pipelines[ i ][ j ][ k ] = vk_find_pipeline_ext( 0, &def, r_dlightMode->integer == 0 ? qtrue : qfalse );
#else
					vk.dlight_pipelines[ i ][ j ][ k ] = vk_find_pipeline_ext( 0, &def, qtrue );
#endif
#endif
				}
			}
		}

#ifdef USE_PMLIGHT
		def.state_bits = GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL;
		//def.shader_type = TYPE_SIGNLE_TEXTURE_LIGHTING;
		for (i = 0; i < 3; i++) { // cullType
			def.face_culling = i;
			for ( j = 0; j < 2; j++ ) { // polygonOffset
				def.polygon_offset = polygon_offset[j];
				for ( k = 0; k < 2; k++ ) {
					def.fog_stage = k; // fogStage
					for ( l = 0; l < 2; l++ ) {
						def.abs_light = l;
						def.shader_type = TYPE_SIGNLE_TEXTURE_LIGHTING;
						vk.dlight_pipelines_x[i][j][k][l] = vk_find_pipeline_ext( 0, &def, qfalse );
						def.shader_type = TYPE_SIGNLE_TEXTURE_LIGHTING_LINEAR;
						vk.dlight1_pipelines_x[i][j][k][l] = vk_find_pipeline_ext( 0, &def, qfalse );
					}
				}
			}
		}
#endif // USE_PMLIGHT
	}

	// RT_BEAM surface
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
		def.face_culling = CT_FRONT_SIDED;
		def.primitives = TRIANGLE_STRIP;
		vk.surface_beam_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// axis for missing models
	{
		Com_Memset( &def, 0, sizeof( def ) );
		def.state_bits = GLS_DEFAULT;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.face_culling = CT_TWO_SIDED;
		def.primitives = LINE_LIST;
		if ( vk.wideLines )
			def.line_width = 3;
		vk.surface_axis_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// flare visibility test dot
	if ( vk.fragmentStores )
	{
		Com_Memset( &def, 0, sizeof( def ) );
		//def.state_bits = GLS_DEFAULT;
		def.face_culling = CT_TWO_SIDED;
		def.shader_type = TYPE_DOT;
		def.primitives = POINT_LIST;
		vk.dot_pipeline = vk_find_pipeline_ext( 0, &def, qtrue );
	}

	// occlusion culling: bbox pipeline (depth test on, depth/color write off)
	{
		Com_Memset( &def, 0, sizeof( def ) );
		def.state_bits = 0;  /* depth test on, depth write off */
		def.face_culling = CT_FRONT_SIDED;
		def.shader_type = TYPE_OCCLUSION_BBOX;
		def.primitives = TRIANGLE_LIST;
		vk.occlusion_bbox_pipeline = vk_find_pipeline_ext( 0, &def, qtrue );
	}

	// DrawTris()
	state_bits = GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE;
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_WHITE;
		def.face_culling = CT_FRONT_SIDED;
		vk.tris_debug_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_WHITE;
		def.face_culling = CT_BACK_SIDED;
		vk.tris_mirror_debug_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_GREEN;
		def.face_culling = CT_FRONT_SIDED;
		vk.tris_debug_green_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_GREEN;
		def.face_culling = CT_BACK_SIDED;
		vk.tris_mirror_debug_green_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_RED;
		def.face_culling = CT_FRONT_SIDED;
		vk.tris_debug_red_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_RED;
		def.face_culling = CT_BACK_SIDED;
		vk.tris_mirror_debug_red_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// DrawNormals()
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = GLS_DEPTHMASK_TRUE;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.primitives = LINE_LIST;
		vk.normals_debug_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// RB_DebugPolygon()
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		vk.surface_debug_pipeline_solid = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.primitives = LINE_LIST;
		vk.surface_debug_pipeline_outline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// RB_ShowImages
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.primitives = TRIANGLE_STRIP;
		vk.images_debug_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );

		def.state_bits = GLS_DEPTHTEST_DISABLE;
		def.shader_type = TYPE_COLOR_BLACK;
		def.primitives = TRIANGLE_STRIP;
		vk.images_debug_pipeline2 = vk_find_pipeline_ext( 0, &def, qfalse );
	}
}

void vk_create_blur_pipeline( uint32_t index, uint32_t width, uint32_t height, qboolean horizontal_pass );
static void vk_create_atmosphere_pipeline( void );
static void vk_create_oit_accum_pipeline( void );

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


typedef struct vk_attach_desc_s  {
	VkImage descriptor;
	VkImageView *image_view;
	VkImageViewType viewType;
	VkImageUsageFlags usage;
	VkMemoryRequirements reqs;
	uint32_t memoryTypeIndex;
	VkDeviceSize  memory_offset;
	// for layout transition:
	VkImageAspectFlags aspect_flags;
	VkImageLayout image_layout;
	VkFormat image_format;
} vk_attach_desc_t;

static vk_attach_desc_t attachments[ MAX_ATTACHMENTS_IN_POOL ];
static uint32_t num_attachments = 0;


static void vk_clear_attachment_pool( void )
{
	num_attachments = 0;
}


static void vk_alloc_attachments( void )
{
	VkImageViewCreateInfo view_desc;
	VkMemoryDedicatedAllocateInfoKHR alloc_info2;
	VkMemoryAllocateInfo alloc_info;
	VkCommandBuffer command_buffer;
	VkDeviceMemory memory;
	VkDeviceSize offset;
	uint32_t memoryTypeBits;
	uint32_t memoryTypeIndex;
	uint32_t i;
	int layer;

	if ( num_attachments == 0 ) {
		return;
	}

	if ( vk.image_memory_count >= ARRAY_LEN( vk.image_memory ) ) {
		ri.Error( ERR_DROP, "vk.image_memory_count == %i", (int)ARRAY_LEN( vk.image_memory ) );
	}

	memoryTypeBits = ~0U;
	offset = 0;

	for ( i = 0; i < num_attachments; i++ ) {
#ifdef MIN_IMAGE_ALIGN
		VkDeviceSize alignment = MAX( attachments[ i ].reqs.alignment, MIN_IMAGE_ALIGN );
#else
		VkDeviceSize alignment = attachments[ i ].reqs.alignment;
#endif
		memoryTypeBits &= attachments[ i ].reqs.memoryTypeBits;
		offset = PAD( offset, alignment );
		attachments[ i ].memory_offset = offset;
		offset += attachments[ i ].reqs.size;
#ifdef _DEBUG
		ri.Printf( PRINT_ALL, S_COLOR_CYAN "[%i] type %i, size %i, align %i\n", i,
			attachments[ i ].reqs.memoryTypeBits,
			(int)attachments[ i ].reqs.size,
			(int)attachments[ i ].reqs.alignment );
#endif
	}

	if ( num_attachments == 1 && attachments[ 0 ].usage & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT ) {
		// try lazy memory
		memoryTypeIndex = vk_find_memory_type2( vk.physical_device, memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT, NULL );
		if ( memoryTypeIndex == ~0U ) {
			memoryTypeIndex = vk_find_memory_type( vk.physical_device, memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		}
	} else {
		memoryTypeIndex = vk_find_memory_type( vk.physical_device, memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	}

#ifdef _DEBUG
	ri.Printf( PRINT_ALL, "memory type bits: %04x\n", memoryTypeBits );
	ri.Printf( PRINT_ALL, "memory type index: %04x\n", memoryTypeIndex );
	ri.Printf( PRINT_ALL, "total size: %i\n", (int)offset );
#endif

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = offset;
	alloc_info.memoryTypeIndex = memoryTypeIndex;

	if ( num_attachments == 1 ) {
		if ( vk.dedicatedAllocation ) {
			Com_Memset( &alloc_info2, 0, sizeof( alloc_info2 ) );
			alloc_info2.sType =  VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR;
			alloc_info2.image = attachments[ 0 ].descriptor;
			alloc_info.pNext = &alloc_info2;
		}
	}

	// allocate and bind memory
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &memory ) );

	vk.image_memory[ vk.image_memory_count++ ] = memory;

	for ( i = 0; i < num_attachments; i++ ) {
		VkImageViewType viewType = attachments[i].viewType; // preserve original type

		VK_CHECK( qvkBindImageMemory( vk.device, attachments[i].descriptor, memory, attachments[i].memory_offset ) );
        
		layer = 0;
        while ( qtrue ) {
            // create color image view
            view_desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            view_desc.pNext = NULL;
            view_desc.flags = 0;
            view_desc.image = attachments[i].descriptor;
            view_desc.viewType = viewType;
            view_desc.format = attachments[i].image_format;
            view_desc.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            view_desc.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            view_desc.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            view_desc.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            view_desc.subresourceRange.aspectMask = attachments[i].aspect_flags;
            view_desc.subresourceRange.baseMipLevel = 0;
            view_desc.subresourceRange.levelCount = 1;
            view_desc.subresourceRange.baseArrayLayer = MAX( ( layer - 1 ), 0 );
            view_desc.subresourceRange.layerCount = ( viewType == VK_IMAGE_VIEW_TYPE_CUBE ) ? 6 : 1;

            VK_CHECK(qvkCreateImageView(vk.device, &view_desc, NULL, attachments[i].image_view + layer));
        
            // discard if not a cube or the 6th face/layer view has been created
            if ( attachments[i].viewType != VK_IMAGE_VIEW_TYPE_CUBE || layer == 6 )
                break;

            // create a view for each face/layer with view type VK_IMAGE_VIEW_TYPE_2D
            viewType = VK_IMAGE_VIEW_TYPE_2D;
            layer++;
        }
	}

	// perform layout transition
	command_buffer = vk_begin_command_buffer();
	for ( i = 0; i < num_attachments; i++ ) {
		record_image_layout_transition( command_buffer,
			attachments[i].descriptor,
			attachments[i].aspect_flags,
			VK_IMAGE_LAYOUT_UNDEFINED, // old_layout
			attachments[i].image_layout,
			0, 0 );
	}
	vk_end_command_buffer( command_buffer, __func__ );

	num_attachments = 0;
}


static void vk_add_attachment_desc( VkImage desc, VkImageView *image_view, VkImageUsageFlags usage, VkMemoryRequirements *reqs, VkFormat image_format, VkImageAspectFlags aspect_flags, VkImageLayout image_layout
#ifdef USE_VK_PBR
	, VkImageViewType view_type )
#endif
{
	if ( num_attachments >= ARRAY_LEN( attachments ) ) {
		ri.Error( ERR_FATAL, "Attachments array overflow" );
	} else {
		attachments[ num_attachments ].descriptor = desc;
		attachments[ num_attachments ].image_view = image_view;
		attachments[ num_attachments ].viewType = view_type;
		attachments[ num_attachments ].usage = usage;
		attachments[ num_attachments ].reqs = *reqs;
		attachments[ num_attachments ].aspect_flags = aspect_flags;
		attachments[ num_attachments ].image_layout = image_layout;
		attachments[ num_attachments ].image_format = image_format;
		attachments[ num_attachments ].memory_offset = 0;
		num_attachments++;
	}
}


static void vk_get_image_memory_erquirements( VkImage image, VkMemoryRequirements *memory_requirements )
{
	if ( vk.dedicatedAllocation ) {
		VkMemoryRequirements2KHR memory_requirements2;
		VkImageMemoryRequirementsInfo2KHR image_requirements2;
		VkMemoryDedicatedRequirementsKHR mem_req2;

		Com_Memset( &mem_req2, 0, sizeof( mem_req2 ) );
		mem_req2.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS_KHR;

		image_requirements2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2_KHR;
		image_requirements2.image = image;
		image_requirements2.pNext = NULL;

		Com_Memset( &memory_requirements2, 0, sizeof( memory_requirements2 ) );
		memory_requirements2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2_KHR;
		memory_requirements2.pNext = &mem_req2;

		qvkGetImageMemoryRequirements2KHR( vk.device, &image_requirements2, &memory_requirements2 );

		*memory_requirements = memory_requirements2.memoryRequirements;
	} else {
		qvkGetImageMemoryRequirements( vk.device, image, memory_requirements );
	}
}


static void create_color_attachment( 
	uint32_t width, uint32_t height, 
	VkSampleCountFlagBits samples, VkFormat format,
	VkImageUsageFlags usage, VkImage *image, 
	VkImageView *image_view, VkImageLayout image_layout, 
	qboolean multisample, VkImageCreateFlags flags )
{
	VkImageCreateInfo create_desc;
	VkMemoryRequirements memory_requirements;

	if ( multisample && !( usage & VK_IMAGE_USAGE_SAMPLED_BIT ) )
		usage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;

	// create color image
	create_desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	create_desc.pNext = NULL;
	create_desc.flags = flags;
	create_desc.imageType = VK_IMAGE_TYPE_2D;
	create_desc.format = format;
	create_desc.extent.width = width;
	create_desc.extent.height = height;
	create_desc.extent.depth = 1;
	create_desc.mipLevels = 1;
	create_desc.arrayLayers = ( flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT ) ? 6 : 1;
	create_desc.samples = samples;
	create_desc.tiling = VK_IMAGE_TILING_OPTIMAL;
	create_desc.usage = usage;
	create_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	create_desc.queueFamilyIndexCount = 0;
	create_desc.pQueueFamilyIndices = NULL;
	create_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VK_CHECK( qvkCreateImage( vk.device, &create_desc, NULL, image ) );

	vk_get_image_memory_erquirements( *image, &memory_requirements );

#ifdef USE_VK_PBR
    VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D;

	if ( flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT )
		view_type = VK_IMAGE_VIEW_TYPE_CUBE;
	vk_add_attachment_desc( *image, image_view, usage, &memory_requirements, format, VK_IMAGE_ASPECT_COLOR_BIT, image_layout, view_type );
#else
	vk_add_attachment_desc( *image, image_view, usage, &memory_requirements, format, VK_IMAGE_ASPECT_COLOR_BIT, image_layout );
#endif
}


static void create_depth_attachment( uint32_t width, uint32_t height, VkSampleCountFlagBits samples, VkImage *image, VkImageView *image_view, qboolean allowTransient )
{
	VkImageCreateInfo create_desc;
	VkMemoryRequirements memory_requirements;
	VkImageAspectFlags image_aspect_flags;
	const qboolean sampledDepth = qtrue;

	// create depth image
	create_desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	create_desc.pNext = NULL;
	create_desc.flags = 0;
	create_desc.imageType = VK_IMAGE_TYPE_2D;
	create_desc.format = vk.depth_format;
	create_desc.extent.width = width;
	create_desc.extent.height = height;
	create_desc.extent.depth = 1;
	create_desc.mipLevels = 1;
	create_desc.arrayLayers = 1;
	create_desc.samples = samples;
	create_desc.tiling = VK_IMAGE_TILING_OPTIMAL;
	create_desc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	if ( sampledDepth ) {
		create_desc.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
	}
	if ( allowTransient && !sampledDepth ) {
		create_desc.usage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
	}
	create_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	create_desc.queueFamilyIndexCount = 0;
	create_desc.pQueueFamilyIndices = NULL;
	create_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	image_aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT;
	if ( glConfig.stencilBits > 0 )
		image_aspect_flags |= VK_IMAGE_ASPECT_STENCIL_BIT;

	VK_CHECK( qvkCreateImage( vk.device, &create_desc, NULL, image ) );

	vk_get_image_memory_erquirements( *image, &memory_requirements );

#ifdef USE_VK_PBR
	vk_add_attachment_desc( *image, image_view, create_desc.usage, &memory_requirements, vk.depth_format, image_aspect_flags, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_VIEW_TYPE_2D );
#else
	vk_add_attachment_desc( *image, image_view, create_desc.usage, &memory_requirements, vk.depth_format, image_aspect_flags, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL );
#endif
}

static void vk_create_depth_sample_view( void )
{
	VkImageViewCreateInfo view_desc;

	if ( vk.depth_image_view_sample != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.depth_image_view_sample, NULL );
		vk.depth_image_view_sample = VK_NULL_HANDLE;
	}

	if ( vk.depth_image == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( &view_desc, 0, sizeof( view_desc ) );
	view_desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_desc.image = vk.depth_image;
	view_desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_desc.format = vk.depth_format;
	view_desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	view_desc.subresourceRange.baseMipLevel = 0;
	view_desc.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	view_desc.subresourceRange.baseArrayLayer = 0;
	view_desc.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	VK_CHECK( qvkCreateImageView( vk.device, &view_desc, NULL, &vk.depth_image_view_sample ) );
	SET_OBJECT_NAME( vk.depth_image_view_sample, "depth sample view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
}

static void vk_create_depth_only_image_view( VkImage image, VkFormat format, VkImageViewType view_type,
	uint32_t base_array_layer, uint32_t layer_count, VkImageView *out_view, const char *name )
{
	VkImageViewCreateInfo view_desc;

	if ( out_view == NULL ) {
		return;
	}
	if ( *out_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, *out_view, NULL );
		*out_view = VK_NULL_HANDLE;
	}
	if ( image == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( &view_desc, 0, sizeof( view_desc ) );
	view_desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_desc.image = image;
	view_desc.viewType = view_type;
	view_desc.format = format;
	view_desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	view_desc.subresourceRange.baseMipLevel = 0;
	view_desc.subresourceRange.levelCount = 1;
	view_desc.subresourceRange.baseArrayLayer = base_array_layer;
	view_desc.subresourceRange.layerCount = layer_count;

	VK_CHECK( qvkCreateImageView( vk.device, &view_desc, NULL, out_view ) );
	if ( name != NULL ) {
		SET_OBJECT_NAME( *out_view, name, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	}
}


static void vk_create_attachments( void )
{
	uint32_t i;

	vk_clear_attachment_pool();
	vk_create_volumetric_params_buffer();

	// It looks like resulting performance depends from order you're creating/allocating
	// memory for attachments in vulkan i.e. similar images grouped together will provide best results
	// so [resolve0][resolve1][msaa0][msaa1][depth0][depth1] is most optimal
	// while cases like [resolve0][depth0][color0][...] is the worst

	/* Note: Could preallocate first image chunk in attachment memory pool. */
	if ( vk.fboActive ) {

		const VkImageUsageFlags sampledColorUsage =
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		const VkImageUsageFlags copyableColorUsage =
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		// bloom
		if ( r_bloom->integer ) {
			uint32_t width = gls.captureWidth;
			uint32_t height = gls.captureHeight;
			VkImageUsageFlags bloomUsage = copyableColorUsage;

			create_color_attachment( width, height, VK_SAMPLE_COUNT_1_BIT, vk.bloom_format,
				bloomUsage, &vk.bloom_image[0], &vk.bloom_image_view[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse, 0 );

			for ( i = 1; i < ARRAY_LEN( vk.bloom_image ); i += 2 ) {
				width /= 2;
				height /= 2;
				create_color_attachment( width, height, VK_SAMPLE_COUNT_1_BIT, vk.bloom_format,
					bloomUsage, &vk.bloom_image[i+0], &vk.bloom_image_view[i+0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse, 0 );

				create_color_attachment( width, height, VK_SAMPLE_COUNT_1_BIT, vk.bloom_format,
					bloomUsage, &vk.bloom_image[i+1], &vk.bloom_image_view[i+1], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse, 0 );
			}
		}

		// ssao
		if ( r_ssao && r_ssao->integer ) {
			create_color_attachment( glConfig.vidWidth, glConfig.vidHeight, VK_SAMPLE_COUNT_1_BIT, vk.ssao_format,
				sampledColorUsage, &vk.ssao_image, &vk.ssao_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse, 0 );
			create_color_attachment( glConfig.vidWidth, glConfig.vidHeight, VK_SAMPLE_COUNT_1_BIT, vk.ssao_format,
				sampledColorUsage, &vk.ssao_blur_image, &vk.ssao_blur_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse, 0 );
		}
		if ( r_oit && r_oit->integer ) {
			create_color_attachment( glConfig.vidWidth, glConfig.vidHeight, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R16G16B16A16_SFLOAT,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				&vk.oit_accum_image, &vk.oit_accum_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse, 0 );
			create_color_attachment( glConfig.vidWidth, glConfig.vidHeight, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R16_SFLOAT,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				&vk.oit_reveal_image, &vk.oit_reveal_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse, 0 );
		}

		// ssr (same format as color)
		if ( PostFX_SSR_IsEnabled() ) {
			create_color_attachment( glConfig.vidWidth, glConfig.vidHeight, VK_SAMPLE_COUNT_1_BIT, vk.color_format,
				copyableColorUsage, &vk.ssr_image, &vk.ssr_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse, 0 );
		}

        // cubemap
        if ( vk.cubemapActive ) {
            create_color_attachment( REF_CUBEMAP_SIZE, REF_CUBEMAP_SIZE, VK_SAMPLE_COUNT_1_BIT, vk.color_format,
                sampledColorUsage, &vk.cubeMap.color_image, &vk.cubeMap.color_image_view[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT );

            if ( vk.msaaActive )
                create_color_attachment( REF_CUBEMAP_SIZE, REF_CUBEMAP_SIZE, (VkSampleCountFlagBits)vkSamples, vk.color_format,
                    sampledColorUsage, &vk.cubeMap.color_image_msaa, &vk.cubeMap.color_image_view_msaa[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qtrue, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT );
            
            create_depth_attachment( REF_CUBEMAP_SIZE, REF_CUBEMAP_SIZE, (VkSampleCountFlagBits)vkSamples,
                    &vk.cubeMap.depth_image, &vk.cubeMap.depth_image_view, qtrue );
        }

		// post-processing/msaa-resolve
			create_color_attachment( glConfig.vidWidth, glConfig.vidHeight, VK_SAMPLE_COUNT_1_BIT, vk.color_format,
				copyableColorUsage, &vk.color_image, &vk.color_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse, 0 );
			create_color_attachment( glConfig.vidWidth, glConfig.vidHeight, VK_SAMPLE_COUNT_1_BIT, vk.color_format,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				&vk.ui_overlay_image, &vk.ui_overlay_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse, 0 );
			// scene copy sampled by volumetric composite (avoids read/write feedback on vk.color_image)
				create_color_attachment( glConfig.vidWidth, glConfig.vidHeight, VK_SAMPLE_COUNT_1_BIT, vk.color_format,
					VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
					&vk.fog_scene_image, &vk.fog_scene_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse, 0 );
				create_color_attachment( glConfig.vidWidth, glConfig.vidHeight, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R32_SFLOAT,
					VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
					&vk.volumetric_depth_image, &vk.volumetric_depth_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse, 0 );
				create_color_attachment( glConfig.vidWidth, glConfig.vidHeight, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R16G16_SFLOAT,
					VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
					&vk.motion_vector_image, &vk.motion_vector_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse, 0 );
				if ( vk.msaaActive ) {
					create_color_attachment( glConfig.vidWidth, glConfig.vidHeight, vkSamples, VK_FORMAT_R16G16_SFLOAT,
						VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
						&vk.motion_vector_msaa_image, &vk.motion_vector_msaa_view, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, qtrue, 0 );
				}

		// screenmap-msaa
		if ( vk.screenMapSamples > VK_SAMPLE_COUNT_1_BIT ) {
			create_color_attachment( vk.screenMapWidth, vk.screenMapHeight, vk.screenMapSamples, vk.base_format.format,
				sampledColorUsage, &vk.screenMap.color_image_msaa, &vk.screenMap.color_image_view_msaa, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, qtrue, 0 );
			create_color_attachment( vk.screenMapWidth, vk.screenMapHeight, vk.screenMapSamples, VK_FORMAT_R16G16_SFLOAT,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &vk.screenMap.motion_image_msaa, &vk.screenMap.motion_image_view_msaa, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, qtrue, 0 );
		}

		// screenmap/msaa-resolve
		create_color_attachment( vk.screenMapWidth, vk.screenMapHeight, VK_SAMPLE_COUNT_1_BIT, vk.base_format.format,
			sampledColorUsage, &vk.screenMap.color_image, &vk.screenMap.color_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse, 0 );
		create_color_attachment( vk.screenMapWidth, vk.screenMapHeight, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R16G16_SFLOAT,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &vk.screenMap.motion_image, &vk.screenMap.motion_image_view, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, qtrue, 0 );

		// screenmap depth
		create_depth_attachment( vk.screenMapWidth, vk.screenMapHeight, vk.screenMapSamples, &vk.screenMap.depth_image, &vk.screenMap.depth_image_view, qtrue );

		if ( vk.msaaActive ) {
			create_color_attachment( glConfig.vidWidth, glConfig.vidHeight, vkSamples, vk.color_format,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &vk.msaa_image, &vk.msaa_image_view, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, qtrue, 0 );
			create_color_attachment( glConfig.vidWidth, glConfig.vidHeight, vkSamples, vk.color_format,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &vk.ui_overlay_msaa_image, &vk.ui_overlay_msaa_image_view, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, qtrue, 0 );
		}

		if ( r_ext_supersample->integer ) {
			// capture buffer
			create_color_attachment( gls.captureWidth, gls.captureHeight, VK_SAMPLE_COUNT_1_BIT, vk.capture_format,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
				&vk.capture.image, &vk.capture.image_view, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, qfalse, 0 );
		}

		if ( vk.smaaActive ) {
			VkImageUsageFlags smaaUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			create_color_attachment( glConfig.vidWidth, glConfig.vidHeight, VK_SAMPLE_COUNT_1_BIT, vk.color_format,
				smaaUsage, &vk.smaa_edge_image, &vk.smaa_edge_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse, 0 );
			create_color_attachment( glConfig.vidWidth, glConfig.vidHeight, VK_SAMPLE_COUNT_1_BIT, vk.color_format,
				smaaUsage, &vk.smaa_blend_image, &vk.smaa_blend_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse, 0 );
			create_color_attachment( glConfig.vidWidth, glConfig.vidHeight, VK_SAMPLE_COUNT_1_BIT, vk.color_format,
				smaaUsage, &vk.smaa_output_image, &vk.smaa_output_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse, 0 );
		}
		{
			VkImageUsageFlags taaUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			create_color_attachment( glConfig.vidWidth, glConfig.vidHeight, VK_SAMPLE_COUNT_1_BIT, vk.color_format,
				taaUsage, &vk.taa_history_image[0], &vk.taa_history_image_view[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse, 0 );
			create_color_attachment( glConfig.vidWidth, glConfig.vidHeight, VK_SAMPLE_COUNT_1_BIT, vk.color_format,
				taaUsage, &vk.taa_history_image[1], &vk.taa_history_image_view[1], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse, 0 );
		}

		/* Luminance 1x1 for eye adaptation (r_exposure_auto) */
		if ( vk.luminance_layout != VK_NULL_HANDLE ) {
			VkBufferCreateInfo buf_desc;
			VkMemoryRequirements mem_reqs;
			VkMemoryAllocateInfo alloc_info;
			VkImageUsageFlags lumUsage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
			create_color_attachment( 1, 1, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R32_SFLOAT,
				lumUsage, &vk.luminance_image, &vk.luminance_image_view, VK_IMAGE_LAYOUT_GENERAL, qfalse, 0 );
			/* Staging buffer for luminance readback (4 bytes) */
			if ( vk.luminance_staging_buffer != VK_NULL_HANDLE ) {
				qvkUnmapMemory( vk.device, vk.luminance_staging_memory );
				qvkDestroyBuffer( vk.device, vk.luminance_staging_buffer, NULL );
				qvkFreeMemory( vk.device, vk.luminance_staging_memory, NULL );
				vk.luminance_staging_buffer = VK_NULL_HANDLE;
				vk.luminance_staging_memory = VK_NULL_HANDLE;
				vk.luminance_staging_ptr = NULL;
			}
			Com_Memset( &buf_desc, 0, sizeof( buf_desc ) );
			buf_desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			buf_desc.size = 4;
			buf_desc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			buf_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			VK_CHECK( qvkCreateBuffer( vk.device, &buf_desc, NULL, &vk.luminance_staging_buffer ) );
			qvkGetBufferMemoryRequirements( vk.device, vk.luminance_staging_buffer, &mem_reqs );
			Com_Memset( &alloc_info, 0, sizeof( alloc_info ) );
			alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			alloc_info.allocationSize = mem_reqs.size;
			alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
			VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.luminance_staging_memory ) );
			VK_CHECK( qvkBindBufferMemory( vk.device, vk.luminance_staging_buffer, vk.luminance_staging_memory, 0 ) );
			VK_CHECK( qvkMapMemory( vk.device, vk.luminance_staging_memory, 0, 4, 0, &vk.luminance_staging_ptr ) );
		}

#ifdef VK_PBR_BRDFLUT
        // BRDF LUT
        if( vk.pbrActive ) {
            uint32_t size = 512;
            create_color_attachment( size, size, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R16G16_SFLOAT,
                sampledColorUsage, &vk.brdflut_image, &vk.brdflut_image_view , VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse, 0 );
        }
#endif

	} // if ( vk.fboActive )

	//vk_alloc_attachments();

	create_depth_attachment( glConfig.vidWidth, glConfig.vidHeight, vkSamples, &vk.depth_image, &vk.depth_image_view,
		(vk.fboActive && r_bloom->integer) || (r_ssao && r_ssao->integer) || (PostFX_SSR_IsEnabled()) ? qfalse : qtrue );
	vk.depth_image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	vk_alloc_attachments();
	vk_create_depth_sample_view();
	vk_create_sun_shadow_resources();
	vk_create_local_shadow_resources();

	vk_create_froxel_images();

	for ( i = 0; i < vk.image_memory_count; i++ )
	{
		SET_OBJECT_NAME( vk.image_memory[i], va( "framebuffer memory chunk %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT );
	}

	SET_OBJECT_NAME( vk.depth_image, "depth attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	SET_OBJECT_NAME( vk.depth_image_view, "depth attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );

		SET_OBJECT_NAME( vk.color_image, "color attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.color_image_view, "color attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
		SET_OBJECT_NAME( vk.ui_overlay_image, "ui overlay attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.ui_overlay_image_view, "ui overlay attachment view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
		SET_OBJECT_NAME( vk.fog_scene_image, "fog scene copy", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.fog_scene_image_view, "fog scene copy view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
			SET_OBJECT_NAME( vk.volumetric_depth_image, "volumetric depth resolve", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
			SET_OBJECT_NAME( vk.volumetric_depth_view, "volumetric depth resolve view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
			SET_OBJECT_NAME( vk.motion_vector_image, "volumetric motion vectors", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
			SET_OBJECT_NAME( vk.motion_vector_view, "volumetric motion vectors view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
			for ( int fluid_idx = 0; fluid_idx < 2; fluid_idx++ ) {
				SET_OBJECT_NAME( vk.fluid_velocity_images[fluid_idx], va( "fluid velocity %d", fluid_idx ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
				SET_OBJECT_NAME( vk.fluid_velocity_views[fluid_idx], va( "fluid velocity view %d", fluid_idx ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
				SET_OBJECT_NAME( vk.fluid_density_images[fluid_idx], va( "fluid density %d", fluid_idx ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
				SET_OBJECT_NAME( vk.fluid_density_views[fluid_idx], va( "fluid density view %d", fluid_idx ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
				SET_OBJECT_NAME( vk.fluid_pressure_images[fluid_idx], va( "fluid pressure %d", fluid_idx ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
				SET_OBJECT_NAME( vk.fluid_pressure_views[fluid_idx], va( "fluid pressure view %d", fluid_idx ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
			}
			SET_OBJECT_NAME( vk.fluid_divergence_image, "fluid divergence", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
			SET_OBJECT_NAME( vk.fluid_divergence_view, "fluid divergence view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
			if ( vk.motion_vector_msaa_image ) {
				SET_OBJECT_NAME( vk.motion_vector_msaa_image, "volumetric motion vectors msaa", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
			}
			if ( vk.motion_vector_msaa_view ) {
				SET_OBJECT_NAME( vk.motion_vector_msaa_view, "volumetric motion vectors msaa view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
			}
			if ( vk.ui_overlay_msaa_image ) {
				SET_OBJECT_NAME( vk.ui_overlay_msaa_image, "ui overlay attachment msaa", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
			}
			if ( vk.ui_overlay_msaa_image_view ) {
				SET_OBJECT_NAME( vk.ui_overlay_msaa_image_view, "ui overlay attachment msaa view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
			}
			SET_OBJECT_NAME( vk.sun_shadow_image, "sun shadow depth", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	SET_OBJECT_NAME( vk.sun_shadow_view, "sun shadow depth view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	SET_OBJECT_NAME( vk.sun_shadow_color_image, "sun shadow color", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	SET_OBJECT_NAME( vk.sun_shadow_color_view, "sun shadow color view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	if ( vk.sun_shadow_color_msaa_image ) {
		SET_OBJECT_NAME( vk.sun_shadow_color_msaa_image, "sun shadow color msaa", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	}
	if ( vk.sun_shadow_color_msaa_view ) {
		SET_OBJECT_NAME( vk.sun_shadow_color_msaa_view, "sun shadow color msaa view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	}
	SET_OBJECT_NAME( vk.local_spot_shadow_atlas_image, "local spot shadow atlas depth", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	SET_OBJECT_NAME( vk.local_spot_shadow_atlas_view, "local spot shadow atlas depth view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	SET_OBJECT_NAME( vk.local_spot_shadow_color_image, "local spot shadow atlas color", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	SET_OBJECT_NAME( vk.local_spot_shadow_color_view, "local spot shadow atlas color view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	SET_OBJECT_NAME( vk.local_point_shadow_array_image, "local point shadow array depth", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	SET_OBJECT_NAME( vk.local_point_shadow_array_view, "local point shadow array depth view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	SET_OBJECT_NAME( vk.local_point_shadow_color_array_image, "local point shadow array color", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	SET_OBJECT_NAME( vk.local_point_shadow_color_array_view, "local point shadow array color view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );

	SET_OBJECT_NAME( vk.capture.image, "capture image", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	SET_OBJECT_NAME( vk.capture.image_view, "capture image view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );

	if ( vk.smaa_edge_image ) {
		SET_OBJECT_NAME( vk.smaa_edge_image, "smaa edge image", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.smaa_edge_image_view, "smaa edge image view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	}
	if ( vk.smaa_blend_image ) {
		SET_OBJECT_NAME( vk.smaa_blend_image, "smaa blend image", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.smaa_blend_image_view, "smaa blend image view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	}
	if ( vk.smaa_output_image ) {
		SET_OBJECT_NAME( vk.smaa_output_image, "smaa output image", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.smaa_output_image_view, "smaa output image view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	}
	for ( i = 0; i < 2; i++ ) {
		if ( vk.taa_history_image[i] ) {
			SET_OBJECT_NAME( vk.taa_history_image[i], va( "taa history image %d", i ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
			SET_OBJECT_NAME( vk.taa_history_image_view[i], va( "taa history image view %d", i ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
		}
	}

	for ( i = 0; i < ARRAY_LEN( vk.bloom_image ); i++ )
	{
		SET_OBJECT_NAME( vk.bloom_image[i], va( "bloom attachment %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.bloom_image_view[i], va( "bloom attachment %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	}

	if ( vk.ssao_image ) {
		SET_OBJECT_NAME( vk.ssao_image, "ssao attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.ssao_image_view, "ssao attachment view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	}

	if ( vk.ssao_blur_image ) {
		SET_OBJECT_NAME( vk.ssao_blur_image, "ssao blur attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.ssao_blur_image_view, "ssao blur attachment view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	}

#ifdef VK_PBR_BRDFLUT
    SET_OBJECT_NAME( vk.brdflut_image, "brdf lut image", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
    SET_OBJECT_NAME( vk.brdflut_image_view, "brdf lut image view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
#endif

    SET_OBJECT_NAME( vk.cubeMap.color_image, "cubemap image", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
    SET_OBJECT_NAME( vk.cubeMap.color_image_msaa, "cubemap msaa image", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );

    for ( i = 0; i < ARRAY_LEN(vk.cubeMap.color_image_view); i++) {
        SET_OBJECT_NAME( vk.cubeMap.color_image_view[i], va("cubemap image view %i", i), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
        SET_OBJECT_NAME( vk.cubeMap.color_image_view_msaa[i], va("cubemap face view msaa %i",i), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
    }

    SET_OBJECT_NAME( vk.cubeMap.depth_image, "cubemap depth image", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
    SET_OBJECT_NAME( vk.cubeMap.depth_image_view, "cubemap depth image view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );

}


static void vk_create_framebuffers( void )
{
	VkImageView framebuffer_attachments[5];
	VkFramebufferCreateInfo desc;
	uint32_t n;

	desc.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.pAttachments = framebuffer_attachments;
	desc.layers = 1;

	for ( n = 0; (uint32_t) n < vk.swapchain_image_count; n++ )
	{
		desc.renderPass = vk.render_pass.main;
		desc.attachmentCount = 2;
		if ( !vk.fboActive )
		{
			desc.width = gls.windowWidth;
			desc.height = gls.windowHeight;
			framebuffer_attachments[0] = vk.swapchain_image_views[n];
			framebuffer_attachments[1] = vk.depth_image_view;
			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.main[n] ) );

			SET_OBJECT_NAME( vk.framebuffers.main[n], va( "framebuffer - main %i", n ), VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
		}
		else
		{
			// same framebuffer configuration for main and post-bloom render passes
			if ( n == 0 )
			{
				desc.width = glConfig.vidWidth;
				desc.height = glConfig.vidHeight;
				framebuffer_attachments[0] = vk.color_image_view;
				framebuffer_attachments[1] = vk.depth_image_view;
				framebuffer_attachments[2] = vk.motion_vector_view;
				desc.attachmentCount = 3;
				if ( vk.msaaActive )
				{
					desc.attachmentCount = 5;
					framebuffer_attachments[3] = vk.msaa_image_view;
					framebuffer_attachments[4] = vk.motion_vector_msaa_view;
				}
				VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.main[n] ) );
				SET_OBJECT_NAME( vk.framebuffers.main[n], "framebuffer - main", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
			}
			else
			{
				vk.framebuffers.main[n] = vk.framebuffers.main[0];
			}
			if ( vk.render_pass.atmosphere != VK_NULL_HANDLE && n == 0 ) {
				desc.renderPass = vk.render_pass.atmosphere;
				desc.attachmentCount = 2;
				desc.width = glConfig.vidWidth;
				desc.height = glConfig.vidHeight;
				framebuffer_attachments[0] = vk.msaaActive ? vk.msaa_image_view : vk.color_image_view;
				framebuffer_attachments[1] = vk.depth_image_view;
				VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.atmosphere[0] ) );
				SET_OBJECT_NAME( vk.framebuffers.atmosphere[0], "framebuffer - atmosphere", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
			}
			if ( vk.render_pass.atmosphere != VK_NULL_HANDLE ) {
				vk.framebuffers.atmosphere[n] = vk.framebuffers.atmosphere[0];
			} else {
				vk.framebuffers.atmosphere[n] = VK_NULL_HANDLE;
			}

			// gamma correction: use swapchain extent so framebuffer matches swapchain image dimensions
			desc.renderPass = vk.render_pass.gamma;
			desc.attachmentCount = 1;
			desc.width = vk.swapchain_extent_valid ? vk.swapchain_extent.width : (uint32_t)gls.windowWidth;
			desc.height = vk.swapchain_extent_valid ? vk.swapchain_extent.height : (uint32_t)gls.windowHeight;
			if ( desc.width == 0 ) desc.width = (uint32_t)glConfig.vidWidth;
			if ( desc.height == 0 ) desc.height = (uint32_t)glConfig.vidHeight;
			framebuffer_attachments[0] = vk.swapchain_image_views[n];
			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.gamma[n] ) );

			SET_OBJECT_NAME( vk.framebuffers.gamma[n], "framebuffer - gamma-correction", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );

			desc.renderPass = vk.render_pass.overlay_compose;
			desc.attachmentCount = 1;
			framebuffer_attachments[0] = vk.swapchain_image_views[n];
			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.overlay_compose[n] ) );
			SET_OBJECT_NAME( vk.framebuffers.overlay_compose[n], "framebuffer - overlay compose", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
		}
	}

	if ( vk.fboActive )
	{
		if ( vk.render_pass.ui_overlay != VK_NULL_HANDLE && vk.ui_overlay_image_view != VK_NULL_HANDLE ) {
			desc.renderPass = vk.render_pass.ui_overlay;
			desc.width = glConfig.vidWidth;
			desc.height = glConfig.vidHeight;
			desc.attachmentCount = 3;
			framebuffer_attachments[0] = vk.ui_overlay_image_view;
			framebuffer_attachments[1] = vk.depth_image_view;
			framebuffer_attachments[2] = vk.motion_vector_view;
			if ( vk.msaaActive ) {
				desc.attachmentCount = 5;
				framebuffer_attachments[3] = vk.ui_overlay_msaa_image_view;
				framebuffer_attachments[4] = vk.motion_vector_msaa_view;
			}
			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.ui_overlay[0] ) );
			SET_OBJECT_NAME( vk.framebuffers.ui_overlay[0], "framebuffer - ui overlay", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
			for ( n = 1; (uint32_t)n < vk.swapchain_image_count; n++ ) {
				vk.framebuffers.ui_overlay[n] = vk.framebuffers.ui_overlay[0];
			}
		}

		// screenmap
		desc.renderPass = vk.render_pass.screenmap;
		desc.attachmentCount = 3;
		desc.width = vk.screenMapWidth;
		desc.height = vk.screenMapHeight;
		framebuffer_attachments[0] = vk.screenMap.color_image_view;
		framebuffer_attachments[1] = vk.screenMap.depth_image_view;
		framebuffer_attachments[2] = vk.screenMap.motion_image_view;
		if ( vk.screenMapSamples > VK_SAMPLE_COUNT_1_BIT )
		{
			desc.attachmentCount = 5;
			framebuffer_attachments[2] = vk.screenMap.motion_image_view;
			framebuffer_attachments[3] = vk.screenMap.color_image_view_msaa;
			framebuffer_attachments[4] = vk.screenMap.motion_image_view_msaa;
		}
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.screenmap ) );
		SET_OBJECT_NAME( vk.framebuffers.screenmap, "framebuffer - screenmap", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );

		// sun shadow map framebuffer (single-cascade, resolved depth path)
		if ( vk.sun_shadow_image != VK_NULL_HANDLE && vk.sun_shadow_color_image != VK_NULL_HANDLE ) {
			desc.renderPass = vk.render_pass.sun_shadow;
			desc.attachmentCount = 2;
			desc.width = vk.sun_shadow_width;
			desc.height = vk.sun_shadow_height;
			framebuffer_attachments[0] = vk.sun_shadow_color_view;
			framebuffer_attachments[1] = vk.sun_shadow_view;
			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.sun_shadow ) );
			SET_OBJECT_NAME( vk.framebuffers.sun_shadow, "framebuffer - sun shadow", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
		}

		if ( vk.local_spot_shadow_atlas_image != VK_NULL_HANDLE &&
			vk.local_spot_shadow_color_image != VK_NULL_HANDLE &&
			vk.render_pass.local_spot_shadow != VK_NULL_HANDLE )
		{
			desc.renderPass = vk.render_pass.local_spot_shadow;
			desc.attachmentCount = 2;
			desc.width = vk.local_spot_shadow_atlas_size;
			desc.height = vk.local_spot_shadow_atlas_size;
			framebuffer_attachments[0] = vk.local_spot_shadow_color_view;
			framebuffer_attachments[1] = vk.local_spot_shadow_atlas_view;
			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.local_spot_shadow ) );
			SET_OBJECT_NAME( vk.framebuffers.local_spot_shadow, "framebuffer - local spot shadow", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
		}

		if ( vk.local_point_shadow_array_image != VK_NULL_HANDLE &&
			vk.local_point_shadow_color_array_image != VK_NULL_HANDLE &&
			vk.render_pass.local_point_shadow != VK_NULL_HANDLE )
		{
			const uint32_t point_layers = vk.local_point_shadow_capacity * 6;
			desc.renderPass = vk.render_pass.local_point_shadow;
			desc.attachmentCount = 2;
			desc.width = vk.local_point_shadow_face_size;
			desc.height = vk.local_point_shadow_face_size;

			for ( uint32_t layer = 0; layer < point_layers && layer < ARRAY_LEN( vk.framebuffers.local_point_shadow ); layer++ ) {
				framebuffer_attachments[0] = vk.local_point_shadow_color_face_views[layer];
				framebuffer_attachments[1] = vk.local_point_shadow_face_views[layer];
				VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.local_point_shadow[layer] ) );
			}
		}

	#ifdef VK_CUBEMAP
	if ( vk.cubemapActive )
	{
		// cubemap
		desc.renderPass = vk.render_pass.cubemap;
		desc.attachmentCount = 2;
		desc.width = REF_CUBEMAP_SIZE;
		desc.height = REF_CUBEMAP_SIZE;

		framebuffer_attachments[1] = vk.cubeMap.depth_image_view;

		if ( vk.msaaActive )
			desc.attachmentCount = 3;

		for ( int j = 0; j < 6; j++  ) 
		{
			framebuffer_attachments[0] = vk.cubeMap.color_image_view[j+1];

			if ( vk.msaaActive ) {
				framebuffer_attachments[2] = vk.cubeMap.color_image_view_msaa[0];
			}

			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.cubemap[j] ) );
			SET_OBJECT_NAME( vk.framebuffers.cubemap[j], va( "framebuffer - cubemap face %d", j ), VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
		} 
	}
	#endif

	if ( vk.capture.image != VK_NULL_HANDLE )
	{
		framebuffer_attachments[0] = vk.capture.image_view;

		desc.renderPass = vk.render_pass.capture;
		desc.pAttachments = framebuffer_attachments;
		desc.attachmentCount = 1;
		desc.width = gls.captureWidth;
		desc.height = gls.captureHeight;

		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.capture ) );
		SET_OBJECT_NAME( vk.framebuffers.capture, "framebuffer - capture", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
	}

	if ( vk.smaaActive )
	{
		desc.renderPass = vk.render_pass.smaa_edge;
		desc.attachmentCount = 1;
		desc.width = glConfig.vidWidth;
		desc.height = glConfig.vidHeight;
		framebuffer_attachments[0] = vk.smaa_edge_image_view;
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.smaa_edge ) );
		SET_OBJECT_NAME( vk.framebuffers.smaa_edge, "framebuffer - smaa edge", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );

		desc.renderPass = vk.render_pass.smaa_blend;
		framebuffer_attachments[0] = vk.smaa_blend_image_view;
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.smaa_blend ) );
		SET_OBJECT_NAME( vk.framebuffers.smaa_blend, "framebuffer - smaa blend", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );

		desc.renderPass = vk.render_pass.smaa_compose;
		framebuffer_attachments[0] = vk.smaa_output_image_view;
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.smaa_compose ) );
		SET_OBJECT_NAME( vk.framebuffers.smaa_compose, "framebuffer - smaa compose", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );

		desc.renderPass = vk.render_pass.taa;
		framebuffer_attachments[0] = vk.taa_history_image_view[0];
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.taa[0] ) );
		SET_OBJECT_NAME( vk.framebuffers.taa[0], "framebuffer - taa history 0", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
		framebuffer_attachments[0] = vk.taa_history_image_view[1];
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.taa[1] ) );
		SET_OBJECT_NAME( vk.framebuffers.taa[1], "framebuffer - taa history 1", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
	}

	if ( r_bloom->integer )
	{
		uint32_t width = gls.captureWidth;
		uint32_t height = gls.captureHeight;

		// bloom color extraction
		desc.renderPass = vk.render_pass.bloom_extract;
		desc.width = width;
		desc.height = height;

		desc.attachmentCount = 1;
		framebuffer_attachments[0] = vk.bloom_image_view[0];

		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.bloom_extract ) );

		SET_OBJECT_NAME( vk.framebuffers.bloom_extract, "framebuffer - bloom extraction", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );

		for ( n = 0; n < ARRAY_LEN( vk.framebuffers.blur ); n += 2 )
		{
			width /= 2;
			height /= 2;

			desc.renderPass = vk.render_pass.blur[n];
			desc.width = width;
			desc.height = height;

			desc.attachmentCount = 1;

			framebuffer_attachments[0] = vk.bloom_image_view[n+0+1];
			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.blur[n+0] ) );

			framebuffer_attachments[0] = vk.bloom_image_view[n+1+1];
			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.blur[n+1] ) );

			SET_OBJECT_NAME( vk.framebuffers.blur[n+0], va( "framebuffer - blur %i", n+0 ), VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
			SET_OBJECT_NAME( vk.framebuffers.blur[n+1], va( "framebuffer - blur %i", n+1 ), VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
		}
	}

	if ( r_ssao && r_ssao->integer )
	{
		// ssao
		desc.renderPass = vk.render_pass.ssao;
		desc.attachmentCount = 1;
		desc.width = glConfig.vidWidth;
		desc.height = glConfig.vidHeight;
		framebuffer_attachments[0] = vk.ssao_image_view;
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.ssao ) );
		SET_OBJECT_NAME( vk.framebuffers.ssao, "framebuffer - ssao", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );

		desc.renderPass = vk.render_pass.ssao_blur;
		framebuffer_attachments[0] = vk.ssao_blur_image_view;
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.ssao_blur ) );
		SET_OBJECT_NAME( vk.framebuffers.ssao_blur, "framebuffer - ssao_blur", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );

		desc.renderPass = vk.render_pass.ssao_combine;
		framebuffer_attachments[0] = vk.color_image_view;
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.ssao_combine ) );
		SET_OBJECT_NAME( vk.framebuffers.ssao_combine, "framebuffer - ssao_combine", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
	}

	if ( r_oit && r_oit->integer && vk.render_pass.oit_accum != VK_NULL_HANDLE &&
		vk.oit_accum_image_view && vk.oit_reveal_image_view )
	{
		desc.renderPass = vk.render_pass.oit_accum;
		desc.attachmentCount = ( vkSamples == VK_SAMPLE_COUNT_1_BIT && vk.depth_image_view ) ? 3 : 2;
		desc.width = glConfig.vidWidth;
		desc.height = glConfig.vidHeight;
		framebuffer_attachments[0] = vk.oit_accum_image_view;
		framebuffer_attachments[1] = vk.oit_reveal_image_view;
		if ( desc.attachmentCount == 3 )
			framebuffer_attachments[2] = vk.depth_image_view;
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.oit_accum ) );
		SET_OBJECT_NAME( vk.framebuffers.oit_accum, "framebuffer - oit_accum", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );

		desc.renderPass = vk.render_pass.oit_resolve;
		framebuffer_attachments[0] = vk.color_image_view;
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.oit_resolve ) );
		SET_OBJECT_NAME( vk.framebuffers.oit_resolve, "framebuffer - oit_resolve", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
	}

	if ( PostFX_SSR_IsEnabled() && vk.render_pass.ssr != VK_NULL_HANDLE && vk.ssr_image_view )
	{
		desc.renderPass = vk.render_pass.ssr;
		desc.attachmentCount = 1;
		desc.width = glConfig.vidWidth;
		desc.height = glConfig.vidHeight;
		framebuffer_attachments[0] = vk.ssr_image_view;
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.ssr ) );
		SET_OBJECT_NAME( vk.framebuffers.ssr, "framebuffer - ssr", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
	}

	if ( vk.render_pass.volumetric != VK_NULL_HANDLE ) {
		desc.renderPass = vk.render_pass.volumetric;
		desc.attachmentCount = 1;
		desc.width = glConfig.vidWidth;
		desc.height = glConfig.vidHeight;
		framebuffer_attachments[0] = vk.color_image_view;
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.volumetric[0] ) );
		SET_OBJECT_NAME( vk.framebuffers.volumetric[0], "framebuffer - volumetric fog", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
		for ( n = 1; (uint32_t)n < vk.swapchain_image_count; n++ ) {
			vk.framebuffers.volumetric[n] = vk.framebuffers.volumetric[0];
		}
	}

	#ifdef VK_PBR_BRDFLUT
	if( vk.pbrActive )
	{
		desc.renderPass = vk.render_pass.brdflut;
		desc.width = desc.height = 512;  
		desc.attachmentCount = 1;
		framebuffer_attachments[0] = vk.brdflut_image_view;
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.brdflut ) );
		SET_OBJECT_NAME( vk.framebuffers.brdflut, va( "framebuffer - brdf LUT" ), VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
	}
	#endif
	}
}


static void vk_destroy_framebuffers( void ) {
	uint32_t n;

	for ( n = 0; n < vk.swapchain_image_count; n++ ) {
		if ( vk.framebuffers.main[n] != VK_NULL_HANDLE ) {
			if ( !vk.fboActive || n == 0 ) {
				qvkDestroyFramebuffer( vk.device, vk.framebuffers.main[n], NULL );
			}
			vk.framebuffers.main[n] = VK_NULL_HANDLE;
		}
		if ( vk.framebuffers.atmosphere[n] != VK_NULL_HANDLE ) {
			if ( n == 0 ) {
				qvkDestroyFramebuffer( vk.device, vk.framebuffers.atmosphere[n], NULL );
			}
			vk.framebuffers.atmosphere[n] = VK_NULL_HANDLE;
		}
		if ( vk.framebuffers.gamma[n] != VK_NULL_HANDLE ) {
			qvkDestroyFramebuffer( vk.device, vk.framebuffers.gamma[n], NULL );
			vk.framebuffers.gamma[n] = VK_NULL_HANDLE;
		}
	}

	if ( vk.framebuffers.bloom_extract != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.bloom_extract, NULL );
		vk.framebuffers.bloom_extract = VK_NULL_HANDLE;
	}

	if ( vk.framebuffers.ssao != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.ssao, NULL );
		vk.framebuffers.ssao = VK_NULL_HANDLE;
	}

	if ( vk.framebuffers.ssao_blur != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.ssao_blur, NULL );
		vk.framebuffers.ssao_blur = VK_NULL_HANDLE;
	}

	if ( vk.framebuffers.ssao_combine != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.ssao_combine, NULL );
		vk.framebuffers.ssao_combine = VK_NULL_HANDLE;
	}
	if ( vk.framebuffers.oit_accum != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.oit_accum, NULL );
		vk.framebuffers.oit_accum = VK_NULL_HANDLE;
	}
	if ( vk.framebuffers.oit_resolve != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.oit_resolve, NULL );
		vk.framebuffers.oit_resolve = VK_NULL_HANDLE;
	}

	if ( vk.framebuffers.ssr != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.ssr, NULL );
		vk.framebuffers.ssr = VK_NULL_HANDLE;
	}

	if ( vk.framebuffers.volumetric[0] != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.volumetric[0], NULL );
		for ( n = 0; (uint32_t)n < vk.swapchain_image_count; n++ ) {
			vk.framebuffers.volumetric[n] = VK_NULL_HANDLE;
		}
	}

	if ( vk.framebuffers.screenmap != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.screenmap, NULL );
		vk.framebuffers.screenmap = VK_NULL_HANDLE;
	}
	if ( vk.framebuffers.sun_shadow != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.sun_shadow, NULL );
		vk.framebuffers.sun_shadow = VK_NULL_HANDLE;
	}
	if ( vk.framebuffers.local_spot_shadow != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.local_spot_shadow, NULL );
		vk.framebuffers.local_spot_shadow = VK_NULL_HANDLE;
	}
	for ( n = 0; n < ARRAY_LEN( vk.framebuffers.local_point_shadow ); n++ ) {
		if ( vk.framebuffers.local_point_shadow[n] != VK_NULL_HANDLE ) {
			qvkDestroyFramebuffer( vk.device, vk.framebuffers.local_point_shadow[n], NULL );
			vk.framebuffers.local_point_shadow[n] = VK_NULL_HANDLE;
		}
	}

	if ( vk.framebuffers.capture != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.capture, NULL );
		vk.framebuffers.capture = VK_NULL_HANDLE;
	}
	if ( vk.framebuffers.ui_overlay[0] != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.ui_overlay[0], NULL );
		for ( n = 0; (uint32_t)n < vk.swapchain_image_count; n++ ) {
			vk.framebuffers.ui_overlay[n] = VK_NULL_HANDLE;
		}
	}

	if ( vk.framebuffers.smaa_edge != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.smaa_edge, NULL );
		vk.framebuffers.smaa_edge = VK_NULL_HANDLE;
	}

	if ( vk.framebuffers.smaa_blend != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.smaa_blend, NULL );
		vk.framebuffers.smaa_blend = VK_NULL_HANDLE;
	}

	if ( vk.framebuffers.smaa_compose != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.smaa_compose, NULL );
		vk.framebuffers.smaa_compose = VK_NULL_HANDLE;
	}
	for ( n = 0; n < 2; n++ ) {
		if ( vk.framebuffers.taa[n] != VK_NULL_HANDLE ) {
			qvkDestroyFramebuffer( vk.device, vk.framebuffers.taa[n], NULL );
			vk.framebuffers.taa[n] = VK_NULL_HANDLE;
		}
	}

	for ( n = 0; n < ARRAY_LEN( vk.framebuffers.blur ); n++ ) {
		if ( vk.framebuffers.blur[n] != VK_NULL_HANDLE ) {
			qvkDestroyFramebuffer( vk.device, vk.framebuffers.blur[n], NULL );
			vk.framebuffers.blur[n] = VK_NULL_HANDLE;
		}
	}

#ifdef VK_PBR_BRDFLUT
    if ( vk.framebuffers.brdflut != VK_NULL_HANDLE ) {
        qvkDestroyFramebuffer( vk.device, vk.framebuffers.brdflut, NULL );
        vk.framebuffers.brdflut = VK_NULL_HANDLE;
    }
#endif

#ifdef VK_CUBEMAP
    for ( n = 0; n < ARRAY_LEN( vk.framebuffers.cubemap ); n++ ) {
        if ( vk.framebuffers.cubemap[n] != VK_NULL_HANDLE ) {
            qvkDestroyFramebuffer( vk.device, vk.framebuffers.cubemap[n], NULL );
            vk.framebuffers.cubemap[n] = VK_NULL_HANDLE;
        }
    }
#endif
	for ( n = 0; n < ARRAY_LEN( vk.framebuffers.overlay_compose ); n++ ) {
		if ( vk.framebuffers.overlay_compose[n] != VK_NULL_HANDLE ) {
			qvkDestroyFramebuffer( vk.device, vk.framebuffers.overlay_compose[n], NULL );
			vk.framebuffers.overlay_compose[n] = VK_NULL_HANDLE;
		}
	}
}


/* vk_destroy_swapchain moved to vk_swapchain.c */

static void vk_destroy_attachments( void );
static void vk_destroy_render_passes( void );
static void vk_destroy_pipelines( qboolean resetCount );

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
			Com_Memset( vk_entity_occlusion_visibility, 0xFF, sizeof( vk_entity_occlusion_visibility ) );  /* first frame: all visible */
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

static void vk_create_volumetric_pipeline_layouts( void )
{
	if ( vk.volumetric_compute_pipeline_layout != VK_NULL_HANDLE ||
		vk.volumetric_composite_pipeline_layout != VK_NULL_HANDLE ||
		vk.volumetric_depth_resolve_pipeline_layout != VK_NULL_HANDLE ||
		vk.volumetric_fluid_pipeline_layout != VK_NULL_HANDLE ||
		vk.cbt_terrain_compute_layout != VK_NULL_HANDLE )
	{
		return;
	}

	VkPipelineLayoutCreateInfo desc;
	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.setLayoutCount = 1;
	desc.pSetLayouts = &vk.volumetric_compute_layout;
	desc.pushConstantRangeCount = 0;
	desc.pPushConstantRanges = NULL;

	VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.volumetric_compute_pipeline_layout ) );

	desc.pSetLayouts = &vk.volumetric_composite_layout;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.volumetric_composite_pipeline_layout ) );

	desc.pSetLayouts = &vk.volumetric_depth_resolve_layout;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.volumetric_depth_resolve_pipeline_layout ) );

	if ( vk.luminance_layout != VK_NULL_HANDLE ) {
		VkPushConstantRange luminance_push_range;
		luminance_push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		luminance_push_range.offset = 0;
		luminance_push_range.size = sizeof( VkLuminancePushConstants );
		desc.pSetLayouts = &vk.luminance_layout;
		desc.pushConstantRangeCount = 1;
		desc.pPushConstantRanges = &luminance_push_range;
		VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.luminance_pipeline_layout ) );
		desc.pushConstantRangeCount = 0;
		desc.pPushConstantRanges = NULL;
	}

	desc.pSetLayouts = &vk.volumetric_fluid_layout;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.volumetric_fluid_pipeline_layout ) );

	desc.pSetLayouts = &vk.cbt_terrain_layout;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.cbt_terrain_compute_layout ) );
}

static void vk_create_volumetric_fluid_pipeline( VkPipeline *pipeline, VkShaderModule module, const char *debug_name )
{
	if ( *pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, *pipeline, NULL );
		*pipeline = VK_NULL_HANDLE;
	}

	if ( vk.volumetric_fluid_pipeline_layout == VK_NULL_HANDLE || module == VK_NULL_HANDLE ) {
		return;
	}

	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo desc;
	Com_Memset( &stage, 0, sizeof( stage ) );
	Com_Memset( &desc, 0, sizeof( desc ) );

	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = module;
	stage.pName = "main";

	desc.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	desc.stage = stage;
	desc.layout = vk.volumetric_fluid_pipeline_layout;

	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &desc, NULL, pipeline ) );
	SET_OBJECT_NAME( *pipeline, debug_name, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
}

static void vk_create_volumetric_fluid_pipelines( void )
{
	vk_create_volumetric_fluid_pipeline( &vk.volumetric_fluid_advect_pipeline, vk.modules.fluid_advect_cs, "pipeline - volumetric fluid advect" );
	vk_create_volumetric_fluid_pipeline( &vk.volumetric_fluid_divergence_pipeline, vk.modules.fluid_divergence_cs, "pipeline - volumetric fluid divergence" );
	vk_create_volumetric_fluid_pipeline( &vk.volumetric_fluid_pressure_pipeline, vk.modules.fluid_pressure_cs, "pipeline - volumetric fluid pressure" );
	vk_create_volumetric_fluid_pipeline( &vk.volumetric_fluid_gradient_pipeline, vk.modules.fluid_gradient_cs, "pipeline - volumetric fluid gradient" );
}

static void vk_create_volumetric_compute_pipeline( void )
{
	if ( vk.volumetric_compute_pipeline != VK_NULL_HANDLE )
	{
		qvkDestroyPipeline( vk.device, vk.volumetric_compute_pipeline, NULL );
		vk.volumetric_compute_pipeline = VK_NULL_HANDLE;
	}

	if ( vk.volumetric_compute_pipeline_layout == VK_NULL_HANDLE || vk.modules.volumetric_fog_cs == VK_NULL_HANDLE )
	{
		return;
	}

	VkPipelineShaderStageCreateInfo stage;
	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.modules.volumetric_fog_cs;
	stage.pName = "main";

	VkComputePipelineCreateInfo desc;
	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.stage = stage;
	desc.layout = vk.volumetric_compute_pipeline_layout;

	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &desc, NULL, &vk.volumetric_compute_pipeline ) );
	SET_OBJECT_NAME( vk.volumetric_compute_pipeline, "pipeline - volumetric compute", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
}

static void vk_create_volumetric_depth_resolve_pipeline( void )
{
	if ( vk.volumetric_depth_resolve_pipeline != VK_NULL_HANDLE )
	{
		qvkDestroyPipeline( vk.device, vk.volumetric_depth_resolve_pipeline, NULL );
		vk.volumetric_depth_resolve_pipeline = VK_NULL_HANDLE;
	}

	if ( vk.volumetric_depth_resolve_pipeline_layout == VK_NULL_HANDLE ||
		vk.modules.volumetric_depth_resolve_msaa_cs == VK_NULL_HANDLE )
	{
		return;
	}

	VkPipelineShaderStageCreateInfo stage;
	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.modules.volumetric_depth_resolve_msaa_cs;
	stage.pName = "main";

	VkComputePipelineCreateInfo desc;
	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.stage = stage;
	desc.layout = vk.volumetric_depth_resolve_pipeline_layout;

	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &desc, NULL, &vk.volumetric_depth_resolve_pipeline ) );
	SET_OBJECT_NAME( vk.volumetric_depth_resolve_pipeline, "pipeline - volumetric depth resolve", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
}

static void vk_create_luminance_pipeline( void )
{
	if ( vk.luminance_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.luminance_pipeline, NULL );
		vk.luminance_pipeline = VK_NULL_HANDLE;
	}

	if ( vk.luminance_pipeline_layout == VK_NULL_HANDLE || vk.modules.luminance_cs == VK_NULL_HANDLE ) {
		return;
	}

	VkPipelineShaderStageCreateInfo stage;
	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.modules.luminance_cs;
	stage.pName = "main";

	VkComputePipelineCreateInfo desc;
	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	desc.stage = stage;
	desc.layout = vk.luminance_pipeline_layout;

	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &desc, NULL, &vk.luminance_pipeline ) );
	SET_OBJECT_NAME( vk.luminance_pipeline, "pipeline - luminance", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
}

static void vk_create_volumetric_composite_pipeline( void )
{
	if ( vk.volumetric_composite_pipeline != VK_NULL_HANDLE )
	{
		qvkDestroyPipeline( vk.device, vk.volumetric_composite_pipeline, NULL );
		vk.volumetric_composite_pipeline = VK_NULL_HANDLE;
	}

	if ( vk.volumetric_composite_pipeline_layout == VK_NULL_HANDLE ||
		vk.modules.volumetric_fog_vs == VK_NULL_HANDLE ||
		vk.modules.volumetric_fog_fs == VK_NULL_HANDLE )
	{
		return;
	}

	VkPipelineShaderStageCreateInfo shader_stages[2];
	Com_Memset( shader_stages, 0, sizeof( shader_stages ) );
	shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shader_stages[0].module = vk.modules.volumetric_fog_vs;
	shader_stages[0].pName = "main";
	shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shader_stages[1].module = vk_hdr64_active() ? vk.modules.volumetric_fog_fs_hdr64 : vk.modules.volumetric_fog_fs;
	shader_stages[1].pName = "main";

	VkPipelineVertexInputStateCreateInfo vertex_input;
	Com_Memset( &vertex_input, 0, sizeof( vertex_input ) );
	vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input.vertexBindingDescriptionCount = 0;
	vertex_input.vertexAttributeDescriptionCount = 0;

	VkPipelineInputAssemblyStateCreateInfo input_assembly;
	Com_Memset( &input_assembly, 0, sizeof( input_assembly ) );
	input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	input_assembly.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewport_state;
	VkViewport viewport = { 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f };
	VkRect2D scissor = { { 0, 0 }, { 1, 1 } };
	Com_Memset( &viewport_state, 0, sizeof( viewport_state ) );
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = &viewport;
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo raster_state;
	Com_Memset( &raster_state, 0, sizeof( raster_state ) );
	raster_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	raster_state.polygonMode = VK_POLYGON_MODE_FILL;
	raster_state.rasterizerDiscardEnable = VK_FALSE;
	raster_state.cullMode = VK_CULL_MODE_NONE;
	raster_state.frontFace = VK_FRONT_FACE_CLOCKWISE;
	raster_state.depthBiasEnable = VK_FALSE;
	raster_state.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo multisample_state;
	Com_Memset( &multisample_state, 0, sizeof( multisample_state ) );
	multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState blend_attachment;
	Com_Memset( &blend_attachment, 0, sizeof( blend_attachment ) );
	blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	blend_attachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo blend_state;
	Com_Memset( &blend_state, 0, sizeof( blend_state ) );
	blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state.attachmentCount = 1;
	blend_state.pAttachments = &blend_attachment;

	VkPipelineDepthStencilStateCreateInfo depth_state;
	Com_Memset( &depth_state, 0, sizeof( depth_state ) );
	depth_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_state.depthTestEnable = VK_FALSE;
	depth_state.depthWriteEnable = VK_FALSE;

	VkDynamicState dynamic_states[3] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	uint32_t dynamic_state_count = 2;
	if ( vk.colorWriteMaskDynamic ) {
		dynamic_states[dynamic_state_count++] = VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT;
	}
	VkPipelineDynamicStateCreateInfo dynamic_state;
	Com_Memset( &dynamic_state, 0, sizeof( dynamic_state ) );
	dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.dynamicStateCount = dynamic_state_count;
	dynamic_state.pDynamicStates = dynamic_states;

	VkGraphicsPipelineCreateInfo desc;
	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	desc.stageCount = 2;
	desc.pStages = shader_stages;
	desc.pVertexInputState = &vertex_input;
	desc.pInputAssemblyState = &input_assembly;
	desc.pViewportState = &viewport_state;
	desc.pRasterizationState = &raster_state;
	desc.pMultisampleState = &multisample_state;
	desc.pDepthStencilState = &depth_state;
	desc.pColorBlendState = &blend_state;
	desc.pDynamicState = &dynamic_state;
	desc.layout = vk.volumetric_composite_pipeline_layout;
	desc.renderPass = vk.render_pass.volumetric;
	desc.subpass = 0;

	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &desc, NULL, &vk.volumetric_composite_pipeline ) );
	SET_OBJECT_NAME( vk.volumetric_composite_pipeline, "pipeline - volumetric composite", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
}

static void vk_create_volumetric_pipelines( void )
{
	vk_create_volumetric_pipeline_layouts();
	vk_create_volumetric_depth_resolve_pipeline();
	vk_create_luminance_pipeline();
	vk_create_volumetric_compute_pipeline();
	vk_create_volumetric_fluid_pipelines();
	vk_create_volumetric_composite_pipeline();

	if ( vk.modules.cbt_terrain_cs != VK_NULL_HANDLE && vk.cbt_terrain_compute_layout != VK_NULL_HANDLE ) {
		VkPipelineShaderStageCreateInfo stage;
		VkComputePipelineCreateInfo desc;
		Com_Memset( &stage, 0, sizeof( stage ) );
		Com_Memset( &desc, 0, sizeof( desc ) );
		stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		stage.module = vk.modules.cbt_terrain_cs;
		stage.pName = "main";
		desc.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		desc.stage = stage;
		desc.layout = vk.cbt_terrain_compute_layout;
		if ( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &desc, NULL, &vk.cbt_terrain_compute_pipeline ) == VK_SUCCESS ) {
			SET_OBJECT_NAME( vk.cbt_terrain_compute_pipeline, "pipeline - cbt terrain compute", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
			ri.Printf( PRINT_ALL, "[VK] CBT terrain tessellation pipeline created (r_cbtTerrain)\n" );
		}
	}

	if ( vk.modules.vegetation_wind_cs != VK_NULL_HANDLE && vk.vegwind_layout != VK_NULL_HANDLE ) {
		VkPipelineShaderStageCreateInfo stage;
		VkComputePipelineCreateInfo desc;
		VkPipelineLayoutCreateInfo layout_desc;
		VkPushConstantRange push_range;

		Com_Memset( &layout_desc, 0, sizeof( layout_desc ) );
		layout_desc.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layout_desc.setLayoutCount = 1;
		layout_desc.pSetLayouts = &vk.vegwind_layout;
		push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		push_range.offset = 0;
		push_range.size = 80; /* 4 vec4 + 4 uint */
		layout_desc.pushConstantRangeCount = 1;
		layout_desc.pPushConstantRanges = &push_range;
		VK_CHECK( qvkCreatePipelineLayout( vk.device, &layout_desc, NULL, &vk.pipeline_layout_vegwind ) );
		SET_OBJECT_NAME( vk.pipeline_layout_vegwind, "pipeline layout - vegwind", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );

		Com_Memset( &stage, 0, sizeof( stage ) );
		Com_Memset( &desc, 0, sizeof( desc ) );
		stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		stage.module = vk.modules.vegetation_wind_cs;
		stage.pName = "main";
		desc.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		desc.stage = stage;
		desc.layout = vk.pipeline_layout_vegwind;
		if ( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &desc, NULL, &vk.vegwind_pipeline ) == VK_SUCCESS ) {
			SET_OBJECT_NAME( vk.vegwind_pipeline, "pipeline - vegetation wind compute", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );

			/* Storage buffer for VegetationVertex (32 bytes/vertex); max VEGWIND_MAX_VERTS */
			{
				VkBufferCreateInfo buf_desc;
				VkMemoryRequirements mem_req;
				VkMemoryAllocateInfo alloc_info;
				VkDescriptorSetAllocateInfo set_alloc;
				VkWriteDescriptorSet write_desc;
				VkDescriptorBufferInfo buf_info;
				const VkDeviceSize vegwind_buf_size = (VkDeviceSize)VEGWIND_MAX_VERTS * VEGWIND_VERTEX_STRIDE;

				Com_Memset( &buf_desc, 0, sizeof( buf_desc ) );
				buf_desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
				buf_desc.size = vegwind_buf_size;
				buf_desc.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
				buf_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
				VK_CHECK( qvkCreateBuffer( vk.device, &buf_desc, NULL, &vk.vegwind_vertex_buffer ) );

				qvkGetBufferMemoryRequirements( vk.device, vk.vegwind_vertex_buffer, &mem_req );
				Com_Memset( &alloc_info, 0, sizeof( alloc_info ) );
				alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
				alloc_info.allocationSize = mem_req.size;
				alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
				VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.vegwind_vertex_memory ) );
				VK_CHECK( qvkBindBufferMemory( vk.device, vk.vegwind_vertex_buffer, vk.vegwind_vertex_memory, 0 ) );

				Com_Memset( &set_alloc, 0, sizeof( set_alloc ) );
				set_alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
				set_alloc.descriptorPool = vk.descriptor_pool;
				set_alloc.descriptorSetCount = 1;
				set_alloc.pSetLayouts = &vk.vegwind_layout;
				VK_CHECK( qvkAllocateDescriptorSets( vk.device, &set_alloc, &vk.vegwind_descriptor ) );

				buf_info.buffer = vk.vegwind_vertex_buffer;
				buf_info.offset = 0;
				buf_info.range = vegwind_buf_size;
				Com_Memset( &write_desc, 0, sizeof( write_desc ) );
				write_desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				write_desc.dstSet = vk.vegwind_descriptor;
				write_desc.dstBinding = 0;
				write_desc.dstArrayElement = 0;
				write_desc.descriptorCount = 1;
				write_desc.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				write_desc.pBufferInfo = &buf_info;
				qvkUpdateDescriptorSets( vk.device, 1, &write_desc, 0, NULL );
			}
		}
	}
}

static void vk_destroy_volumetric_pipelines( void )
{
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

static void vk_create_fog_noise_texture( void )
{
	int noise_dim = 64;
	const VkDeviceSize max_bytes = 128ull * 128ull * 128ull;
	byte *noise_data;
	VkDeviceSize noise_bytes;
	VkBuffer staging_buffer = VK_NULL_HANDLE;
	VkDeviceMemory staging_memory = VK_NULL_HANDLE;
	VkBufferCreateInfo buffer_info;
	VkMemoryRequirements mem_req;
	VkMemoryAllocateInfo alloc_info;
	VkCommandBuffer command_buffer;
	VkBufferImageCopy copy_region;
	VkImageCreateInfo image_info;
	VkImageViewCreateInfo view_info;

	if ( r_volumetricFogNoiseDim ) {
		noise_dim = r_volumetricFogNoiseDim->integer;
	}
	if ( noise_dim < 8 ) noise_dim = 8;
	if ( noise_dim > 128 ) noise_dim = 128;

	noise_bytes = (VkDeviceSize)noise_dim * noise_dim * noise_dim;
	if ( noise_bytes > max_bytes ) {
		noise_dim = 128;
		noise_bytes = max_bytes;
	}

	noise_data = (byte *)ri.Hunk_AllocateTempMemory( (int)noise_bytes );
	for ( uint32_t z = 0; z < (uint32_t)noise_dim; z++ ) {
		for ( uint32_t y = 0; y < (uint32_t)noise_dim; y++ ) {
			for ( uint32_t x = 0; x < (uint32_t)noise_dim; x++ ) {
				const uint32_t h = vk_noise_hash3( x, y, z );
				noise_data[ ( z * (uint32_t)noise_dim + y ) * (uint32_t)noise_dim + x ] = (byte)( h & 0xFFu );
			}
		}
	}

	Com_Memset( &image_info, 0, sizeof( image_info ) );
	image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_info.imageType = VK_IMAGE_TYPE_3D;
	image_info.format = VK_FORMAT_R8_UNORM;
	image_info.extent.width = (uint32_t)noise_dim;
	image_info.extent.height = (uint32_t)noise_dim;
	image_info.extent.depth = (uint32_t)noise_dim;
	image_info.mipLevels = 1;
	image_info.arrayLayers = 1;
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VK_CHECK( qvkCreateImage( vk.device, &image_info, NULL, &vk.fog_noise_image ) );

	qvkGetImageMemoryRequirements( vk.device, vk.fog_noise_image, &mem_req );
	Com_Memset( &alloc_info, 0, sizeof( alloc_info ) );
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.fog_noise_memory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, vk.fog_noise_image, vk.fog_noise_memory, 0 ) );

	Com_Memset( &view_info, 0, sizeof( view_info ) );
	view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_info.image = vk.fog_noise_image;
	view_info.viewType = VK_IMAGE_VIEW_TYPE_3D;
	view_info.format = VK_FORMAT_R8_UNORM;
	view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	view_info.subresourceRange.levelCount = 1;
	view_info.subresourceRange.layerCount = 1;
	VK_CHECK( qvkCreateImageView( vk.device, &view_info, NULL, &vk.fog_noise_view ) );

	Com_Memset( &buffer_info, 0, sizeof( buffer_info ) );
	buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_info.size = noise_bytes;
	buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK( qvkCreateBuffer( vk.device, &buffer_info, NULL, &staging_buffer ) );
	qvkGetBufferMemoryRequirements( vk.device, staging_buffer, &mem_req );

	Com_Memset( &alloc_info, 0, sizeof( alloc_info ) );
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &staging_memory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, staging_buffer, staging_memory, 0 ) );

	void *mapped = NULL;
	VK_CHECK( qvkMapMemory( vk.device, staging_memory, 0, noise_bytes, 0, &mapped ) );
	Com_Memcpy( mapped, noise_data, (size_t)noise_bytes );
	qvkUnmapMemory( vk.device, staging_memory );
	ri.Hunk_FreeTempMemory( noise_data );

	Com_Memset( &copy_region, 0, sizeof( copy_region ) );
	copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copy_region.imageSubresource.layerCount = 1;
	copy_region.imageExtent.width = (uint32_t)noise_dim;
	copy_region.imageExtent.height = (uint32_t)noise_dim;
	copy_region.imageExtent.depth = (uint32_t)noise_dim;

	command_buffer = vk_begin_command_buffer();
	record_image_layout_transition( command_buffer, vk.fog_noise_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT );
	qvkCmdCopyBufferToImage( command_buffer, staging_buffer, vk.fog_noise_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region );
	record_image_layout_transition( command_buffer, vk.fog_noise_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	vk_end_command_buffer( command_buffer, __func__ );

	qvkDestroyBuffer( vk.device, staging_buffer, NULL );
	qvkFreeMemory( vk.device, staging_memory, NULL );

	if ( r_fogDebug && r_fogDebug->integer >= 1 ) {
		ri.Printf( PRINT_ALL, "[VK][fog] noise texture created %dx%dx%d\n", noise_dim, noise_dim, noise_dim );
	}
}

static void vk_destroy_sun_shadow_resources( void )
{
	if ( vk.sun_shadow_sampler ) {
		vk.sun_shadow_sampler = VK_NULL_HANDLE;
	}
	if ( vk.sun_shadow_image ) {
		qvkDestroyImage( vk.device, vk.sun_shadow_image, NULL );
		vk.sun_shadow_image = VK_NULL_HANDLE;
	}
	if ( vk.sun_shadow_view ) {
		qvkDestroyImageView( vk.device, vk.sun_shadow_view, NULL );
		vk.sun_shadow_view = VK_NULL_HANDLE;
	}
	if ( vk.sun_shadow_sample_view ) {
		qvkDestroyImageView( vk.device, vk.sun_shadow_sample_view, NULL );
		vk.sun_shadow_sample_view = VK_NULL_HANDLE;
	}
	if ( vk.sun_shadow_memory ) {
		qvkFreeMemory( vk.device, vk.sun_shadow_memory, NULL );
		vk.sun_shadow_memory = VK_NULL_HANDLE;
	}
	if ( vk.sun_shadow_color_image ) {
		qvkDestroyImage( vk.device, vk.sun_shadow_color_image, NULL );
		vk.sun_shadow_color_image = VK_NULL_HANDLE;
	}
	if ( vk.sun_shadow_color_view ) {
		qvkDestroyImageView( vk.device, vk.sun_shadow_color_view, NULL );
		vk.sun_shadow_color_view = VK_NULL_HANDLE;
	}
	if ( vk.sun_shadow_color_memory ) {
		qvkFreeMemory( vk.device, vk.sun_shadow_color_memory, NULL );
		vk.sun_shadow_color_memory = VK_NULL_HANDLE;
	}
	if ( vk.sun_shadow_color_msaa_image ) {
		qvkDestroyImage( vk.device, vk.sun_shadow_color_msaa_image, NULL );
		vk.sun_shadow_color_msaa_image = VK_NULL_HANDLE;
	}
	if ( vk.sun_shadow_color_msaa_view ) {
		qvkDestroyImageView( vk.device, vk.sun_shadow_color_msaa_view, NULL );
		vk.sun_shadow_color_msaa_view = VK_NULL_HANDLE;
	}
	if ( vk.sun_shadow_color_msaa_memory ) {
		qvkFreeMemory( vk.device, vk.sun_shadow_color_msaa_memory, NULL );
		vk.sun_shadow_color_msaa_memory = VK_NULL_HANDLE;
	}
	vk.sun_shadow_width = 0;
	vk.sun_shadow_height = 0;
	Matrix16Identity( vk.sun_shadow_matrix0 );
	vk.sun_shadow_valid = qfalse;
}

static void vk_create_sun_shadow_resources( void )
{
	int map_size = 1024;
	VkImageCreateInfo image_info;
	VkImageViewCreateInfo view_info;
	VkMemoryRequirements mem_req;
	VkMemoryAllocateInfo alloc_info;
	VkImageAspectFlags depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	VkCommandBuffer cmd;

	if ( !vk.fboActive ) {
		vk_destroy_sun_shadow_resources();
		return;
	}

	if ( r_fogShadowMapSize ) {
		map_size = r_fogShadowMapSize->integer;
	}
	if ( map_size < 256 ) map_size = 256;
	if ( map_size > 4096 ) map_size = 4096;

	vk_destroy_sun_shadow_resources();

	vk.sun_shadow_width = (uint32_t)map_size;
	vk.sun_shadow_height = (uint32_t)map_size;
	Matrix16Identity( vk.sun_shadow_matrix0 );
	vk.sun_shadow_valid = qfalse;

	if ( glConfig.stencilBits > 0 ) {
		depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	Com_Memset( &image_info, 0, sizeof( image_info ) );
	image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_info.imageType = VK_IMAGE_TYPE_2D;
	image_info.extent.width = vk.sun_shadow_width;
	image_info.extent.height = vk.sun_shadow_height;
	image_info.extent.depth = 1;
	image_info.mipLevels = 1;
	image_info.arrayLayers = 1;
	image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	image_info.format = vk.depth_format;
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	VK_CHECK( qvkCreateImage( vk.device, &image_info, NULL, &vk.sun_shadow_image ) );

	qvkGetImageMemoryRequirements( vk.device, vk.sun_shadow_image, &mem_req );
	Com_Memset( &alloc_info, 0, sizeof( alloc_info ) );
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.sun_shadow_memory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, vk.sun_shadow_image, vk.sun_shadow_memory, 0 ) );

	image_info.format = vk.color_format;
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	VK_CHECK( qvkCreateImage( vk.device, &image_info, NULL, &vk.sun_shadow_color_image ) );

	qvkGetImageMemoryRequirements( vk.device, vk.sun_shadow_color_image, &mem_req );
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.sun_shadow_color_memory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, vk.sun_shadow_color_image, vk.sun_shadow_color_memory, 0 ) );

	Com_Memset( &view_info, 0, sizeof( view_info ) );
	view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_info.subresourceRange.baseMipLevel = 0;
	view_info.subresourceRange.levelCount = 1;
	view_info.subresourceRange.baseArrayLayer = 0;
	view_info.subresourceRange.layerCount = 1;

	view_info.image = vk.sun_shadow_image;
	view_info.format = vk.depth_format;
	view_info.subresourceRange.aspectMask = depth_aspect;
	VK_CHECK( qvkCreateImageView( vk.device, &view_info, NULL, &vk.sun_shadow_view ) );
	vk_create_depth_only_image_view( vk.sun_shadow_image, vk.depth_format, VK_IMAGE_VIEW_TYPE_2D,
		0, 1, &vk.sun_shadow_sample_view, "sun shadow sample view" );

	view_info.image = vk.sun_shadow_color_image;
	view_info.format = vk.color_format;
	view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	VK_CHECK( qvkCreateImageView( vk.device, &view_info, NULL, &vk.sun_shadow_color_view ) );

	cmd = vk_begin_command_buffer();
	record_image_layout_transition( cmd, vk.sun_shadow_color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
	record_image_layout_transition( cmd, vk.sun_shadow_image, depth_aspect,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	vk_end_command_buffer( cmd, __func__ );

	vk.sun_shadow_color_msaa_image = VK_NULL_HANDLE;
	vk.sun_shadow_color_msaa_view = VK_NULL_HANDLE;
	vk.sun_shadow_color_msaa_memory = VK_NULL_HANDLE;

	if ( r_fogDebug && r_fogDebug->integer >= 1 ) {
		ri.Printf( PRINT_ALL, "[VK][fog] sun shadow resources created %ux%u (resolved depth path, screenMapSamples=%u)\n",
			vk.sun_shadow_width, vk.sun_shadow_height, vk.screenMapSamples );
	}
}

static void vk_destroy_local_shadow_resources( void )
{
	for ( uint32_t i = 0; i < ARRAY_LEN( vk.local_point_shadow_face_views ); i++ ) {
		if ( vk.local_point_shadow_face_views[i] ) {
			qvkDestroyImageView( vk.device, vk.local_point_shadow_face_views[i], NULL );
			vk.local_point_shadow_face_views[i] = VK_NULL_HANDLE;
		}
		if ( vk.local_point_shadow_color_face_views[i] ) {
			qvkDestroyImageView( vk.device, vk.local_point_shadow_color_face_views[i], NULL );
			vk.local_point_shadow_color_face_views[i] = VK_NULL_HANDLE;
		}
	}

	if ( vk.local_spot_shadow_atlas_image ) {
		qvkDestroyImage( vk.device, vk.local_spot_shadow_atlas_image, NULL );
		vk.local_spot_shadow_atlas_image = VK_NULL_HANDLE;
	}
	if ( vk.local_spot_shadow_atlas_view ) {
		qvkDestroyImageView( vk.device, vk.local_spot_shadow_atlas_view, NULL );
		vk.local_spot_shadow_atlas_view = VK_NULL_HANDLE;
	}
	if ( vk.local_spot_shadow_atlas_sample_view ) {
		qvkDestroyImageView( vk.device, vk.local_spot_shadow_atlas_sample_view, NULL );
		vk.local_spot_shadow_atlas_sample_view = VK_NULL_HANDLE;
	}
	if ( vk.local_spot_shadow_atlas_memory ) {
		qvkFreeMemory( vk.device, vk.local_spot_shadow_atlas_memory, NULL );
		vk.local_spot_shadow_atlas_memory = VK_NULL_HANDLE;
	}

	if ( vk.local_spot_shadow_color_image ) {
		qvkDestroyImage( vk.device, vk.local_spot_shadow_color_image, NULL );
		vk.local_spot_shadow_color_image = VK_NULL_HANDLE;
	}
	if ( vk.local_spot_shadow_color_view ) {
		qvkDestroyImageView( vk.device, vk.local_spot_shadow_color_view, NULL );
		vk.local_spot_shadow_color_view = VK_NULL_HANDLE;
	}
	if ( vk.local_spot_shadow_color_memory ) {
		qvkFreeMemory( vk.device, vk.local_spot_shadow_color_memory, NULL );
		vk.local_spot_shadow_color_memory = VK_NULL_HANDLE;
	}

	if ( vk.local_point_shadow_array_image ) {
		qvkDestroyImage( vk.device, vk.local_point_shadow_array_image, NULL );
		vk.local_point_shadow_array_image = VK_NULL_HANDLE;
	}
	if ( vk.local_point_shadow_array_view ) {
		qvkDestroyImageView( vk.device, vk.local_point_shadow_array_view, NULL );
		vk.local_point_shadow_array_view = VK_NULL_HANDLE;
	}
	if ( vk.local_point_shadow_array_sample_view ) {
		qvkDestroyImageView( vk.device, vk.local_point_shadow_array_sample_view, NULL );
		vk.local_point_shadow_array_sample_view = VK_NULL_HANDLE;
	}
	if ( vk.local_point_shadow_array_memory ) {
		qvkFreeMemory( vk.device, vk.local_point_shadow_array_memory, NULL );
		vk.local_point_shadow_array_memory = VK_NULL_HANDLE;
	}

	if ( vk.local_point_shadow_color_array_image ) {
		qvkDestroyImage( vk.device, vk.local_point_shadow_color_array_image, NULL );
		vk.local_point_shadow_color_array_image = VK_NULL_HANDLE;
	}
	if ( vk.local_point_shadow_color_array_view ) {
		qvkDestroyImageView( vk.device, vk.local_point_shadow_color_array_view, NULL );
		vk.local_point_shadow_color_array_view = VK_NULL_HANDLE;
	}
	if ( vk.local_point_shadow_color_array_memory ) {
		qvkFreeMemory( vk.device, vk.local_point_shadow_color_array_memory, NULL );
		vk.local_point_shadow_color_array_memory = VK_NULL_HANDLE;
	}

	vk.local_spot_shadow_atlas_size = 0;
	vk.local_spot_shadow_tile_size = 0;
	vk.local_spot_shadow_capacity = 0;
	vk.local_point_shadow_face_size = 0;
	vk.local_point_shadow_capacity = 0;
}

static void vk_create_local_shadow_resources( void )
{
	int map_size = 1024;
	uint32_t point_layers;
	VkImageCreateInfo image_info;
	VkImageViewCreateInfo view_info;
	VkMemoryRequirements mem_req;
	VkMemoryAllocateInfo alloc_info;
	VkImageAspectFlags depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	VkCommandBuffer cmd;

	if ( !vk.fboActive ) {
		vk_destroy_local_shadow_resources();
		return;
	}

	if ( r_fogShadowMapSize ) {
		map_size = r_fogShadowMapSize->integer;
	}
	if ( map_size < 256 ) map_size = 256;
	if ( map_size > 4096 ) map_size = 4096;

	vk_destroy_local_shadow_resources();

	int atlas_size = map_size * 2;
	int point_size = map_size / 4;
	if ( atlas_size < 512 ) atlas_size = 512;
	if ( atlas_size > (int)vk.hwMaxImageDimension2D ) atlas_size = (int)vk.hwMaxImageDimension2D;
	if ( atlas_size < 128 ) atlas_size = 128;
	if ( point_size < 128 ) point_size = 128;
	if ( point_size > 512 ) point_size = 512;
	vk.local_spot_shadow_atlas_size = (uint32_t)atlas_size;
	vk.local_point_shadow_face_size = (uint32_t)point_size;
	if ( vk.local_point_shadow_face_size > vk.local_spot_shadow_atlas_size ) {
		vk.local_point_shadow_face_size = vk.local_spot_shadow_atlas_size;
	}
	vk.local_spot_shadow_tile_size = vk.local_point_shadow_face_size;
	if ( vk.local_spot_shadow_tile_size == 0 ) {
		vk.local_spot_shadow_tile_size = 128;
	}

	const uint32_t grid = MAX( 1u, vk.local_spot_shadow_atlas_size / vk.local_spot_shadow_tile_size );
	vk.local_spot_shadow_capacity = MIN( (uint32_t)MAX_DLIGHTS, grid * grid );
	vk.local_point_shadow_capacity = MIN( (uint32_t)MAX_DLIGHTS, 16u );
	point_layers = vk.local_point_shadow_capacity * 6u;
	if ( point_layers == 0 ) {
		point_layers = 6;
		vk.local_point_shadow_capacity = 1;
	}

	if ( glConfig.stencilBits > 0 ) {
		depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	Com_Memset( &image_info, 0, sizeof( image_info ) );
	image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_info.imageType = VK_IMAGE_TYPE_2D;
	image_info.extent.depth = 1;
	image_info.mipLevels = 1;
	image_info.arrayLayers = 1;
	image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;

	image_info.format = vk.depth_format;
	image_info.extent.width = vk.local_spot_shadow_atlas_size;
	image_info.extent.height = vk.local_spot_shadow_atlas_size;
	image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	VK_CHECK( qvkCreateImage( vk.device, &image_info, NULL, &vk.local_spot_shadow_atlas_image ) );

	qvkGetImageMemoryRequirements( vk.device, vk.local_spot_shadow_atlas_image, &mem_req );
	Com_Memset( &alloc_info, 0, sizeof( alloc_info ) );
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.local_spot_shadow_atlas_memory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, vk.local_spot_shadow_atlas_image, vk.local_spot_shadow_atlas_memory, 0 ) );

	image_info.format = vk.color_format;
	image_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	VK_CHECK( qvkCreateImage( vk.device, &image_info, NULL, &vk.local_spot_shadow_color_image ) );
	qvkGetImageMemoryRequirements( vk.device, vk.local_spot_shadow_color_image, &mem_req );
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.local_spot_shadow_color_memory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, vk.local_spot_shadow_color_image, vk.local_spot_shadow_color_memory, 0 ) );

	image_info.format = vk.depth_format;
	image_info.extent.width = vk.local_point_shadow_face_size;
	image_info.extent.height = vk.local_point_shadow_face_size;
	image_info.arrayLayers = point_layers;
	image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	VK_CHECK( qvkCreateImage( vk.device, &image_info, NULL, &vk.local_point_shadow_array_image ) );

	qvkGetImageMemoryRequirements( vk.device, vk.local_point_shadow_array_image, &mem_req );
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.local_point_shadow_array_memory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, vk.local_point_shadow_array_image, vk.local_point_shadow_array_memory, 0 ) );

	image_info.format = vk.color_format;
	image_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	VK_CHECK( qvkCreateImage( vk.device, &image_info, NULL, &vk.local_point_shadow_color_array_image ) );

	qvkGetImageMemoryRequirements( vk.device, vk.local_point_shadow_color_array_image, &mem_req );
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.local_point_shadow_color_array_memory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, vk.local_point_shadow_color_array_image, vk.local_point_shadow_color_array_memory, 0 ) );

	Com_Memset( &view_info, 0, sizeof( view_info ) );
	view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_info.subresourceRange.baseMipLevel = 0;
	view_info.subresourceRange.levelCount = 1;
	view_info.subresourceRange.baseArrayLayer = 0;
	view_info.subresourceRange.layerCount = 1;

	view_info.image = vk.local_spot_shadow_atlas_image;
	view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_info.format = vk.depth_format;
	view_info.subresourceRange.aspectMask = depth_aspect;
	VK_CHECK( qvkCreateImageView( vk.device, &view_info, NULL, &vk.local_spot_shadow_atlas_view ) );
	vk_create_depth_only_image_view( vk.local_spot_shadow_atlas_image, vk.depth_format, VK_IMAGE_VIEW_TYPE_2D,
		0, 1, &vk.local_spot_shadow_atlas_sample_view, "local spot shadow atlas sample view" );

	view_info.image = vk.local_spot_shadow_color_image;
	view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_info.format = vk.color_format;
	view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	VK_CHECK( qvkCreateImageView( vk.device, &view_info, NULL, &vk.local_spot_shadow_color_view ) );

	view_info.image = vk.local_point_shadow_array_image;
	view_info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	view_info.format = vk.depth_format;
	view_info.subresourceRange.aspectMask = depth_aspect;
	view_info.subresourceRange.layerCount = point_layers;
	VK_CHECK( qvkCreateImageView( vk.device, &view_info, NULL, &vk.local_point_shadow_array_view ) );
	vk_create_depth_only_image_view( vk.local_point_shadow_array_image, vk.depth_format, VK_IMAGE_VIEW_TYPE_2D_ARRAY,
		0, point_layers, &vk.local_point_shadow_array_sample_view, "local point shadow array sample view" );

	view_info.image = vk.local_point_shadow_color_array_image;
	view_info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	view_info.format = vk.color_format;
	view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	view_info.subresourceRange.layerCount = point_layers;
	VK_CHECK( qvkCreateImageView( vk.device, &view_info, NULL, &vk.local_point_shadow_color_array_view ) );

	for ( uint32_t layer = 0; layer < point_layers; layer++ ) {
		view_info.image = vk.local_point_shadow_array_image;
		view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_info.format = vk.depth_format;
		view_info.subresourceRange.aspectMask = depth_aspect;
		view_info.subresourceRange.baseArrayLayer = layer;
		view_info.subresourceRange.layerCount = 1;
		VK_CHECK( qvkCreateImageView( vk.device, &view_info, NULL, &vk.local_point_shadow_face_views[layer] ) );

		view_info.image = vk.local_point_shadow_color_array_image;
		view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_info.format = vk.color_format;
		view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view_info.subresourceRange.baseArrayLayer = layer;
		view_info.subresourceRange.layerCount = 1;
		VK_CHECK( qvkCreateImageView( vk.device, &view_info, NULL, &vk.local_point_shadow_color_face_views[layer] ) );
	}

	cmd = vk_begin_command_buffer();
	record_image_layout_transition( cmd, vk.local_spot_shadow_color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
	record_image_layout_transition( cmd, vk.local_spot_shadow_atlas_image, depth_aspect,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	record_image_layout_transition( cmd, vk.local_point_shadow_color_array_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
	record_image_layout_transition( cmd, vk.local_point_shadow_array_image, depth_aspect,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	vk_end_command_buffer( cmd, __func__ );
}

static void vk_destroy_froxel_images( void )
{
	if ( vk.froxel_volume_image ) {
		qvkDestroyImage( vk.device, vk.froxel_volume_image, NULL );
		vk.froxel_volume_image = VK_NULL_HANDLE;
	}
	if ( vk.froxel_volume_view ) {
		qvkDestroyImageView( vk.device, vk.froxel_volume_view, NULL );
		vk.froxel_volume_view = VK_NULL_HANDLE;
	}
	if ( vk.froxel_volume_memory ) {
		qvkFreeMemory( vk.device, vk.froxel_volume_memory, NULL );
		vk.froxel_volume_memory = VK_NULL_HANDLE;
	}

	if ( vk.froxel_history_image ) {
		qvkDestroyImage( vk.device, vk.froxel_history_image, NULL );
		vk.froxel_history_image = VK_NULL_HANDLE;
	}
	if ( vk.froxel_history_view ) {
		qvkDestroyImageView( vk.device, vk.froxel_history_view, NULL );
		vk.froxel_history_view = VK_NULL_HANDLE;
	}
	if ( vk.froxel_history_memory ) {
		qvkFreeMemory( vk.device, vk.froxel_history_memory, NULL );
		vk.froxel_history_memory = VK_NULL_HANDLE;
	}
	if ( vk.froxel_extinction_image ) {
		qvkDestroyImage( vk.device, vk.froxel_extinction_image, NULL );
		vk.froxel_extinction_image = VK_NULL_HANDLE;
	}
	if ( vk.froxel_extinction_view ) {
		qvkDestroyImageView( vk.device, vk.froxel_extinction_view, NULL );
		vk.froxel_extinction_view = VK_NULL_HANDLE;
	}
	if ( vk.froxel_extinction_memory ) {
		qvkFreeMemory( vk.device, vk.froxel_extinction_memory, NULL );
		vk.froxel_extinction_memory = VK_NULL_HANDLE;
	}
	if ( vk.froxel_light_image ) {
		qvkDestroyImage( vk.device, vk.froxel_light_image, NULL );
		vk.froxel_light_image = VK_NULL_HANDLE;
	}
	if ( vk.froxel_light_view ) {
		qvkDestroyImageView( vk.device, vk.froxel_light_view, NULL );
		vk.froxel_light_view = VK_NULL_HANDLE;
	}
	if ( vk.froxel_light_memory ) {
		qvkFreeMemory( vk.device, vk.froxel_light_memory, NULL );
		vk.froxel_light_memory = VK_NULL_HANDLE;
	}
	if ( vk.froxel_clamp_image ) {
		qvkDestroyImage( vk.device, vk.froxel_clamp_image, NULL );
		vk.froxel_clamp_image = VK_NULL_HANDLE;
	}
	if ( vk.froxel_clamp_view ) {
		qvkDestroyImageView( vk.device, vk.froxel_clamp_view, NULL );
		vk.froxel_clamp_view = VK_NULL_HANDLE;
	}
	if ( vk.froxel_clamp_memory ) {
		qvkFreeMemory( vk.device, vk.froxel_clamp_memory, NULL );
		vk.froxel_clamp_memory = VK_NULL_HANDLE;
	}
	for ( int i = 0; i < 2; i++ ) {
		if ( vk.fluid_velocity_images[i] ) {
			qvkDestroyImage( vk.device, vk.fluid_velocity_images[i], NULL );
			vk.fluid_velocity_images[i] = VK_NULL_HANDLE;
		}
		if ( vk.fluid_velocity_views[i] ) {
			qvkDestroyImageView( vk.device, vk.fluid_velocity_views[i], NULL );
			vk.fluid_velocity_views[i] = VK_NULL_HANDLE;
		}
		if ( vk.fluid_velocity_memory[i] ) {
			qvkFreeMemory( vk.device, vk.fluid_velocity_memory[i], NULL );
			vk.fluid_velocity_memory[i] = VK_NULL_HANDLE;
		}
		if ( vk.fluid_density_images[i] ) {
			qvkDestroyImage( vk.device, vk.fluid_density_images[i], NULL );
			vk.fluid_density_images[i] = VK_NULL_HANDLE;
		}
		if ( vk.fluid_density_views[i] ) {
			qvkDestroyImageView( vk.device, vk.fluid_density_views[i], NULL );
			vk.fluid_density_views[i] = VK_NULL_HANDLE;
		}
		if ( vk.fluid_density_memory[i] ) {
			qvkFreeMemory( vk.device, vk.fluid_density_memory[i], NULL );
			vk.fluid_density_memory[i] = VK_NULL_HANDLE;
		}
		if ( vk.fluid_pressure_images[i] ) {
			qvkDestroyImage( vk.device, vk.fluid_pressure_images[i], NULL );
			vk.fluid_pressure_images[i] = VK_NULL_HANDLE;
		}
		if ( vk.fluid_pressure_views[i] ) {
			qvkDestroyImageView( vk.device, vk.fluid_pressure_views[i], NULL );
			vk.fluid_pressure_views[i] = VK_NULL_HANDLE;
		}
		if ( vk.fluid_pressure_memory[i] ) {
			qvkFreeMemory( vk.device, vk.fluid_pressure_memory[i], NULL );
			vk.fluid_pressure_memory[i] = VK_NULL_HANDLE;
		}
	}
	if ( vk.fluid_divergence_image ) {
		qvkDestroyImage( vk.device, vk.fluid_divergence_image, NULL );
		vk.fluid_divergence_image = VK_NULL_HANDLE;
	}
	if ( vk.fluid_divergence_view ) {
		qvkDestroyImageView( vk.device, vk.fluid_divergence_view, NULL );
		vk.fluid_divergence_view = VK_NULL_HANDLE;
	}
	if ( vk.fluid_divergence_memory ) {
		qvkFreeMemory( vk.device, vk.fluid_divergence_memory, NULL );
		vk.fluid_divergence_memory = VK_NULL_HANDLE;
	}
	if ( vk.volumetric_telemetry_image ) {
		qvkDestroyImage( vk.device, vk.volumetric_telemetry_image, NULL );
		vk.volumetric_telemetry_image = VK_NULL_HANDLE;
	}
	if ( vk.volumetric_telemetry_view ) {
		qvkDestroyImageView( vk.device, vk.volumetric_telemetry_view, NULL );
		vk.volumetric_telemetry_view = VK_NULL_HANDLE;
	}
	if ( vk.volumetric_telemetry_memory ) {
		qvkFreeMemory( vk.device, vk.volumetric_telemetry_memory, NULL );
		vk.volumetric_telemetry_memory = VK_NULL_HANDLE;
	}
	if ( vk.fog_noise_sampler ) {
		vk.fog_noise_sampler = VK_NULL_HANDLE;
	}
	if ( vk.fog_noise_image ) {
		qvkDestroyImage( vk.device, vk.fog_noise_image, NULL );
		vk.fog_noise_image = VK_NULL_HANDLE;
	}
	if ( vk.fog_noise_view ) {
		qvkDestroyImageView( vk.device, vk.fog_noise_view, NULL );
		vk.fog_noise_view = VK_NULL_HANDLE;
	}
	if ( vk.fog_noise_memory ) {
		qvkFreeMemory( vk.device, vk.fog_noise_memory, NULL );
		vk.fog_noise_memory = VK_NULL_HANDLE;
	}

	vk.froxel_width = 0;
	vk.froxel_height = 0;
	vk.froxel_slices = 0;
	vk.fluid_width = 0;
	vk.fluid_height = 0;
	vk.fluid_active_width = 0;
	vk.fluid_active_height = 0;
	vk.fluid_velocity_index = 0;
	vk.fluid_density_index = 0;
	vk.fluid_pressure_index = 0;
}

static void vk_create_froxel_images( void )
{
	int grid_x = VK_FROXEL_DEFAULT_WIDTH;
	int grid_y = VK_FROXEL_DEFAULT_HEIGHT;
	int grid_z = VK_FROXEL_DEFAULT_SLICES;
	int quality = ( r_volumetricFogQuality ) ? r_volumetricFogQuality->integer : 2;
	int fluid_quality = ( r_fogFluidQuality ) ? r_fogFluidQuality->integer : 2;
	float resolution_scale = ( r_volumetricFogResolutionScale ) ? r_volumetricFogResolutionScale->value : 1.0f;
	float fluid_resolution_scale = ( r_fogFluidResolutionScale ) ? r_fogFluidResolutionScale->value : VK_FLUID_DEFAULT_RESOLUTION_SCALE;
	float fluid_quality_scale = 1.0f;
	{
		int tier = 0;
		cvar_t *tierCvar = ri.Cvar_Get( "r_volumetricFogTier", "0", 0 );
		if ( tierCvar ) tier = tierCvar->integer;
		if ( tier == 1 ) {
			quality = MIN( quality, 1 );
			resolution_scale = MIN( resolution_scale, 0.75f );
			fluid_quality = MIN( fluid_quality, 1 );
			fluid_resolution_scale = MIN( fluid_resolution_scale, 0.5f );
		}
	}
	int fluid_x;
	int fluid_y;

	if ( !glConfig.vidWidth || !glConfig.vidHeight ) {
		return;
	}

	vk_destroy_froxel_images();

	if ( r_volumetricFogGridDim && r_volumetricFogGridDim->string && r_volumetricFogGridDim->string[0] ) {
		if ( sscanf( r_volumetricFogGridDim->string, "%d %d %d", &grid_x, &grid_y, &grid_z ) != 3 ) {
			grid_x = VK_FROXEL_DEFAULT_WIDTH;
			grid_y = VK_FROXEL_DEFAULT_HEIGHT;
			grid_z = VK_FROXEL_DEFAULT_SLICES;
		}
	}

	if ( grid_x < 1 ) grid_x = 1;
	if ( grid_x > 1024 ) grid_x = 1024;
	if ( grid_y < 1 ) grid_y = 1;
	if ( grid_y > 1024 ) grid_y = 1024;
	if ( grid_z < 1 ) grid_z = 1;
	if ( grid_z > 256 ) grid_z = 256;
	if ( resolution_scale < 0.25f ) {
		resolution_scale = 0.25f;
	} else if ( resolution_scale > 1.0f ) {
		resolution_scale = 1.0f;
	}

	if ( quality < 0 ) {
		quality = 0;
	} else if ( quality > 3 ) {
		quality = 3;
	}
	if ( fluid_quality < 0 ) {
		fluid_quality = 0;
	} else if ( fluid_quality > 3 ) {
		fluid_quality = 3;
	}
	if ( fluid_resolution_scale < 0.125f ) {
		fluid_resolution_scale = 0.125f;
	} else if ( fluid_resolution_scale > 1.0f ) {
		fluid_resolution_scale = 1.0f;
	}

	switch ( quality ) {
		case 0:
			grid_x = MAX( 1, grid_x / 2 );
			grid_y = MAX( 1, grid_y / 2 );
			grid_z = MAX( 1, grid_z / 2 );
			break;
		case 1:
			grid_x = MAX( 1, ( grid_x * 3 ) / 4 );
			grid_y = MAX( 1, ( grid_y * 3 ) / 4 );
			grid_z = MAX( 1, ( grid_z * 3 ) / 4 );
			break;
		case 3:
			grid_z = MIN( 256, MAX( 1, ( grid_z * 5 ) / 4 ) );
			break;
		default:
			break;
	}

	grid_x = MAX( 1, (int)( (float)grid_x * resolution_scale + 0.5f ) );
	grid_y = MAX( 1, (int)( (float)grid_y * resolution_scale + 0.5f ) );

	switch ( fluid_quality ) {
		case 0: fluid_quality_scale = 0.5f; break;
		case 1: fluid_quality_scale = 0.75f; break;
		case 3: fluid_quality_scale = 1.25f; break;
		default: fluid_quality_scale = 1.0f; break;
	}
	fluid_x = MAX( 8, (int)( (float)grid_x * fluid_resolution_scale * fluid_quality_scale + 0.5f ) );
	fluid_y = MAX( 8, (int)( (float)grid_y * fluid_resolution_scale * fluid_quality_scale + 0.5f ) );
	fluid_x = MIN( fluid_x, 1024 );
	fluid_y = MIN( fluid_y, 1024 );

	vk.froxel_width = (uint32_t)grid_x;
	vk.froxel_height = (uint32_t)grid_y;
	vk.froxel_slices = (uint32_t)grid_z;
	vk.fluid_width = (uint32_t)fluid_x;
	vk.fluid_height = (uint32_t)fluid_y;
	vk.fluid_active_width = vk.fluid_width;
	vk.fluid_active_height = vk.fluid_height;
	vk.fluid_velocity_index = 0;
	vk.fluid_density_index = 0;
	vk.fluid_pressure_index = 0;
	if ( ( vk.froxel_width <= 1 || vk.froxel_height <= 1 || vk.froxel_slices <= 1 ) &&
		( glConfig.vidWidth > 640 || glConfig.vidHeight > 480 ) )
	{
		ri.Printf( PRINT_WARNING, "[VK][fog] suspicious froxel dims %ux%ux%u for screen %dx%d\n",
			vk.froxel_width, vk.froxel_height, vk.froxel_slices, glConfig.vidWidth, glConfig.vidHeight );
	}

	if ( r_fogDebug && r_fogDebug->integer >= 1 ) {
		ri.Printf( PRINT_ALL, "[VK][fog] froxel create/resize %ux%ux%u quality=%d resolutionScale=%.2f fluid=%ux%u fluidQuality=%d fluidScale=%.3f (screen %dx%d)\n",
			vk.froxel_width, vk.froxel_height, vk.froxel_slices, quality, resolution_scale,
			vk.fluid_width, vk.fluid_height, fluid_quality, fluid_resolution_scale, glConfig.vidWidth, glConfig.vidHeight );
	}

	VkImageCreateInfo create_info;
	VkImageCreateInfo create_info_extinction;
	VkImageCreateInfo create_info_fluid_velocity;
	VkImageCreateInfo create_info_fluid_scalar;
	VkImageCreateInfo create_info_telemetry;
	VkMemoryRequirements mem_req;
	VkMemoryAllocateInfo alloc_info;

	Com_Memset( &create_info, 0, sizeof( create_info ) );
	create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	create_info.pNext = NULL;
	create_info.flags = 0;
	create_info.imageType = VK_IMAGE_TYPE_3D;
	create_info.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	create_info.extent.width = vk.froxel_width;
	create_info.extent.height = vk.froxel_height;
	create_info.extent.depth = vk.froxel_slices;
	create_info.mipLevels = 1;
	create_info.arrayLayers = 1;
	create_info.samples = VK_SAMPLE_COUNT_1_BIT;
	create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	create_info.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	create_info.queueFamilyIndexCount = 0;
	create_info.pQueueFamilyIndices = NULL;
	create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	create_info_extinction = create_info;
	create_info_extinction.format = VK_FORMAT_R16_SFLOAT;

	Com_Memset( &create_info_fluid_velocity, 0, sizeof( create_info_fluid_velocity ) );
	create_info_fluid_velocity.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	create_info_fluid_velocity.imageType = VK_IMAGE_TYPE_2D;
	create_info_fluid_velocity.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	create_info_fluid_velocity.extent.width = vk.fluid_width;
	create_info_fluid_velocity.extent.height = vk.fluid_height;
	create_info_fluid_velocity.extent.depth = 1;
	create_info_fluid_velocity.mipLevels = 1;
	create_info_fluid_velocity.arrayLayers = 1;
	create_info_fluid_velocity.samples = VK_SAMPLE_COUNT_1_BIT;
	create_info_fluid_velocity.tiling = VK_IMAGE_TILING_OPTIMAL;
	create_info_fluid_velocity.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	create_info_fluid_velocity.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	create_info_fluid_velocity.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	create_info_fluid_scalar = create_info_fluid_velocity;
	create_info_fluid_scalar.format = VK_FORMAT_R16_SFLOAT;

	create_info_telemetry = create_info_fluid_velocity;
	create_info_telemetry.format = VK_FORMAT_R32_UINT;
	create_info_telemetry.extent.width = VK_VOLUMETRIC_TELEMETRY_COUNTERS;
	create_info_telemetry.extent.height = 1;
	create_info_telemetry.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	VK_CHECK( qvkCreateImage( vk.device, &create_info, NULL, &vk.froxel_volume_image ) );

	qvkGetImageMemoryRequirements( vk.device, vk.froxel_volume_image, &mem_req );
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.froxel_volume_memory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, vk.froxel_volume_image, vk.froxel_volume_memory, 0 ) );

	VK_CHECK( qvkCreateImage( vk.device, &create_info, NULL, &vk.froxel_history_image ) );

	qvkGetImageMemoryRequirements( vk.device, vk.froxel_history_image, &mem_req );
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.froxel_history_memory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, vk.froxel_history_image, vk.froxel_history_memory, 0 ) );

	VK_CHECK( qvkCreateImage( vk.device, &create_info, NULL, &vk.froxel_light_image ) );

	qvkGetImageMemoryRequirements( vk.device, vk.froxel_light_image, &mem_req );
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.froxel_light_memory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, vk.froxel_light_image, vk.froxel_light_memory, 0 ) );

	VK_CHECK( qvkCreateImage( vk.device, &create_info_extinction, NULL, &vk.froxel_extinction_image ) );

	qvkGetImageMemoryRequirements( vk.device, vk.froxel_extinction_image, &mem_req );
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.froxel_extinction_memory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, vk.froxel_extinction_image, vk.froxel_extinction_memory, 0 ) );

	VK_CHECK( qvkCreateImage( vk.device, &create_info, NULL, &vk.froxel_clamp_image ) );

	qvkGetImageMemoryRequirements( vk.device, vk.froxel_clamp_image, &mem_req );
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.froxel_clamp_memory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, vk.froxel_clamp_image, vk.froxel_clamp_memory, 0 ) );

	for ( int i = 0; i < 2; i++ ) {
		VK_CHECK( qvkCreateImage( vk.device, &create_info_fluid_velocity, NULL, &vk.fluid_velocity_images[i] ) );
		qvkGetImageMemoryRequirements( vk.device, vk.fluid_velocity_images[i], &mem_req );
		alloc_info.allocationSize = mem_req.size;
		alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.fluid_velocity_memory[i] ) );
		VK_CHECK( qvkBindImageMemory( vk.device, vk.fluid_velocity_images[i], vk.fluid_velocity_memory[i], 0 ) );

		VK_CHECK( qvkCreateImage( vk.device, &create_info_fluid_scalar, NULL, &vk.fluid_density_images[i] ) );
		qvkGetImageMemoryRequirements( vk.device, vk.fluid_density_images[i], &mem_req );
		alloc_info.allocationSize = mem_req.size;
		alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.fluid_density_memory[i] ) );
		VK_CHECK( qvkBindImageMemory( vk.device, vk.fluid_density_images[i], vk.fluid_density_memory[i], 0 ) );

		VK_CHECK( qvkCreateImage( vk.device, &create_info_fluid_scalar, NULL, &vk.fluid_pressure_images[i] ) );
		qvkGetImageMemoryRequirements( vk.device, vk.fluid_pressure_images[i], &mem_req );
		alloc_info.allocationSize = mem_req.size;
		alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.fluid_pressure_memory[i] ) );
		VK_CHECK( qvkBindImageMemory( vk.device, vk.fluid_pressure_images[i], vk.fluid_pressure_memory[i], 0 ) );
	}

	VK_CHECK( qvkCreateImage( vk.device, &create_info_fluid_scalar, NULL, &vk.fluid_divergence_image ) );
	qvkGetImageMemoryRequirements( vk.device, vk.fluid_divergence_image, &mem_req );
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.fluid_divergence_memory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, vk.fluid_divergence_image, vk.fluid_divergence_memory, 0 ) );

	VK_CHECK( qvkCreateImage( vk.device, &create_info_telemetry, NULL, &vk.volumetric_telemetry_image ) );
	qvkGetImageMemoryRequirements( vk.device, vk.volumetric_telemetry_image, &mem_req );
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.volumetric_telemetry_memory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, vk.volumetric_telemetry_image, vk.volumetric_telemetry_memory, 0 ) );

	VkImageViewCreateInfo view_info;
	Com_Memset( &view_info, 0, sizeof( view_info ) );
	view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_info.pNext = NULL;
	view_info.flags = 0;
	view_info.viewType = VK_IMAGE_VIEW_TYPE_3D;
	view_info.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	view_info.subresourceRange.baseMipLevel = 0;
	view_info.subresourceRange.levelCount = 1;
	view_info.subresourceRange.baseArrayLayer = 0;
	view_info.subresourceRange.layerCount = 1;

	view_info.image = vk.froxel_volume_image;
	VK_CHECK( qvkCreateImageView( vk.device, &view_info, NULL, &vk.froxel_volume_view ) );

	view_info.image = vk.froxel_history_image;
	VK_CHECK( qvkCreateImageView( vk.device, &view_info, NULL, &vk.froxel_history_view ) );

	view_info.image = vk.froxel_light_image;
	VK_CHECK( qvkCreateImageView( vk.device, &view_info, NULL, &vk.froxel_light_view ) );

	view_info.image = vk.froxel_extinction_image;
	view_info.format = VK_FORMAT_R16_SFLOAT;
	VK_CHECK( qvkCreateImageView( vk.device, &view_info, NULL, &vk.froxel_extinction_view ) );

	view_info.image = vk.froxel_clamp_image;
	view_info.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	VK_CHECK( qvkCreateImageView( vk.device, &view_info, NULL, &vk.froxel_clamp_view ) );

	VkImageViewCreateInfo fluid_view_info;
	Com_Memset( &fluid_view_info, 0, sizeof( fluid_view_info ) );
	fluid_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	fluid_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	fluid_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	fluid_view_info.subresourceRange.baseMipLevel = 0;
	fluid_view_info.subresourceRange.levelCount = 1;
	fluid_view_info.subresourceRange.baseArrayLayer = 0;
	fluid_view_info.subresourceRange.layerCount = 1;
	fluid_view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	fluid_view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	fluid_view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	fluid_view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

	for ( int i = 0; i < 2; i++ ) {
		fluid_view_info.image = vk.fluid_velocity_images[i];
		fluid_view_info.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		VK_CHECK( qvkCreateImageView( vk.device, &fluid_view_info, NULL, &vk.fluid_velocity_views[i] ) );

		fluid_view_info.image = vk.fluid_density_images[i];
		fluid_view_info.format = VK_FORMAT_R16_SFLOAT;
		VK_CHECK( qvkCreateImageView( vk.device, &fluid_view_info, NULL, &vk.fluid_density_views[i] ) );

		fluid_view_info.image = vk.fluid_pressure_images[i];
		fluid_view_info.format = VK_FORMAT_R16_SFLOAT;
		VK_CHECK( qvkCreateImageView( vk.device, &fluid_view_info, NULL, &vk.fluid_pressure_views[i] ) );
	}

	fluid_view_info.image = vk.fluid_divergence_image;
	fluid_view_info.format = VK_FORMAT_R16_SFLOAT;
	VK_CHECK( qvkCreateImageView( vk.device, &fluid_view_info, NULL, &vk.fluid_divergence_view ) );

	fluid_view_info.image = vk.volumetric_telemetry_image;
	fluid_view_info.format = VK_FORMAT_R32_UINT;
	VK_CHECK( qvkCreateImageView( vk.device, &fluid_view_info, NULL, &vk.volumetric_telemetry_view ) );

	VkCommandBuffer command_buffer = vk_begin_command_buffer();
	VkImageSubresourceRange fluid_clear_range;
	VkClearColorValue fluid_clear_color;
	record_image_layout_transition( command_buffer, vk.froxel_volume_image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
	record_image_layout_transition( command_buffer, vk.froxel_history_image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, 0 );
	record_image_layout_transition( command_buffer, vk.froxel_light_image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, 0 );
	record_image_layout_transition( command_buffer, vk.froxel_extinction_image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
	record_image_layout_transition( command_buffer, vk.froxel_clamp_image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, 0 );
	for ( int i = 0; i < 2; i++ ) {
		record_image_layout_transition( command_buffer, vk.fluid_velocity_images[i], VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, 0 );
		record_image_layout_transition( command_buffer, vk.fluid_density_images[i], VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, 0 );
		record_image_layout_transition( command_buffer, vk.fluid_pressure_images[i], VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, 0 );
	}
	record_image_layout_transition( command_buffer, vk.fluid_divergence_image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, 0 );
	record_image_layout_transition( command_buffer, vk.volumetric_telemetry_image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, 0 );
	Com_Memset( &fluid_clear_range, 0, sizeof( fluid_clear_range ) );
	fluid_clear_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	fluid_clear_range.levelCount = 1;
	fluid_clear_range.layerCount = 1;
	Com_Memset( &fluid_clear_color, 0, sizeof( fluid_clear_color ) );
	for ( int i = 0; i < 2; i++ ) {
		qvkCmdClearColorImage( command_buffer, vk.fluid_velocity_images[i], VK_IMAGE_LAYOUT_GENERAL, &fluid_clear_color, 1, &fluid_clear_range );
		qvkCmdClearColorImage( command_buffer, vk.fluid_density_images[i], VK_IMAGE_LAYOUT_GENERAL, &fluid_clear_color, 1, &fluid_clear_range );
		qvkCmdClearColorImage( command_buffer, vk.fluid_pressure_images[i], VK_IMAGE_LAYOUT_GENERAL, &fluid_clear_color, 1, &fluid_clear_range );
	}
	qvkCmdClearColorImage( command_buffer, vk.fluid_divergence_image, VK_IMAGE_LAYOUT_GENERAL, &fluid_clear_color, 1, &fluid_clear_range );
	qvkCmdClearColorImage( command_buffer, vk.volumetric_telemetry_image, VK_IMAGE_LAYOUT_GENERAL, &fluid_clear_color, 1, &fluid_clear_range );
	vk_end_command_buffer( command_buffer, __func__ );

	vk_create_fog_noise_texture();
}

static void vk_destroy_attachments( void )
{
	uint32_t i;

	vk_destroy_volumetric_params_buffer();
	vk_destroy_postfx_params_buffers();
	vk_destroy_froxel_images();
	vk_destroy_sun_shadow_resources();
	vk_destroy_local_shadow_resources();
	vk.volumetric_compute_descriptor = VK_NULL_HANDLE;
	vk.volumetric_composite_descriptor = VK_NULL_HANDLE;
	vk.volumetric_depth_resolve_descriptor = VK_NULL_HANDLE;
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ )
		vk.luminance_descriptor[i] = VK_NULL_HANDLE;
	vk.volumetric_fluid_descriptor = VK_NULL_HANDLE;

	if ( vk.luminance_staging_buffer != VK_NULL_HANDLE ) {
		qvkUnmapMemory( vk.device, vk.luminance_staging_memory );
		qvkDestroyBuffer( vk.device, vk.luminance_staging_buffer, NULL );
		qvkFreeMemory( vk.device, vk.luminance_staging_memory, NULL );
		vk.luminance_staging_buffer = VK_NULL_HANDLE;
		vk.luminance_staging_memory = VK_NULL_HANDLE;
		vk.luminance_staging_ptr = NULL;
	}
	if ( vk.luminance_image != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.luminance_image_view, NULL );
		qvkDestroyImage( vk.device, vk.luminance_image, NULL );
		vk.luminance_image = VK_NULL_HANDLE;
		vk.luminance_image_view = VK_NULL_HANDLE;
	}

	if ( vk.bloom_image[0] ) {
		for ( i = 0; i < ARRAY_LEN( vk.bloom_image ); i++ ) {
			qvkDestroyImage( vk.device, vk.bloom_image[i], NULL );
			qvkDestroyImageView( vk.device, vk.bloom_image_view[i], NULL );
			vk.bloom_image[i] = VK_NULL_HANDLE;
			vk.bloom_image_view[i] = VK_NULL_HANDLE;
		}
	}

	if ( vk.ssao_image ) {
		qvkDestroyImage( vk.device, vk.ssao_image, NULL );
		qvkDestroyImageView( vk.device, vk.ssao_image_view, NULL );
		vk.ssao_image = VK_NULL_HANDLE;
		vk.ssao_image_view = VK_NULL_HANDLE;
	}

	if ( vk.ssao_blur_image ) {
		qvkDestroyImage( vk.device, vk.ssao_blur_image, NULL );
		qvkDestroyImageView( vk.device, vk.ssao_blur_image_view, NULL );
		vk.ssao_blur_image = VK_NULL_HANDLE;
		vk.ssao_blur_image_view = VK_NULL_HANDLE;
	}
	if ( vk.oit_accum_image ) {
		qvkDestroyImage( vk.device, vk.oit_accum_image, NULL );
		qvkDestroyImageView( vk.device, vk.oit_accum_image_view, NULL );
		vk.oit_accum_image = VK_NULL_HANDLE;
		vk.oit_accum_image_view = VK_NULL_HANDLE;
	}
	if ( vk.oit_reveal_image ) {
		qvkDestroyImage( vk.device, vk.oit_reveal_image, NULL );
		qvkDestroyImageView( vk.device, vk.oit_reveal_image_view, NULL );
		vk.oit_reveal_image = VK_NULL_HANDLE;
		vk.oit_reveal_image_view = VK_NULL_HANDLE;
	}

	if ( vk.ssr_image ) {
		qvkDestroyImage( vk.device, vk.ssr_image, NULL );
		qvkDestroyImageView( vk.device, vk.ssr_image_view, NULL );
		vk.ssr_image = VK_NULL_HANDLE;
		vk.ssr_image_view = VK_NULL_HANDLE;
	}

	if ( vk.color_image ) {
		qvkDestroyImage( vk.device, vk.color_image, NULL );
		qvkDestroyImageView( vk.device, vk.color_image_view, NULL );
		vk.color_image = VK_NULL_HANDLE;
		vk.color_image_view = VK_NULL_HANDLE;
		vk.post_fog_color_source = VK_NULL_HANDLE;
		vk.scene_post_fog_color_source = VK_NULL_HANDLE;
	}
	if ( vk.ui_overlay_image ) {
		qvkDestroyImage( vk.device, vk.ui_overlay_image, NULL );
		qvkDestroyImageView( vk.device, vk.ui_overlay_image_view, NULL );
		vk.ui_overlay_image = VK_NULL_HANDLE;
		vk.ui_overlay_image_view = VK_NULL_HANDLE;
	}
	if ( vk.fog_scene_image ) {
		qvkDestroyImage( vk.device, vk.fog_scene_image, NULL );
		qvkDestroyImageView( vk.device, vk.fog_scene_image_view, NULL );
		vk.fog_scene_image = VK_NULL_HANDLE;
		vk.fog_scene_image_view = VK_NULL_HANDLE;
	}
	if ( vk.volumetric_depth_image ) {
		qvkDestroyImage( vk.device, vk.volumetric_depth_image, NULL );
		qvkDestroyImageView( vk.device, vk.volumetric_depth_view, NULL );
		vk.volumetric_depth_image = VK_NULL_HANDLE;
		vk.volumetric_depth_view = VK_NULL_HANDLE;
	}
	if ( vk.motion_vector_image ) {
		qvkDestroyImage( vk.device, vk.motion_vector_image, NULL );
		qvkDestroyImageView( vk.device, vk.motion_vector_view, NULL );
		vk.motion_vector_image = VK_NULL_HANDLE;
		vk.motion_vector_view = VK_NULL_HANDLE;
	}
	if ( vk.motion_vector_msaa_image ) {
		qvkDestroyImage( vk.device, vk.motion_vector_msaa_image, NULL );
		qvkDestroyImageView( vk.device, vk.motion_vector_msaa_view, NULL );
		vk.motion_vector_msaa_image = VK_NULL_HANDLE;
		vk.motion_vector_msaa_view = VK_NULL_HANDLE;
	}

	if ( vk.smaa_edge_image ) {
		qvkDestroyImage( vk.device, vk.smaa_edge_image, NULL );
		qvkDestroyImageView( vk.device, vk.smaa_edge_image_view, NULL );
		vk.smaa_edge_image = VK_NULL_HANDLE;
		vk.smaa_edge_image_view = VK_NULL_HANDLE;
	}

	if ( vk.smaa_blend_image ) {
		qvkDestroyImage( vk.device, vk.smaa_blend_image, NULL );
		qvkDestroyImageView( vk.device, vk.smaa_blend_image_view, NULL );
		vk.smaa_blend_image = VK_NULL_HANDLE;
		vk.smaa_blend_image_view = VK_NULL_HANDLE;
	}

	if ( vk.smaa_output_image ) {
		qvkDestroyImage( vk.device, vk.smaa_output_image, NULL );
		qvkDestroyImageView( vk.device, vk.smaa_output_image_view, NULL );
		vk.smaa_output_image = VK_NULL_HANDLE;
		vk.smaa_output_image_view = VK_NULL_HANDLE;
	}
	for ( i = 0; i < 2; i++ ) {
		if ( vk.taa_history_image[i] ) {
			qvkDestroyImage( vk.device, vk.taa_history_image[i], NULL );
			vk.taa_history_image[i] = VK_NULL_HANDLE;
		}
		if ( vk.taa_history_image_view[i] ) {
			qvkDestroyImageView( vk.device, vk.taa_history_image_view[i], NULL );
			vk.taa_history_image_view[i] = VK_NULL_HANDLE;
		}
		vk.taa_history_descriptor[i] = VK_NULL_HANDLE;
	}

	if ( vk.msaa_image ) {
		qvkDestroyImage( vk.device, vk.msaa_image, NULL );
		qvkDestroyImageView( vk.device, vk.msaa_image_view, NULL );
		vk.msaa_image = VK_NULL_HANDLE;
		vk.msaa_image_view = VK_NULL_HANDLE;
	}
	if ( vk.ui_overlay_msaa_image ) {
		qvkDestroyImage( vk.device, vk.ui_overlay_msaa_image, NULL );
		qvkDestroyImageView( vk.device, vk.ui_overlay_msaa_image_view, NULL );
		vk.ui_overlay_msaa_image = VK_NULL_HANDLE;
		vk.ui_overlay_msaa_image_view = VK_NULL_HANDLE;
	}

	if ( vk.depth_image_view_sample != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.depth_image_view_sample, NULL );
		vk.depth_image_view_sample = VK_NULL_HANDLE;
	}
	qvkDestroyImageView( vk.device, vk.depth_image_view, NULL );
	qvkDestroyImage( vk.device, vk.depth_image, NULL );
	vk.depth_image = VK_NULL_HANDLE;
	vk.depth_image_view = VK_NULL_HANDLE;
	vk.depth_image_layout = VK_IMAGE_LAYOUT_UNDEFINED;

	if ( vk.screenMap.color_image ) {
		qvkDestroyImage( vk.device, vk.screenMap.color_image, NULL );
		qvkDestroyImageView( vk.device, vk.screenMap.color_image_view, NULL );
		vk.screenMap.color_image = VK_NULL_HANDLE;
		vk.screenMap.color_image_view = VK_NULL_HANDLE;
	}

	if ( vk.screenMap.motion_image ) {
		qvkDestroyImage( vk.device, vk.screenMap.motion_image, NULL );
		qvkDestroyImageView( vk.device, vk.screenMap.motion_image_view, NULL );
		vk.screenMap.motion_image = VK_NULL_HANDLE;
		vk.screenMap.motion_image_view = VK_NULL_HANDLE;
	}

	if ( vk.screenMap.color_image_msaa ) {
		qvkDestroyImage( vk.device, vk.screenMap.color_image_msaa, NULL );
		qvkDestroyImageView( vk.device, vk.screenMap.color_image_view_msaa, NULL );
		vk.screenMap.color_image_msaa = VK_NULL_HANDLE;
		vk.screenMap.color_image_view_msaa = VK_NULL_HANDLE;
	}

	if ( vk.screenMap.motion_image_msaa ) {
		qvkDestroyImage( vk.device, vk.screenMap.motion_image_msaa, NULL );
		qvkDestroyImageView( vk.device, vk.screenMap.motion_image_view_msaa, NULL );
		vk.screenMap.motion_image_msaa = VK_NULL_HANDLE;
		vk.screenMap.motion_image_view_msaa = VK_NULL_HANDLE;
	}

	if ( vk.screenMap.depth_image ) {
		qvkDestroyImage( vk.device, vk.screenMap.depth_image, NULL );
		qvkDestroyImageView( vk.device, vk.screenMap.depth_image_view, NULL );
		vk.screenMap.depth_image = VK_NULL_HANDLE;
		vk.screenMap.depth_image_view = VK_NULL_HANDLE;
	}

	if ( vk.capture.image ) {
		qvkDestroyImage( vk.device, vk.capture.image, NULL );
		qvkDestroyImageView( vk.device, vk.capture.image_view, NULL );
		vk.capture.image = VK_NULL_HANDLE;
		vk.capture.image_view = VK_NULL_HANDLE;
	}

#ifdef VK_PBR_BRDFLUT
    if ( vk.brdflut_image_view ) {
        qvkDestroyImage( vk.device, vk.brdflut_image, NULL );
        qvkDestroyImageView( vk.device, vk.brdflut_image_view, NULL );
        vk.brdflut_image = VK_NULL_HANDLE;
        vk.brdflut_image_view = VK_NULL_HANDLE;
    }
#endif

	// render world to cubemap
    if ( vk.cubeMap.color_image ) {
        qvkDestroyImage(vk.device, vk.cubeMap.color_image, NULL);
        vk.cubeMap.color_image = VK_NULL_HANDLE;
    }
	
    if ( vk.cubeMap.color_image_msaa ) {
        qvkDestroyImage(vk.device, vk.cubeMap.color_image_msaa, NULL);
        vk.cubeMap.color_image_msaa = VK_NULL_HANDLE;
    }
    
    for ( i = 0; i < ARRAY_LEN(vk.cubeMap.color_image_view); i++) {      
        qvkDestroyImageView(vk.device, vk.cubeMap.color_image_view[i], NULL);
        qvkDestroyImageView(vk.device, vk.cubeMap.color_image_view_msaa[i], NULL);
        vk.cubeMap.color_image_view[i] = VK_NULL_HANDLE;
        vk.cubeMap.color_image_view_msaa[i] = VK_NULL_HANDLE;
    }
    if ( vk.cubeMap.depth_image ) {
        qvkDestroyImage(vk.device, vk.cubeMap.depth_image, NULL);
        qvkDestroyImageView(vk.device, vk.cubeMap.depth_image_view, NULL);
        vk.cubeMap.depth_image = VK_NULL_HANDLE;
        vk.cubeMap.depth_image_view = VK_NULL_HANDLE;
    }

	for ( i = 0; i < vk.image_memory_count; i++ ) {
		qvkFreeMemory( vk.device, vk.image_memory[i], NULL );
	}

	vk.image_memory_count = 0;
}


static void vk_destroy_render_passes( void )
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

static void vk_destroy_pipelines( qboolean resetCounter )
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
	vk_destroy_volumetric_pipelines();
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

void vk_create_image( image_t *image, int width, int height, int mip_levels ) {

	VkFormat			format = (VkFormat)image->internalFormat;
	VkImageCreateFlags	image_flags = 0;
	VkImageViewType		view_type = (VkImageViewType)VK_IMAGE_VIEW_TYPE_2D;

	if ( image->handle ) {
		qvkDestroyImage( vk.device, image->handle, NULL );
		image->handle = VK_NULL_HANDLE;
	}

	if ( image->view ) {
		qvkDestroyImageView( vk.device, image->view, NULL );
		image->view = VK_NULL_HANDLE;
	}

	if ( image->flags & IMGFLAG_CUBEMAP ) {
		image_flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		view_type = VK_IMAGE_VIEW_TYPE_CUBE;
	}

	// create image
	{
		VkImageCreateInfo desc;

		desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = image_flags;
		desc.imageType = VK_IMAGE_TYPE_2D;
		desc.format = format;
		desc.extent.width = width;
		desc.extent.height = height;
		desc.extent.depth = 1;
		desc.mipLevels = mip_levels;
		desc.arrayLayers = image->layers;
		desc.samples = VK_SAMPLE_COUNT_1_BIT;
		desc.tiling = VK_IMAGE_TILING_OPTIMAL;
		desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		desc.queueFamilyIndexCount = 0;
		desc.pQueueFamilyIndices = NULL;
		desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VK_CHECK( qvkCreateImage( vk.device, &desc, NULL, &image->handle ) );

		vk_allocate_and_bind_image_memory( image->handle );
	}

	// create image view
	{
		VkImageViewCreateInfo desc;

		desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.image = image->handle;
		desc.viewType = (VkImageViewType)view_type;
		desc.format = format;
		desc.components = textureMapTypes[image->type].swizzle;
		desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		desc.subresourceRange.baseMipLevel = 0;
		desc.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		desc.subresourceRange.baseArrayLayer = 0;
		desc.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

		VK_CHECK( qvkCreateImageView( vk.device, &desc, NULL, &image->view ) );
	}

	// create associated descriptor set
	if ( image->descriptor == VK_NULL_HANDLE ) {
		VkDescriptorSetAllocateInfo desc;

		desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		desc.pNext = NULL;
		desc.descriptorPool = vk.descriptor_pool;
		desc.descriptorSetCount = 1;
		desc.pSetLayouts = &vk.set_layout_sampler;

		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &desc, &image->descriptor ) );
	}

	vk_update_descriptor_set( image, mip_levels > 1 ? qtrue : qfalse );

	SET_OBJECT_NAME( image->handle, image->imgName, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	SET_OBJECT_NAME( image->view, image->imgName, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	SET_OBJECT_NAME( image->descriptor, image->imgName, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
}


static byte *resample_image_data( const int target_format, byte *data, const int data_size, int *bytes_per_pixel )
{
	byte* buffer;
	uint16_t* p;
	int i, n;

	switch ( target_format ) {
	case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
		buffer = (byte*)ri.Hunk_AllocateTempMemory( data_size / 2 );
		p = (uint16_t*)buffer;
		for ( i = 0; i < data_size; i += 4, p++ ) {
			byte r = data[i + 0];
			byte g = data[i + 1];
			byte b = data[i + 2];
			byte a = data[i + 3];
			*p = (uint32_t)((a / 255.0) * 15.0 + 0.5) |
				((uint32_t)((r / 255.0) * 15.0 + 0.5) << 4) |
				((uint32_t)((g / 255.0) * 15.0 + 0.5) << 8) |
				((uint32_t)((b / 255.0) * 15.0 + 0.5) << 12);
		}
		*bytes_per_pixel = 2;
		return buffer; // must be freed after upload!

	case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
		buffer = (byte*)ri.Hunk_AllocateTempMemory( data_size / 2 );
		p = (uint16_t*)buffer;
		for ( i = 0; i < data_size; i += 4, p++ ) {
			byte r = data[i + 0];
			byte g = data[i + 1];
			byte b = data[i + 2];
			*p = (uint32_t)((b / 255.0) * 31.0 + 0.5) |
				((uint32_t)((g / 255.0) * 31.0 + 0.5) << 5) |
				((uint32_t)((r / 255.0) * 31.0 + 0.5) << 10) |
				(1 << 15);
		}
		*bytes_per_pixel = 2;
		return buffer; // must be freed after upload!

	case VK_FORMAT_B8G8R8A8_UNORM:
		buffer = (byte*)ri.Hunk_AllocateTempMemory( data_size );
		for ( i = 0; i < data_size; i += 4 ) {
			buffer[i + 0] = data[i + 2];
			buffer[i + 1] = data[i + 1];
			buffer[i + 2] = data[i + 0];
			buffer[i + 3] = data[i + 3];
		}
		*bytes_per_pixel = 4;
		return buffer;

	case VK_FORMAT_R8G8B8_UNORM: {
		buffer = (byte*)ri.Hunk_AllocateTempMemory( (data_size * 3) / 4 );
		for ( i = 0, n = 0; i < data_size; i += 4, n += 3 ) {
			buffer[n + 0] = data[i + 0];
			buffer[n + 1] = data[i + 1];
			buffer[n + 2] = data[i + 2];
		}
		*bytes_per_pixel = 3;
		return buffer;
	}

	default:
		*bytes_per_pixel = 4;
		return data;
	}
}


void vk_upload_image_data( image_t *image, int x, int y, int width, int height, int mipmaps, byte *pixels, int size, qboolean update ) {

	VkCommandBuffer   command_buffer;
	VkBufferImageCopy regions[16];
	VkBufferImageCopy region;
	byte *buf;
	int n;

	int num_regions = 0;
	int buffer_size = 0;

	buf = resample_image_data( image->internalFormat, pixels, size, &n /*bpp*/ );

	while (qtrue) {
		Com_Memset(&region, 0, sizeof(region));
		region.bufferOffset = buffer_size;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = num_regions;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset.x = x;
		region.imageOffset.y = y;
		region.imageOffset.z = 0;
		region.imageExtent.width = width;
		region.imageExtent.height = height;
		region.imageExtent.depth = 1;

		regions[num_regions] = region;
		num_regions++;

		buffer_size += width * height * n;

		if ( num_regions >= mipmaps || (width == 1 && height == 1) || (size_t) num_regions >= ARRAY_LEN( regions ) )
			break;

		x >>= 1;
		y >>= 1;

		width >>= 1;
		if (width < 1) width = 1;

		height >>= 1;
		if (height < 1) height = 1;
	}

#ifdef USE_UPLOAD_QUEUE
	if ( vk_wait_staging_buffer() ) {
		// wait for vkQueueSubmit() completion before new upload
	}

	if ( vk.staging_buffer.size - vk.staging_buffer.offset < buffer_size ) {
		// try to flush staging buffer and reset offset
		vk_flush_staging_buffer( qfalse );
	}

	if ( vk.staging_buffer.size /* - vk_world.staging_buffer_offset */ < buffer_size ) {
		// if still not enough - reallocate staging buffer
		vk_alloc_staging_buffer( buffer_size );
	}

	for ( n = 0; n < num_regions; n++ ) {
		regions[n].bufferOffset += vk.staging_buffer.offset;
	}

	Com_Memcpy( vk.staging_buffer.ptr + vk.staging_buffer.offset, buf, buffer_size );

	if ( vk.staging_buffer.offset == 0 ) {
		VkCommandBufferBeginInfo begin_info;
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.pNext = NULL;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		begin_info.pInheritanceInfo = NULL;
		VK_CHECK( qvkBeginCommandBuffer( vk.staging_command_buffer, &begin_info ) );
	}

	//ri.Printf( PRINT_WARNING, "batch @%6i + %i %s \n", (int)vk_world.staging_buffer_offset, (int)buffer_size, image->imgName );
	vk.staging_buffer.offset += buffer_size;

	command_buffer = vk.staging_command_buffer;

	if ( update ) {
		record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );
	} else {
		record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT, 0 );
	}

	qvkCmdCopyBufferToImage( command_buffer, vk.staging_buffer.handle, image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, num_regions, regions );

	// final transition after upload comleted
	record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
#else
	if ( vk.staging_buffer.size < (VkDeviceSize) buffer_size ) {
		vk_alloc_staging_buffer( buffer_size );
	}

	Com_Memcpy( vk.staging_buffer.ptr, buf, buffer_size );

	command_buffer = vk_begin_command_buffer();
	// record_buffer_memory_barrier( command_buffer, vk_world.staging_buffer, VK_WHOLE_SIZE, 0, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT );
	if ( update ) {
		record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );
	} else {
		record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT, 0 );
	}
	qvkCmdCopyBufferToImage( command_buffer, vk.staging_buffer.handle, image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, num_regions, regions );
	record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
	vk_end_command_buffer( command_buffer, __func__ );
#endif

	if ( buf != pixels ) {
		ri.Hunk_FreeTempMemory( buf );
	}
}

void vk_upload_cubemap_mip_data( image_t *image, int face_size, int miplevels, const byte *pixels, int size, int bytes_per_pixel, qboolean update ) {
	VkCommandBuffer command_buffer;
	VkBufferImageCopy regions[64];
	int num_regions = 0;
	int buffer_size = 0;
	int mip;
	int face;
	int width = face_size;
	int height = face_size;

	if ( !image || !pixels || !( image->flags & IMGFLAG_CUBEMAP ) || face_size <= 0 || miplevels <= 0 || bytes_per_pixel <= 0 ) {
		return;
	}

	for ( mip = 0; mip < miplevels && num_regions < (int)ARRAY_LEN( regions ); mip++ ) {
		const int face_bytes = width * height * bytes_per_pixel;

		for ( face = 0; face < 6 && num_regions < (int)ARRAY_LEN( regions ); face++ ) {
			VkBufferImageCopy region;

			Com_Memset( &region, 0, sizeof( region ) );
			region.bufferOffset = buffer_size;
			region.bufferRowLength = 0;
			region.bufferImageHeight = 0;
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.mipLevel = mip;
			region.imageSubresource.baseArrayLayer = face;
			region.imageSubresource.layerCount = 1;
			region.imageOffset.x = 0;
			region.imageOffset.y = 0;
			region.imageOffset.z = 0;
			region.imageExtent.width = width;
			region.imageExtent.height = height;
			region.imageExtent.depth = 1;
			regions[num_regions++] = region;

			buffer_size += face_bytes;
		}

		width >>= 1;
		height >>= 1;
		if ( width < 1 ) width = 1;
		if ( height < 1 ) height = 1;
	}

	if ( buffer_size > size ) {
		ri.Printf( PRINT_WARNING, "vk_upload_cubemap_mip_data: buffer underrun for %s (%d > %d)\n",
			image->imgName ? image->imgName : "<unnamed cubemap>", buffer_size, size );
		return;
	}

#ifdef USE_UPLOAD_QUEUE
	if ( vk_wait_staging_buffer() ) {
	}

	if ( vk.staging_buffer.size - vk.staging_buffer.offset < (VkDeviceSize)buffer_size ) {
		vk_flush_staging_buffer( qfalse );
	}

	if ( vk.staging_buffer.size < (VkDeviceSize)buffer_size ) {
		vk_alloc_staging_buffer( buffer_size );
	}

	for ( mip = 0; mip < num_regions; mip++ ) {
		regions[mip].bufferOffset += vk.staging_buffer.offset;
	}

	Com_Memcpy( vk.staging_buffer.ptr + vk.staging_buffer.offset, pixels, buffer_size );

	if ( vk.staging_buffer.offset == 0 ) {
		VkCommandBufferBeginInfo begin_info;
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.pNext = NULL;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		begin_info.pInheritanceInfo = NULL;
		VK_CHECK( qvkBeginCommandBuffer( vk.staging_command_buffer, &begin_info ) );
	}

	vk.staging_buffer.offset += buffer_size;
	command_buffer = vk.staging_command_buffer;
#else
	if ( vk.staging_buffer.size < (VkDeviceSize)buffer_size ) {
		vk_alloc_staging_buffer( buffer_size );
	}

	Com_Memcpy( vk.staging_buffer.ptr, pixels, buffer_size );
	command_buffer = vk_begin_command_buffer();
#endif

	if ( update ) {
		record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );
	} else {
		record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT, 0 );
	}

	qvkCmdCopyBufferToImage( command_buffer, vk.staging_buffer.handle, image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, num_regions, regions );

	record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );

#ifndef USE_UPLOAD_QUEUE
	vk_end_command_buffer( command_buffer, __func__ );
#endif
}

/*
 * Upload pre-compressed BC7 block data. pixels layout: mip0, mip1, ... (no resampling).
 */
void vk_upload_compressed_image_data( image_t *image, int width, int height, int miplevels, byte *pixels, int size, qboolean update ) {
	VkCommandBuffer   command_buffer;
	VkBufferImageCopy regions[16];
	VkBufferImageCopy region;
	int num_regions = 0;
	int buffer_offset = 0;
	int w, h;
	int n;

	for ( n = 0; n < miplevels && (size_t)n < ARRAY_LEN( regions ); n++ ) {
		w = width >> n;
		h = height >> n;
		if ( w < 1 ) w = 1;
		if ( h < 1 ) h = 1;

		Com_Memset( &region, 0, sizeof( region ) );
		region.bufferOffset = buffer_offset;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = n;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset.x = 0;
		region.imageOffset.y = 0;
		region.imageOffset.z = 0;
		region.imageExtent.width = w;
		region.imageExtent.height = h;
		region.imageExtent.depth = 1;

		regions[num_regions] = region;
		num_regions++;

		/* BC7: 4x4 blocks, 16 bytes per block */
		buffer_offset += ( ( w + 3 ) / 4 ) * ( ( h + 3 ) / 4 ) * 16;

		if ( w == 1 && h == 1 )
			break;
	}

#ifdef USE_UPLOAD_QUEUE
	if ( vk_wait_staging_buffer() ) {
		vk_flush_staging_buffer( qfalse );
	}
	if ( vk.staging_buffer.size - vk.staging_buffer.offset < (VkDeviceSize)size ) {
		vk_flush_staging_buffer( qfalse );
	}
	if ( vk.staging_buffer.size < (VkDeviceSize)size ) {
		vk_alloc_staging_buffer( size );
	}
	for ( n = 0; n < num_regions; n++ ) {
		regions[n].bufferOffset += vk.staging_buffer.offset;
	}
	Com_Memcpy( vk.staging_buffer.ptr + vk.staging_buffer.offset, pixels, size );
	if ( vk.staging_buffer.offset == 0 ) {
		VkCommandBufferBeginInfo begin_info;
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.pNext = NULL;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		begin_info.pInheritanceInfo = NULL;
		VK_CHECK( qvkBeginCommandBuffer( vk.staging_command_buffer, &begin_info ) );
	}
	vk.staging_buffer.offset += size;
	command_buffer = vk.staging_command_buffer;
#else
	if ( vk.staging_buffer.size < (VkDeviceSize)size ) {
		vk_alloc_staging_buffer( size );
	}
	Com_Memcpy( vk.staging_buffer.ptr, pixels, size );
	command_buffer = vk_begin_command_buffer();
#endif

	if ( update ) {
		record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );
	} else {
		record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT, 0 );
	}
	qvkCmdCopyBufferToImage( command_buffer, vk.staging_buffer.handle, image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, num_regions, regions );
	record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );

#ifndef USE_UPLOAD_QUEUE
	vk_end_command_buffer( command_buffer, __func__ );
#endif
}

void vk_update_descriptor_set( image_t *image, qboolean mipmap ) {
	Vk_Sampler_Def sampler_def;
	VkDescriptorImageInfo image_info;
	VkWriteDescriptorSet descriptor_write;

	Com_Memset( &sampler_def, 0, sizeof( sampler_def ) );

	sampler_def.address_mode = image->wrapClampMode;

	if ( mipmap ) {
		sampler_def.gl_mag_filter = gl_filter_max;
		sampler_def.gl_min_filter = gl_filter_min;
	} else {
		sampler_def.gl_mag_filter = GL_LINEAR;
		sampler_def.gl_min_filter = GL_LINEAR;
		// no anisotropy without mipmaps
		sampler_def.noAnisotropy = qtrue;
	}

	image_info.sampler = vk_find_sampler( &sampler_def );
	image_info.imageView = image->view;
	image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	image->vk_sampler = image_info.sampler;

	descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptor_write.dstSet = image->descriptor;
	descriptor_write.dstBinding = 0;
	descriptor_write.dstArrayElement = 0;
	descriptor_write.descriptorCount = 1;
	descriptor_write.pNext = NULL;
	descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptor_write.pImageInfo = &image_info;
	descriptor_write.pBufferInfo = NULL;
	descriptor_write.pTexelBufferView = NULL;

	qvkUpdateDescriptorSets( vk.device, 1, &descriptor_write, 0, NULL );
}


void vk_destroy_image_resources( VkImage *image, VkImageView *imageView )
{
	if ( vk.device == VK_NULL_HANDLE || qvkDestroyImage == NULL || qvkDestroyImageView == NULL ) {
		return;
	}
	if ( image != NULL ) {
		if ( *image != VK_NULL_HANDLE ) {
			qvkDestroyImage( vk.device, *image, NULL );
			*image = VK_NULL_HANDLE;
		}
	}
	if ( imageView != NULL ) {
		if ( *imageView != VK_NULL_HANDLE ) {
			qvkDestroyImageView( vk.device, *imageView, NULL );
			*imageView = VK_NULL_HANDLE;
		}
	}
}


static void set_shader_stage_desc(VkPipelineShaderStageCreateInfo *desc, VkShaderStageFlagBits stage, VkShaderModule shader_module, const char *entry) {
	desc->sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	desc->pNext = NULL;
	desc->flags = 0;
	desc->stage = stage;
	desc->module = shader_module;
	desc->pName = entry;
	desc->pSpecializationInfo = NULL;
}


static void vk_create_atmosphere_pipeline( void )
{
	VkPipelineShaderStageCreateInfo shader_stages[2];
	VkPipelineVertexInputStateCreateInfo vertex_input_state;
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
	VkPipelineRasterizationStateCreateInfo rasterization_state;
	VkPipelineDepthStencilStateCreateInfo depth_stencil_state;
	VkPipelineViewportStateCreateInfo viewport_state;
	VkPipelineMultisampleStateCreateInfo multisample_state;
	VkPipelineColorBlendStateCreateInfo blend_state;
	VkPipelineColorBlendAttachmentState attachment_blend;
	VkDynamicState dynamic_states[2];
	VkPipelineDynamicStateCreateInfo dynamic_state;
	VkGraphicsPipelineCreateInfo create_info;
	VkViewport viewport;
	VkRect2D scissor;

	if ( vk.atmosphere_pipeline ) {
		qvkDestroyPipeline( vk.device, vk.atmosphere_pipeline, NULL );
		vk.atmosphere_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.render_pass.atmosphere == VK_NULL_HANDLE ) return;

	Com_Memset( &shader_stages, 0, sizeof( shader_stages ) );
	shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shader_stages[0].module = vk.modules.gamma_vs;
	shader_stages[0].pName = "main";
	shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shader_stages[1].module = vk_hdr64_active() ? vk.modules.atmosphere_fs_hdr64 : vk.modules.atmosphere_fs;
	shader_stages[1].pName = "main";

	Com_Memset( &vertex_input_state, 0, sizeof( vertex_input_state ) );
	vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	Com_Memset( &input_assembly_state, 0, sizeof( input_assembly_state ) );
	input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

	viewport = (VkViewport){ 0, 0, (float)glConfig.vidWidth, (float)glConfig.vidHeight, 0.0f, 1.0f };
	scissor = (VkRect2D){ {0, 0}, { (uint32_t)glConfig.vidWidth, (uint32_t)glConfig.vidHeight } };
	Com_Memset( &viewport_state, 0, sizeof( viewport_state ) );
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = &viewport;
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = &scissor;

	Com_Memset( &rasterization_state, 0, sizeof( rasterization_state ) );
	rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization_state.cullMode = VK_CULL_MODE_NONE;
	rasterization_state.frontFace = VK_FRONT_FACE_CLOCKWISE;

	Com_Memset( &depth_stencil_state, 0, sizeof( depth_stencil_state ) );
	depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil_state.depthTestEnable = VK_TRUE;
	depth_stencil_state.depthWriteEnable = VK_FALSE;
#ifdef USE_REVERSED_DEPTH
	/* Reversed depth: far=0.0. Pass only where stored==0.0 (sky). Shader outputs gl_FragDepth=0.0. */
	depth_stencil_state.depthCompareOp = VK_COMPARE_OP_EQUAL;
#else
	depth_stencil_state.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
#endif

	Com_Memset( &attachment_blend, 0, sizeof( attachment_blend ) );
	attachment_blend.blendEnable = VK_TRUE;
	attachment_blend.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	attachment_blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	attachment_blend.colorBlendOp = VK_BLEND_OP_ADD;
	attachment_blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	attachment_blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	attachment_blend.alphaBlendOp = VK_BLEND_OP_ADD;
	attachment_blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	Com_Memset( &blend_state, 0, sizeof( blend_state ) );
	blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state.attachmentCount = 1;
	blend_state.pAttachments = &attachment_blend;

	Com_Memset( &multisample_state, 0, sizeof( multisample_state ) );
	multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_state.rasterizationSamples = (VkSampleCountFlagBits)vkSamples;

	dynamic_states[0] = VK_DYNAMIC_STATE_VIEWPORT;
	dynamic_states[1] = VK_DYNAMIC_STATE_SCISSOR;
	Com_Memset( &dynamic_state, 0, sizeof( dynamic_state ) );
	dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.dynamicStateCount = ARRAY_LEN( dynamic_states );
	dynamic_state.pDynamicStates = dynamic_states;

	Com_Memset( &create_info, 0, sizeof( create_info ) );
	create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	create_info.stageCount = 2;
	create_info.pStages = shader_stages;
	create_info.pVertexInputState = &vertex_input_state;
	create_info.pInputAssemblyState = &input_assembly_state;
	create_info.pViewportState = &viewport_state;
	create_info.pRasterizationState = &rasterization_state;
	create_info.pMultisampleState = &multisample_state;
	create_info.pDepthStencilState = &depth_stencil_state;
	create_info.pColorBlendState = &blend_state;
	create_info.pDynamicState = &dynamic_state;
	create_info.layout = vk.pipeline_layout_atmosphere;
	create_info.renderPass = vk.render_pass.atmosphere;
	create_info.subpass = 0;

	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, vk.pipelineCache, 1, &create_info, NULL, &vk.atmosphere_pipeline ) );
	SET_OBJECT_NAME( vk.atmosphere_pipeline, "atmosphere pipeline", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
}

static void vk_create_oit_accum_pipeline( void )
{
	VkPipelineVertexInputStateCreateInfo vertex_input;
	VkVertexInputBindingDescription bindings[3];
	VkVertexInputAttributeDescription attribs[3];
	VkPipelineShaderStageCreateInfo shader_stages[2];
	VkPipelineInputAssemblyStateCreateInfo input_assembly;
	VkPipelineRasterizationStateCreateInfo rasterization;
	VkPipelineMultisampleStateCreateInfo multisample;
	VkPipelineDepthStencilStateCreateInfo depth_stencil;
	VkPipelineColorBlendAttachmentState blend_attachments[2];
	VkPipelineColorBlendStateCreateInfo blend_state;
	VkPipelineViewportStateCreateInfo viewport_state;
	VkDynamicState dynamic_states[3] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamic_state;
	VkGraphicsPipelineCreateInfo create_info;
	VkSpecializationMapEntry spec_entries[1];
	VkSpecializationInfo frag_spec_info;
	int manual_depth_test = vk.msaaActive ? 1 : 0;
	VkViewport viewport = { 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f };
	VkRect2D scissor = { { 0, 0 }, { 1, 1 } };

	if ( vk.pipeline_layout_oit_accum == VK_NULL_HANDLE || vk.render_pass.oit_accum == VK_NULL_HANDLE ||
		vk.modules.oit_accum_vs == VK_NULL_HANDLE || vk.modules.oit_accum_fs == VK_NULL_HANDLE ) {
		return;
	}

	if ( vk.oit_accum_pipeline != VK_NULL_HANDLE ) {
		vk_wait_idle();
		qvkDestroyPipeline( vk.device, vk.oit_accum_pipeline, NULL );
		vk.oit_accum_pipeline = VK_NULL_HANDLE;
	}

	/* Gen vertex layout: position, color, texcoord (TYPE_SIGNLE_TEXTURE) */
	Com_Memset( &vertex_input, 0, sizeof( vertex_input ) );
	Com_Memset( bindings, 0, sizeof( bindings ) );
	bindings[0].binding = 0;
	bindings[0].stride = sizeof( vec4_t );
	bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	bindings[1].binding = 1;
	bindings[1].stride = sizeof( color4ub_t );
	bindings[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	bindings[2].binding = 2;
	bindings[2].stride = sizeof( vec2_t );
	bindings[2].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	Com_Memset( attribs, 0, sizeof( attribs ) );
	attribs[0].location = 0;
	attribs[0].binding = 0;
	attribs[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	attribs[0].offset = 0;
	attribs[1].location = 1;
	attribs[1].binding = 1;
	attribs[1].format = VK_FORMAT_R8G8B8A8_UNORM;
	attribs[1].offset = 0;
	attribs[2].location = 2;
	attribs[2].binding = 2;
	attribs[2].format = VK_FORMAT_R32G32_SFLOAT;
	attribs[2].offset = 0;

	vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input.vertexBindingDescriptionCount = 3;
	vertex_input.pVertexBindingDescriptions = bindings;
	vertex_input.vertexAttributeDescriptionCount = 3;
	vertex_input.pVertexAttributeDescriptions = attribs;

	Com_Memset( shader_stages, 0, sizeof( shader_stages ) );
	shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shader_stages[0].module = vk.modules.oit_accum_vs;
	shader_stages[0].pName = "main";
	shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shader_stages[1].module = vk.modules.oit_accum_fs;
	shader_stages[1].pName = "main";
	spec_entries[0].constantID = 0;
	spec_entries[0].offset = 0;
	spec_entries[0].size = sizeof( int );
	frag_spec_info.mapEntryCount = 1;
	frag_spec_info.pMapEntries = spec_entries;
	frag_spec_info.dataSize = sizeof( int );
	frag_spec_info.pData = &manual_depth_test;
	shader_stages[1].pSpecializationInfo = &frag_spec_info;

	Com_Memset( &input_assembly, 0, sizeof( input_assembly ) );
	input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	input_assembly.primitiveRestartEnable = VK_FALSE;

	Com_Memset( &viewport_state, 0, sizeof( viewport_state ) );
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = &viewport;
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = &scissor;

	Com_Memset( &rasterization, 0, sizeof( rasterization ) );
	rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization.polygonMode = VK_POLYGON_MODE_FILL;
	rasterization.cullMode = VK_CULL_MODE_NONE;
	rasterization.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterization.lineWidth = 1.0f;

	Com_Memset( &multisample, 0, sizeof( multisample ) );
	multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	Com_Memset( &depth_stencil, 0, sizeof( depth_stencil ) );
	depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	/* When MSAA off: depth-test transparents against opaque scene. Weight uses gl_FragCoord.z. */
	if ( vkSamples == VK_SAMPLE_COUNT_1_BIT ) {
		depth_stencil.depthTestEnable = VK_TRUE;
		depth_stencil.depthWriteEnable = VK_FALSE;
		depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	} else {
		depth_stencil.depthTestEnable = VK_FALSE;
		depth_stencil.depthWriteEnable = VK_FALSE;
	}
	depth_stencil.depthBoundsTestEnable = VK_FALSE;
	depth_stencil.stencilTestEnable = VK_FALSE;

	/* RT0: additive weighted accumulation. RT1: multiplicative revealage. */
	Com_Memset( blend_attachments, 0, sizeof( blend_attachments ) );
	blend_attachments[0].blendEnable = VK_TRUE;
	blend_attachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	blend_attachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	blend_attachments[0].colorBlendOp = VK_BLEND_OP_ADD;
	blend_attachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	blend_attachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	blend_attachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
	blend_attachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	blend_attachments[1].blendEnable = VK_TRUE;
	blend_attachments[1].srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	blend_attachments[1].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
	blend_attachments[1].colorBlendOp = VK_BLEND_OP_ADD;
	blend_attachments[1].srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	blend_attachments[1].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blend_attachments[1].alphaBlendOp = VK_BLEND_OP_ADD;
	blend_attachments[1].colorWriteMask = VK_COLOR_COMPONENT_R_BIT;

	Com_Memset( &blend_state, 0, sizeof( blend_state ) );
	blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state.attachmentCount = ARRAY_LEN( blend_attachments );
	blend_state.pAttachments = blend_attachments;

	{
		uint32_t dyn_count = 2;
		if ( vk.colorWriteMaskDynamic ) {
			dynamic_states[dyn_count++] = VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT;
		}
		Com_Memset( &dynamic_state, 0, sizeof( dynamic_state ) );
		dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamic_state.pNext = NULL;
		dynamic_state.flags = 0;
		dynamic_state.dynamicStateCount = dyn_count;
		dynamic_state.pDynamicStates = dynamic_states;
	}

	Com_Memset( &create_info, 0, sizeof( create_info ) );
	create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	create_info.stageCount = 2;
	create_info.pStages = shader_stages;
	create_info.pVertexInputState = &vertex_input;
	create_info.pInputAssemblyState = &input_assembly;
	create_info.pViewportState = &viewport_state;
	create_info.pRasterizationState = &rasterization;
	create_info.pMultisampleState = &multisample;
	create_info.pDepthStencilState = &depth_stencil;
	create_info.pColorBlendState = &blend_state;
	create_info.pDynamicState = &dynamic_state;
	create_info.layout = vk.pipeline_layout_oit_accum;
	create_info.renderPass = vk.render_pass.oit_accum;
	create_info.subpass = 0;

	if ( qvkCreateGraphicsPipelines( vk.device, vk.pipelineCache, 1, &create_info, NULL, &vk.oit_accum_pipeline ) == VK_SUCCESS ) {
		SET_OBJECT_NAME( vk.oit_accum_pipeline, "pipeline - oit accum", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
		ri.Printf( PRINT_ALL, "[VK] OIT accum pipeline created (weighted blended OIT)\n" );
	}
}

void vk_create_blur_pipeline( uint32_t index, uint32_t width, uint32_t height, qboolean horizontal_pass )
{
	VkPipelineShaderStageCreateInfo shader_stages[2];
	VkPipelineVertexInputStateCreateInfo vertex_input_state;
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
	VkPipelineRasterizationStateCreateInfo rasterization_state;
	VkPipelineViewportStateCreateInfo viewport_state;
	VkPipelineMultisampleStateCreateInfo multisample_state;
	VkPipelineColorBlendStateCreateInfo blend_state;
	VkPipelineColorBlendAttachmentState attachment_blend_state;
	VkGraphicsPipelineCreateInfo create_info;
	VkViewport viewport;
	VkRect2D scissor;
	float frag_spec_data[4]; // inner offset (x,y), outer offset (x,y)
	VkSpecializationMapEntry spec_entries[4];
	VkSpecializationInfo frag_spec_info;
	VkPipeline *pipeline;

	pipeline = &vk.blur_pipeline[ index ];

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

	// shaders
	set_shader_stage_desc( shader_stages+0, VK_SHADER_STAGE_VERTEX_BIT, vk.modules.gamma_vs, "main" );
	set_shader_stage_desc( shader_stages+1, VK_SHADER_STAGE_FRAGMENT_BIT, vk.modules.blur_fs, "main" );

	// 9-tap Gaussian via 5 bilinear taps: inner pair at +/-1.333, outer pair at +/-3.111
	// Horizontal passes downsample to half resolution, so offsets must be based on
	// source texel size (2x destination) to avoid over-spaced sampling artifacts.
	if ( horizontal_pass ) {
		const float src_width = (float)width * 2.0f;
		frag_spec_data[0] = 1.33333f / src_width;     // inner offset x (source texel size)
		frag_spec_data[1] = 0.0f;                    // inner offset y
		frag_spec_data[2] = 3.11111f / src_width;     // outer offset x (source texel size)
		frag_spec_data[3] = 0.0f;                    // outer offset y
	} else {
		frag_spec_data[0] = 0.0f;                     // inner offset x
		frag_spec_data[1] = 1.33333f / (float)height; // inner offset y
		frag_spec_data[2] = 0.0f;                     // outer offset x
		frag_spec_data[3] = 3.11111f / (float)height; // outer offset y
	}

	spec_entries[0].constantID = 0;
	spec_entries[0].offset = 0 * sizeof( float );
	spec_entries[0].size = sizeof( float );

	spec_entries[1].constantID = 1;
	spec_entries[1].offset = 1 * sizeof( float );
	spec_entries[1].size = sizeof( float );

	spec_entries[2].constantID = 2;
	spec_entries[2].offset = 2 * sizeof( float );
	spec_entries[2].size = sizeof( float );

	spec_entries[3].constantID = 3;
	spec_entries[3].offset = 3 * sizeof( float );
	spec_entries[3].size = sizeof( float );

	frag_spec_info.mapEntryCount = 4;
	frag_spec_info.pMapEntries = spec_entries;
	frag_spec_info.dataSize = 4 * sizeof( float );
	frag_spec_info.pData = &frag_spec_data[0];

	shader_stages[1].pSpecializationInfo = &frag_spec_info;

	//
	// Primitive assembly.
	//
	input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_state.pNext = NULL;
	input_assembly_state.flags = 0;
	input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	input_assembly_state.primitiveRestartEnable = VK_FALSE;

	//
	// Viewport.
	//
	viewport.x = 0.0;
	viewport.y = 0.0;
	viewport.width = width;
	viewport.height = height;
	viewport.minDepth = 0.0;
	viewport.maxDepth = 1.0;

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

	//
	// Rasterization.
	//
	rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization_state.pNext = NULL;
	rasterization_state.flags = 0;
	rasterization_state.depthClampEnable = VK_FALSE;
	rasterization_state.rasterizerDiscardEnable = VK_FALSE;
	rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
	//rasterization_state.cullMode = VK_CULL_MODE_BACK_BIT; // VK_CULL_MODE_NONE;
	rasterization_state.cullMode = VK_CULL_MODE_NONE;
	rasterization_state.frontFace = VK_FRONT_FACE_CLOCKWISE; // Q3 defaults to clockwise vertex order
	rasterization_state.depthBiasEnable = VK_FALSE;
	rasterization_state.depthBiasConstantFactor = 0.0f;
	rasterization_state.depthBiasClamp = 0.0f;
	rasterization_state.depthBiasSlopeFactor = 0.0f;
	rasterization_state.lineWidth = 1.0f;

	multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_state.pNext = NULL;
	multisample_state.flags = 0;
	multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisample_state.sampleShadingEnable = VK_FALSE;
	multisample_state.minSampleShading = 1.0f;
	multisample_state.pSampleMask = NULL;
	multisample_state.alphaToCoverageEnable = VK_FALSE;
	multisample_state.alphaToOneEnable = VK_FALSE;

	Com_Memset(&attachment_blend_state, 0, sizeof(attachment_blend_state));
	attachment_blend_state.blendEnable = VK_FALSE;
	attachment_blend_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state.pNext = NULL;
	blend_state.flags = 0;
	blend_state.logicOpEnable = VK_FALSE;
	blend_state.logicOp = VK_LOGIC_OP_COPY;
	blend_state.attachmentCount = 1;
	blend_state.pAttachments = &attachment_blend_state;
	blend_state.blendConstants[0] = 0.0f;
	blend_state.blendConstants[1] = 0.0f;
	blend_state.blendConstants[2] = 0.0f;
	blend_state.blendConstants[3] = 0.0f;

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
	create_info.pDepthStencilState = NULL;
	create_info.pColorBlendState = &blend_state;
	create_info.pDynamicState = NULL;
	create_info.layout = vk.pipeline_layout_post_process; // one input attachment
	create_info.renderPass = vk.render_pass.blur[ index ];
	create_info.subpass = 0;
	create_info.basePipelineHandle = VK_NULL_HANDLE;
	create_info.basePipelineIndex = -1;

	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &create_info, NULL, pipeline ) );

	SET_OBJECT_NAME( *pipeline, va( "%s blur pipeline %i", horizontal_pass ? "horizontal" : "vertical", index/2 + 1 ), VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
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


VkPipeline create_pipeline( const Vk_Pipeline_Def *def, renderPass_t renderPassIndex, uint32_t def_index ) {
	(void)def_index; // unused parameter
	VkShaderModule *vs_module = NULL;
	VkShaderModule *fs_module = NULL;
	//int32_t vert_spec_data[1]; // clippping
	//VkSpecializationInfo vert_spec_info;
    struct Vk_Pipeline_FragSpecData frag_spec_data;

#ifdef USE_VK_PBR
    VkSpecializationMapEntry spec_entries[37];
#else
    VkSpecializationMapEntry spec_entries[12];
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
				vs_module = &vk.modules.vert.gen[use_pbr][0][0][0][0];
				fs_module = vk_hdr64_active() ? &vk.modules.frag.gen_hdr64[use_pbr][0][0][0] : &vk.modules.frag.gen[use_pbr][0][0][0];
			}
			break;

		case TYPE_SIGNLE_TEXTURE_ENV:
			vs_module = &vk.modules.vert.gen[use_pbr][0][0][1][0];
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
			vs_module = &vk.modules.vert.gen[use_pbr][1][0][0][0];
			fs_module = vk_hdr64_active() ? &vk.modules.frag.gen_hdr64[use_pbr][1][0][0] : &vk.modules.frag.gen[use_pbr][1][0][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL2_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_ENV:
			vs_module = &vk.modules.vert.gen[use_pbr][1][0][1][0];
			fs_module = vk_hdr64_active() ? &vk.modules.frag.gen_hdr64[use_pbr][1][0][0] : &vk.modules.frag.gen[use_pbr][1][0][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL3:
		case TYPE_MULTI_TEXTURE_ADD3_1_1:
		case TYPE_MULTI_TEXTURE_ADD3:
			vs_module = &vk.modules.vert.gen[use_pbr][2][0][0][0];
			fs_module = vk_hdr64_active() ? &vk.modules.frag.gen_hdr64[use_pbr][2][0][0] : &vk.modules.frag.gen[use_pbr][2][0][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL3_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_ENV:
			vs_module = &vk.modules.vert.gen[use_pbr][2][0][1][0];
			fs_module = vk_hdr64_active() ? &vk.modules.frag.gen_hdr64[use_pbr][2][0][0] : &vk.modules.frag.gen[use_pbr][2][0][0];
			break;

		case TYPE_BLEND2_ADD:
		case TYPE_BLEND2_MUL:
		case TYPE_BLEND2_ALPHA:
		case TYPE_BLEND2_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_MIX_ALPHA:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA:
			vs_module = &vk.modules.vert.gen[use_pbr][1][1][0][0];
			fs_module = vk_hdr64_active() ? &vk.modules.frag.gen_hdr64[use_pbr][1][1][0] : &vk.modules.frag.gen[use_pbr][1][1][0];
			break;

		case TYPE_BLEND2_ADD_ENV:
		case TYPE_BLEND2_MUL_ENV:
		case TYPE_BLEND2_ALPHA_ENV:
		case TYPE_BLEND2_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND2_MIX_ALPHA_ENV:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV:
			vs_module = &vk.modules.vert.gen[use_pbr][1][1][1][0];
			fs_module = vk_hdr64_active() ? &vk.modules.frag.gen_hdr64[use_pbr][1][1][0] : &vk.modules.frag.gen[use_pbr][1][1][0];
			break;

		case TYPE_BLEND3_ADD:
		case TYPE_BLEND3_MUL:
		case TYPE_BLEND3_ALPHA:
		case TYPE_BLEND3_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_MIX_ALPHA:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA:
			vs_module = &vk.modules.vert.gen[use_pbr][2][1][0][0];
			fs_module = vk_hdr64_active() ? &vk.modules.frag.gen_hdr64[use_pbr][2][1][0] : &vk.modules.frag.gen[use_pbr][2][1][0];
			break;

		case TYPE_BLEND3_ADD_ENV:
		case TYPE_BLEND3_MUL_ENV:
		case TYPE_BLEND3_ALPHA_ENV:
		case TYPE_BLEND3_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV:
			vs_module = &vk.modules.vert.gen[use_pbr][2][1][1][0];
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

	set_shader_stage_desc(shader_stages+0, VK_SHADER_STAGE_VERTEX_BIT, *vs_module, "main");
	set_shader_stage_desc(shader_stages+1, VK_SHADER_STAGE_FRAGMENT_BIT, *fs_module, "main");

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
	if ( r_ext_alpha_to_coverage->integer && vkSamples != VK_SAMPLE_COUNT_1_BIT && frag_spec_data.alpha_test_func ) {
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
	/* constant_id order must match gen_frag.tmpl (19..35) */
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
#ifdef HDR_DELUXE_LIGHTMAP
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
#endif // HDR_DELUXE_LIGHTMAP
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
		multisample_state.rasterizationSamples = (VkSampleCountFlagBits)vkSamples;
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


static VkPipeline vk_gen_pipeline( uint32_t index ) {
	if ( index < vk.pipelines_count ) {
		VK_Pipeline_t *pipeline = vk.pipelines + index;
		const renderPass_t pass = vk.renderPassIndex;
		if ( pipeline->handle[ pass ] == VK_NULL_HANDLE ) {
			pipeline->handle[ pass ] = create_pipeline( &pipeline->def, pass, index );
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


/* Unit cube for occlusion bbox: 8 vertices, 36 indices (12 triangles) */
static const float s_occlusion_cube_verts[8][3] = {
	{ 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 }, { 1, 1, 0 },
	{ 0, 0, 1 }, { 1, 0, 1 }, { 0, 1, 1 }, { 1, 1, 1 }
};
static const uint16_t s_occlusion_cube_indices[36] = {
	0,2,1, 1,2,3, 4,5,6, 5,7,6, 0,1,4, 1,5,4,
	2,6,3, 3,6,7, 0,4,2, 2,4,6, 1,3,5, 3,7,5
};

void vk_occlusion_draw_entity_bboxes( const struct drawSurfsCommand_s *cmd )
{
	const trRefEntity_t *ent;
	vec3_t mins, maxs;
	float model[16], mvp[16];
	int i, n;
	const drawSurfsCommand_t *c = (const drawSurfsCommand_t *)cmd;

	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE )
		return;
	if ( vk.occlusion_query_pool == VK_NULL_HANDLE || vk.occlusion_bbox_pipeline == 0 ||
		!qvkCmdBeginQuery || !qvkCmdEndQuery || !qvkCmdResetQueryPool )
		return;

	n = c->refdef.num_entities;
	vk_occlusion_last_entity_count = (uint32_t)n;
	if ( n <= 0 || n > MAX_REFENTITIES )
		return;

	qvkCmdResetQueryPool( vk.cmd->command_buffer, vk.occlusion_query_pool, 0, (uint32_t)n );
	vk_bind_pipeline( vk.occlusion_bbox_pipeline );

	for ( i = 0; i < n; i++ ) {
		ent = &c->refdef.entities[i];
		if ( ent->e.reType != RT_MODEL || !ent->e.hModel )
			continue;

		R_ModelBounds( ent->e.hModel, mins, maxs );
		R_RotateForEntity( ent, &c->viewParms, &backEnd.or );

		/* Build model matrix: origin + axis * (mins + v * (maxs-mins)) for v in [0,1]^3 */
		{
			float *m = model;
			float sx = maxs[0] - mins[0], sy = maxs[1] - mins[1], sz = maxs[2] - mins[2];
			const float *ax0 = backEnd.or.axis[0], *ax1 = backEnd.or.axis[1], *ax2 = backEnd.or.axis[2];
			const float *o = backEnd.or.origin;

			m[0]  = ax0[0]*sx; m[1]  = ax0[1]*sx; m[2]  = ax0[2]*sx; m[3]  = 0;
			m[4]  = ax1[0]*sy; m[5]  = ax1[1]*sy; m[6]  = ax1[2]*sy; m[7]  = 0;
			m[8]  = ax2[0]*sz; m[9]  = ax2[1]*sz; m[10] = ax2[2]*sz; m[11] = 0;
			m[12] = o[0] + ax0[0]*mins[0] + ax1[0]*mins[1] + ax2[0]*mins[2];
			m[13] = o[1] + ax0[1]*mins[0] + ax1[1]*mins[1] + ax2[1]*mins[2];
			m[14] = o[2] + ax0[2]*mins[0] + ax1[2]*mins[1] + ax2[2]*mins[2];
			m[15] = 1;
		}
		myGlMultMatrix( c->viewParms.world.modelViewMatrix, model, mvp );
		myGlMultMatrix( c->viewParms.projectionMatrix, mvp, mvp );

		qvkCmdBeginQuery( vk.cmd->command_buffer, vk.occlusion_query_pool, (uint32_t)i, 0 );
		vk_update_mvp( mvp );
		RB_CheckOverflow( 8, 36 );
		for ( n = 0; n < 8; n++ ) {
			tess.xyz[n][0] = s_occlusion_cube_verts[n][0];
			tess.xyz[n][1] = s_occlusion_cube_verts[n][1];
			tess.xyz[n][2] = s_occlusion_cube_verts[n][2];
			tess.xyz[n][3] = 1.0f;
		}
		tess.numVertexes = 8;
		{
			glIndex_t idx_buf[36];
			uint32_t idx_off;
			for ( n = 0; n < 36; n++ )
				idx_buf[n] = s_occlusion_cube_indices[n];
			vk_bind_geometry( TESS_XYZ );
			idx_off = vk_tess_index( 36, idx_buf );
			if ( idx_off != ~0U ) {
				vk_bind_index_buffer( vk.cmd->vertex_buffer, idx_off );
				vk.cmd->num_indexes = 36;
				vk_draw_geometry( DEPTH_RANGE_NORMAL, qtrue );
			}
		}
		qvkCmdEndQuery( vk.cmd->command_buffer, vk.occlusion_query_pool, (uint32_t)i );
	}
}

void vk_occlusion_readback( void )
{
	VkResult res;
	uint32_t n;

	if ( vk.occlusion_query_pool == VK_NULL_HANDLE || !qvkGetQueryPoolResults )
		return;

	n = vk_occlusion_last_entity_count;
	if ( n <= 0 || n > MAX_REFENTITIES )
		return;

	/* VUID-09401: queries must be reset before use. vk_occlusion_draw_entity_bboxes resets
	 * before begin/end. If we never drew this frame, skip readback (queries may be uninitialized). */
	res = qvkGetQueryPoolResults( vk.device, vk.occlusion_query_pool, 0, n,
		sizeof( vk_entity_occlusion_visibility ), vk_entity_occlusion_visibility,
		sizeof( uint64_t ), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT );
	if ( res != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, "Occlusion readback failed: %s\n", vk_result_string( res ) );
		Com_Memset( vk_entity_occlusion_visibility, 0, sizeof( vk_entity_occlusion_visibility ) );
		return;
	}
}

void vk_occlusion_pass( const struct drawSurfsCommand_s *cmd )
{
	/* Called from RB_DrawSurfs. World depth + entity bboxes are done there. */
	(void)cmd;
}

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


#ifdef USE_VK_PBR
static VkBuffer shade_bufs[10];
#else
static VkBuffer shade_bufs[8];
#endif
static int bind_base;
static int bind_count;

static void vk_bind_index_attr( int index )
{
	if ( bind_base == -1 ) {
		bind_base = index;
		bind_count = 1;
	} else {
		bind_count = index - bind_base + 1;
	}
}


static void vk_bind_attr( int index, unsigned int item_size, const void *src ) {
	const uint32_t offset = PAD( vk.cmd->vertex_buffer_offset, 32 );
	const uint32_t size = tess.numVertexes * item_size;

	if ( offset + size > vk.geometry_buffer_size ) {
		// schedule geometry buffer resize
		vk.geometry_buffer_size_new = log2pad( offset + size, 1 );
	} else {
		vk.cmd->buf_offset[ index ] = offset;
		Com_Memcpy( vk.cmd->vertex_buffer_ptr + offset, src, size );
		vk.cmd->vertex_buffer_offset = (VkDeviceSize)offset + size;
	}

	vk_bind_index_attr( index );
}

void *vk_alloc_storage( size_t size, uint32_t *offset )
{
	const uint32_t aligned = PAD( vk.cmd->vertex_buffer_offset, (VkDeviceSize)vk.storage_alignment );
	const uint32_t size32 = (uint32_t)size;

	if ( aligned + size32 > vk.geometry_buffer_size ) {
		// schedule geometry buffer resize
		vk.geometry_buffer_size_new = log2pad( aligned + size32, 1 );
		return NULL;
	}

	if ( offset ) {
		*offset = aligned;
	}

	vk.cmd->vertex_buffer_offset = (VkDeviceSize)aligned + size32;
	return vk.cmd->vertex_buffer_ptr + aligned;
}

void vk_set_iqm_storage_offsets( uint32_t skin_offset, uint32_t morph_offset )
{
	if ( !vk.cmd ) {
		return;
	}

	vk.cmd->iqm_skin_offset = skin_offset;
	vk.cmd->iqm_morph_offset = morph_offset;
}

void vk_reset_iqm_storage_offsets( void )
{
	vk_set_iqm_storage_offsets( 0, 0 );
}


uint32_t vk_tess_index( uint32_t numIndexes, const void *src ) {
	const uint32_t offset = vk.cmd->vertex_buffer_offset;
	const uint32_t size = numIndexes * sizeof( tess.indexes[0] );

	if ( offset + size > vk.geometry_buffer_size ) {
		// schedule geometry buffer resize
		vk.geometry_buffer_size_new = log2pad( offset + size, 1 );
		return ~0U;
	} else {
		Com_Memcpy( vk.cmd->vertex_buffer_ptr + offset, src, size );
		vk.cmd->vertex_buffer_offset = (VkDeviceSize)offset + size;
		return offset;
	}
}


void vk_bind_index_buffer( VkBuffer buffer, uint32_t offset )
{
	if ( vk.cmd->curr_index_buffer != buffer || vk.cmd->curr_index_offset != offset )
		qvkCmdBindIndexBuffer( vk.cmd->command_buffer, buffer, offset, VK_INDEX_TYPE_UINT32 );

	vk.cmd->curr_index_buffer = buffer;
	vk.cmd->curr_index_offset = offset;
}


#ifdef USE_VBO
void vk_draw_indexed( uint32_t indexCount, uint32_t firstIndex )
{
	qvkCmdDrawIndexed( vk.cmd->command_buffer, indexCount, 1, firstIndex, 0, 0 );
}
#endif


void vk_bind_index( void )
{
#ifdef USE_VBO
	if ( tess.vboIndex ) {
		vk.cmd->num_indexes = 0;
		//qvkCmdBindIndexBuffer( vk.cmd->command_buffer, vk.vbo.index_buffer, tess.shader->iboOffset, VK_INDEX_TYPE_UINT32 );
		return;
	}
#endif

	vk_bind_index_ext( tess.numIndexes, tess.indexes );
}


void vk_bind_index_ext( const int numIndexes, const uint32_t *indexes )
{
	uint32_t offset	= vk_tess_index( numIndexes, indexes );
	if ( offset != ~0U ) {
		vk_bind_index_buffer( vk.cmd->vertex_buffer, offset );
		vk.cmd->num_indexes = numIndexes;
	} else {
		// overflowed
		vk.cmd->num_indexes = 0;
	}
}


void vk_bind_geometry( uint32_t flags )
{
	//unsigned int size;
	bind_base = -1;
	bind_count = 0;

	if ( ( flags & ( TESS_XYZ | TESS_RGBA0 | TESS_ST0 | TESS_ST1 | TESS_ST2 | TESS_NNN | TESS_RGBA1 | TESS_RGBA2 ) ) == 0 )
		return;

	/* glTF VBO path: bind per-primitive buffers instead of tess */
	if ( tess.gltfDrawSurface ) {
		const struct srfGLTFPrimitive_s *surf = tess.gltfDrawSurface;
		tess.gltfDrawSurface = NULL;
		if ( surf->vbo_vertex != VK_NULL_HANDLE && surf->vbo_index != VK_NULL_HANDLE &&
		     surf->numVertices > 0 && surf->numIndices > 0 ) {
			VkBuffer bufs[6];
			VkDeviceSize offs[6];
			bufs[0] = bufs[1] = bufs[2] = bufs[3] = bufs[4] = bufs[5] = surf->vbo_vertex;
			offs[0] = surf->vbo_vertex_offsets[0];
			offs[1] = surf->vbo_vertex_offsets[1];
			offs[2] = surf->vbo_vertex_offsets[2];
			offs[3] = offs[4] = 0; /* unused bindings */
			offs[5] = surf->vbo_vertex_offsets[5];
			qvkCmdBindVertexBuffers( vk.cmd->command_buffer, 0, 6, bufs, offs );
			vk_bind_index_buffer( surf->vbo_index, 0 );
			vk.cmd->num_indexes = surf->numIndices;
		} else {
			vk.cmd->num_indexes = 0;
		}
		return;
	}

#ifdef USE_VBO
	if ( tess.vboIndex ) {

		shade_bufs[0] = shade_bufs[1] = shade_bufs[2] = shade_bufs[3] = shade_bufs[4] = shade_bufs[5] = shade_bufs[6] = shade_bufs[7] = vk.vbo.vertex_buffer;
#ifdef USE_VK_PBR
		shade_bufs[8] = vk.vbo.vertex_buffer;
		shade_bufs[9] = vk.vbo.vertex_buffer;
#endif

		if ( flags & TESS_XYZ ) {  // 0
			vk.cmd->vbo_offset[0] = tess.shader->vboOffset + 0;
			vk_bind_index_attr( 0 );
		}

		if ( flags & TESS_RGBA0 ) { // 1
			vk.cmd->vbo_offset[1] = tess.shader->stages[ tess.vboStage ]->rgb_offset[0];
			vk_bind_index_attr( 1 );
		}

		if ( flags & TESS_ST0 ) {  // 2
			vk.cmd->vbo_offset[2] = tess.shader->stages[ tess.vboStage ]->tex_offset[0];
			vk_bind_index_attr( 2 );
		}

		if ( flags & TESS_ST1 ) {  // 3
			vk.cmd->vbo_offset[3] = tess.shader->stages[ tess.vboStage ]->tex_offset[1];
			vk_bind_index_attr( 3 );
		}

		if ( flags & TESS_ST2 ) {  // 4
			vk.cmd->vbo_offset[4] = tess.shader->stages[ tess.vboStage ]->tex_offset[2];
			vk_bind_index_attr( 4 );
		}

		if ( flags & TESS_NNN ) { // 5
			vk.cmd->vbo_offset[5] = tess.shader->normalOffset;
			vk_bind_index_attr( 5 );
		}

		if ( flags & TESS_RGBA1 ) { // 6
			vk.cmd->vbo_offset[6] = tess.shader->stages[ tess.vboStage ]->rgb_offset[1];
			vk_bind_index_attr( 6 );
		}

		if ( flags & TESS_RGBA2 ) { // 7
			vk.cmd->vbo_offset[7] = tess.shader->stages[ tess.vboStage ]->rgb_offset[2];
			vk_bind_index_attr( 7 );
		}
#ifdef USE_VK_PBR
		if (flags & TESS_PBR) {
			vk.cmd->vbo_offset[5] = tess.shader->normalOffset;
			vk_bind_index_attr( 5 );

			vk.cmd->vbo_offset[8] = tess.shader->qtangentOffset;
			vk_bind_index_attr(8);

			vk.cmd->vbo_offset[9] = tess.shader->lightdirOffset;
			vk_bind_index_attr(9);
		}
#endif

		qvkCmdBindVertexBuffers( vk.cmd->command_buffer, bind_base, bind_count, shade_bufs, vk.cmd->vbo_offset + bind_base );

	} else
#endif // USE_VBO
	{
		shade_bufs[0] = shade_bufs[1] = shade_bufs[2] = shade_bufs[3] = shade_bufs[4] = shade_bufs[5] = shade_bufs[6] = shade_bufs[7] = vk.cmd->vertex_buffer;
#ifdef USE_VK_PBR
		shade_bufs[8] = vk.cmd->vertex_buffer;
		shade_bufs[9] = vk.cmd->vertex_buffer;
#endif

		if ( flags & TESS_XYZ ) {
			vk_bind_attr(0, sizeof(tess.xyz[0]), &tess.xyz[0]);
		}

		if ( flags & TESS_RGBA0 ) {
			vk_bind_attr(1, sizeof( color4ub_t ), tess.svars.colors[0][0].rgba);
		}

		if ( flags & TESS_ST0 ) {
			vk_bind_attr(2, sizeof( vec2_t ), tess.svars.texcoordPtr[0]);
		}

		if ( flags & TESS_ST1 ) {
			vk_bind_attr(3, sizeof( vec2_t ), tess.svars.texcoordPtr[1]);
		}

		if ( flags & TESS_ST2 ) {
			vk_bind_attr(4, sizeof( vec2_t ), tess.svars.texcoordPtr[2]);
		}

		if ( flags & TESS_NNN ) {
			vk_bind_attr(5, sizeof(tess.normal[0]), tess.normal);
		}

		if ( flags & TESS_RGBA1 ) {
			vk_bind_attr(6, sizeof( color4ub_t ), tess.svars.colors[1][0].rgba);
		}

		if ( flags & TESS_RGBA2 ) {
			vk_bind_attr(7, sizeof( color4ub_t ), tess.svars.colors[2][0].rgba);
		}
#ifdef USE_VK_PBR
		if (flags & TESS_PBR) {
			vk_bind_attr(5, sizeof(tess.normal[0]), tess.normal);
			vk_bind_attr(8, sizeof(tess.qtangent[0]), tess.qtangent);
			vk_bind_attr(9, sizeof(tess.lightdir[0]), tess.lightdir);
		}
#endif

		qvkCmdBindVertexBuffers( vk.cmd->command_buffer, bind_base, bind_count, shade_bufs, vk.cmd->buf_offset + bind_base );
	}
}


void vk_bind_lighting( int stage, int bundle )
{
	bind_base = -1;
	bind_count = 0;

#ifdef USE_VBO
	if ( tess.vboIndex ) {

		shade_bufs[0] = shade_bufs[1] = shade_bufs[2] = vk.vbo.vertex_buffer;

		vk.cmd->vbo_offset[0] = tess.shader->vboOffset + 0;
		vk.cmd->vbo_offset[1] = tess.shader->stages[ stage ]->tex_offset[ bundle ];
		vk.cmd->vbo_offset[2] = tess.shader->normalOffset;

		qvkCmdBindVertexBuffers( vk.cmd->command_buffer, 0, 3, shade_bufs, vk.cmd->vbo_offset + 0 );

	}
	else
#endif // USE_VBO
	{
		shade_bufs[0] = shade_bufs[1] = shade_bufs[2] = vk.cmd->vertex_buffer;

		vk_bind_attr( 0, sizeof( tess.xyz[0] ), &tess.xyz[0] );
		vk_bind_attr( 1, sizeof( vec2_t ), tess.svars.texcoordPtr[ bundle ] );
		vk_bind_attr( 2, sizeof( tess.normal[0] ), tess.normal );

		qvkCmdBindVertexBuffers( vk.cmd->command_buffer, bind_base, bind_count, shade_bufs, vk.cmd->buf_offset + bind_base );
	}
}


void vk_reset_descriptor( int index )
{
	vk.cmd->descriptor_set.current[ index ] = VK_NULL_HANDLE;
	vk.cmd->descriptor_set.image[ index ] = NULL;
}


void vk_update_descriptor( int index, VkDescriptorSet descriptor )
{
	if ( vk.cmd->descriptor_set.current[ index ] != descriptor ) {
		vk.cmd->descriptor_set.start = ( (uint32_t)index < vk.cmd->descriptor_set.start ) ? (uint32_t)index : vk.cmd->descriptor_set.start;
		vk.cmd->descriptor_set.end = ( (uint32_t)index > vk.cmd->descriptor_set.end ) ? (uint32_t)index : vk.cmd->descriptor_set.end;
	}
	vk.cmd->descriptor_set.current[ index ] = descriptor;
}

void vk_update_descriptor_offset( int index, uint32_t offset )
{
	vk.cmd->descriptor_set.offset[ index ] = offset;
}


void vk_bind_descriptor_sets( void )
{
	uint32_t offsets[VK_DESC_UNIFORM_COUNT], offset_count;
	uint32_t start, end, count, i;
	uint32_t bound_set_count = MIN( (uint32_t)VK_DESC_COUNT, vk.maxBoundDescriptorSets );

	start = vk.cmd->descriptor_set.start;
	if ( start == ~0U && !backEnd.oitAccumPass ) {
		if ( vk.cmd->last_pipeline == VK_NULL_HANDLE ) {
			if ( vk.cmd->descriptor_set.current[VK_DESC_UNIFORM] == VK_NULL_HANDLE &&
				vk.cmd->uniform_descriptor != VK_NULL_HANDLE )
			{
				vk.cmd->descriptor_set.current[VK_DESC_UNIFORM] = vk.cmd->uniform_descriptor;
			}

			if ( bound_set_count == 0 || vk.cmd->descriptor_set.current[VK_DESC_UNIFORM] == VK_NULL_HANDLE ) {
				return;
			}

			start = 0;
			end = bound_set_count - 1;
		} else {
			return;
		}
	}

	end = backEnd.oitAccumPass ? 0 : vk.cmd->descriptor_set.end;
	if ( backEnd.oitAccumPass )
		start = 0;

	offset_count = 0;
	if ( start == VK_DESC_UNIFORM ) {
		for ( i = 0; i < VK_DESC_UNIFORM_COUNT; i++ ) {
			offsets[offset_count++] = vk.cmd->descriptor_set.offset[i];
		}
	}

	count = end - start + 1;

	// fill NULL descriptor gaps
	if ( tr.whiteImage ) {
		for ( i = start + 1; i <= end; i++ ) {
			if ( vk.cmd->descriptor_set.current[i] == VK_NULL_HANDLE ) {
				vk.cmd->descriptor_set.current[i] = tr.whiteImage->descriptor;
				vk.cmd->descriptor_set.image[i] = tr.whiteImage;
			}
		}
	}

#ifdef USE_VK_PBR
	if ( r_vk_pipeline_debug && r_vk_pipeline_debug->integer && vk.cmd ) {
		struct {
			int index;
			const char *name;
		} pbr_descs[] = {
			{ VK_DESC_PBR_NORMAL, "normal" },
			{ VK_DESC_PBR_PHYSICAL, "physical" },
			{ VK_DESC_PBR_CUBEMAP, "env" },
			{ VK_DESC_PBR_IRRADIANCE, "irradiance" }
		};

		ri.Printf( PRINT_DEVELOPER, "vk bind descriptors PBR\n" );
		for ( int desc_index = 0; desc_index < (int)ARRAY_LEN(pbr_descs); desc_index++ ) {
			int index = pbr_descs[desc_index].index;
			const char *name = pbr_descs[desc_index].name;
			const image_t *img = vk.cmd->descriptor_set.image[index];
			const char *source = img ? img->imgName : "none";
			const char *tag = "missing";
			if ( img == tr.whiteImage ) {
				tag = "fallback";
			} else if ( img != NULL ) {
				tag = "source";
			}
			ri.Printf( PRINT_DEVELOPER, "  %s desc=%p view=%p sampler=%p %s(%s)\n",
				name,
				(void*)vk.cmd->descriptor_set.current[index],
				(void*)(img ? img->view : VK_NULL_HANDLE),
				(void*)(img ? img->vk_sampler : VK_NULL_HANDLE),
				tag,
				source );
		}
	}
#endif

	if ( backEnd.oitAccumPass && vk.pipeline_layout_oit_accum != VK_NULL_HANDLE ) {
		/* OIT accum set 0 = tex0; set 1 = opaque depth for MSAA/manual rejection. */
		if ( vk.cmd->descriptor_set.current[VK_DESC_TEXTURE0] != VK_NULL_HANDLE ) {
			VkDescriptorSet sets[2] = {
				vk.cmd->descriptor_set.current[VK_DESC_TEXTURE0],
				vk.oit_depth_descriptor
			};
			uint32_t set_count = ( vk.oit_depth_descriptor != VK_NULL_HANDLE ) ? 2u : 1u;
			qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
				vk.pipeline_layout_oit_accum, 0, set_count, sets, 0, NULL );
		}
	} else {
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout, start, count, vk.cmd->descriptor_set.current + start, offset_count, offsets );
	}

	vk.cmd->descriptor_set.end = 0;
	vk.cmd->descriptor_set.start = ~0U;
}

static void vk_force_descriptor_rebind( void )
{
	uint32_t bound_set_count = MIN( (uint32_t)VK_DESC_COUNT, vk.maxBoundDescriptorSets );

	if ( !vk.cmd || backEnd.oitAccumPass ) {
		return;
	}

	if ( vk.cmd->descriptor_set.current[VK_DESC_UNIFORM] == VK_NULL_HANDLE &&
		vk.cmd->uniform_descriptor != VK_NULL_HANDLE ) {
		vk.cmd->descriptor_set.current[VK_DESC_UNIFORM] = vk.cmd->uniform_descriptor;
	}

	if ( bound_set_count == 0 || vk.cmd->descriptor_set.current[VK_DESC_UNIFORM] == VK_NULL_HANDLE ) {
		return;
	}

	vk.cmd->descriptor_set.start = 0;
	vk.cmd->descriptor_set.end = bound_set_count - 1;
}


void vk_bind_pipeline( uint32_t pipeline ) {
	VkPipeline vkpipe;

	if ( backEnd.oitAccumPass && vk.oit_accum_pipeline != VK_NULL_HANDLE ) {
		vkpipe = vk.oit_accum_pipeline;
	} else {
		vkpipe = vk_gen_pipeline( pipeline );
	}

	if ( vkpipe != vk.cmd->last_pipeline ) {
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkpipe );
		vk.cmd->last_pipeline = vkpipe;
		vk_force_descriptor_rebind();
	}

	if ( !backEnd.oitAccumPass ) {
		vk_world.dirty_depth_attachment |= ( vk.pipelines[ pipeline ].def.state_bits & GLS_DEPTHMASK_TRUE );
	}
}

void vk_draw_geometry( Vk_Depth_Range depth_range, qboolean indexed ) {

	if ( vk.geometry_buffer_size_new ) {
		// geometry buffer overflow happened this frame
		return;
	}
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE || !vk.inRenderPass )
		return;

	vk_bind_descriptor_sets();

	// configure pipeline's dynamic state
	vk_update_depth_range( depth_range );

	// issue draw call(s)
#ifdef USE_VBO
	if ( tess.vboIndex )
		VBO_RenderIBOItems();
	else
#endif
	if ( indexed ) {
		qvkCmdDrawIndexed( vk.cmd->command_buffer, vk.cmd->num_indexes, 1, 0, 0, 0 );
	} else {
		qvkCmdDraw( vk.cmd->command_buffer, tess.numVertexes, 1, 0, 0 );
	}
}


void vk_draw_dot( uint32_t storage_offset )
{
	if ( vk.geometry_buffer_size_new ) {
		// geometry buffer overflow happened this frame
		return;
	}
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE || !vk.inRenderPass )
		return;

	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_storage, VK_DESC_STORAGE, 1, &vk.storage.descriptor, 1, &storage_offset );

	// configure pipeline's dynamic state
	vk_update_depth_range( DEPTH_RANGE_NORMAL );

	qvkCmdDraw( vk.cmd->command_buffer, tess.numVertexes, 1, 0, 0 );
}


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

void vk_reset_occlusion_visibility( void )
{
	/* Stale visibility after camera cut / world change; treat all visible until next query */
	Com_Memset( vk_entity_occlusion_visibility, 0xFF, sizeof( vk_entity_occlusion_visibility ) );
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

static void vk_destroy_volumetric_params_buffer( void )
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

static void vk_create_volumetric_params_buffer( void )
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

static void vk_destroy_postfx_params_buffers( void )
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

static void vk_create_postfx_params_buffers( void )
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

	set_shader_stage_desc( shader_stages + 0, VK_SHADER_STAGE_VERTEX_BIT, *def->shaders.vs_module, "main" );
	set_shader_stage_desc( shader_stages + 1, VK_SHADER_STAGE_FRAGMENT_BIT, *def->shaders.fs_module, "main" );
	set_shader_stage_desc( shader_stages + 2, VK_SHADER_STAGE_GEOMETRY_BIT, *def->shaders.gm_module, "main" );

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
