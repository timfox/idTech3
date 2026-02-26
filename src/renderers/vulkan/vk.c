#include "tr_local.h"
#include "vk.h"
#include "vk_postfx.h"
#include <stddef.h>

#if defined (_DEBUG)
#if defined (_WIN32)
#define USE_VK_VALIDATION
#include <windows.h> // for win32 debug callback
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
	int32_t normal_texture_set;
	int32_t physical_texture_set;
	int32_t env_texture_set;
	int32_t lightmap_texture_set;
	int32_t deluxe_mapping;
	float deluxe_specular_scale;
	int32_t irradiance_texture_set;
	int32_t emissive_texture_set;
	int32_t clearcoat_texture_set;
	int32_t sheen_texture_set;
	int32_t anisotropy_texture_set;
	int32_t transmission_texture_set;
	int32_t subsurface_texture_set;
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
	float srcUVScaleBias[4]; // scale.xy, bias.xy
} VkPostProcessPushConstants;

static int vkSamples = VK_SAMPLE_COUNT_1_BIT;
static int vkMaxSamples = VK_SAMPLE_COUNT_1_BIT;
static VkRect2D vk_scene_src_rect;
static qboolean vk_scene_src_rect_valid;

static VkInstance vk_instance = VK_NULL_HANDLE;
static VkSurfaceKHR vk_surface = VK_NULL_HANDLE;

#ifndef NDEBUG
VkDebugReportCallbackEXT vk_debug_callback = VK_NULL_HANDLE;
#endif

//
// Vulkan API functions used by the renderer.
//
static PFN_vkCreateInstance								qvkCreateInstance;
static PFN_vkEnumerateInstanceExtensionProperties		qvkEnumerateInstanceExtensionProperties;

static PFN_vkCreateDevice								qvkCreateDevice;
static PFN_vkDestroyInstance							qvkDestroyInstance;
static PFN_vkEnumerateDeviceExtensionProperties			qvkEnumerateDeviceExtensionProperties;
static PFN_vkEnumeratePhysicalDevices					qvkEnumeratePhysicalDevices;
static PFN_vkGetDeviceProcAddr							qvkGetDeviceProcAddr;
static PFN_vkGetPhysicalDeviceFeatures					qvkGetPhysicalDeviceFeatures;
static PFN_vkGetPhysicalDeviceFormatProperties			qvkGetPhysicalDeviceFormatProperties;
static PFN_vkGetPhysicalDeviceMemoryProperties			qvkGetPhysicalDeviceMemoryProperties;
static PFN_vkGetPhysicalDeviceProperties				qvkGetPhysicalDeviceProperties;
static PFN_vkGetPhysicalDeviceQueueFamilyProperties		qvkGetPhysicalDeviceQueueFamilyProperties;
static PFN_vkDestroySurfaceKHR							qvkDestroySurfaceKHR;
static PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR	qvkGetPhysicalDeviceSurfaceCapabilitiesKHR;
static PFN_vkGetPhysicalDeviceSurfaceFormatsKHR			qvkGetPhysicalDeviceSurfaceFormatsKHR;
static PFN_vkGetPhysicalDeviceSurfacePresentModesKHR	qvkGetPhysicalDeviceSurfacePresentModesKHR;
static PFN_vkGetPhysicalDeviceSurfaceSupportKHR			qvkGetPhysicalDeviceSurfaceSupportKHR;
#ifdef USE_VK_VALIDATION
static PFN_vkCreateDebugReportCallbackEXT				qvkCreateDebugReportCallbackEXT;
static PFN_vkDestroyDebugReportCallbackEXT				qvkDestroyDebugReportCallbackEXT;
#endif
static PFN_vkAllocateCommandBuffers						qvkAllocateCommandBuffers;
static PFN_vkAllocateDescriptorSets						qvkAllocateDescriptorSets;
static PFN_vkAllocateMemory								qvkAllocateMemory;
static PFN_vkBeginCommandBuffer							qvkBeginCommandBuffer;
static PFN_vkBindBufferMemory							qvkBindBufferMemory;
static PFN_vkBindImageMemory							qvkBindImageMemory;
static PFN_vkCmdBeginRenderPass							qvkCmdBeginRenderPass;
static PFN_vkCmdBindDescriptorSets						qvkCmdBindDescriptorSets;
static PFN_vkCmdBindIndexBuffer							qvkCmdBindIndexBuffer;
static PFN_vkCmdBindPipeline							qvkCmdBindPipeline;
static PFN_vkCmdBindVertexBuffers						qvkCmdBindVertexBuffers;
static PFN_vkCmdBlitImage								qvkCmdBlitImage;
static PFN_vkCmdClearAttachments						qvkCmdClearAttachments;
static PFN_vkCmdCopyBuffer								qvkCmdCopyBuffer;
static PFN_vkCmdCopyBufferToImage						qvkCmdCopyBufferToImage;
static PFN_vkCmdCopyImage								qvkCmdCopyImage;
static PFN_vkCmdCopyImageToBuffer						qvkCmdCopyImageToBuffer;
static PFN_vkCmdDraw									qvkCmdDraw;
static PFN_vkCmdDrawIndexed								qvkCmdDrawIndexed;
static PFN_vkCmdDispatch								qvkCmdDispatch;
static PFN_vkCmdEndRenderPass							qvkCmdEndRenderPass;
static PFN_vkCmdNextSubpass								qvkCmdNextSubpass;
static PFN_vkCmdPipelineBarrier							qvkCmdPipelineBarrier;
static PFN_vkCmdPushConstants							qvkCmdPushConstants;
static PFN_vkCmdSetDepthBias							qvkCmdSetDepthBias;
static PFN_vkCmdSetScissor								qvkCmdSetScissor;
static PFN_vkCmdSetViewport								qvkCmdSetViewport;
static PFN_vkCmdWriteTimestamp							qvkCmdWriteTimestamp;
static PFN_vkCmdResetQueryPool							qvkCmdResetQueryPool;
static PFN_vkCreateBuffer								qvkCreateBuffer;
static PFN_vkCreateCommandPool							qvkCreateCommandPool;
static PFN_vkCreateDescriptorPool						qvkCreateDescriptorPool;
static PFN_vkCreateDescriptorSetLayout					qvkCreateDescriptorSetLayout;
static PFN_vkCreateFence								qvkCreateFence;
static PFN_vkCreateFramebuffer							qvkCreateFramebuffer;
static PFN_vkCreateComputePipelines						qvkCreateComputePipelines;
static PFN_vkCreateGraphicsPipelines					qvkCreateGraphicsPipelines;
static PFN_vkCreateImage								qvkCreateImage;
static PFN_vkCreateImageView							qvkCreateImageView;
static PFN_vkCreatePipelineLayout						qvkCreatePipelineLayout;
static PFN_vkCreatePipelineCache						qvkCreatePipelineCache;
static PFN_vkCreateQueryPool							qvkCreateQueryPool;
static PFN_vkCreateRenderPass							qvkCreateRenderPass;
static PFN_vkCreateSampler								qvkCreateSampler;
static PFN_vkCreateSemaphore							qvkCreateSemaphore;
static PFN_vkCreateShaderModule							qvkCreateShaderModule;
static PFN_vkDestroyBuffer								qvkDestroyBuffer;
static PFN_vkDestroyCommandPool							qvkDestroyCommandPool;
static PFN_vkDestroyDescriptorPool						qvkDestroyDescriptorPool;
static PFN_vkDestroyDescriptorSetLayout					qvkDestroyDescriptorSetLayout;
static PFN_vkDestroyDevice								qvkDestroyDevice;
static PFN_vkDestroyFence								qvkDestroyFence;
static PFN_vkDestroyFramebuffer							qvkDestroyFramebuffer;
static PFN_vkDestroyImage								qvkDestroyImage;
static PFN_vkDestroyImageView							qvkDestroyImageView;
static PFN_vkDestroyPipeline							qvkDestroyPipeline;
static PFN_vkDestroyPipelineCache						qvkDestroyPipelineCache;
static PFN_vkDestroyPipelineLayout						qvkDestroyPipelineLayout;
static PFN_vkDestroyQueryPool							qvkDestroyQueryPool;
static PFN_vkDestroyRenderPass							qvkDestroyRenderPass;
static PFN_vkDestroySampler								qvkDestroySampler;
static PFN_vkDestroySemaphore							qvkDestroySemaphore;
static PFN_vkDestroyShaderModule						qvkDestroyShaderModule;
static PFN_vkDeviceWaitIdle								qvkDeviceWaitIdle;
static PFN_vkEndCommandBuffer							qvkEndCommandBuffer;
static PFN_vkFlushMappedMemoryRanges					qvkFlushMappedMemoryRanges;
static PFN_vkFreeCommandBuffers							qvkFreeCommandBuffers;
static PFN_vkFreeDescriptorSets							qvkFreeDescriptorSets;
static PFN_vkFreeMemory									qvkFreeMemory;
static PFN_vkGetBufferMemoryRequirements				qvkGetBufferMemoryRequirements;
static PFN_vkGetDeviceQueue								qvkGetDeviceQueue;
static PFN_vkGetImageMemoryRequirements					qvkGetImageMemoryRequirements;
static PFN_vkGetImageSubresourceLayout					qvkGetImageSubresourceLayout;
static PFN_vkInvalidateMappedMemoryRanges				qvkInvalidateMappedMemoryRanges;
static PFN_vkMapMemory									qvkMapMemory;
static PFN_vkQueueSubmit								qvkQueueSubmit;
static PFN_vkQueueWaitIdle								qvkQueueWaitIdle;
static PFN_vkResetCommandBuffer							qvkResetCommandBuffer;
static PFN_vkResetDescriptorPool						qvkResetDescriptorPool;
static PFN_vkResetFences								qvkResetFences;
static PFN_vkGetQueryPoolResults						qvkGetQueryPoolResults;
static PFN_vkUnmapMemory								qvkUnmapMemory;
static PFN_vkUpdateDescriptorSets						qvkUpdateDescriptorSets;
static PFN_vkWaitForFences								qvkWaitForFences;
static PFN_vkAcquireNextImageKHR						qvkAcquireNextImageKHR;
static PFN_vkCreateSwapchainKHR							qvkCreateSwapchainKHR;
static PFN_vkDestroySwapchainKHR						qvkDestroySwapchainKHR;
static PFN_vkGetSwapchainImagesKHR						qvkGetSwapchainImagesKHR;
static PFN_vkQueuePresentKHR							qvkQueuePresentKHR;

static PFN_vkGetBufferMemoryRequirements2KHR			qvkGetBufferMemoryRequirements2KHR;
static PFN_vkGetImageMemoryRequirements2KHR				qvkGetImageMemoryRequirements2KHR;

static PFN_vkDebugMarkerSetObjectNameEXT				qvkDebugMarkerSetObjectNameEXT;

static PFN_vkCmdClearColorImage								qvkCmdClearColorImage;

////////////////////////////////////////////////////////////////////////////

// forward declaration
VkPipeline create_pipeline( const Vk_Pipeline_Def *def, renderPass_t renderPassIndex, uint32_t def_index );
static uint32_t vk_alloc_pipeline( const Vk_Pipeline_Def *def );
static VkPipeline vk_gen_pipeline( uint32_t index );
uint32_t vk_find_pipeline_ext( uint32_t base, const Vk_Pipeline_Def *def, qboolean use );

#define VK_FROXEL_DEFAULT_WIDTH 160
#define VK_FROXEL_DEFAULT_HEIGHT 90
#define VK_FROXEL_DEFAULT_SLICES 96
#define VK_FLUID_DEFAULT_RESOLUTION_SCALE 0.5f
#define VK_FLUID_MAX_PRESSURE_ITERATIONS 64
#define VK_VOLUMETRIC_MAX_VOLUMES 24
#define VK_VOLUMETRIC_MAX_LIGHTS 32
#define VK_VOLUMETRIC_QUERY_SLOTS 16
#define VK_VOLUMETRIC_QUERY_COUNT (VK_VOLUMETRIC_QUERY_SLOTS * NUM_COMMAND_BUFFERS)
#define VK_VOLUMETRIC_TELEMETRY_COUNTERS 8

typedef struct {
	float invProj[16];
	float invView[16];
	float proj[16];
	float viewProj[16];
	float prevView[16];
	float prevViewProj[16];
	float viewOrigin[4];
	float sunDirection[4];
	float fogColor[4];
	float densityParams[4];
	float worldMin[4];
	float worldMax[4];
	float gridDim[4];
	float miscParams[4];
	float sliceParams[4];
	float phaseParams[4];
	float noiseParams[4];
	float noiseScroll[4];
	float temporalParams[4];
	float qualityParams[4];
	float windParams[4];
	float volumeCounts[4];
	float passParams[4];
	float volumeBoundsMin[VK_VOLUMETRIC_MAX_VOLUMES][4];
	float volumeBoundsMax[VK_VOLUMETRIC_MAX_VOLUMES][4];
	float volumeColorDensity[VK_VOLUMETRIC_MAX_VOLUMES][4];
	float lightPosRadius[VK_VOLUMETRIC_MAX_LIGHTS][4];
	float lightColorType[VK_VOLUMETRIC_MAX_LIGHTS][4];
	float lightDirAngle[VK_VOLUMETRIC_MAX_LIGHTS][4];
	float lightExtra[VK_VOLUMETRIC_MAX_LIGHTS][4];
	float sunShadowMatrix0[16];
	float shadowParams0[4];
	float shadowMapSize0[4];
	float localSpotShadowMatrix[VK_VOLUMETRIC_MAX_LIGHTS][16];
	float localPointShadowMatrix[VK_VOLUMETRIC_MAX_LIGHTS][6][16];
	float localShadowAtlasUv[VK_VOLUMETRIC_MAX_LIGHTS][4];
	float localSpotShadowMapSize[4];
	float localPointShadowMapSize[4];
	float fluidParams0[4];
	float fluidParams1[4];
	float fluidParams2[4];
	float fluidWorldMap[4];
	float telemetryParams0[4];
	float telemetryParams1[4];
} volumetric_params_t;

_Static_assert( ( sizeof( volumetric_params_t ) % 16 ) == 0, "volumetric_params_t must be 16-byte aligned in size" );
#define VK_VOLUMETRIC_ASSERT_ALIGNED16(member) _Static_assert( ( offsetof( volumetric_params_t, member ) % 16 ) == 0, "volumetric_params_t::" #member " must be 16-byte aligned" )
VK_VOLUMETRIC_ASSERT_ALIGNED16( invProj );
VK_VOLUMETRIC_ASSERT_ALIGNED16( invView );
VK_VOLUMETRIC_ASSERT_ALIGNED16( proj );
VK_VOLUMETRIC_ASSERT_ALIGNED16( viewProj );
VK_VOLUMETRIC_ASSERT_ALIGNED16( prevView );
VK_VOLUMETRIC_ASSERT_ALIGNED16( prevViewProj );
VK_VOLUMETRIC_ASSERT_ALIGNED16( viewOrigin );
VK_VOLUMETRIC_ASSERT_ALIGNED16( sunDirection );
VK_VOLUMETRIC_ASSERT_ALIGNED16( fogColor );
VK_VOLUMETRIC_ASSERT_ALIGNED16( densityParams );
VK_VOLUMETRIC_ASSERT_ALIGNED16( worldMin );
VK_VOLUMETRIC_ASSERT_ALIGNED16( worldMax );
VK_VOLUMETRIC_ASSERT_ALIGNED16( gridDim );
VK_VOLUMETRIC_ASSERT_ALIGNED16( miscParams );
VK_VOLUMETRIC_ASSERT_ALIGNED16( sliceParams );
VK_VOLUMETRIC_ASSERT_ALIGNED16( phaseParams );
VK_VOLUMETRIC_ASSERT_ALIGNED16( noiseParams );
VK_VOLUMETRIC_ASSERT_ALIGNED16( noiseScroll );
VK_VOLUMETRIC_ASSERT_ALIGNED16( temporalParams );
VK_VOLUMETRIC_ASSERT_ALIGNED16( qualityParams );
VK_VOLUMETRIC_ASSERT_ALIGNED16( windParams );
VK_VOLUMETRIC_ASSERT_ALIGNED16( volumeCounts );
VK_VOLUMETRIC_ASSERT_ALIGNED16( passParams );
VK_VOLUMETRIC_ASSERT_ALIGNED16( volumeBoundsMin );
VK_VOLUMETRIC_ASSERT_ALIGNED16( volumeBoundsMax );
VK_VOLUMETRIC_ASSERT_ALIGNED16( volumeColorDensity );
VK_VOLUMETRIC_ASSERT_ALIGNED16( lightPosRadius );
VK_VOLUMETRIC_ASSERT_ALIGNED16( lightColorType );
VK_VOLUMETRIC_ASSERT_ALIGNED16( lightDirAngle );
VK_VOLUMETRIC_ASSERT_ALIGNED16( lightExtra );
VK_VOLUMETRIC_ASSERT_ALIGNED16( sunShadowMatrix0 );
VK_VOLUMETRIC_ASSERT_ALIGNED16( shadowParams0 );
VK_VOLUMETRIC_ASSERT_ALIGNED16( shadowMapSize0 );
VK_VOLUMETRIC_ASSERT_ALIGNED16( localSpotShadowMatrix );
VK_VOLUMETRIC_ASSERT_ALIGNED16( localPointShadowMatrix );
VK_VOLUMETRIC_ASSERT_ALIGNED16( localShadowAtlasUv );
VK_VOLUMETRIC_ASSERT_ALIGNED16( localSpotShadowMapSize );
VK_VOLUMETRIC_ASSERT_ALIGNED16( localPointShadowMapSize );
VK_VOLUMETRIC_ASSERT_ALIGNED16( fluidParams0 );
VK_VOLUMETRIC_ASSERT_ALIGNED16( fluidParams1 );
VK_VOLUMETRIC_ASSERT_ALIGNED16( fluidParams2 );
VK_VOLUMETRIC_ASSERT_ALIGNED16( fluidWorldMap );
VK_VOLUMETRIC_ASSERT_ALIGNED16( telemetryParams0 );
VK_VOLUMETRIC_ASSERT_ALIGNED16( telemetryParams1 );
#undef VK_VOLUMETRIC_ASSERT_ALIGNED16

static VkSampler vk_find_sampler( const Vk_Sampler_Def *def );
static void vk_create_froxel_images( void );
static void vk_update_volumetric_descriptors( void );
static void vk_create_sun_shadow_resources( void );
static void vk_destroy_sun_shadow_resources( void );
static void vk_create_local_shadow_resources( void );
static void vk_destroy_local_shadow_resources( void );
static void vk_create_volumetric_pipelines( void );
static void vk_create_volumetric_params_buffer( void );
static void vk_destroy_volumetric_params_buffer( void );
static void vk_update_volumetric_params( void );
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
static void vk_reset_motion_history( void );

static float vk_prev_view_matrix[16];
static float vk_prev_projection_matrix[16];
static float vk_prev_viewproj_matrix[16];
static qboolean vk_prev_matrices_valid = qfalse;
static int vk_prev_volumetric_time_ms = 0;
static qboolean vk_prev_volumetric_time_valid = qfalse;
static float vk_volumetric_noise_time = 0.0f;
static float vk_prev_entity_model_matrices[MAX_REFENTITIES][16];
static float vk_curr_entity_model_matrices[MAX_REFENTITIES][16];
static int vk_prev_entity_model_handles[MAX_REFENTITIES];
static int vk_curr_entity_model_handles[MAX_REFENTITIES];
static int vk_prev_entity_types[MAX_REFENTITIES];
static int vk_curr_entity_types[MAX_REFENTITIES];
static qboolean vk_prev_entity_model_valid[MAX_REFENTITIES];
static qboolean vk_curr_entity_model_valid[MAX_REFENTITIES];
typedef struct {
	uint32_t camera_cut_events;
	uint32_t forced_camera_cut_events;
	uint32_t local_shadow_ready_spot;
	uint32_t local_shadow_ready_point;
	uint32_t local_light_count;
	uint32_t telemetry_nan_or_inf;
	uint32_t telemetry_extinction_clamp_hits;
	uint32_t telemetry_velocity_clamp_hits;
	uint32_t telemetry_density_clamp_hits;
	uint32_t telemetry_pressure_sanitize_hits;
	uint32_t telemetry_temporal_rejects;
} vk_volumetric_validation_state_t;
static vk_volumetric_validation_state_t vk_volumetric_validation_state;
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


static uint32_t find_memory_type( uint32_t memory_type_bits, VkMemoryPropertyFlags properties ) {
	VkPhysicalDeviceMemoryProperties memory_properties;
	uint32_t i;

	qvkGetPhysicalDeviceMemoryProperties( vk.physical_device, &memory_properties );

	for ( i = 0; i < memory_properties.memoryTypeCount; i++ ) {
		if ((memory_type_bits & (1 << i)) != 0 &&
			(memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}
	ri.Error( ERR_FATAL, "Vulkan: failed to find matching memory type with requested properties" );
	return ~0U;
}


static uint32_t find_memory_type2( uint32_t memory_type_bits, VkMemoryPropertyFlags properties, VkMemoryPropertyFlags *outprops ) {
	VkPhysicalDeviceMemoryProperties memory_properties;
	uint32_t i;

	qvkGetPhysicalDeviceMemoryProperties( vk.physical_device, &memory_properties );

	for ( i = 0; i < memory_properties.memoryTypeCount; i++ ) {
		if ( (memory_type_bits & (1 << i)) != 0 && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties ) {
			if ( outprops ) {
				*outprops = memory_properties.memoryTypes[i].propertyFlags;
			}
			return i;
		}
	}

	return ~0U;
}


static const char *pmode_to_str( VkPresentModeKHR mode )
{
	static char buf[32];

	switch ( mode ) {
		case VK_PRESENT_MODE_IMMEDIATE_KHR: return "IMMEDIATE";
		case VK_PRESENT_MODE_MAILBOX_KHR: return "MAILBOX";
		case VK_PRESENT_MODE_FIFO_KHR: return "FIFO";
		case VK_PRESENT_MODE_FIFO_RELAXED_KHR: return "FIFO_RELAXED";
		case VK_PRESENT_MODE_FIFO_LATEST_READY_EXT: return "FIFO_LATEST_READY";
		default: sprintf( buf, "mode#%x", mode ); return buf;
	};
}


#define CASE_STR(x) case (x): return #x

const char *vk_format_string( VkFormat format )
{
	static char buf[16];

	switch ( format ) {
		// color formats
		CASE_STR( VK_FORMAT_R5G5B5A1_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_B5G5R5A1_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_R5G6B5_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_B5G6R5_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_B8G8R8A8_SRGB );
		CASE_STR( VK_FORMAT_R8G8B8A8_SRGB );
		CASE_STR( VK_FORMAT_B8G8R8A8_SNORM );
		CASE_STR( VK_FORMAT_R8G8B8A8_SNORM );
		CASE_STR( VK_FORMAT_B8G8R8A8_UNORM );
		CASE_STR( VK_FORMAT_R8G8B8A8_UNORM );
		CASE_STR( VK_FORMAT_B4G4R4A4_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_R4G4B4A4_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_R16G16B16A16_UNORM );
		CASE_STR( VK_FORMAT_A2B10G10R10_UNORM_PACK32 );
		CASE_STR( VK_FORMAT_A2R10G10B10_UNORM_PACK32 );
		CASE_STR( VK_FORMAT_B10G11R11_UFLOAT_PACK32 );
		// depth formats
		CASE_STR( VK_FORMAT_D16_UNORM );
		CASE_STR( VK_FORMAT_D16_UNORM_S8_UINT );
		CASE_STR( VK_FORMAT_X8_D24_UNORM_PACK32 );
		CASE_STR( VK_FORMAT_D24_UNORM_S8_UINT );
		CASE_STR( VK_FORMAT_D32_SFLOAT );
		CASE_STR( VK_FORMAT_D32_SFLOAT_S8_UINT );
	default:
		Com_sprintf( buf, sizeof( buf ), "#%i", format );
		return buf;
	}
}


static const char *vk_result_string( VkResult code ) {
	static char buffer[32];

	switch ( code ) {
		CASE_STR( VK_SUCCESS );
		CASE_STR( VK_NOT_READY );
		CASE_STR( VK_TIMEOUT );
		CASE_STR( VK_EVENT_SET );
		CASE_STR( VK_EVENT_RESET );
		CASE_STR( VK_INCOMPLETE );
		CASE_STR( VK_ERROR_OUT_OF_HOST_MEMORY );
		CASE_STR( VK_ERROR_OUT_OF_DEVICE_MEMORY );
		CASE_STR( VK_ERROR_INITIALIZATION_FAILED );
		CASE_STR( VK_ERROR_DEVICE_LOST );
		CASE_STR( VK_ERROR_MEMORY_MAP_FAILED );
		CASE_STR( VK_ERROR_LAYER_NOT_PRESENT );
		CASE_STR( VK_ERROR_EXTENSION_NOT_PRESENT );
		CASE_STR( VK_ERROR_FEATURE_NOT_PRESENT );
		CASE_STR( VK_ERROR_INCOMPATIBLE_DRIVER );
		CASE_STR( VK_ERROR_TOO_MANY_OBJECTS );
		CASE_STR( VK_ERROR_FORMAT_NOT_SUPPORTED );
		CASE_STR( VK_ERROR_FRAGMENTED_POOL );
		CASE_STR( VK_ERROR_UNKNOWN );
		CASE_STR( VK_ERROR_OUT_OF_POOL_MEMORY );
		CASE_STR( VK_ERROR_INVALID_EXTERNAL_HANDLE );
		CASE_STR( VK_ERROR_FRAGMENTATION );
		CASE_STR( VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS );
		CASE_STR( VK_ERROR_SURFACE_LOST_KHR );
		CASE_STR( VK_ERROR_NATIVE_WINDOW_IN_USE_KHR );
		CASE_STR( VK_SUBOPTIMAL_KHR );
		CASE_STR( VK_ERROR_OUT_OF_DATE_KHR );
		CASE_STR( VK_ERROR_INCOMPATIBLE_DISPLAY_KHR );
		CASE_STR( VK_ERROR_VALIDATION_FAILED_EXT );
		CASE_STR( VK_ERROR_INVALID_SHADER_NV );
		CASE_STR( VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT );
		CASE_STR( VK_ERROR_NOT_PERMITTED_EXT );
		CASE_STR( VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT );
		CASE_STR( VK_THREAD_IDLE_KHR );
		CASE_STR( VK_THREAD_DONE_KHR );
		CASE_STR( VK_OPERATION_DEFERRED_KHR );
		CASE_STR( VK_OPERATION_NOT_DEFERRED_KHR );
		CASE_STR( VK_PIPELINE_COMPILE_REQUIRED_EXT );
	default:
		sprintf( buffer, "code %i", code );
		return buffer;
	}
}
#undef CASE_STR

#define VK_CHECK( function_call ) { \
	VkResult _res_ = function_call; \
	if ( _res_ < 0 ) { \
		ri.Error( ERR_FATAL, "Vulkan: %s returned %s", #function_call, vk_result_string( _res_ ) ); \
	} \
}


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


static VkCommandBuffer begin_command_buffer( void )
{
	VkCommandBufferBeginInfo begin_info;
	VkCommandBufferAllocateInfo alloc_info;
	VkCommandBuffer command_buffer;

	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.commandPool = vk.command_pool;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = 1;
	VK_CHECK( qvkAllocateCommandBuffers( vk.device, &alloc_info, &command_buffer ) );

	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.pNext = NULL;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	begin_info.pInheritanceInfo = NULL;

	VK_CHECK( qvkBeginCommandBuffer( command_buffer, &begin_info ) );

	return command_buffer;
}


static void end_command_buffer( VkCommandBuffer command_buffer, const char *location )
{
	(void)location;
#ifdef USE_UPLOAD_QUEUE
	const VkPipelineStageFlags wait_dst_stage_mask = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	VkSemaphore waits;
#endif
	VkSubmitInfo submit_info;
	VkCommandBuffer cmdbuf[1];

	cmdbuf[0] = command_buffer;

	VK_CHECK( qvkEndCommandBuffer( command_buffer ) );

	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = NULL;
#ifdef USE_UPLOAD_QUEUE
	if ( vk.rendering_finished != VK_NULL_HANDLE ) {
		waits = vk.rendering_finished;
		vk.rendering_finished = VK_NULL_HANDLE;
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &waits;
		submit_info.pWaitDstStageMask = &wait_dst_stage_mask;
	} else 
#endif
	{
		submit_info.waitSemaphoreCount = 0;
		submit_info.pWaitSemaphores = NULL;
		submit_info.pWaitDstStageMask = NULL;
	}

	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = cmdbuf;
	submit_info.signalSemaphoreCount = 0;
	submit_info.pSignalSemaphores = NULL;

	VK_CHECK( qvkQueueSubmit( vk.queue, 1, &submit_info, VK_NULL_HANDLE ) );

	vk_queue_wait_idle();

	qvkFreeCommandBuffers( vk.device, vk.command_pool, 1, cmdbuf );
}


static void record_image_layout_transition( VkCommandBuffer command_buffer, VkImage image, VkImageAspectFlags image_aspect_flags, 
	VkImageLayout old_layout, VkImageLayout new_layout, uint32_t src_stage_override, uint32_t dst_stage_override ) {
	VkImageMemoryBarrier barrier;
	uint32_t src_stage, dst_stage;

	switch ( old_layout ) {
		case VK_IMAGE_LAYOUT_UNDEFINED:
			src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			barrier.srcAccessMask = VK_ACCESS_NONE;
			break;
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			src_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_GENERAL:
			src_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			src_stage = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
			src_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			src_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.srcAccessMask = VK_ACCESS_NONE;
			break;
		default:
			ri.Error( ERR_DROP, "unsupported old layout %i", old_layout );
			src_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			barrier.srcAccessMask = VK_ACCESS_NONE;
			break;
	}

	switch ( new_layout ) {
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_GENERAL:
			dst_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.dstAccessMask = VK_ACCESS_NONE;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
			break;
		default:
			ri.Error( ERR_DROP, "unsupported new layout %i", new_layout);
			dst_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			barrier.dstAccessMask = VK_ACCESS_NONE;
			break;
	}

	if ( src_stage_override != 0 ) {
		src_stage = src_stage_override;
	}
	if ( dst_stage_override != 0 ) {
		dst_stage = dst_stage_override;
	}

	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.pNext = NULL;
	//barrier.srcAccessMask = src_access_flags;
	//barrier.dstAccessMask = dst_access_flags;
	barrier.oldLayout = old_layout;
	barrier.newLayout = new_layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = image_aspect_flags;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	qvkCmdPipelineBarrier( command_buffer, src_stage, dst_stage, 0, 0, NULL, 0, NULL, 1, &barrier );
}


// debug markers
#define SET_OBJECT_NAME(obj,objName,objType) vk_set_object_name( (uint64_t)(obj), (objName), (objType) )

static void vk_set_object_name( uint64_t obj, const char *objName, VkDebugReportObjectTypeEXT objType )
{
	if ( qvkDebugMarkerSetObjectNameEXT && obj )
	{
		VkDebugMarkerObjectNameInfoEXT info;
		info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
		info.pNext = NULL;
		info.objectType = objType;
		info.object = obj;
		info.pObjectName = objName;
		qvkDebugMarkerSetObjectNameEXT( vk.device, &info );
	}
}


static void vk_create_swapchain( VkPhysicalDevice physical_device, VkDevice device, VkSurfaceKHR surface, VkSurfaceFormatKHR surface_format, VkSwapchainKHR *swapchain, qboolean verbose ) {
	VkImageViewCreateInfo view;
	VkSurfaceCapabilitiesKHR surface_caps;
	VkExtent2D image_extent;
	uint32_t present_mode_count, i;
	VkPresentModeKHR present_mode;
	VkPresentModeKHR *present_modes;
	uint32_t image_count;
	VkSwapchainCreateInfoKHR desc;
	qboolean mailbox_supported = qfalse;
	qboolean immediate_supported = qfalse;
	qboolean fifo_relaxed_supported = qfalse;
	int v;

	VK_CHECK( qvkGetPhysicalDeviceSurfaceCapabilitiesKHR( physical_device, surface, &surface_caps ) );

	image_extent = surface_caps.currentExtent;
	if ( image_extent.width == 0xffffffff && image_extent.height == 0xffffffff ) {
		image_extent.width = MIN( surface_caps.maxImageExtent.width, MAX( surface_caps.minImageExtent.width, (uint32_t) glConfig.vidWidth ) );
		image_extent.height = MIN( surface_caps.maxImageExtent.height, MAX( surface_caps.minImageExtent.height, (uint32_t) glConfig.vidHeight ) );
	}

	vk.clearAttachment = qtrue;

	if ( !vk.fboActive ) {
		// VK_IMAGE_USAGE_TRANSFER_DST_BIT is required by image clear operations.
		if ( ( surface_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT ) == 0 ) {
			vk.clearAttachment = qfalse;
			ri.Printf( PRINT_WARNING, "VK_IMAGE_USAGE_TRANSFER_DST_BIT is not supported by the swapchain, \\r_clear might not work\n" );
		}
		// VK_IMAGE_USAGE_TRANSFER_SRC_BIT is required in order to take screenshots.
		if ((surface_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) == 0) {
			ri.Error(ERR_FATAL, "create_swapchain: VK_IMAGE_USAGE_TRANSFER_SRC_BIT is not supported by the swapchain");
		}
	}

	// determine present mode and swapchain image count
	VK_CHECK(qvkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, NULL));

	present_modes = (VkPresentModeKHR *) ri.Malloc( present_mode_count * sizeof( VkPresentModeKHR ) );
	VK_CHECK(qvkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, present_modes));

	if ( verbose ) {
		ri.Printf( PRINT_ALL, "...presentation modes:" );
	}
	for ( i = 0; i < present_mode_count; i++ ) {
		if ( verbose ) {
			ri.Printf( PRINT_ALL, " %s", pmode_to_str( present_modes[i] ) );
		}
		if ( present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR )
			mailbox_supported = qtrue;
		else if ( present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR )
			immediate_supported = qtrue;
		else if ( present_modes[i] == VK_PRESENT_MODE_FIFO_RELAXED_KHR )
			fifo_relaxed_supported = qtrue;
	}
	if ( verbose ) {
		ri.Printf( PRINT_ALL, "\n" );
	}

	ri.Free( present_modes );

	if ( ( v = ri.Cvar_VariableIntegerValue( "r_swapInterval" ) ) != 0 ) {
		if ( v == 2 && mailbox_supported )
			present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
		else if ( fifo_relaxed_supported )
			present_mode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
		else
			present_mode = VK_PRESENT_MODE_FIFO_KHR;
		image_count = MAX( MIN_SWAPCHAIN_IMAGES_FIFO, surface_caps.minImageCount );
	} else {
		if ( immediate_supported ) {
			present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
			image_count = MAX( MIN_SWAPCHAIN_IMAGES_IMM, surface_caps.minImageCount );
		} else if ( mailbox_supported ) {
			present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
			image_count = MAX( MIN_SWAPCHAIN_IMAGES_MAILBOX, surface_caps.minImageCount );
		} else if ( fifo_relaxed_supported ) {
			present_mode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
			image_count = MAX( MIN_SWAPCHAIN_IMAGES_FIFO, surface_caps.minImageCount );
		} else {
			present_mode = VK_PRESENT_MODE_FIFO_KHR;
			image_count = MAX( MIN_SWAPCHAIN_IMAGES_FIFO, surface_caps.minImageCount );
		}
	}

	if ( image_count < 2 ) {
		image_count = 2;
	}

	if ( surface_caps.maxImageCount == 0 && present_mode == VK_PRESENT_MODE_FIFO_KHR ) {
		image_count = MAX( MIN_SWAPCHAIN_IMAGES_FIFO_0, surface_caps.minImageCount );
	} else if ( surface_caps.maxImageCount > 0 ) {
		image_count = MIN( MIN( image_count, surface_caps.maxImageCount ), MAX_SWAPCHAIN_IMAGES );
	}

	if ( verbose ) {
		ri.Printf( PRINT_ALL, "...selected presentation mode: %s, image count: %i\n", pmode_to_str( present_mode ), image_count );
	}

	// create swap chain
	desc.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.surface = surface;
	desc.minImageCount = image_count;
	desc.imageFormat = surface_format.format;
	desc.imageColorSpace = surface_format.colorSpace;
	desc.imageExtent = image_extent;
	vk.swapchain_extent = image_extent;
	vk.swapchain_extent_valid = qtrue;
	desc.imageArrayLayers = 1;
	desc.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if ( !vk.fboActive ) {
		desc.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	}
	desc.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = NULL;
	desc.preTransform = surface_caps.currentTransform;
	//desc.compositeAlpha = get_composite_alpha( surface_caps.supportedCompositeAlpha );
	desc.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	desc.presentMode = present_mode;
	desc.clipped = VK_TRUE;
	desc.oldSwapchain = VK_NULL_HANDLE;

	VK_CHECK( qvkCreateSwapchainKHR( device, &desc, NULL, swapchain ) );

	VK_CHECK( qvkGetSwapchainImagesKHR( vk.device, vk.swapchain, &vk.swapchain_image_count, NULL ) );
	vk.swapchain_image_count = MIN( vk.swapchain_image_count, MAX_SWAPCHAIN_IMAGES );
	VK_CHECK( qvkGetSwapchainImagesKHR( vk.device, vk.swapchain, &vk.swapchain_image_count, vk.swapchain_images ) );

	for ( i = 0; (uint32_t) i < vk.swapchain_image_count; i++ ) {
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.pNext = NULL;
		view.flags = 0;
		view.image = vk.swapchain_images[i];
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.format = vk.present_format.format;
		view.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		view.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		view.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		view.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view.subresourceRange.baseMipLevel = 0;
		view.subresourceRange.levelCount = 1;
		view.subresourceRange.baseArrayLayer = 0;
		view.subresourceRange.layerCount = 1;

		VK_CHECK( qvkCreateImageView( vk.device, &view, NULL, &vk.swapchain_image_views[i] ) );

		SET_OBJECT_NAME( vk.swapchain_images[i], va( "swapchain image %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.swapchain_image_views[i], va( "swapchain image %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	}

	for ( i = 0; (uint32_t) i < vk.swapchain_image_count; i++ ) {
		VkSemaphoreCreateInfo s;
		s.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		s.pNext = NULL;
		s.flags = 0;
		VK_CHECK( qvkCreateSemaphore( vk.device, &s, NULL, &vk.swapchain_rendering_finished[i] ) );
		SET_OBJECT_NAME( vk.swapchain_rendering_finished[i], va( "swapchain_rendering_finished semaphore %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT );
	}

	if ( vk.initSwapchainLayout != VK_IMAGE_LAYOUT_UNDEFINED ) {
		VkCommandBuffer command_buffer = begin_command_buffer();

		for ( i = 0; (uint32_t) i < vk.swapchain_image_count; i++ ) {
			record_image_layout_transition( command_buffer, vk.swapchain_images[i],
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED, vk.initSwapchainLayout, 0, 0 );
		}

		end_command_buffer( command_buffer, __func__ );
	}
}

static qboolean vk_query_surface_extent( VkExtent2D *extent ) {
	VkSurfaceCapabilitiesKHR caps;

	if ( qvkGetPhysicalDeviceSurfaceCapabilitiesKHR( vk.physical_device, vk_surface, &caps ) != VK_SUCCESS ) {
		return qfalse;
	}

	if ( caps.currentExtent.width == UINT32_MAX && caps.currentExtent.height == UINT32_MAX ) {
		extent->width = (uint32_t) gls.windowWidth;
		extent->height = (uint32_t) gls.windowHeight;
	}
	else {
		extent->width = caps.currentExtent.width;
		extent->height = caps.currentExtent.height;
	}

	if ( extent->width == 0 || extent->height == 0 ) {
		return qfalse;
	}

	return qtrue;
}

static void vk_log_swapchain_recreation( VkResult res, const VkExtent2D *old_extent, const VkExtent2D *new_extent ) {
	uint32_t old_width = vk.swapchain_extent_valid && old_extent ? old_extent->width : 0;
	uint32_t old_height = vk.swapchain_extent_valid && old_extent ? old_extent->height : 0;
	uint32_t new_width = new_extent ? new_extent->width : 0;
	uint32_t new_height = new_extent ? new_extent->height : 0;
	static int last_print_ms = -1;
	static VkResult last_res = VK_SUCCESS;
	static uint32_t last_old_width, last_old_height, last_new_width, last_new_height;
	static int last_fullscreen, last_refresh;
	const int now_ms = ri.Milliseconds();
	const qboolean same_as_last =
		( res == last_res ) &&
		( old_width == last_old_width ) &&
		( old_height == last_old_height ) &&
		( new_width == last_new_width ) &&
		( new_height == last_new_height ) &&
		( (int)glConfig.isFullscreen == last_fullscreen ) &&
		( glConfig.displayFrequency == last_refresh );

	// Avoid spam when the driver repeatedly returns SUBOPTIMAL_KHR with unchanged
	// swapchain metrics (common on some WMs/compositors).
	if ( same_as_last && last_print_ms >= 0 ) {
		return;
	}

	ri.Printf( PRINT_WARNING, "vk_present_frame(): %s old=%ux%u new=%ux%u fullscreen=%d refresh=%d\n",
		vk_result_string( res ), old_width, old_height, new_width, new_height, glConfig.isFullscreen, glConfig.displayFrequency );

	last_print_ms = now_ms;
	last_res = res;
	last_old_width = old_width;
	last_old_height = old_height;
	last_new_width = new_width;
	last_new_height = new_height;
	last_fullscreen = (int)glConfig.isFullscreen;
	last_refresh = glConfig.displayFrequency;
}

static void vk_create_render_passes( void )
{
	VkAttachmentDescription attachments[5]; // color resolve | depth | motion resolve | msaa color | msaa motion
	VkAttachmentReference colorResolveRefs[2];
	VkAttachmentReference colorResolveRef;
	VkAttachmentReference colorRefs[2];
	VkAttachmentReference colorRef0;
	VkAttachmentReference depthRef0;
	VkSubpassDescription subpass;
	VkSubpassDependency deps[3];
	VkRenderPassCreateInfo desc;
	VkFormat depth_format;
	VkDevice device;
	uint32_t i;

	depth_format = vk.depth_format;
	device = vk.device;

	if ( r_fbo->integer == 0 )
	{
		// presentation
		attachments[0].flags = 0;
		attachments[0].format = vk.present_format.format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
#ifdef USE_BUFFER_CLEAR
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
#else
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;	// Assuming this will be completely overwritten
#endif
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;		// needed for presentation
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = vk.initSwapchainLayout;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	}
	else
	{
		// resolve/color buffer
		attachments[0].flags = 0;
		attachments[0].format = vk.color_format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;

#ifdef USE_BUFFER_CLEAR
		if ( vk.msaaActive )
			attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;	// Assuming this will be completely overwritten
		else
			attachments[ 0 ].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
#else
		attachments[ 0 ].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;	// Assuming this will be completely overwritten
#endif

		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;   // needed for next render pass
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}

	// depth buffer
	attachments[1].flags = 0;
	attachments[1].format = depth_format;
	attachments[1].samples = vkSamples;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Need empty depth buffer before use
	attachments[1].stencilLoadOp = glConfig.stencilBits ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	if ( r_bloom->integer || ( r_ssao && r_ssao->integer ) ) {
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE; // keep it for post-bloom pass
		attachments[1].stencilStoreOp = glConfig.stencilBits ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
	} else {
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	}
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	colorRefs[0].attachment = 0;
	colorRefs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorRefs[1].attachment = VK_ATTACHMENT_UNUSED;
	colorRefs[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	depthRef0.attachment = 1;
	depthRef0.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	Com_Memset( colorResolveRefs, 0, sizeof( colorResolveRefs ) );
	colorResolveRefs[0].attachment = VK_ATTACHMENT_UNUSED;
	colorResolveRefs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorResolveRefs[1].attachment = VK_ATTACHMENT_UNUSED;
	colorResolveRefs[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	if ( r_fbo->integer ) {
		// velocity buffer used for per-pixel reprojection.
		attachments[2].flags = 0;
		attachments[2].format = VK_FORMAT_R16G16_SFLOAT;
		attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[2].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		attachments[2].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		colorRefs[1].attachment = 2;
	}

	Com_Memset( &subpass, 0, sizeof( subpass ) );
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = r_fbo->integer ? 2 : 1;
	subpass.pColorAttachments = colorRefs;
	subpass.pDepthStencilAttachment = &depthRef0;
	subpass.pResolveAttachments = NULL;

	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.pAttachments = attachments;
	desc.pSubpasses = &subpass;

	desc.subpassCount = 1;
	desc.attachmentCount = r_fbo->integer ? 3 : 2;

	if ( vk.msaaActive )
	{
		attachments[3].flags = 0;
		attachments[3].format = vk.color_format;
		attachments[3].samples = vkSamples;
#ifdef USE_BUFFER_CLEAR
		attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
#else
		attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
#endif
		if ( r_bloom->integer ) {
			attachments[3].storeOp = VK_ATTACHMENT_STORE_OP_STORE; // keep it for post-bloom pass
		} else {
			attachments[3].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // Intermediate storage (not written)
		}
		attachments[3].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[3].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[3].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachments[3].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		if ( r_fbo->integer ) {
			attachments[4].flags = 0;
			attachments[4].format = VK_FORMAT_R16G16_SFLOAT;
			attachments[4].samples = vkSamples;
#ifdef USE_BUFFER_CLEAR
			attachments[4].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
#else
			attachments[4].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
#endif
			attachments[4].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[4].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[4].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[4].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			attachments[4].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			desc.attachmentCount = 5;

			colorRefs[0].attachment = 3; // msaa scene color attachment
			colorRefs[1].attachment = 4; // msaa motion attachment

			colorResolveRefs[0].attachment = 0; // scene resolve
			colorResolveRefs[1].attachment = 2; // motion resolve
			subpass.pResolveAttachments = colorResolveRefs;
		} else {
			desc.attachmentCount = 3;

			colorRefs[0].attachment = 2; // msaa image attachment
			colorResolveRefs[0].attachment = 0; // resolve image attachment
			subpass.pResolveAttachments = &colorResolveRefs[0];
		}
	}

	// subpass dependencies

	Com_Memset( &deps, 0, sizeof( deps ) );

	deps[2].srcSubpass = VK_SUBPASS_EXTERNAL;
	deps[2].dstSubpass = 0;
	deps[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;	// What pipeline stage is waiting on the dependency
	deps[2].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;	// What pipeline stage is waiting on the dependency
	deps[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;			// What access scopes are influence the dependency
	deps[2].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;			// What access scopes are waiting on the dependency
	deps[2].dependencyFlags = 0;

	if ( r_fbo->integer == 0 )
	{
		desc.dependencyCount = 1;
		desc.pDependencies = &deps[2];

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.main ) );
		SET_OBJECT_NAME( vk.render_pass.main, "render pass - main", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

		return;
	}

	desc.dependencyCount = 2;
	desc.pDependencies = &deps[0];

	deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	deps[0].dstSubpass = 0;
	deps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;			// What pipeline stage must have completed for the dependency
	deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;	// What pipeline stage is waiting on the dependency
	deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;						// What access scopes are influence the dependency
	deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; // What access scopes are waiting on the dependency
	deps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;					// Only need the current fragment (or tile) synchronized, not the whole framebuffer

	deps[1].srcSubpass = 0;
	deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;	// Fragment data has been written
	deps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;			// Don't start shading until data is available
	deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;			// Waiting for color data to be written
	deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;						// Don't read things from the shader before ready
	deps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;					// Only need the current fragment (or tile) synchronized, not the whole framebuffer

	VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.main ) );
	SET_OBJECT_NAME( vk.render_pass.main, "render pass - main", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

	if ( r_bloom->integer ) {

		// post-bloom pass
		// color buffer
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // load from previous pass
		if ( r_fbo->integer ) {
			attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		}
		 // depth buffer
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		if ( vk.msaaActive ) {
			// msaa render target
			attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			attachments[3].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			if ( r_fbo->integer ) {
				attachments[4].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
				attachments[4].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			}
		}
		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.post_bloom ) );
		SET_OBJECT_NAME( vk.render_pass.post_bloom, "render pass - post_bloom", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

		// bloom extraction, using resolved/main fbo as a source
		desc.attachmentCount = 1;

		colorRef0.attachment = 0;
		colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		Com_Memset( &subpass, 0, sizeof( subpass ) );
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef0;

		attachments[0].flags = 0;
		attachments[0].format = vk.bloom_format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;	// Assuming this will be completely overwritten
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;		// needed for next render pass
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.bloom_extract ) );
		SET_OBJECT_NAME( vk.render_pass.bloom_extract, "render pass - bloom_extract", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

		for ( i = 0; i < ARRAY_LEN( vk.render_pass.blur ); i++ )
		{
			VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.blur[i] ) );
			SET_OBJECT_NAME( vk.render_pass.blur[i], va( "render pass - blur %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
		}
	}

	if ( r_ssao && r_ssao->integer && r_fbo->integer )
	{
		// ssao render pass
		desc.attachmentCount = 1;

		colorRef0.attachment = 0;
		colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		Com_Memset( &subpass, 0, sizeof( subpass ) );
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef0;

		attachments[0].flags = 0;
		attachments[0].format = vk.ssao_format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.ssao ) );
		SET_OBJECT_NAME( vk.render_pass.ssao, "render pass - ssao", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.ssao_blur ) );
		SET_OBJECT_NAME( vk.render_pass.ssao_blur, "render pass - ssao_blur", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

		// ssao combine pass (write back to main color)
		attachments[0].flags = 0;
		attachments[0].format = vk.color_format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.ssao_combine ) );
		SET_OBJECT_NAME( vk.render_pass.ssao_combine, "render pass - ssao_combine", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
	}

	if ( vk.fboActive )
	{
		Com_Memset( &subpass, 0, sizeof( subpass ) );
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef0;

		attachments[0].flags = 0;
		attachments[0].format = vk.color_format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.pAttachments = attachments;
		desc.attachmentCount = 1;
		desc.pSubpasses = &subpass;
		desc.subpassCount = 1;
		desc.dependencyCount = 0;
		desc.pDependencies = NULL;

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.volumetric ) );
		SET_OBJECT_NAME( vk.render_pass.volumetric, "render pass - volumetric fog", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
	}

	// capture render pass
	if ( vk.capture.image )
	{
		Com_Memset( &subpass, 0, sizeof( subpass ) );

		attachments[0].flags = 0;
		attachments[0].format = vk.capture_format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // this will be completely overwritten
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;   // needed for next render pass
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

		colorRef0.attachment = 0;
		colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef0;

		desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.pAttachments = attachments;
		desc.attachmentCount = 1;
		desc.pSubpasses = &subpass;
		desc.subpassCount = 1;

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.capture ) );
		SET_OBJECT_NAME( vk.render_pass.capture, "render pass - capture", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
	}

	colorRef0.attachment = 0;
	colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	desc.attachmentCount = 1;

	Com_Memset( &subpass, 0, sizeof( subpass ) );
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef0;

	// gamma post-processing
	attachments[0].flags = 0;
	attachments[0].format = vk.present_format.format;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE; // needed for presentation
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = vk.initSwapchainLayout;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	desc.dependencyCount = 1;
	desc.pDependencies = &deps[2];

	VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.gamma ) );
	SET_OBJECT_NAME( vk.render_pass.gamma, "render pass - gamma", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

	if ( vk.smaaActive )
	{
		VkAttachmentDescription smaaAttachment;
		VkAttachmentReference smaaColorRef;
		VkSubpassDescription smaaSubpass;
		VkSubpassDependency smaaDeps[2];
		VkRenderPassCreateInfo smaaDesc;
		VkRenderPass *smaaPasses[3];
		const char *smaaNames[3];

		smaaPasses[0] = &vk.render_pass.smaa_edge;
		smaaPasses[1] = &vk.render_pass.smaa_blend;
		smaaPasses[2] = &vk.render_pass.smaa_compose;

		smaaNames[0] = "render pass - smaa edge";
		smaaNames[1] = "render pass - smaa blend";
		smaaNames[2] = "render pass - smaa compose";

		Com_Memset( &smaaAttachment, 0, sizeof( smaaAttachment ) );
		smaaAttachment.flags = 0;
		smaaAttachment.format = vk.color_format;
		smaaAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		smaaAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		smaaAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		smaaAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		smaaAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		smaaAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		smaaAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		smaaColorRef.attachment = 0;
		smaaColorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		Com_Memset( &smaaSubpass, 0, sizeof( smaaSubpass ) );
		smaaSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		smaaSubpass.colorAttachmentCount = 1;
		smaaSubpass.pColorAttachments = &smaaColorRef;

		Com_Memset( smaaDeps, 0, sizeof( smaaDeps ) );
		smaaDeps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		smaaDeps[0].dstSubpass = 0;
		smaaDeps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		smaaDeps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		smaaDeps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		smaaDeps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		smaaDeps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		smaaDeps[1].srcSubpass = 0;
		smaaDeps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		smaaDeps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		smaaDeps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		smaaDeps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		smaaDeps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		smaaDeps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		Com_Memset( &smaaDesc, 0, sizeof( smaaDesc ) );
		smaaDesc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		smaaDesc.pNext = NULL;
		smaaDesc.flags = 0;
		smaaDesc.pAttachments = &smaaAttachment;
		smaaDesc.attachmentCount = 1;
		smaaDesc.pSubpasses = &smaaSubpass;
		smaaDesc.subpassCount = 1;
		smaaDesc.pDependencies = smaaDeps;
		smaaDesc.dependencyCount = 2;

		for ( i = 0; i < ARRAY_LEN( smaaPasses ); i++ )
		{
			VK_CHECK( qvkCreateRenderPass( device, &smaaDesc, NULL, smaaPasses[i] ) );
			SET_OBJECT_NAME( *smaaPasses[i], smaaNames[i], VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
		}
	}

	// screenmap
	desc.dependencyCount = 2;
	desc.pDependencies = &deps[0];

	// screenmap resolve/color buffer
	attachments[0].flags = 0;
	attachments[0].format = vk.color_format;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
#ifdef USE_BUFFER_CLEAR
	if ( vk.screenMapSamples > VK_SAMPLE_COUNT_1_BIT )
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	else
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
#else
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // Assuming this will be completely overwritten
#endif
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;   // needed for next render pass
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// screenmap depth buffer
	attachments[1].flags = 0;
	attachments[1].format = depth_format;
	attachments[1].samples = vk.screenMapSamples;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Need empty depth buffer before use
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	colorRef0.attachment = 0;
	colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	depthRef0.attachment = 1;
	depthRef0.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	Com_Memset( &subpass, 0, sizeof( subpass ) );
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef0;
	subpass.pDepthStencilAttachment = &depthRef0;

	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.pAttachments = attachments;
	desc.pSubpasses = &subpass;
	desc.subpassCount = 1;
	desc.attachmentCount = 2;
	desc.dependencyCount = 2;
	desc.pDependencies = deps;

	if ( vk.screenMapSamples > VK_SAMPLE_COUNT_1_BIT ) {

		attachments[2].flags = 0;
		attachments[2].format = vk.color_format;
		attachments[2].samples = vk.screenMapSamples;
#ifdef USE_BUFFER_CLEAR
		attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
#else
		attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
#endif
		attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[2].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachments[2].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		desc.attachmentCount = 3;

		colorRef0.attachment = 2; // screenmap msaa image attachment
		colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		colorResolveRef.attachment = 0; // screenmap resolve image attachment
		colorResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		subpass.pResolveAttachments = &colorResolveRef;
	}

	VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.screenmap ) );

	SET_OBJECT_NAME( vk.render_pass.screenmap, "render pass - screenmap", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

	// sun shadow (always 1x depth/color so compute sampling is never MSAA)
	attachments[0].flags = 0;
	attachments[0].format = vk.color_format;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	attachments[1].flags = 0;
	attachments[1].format = depth_format;
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	colorRef0.attachment = 0;
	colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	depthRef0.attachment = 1;
	depthRef0.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	Com_Memset( &subpass, 0, sizeof( subpass ) );
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef0;
	subpass.pDepthStencilAttachment = &depthRef0;
	subpass.pResolveAttachments = NULL;

	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.pAttachments = attachments;
	desc.attachmentCount = 2;
	desc.pSubpasses = &subpass;
	desc.subpassCount = 1;
	desc.dependencyCount = 2;
	desc.pDependencies = deps;

	VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.sun_shadow ) );
	SET_OBJECT_NAME( vk.render_pass.sun_shadow, "render pass - sun shadow", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

	VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.local_spot_shadow ) );
	SET_OBJECT_NAME( vk.render_pass.local_spot_shadow, "render pass - local spot shadow", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

	VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.local_point_shadow ) );
	SET_OBJECT_NAME( vk.render_pass.local_point_shadow, "render pass - local point shadow", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

#ifdef VK_PBR_BRDFLUT
    if( vk.pbrActive )
    {
    #ifdef VK_CUBEMAP 
        if ( vk.cubemapActive ) 
        {   			
			desc.attachmentCount = 2;
			attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[1].samples = vkSamples;

			colorRef0.attachment = 0;
			colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			depthRef0.attachment = 1;
			depthRef0.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			Com_Memset( &subpass, 0, sizeof( subpass ) );
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.colorAttachmentCount = 1;
			subpass.pColorAttachments = &colorRef0;
			subpass.pDepthStencilAttachment = &depthRef0;

			if ( vk.msaaActive ) {
				desc.attachmentCount = 3;
				attachments[2].samples = vkSamples;
				attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE; 

				colorRef0.attachment = 2; // msaa image attachment
				colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

				colorResolveRef.attachment = 0; // resolve image attachment
				colorResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

				subpass.pResolveAttachments = &colorResolveRef;
			}

            VK_CHECK(qvkCreateRenderPass(device, &desc, NULL, &vk.render_pass.cubemap));
            SET_OBJECT_NAME(vk.render_pass.cubemap, "render pass - cubemap", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT);
        }  
    #endif

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
        
		attachments[0].format = VK_FORMAT_R16G16_SFLOAT;
        attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        
		colorRef0.attachment = 0;
        colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        
		Com_Memset(&subpass, 0, sizeof(subpass));
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef0;
        subpass.pDepthStencilAttachment = VK_NULL_HANDLE;

        Com_Memset(&desc, 0, sizeof(desc));
        desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        desc.pNext = NULL;
        desc.flags = 0;
        desc.pAttachments = attachments;
        desc.pSubpasses = &subpass;
        desc.subpassCount = 1;
        desc.attachmentCount = 1;
        desc.dependencyCount = 2;
        desc.pDependencies = deps;
        VK_CHECK(qvkCreateRenderPass(device, &desc, NULL, &vk.render_pass.brdflut));
        SET_OBJECT_NAME(vk.render_pass.brdflut, "render pass - brdf lut", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT);
    }
#endif
}


static void allocate_and_bind_image_memory(VkImage image) {
	VkMemoryRequirements memory_requirements;
	VkDeviceSize alignment;
	ImageChunk *chunk;
	int i;

	qvkGetImageMemoryRequirements(vk.device, image, &memory_requirements);

	if ( memory_requirements.size > vk.image_chunk_size ) {
		ri.Error( ERR_FATAL, "Vulkan: could not allocate memory, image is too large (%ikbytes).",
			(int)(memory_requirements.size/1024) );
	}

	chunk = NULL;

	// Try to find an existing chunk of sufficient capacity.
	alignment = memory_requirements.alignment;
	for ( i = 0; i < vk_world.num_image_chunks; i++ ) {
		// ensure that memory region has proper alignment
		VkDeviceSize offset = PAD( vk_world.image_chunks[i].used, alignment );

		if ( offset + memory_requirements.size <= vk.image_chunk_size ) {
			chunk = &vk_world.image_chunks[i];
			chunk->used = offset + memory_requirements.size;
			break;
		}
	}

	// Allocate a new chunk in case we couldn't find suitable existing chunk.
	if (chunk == NULL) {
		VkMemoryAllocateInfo alloc_info;
		VkDeviceMemory memory;

		if (vk_world.num_image_chunks >= MAX_IMAGE_CHUNKS) {
			ri.Error(ERR_FATAL, "Vulkan: image chunk limit has been reached" );
		}

		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.pNext = NULL;
		alloc_info.allocationSize = vk.image_chunk_size;
		alloc_info.memoryTypeIndex = find_memory_type( memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

		VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &memory ) );

		chunk = &vk_world.image_chunks[vk_world.num_image_chunks];
		chunk->memory = memory;
		chunk->used = memory_requirements.size;

		SET_OBJECT_NAME( memory, va( "image memory chunk %i", vk_world.num_image_chunks ), VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT );

		vk_world.num_image_chunks++;
	}

	VK_CHECK(qvkBindImageMemory(vk.device, image, chunk->memory, chunk->used - memory_requirements.size));
}


static void vk_clean_staging_buffer( void )
{
	if ( vk.staging_buffer.handle != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.staging_buffer.handle, NULL );
		vk.staging_buffer.handle = VK_NULL_HANDLE;
	}

	//if ( vk.staging_buffer.ptr != NULL ) 
	//	qvkUnmapMemory( vk.device, vk.staging_buffer.memory ) {
	//	vk.staging_buffer.ptr = NULL;
	//}

	if ( vk.staging_buffer.memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.staging_buffer.memory, NULL );
		vk.staging_buffer.memory = VK_NULL_HANDLE;
	}

	vk.staging_buffer.ptr = NULL;
	vk.staging_buffer.size = 0;
#ifdef USE_UPLOAD_QUEUE
	vk.staging_buffer.offset = 0;
#endif
}


#ifdef USE_UPLOAD_QUEUE
static qboolean vk_wait_staging_buffer( void )
{
	if ( vk.aux_fence_wait ) {
		VkResult res = qvkWaitForFences( vk.device, 1, &vk.aux_fence, VK_TRUE, 5 * 1000000000ULL );
		if ( res != VK_SUCCESS ) {
			ri.Error( ERR_FATAL, "vkWaitForFences() failed with %s at %s", vk_result_string( res ), __FUNCTION__ );
		}
		qvkResetFences( vk.device, 1, &vk.aux_fence );
		VK_CHECK( qvkResetCommandBuffer( vk.staging_command_buffer, 0 ) );
		vk.staging_buffer.offset = 0; // FIXME: is this correct?
		vk.aux_fence_wait = qfalse;
		return qtrue;
	} else {
		return qfalse;
	}
}


static void vk_flush_staging_buffer( qboolean final )
{
	const VkPipelineStageFlags wait_dst_stage_mask = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	VkSemaphore waits;
	VkSubmitInfo submit_info;
	VkResult res;

	if ( vk.staging_buffer.offset == 0 ) {
		return;
	}

	//ri.Printf( PRINT_WARNING, S_COLOR_CYAN ">>> flush %i bytes (final=%i)<<<\n", (int)vk_world.staging_buffer_offset, final );

	vk.staging_buffer.offset = 0;

	VK_CHECK( qvkEndCommandBuffer( vk.staging_command_buffer ) );

	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = NULL;

	if ( vk.rendering_finished != VK_NULL_HANDLE ) {
		// first call after previous queue submission?
		waits = vk.rendering_finished;
		vk.rendering_finished = VK_NULL_HANDLE;
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &waits;
		submit_info.pWaitDstStageMask = &wait_dst_stage_mask;
	} else {
		submit_info.waitSemaphoreCount = 0;
		submit_info.pWaitSemaphores = NULL;
		submit_info.pWaitDstStageMask = NULL;
	}

	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &vk.staging_command_buffer;

	if ( vk.image_uploaded != VK_NULL_HANDLE ) {
		ri.Error( ERR_FATAL, "Vulkan: incorrect state during image upload" );
	}
	if ( final ) {
		// final submission before recording
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &vk.image_uploaded2;
		vk.image_uploaded = vk.image_uploaded2;
		VK_CHECK( qvkQueueSubmit( vk.queue, 1, &submit_info, vk.aux_fence ) );
		vk.aux_fence_wait = qtrue;
	} else {
		// if submission before another upload then do explicit wait
		submit_info.signalSemaphoreCount = 0;
		submit_info.pSignalSemaphores = NULL;
		VK_CHECK( qvkQueueSubmit( vk.queue, 1, &submit_info, vk.aux_fence ) );
		res = qvkWaitForFences( vk.device, 1, &vk.aux_fence, VK_TRUE, 5 * 1000000000ULL );
		if ( res != VK_SUCCESS ) {
			ri.Error( ERR_FATAL, "vkWaitForFences() failed with %s at %s", vk_result_string( res ), __FUNCTION__ );
		}
		qvkResetFences( vk.device, 1, &vk.aux_fence );
		VK_CHECK( qvkResetCommandBuffer( vk.staging_command_buffer, 0 ) );
	}
}
#endif // USE_UPLOAD_QUEUE


static void vk_alloc_staging_buffer( VkDeviceSize size )
{
	VkBufferCreateInfo buffer_desc;
	VkMemoryRequirements memory_requirements;
	VkMemoryAllocateInfo alloc_info;
	uint32_t memory_type;
	void *data;

	vk_clean_staging_buffer();

	vk.staging_buffer.size = MAX( size, STAGING_BUFFER_SIZE );
	vk.staging_buffer.size = PAD( vk.staging_buffer.size, 1024 * 1024 );

	buffer_desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_desc.pNext = NULL;
	buffer_desc.flags = 0;
	buffer_desc.size = vk.staging_buffer.size;
	buffer_desc.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	buffer_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	buffer_desc.queueFamilyIndexCount = 0;
	buffer_desc.pQueueFamilyIndices = NULL;
	VK_CHECK(qvkCreateBuffer(vk.device, &buffer_desc, NULL, &vk.staging_buffer.handle));

	qvkGetBufferMemoryRequirements( vk.device, vk.staging_buffer.handle, &memory_requirements );

	memory_type = find_memory_type( memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = memory_requirements.size;
	alloc_info.memoryTypeIndex = memory_type;

	VK_CHECK(qvkAllocateMemory(vk.device, &alloc_info, NULL, &vk.staging_buffer.memory));
	VK_CHECK(qvkBindBufferMemory(vk.device, vk.staging_buffer.handle, vk.staging_buffer.memory, 0));

	VK_CHECK(qvkMapMemory(vk.device, vk.staging_buffer.memory, 0, VK_WHOLE_SIZE, 0, &data));
	vk.staging_buffer.ptr = (byte*)data;
#ifdef USE_UPLOAD_QUEUE
	vk.staging_buffer.offset = 0;
#endif
	SET_OBJECT_NAME( vk.staging_buffer.handle, "staging buffer", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	SET_OBJECT_NAME( vk.staging_buffer.memory, "staging buffer memory", VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT );
}


#ifdef USE_VK_VALIDATION
static qboolean vk_validation_error_pending = qfalse;
static char vk_validation_error_message[512];

static const char *vk_debug_report_severity( VkDebugReportFlagsEXT flags ) {
	if ( flags & VK_DEBUG_REPORT_ERROR_BIT_EXT ) {
		return "ERROR";
	}
	if ( flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT ) {
		return "PERFORMANCE WARNING";
	}
	if ( flags & VK_DEBUG_REPORT_WARNING_BIT_EXT ) {
		return "WARNING";
	}
	return "INFO";
}

static void vk_record_validation_error( VkDebugReportFlagsEXT flags, const char *message ) {
	const char *severity = vk_debug_report_severity( flags );
	const char *msg = message ? message : "<no message>";

	Com_sprintf( vk_validation_error_message, sizeof( vk_validation_error_message ), "%s: %s", severity, msg );
	vk_validation_error_pending = qtrue;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT object_type, uint64_t object, size_t location,
	int32_t message_code, const char* layer_prefix, const char* message, void* user_data) {
	if ( flags & (VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) ) {
		vk_record_validation_error( flags, message );
	}
#ifdef _WIN32
	MessageBoxA( 0, message, layer_prefix, MB_ICONWARNING );
	OutputDebugString(message);
	OutputDebugString("\n");
	DebugBreak();
#endif
	return VK_FALSE;
}

qboolean vk_consume_validation_error( char *buffer, size_t bufsize ) {
	if ( !vk_validation_error_pending ) {
		return qfalse;
	}

	Q_strncpyz( buffer, vk_validation_error_message, bufsize );
	vk_validation_error_pending = qfalse;
	return qtrue;
}
#else
qboolean vk_consume_validation_error( char *buffer, size_t bufsize ) {
	(void)buffer;
	(void)bufsize;
	return qfalse;
}
#endif


static qboolean used_instance_extension( const char *ext )
{
	const char *u;

	// allow all VK_*_surface extensions
	u = strrchr( ext, '_' );
	if ( u && Q_stricmp( u + 1, "surface" ) == 0 )
		return qtrue;

	if ( Q_stricmp( ext, VK_KHR_DISPLAY_EXTENSION_NAME ) == 0 )
		return qtrue; // needed for KMSDRM instances/devices?

	if ( Q_stricmp( ext, VK_KHR_SWAPCHAIN_EXTENSION_NAME ) == 0 )
		return qtrue;

#ifdef USE_VK_VALIDATION
	if ( Q_stricmp( ext, VK_EXT_DEBUG_REPORT_EXTENSION_NAME ) == 0 )
		return qtrue;
#endif

	if ( Q_stricmp( ext, VK_EXT_DEBUG_UTILS_EXTENSION_NAME ) == 0 )
		return qtrue;

	if ( Q_stricmp( ext, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME ) == 0 )
		return qtrue;

	if ( Q_stricmp( ext, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME ) == 0 )
		return qtrue;

	return qfalse;
}


static void create_instance( void )
{
#ifdef USE_VK_VALIDATION
	const char* validation_layer_name = "VK_LAYER_LUNARG_standard_validation";
	const char* validation_layer_name2 = "VK_LAYER_KHRONOS_validation";
#endif
	VkInstanceCreateInfo desc;
	VkInstanceCreateFlags flags;
	VkExtensionProperties *extension_properties;
	VkResult res;
	const char **extension_names;
	uint32_t i, n, count, extension_count;
	VkApplicationInfo appInfo;

	flags = 0;
	count = 0;
	extension_count = 0;
	VK_CHECK(qvkEnumerateInstanceExtensionProperties(NULL, &count, NULL));

	extension_properties = (VkExtensionProperties *)ri.Malloc(sizeof(VkExtensionProperties) * count);
	extension_names = (const char**)ri.Malloc(sizeof(char *) * count);

	VK_CHECK( qvkEnumerateInstanceExtensionProperties( NULL, &count, extension_properties ) );
	for ( i = 0; i < count; i++ ) {
		const char *ext = extension_properties[i].extensionName;

		if ( !used_instance_extension( ext ) ) {
			continue;
		}

		// search for duplicates
		for ( n = 0; n < extension_count; n++ ) {
			if ( Q_stricmp( ext, extension_names[ n ] ) == 0 ) {
				break;
			}
		}
		if ( n != extension_count ) {
			continue;
		}

		extension_names[ extension_count++ ] = ext;

		if ( Q_stricmp( ext, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME ) == 0 ) {
			flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
		}

		ri.Printf(PRINT_DEVELOPER, "instance extension: %s\n", ext);
	}

	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pNext = NULL;
	appInfo.pApplicationName = NULL; // Q3_VERSION;
	appInfo.applicationVersion = 0x0;
	appInfo.pEngineName = NULL;
	appInfo.engineVersion = 0x0;
#ifdef _DEBUG
	appInfo.apiVersion = VK_API_VERSION_1_1;
#else
	appInfo.apiVersion = VK_API_VERSION_1_0;
#endif

	// create instance
	desc.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = flags;
	desc.pApplicationInfo = &appInfo;
	desc.enabledExtensionCount = extension_count;
	desc.ppEnabledExtensionNames = extension_names;

#ifdef USE_VK_VALIDATION
	desc.enabledLayerCount = 1;
	desc.ppEnabledLayerNames = &validation_layer_name;

	res = qvkCreateInstance( &desc, NULL, &vk_instance );

	if ( res == VK_ERROR_LAYER_NOT_PRESENT ) {

		desc.enabledLayerCount = 1;
		desc.ppEnabledLayerNames = &validation_layer_name2;

		res = qvkCreateInstance( &desc, NULL, &vk_instance );

		if ( res == VK_ERROR_LAYER_NOT_PRESENT ) {

			ri.Printf( PRINT_WARNING, "...validation layer is not available\n" );

			// try without validation layer
			desc.enabledLayerCount = 0;
			desc.ppEnabledLayerNames = NULL;

			res = qvkCreateInstance( &desc, NULL, &vk_instance );
		}
	}
#else
	desc.enabledLayerCount = 0;
	desc.ppEnabledLayerNames = NULL;

	res = qvkCreateInstance( &desc, NULL, &vk_instance );
#endif

	ri.Free( (void*)extension_names );
	ri.Free( extension_properties );

	if ( res != VK_SUCCESS ) {
		ri.Error( ERR_FATAL, "Vulkan: instance creation failed with %s", vk_result_string( res ) );
	}
}


static VkFormat get_depth_format( VkPhysicalDevice physical_device ) {
	VkFormatProperties props;
	VkFormat formats[2];
	int i;

	if ( glConfig.stencilBits > 0 ) {
		formats[0] = glConfig.depthBits == 16 ? VK_FORMAT_D16_UNORM_S8_UINT : VK_FORMAT_D24_UNORM_S8_UINT;
		formats[1] = VK_FORMAT_D32_SFLOAT_S8_UINT;
	} else {
		formats[0] = glConfig.depthBits == 16 ? VK_FORMAT_D16_UNORM : VK_FORMAT_X8_D24_UNORM_PACK32;
		formats[1] = VK_FORMAT_D32_SFLOAT;
	}

	for ( i = 0; (size_t) i < ARRAY_LEN( formats ); i++ ) {
		qvkGetPhysicalDeviceFormatProperties( physical_device, formats[i], &props );
		if ( ( props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT ) != 0 ) {
			return formats[i];
		}
	}

	ri.Error( ERR_FATAL, "get_depth_format: failed to find depth attachment format" );
	return VK_FORMAT_UNDEFINED; // never get here
}


// Check if we can use vkCmdBlitImage for the given source and destination image formats.
static qboolean vk_blit_enabled( VkPhysicalDevice physical_device, const VkFormat srcFormat, const VkFormat dstFormat )
{
	VkFormatProperties formatProps;

	qvkGetPhysicalDeviceFormatProperties( physical_device, srcFormat, &formatProps );
	if ( ( formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT ) == 0 ) {
		return qfalse;
	}

	qvkGetPhysicalDeviceFormatProperties( physical_device, dstFormat, &formatProps );
	if ( ( formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT ) == 0 ) {
		return qfalse;
	}

	return qtrue;
}


static VkFormat get_hdr_format( VkFormat base_format )
{
	if ( r_fbo->integer == 0 ) {
		return base_format;
	}

	switch ( r_hdr->integer ) {
		case -1: return VK_FORMAT_B4G4R4A4_UNORM_PACK16;
		case 1: return VK_FORMAT_R16G16B16A16_SFLOAT;
		default: return base_format;
	}
}

static qboolean vk_format_has_features( VkPhysicalDevice physical_device, VkFormat format, VkFormatFeatureFlags required )
{
	VkFormatProperties props;
	qvkGetPhysicalDeviceFormatProperties( physical_device, format, &props );
	return ( props.optimalTilingFeatures & required ) == required;
}

static VkFormat get_bloom_format( VkPhysicalDevice physical_device, VkFormat fallback )
{
	const VkFormat preferred[] = {
		VK_FORMAT_A2B10G10R10_UNORM_PACK32,
		VK_FORMAT_A2R10G10B10_UNORM_PACK32,
		VK_FORMAT_B10G11R11_UFLOAT_PACK32
	};
	const VkFormatFeatureFlags required = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
		VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
		VK_FORMAT_FEATURE_BLIT_SRC_BIT |
		VK_FORMAT_FEATURE_BLIT_DST_BIT |
		VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
	uint32_t i;

	for ( i = 0; i < ARRAY_LEN( preferred ); i++ ) {
		const VkFormat fmt = preferred[i];
		if ( fmt == fallback ) {
			return fmt;
		}
		if ( vk_format_has_features( physical_device, fmt, required ) ) {
			return fmt;
		}
	}

	return fallback;
}

typedef struct {
	int bits;
	VkFormat rgb;
	VkFormat bgr;
} present_format_t;

static const present_format_t present_formats[] = {
	//{12, VK_FORMAT_B4G4R4A4_UNORM_PACK16, VK_FORMAT_R4G4B4A4_UNORM_PACK16},
	//{15, VK_FORMAT_B5G5R5A1_UNORM_PACK16, VK_FORMAT_R5G5B5A1_UNORM_PACK16},
	{16, VK_FORMAT_B5G6R5_UNORM_PACK16, VK_FORMAT_R5G6B5_UNORM_PACK16},
	{24, VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_SRGB},
	{30, VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_FORMAT_A2R10G10B10_UNORM_PACK32},
	//{32, VK_FORMAT_B10G11R11_UFLOAT_PACK32, VK_FORMAT_B10G11R11_UFLOAT_PACK32}
};

static void get_present_format( int present_bits, VkFormat *bgr, VkFormat *rgb ) {
	const present_format_t *pf, *sel;
	int i;

	sel = NULL;
	pf = present_formats;
	for ( i = 0; (size_t) i < ARRAY_LEN( present_formats ); i++, pf++ ) {
		if ( pf->bits <= present_bits  ) {
			sel = pf;
		}
	}
	if ( !sel ) {
		*bgr = VK_FORMAT_B8G8R8A8_UNORM;
		*rgb = VK_FORMAT_R8G8B8A8_UNORM;
	} else {
		*bgr = sel->bgr;
		*rgb = sel->rgb;
	}
}

static qboolean vk_format_is_srgb( VkFormat format );

static qboolean vk_select_surface_format( VkPhysicalDevice physical_device, VkSurfaceKHR surface )
{
	VkFormat base_bgr, base_rgb;
	VkFormat ext_bgr, ext_rgb;
	VkSurfaceFormatKHR *candidates;
	uint32_t format_count;
	VkResult res;

	res = qvkGetPhysicalDeviceSurfaceFormatsKHR( physical_device, surface, &format_count, NULL );
	if ( res < 0 ) {
		ri.Printf( PRINT_ERROR, "vkGetPhysicalDeviceSurfaceFormatsKHR returned %s\n", vk_result_string( res ) );
		return qfalse;
	}

	if ( format_count == 0 ) {
		ri.Printf( PRINT_ERROR, "...no surface formats found\n" );
		return qfalse;
	}

	candidates = (VkSurfaceFormatKHR*)ri.Malloc( format_count * sizeof(VkSurfaceFormatKHR) );

	VK_CHECK( qvkGetPhysicalDeviceSurfaceFormatsKHR( physical_device, surface, &format_count, candidates ) );

	get_present_format( 24, &base_bgr, &base_rgb );

	if ( r_fbo->integer ) {
		get_present_format( r_presentBits->integer, &ext_bgr, &ext_rgb );
	} else {
		ext_bgr = base_bgr;
		ext_rgb = base_rgb;
	}

	if ( format_count == 1 && candidates[0].format == VK_FORMAT_UNDEFINED ) {
		// special case that means we can choose any format
		vk.base_format.format = base_bgr;
		vk.base_format.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
		vk.present_format.format = ext_bgr;
		vk.present_format.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	}
	else {
		uint32_t i;
		for ( i = 0; i < format_count; i++ ) {
			if ( ( candidates[i].format == base_bgr || candidates[i].format == base_rgb ) && candidates[i].colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR ) {
				vk.base_format = candidates[i];
				break;
			}
		}
		if ( i == format_count ) {
			vk.base_format = candidates[0];
		}
		for ( i = 0; i < format_count; i++ ) {
			if ( ( candidates[i].format == ext_bgr || candidates[i].format == ext_rgb ) && candidates[i].colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR ) {
				vk.present_format = candidates[i];
				break;
			}
		}
		if ( i == format_count ) {
			vk.present_format = vk.base_format;
		}
	}

	if ( !r_fbo->integer ) {
		vk.present_format = vk.base_format;
	}

	if ( r_vk_swapchain_srgb ) {
		ri.Cvar_Set( "r_vk_swapchain_srgb", vk_format_is_srgb( vk.present_format.format ) ? "1" : "0" );
	}

	ri.Free( candidates );

	return qtrue;
}


static void setup_surface_formats( VkPhysicalDevice physical_device )
{
	vk.depth_format = get_depth_format( physical_device );

	vk.color_format = get_hdr_format( vk.base_format.format );

	vk.capture_format = VK_FORMAT_R8G8B8A8_UNORM;

	// Prefer a higher-precision bloom chain to avoid blocky quantization artifacts
	// around bright highlights when the main color format is 8-bit.
	vk.bloom_format = get_bloom_format( physical_device, vk.color_format );
	vk.ssao_format = VK_FORMAT_R8_UNORM;

	vk.blitEnabled = vk_blit_enabled( physical_device, vk.color_format, vk.capture_format );

	if ( !vk.blitEnabled )
	{
		vk.capture_format = vk.color_format;
	}
}


static const char *renderer_name( const VkPhysicalDeviceProperties *props ) {
	static char buf[sizeof( props->deviceName ) + 64];
	const char *device_type;

	switch ( props->deviceType ) {
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: device_type = "Integrated"; break;
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: device_type = "Discrete"; break;
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: device_type = "Virtual"; break;
		case VK_PHYSICAL_DEVICE_TYPE_CPU: device_type = "CPU"; break;
		default: device_type = "OTHER"; break;
	}

	Com_sprintf( buf, sizeof( buf ), "%s %s, 0x%04x",
		device_type, props->deviceName, props->deviceID );

	return buf;
}


static qboolean vk_create_device( VkPhysicalDevice physical_device, int device_index ) {

#ifdef _DEBUG
	VkPhysicalDeviceTimelineSemaphoreFeatures timeline_semaphore;
	VkPhysicalDeviceVulkanMemoryModelFeatures memory_model;
	VkPhysicalDeviceBufferDeviceAddressFeatures devaddr_features;
	VkPhysicalDevice8BitStorageFeatures storage_8bit_features;
#endif

	ri.Printf( PRINT_ALL, "...selected physical device: %i\n", device_index );

	// select surface format
	if ( !vk_select_surface_format( physical_device, vk_surface ) ) {
		return qfalse;
	}

	setup_surface_formats( physical_device );

	// select queue family
	{
		VkQueueFamilyProperties *queue_families;
		uint32_t queue_family_count;
		uint32_t i;

		qvkGetPhysicalDeviceQueueFamilyProperties( physical_device, &queue_family_count, NULL );
		queue_families = (VkQueueFamilyProperties*)ri.Malloc( queue_family_count * sizeof( VkQueueFamilyProperties ) );
		qvkGetPhysicalDeviceQueueFamilyProperties( physical_device, &queue_family_count, queue_families );

		// select queue family with presentation and graphics support
		vk.queue_family_index = ~0U;
		for (i = 0; i < queue_family_count; i++) {
			VkBool32 presentation_supported;
			VK_CHECK( qvkGetPhysicalDeviceSurfaceSupportKHR( physical_device, i, vk_surface, &presentation_supported ) );

			if (presentation_supported && (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
				vk.queue_family_index = i;
				break;
			}
		}

		ri.Free( queue_families );

		if ( vk.queue_family_index == ~0U ) {
			ri.Printf( PRINT_ERROR, "...failed to find graphics queue family\n" );

			return qfalse;
		}
	}

	// create VkDevice
	{
		const char *device_extension_list[8];
		uint32_t device_extension_count;
		const char *ext, *end;
		char *str;
		const float priority = 1.0;
		VkExtensionProperties *extension_properties;
		VkDeviceQueueCreateInfo queue_desc;
		VkPhysicalDeviceFeatures device_features;
		VkPhysicalDeviceFeatures features;
		VkDeviceCreateInfo device_desc;
		VkResult res;
		qboolean swapchainSupported = qfalse;
		qboolean dedicatedAllocation = qfalse;
		qboolean memoryRequirements2 = qfalse;
		qboolean debugMarker = qfalse;
#ifdef _DEBUG
		qboolean timelineSemaphore = qfalse;
		qboolean memoryModel = qfalse;
		qboolean devAddrFeat = qfalse;
		qboolean storage8bit = qfalse;
		const void** pNextPtr;
#endif
		uint32_t i, len, count = 0;

		VK_CHECK( qvkEnumerateDeviceExtensionProperties( physical_device, NULL, &count, NULL ) );
		extension_properties = (VkExtensionProperties*)ri.Malloc( count * sizeof( VkExtensionProperties ) );
		VK_CHECK( qvkEnumerateDeviceExtensionProperties( physical_device, NULL, &count, extension_properties ) );

		// fill glConfig.extensions_string
		str = glConfig.extensions_string; *str = '\0';
		end = &glConfig.extensions_string[ sizeof( glConfig.extensions_string ) - 1];

		for ( i = 0; i < count; i++ ) {
			ext = extension_properties[i].extensionName;
			if ( strcmp( ext, VK_KHR_SWAPCHAIN_EXTENSION_NAME ) == 0 ) {
				swapchainSupported = qtrue;
			} else if ( strcmp( ext, VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME ) == 0 ) {
				dedicatedAllocation = qtrue;
			} else if ( strcmp( ext, VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME ) == 0 ) {
				memoryRequirements2 = qtrue;
			} else if ( strcmp( ext, VK_EXT_DEBUG_MARKER_EXTENSION_NAME ) == 0 ) {
				debugMarker = qtrue;
#ifdef _DEBUG
			} else if ( strcmp( ext, VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME ) == 0 ) {
				timelineSemaphore = qtrue;
			} else if ( strcmp( ext, VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME ) == 0 ) {
				memoryModel = qtrue;
			} else if ( strcmp( ext, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME ) == 0 ) {
				devAddrFeat = qtrue;
			} else if ( strcmp( ext, VK_KHR_8BIT_STORAGE_EXTENSION_NAME ) == 0 ) {
				storage8bit = qtrue;
#endif
			}
			// add this device extension to glConfig
			if ( i != 0 ) {
				if ( str + 1 >= end )
					continue;
				str = Q_stradd( str, " " );
			}
			len = (uint32_t)strlen( ext );
			if ( str + len >= end )
				continue;
			str = Q_stradd( str, ext );
		}

		ri.Free( extension_properties );

		device_extension_count = 0;

		if ( !swapchainSupported ) {
			ri.Printf( PRINT_ERROR, "...required device extension is not available: %s\n", VK_KHR_SWAPCHAIN_EXTENSION_NAME );
			return qfalse;
		}

		if ( !memoryRequirements2 )
			dedicatedAllocation = qfalse;
		else
			vk.dedicatedAllocation = dedicatedAllocation;

#ifndef USE_DEDICATED_ALLOCATION
		vk.dedicatedAllocation = qfalse;
#endif

		device_extension_list[ device_extension_count++ ] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

		if ( vk.dedicatedAllocation ) {
			device_extension_list[ device_extension_count++ ] = VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME;
			device_extension_list[ device_extension_count++ ] = VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME;
		}

		if ( debugMarker ) {
			device_extension_list[ device_extension_count++ ] = VK_EXT_DEBUG_MARKER_EXTENSION_NAME;
			vk.debugMarkers = qtrue;
		}
#ifdef _DEBUG
		if ( timelineSemaphore ) {
			device_extension_list[ device_extension_count++ ] = VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME;
		}

		if ( memoryModel ) {
			device_extension_list[ device_extension_count++ ] = VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME;
		}

		if ( devAddrFeat ) {
			device_extension_list[ device_extension_count++ ] = VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME;
		}

		if ( storage8bit ) {
			device_extension_list[ device_extension_count++ ] = VK_KHR_8BIT_STORAGE_EXTENSION_NAME;
		}
#endif // _DEBUG
		qvkGetPhysicalDeviceFeatures( physical_device, &device_features );

		if ( device_features.fillModeNonSolid == VK_FALSE ) {
			ri.Printf( PRINT_ERROR, "...fillModeNonSolid feature is not supported\n" );
			return qfalse;
		}

		queue_desc.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_desc.pNext = NULL;
		queue_desc.flags = 0;
		queue_desc.queueFamilyIndex = vk.queue_family_index;
		queue_desc.queueCount = 1;
		queue_desc.pQueuePriorities = &priority;

		Com_Memset( &features, 0, sizeof( features ) );
		features.fillModeNonSolid = VK_TRUE;

#ifdef _DEBUG
		if ( device_features.shaderInt64 ) {
			features.shaderInt64 = VK_TRUE;
		}
#endif
		if ( device_features.wideLines ) { // needed for RB_SurfaceAxis
			features.wideLines = VK_TRUE;
			vk.wideLines = qtrue;
		}

		if ( device_features.fragmentStoresAndAtomics && device_features.vertexPipelineStoresAndAtomics ) {
			features.vertexPipelineStoresAndAtomics = VK_TRUE;
			features.fragmentStoresAndAtomics = VK_TRUE;
			vk.fragmentStores = qtrue;
		}
#ifdef USE_VK_PBR
		if ( device_features.geometryShader )
			features.geometryShader = VK_TRUE;
#endif
		if ( r_ext_texture_filter_anisotropic->integer && device_features.samplerAnisotropy ) {
			features.samplerAnisotropy = VK_TRUE;
			vk.samplerAnisotropy = qtrue;
		}

		device_desc.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		device_desc.pNext = NULL;
		device_desc.flags = 0;
		device_desc.queueCreateInfoCount = 1;
		device_desc.pQueueCreateInfos = &queue_desc;
		device_desc.enabledLayerCount = 0;
		device_desc.ppEnabledLayerNames = NULL;
		device_desc.enabledExtensionCount = device_extension_count;
		device_desc.ppEnabledExtensionNames = device_extension_list;
		device_desc.pEnabledFeatures = &features;

#ifdef _DEBUG
		pNextPtr = (const void **)&device_desc.pNext;

		if ( timelineSemaphore ) {
			*pNextPtr = &timeline_semaphore;
			timeline_semaphore.pNext = NULL;
			timeline_semaphore.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
			timeline_semaphore.timelineSemaphore = VK_TRUE;
			pNextPtr = (const void **)&timeline_semaphore.pNext;
		}

		if ( memoryModel ) {
			*pNextPtr = &memory_model;
			memory_model.pNext = NULL;
			memory_model.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES;
			memory_model.vulkanMemoryModel = VK_TRUE;
			memory_model.vulkanMemoryModelAvailabilityVisibilityChains = VK_FALSE;
			memory_model.vulkanMemoryModelDeviceScope = VK_TRUE;
			pNextPtr = (const void **)&memory_model.pNext;
		}

		if ( devAddrFeat ) {
			*pNextPtr = &devaddr_features;
			devaddr_features.pNext = NULL;
			devaddr_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
			devaddr_features.bufferDeviceAddress = VK_TRUE;
			devaddr_features.bufferDeviceAddressCaptureReplay = VK_FALSE;
			devaddr_features.bufferDeviceAddressMultiDevice = VK_FALSE;
			pNextPtr = (const void **)&devaddr_features.pNext;
		}

		if ( storage8bit ) {
			*pNextPtr = &storage_8bit_features;
			storage_8bit_features.pNext = NULL;
			storage_8bit_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES;
			storage_8bit_features.storageBuffer8BitAccess = VK_TRUE;
			storage_8bit_features.storagePushConstant8 = VK_FALSE;
			storage_8bit_features.uniformAndStorageBuffer8BitAccess = VK_TRUE;
			pNextPtr = (const void **)&storage_8bit_features.pNext;
		}
#endif
		res = qvkCreateDevice( physical_device, &device_desc, NULL, &vk.device );
		if ( res < 0 ) {
			ri.Printf( PRINT_ERROR, "vkCreateDevice returned %s\n", vk_result_string( res ) );
			return qfalse;
		}
	}

	return qtrue;
}


#define INIT_INSTANCE_FUNCTION(func) \
	do { \
		void *_sym = ri.VK_GetInstanceProcAddr( vk_instance, #func ); \
		Com_Memcpy( &q##func, &_sym, sizeof( q##func ) ); \
		if ( q##func == NULL ) { \
			ri.Error( ERR_FATAL, "Failed to find entrypoint %s", #func ); \
		} \
	} while ( 0 );

#define INIT_INSTANCE_FUNCTION_EXT(func) \
	do { \
		void *_sym = ri.VK_GetInstanceProcAddr( vk_instance, #func ); \
		Com_Memcpy( &q##func, &_sym, sizeof( q##func ) ); \
	} while ( 0 );


#define INIT_DEVICE_FUNCTION(func) \
	do { \
		PFN_vkVoidFunction _sym = qvkGetDeviceProcAddr( vk.device, #func ); \
		Com_Memcpy( &q##func, &_sym, sizeof( q##func ) ); \
		if ( q##func == NULL ) { \
			ri.Error( ERR_FATAL, "Failed to find entrypoint %s", #func ); \
		} \
	} while ( 0 );

#define INIT_DEVICE_FUNCTION_EXT(func) \
	do { \
		PFN_vkVoidFunction _sym = qvkGetDeviceProcAddr( vk.device, #func ); \
		Com_Memcpy( &q##func, &_sym, sizeof( q##func ) ); \
	} while ( 0 );


static void vk_destroy_instance( void ) {
	if ( vk_surface != VK_NULL_HANDLE ) {
		if ( qvkDestroySurfaceKHR != NULL ) {
			qvkDestroySurfaceKHR( vk_instance, vk_surface, NULL );
		}
		vk_surface = VK_NULL_HANDLE;
	}

#ifdef USE_VK_VALIDATION
	if ( vk_debug_callback ) {
		if ( qvkDestroyDebugReportCallbackEXT != NULL ) {
			qvkDestroyDebugReportCallbackEXT( vk_instance, vk_debug_callback, NULL );
		}
		vk_debug_callback = VK_NULL_HANDLE;
	}
#endif

	if ( vk_instance != VK_NULL_HANDLE ) {
		if ( qvkDestroyInstance ) {
			qvkDestroyInstance( vk_instance, NULL );
		}
		vk_instance = VK_NULL_HANDLE;
	}
}


static void init_vulkan_library( void )
{
	VkPhysicalDeviceProperties props;
	VkPhysicalDevice *physical_devices;
	uint32_t device_count;
	int device_index, i;
	VkResult res;

	Com_Memset( &vk, 0, sizeof( vk ) );

	if ( vk_instance == VK_NULL_HANDLE ) {

		// force cleanup
		vk_destroy_instance();

		// Get functions that do not depend on VkInstance (vk_instance == nullptr at this point).
		INIT_INSTANCE_FUNCTION( vkCreateInstance )
		INIT_INSTANCE_FUNCTION( vkEnumerateInstanceExtensionProperties )

		// Get instance level functions.
		create_instance();

		INIT_INSTANCE_FUNCTION( vkCreateDevice )
		INIT_INSTANCE_FUNCTION( vkDestroyInstance )
		INIT_INSTANCE_FUNCTION( vkEnumerateDeviceExtensionProperties )
		INIT_INSTANCE_FUNCTION( vkEnumeratePhysicalDevices )
		INIT_INSTANCE_FUNCTION( vkGetDeviceProcAddr )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceFeatures )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceFormatProperties )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceMemoryProperties )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceProperties )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceQueueFamilyProperties )
		INIT_INSTANCE_FUNCTION( vkDestroySurfaceKHR )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceSurfaceCapabilitiesKHR )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceSurfaceFormatsKHR )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceSurfacePresentModesKHR )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceSurfaceSupportKHR )

#ifdef USE_VK_VALIDATION
		INIT_INSTANCE_FUNCTION_EXT( vkCreateDebugReportCallbackEXT )
		INIT_INSTANCE_FUNCTION_EXT( vkDestroyDebugReportCallbackEXT )

		// Create debug callback.
		if ( qvkCreateDebugReportCallbackEXT && qvkDestroyDebugReportCallbackEXT ) {
			VkDebugReportCallbackCreateInfoEXT desc;
			desc.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
			desc.pNext = NULL;
			desc.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT |
				VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
				VK_DEBUG_REPORT_ERROR_BIT_EXT;
			desc.pfnCallback = &debug_callback;
			desc.pUserData = NULL;

			VK_CHECK( qvkCreateDebugReportCallbackEXT( vk_instance, &desc, NULL, &vk_debug_callback ) );
		}
#endif

		// create surface
		if ( !ri.VK_CreateSurface( vk_instance, &vk_surface ) ) {
			ri.Error( ERR_FATAL, "Error creating Vulkan surface" );
			return;
		}
	} // vk_instance == VK_NULL_HANDLE

	res = qvkEnumeratePhysicalDevices( vk_instance, &device_count, NULL );
	if ( device_count == 0 ) {
		ri.Error( ERR_FATAL, "Vulkan: no physical devices found" );
		return;
	}
	else if ( res < 0 ) {
		ri.Error( ERR_FATAL, "vkEnumeratePhysicalDevices returned %s", vk_result_string( res ) );
		return;
	}

	physical_devices = (VkPhysicalDevice*)ri.Malloc( device_count * sizeof( VkPhysicalDevice ) );
	VK_CHECK( qvkEnumeratePhysicalDevices( vk_instance, &device_count, physical_devices ) );

	// initial physical device index
	device_index = r_device->integer;

	ri.Printf( PRINT_ALL, ".......................\nAvailable physical devices:\n" );
	for ( i = 0; (uint32_t) i < device_count; i++ ) {
		qvkGetPhysicalDeviceProperties( physical_devices[ i ], &props );
		ri.Printf( PRINT_ALL, " %i: %s\n", i, renderer_name( &props ) );
		if ( device_index == -1 && props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ) {
			device_index = i;
		} else if ( device_index == -2 && props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ) {
			device_index = i;
		}
	}
	ri.Printf( PRINT_ALL, ".......................\n" );

	vk.physical_device = VK_NULL_HANDLE;
	for ( i = 0; (uint32_t) i < device_count; i++, device_index++ ) {
		if ( device_index < 0 || (uint32_t)device_index >= device_count ) {
			device_index = 0;
		}
		if ( vk_create_device( physical_devices[ device_index ], device_index ) ) {
			vk.physical_device = physical_devices[ device_index ];
			break;
		}
	}

	ri.Free( physical_devices );

	if ( vk.physical_device == VK_NULL_HANDLE ) {
		ri.Error( ERR_FATAL, "Vulkan: unable to find any suitable physical device" );
		return;
	}

	//
	// Get device level functions.
	//
	INIT_DEVICE_FUNCTION(vkAllocateCommandBuffers)
	INIT_DEVICE_FUNCTION(vkAllocateDescriptorSets)
	INIT_DEVICE_FUNCTION(vkAllocateMemory)
	INIT_DEVICE_FUNCTION(vkBeginCommandBuffer)
	INIT_DEVICE_FUNCTION(vkBindBufferMemory)
	INIT_DEVICE_FUNCTION(vkBindImageMemory)
	INIT_DEVICE_FUNCTION(vkCmdBeginRenderPass)
	INIT_DEVICE_FUNCTION(vkCmdBindDescriptorSets)
	INIT_DEVICE_FUNCTION(vkCmdBindIndexBuffer)
	INIT_DEVICE_FUNCTION(vkCmdBindPipeline)
	INIT_DEVICE_FUNCTION(vkCmdBindVertexBuffers)
	INIT_DEVICE_FUNCTION(vkCmdBlitImage)
	INIT_DEVICE_FUNCTION(vkCmdClearAttachments)
	INIT_DEVICE_FUNCTION(vkCmdCopyBuffer)
	INIT_DEVICE_FUNCTION(vkCmdCopyBufferToImage)
	INIT_DEVICE_FUNCTION(vkCmdCopyImage)
	INIT_DEVICE_FUNCTION(vkCmdCopyImageToBuffer)
	INIT_DEVICE_FUNCTION(vkCmdDraw)
	INIT_DEVICE_FUNCTION(vkCmdDrawIndexed)
	INIT_DEVICE_FUNCTION(vkCmdDispatch)
	INIT_DEVICE_FUNCTION(vkCmdEndRenderPass)
	INIT_DEVICE_FUNCTION(vkCmdNextSubpass)
	INIT_DEVICE_FUNCTION(vkCmdPipelineBarrier)
	INIT_DEVICE_FUNCTION(vkCmdPushConstants)
	INIT_DEVICE_FUNCTION(vkCmdSetDepthBias)
	INIT_DEVICE_FUNCTION(vkCmdSetScissor)
	INIT_DEVICE_FUNCTION(vkCmdSetViewport)
	INIT_DEVICE_FUNCTION(vkCmdWriteTimestamp)
	INIT_DEVICE_FUNCTION_EXT(vkCmdResetQueryPool)
	INIT_DEVICE_FUNCTION(vkCreateBuffer)
	INIT_DEVICE_FUNCTION(vkCreateCommandPool)
	INIT_DEVICE_FUNCTION(vkCreateDescriptorPool)
	INIT_DEVICE_FUNCTION(vkCreateDescriptorSetLayout)
	INIT_DEVICE_FUNCTION(vkCreateFence)
	INIT_DEVICE_FUNCTION(vkCreateFramebuffer)
	INIT_DEVICE_FUNCTION(vkCreateComputePipelines)
	INIT_DEVICE_FUNCTION(vkCreateGraphicsPipelines)
	INIT_DEVICE_FUNCTION(vkCreateImage)
	INIT_DEVICE_FUNCTION(vkCreateImageView)
	INIT_DEVICE_FUNCTION(vkCreatePipelineCache)
	INIT_DEVICE_FUNCTION(vkCreatePipelineLayout)
	INIT_DEVICE_FUNCTION(vkCreateQueryPool)
	INIT_DEVICE_FUNCTION(vkCreateRenderPass)
	INIT_DEVICE_FUNCTION(vkCreateSampler)
	INIT_DEVICE_FUNCTION(vkCreateSemaphore)
	INIT_DEVICE_FUNCTION(vkCreateShaderModule)
	INIT_DEVICE_FUNCTION(vkDestroyBuffer)
	INIT_DEVICE_FUNCTION(vkDestroyCommandPool)
	INIT_DEVICE_FUNCTION(vkDestroyDescriptorPool)
	INIT_DEVICE_FUNCTION(vkDestroyDescriptorSetLayout)
	INIT_DEVICE_FUNCTION(vkDestroyDevice)
	INIT_DEVICE_FUNCTION(vkDestroyFence)
	INIT_DEVICE_FUNCTION(vkDestroyFramebuffer)
	INIT_DEVICE_FUNCTION(vkDestroyImage)
	INIT_DEVICE_FUNCTION(vkDestroyImageView)
	INIT_DEVICE_FUNCTION(vkDestroyPipeline)
	INIT_DEVICE_FUNCTION(vkDestroyPipelineCache)
	INIT_DEVICE_FUNCTION(vkDestroyPipelineLayout)
	INIT_DEVICE_FUNCTION(vkDestroyQueryPool)
	INIT_DEVICE_FUNCTION(vkDestroyRenderPass)
	INIT_DEVICE_FUNCTION(vkDestroySampler)
	INIT_DEVICE_FUNCTION(vkDestroySemaphore)
	INIT_DEVICE_FUNCTION(vkDestroyShaderModule)
	INIT_DEVICE_FUNCTION(vkDeviceWaitIdle)
	INIT_DEVICE_FUNCTION(vkEndCommandBuffer)
	INIT_DEVICE_FUNCTION(vkFlushMappedMemoryRanges)
	INIT_DEVICE_FUNCTION(vkFreeCommandBuffers)
	INIT_DEVICE_FUNCTION(vkFreeDescriptorSets)
	INIT_DEVICE_FUNCTION(vkFreeMemory)
	INIT_DEVICE_FUNCTION(vkGetBufferMemoryRequirements)
	INIT_DEVICE_FUNCTION(vkGetDeviceQueue)
	INIT_DEVICE_FUNCTION(vkGetImageMemoryRequirements)
	INIT_DEVICE_FUNCTION(vkGetImageSubresourceLayout)
	INIT_DEVICE_FUNCTION(vkInvalidateMappedMemoryRanges)
	INIT_DEVICE_FUNCTION(vkMapMemory)
	INIT_DEVICE_FUNCTION(vkQueueSubmit)
	INIT_DEVICE_FUNCTION(vkQueueWaitIdle)
	INIT_DEVICE_FUNCTION(vkResetCommandBuffer)
	INIT_DEVICE_FUNCTION(vkResetDescriptorPool)
	INIT_DEVICE_FUNCTION(vkResetFences)
	INIT_DEVICE_FUNCTION(vkGetQueryPoolResults)
	INIT_DEVICE_FUNCTION(vkUnmapMemory)
	INIT_DEVICE_FUNCTION(vkUpdateDescriptorSets)
	INIT_DEVICE_FUNCTION(vkWaitForFences)
	INIT_DEVICE_FUNCTION(vkAcquireNextImageKHR)
	INIT_DEVICE_FUNCTION(vkCreateSwapchainKHR)
	INIT_DEVICE_FUNCTION(vkDestroySwapchainKHR)
	INIT_DEVICE_FUNCTION(vkGetSwapchainImagesKHR)
	INIT_DEVICE_FUNCTION(vkQueuePresentKHR)

	if ( vk.dedicatedAllocation ) {
		INIT_DEVICE_FUNCTION_EXT(vkGetBufferMemoryRequirements2KHR);
		INIT_DEVICE_FUNCTION_EXT(vkGetImageMemoryRequirements2KHR);
		if ( !qvkGetBufferMemoryRequirements2KHR || !qvkGetImageMemoryRequirements2KHR ) {
			vk.dedicatedAllocation = qfalse;
		}
	}

	if ( vk.debugMarkers ) {
		INIT_DEVICE_FUNCTION_EXT(vkDebugMarkerSetObjectNameEXT)
	}

	INIT_DEVICE_FUNCTION_EXT(vkCmdClearColorImage)
}

#undef INIT_INSTANCE_FUNCTION
#undef INIT_DEVICE_FUNCTION
#undef INIT_DEVICE_FUNCTION_EXT

static void deinit_instance_functions( void )
{
	qvkCreateInstance = NULL;
	qvkEnumerateInstanceExtensionProperties = NULL;

	// instance functions:
	qvkCreateDevice = NULL;
	qvkDestroyInstance = NULL;
	qvkEnumerateDeviceExtensionProperties = NULL;
	qvkEnumeratePhysicalDevices = NULL;
	qvkGetDeviceProcAddr = NULL;
	qvkGetPhysicalDeviceFeatures = NULL;
	qvkGetPhysicalDeviceFormatProperties = NULL;
	qvkGetPhysicalDeviceMemoryProperties = NULL;
	qvkGetPhysicalDeviceProperties = NULL;
	qvkGetPhysicalDeviceQueueFamilyProperties = NULL;
	qvkDestroySurfaceKHR = NULL;
	qvkGetPhysicalDeviceSurfaceCapabilitiesKHR = NULL;
	qvkGetPhysicalDeviceSurfaceFormatsKHR = NULL;
	qvkGetPhysicalDeviceSurfacePresentModesKHR = NULL;
	qvkGetPhysicalDeviceSurfaceSupportKHR = NULL;
#ifdef USE_VK_VALIDATION
	qvkCreateDebugReportCallbackEXT = NULL;
	qvkDestroyDebugReportCallbackEXT = NULL;
#endif
}


static void deinit_device_functions( void )
{
	// device functions:
	qvkAllocateCommandBuffers					= NULL;
	qvkAllocateDescriptorSets					= NULL;
	qvkAllocateMemory							= NULL;
	qvkBeginCommandBuffer						= NULL;
	qvkBindBufferMemory							= NULL;
	qvkBindImageMemory							= NULL;
	qvkCmdBeginRenderPass						= NULL;
	qvkCmdBindDescriptorSets					= NULL;
	qvkCmdBindIndexBuffer						= NULL;
	qvkCmdBindPipeline							= NULL;
	qvkCmdBindVertexBuffers						= NULL;
	qvkCmdBlitImage								= NULL;
	qvkCmdClearAttachments						= NULL;
	qvkCmdCopyBuffer							= NULL;
	qvkCmdCopyBufferToImage						= NULL;
	qvkCmdCopyImage								= NULL;
	qvkCmdCopyImageToBuffer						= NULL;
	qvkCmdDraw									= NULL;
	qvkCmdDrawIndexed							= NULL;
	qvkCmdDispatch								= NULL;
	qvkCmdEndRenderPass							= NULL;
	qvkCmdNextSubpass							= NULL;
	qvkCmdPipelineBarrier						= NULL;
	qvkCmdPushConstants							= NULL;
	qvkCmdSetDepthBias							= NULL;
	qvkCmdSetScissor							= NULL;
	qvkCmdSetViewport							= NULL;
	qvkCmdWriteTimestamp						= NULL;
	qvkCmdResetQueryPool						= NULL;
	qvkCreateBuffer								= NULL;
	qvkCreateCommandPool						= NULL;
	qvkCreateDescriptorPool						= NULL;
	qvkCreateDescriptorSetLayout				= NULL;
	qvkCreateFence								= NULL;
	qvkCreateFramebuffer						= NULL;
	qvkCreateComputePipelines					= NULL;
	qvkCreateGraphicsPipelines					= NULL;
	qvkCreateImage								= NULL;
	qvkCreateImageView							= NULL;
	qvkCreatePipelineCache						= NULL;
	qvkCreatePipelineLayout						= NULL;
	qvkCreateQueryPool							= NULL;
	qvkCreateRenderPass							= NULL;
	qvkCreateSampler							= NULL;
	qvkCreateSemaphore							= NULL;
	qvkCreateShaderModule						= NULL;
	qvkDestroyBuffer							= NULL;
	qvkDestroyCommandPool						= NULL;
	qvkDestroyDescriptorPool					= NULL;
	qvkDestroyDescriptorSetLayout				= NULL;
	qvkDestroyDevice							= NULL;
	qvkDestroyFence								= NULL;
	qvkDestroyFramebuffer						= NULL;
	qvkDestroyImage								= NULL;
	qvkDestroyImageView							= NULL;
	qvkDestroyPipeline							= NULL;
	qvkDestroyPipelineCache						= NULL;
	qvkDestroyPipelineLayout					= NULL;
	qvkDestroyQueryPool							= NULL;
	qvkDestroyRenderPass						= NULL;
	qvkDestroySampler							= NULL;
	qvkDestroySemaphore							= NULL;
	qvkDestroyShaderModule						= NULL;
	qvkDeviceWaitIdle							= NULL;
	qvkEndCommandBuffer							= NULL;
	qvkFlushMappedMemoryRanges					= NULL;
	qvkFreeCommandBuffers						= NULL;
	qvkFreeDescriptorSets						= NULL;
	qvkFreeMemory								= NULL;
	qvkGetBufferMemoryRequirements				= NULL;
	qvkGetDeviceQueue							= NULL;
	qvkGetImageMemoryRequirements				= NULL;
	qvkGetImageSubresourceLayout				= NULL;
	qvkInvalidateMappedMemoryRanges				= NULL;
	qvkMapMemory								= NULL;
	qvkQueueSubmit								= NULL;
	qvkQueueWaitIdle							= NULL;
	qvkResetCommandBuffer						= NULL;
	qvkResetDescriptorPool						= NULL;
	qvkResetFences								= NULL;
	qvkGetQueryPoolResults						= NULL;
	qvkUnmapMemory								= NULL;
	qvkUpdateDescriptorSets						= NULL;
	qvkWaitForFences							= NULL;
	qvkAcquireNextImageKHR						= NULL;
	qvkCreateSwapchainKHR						= NULL;
	qvkDestroySwapchainKHR						= NULL;
	qvkGetSwapchainImagesKHR					= NULL;
	qvkQueuePresentKHR							= NULL;

	qvkGetBufferMemoryRequirements2KHR			= NULL;
	qvkGetImageMemoryRequirements2KHR			= NULL;

	qvkDebugMarkerSetObjectNameEXT				= NULL;
	qvkCmdClearColorImage						= NULL;
}


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


static void vk_create_layout_binding( int binding, VkDescriptorType type, VkShaderStageFlags flags, VkDescriptorSetLayout *layout )
{
	uint32_t count = 0;
	VkDescriptorSetLayoutBinding bind[2];
	VkDescriptorSetLayoutCreateInfo desc;

	bind[count].binding = binding;
	bind[count].descriptorType = type;
	bind[count].descriptorCount = 1;
	bind[count].stageFlags = flags;
	bind[count].pImmutableSamplers = NULL;
	count++;

	if ( *layout == vk.set_layout_uniform ) {
		bind[count].binding = VK_DESC_UNIFORM_CAMERA_BINDING;
		bind[count].descriptorType = type;
		bind[count].descriptorCount = 1;
		bind[count].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		bind[count].pImmutableSamplers = NULL;
		count++;    
	}

	desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.bindingCount = count;
	desc.pBindings = bind;

	VK_CHECK( qvkCreateDescriptorSetLayout(vk.device, &desc, NULL, layout ) );
}

static void vk_write_uniform_descriptor( VkWriteDescriptorSet *desc, VkDescriptorBufferInfo *info, 
	VkBuffer buffer, VkDescriptorSet descriptor, const uint32_t binding, const size_t size )
{
	info[binding].buffer = buffer;
	info[binding].offset = 0;
	info[binding].range = size;

	desc[binding].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	desc[binding].dstSet = descriptor;
	desc[binding].dstBinding = binding;
	desc[binding].dstArrayElement = 0;
	desc[binding].descriptorCount = 1;
	desc[binding].pNext = NULL;
	desc[binding].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	desc[binding].pImageInfo = NULL;
	desc[binding].pBufferInfo = &info[binding];
	desc[binding].pTexelBufferView = NULL;
}

static void vk_update_uniform_descriptor( VkDescriptorSet descriptor, VkBuffer buffer )
{
	VkDescriptorBufferInfo info[VK_DESC_UNIFORM_COUNT];
	VkWriteDescriptorSet desc[VK_DESC_UNIFORM_COUNT];

	vk_write_uniform_descriptor( desc, info, buffer, descriptor, VK_DESC_UNIFORM_MAIN_BINDING, sizeof(vkUniform_t) );
	vk_write_uniform_descriptor( desc, info, buffer, descriptor, VK_DESC_UNIFORM_CAMERA_BINDING, sizeof(vkUniformCamera_t) );

	qvkUpdateDescriptorSets(vk.device, VK_DESC_UNIFORM_COUNT, desc, 0, NULL);
}


static VkSampler vk_find_sampler( const Vk_Sampler_Def *def ) {
	VkSamplerAddressMode address_mode;
	VkSamplerCreateInfo desc;
	VkSampler sampler;
	VkFilter mag_filter;
	VkFilter min_filter;
	VkSamplerMipmapMode mipmap_mode;
	float maxLod;
	int i;

	// Look for sampler among existing samplers.
	for ( i = 0; i < vk.samplers.count; i++ ) {
		const Vk_Sampler_Def *cur_def = &vk.samplers.def[i];
		if ( memcmp( cur_def, def, sizeof( *def ) ) == 0 ) {
			return vk.samplers.handle[i];
		}
	}

	// Create new sampler.
	if ( vk.samplers.count >= MAX_VK_SAMPLERS ) {
		ri.Error( ERR_DROP, "vk_find_sampler: MAX_VK_SAMPLERS hit\n" );
		// return VK_NULL_HANDLE;
	}

	address_mode = def->address_mode;

	if (def->gl_mag_filter == GL_NEAREST) {
		mag_filter = VK_FILTER_NEAREST;
	} else if (def->gl_mag_filter == GL_LINEAR) {
		mag_filter = VK_FILTER_LINEAR;
	} else {
		ri.Error(ERR_FATAL, "vk_find_sampler: invalid gl_mag_filter");
		return VK_NULL_HANDLE;
	}

	maxLod = vk.maxLod;

	if (def->gl_min_filter == GL_NEAREST) {
		min_filter = VK_FILTER_NEAREST;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		maxLod = 0.25f; // used to emulate OpenGL's GL_LINEAR/GL_NEAREST minification filter
	} else if (def->gl_min_filter == GL_LINEAR) {
		min_filter = VK_FILTER_LINEAR;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		maxLod = 0.25f; // used to emulate OpenGL's GL_LINEAR/GL_NEAREST minification filter
	} else if (def->gl_min_filter == GL_NEAREST_MIPMAP_NEAREST) {
		min_filter = VK_FILTER_NEAREST;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	} else if (def->gl_min_filter == GL_LINEAR_MIPMAP_NEAREST) {
		min_filter = VK_FILTER_LINEAR;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	} else if (def->gl_min_filter == GL_NEAREST_MIPMAP_LINEAR) {
		min_filter = VK_FILTER_NEAREST;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	} else if (def->gl_min_filter == GL_LINEAR_MIPMAP_LINEAR) {
		min_filter = VK_FILTER_LINEAR;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	} else {
		ri.Error(ERR_FATAL, "vk_find_sampler: invalid gl_min_filter");
		return VK_NULL_HANDLE;
	}

	if ( def->max_lod_1_0 ) {
		maxLod = 1.0f;
	}

	desc.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.magFilter = mag_filter;
	desc.minFilter = min_filter;
	desc.mipmapMode = mipmap_mode;
	desc.addressModeU = address_mode;
	desc.addressModeV = address_mode;
	desc.addressModeW = address_mode;
	desc.mipLodBias = r_mipLodBias->value;

	if ( def->noAnisotropy || mipmap_mode == VK_SAMPLER_MIPMAP_MODE_NEAREST || mag_filter == VK_FILTER_NEAREST ) {
		desc.anisotropyEnable = VK_FALSE;
		desc.maxAnisotropy = 1.0f;
	} else {
		desc.anisotropyEnable = (r_ext_texture_filter_anisotropic->integer && vk.samplerAnisotropy) ? VK_TRUE : VK_FALSE;
		if ( desc.anisotropyEnable ) {
			desc.maxAnisotropy = MIN( r_ext_max_anisotropy->integer, vk.maxAnisotropy );
		}
	}

	desc.compareEnable = VK_FALSE;
	desc.compareOp = VK_COMPARE_OP_ALWAYS;
	desc.minLod = 0.0f;
	desc.maxLod = (maxLod == vk.maxLod) ? VK_LOD_CLAMP_NONE : maxLod;
	desc.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	desc.unnormalizedCoordinates = VK_FALSE;

	VK_CHECK( qvkCreateSampler( vk.device, &desc, NULL, &sampler ) );

	SET_OBJECT_NAME( sampler, va( "image sampler %i", vk.samplers.count ), VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT );

	vk.samplers.def[ vk.samplers.count ] = *def;
	vk.samplers.handle[ vk.samplers.count ] = sampler;
	vk.samplers.count++;

	return sampler;
}


void vk_destroy_samplers( void )
{
	int i;

	for ( i = 0; i < vk.samplers.count; i++ ) {
		qvkDestroySampler( vk.device, vk.samplers.handle[i], NULL );
		memset( &vk.samplers.def[i], 0x0, sizeof( vk.samplers.def[i] ) );
		vk.samplers.handle[i] = VK_NULL_HANDLE;
	}

	vk.samplers.count = 0;
}


static void vk_update_color_descriptor_image( VkImageView color_view )
{
	if ( vk.color_descriptor == VK_NULL_HANDLE || color_view == VK_NULL_HANDLE ) {
		return;
	}

	VkDescriptorImageInfo info;
	VkWriteDescriptorSet desc;
	Vk_Sampler_Def sd;

	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.max_lod_1_0 = qtrue;
	sd.noAnisotropy = qtrue;

	info.sampler = vk_find_sampler( &sd );
	info.imageView = color_view;
	info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	desc.dstSet = vk.color_descriptor;
	desc.dstBinding = 0;
	desc.dstArrayElement = 0;
	desc.descriptorCount = 1;
	desc.pNext = NULL;
	desc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	desc.pImageInfo = &info;
	desc.pBufferInfo = NULL;
	desc.pTexelBufferView = NULL;

	qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
}



void vk_update_attachment_descriptors( void ) {

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
		desc.dstSet = vk.color_descriptor;
		desc.dstBinding = 0;
		desc.dstArrayElement = 0;
		desc.descriptorCount = 1;
		desc.pNext = NULL;
		desc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		desc.pImageInfo = &info;
		desc.pBufferInfo = NULL;
		desc.pTexelBufferView = NULL;

		qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

		// screenmap
		sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
		sd.max_lod_1_0 = qfalse;
		sd.noAnisotropy = qtrue;

		info.sampler = vk_find_sampler( &sd );

		info.imageView = vk.screenMap.color_image_view;
		desc.dstSet = vk.screenMap.color_descriptor;

		qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

		if ( r_ssao && r_ssao->integer )
		{
			// depth sampling for SSAO
			sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
			sd.max_lod_1_0 = qtrue;
			sd.noAnisotropy = qtrue;
			info.sampler = vk_find_sampler( &sd );
			info.imageView = vk.depth_image_view;
			info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			desc.dstSet = vk.depth_descriptor;
			qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

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
		}

		// bloom images
		if ( r_bloom->integer )
		{
			uint32_t i;

			sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
			sd.max_lod_1_0 = qtrue;
			sd.noAnisotropy = qtrue;
			info.sampler = vk_find_sampler( &sd );
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			for ( i = 0; i < ARRAY_LEN( vk.bloom_image_descriptor ); i++ )
			{
				info.imageView = vk.bloom_image_view[i];
				desc.dstSet = vk.bloom_image_descriptor[i];

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

	volumetric_depth_view = vk.depth_image_view;
	volumetric_depth_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	if ( vk.msaaActive && vk.volumetric_depth_view != VK_NULL_HANDLE ) {
		volumetric_depth_view = vk.volumetric_depth_view;
		volumetric_depth_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}

	if ( vk.volumetric_compute_descriptor != VK_NULL_HANDLE &&
		vk.froxel_volume_view && vk.froxel_history_view && vk.froxel_light_view &&
		vk.froxel_extinction_view && vk.froxel_clamp_view &&
		volumetric_depth_view && vk.fog_noise_view && vk.sun_shadow_view &&
		vk.local_spot_shadow_atlas_view && vk.local_point_shadow_array_view && vk.motion_vector_view &&
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
		shadow_info.imageView = vk.sun_shadow_view;
		shadow_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

		Com_Memset( &local_spot_shadow_info, 0, sizeof( local_spot_shadow_info ) );
		local_spot_shadow_info.sampler = vk.sun_shadow_sampler;
		local_spot_shadow_info.imageView = vk.local_spot_shadow_atlas_view;
		local_spot_shadow_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

		Com_Memset( &local_point_shadow_info, 0, sizeof( local_point_shadow_info ) );
		local_point_shadow_info.sampler = vk.sun_shadow_sampler;
		local_point_shadow_info.imageView = vk.local_point_shadow_array_view;
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
		vk.motion_vector_view && vk.local_spot_shadow_atlas_view && vk.local_point_shadow_array_view &&
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
		composite_info[5].imageView = vk.local_spot_shadow_atlas_view;
		composite_info[5].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

		// localPointShadowMap (binding 7)
		composite_info[6].sampler = vk_find_sampler( &shadow_sd );
		composite_info[6].imageView = vk.local_point_shadow_array_view;
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
		resolve_info[0].imageView = vk.depth_image_view;
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
			(unsigned long long)(uintptr_t)vk.sun_shadow_view,
			(unsigned long long)(uintptr_t)vk.local_spot_shadow_atlas_view,
			(unsigned long long)(uintptr_t)vk.local_point_shadow_array_view,
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

		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.color_descriptor ) );

		if ( r_ssao && r_ssao->integer ) {
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.depth_descriptor ) );
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.ssao_descriptor ) );
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.ssao_blur_descriptor ) );
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

		alloc.descriptorSetCount = 1;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.screenMap.color_descriptor ) ); // screenmap

#ifdef VK_PBR_BRDFLUT
		if( vk.pbrActive )
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.brdflut_image_descriptor ) );
#endif

		// cubemap
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.cubeMap.color_descriptor ) );

		alloc.pSetLayouts = &vk.volumetric_compute_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.volumetric_compute_descriptor ) );

		alloc.pSetLayouts = &vk.volumetric_composite_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.volumetric_composite_descriptor ) );

			if ( vk.volumetric_depth_resolve_layout != VK_NULL_HANDLE ) {
				alloc.pSetLayouts = &vk.volumetric_depth_resolve_layout;
				VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.volumetric_depth_resolve_descriptor ) );
			}
			if ( vk.volumetric_fluid_layout != VK_NULL_HANDLE ) {
				alloc.pSetLayouts = &vk.volumetric_fluid_layout;
				VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.volumetric_fluid_descriptor ) );
			}

			vk_update_attachment_descriptors();
			vk_update_volumetric_descriptors();
		}
	}


static void vk_release_geometry_buffers( void )
{
	int i;

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		qvkDestroyBuffer( vk.device, vk.tess[i].vertex_buffer, NULL );
		vk.tess[i].vertex_buffer = VK_NULL_HANDLE;
	}

	qvkFreeMemory( vk.device, vk.geometry_buffer_memory, NULL );
	vk.geometry_buffer_memory = VK_NULL_HANDLE;
}


static void vk_create_geometry_buffers( VkDeviceSize size )
{
	VkMemoryRequirements vb_memory_requirements;
	VkMemoryAllocateInfo alloc_info;
	VkBufferCreateInfo desc;
	VkDeviceSize vertex_buffer_offset;
	uint32_t memory_type_bits;
	uint32_t memory_type;
	void *data;
	int i;

	desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = NULL;

	Com_Memset( &vb_memory_requirements, 0, sizeof( vb_memory_requirements ) );

	for ( i = 0 ; i < NUM_COMMAND_BUFFERS; i++ ) {
		desc.size = size;
		desc.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		VK_CHECK( qvkCreateBuffer( vk.device, &desc, NULL, &vk.tess[i].vertex_buffer ) );

		qvkGetBufferMemoryRequirements( vk.device, vk.tess[i].vertex_buffer, &vb_memory_requirements );
	}

	memory_type_bits = vb_memory_requirements.memoryTypeBits;
	memory_type = find_memory_type( memory_type_bits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = vb_memory_requirements.size * NUM_COMMAND_BUFFERS;
	alloc_info.memoryTypeIndex = memory_type;

	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.geometry_buffer_memory ) );
	VK_CHECK( qvkMapMemory( vk.device, vk.geometry_buffer_memory, 0, VK_WHOLE_SIZE, 0, &data ) );

	vertex_buffer_offset = 0;

	for ( i = 0 ; i < NUM_COMMAND_BUFFERS; i++ ) {
		qvkBindBufferMemory( vk.device, vk.tess[i].vertex_buffer, vk.geometry_buffer_memory, vertex_buffer_offset );
		vk.tess[i].vertex_buffer_ptr = (byte*)data + vertex_buffer_offset;
		vk.tess[i].vertex_buffer_offset = 0;
		vertex_buffer_offset += vb_memory_requirements.size;

		SET_OBJECT_NAME( vk.tess[i].vertex_buffer, va( "geometry buffer %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	}

	SET_OBJECT_NAME( vk.geometry_buffer_memory, "geometry buffer memory", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );

	vk.geometry_buffer_size = vb_memory_requirements.size;

	Com_Memset( &vk.stats, 0, sizeof( vk.stats ) );
}


static void vk_create_storage_buffer( uint32_t size )
{
	VkMemoryRequirements memory_requirements;
	VkMemoryAllocateInfo alloc_info;
	VkBufferCreateInfo desc;
	uint32_t memory_type_bits;
	uint32_t memory_type;

	desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = NULL;

	Com_Memset( &memory_requirements, 0, sizeof( memory_requirements ) );

	desc.size = size;
	desc.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	VK_CHECK( qvkCreateBuffer( vk.device, &desc, NULL, &vk.storage.buffer ) );

	qvkGetBufferMemoryRequirements( vk.device, vk.storage.buffer, &memory_requirements );

	memory_type_bits = memory_requirements.memoryTypeBits;
	memory_type = find_memory_type( memory_type_bits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = memory_requirements.size;
	alloc_info.memoryTypeIndex = memory_type;

	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.storage.memory ) );
	VK_CHECK( qvkMapMemory( vk.device, vk.storage.memory, 0, VK_WHOLE_SIZE, 0, (void**)&vk.storage.buffer_ptr ) );

	Com_Memset( vk.storage.buffer_ptr, 0, memory_requirements.size );

	qvkBindBufferMemory( vk.device, vk.storage.buffer, vk.storage.memory, 0 );

	SET_OBJECT_NAME( vk.storage.buffer, "storage buffer", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	SET_OBJECT_NAME( vk.storage.descriptor, "storage buffer", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
	SET_OBJECT_NAME( vk.storage.memory, "storage buffer memory", VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT );
}


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
	alloc_info.memoryTypeIndex = find_memory_type( memory_type_bits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
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
		command_buffer = begin_command_buffer();
		copyRegion[0].srcOffset = 0;
		copyRegion[0].dstOffset = uploadDone;
		copyRegion[0].size = uploadSize;
		qvkCmdCopyBuffer( command_buffer, vk.staging_buffer.handle, vk.vbo.vertex_buffer, 1, &copyRegion[0] );
		end_command_buffer( command_buffer, __func__ );
		uploadDone += uploadSize;
	}

	SET_OBJECT_NAME( vk.vbo.vertex_buffer, "static VBO", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	SET_OBJECT_NAME( vk.vbo.buffer_memory, "static VBO memory", VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT );

	return qtrue;
}
#endif

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
	vk.modules.ssao_blur_fs = SHADER_MODULE( ssao_blur_frag_spv );
	vk.modules.ssao_combine_fs = SHADER_MODULE( ssao_combine_frag_spv );
	vk.modules.ssao_debug_fs = SHADER_MODULE( ssao_debug_frag_spv );
	vk.modules.ssao_depth_debug_fs = SHADER_MODULE( ssao_depth_debug_frag_spv );

	SET_OBJECT_NAME( vk.modules.bloom_fs, "bloom extraction fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.blur_fs, "gaussian blur fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.blend_fs, "final bloom blend fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.ssao_fs, "ssao fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.ssao_blur_fs, "ssao blur fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.ssao_combine_fs, "ssao combine fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.ssao_debug_fs, "ssao debug fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.ssao_depth_debug_fs, "ssao depth debug fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.gamma_fs = SHADER_MODULE( gamma_frag_spv );
	vk.modules.gamma_vs = SHADER_MODULE( gamma_vert_spv );

	SET_OBJECT_NAME( vk.modules.gamma_fs, "gamma post-processing fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.gamma_vs, "gamma post-processing vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.smaa_edge_fs = SHADER_MODULE( smaa_edge_frag_spv );
	vk.modules.smaa_blend_fs = SHADER_MODULE( smaa_blend_frag_spv );
	vk.modules.smaa_compose_fs = SHADER_MODULE( smaa_compose_frag_spv );

	SET_OBJECT_NAME( vk.modules.smaa_edge_fs, "smaa edge fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.smaa_blend_fs, "smaa blend fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.smaa_compose_fs, "smaa compose fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

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

	// stencil shadows
	{
		cullType_t cull_types[2] = { CT_FRONT_SIDED, CT_BACK_SIDED };
		qboolean mirror_flags[2] = { qfalse, qtrue };
		int i, j;

		Com_Memset(&def, 0, sizeof(def));
		def.polygon_offset = qfalse;
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

void vk_update_post_process_pipelines( void )
{
	if ( vk.fboActive ) {
		// update gamma shader
		vk_create_post_process_pipeline( 0, 0, 0 );
		if ( vk.capture.image ) {
			// update capture pipeline
			vk_create_post_process_pipeline( 3, gls.captureWidth, gls.captureHeight );
		}
		if ( vk.smaaActive ) {
			vk_create_post_process_pipeline( 10, glConfig.vidWidth, glConfig.vidHeight );
			vk_create_post_process_pipeline( 11, glConfig.vidWidth, glConfig.vidHeight );
			vk_create_post_process_pipeline( 12, glConfig.vidWidth, glConfig.vidHeight );
		}
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
			vk_create_post_process_pipeline( 6, glConfig.vidWidth, glConfig.vidHeight );
			vk_create_post_process_pipeline( 7, glConfig.vidWidth, glConfig.vidHeight );
			vk_create_post_process_pipeline( 8, glConfig.vidWidth, glConfig.vidHeight );
			vk_create_post_process_pipeline( 9, glConfig.vidWidth, glConfig.vidHeight );
		}
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
		memoryTypeIndex = find_memory_type2( memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT, NULL );
		if ( memoryTypeIndex == ~0U ) {
			memoryTypeIndex = find_memory_type( memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		}
	} else {
		memoryTypeIndex = find_memory_type( memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
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
	command_buffer = begin_command_buffer();
	for ( i = 0; i < num_attachments; i++ ) {
		record_image_layout_transition( command_buffer,
			attachments[i].descriptor,
			attachments[i].aspect_flags,
			VK_IMAGE_LAYOUT_UNDEFINED, // old_layout
			attachments[i].image_layout,
			0, 0 );
	}
	end_command_buffer( command_buffer, __func__ );

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


static void vk_create_attachments( void )
{
	uint32_t i;

	vk_clear_attachment_pool();
	vk_create_volumetric_params_buffer();

	// It looks like resulting performance depends from order you're creating/allocating
	// memory for attachments in vulkan i.e. similar images grouped together will provide best results
	// so [resolve0][resolve1][msaa0][msaa1][depth0][depth1] is most optimal
	// while cases like [resolve0][depth0][color0][...] is the worst

	// TODO: preallocate first image chunk in attachment' memory pool?
	if ( vk.fboActive ) {

		VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

		// bloom
		if ( r_bloom->integer ) {
			uint32_t width = gls.captureWidth;
			uint32_t height = gls.captureHeight;
			VkImageUsageFlags bloomUsage = usage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

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
				usage, &vk.ssao_image, &vk.ssao_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse, 0 );
			create_color_attachment( glConfig.vidWidth, glConfig.vidHeight, VK_SAMPLE_COUNT_1_BIT, vk.ssao_format,
				usage, &vk.ssao_blur_image, &vk.ssao_blur_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse, 0 );
		}

        // cubemap
        if ( vk.cubemapActive ) {
            usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

            create_color_attachment( REF_CUBEMAP_SIZE, REF_CUBEMAP_SIZE, VK_SAMPLE_COUNT_1_BIT, vk.color_format,
                usage, &vk.cubeMap.color_image, &vk.cubeMap.color_image_view[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT );

            if ( vk.msaaActive )
                create_color_attachment( REF_CUBEMAP_SIZE, REF_CUBEMAP_SIZE, (VkSampleCountFlagBits)vkSamples, vk.color_format,
                    usage, &vk.cubeMap.color_image_msaa, &vk.cubeMap.color_image_view_msaa[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qtrue, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT );
            
            create_depth_attachment( REF_CUBEMAP_SIZE, REF_CUBEMAP_SIZE, (VkSampleCountFlagBits)vkSamples,
                    &vk.cubeMap.depth_image, &vk.cubeMap.depth_image_view, qtrue );
        
            usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        }

		// post-processing/msaa-resolve
			create_color_attachment( glConfig.vidWidth, glConfig.vidHeight, VK_SAMPLE_COUNT_1_BIT, vk.color_format,
				usage, &vk.color_image, &vk.color_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse, 0 );
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
			create_color_attachment( vk.screenMapWidth, vk.screenMapHeight, vk.screenMapSamples, vk.color_format,
				usage, &vk.screenMap.color_image_msaa, &vk.screenMap.color_image_view_msaa, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, qtrue, 0 );
		}

		// screenmap/msaa-resolve
		create_color_attachment( vk.screenMapWidth, vk.screenMapHeight, VK_SAMPLE_COUNT_1_BIT, vk.color_format,
			usage, &vk.screenMap.color_image, &vk.screenMap.color_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse, 0 );

		// screenmap depth
		create_depth_attachment( vk.screenMapWidth, vk.screenMapHeight, vk.screenMapSamples, &vk.screenMap.depth_image, &vk.screenMap.depth_image_view, qtrue );

		if ( vk.msaaActive ) {
			create_color_attachment( glConfig.vidWidth, glConfig.vidHeight, vkSamples, vk.color_format,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &vk.msaa_image, &vk.msaa_image_view, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, qtrue, 0 );
		}

		if ( r_ext_supersample->integer ) {
			// capture buffer
			usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
			create_color_attachment( gls.captureWidth, gls.captureHeight, VK_SAMPLE_COUNT_1_BIT, vk.capture_format,
				usage, &vk.capture.image, &vk.capture.image_view, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, qfalse, 0 );
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

#ifdef VK_PBR_BRDFLUT
        // BRDF LUT
        if( vk.pbrActive ) {
            uint32_t size = 512;
            usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            
            create_color_attachment( size, size, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R16G16_SFLOAT,
                usage, &vk.brdflut_image, &vk.brdflut_image_view , VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse, 0 );
        }
#endif

	} // if ( vk.fboActive )

	//vk_alloc_attachments();

	create_depth_attachment( glConfig.vidWidth, glConfig.vidHeight, vkSamples, &vk.depth_image, &vk.depth_image_view,
		(vk.fboActive && r_bloom->integer) || (r_ssao && r_ssao->integer) ? qfalse : qtrue );

	vk_alloc_attachments();
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
		if ( r_fbo->integer == 0 )
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

			// gamma correction
			desc.renderPass = vk.render_pass.gamma;
			desc.attachmentCount = 1;
			desc.width = gls.windowWidth;
			desc.height = gls.windowHeight;
			framebuffer_attachments[0] = vk.swapchain_image_views[n];
			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.gamma[n] ) );

			SET_OBJECT_NAME( vk.framebuffers.gamma[n], "framebuffer - gamma-correction", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
		}
	}

	if ( vk.fboActive )
	{
		// screenmap
		desc.renderPass = vk.render_pass.screenmap;
		desc.attachmentCount = 2;
		desc.width = vk.screenMapWidth;
		desc.height = vk.screenMapHeight;
		framebuffer_attachments[0] = vk.screenMap.color_image_view;
		framebuffer_attachments[1] = vk.screenMap.depth_image_view;
		if ( vk.screenMapSamples > VK_SAMPLE_COUNT_1_BIT )
		{
			desc.attachmentCount = 3;
			framebuffer_attachments[2] = vk.screenMap.color_image_view_msaa;
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


static void vk_create_sync_primitives( void ) {
	VkSemaphoreCreateInfo desc;
	VkFenceCreateInfo fence_desc;
	uint32_t i;

	desc.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;

#ifdef USE_UPLOAD_QUEUE
	VK_CHECK( qvkCreateSemaphore( vk.device, &desc, NULL, &vk.image_uploaded2 ) );
#endif

	// all commands submitted
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ )
	{
		desc.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;

		// swapchain image acquired
		VK_CHECK( qvkCreateSemaphore( vk.device, &desc, NULL, &vk.tess[i].image_acquired ) );

#ifdef USE_UPLOAD_QUEUE
		// second semaphore to synchronize additional tasks (e.g. image upload)
		VK_CHECK( qvkCreateSemaphore( vk.device, &desc, NULL, &vk.tess[i].rendering_finished2 ) );
#endif
		fence_desc.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fence_desc.pNext = NULL;
		//fence_desc.flags = VK_FENCE_CREATE_SIGNALED_BIT; // so it can be used to start rendering
		fence_desc.flags = 0; // non-signalled state

		VK_CHECK( qvkCreateFence( vk.device, &fence_desc, NULL, &vk.tess[i].rendering_finished_fence ) );
		vk.tess[i].waitForFence = qfalse;

		SET_OBJECT_NAME( vk.tess[i].image_acquired, va( "image_acquired semaphore %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT );
#ifdef USE_UPLOAD_QUEUE
		SET_OBJECT_NAME( vk.tess[i].rendering_finished2, va( "rendering_finished2 semaphore %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT );
#endif
		SET_OBJECT_NAME( vk.tess[i].rendering_finished_fence, va( "rendering_finished fence %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT );
	}

	fence_desc.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_desc.pNext = NULL;
	fence_desc.flags = 0;

#ifdef USE_UPLOAD_QUEUE
	VK_CHECK( qvkCreateFence( vk.device, &fence_desc, NULL, &vk.aux_fence ) );
	SET_OBJECT_NAME( vk.aux_fence, "aux fence", VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT );

	vk.rendering_finished = VK_NULL_HANDLE;
	vk.image_uploaded = VK_NULL_HANDLE;
	vk.aux_fence_wait = qfalse;
#endif
}


static void vk_destroy_sync_primitives( void  ) {
	uint32_t i;

#ifdef USE_UPLOAD_QUEUE
	qvkDestroySemaphore( vk.device, vk.image_uploaded2, NULL );
#endif

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		qvkDestroySemaphore( vk.device, vk.tess[i].image_acquired, NULL );
#ifdef USE_UPLOAD_QUEUE
		qvkDestroySemaphore( vk.device, vk.tess[i].rendering_finished2, NULL );
#endif
		qvkDestroyFence( vk.device, vk.tess[i].rendering_finished_fence, NULL );
		vk.tess[i].waitForFence = qfalse;
		vk.tess[i].swapchain_image_acquired = qfalse;
	}

#ifdef USE_UPLOAD_QUEUE
	qvkDestroyFence( vk.device, vk.aux_fence, NULL );

	vk.rendering_finished = VK_NULL_HANDLE;
	vk.image_uploaded = VK_NULL_HANDLE;
#endif
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
}


static void vk_destroy_swapchain( void ) {
	uint32_t i;

	vk.swapchain_extent_valid = qfalse;

	for ( i = 0; (uint32_t) i < vk.swapchain_image_count; i++ ) {
		if ( vk.swapchain_image_views[i] != VK_NULL_HANDLE ) {
			qvkDestroyImageView( vk.device, vk.swapchain_image_views[i], NULL );
			vk.swapchain_image_views[i] = VK_NULL_HANDLE;
		}
		if ( vk.swapchain_rendering_finished[i] != VK_NULL_HANDLE ) {
			qvkDestroySemaphore( vk.device, vk.swapchain_rendering_finished[i], NULL );
			vk.swapchain_rendering_finished[i] = VK_NULL_HANDLE;
		}
	}

	qvkDestroySwapchainKHR( vk.device, vk.swapchain, NULL );
}

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
	setup_surface_formats( vk.physical_device );

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

	init_vulkan_library();

	qvkGetDeviceQueue( vk.device, vk.queue_family_index, 0, &vk.queue );

	qvkGetPhysicalDeviceProperties( vk.physical_device, &props );

	vk.cmd = vk.tess + 0;
	vk_reset_motion_history();
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

	vk_set_render_scale();

	if ( r_fbo->integer ) {
		vk.fboActive = qtrue;
		if ( r_ext_multisample->integer ) {
			vk.msaaActive = qtrue;
		}
	} else {
		vk.fboActive = qfalse;
	}
	vk.smaaActive = (vk.fboActive && r_ext_smaa->integer) ? qtrue : qfalse;

	// multisampling

	vkMaxSamples = MIN( props.limits.sampledImageColorSampleCounts, props.limits.sampledImageDepthSampleCounts );

	if ( /*vk.fboActive &&*/ vk.msaaActive ) {
		VkSampleCountFlags mask = vkMaxSamples;
		vkSamples = MAX( log2pad( r_ext_multisample->integer, 1 ), VK_SAMPLE_COUNT_2_BIT );
		while ( (VkSampleCountFlags)vkSamples > mask )
				vkSamples >>= 1;
		ri.Printf( PRINT_ALL, "...using %ix MSAA\n", vkSamples );
	} else {
		vkSamples = VK_SAMPLE_COUNT_1_BIT;
	}

	vk.screenMapSamples = MIN( vkMaxSamples, VK_SAMPLE_COUNT_4_BIT );

	vk.screenMapWidth = (float) glConfig.vidWidth / 16.0;
	if ( vk.screenMapWidth < 4 )
		vk.screenMapWidth = 4;

	vk.screenMapHeight = (float) glConfig.vidHeight / 16.0;
	if ( vk.screenMapHeight < 4 )
		vk.screenMapHeight = 4;

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
			ri.Printf( PRINT_ALL, S_COLOR_YELLOW "PBR: disabled (requires \\r_fbo 1)\n" S_COLOR_WHITE );
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
	Q_strncpyz( glConfig.renderer_string, renderer_name( &props ), sizeof( glConfig.renderer_string ) );

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
			ri.Printf( PRINT_ALL, "[VK]   GPU         : %s\n", renderer_name( &props ) );
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
		} else {
			vk.volumetric_query_pool = VK_NULL_HANDLE;
		}
	}

	//
	// Descriptor pool.
	//
		{
			VkDescriptorPoolSize pool_size[5];
		VkDescriptorPoolCreateInfo desc;
		uint32_t j, maxSets;

		pool_size[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			pool_size[0].descriptorCount = MAX_DRAWIMAGES + 1 + 1 + 1 + 3 + 6 + VK_NUM_BLOOM_PASSES * 2 + 25; // color, screenmap, depth/ssao, volumetric descriptors (including fluid + telemetry), bloom descriptors, SMAA aux descriptors
#ifdef USE_VK_PBR
        if ( vk.pbrActive )
            pool_size[0].descriptorCount += 2 + ( MAX_DRAWIMAGES * 8 ); // brdf-lut + irradiance | MAX_DRAWIMAGES * (physical, normal, emissive, clearcoat, sheen, anisotropy, transmission, subsurface)
#endif

		pool_size[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		pool_size[1].descriptorCount = NUM_COMMAND_BUFFERS;
#ifdef USE_VK_PBR
        if ( vk.pbrActive )
            pool_size[1].descriptorCount += NUM_COMMAND_BUFFERS; // camera uniform
#endif

		//pool_size[2].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		//pool_size[2].descriptorCount = NUM_COMMAND_BUFFERS;

		pool_size[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
		pool_size[2].descriptorCount = 1;

		pool_size[3].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			pool_size[3].descriptorCount = 22;

		pool_size[4].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		pool_size[4].descriptorCount = 8;

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
	vk_create_layout_binding( 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, &vk.set_layout_uniform );
	vk_create_layout_binding( 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, &vk.set_layout_storage );
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
		set_layouts[0] = vk.set_layout_sampler; // sampler
		set_layouts[1] = vk.set_layout_sampler; // sampler
		set_layouts[2] = vk.set_layout_sampler; // sampler
		set_layouts[3] = vk.set_layout_sampler; // sampler

		push_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		push_range.offset = 0;
		push_range.size = sizeof( VkPostProcessPushConstants );

		desc.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.setLayoutCount = 1;
		desc.pSetLayouts = set_layouts;
		desc.pushConstantRangeCount = 1;
		desc.pPushConstantRanges = &push_range;

		VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.pipeline_layout_post_process ) );

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

			smaa_layouts[0] = vk.set_layout_sampler;
			smaa_layouts[1] = vk.set_layout_sampler;

			Com_Memset( &smaa_desc, 0, sizeof( smaa_desc ) );
			smaa_desc.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			smaa_desc.pNext = NULL;
			smaa_desc.flags = 0;
			smaa_desc.setLayoutCount = ARRAY_LEN( smaa_layouts );
			smaa_desc.pSetLayouts = smaa_layouts;
			smaa_desc.pushConstantRangeCount = 0;
			smaa_desc.pPushConstantRanges = NULL;

			VK_CHECK( qvkCreatePipelineLayout( vk.device, &smaa_desc, NULL, &vk.pipeline_layout_smaa ) );
			SET_OBJECT_NAME( vk.pipeline_layout_smaa, "pipeline layout - smaa", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );
		}

		// ssao pipeline layout (depth sampler + push constants)
		set_layouts[0] = vk.set_layout_sampler;
		desc.setLayoutCount = 1;
		desc.pSetLayouts = set_layouts;
		push_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		push_range.offset = 0;
		push_range.size = 48; // ssao push constants
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
	
#ifdef VK_PBR_BRDFLUT
		if( vk.pbrActive ) {
			desc.setLayoutCount = 1;
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
		vk.volumetric_fluid_pipeline_layout != VK_NULL_HANDLE )
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

	desc.pSetLayouts = &vk.volumetric_fluid_layout;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.volumetric_fluid_pipeline_layout ) );
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
	shader_stages[1].module = vk.modules.volumetric_fog_fs;
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

	VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamic_state;
	Com_Memset( &dynamic_state, 0, sizeof( dynamic_state ) );
	dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.dynamicStateCount = ARRAY_LEN( dynamic_states );
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
	vk_create_volumetric_compute_pipeline();
	vk_create_volumetric_fluid_pipelines();
	vk_create_volumetric_composite_pipeline();
}

static void vk_destroy_volumetric_pipelines( void )
{
	if ( vk.volumetric_depth_resolve_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.volumetric_depth_resolve_pipeline, NULL );
		vk.volumetric_depth_resolve_pipeline = VK_NULL_HANDLE;
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
	if ( vk.volumetric_fluid_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.volumetric_fluid_pipeline_layout, NULL );
		vk.volumetric_fluid_pipeline_layout = VK_NULL_HANDLE;
	}
}

static qboolean Mat4Inverse( const float *m, float *out ) {
	float tmp[16];
	tmp[0] = m[5]  * m[10] * m[15] -
	         m[5]  * m[11] * m[14] -
	         m[9]  * m[6]  * m[15] +
	         m[9]  * m[7]  * m[14] +
	         m[13] * m[6]  * m[11] -
	         m[13] * m[7]  * m[10];

	tmp[4] = -m[4]  * m[10] * m[15] +
	          m[4]  * m[11] * m[14] +
	          m[8]  * m[6]  * m[15] -
	          m[8]  * m[7]  * m[14] -
	          m[12] * m[6]  * m[11] +
	          m[12] * m[7]  * m[10];

	tmp[8] = m[4]  * m[9] * m[15] -
	         m[4]  * m[11] * m[13] -
	         m[8]  * m[5] * m[15] +
	         m[8]  * m[7] * m[13] +
	         m[12] * m[5] * m[11] -
	         m[12] * m[7] * m[9];

	tmp[12] = -m[4]  * m[9] * m[14] +
	           m[4]  * m[10] * m[13] +
	           m[8]  * m[5] * m[14] -
	           m[8]  * m[6] * m[13] -
	           m[12] * m[5] * m[10] +
	           m[12] * m[6] * m[9];

	tmp[1] = -m[1]  * m[10] * m[15] +
	          m[1]  * m[11] * m[14] +
	          m[9]  * m[2] * m[15] -
	          m[9]  * m[3] * m[14] -
	          m[13] * m[2] * m[11] +
	          m[13] * m[3] * m[10];

	tmp[5] = m[0]  * m[10] * m[15] -
	         m[0]  * m[11] * m[14] -
	         m[8]  * m[2] * m[15] +
	         m[8]  * m[3] * m[14] +
	         m[12] * m[2] * m[11] -
	         m[12] * m[3] * m[10];

	tmp[9] = -m[0]  * m[9] * m[15] +
	          m[0]  * m[11] * m[13] +
	          m[8]  * m[1] * m[15] -
	          m[8]  * m[3] * m[13] -
	          m[12] * m[1] * m[11] +
	          m[12] * m[3] * m[9];

	tmp[13] = m[0]  * m[9] * m[14] -
	          m[0]  * m[10] * m[13] -
	          m[8]  * m[1] * m[14] +
	          m[8]  * m[2] * m[13] +
	          m[12] * m[1] * m[10] -
	          m[12] * m[2] * m[9];

	tmp[2] = m[1]  * m[6] * m[15] -
	         m[1]  * m[7] * m[14] -
	         m[5]  * m[2] * m[15] +
	         m[5]  * m[3] * m[14] +
	         m[13] * m[2] * m[7] -
	         m[13] * m[3] * m[6];

	tmp[6] = -m[0]  * m[6] * m[15] +
	          m[0]  * m[7] * m[14] +
	          m[4]  * m[2] * m[15] -
	          m[4]  * m[3] * m[14] -
	          m[12] * m[2] * m[7] +
	          m[12] * m[3] * m[6];

	tmp[10] = m[0]  * m[5] * m[15] -
	          m[0]  * m[7] * m[13] -
	          m[4]  * m[1] * m[15] +
	          m[4]  * m[3] * m[13] +
	          m[12] * m[1] * m[7] -
	          m[12] * m[3] * m[5];

	tmp[14] = -m[0]  * m[5] * m[14] +
	           m[0]  * m[6] * m[13] +
	           m[4]  * m[1] * m[14] -
	           m[4]  * m[2] * m[13] -
	           m[12] * m[1] * m[6] +
	           m[12] * m[2] * m[5];

	tmp[3] = -m[1] * m[6] * m[11] +
	          m[1] * m[7] * m[10] +
	          m[5] * m[2] * m[11] -
	          m[5] * m[3] * m[10] -
	          m[9] * m[2] * m[7] +
	          m[9] * m[3] * m[6];

	tmp[7] = m[0] * m[6] * m[11] -
	         m[0] * m[7] * m[10] -
	         m[4] * m[2] * m[11] +
	         m[4] * m[3] * m[10] +
	         m[8] * m[2] * m[7] -
	         m[8] * m[3] * m[6];

	tmp[11] = -m[0] * m[5] * m[11] +
	           m[0] * m[7] * m[9] +
	           m[4] * m[1] * m[11] -
	           m[4] * m[3] * m[9] -
	           m[8] * m[1] * m[7] +
	           m[8] * m[3] * m[5];

	tmp[15] = m[0] * m[5] * m[10] -
	          m[0] * m[6] * m[9] -
	          m[4] * m[1] * m[10] +
	          m[4] * m[2] * m[9] +
	          m[8] * m[1] * m[6] -
	          m[8] * m[2] * m[5];

	float det = m[0] * tmp[0] + m[1] * tmp[4] + m[2] * tmp[8] + m[3] * tmp[12];

	if ( fabs( det ) < 1e-9f ) {
		return qfalse;
	}

	det = 1.0f / det;

	for ( int i = 0; i < 16; ++i ) {
		out[i] = tmp[i] * det;
	}
	return qtrue;
}

static uint32_t vk_noise_hash3( uint32_t x, uint32_t y, uint32_t z )
{
	uint32_t h = x * 374761393u + y * 668265263u + z * 2246822519u;
	h = ( h ^ ( h >> 13 ) ) * 1274126177u;
	return h ^ ( h >> 16 );
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
	alloc_info.memoryTypeIndex = find_memory_type( mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
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
	alloc_info.memoryTypeIndex = find_memory_type( mem_req.memoryTypeBits,
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

	command_buffer = begin_command_buffer();
	record_image_layout_transition( command_buffer, vk.fog_noise_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT );
	qvkCmdCopyBufferToImage( command_buffer, staging_buffer, vk.fog_noise_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region );
	record_image_layout_transition( command_buffer, vk.fog_noise_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	end_command_buffer( command_buffer, __func__ );

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
	alloc_info.memoryTypeIndex = find_memory_type( mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.sun_shadow_memory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, vk.sun_shadow_image, vk.sun_shadow_memory, 0 ) );

	image_info.format = vk.color_format;
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	VK_CHECK( qvkCreateImage( vk.device, &image_info, NULL, &vk.sun_shadow_color_image ) );

	qvkGetImageMemoryRequirements( vk.device, vk.sun_shadow_color_image, &mem_req );
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = find_memory_type( mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
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

	view_info.image = vk.sun_shadow_color_image;
	view_info.format = vk.color_format;
	view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	VK_CHECK( qvkCreateImageView( vk.device, &view_info, NULL, &vk.sun_shadow_color_view ) );

	cmd = begin_command_buffer();
	record_image_layout_transition( cmd, vk.sun_shadow_color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
	record_image_layout_transition( cmd, vk.sun_shadow_image, depth_aspect,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	end_command_buffer( cmd, __func__ );

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
	alloc_info.memoryTypeIndex = find_memory_type( mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.local_spot_shadow_atlas_memory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, vk.local_spot_shadow_atlas_image, vk.local_spot_shadow_atlas_memory, 0 ) );

	image_info.format = vk.color_format;
	image_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	VK_CHECK( qvkCreateImage( vk.device, &image_info, NULL, &vk.local_spot_shadow_color_image ) );
	qvkGetImageMemoryRequirements( vk.device, vk.local_spot_shadow_color_image, &mem_req );
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = find_memory_type( mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
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
	alloc_info.memoryTypeIndex = find_memory_type( mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.local_point_shadow_array_memory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, vk.local_point_shadow_array_image, vk.local_point_shadow_array_memory, 0 ) );

	image_info.format = vk.color_format;
	image_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	VK_CHECK( qvkCreateImage( vk.device, &image_info, NULL, &vk.local_point_shadow_color_array_image ) );

	qvkGetImageMemoryRequirements( vk.device, vk.local_point_shadow_color_array_image, &mem_req );
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = find_memory_type( mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
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

	cmd = begin_command_buffer();
	record_image_layout_transition( cmd, vk.local_spot_shadow_color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
	record_image_layout_transition( cmd, vk.local_spot_shadow_atlas_image, depth_aspect,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	record_image_layout_transition( cmd, vk.local_point_shadow_color_array_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
	record_image_layout_transition( cmd, vk.local_point_shadow_array_image, depth_aspect,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	end_command_buffer( cmd, __func__ );
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
	create_info_fluid_velocity.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
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
	alloc_info.memoryTypeIndex = find_memory_type( mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.froxel_volume_memory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, vk.froxel_volume_image, vk.froxel_volume_memory, 0 ) );

	VK_CHECK( qvkCreateImage( vk.device, &create_info, NULL, &vk.froxel_history_image ) );

	qvkGetImageMemoryRequirements( vk.device, vk.froxel_history_image, &mem_req );
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = find_memory_type( mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.froxel_history_memory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, vk.froxel_history_image, vk.froxel_history_memory, 0 ) );

	VK_CHECK( qvkCreateImage( vk.device, &create_info, NULL, &vk.froxel_light_image ) );

	qvkGetImageMemoryRequirements( vk.device, vk.froxel_light_image, &mem_req );
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = find_memory_type( mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.froxel_light_memory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, vk.froxel_light_image, vk.froxel_light_memory, 0 ) );

	VK_CHECK( qvkCreateImage( vk.device, &create_info_extinction, NULL, &vk.froxel_extinction_image ) );

	qvkGetImageMemoryRequirements( vk.device, vk.froxel_extinction_image, &mem_req );
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = find_memory_type( mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.froxel_extinction_memory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, vk.froxel_extinction_image, vk.froxel_extinction_memory, 0 ) );

	VK_CHECK( qvkCreateImage( vk.device, &create_info, NULL, &vk.froxel_clamp_image ) );

	qvkGetImageMemoryRequirements( vk.device, vk.froxel_clamp_image, &mem_req );
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = find_memory_type( mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.froxel_clamp_memory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, vk.froxel_clamp_image, vk.froxel_clamp_memory, 0 ) );

	for ( int i = 0; i < 2; i++ ) {
		VK_CHECK( qvkCreateImage( vk.device, &create_info_fluid_velocity, NULL, &vk.fluid_velocity_images[i] ) );
		qvkGetImageMemoryRequirements( vk.device, vk.fluid_velocity_images[i], &mem_req );
		alloc_info.allocationSize = mem_req.size;
		alloc_info.memoryTypeIndex = find_memory_type( mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.fluid_velocity_memory[i] ) );
		VK_CHECK( qvkBindImageMemory( vk.device, vk.fluid_velocity_images[i], vk.fluid_velocity_memory[i], 0 ) );

		VK_CHECK( qvkCreateImage( vk.device, &create_info_fluid_scalar, NULL, &vk.fluid_density_images[i] ) );
		qvkGetImageMemoryRequirements( vk.device, vk.fluid_density_images[i], &mem_req );
		alloc_info.allocationSize = mem_req.size;
		alloc_info.memoryTypeIndex = find_memory_type( mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.fluid_density_memory[i] ) );
		VK_CHECK( qvkBindImageMemory( vk.device, vk.fluid_density_images[i], vk.fluid_density_memory[i], 0 ) );

		VK_CHECK( qvkCreateImage( vk.device, &create_info_fluid_scalar, NULL, &vk.fluid_pressure_images[i] ) );
		qvkGetImageMemoryRequirements( vk.device, vk.fluid_pressure_images[i], &mem_req );
		alloc_info.allocationSize = mem_req.size;
		alloc_info.memoryTypeIndex = find_memory_type( mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.fluid_pressure_memory[i] ) );
		VK_CHECK( qvkBindImageMemory( vk.device, vk.fluid_pressure_images[i], vk.fluid_pressure_memory[i], 0 ) );
	}

	VK_CHECK( qvkCreateImage( vk.device, &create_info_fluid_scalar, NULL, &vk.fluid_divergence_image ) );
	qvkGetImageMemoryRequirements( vk.device, vk.fluid_divergence_image, &mem_req );
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = find_memory_type( mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.fluid_divergence_memory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, vk.fluid_divergence_image, vk.fluid_divergence_memory, 0 ) );

	VK_CHECK( qvkCreateImage( vk.device, &create_info_telemetry, NULL, &vk.volumetric_telemetry_image ) );
	qvkGetImageMemoryRequirements( vk.device, vk.volumetric_telemetry_image, &mem_req );
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = find_memory_type( mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
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

	VkCommandBuffer command_buffer = begin_command_buffer();
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
	end_command_buffer( command_buffer, __func__ );

	vk_create_fog_noise_texture();
}

static void vk_destroy_attachments( void )
{
	uint32_t i;

	vk_destroy_volumetric_params_buffer();
	vk_destroy_froxel_images();
	vk_destroy_sun_shadow_resources();
	vk_destroy_local_shadow_resources();
	vk.volumetric_compute_descriptor = VK_NULL_HANDLE;
	vk.volumetric_composite_descriptor = VK_NULL_HANDLE;
	vk.volumetric_depth_resolve_descriptor = VK_NULL_HANDLE;
	vk.volumetric_fluid_descriptor = VK_NULL_HANDLE;

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

	if ( vk.color_image ) {
		qvkDestroyImage( vk.device, vk.color_image, NULL );
		qvkDestroyImageView( vk.device, vk.color_image_view, NULL );
		vk.color_image = VK_NULL_HANDLE;
		vk.color_image_view = VK_NULL_HANDLE;
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

	if ( vk.msaa_image ) {
		qvkDestroyImage( vk.device, vk.msaa_image, NULL );
		qvkDestroyImageView( vk.device, vk.msaa_image_view, NULL );
		vk.msaa_image = VK_NULL_HANDLE;
		vk.msaa_image_view = VK_NULL_HANDLE;
	}

	qvkDestroyImage( vk.device, vk.depth_image, NULL );
	qvkDestroyImageView( vk.device, vk.depth_image_view, NULL );
	vk.depth_image = VK_NULL_HANDLE;
	vk.depth_image_view = VK_NULL_HANDLE;

	if ( vk.screenMap.color_image ) {
		qvkDestroyImage( vk.device, vk.screenMap.color_image, NULL );
		qvkDestroyImageView( vk.device, vk.screenMap.color_image_view, NULL );
		vk.screenMap.color_image = VK_NULL_HANDLE;
		vk.screenMap.color_image_view = VK_NULL_HANDLE;
	}

	if ( vk.screenMap.color_image_msaa ) {
		qvkDestroyImage( vk.device, vk.screenMap.color_image_msaa, NULL );
		qvkDestroyImageView( vk.device, vk.screenMap.color_image_view_msaa, NULL );
		vk.screenMap.color_image_msaa = VK_NULL_HANDLE;
		vk.screenMap.color_image_view_msaa = VK_NULL_HANDLE;
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
        qvkDestroyImageView(vk.device, vk.depth_image_view, NULL);
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

	if ( vk.render_pass.volumetric != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.volumetric, NULL );
		vk.render_pass.volumetric = VK_NULL_HANDLE;
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

	if ( vk.capture_pipeline ) {
		qvkDestroyPipeline( vk.device, vk.capture_pipeline, NULL );
		vk.capture_pipeline = VK_NULL_HANDLE;
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

	if ( vk.ssao_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.ssao_pipeline, NULL );
		vk.ssao_pipeline = VK_NULL_HANDLE;
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

	if ( qvkQueuePresentKHR == NULL ) { // not fully initialized
		goto __cleanup;
	}

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

	if ( vk.pipelineCache != VK_NULL_HANDLE ) {
		qvkDestroyPipelineCache( vk.device, vk.pipelineCache, NULL );
		vk.pipelineCache = VK_NULL_HANDLE;
	}
	if ( vk.volumetric_query_pool != VK_NULL_HANDLE ) {
		qvkDestroyQueryPool( vk.device, vk.volumetric_query_pool, NULL );
		vk.volumetric_query_pool = VK_NULL_HANDLE;
	}

	qvkDestroyCommandPool( vk.device, vk.command_pool, NULL );

	qvkDestroyDescriptorPool(vk.device, vk.descriptor_pool, NULL);

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
	if ( vk.volumetric_fluid_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.volumetric_fluid_layout, NULL );
		vk.volumetric_fluid_layout = VK_NULL_HANDLE;
	}

	qvkDestroyPipelineLayout(vk.device, vk.pipeline_layout, NULL);
	qvkDestroyPipelineLayout(vk.device, vk.pipeline_layout_storage, NULL);
	qvkDestroyPipelineLayout(vk.device, vk.pipeline_layout_post_process, NULL);
	qvkDestroyPipelineLayout(vk.device, vk.pipeline_layout_blend, NULL);
	if ( vk.pipeline_layout_smaa != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout(vk.device, vk.pipeline_layout_smaa, NULL);
		vk.pipeline_layout_smaa = VK_NULL_HANDLE;
	}
	qvkDestroyPipelineLayout(vk.device, vk.pipeline_layout_ssao, NULL);
	qvkDestroyPipelineLayout(vk.device, vk.pipeline_layout_ssao_combine, NULL);
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

	qvkDestroyShaderModule( vk.device, vk.modules.color_fs, NULL );
	qvkDestroyShaderModule( vk.device, vk.modules.color_vs, NULL );

	qvkDestroyShaderModule(vk.device, vk.modules.fog_vs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.fog_fs, NULL);

	qvkDestroyShaderModule(vk.device, vk.modules.dot_vs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.dot_fs, NULL);

	qvkDestroyShaderModule(vk.device, vk.modules.bloom_fs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.blur_fs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.blend_fs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.ssao_fs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.ssao_blur_fs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.ssao_combine_fs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.ssao_debug_fs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.ssao_depth_debug_fs, NULL);

	qvkDestroyShaderModule(vk.device, vk.modules.gamma_vs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.gamma_fs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.smaa_edge_fs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.smaa_blend_fs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.smaa_compose_fs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.volumetric_fog_vs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.volumetric_fog_fs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.volumetric_fog_cs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.volumetric_depth_resolve_msaa_cs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.fluid_advect_cs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.fluid_divergence_cs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.fluid_pressure_cs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.fluid_gradient_cs, NULL);

#ifdef USE_VK_PBR
	qvkDestroyShaderModule(vk.device, vk.modules.brdflut_fs, NULL);
#endif

__cleanup:
	if ( vk.device != VK_NULL_HANDLE ) {
		qvkDestroyDevice( vk.device, NULL );
	}

	deinit_device_functions();

	Com_Memset( &vk, 0, sizeof( vk ) );
	Com_Memset( &vk_world, 0, sizeof( vk_world ) );
	
	if ( code != REF_KEEP_CONTEXT ) {
		vk_destroy_instance();
		deinit_instance_functions();
	}
}


void vk_wait_idle( void )
{
	VK_CHECK( qvkDeviceWaitIdle( vk.device ) );
}


void vk_queue_wait_idle( void )
{
	VK_CHECK( qvkQueueWaitIdle( vk.queue ) );
}


void vk_release_resources( void ) {
	int i, j;

	vk_wait_idle();

	for ( i = 0; i < vk_world.num_image_chunks; i++ )
		qvkFreeMemory( vk.device, vk_world.image_chunks[i].memory, NULL );

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

	if ( vk_world.num_image_chunks > 1 ) {
		// if we allocated more than 2 image chunks - use doubled default size
		vk.image_chunk_size = (IMAGE_CHUNK_SIZE * 2);
	}
#if 0 // do not reduce chunk size
	else if ( vk_world.num_image_chunks == 1 ) {
		// otherwise set to default if used less than a half
		if ( vk_world.image_chunks[0].used < ( IMAGE_CHUNK_SIZE - (IMAGE_CHUNK_SIZE / 10) ) ) {
			vk.image_chunk_size = IMAGE_CHUNK_SIZE;
		}
	}
#endif

	Com_Memset( &vk_world, 0, sizeof( vk_world ) );

	// Reset geometry buffers offsets
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		vk.tess[i].uniform_read_offset = 0;
		vk.tess[i].vertex_buffer_offset = 0;
	}

	Com_Memset( vk.cmd->buf_offset, 0, sizeof( vk.cmd->buf_offset ) );
	Com_Memset( vk.cmd->vbo_offset, 0, sizeof( vk.cmd->vbo_offset ) );

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
		desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		desc.queueFamilyIndexCount = 0;
		desc.pQueueFamilyIndices = NULL;
		desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VK_CHECK( qvkCreateImage( vk.device, &desc, NULL, &image->handle ) );

		allocate_and_bind_image_memory( image->handle );
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

	command_buffer = begin_command_buffer();
	// record_buffer_memory_barrier( command_buffer, vk_world.staging_buffer, VK_WHOLE_SIZE, 0, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT );
	if ( update ) {
		record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );
	} else {
		record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT, 0 );
	}
	qvkCmdCopyBufferToImage( command_buffer, vk.staging_buffer.handle, image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, num_regions, regions );
	record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
	end_command_buffer( command_buffer, __func__ );
#endif

	if ( buf != pixels ) {
		ri.Hunk_FreeTempMemory( buf );
	}
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


#define FORMAT_DEPTH(format, r_bits, g_bits, b_bits) case(VK_FORMAT_##format): *r = r_bits; *b = b_bits; *g = g_bits; return qtrue;
static qboolean vk_surface_format_color_depth( VkFormat format, int *r, int *g, int *b ) {
	switch (format) {
		// Common formats from https://vulkan.gpuinfo.org/listsurfaceformats.php
		FORMAT_DEPTH(B8G8R8A8_UNORM, 255, 255, 255)
			FORMAT_DEPTH(B8G8R8A8_SRGB, 255, 255, 255)
			FORMAT_DEPTH(A2B10G10R10_UNORM_PACK32, 1023, 1023, 1023)
			FORMAT_DEPTH(R8G8B8A8_UNORM, 255, 255, 255)
			FORMAT_DEPTH(R8G8B8A8_SRGB, 255, 255, 255)
			FORMAT_DEPTH(A2R10G10B10_UNORM_PACK32, 1023, 1023, 1023)
			FORMAT_DEPTH(R5G6B5_UNORM_PACK16, 31, 63, 31)
			FORMAT_DEPTH(R8G8B8A8_SNORM, 255, 255, 255)
			FORMAT_DEPTH(A8B8G8R8_UNORM_PACK32, 255, 255, 255)
			FORMAT_DEPTH(A8B8G8R8_SNORM_PACK32, 255, 255, 255)
			FORMAT_DEPTH(A8B8G8R8_SRGB_PACK32, 255, 255, 255)
			FORMAT_DEPTH(R16G16B16A16_UNORM, 65535, 65535, 65535)
			FORMAT_DEPTH(R16G16B16A16_SNORM, 65535, 65535, 65535)
			FORMAT_DEPTH(B5G6R5_UNORM_PACK16, 31, 63, 31)
			FORMAT_DEPTH(B8G8R8A8_SNORM, 255, 255, 255)
			FORMAT_DEPTH(R4G4B4A4_UNORM_PACK16, 15, 15, 15)
			FORMAT_DEPTH(B4G4R4A4_UNORM_PACK16, 15, 15, 15)
			FORMAT_DEPTH(A1R5G5B5_UNORM_PACK16, 31, 31, 31)
			FORMAT_DEPTH(R5G5B5A1_UNORM_PACK16, 31, 31, 31)
			FORMAT_DEPTH(B5G5R5A1_UNORM_PACK16, 31, 31, 31)
	default:
		*r = 255; *g = 255; *b = 255; return qfalse;
	}
}

static qboolean vk_format_is_srgb( VkFormat format ) {
	switch ( format ) {
		case VK_FORMAT_B8G8R8A8_SRGB:
		case VK_FORMAT_R8G8B8A8_SRGB:
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
	VkDynamicState dynamic_state_array[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineMultisampleStateCreateInfo multisample_state;
	VkPipelineColorBlendStateCreateInfo blend_state;
	VkPipelineColorBlendAttachmentState attachment_blend_state;
	VkGraphicsPipelineCreateInfo create_info;
	VkViewport viewport;
	VkRect2D scissor;
	VkSpecializationMapEntry spec_entries[22];
	VkSpecializationInfo frag_spec_info;
	VkPipeline *pipeline;
	VkShaderModule fsmodule;
	VkRenderPass renderpass;
	VkPipelineLayout layout;
	VkFormat target_format;
	VkSampleCountFlagBits samples;
	const char *pipeline_name;
	qboolean blend;

	struct PostProcess_FragSpecData {
		float gamma;           /* constant_id = 0  */
		float preExposureScale; /* constant_id = 1 */
		float greyscale;       /* constant_id = 2  */
		float bloom_threshold; /* constant_id = 3  */
		float bloom_intensity; /* constant_id = 4  */
		int bloom_threshold_mode; /* constant_id = 5 */
		int bloom_modulate;    /* constant_id = 6  */
		int dither;            /* constant_id = 7  */
		int depth_r;           /* constant_id = 8  */
		int depth_g;           /* constant_id = 9  */
		int depth_b;           /* constant_id = 10 */
		float exposure;        /* constant_id = 11 */
		float bloom_knee;      /* constant_id = 12 */
		int tonemap_mode;      /* constant_id = 13 */
		int apply_srgb_gamma;  /* constant_id = 14 */
		int post_debug;        /* constant_id = 15 */
		float vignette_intensity; /* constant_id = 16 */
		float vignette_radius;    /* constant_id = 17 */
		float chromatic_aberration; /* constant_id = 18 */
		float film_grain;          /* constant_id = 19 */
		int postprocess_enabled;   /* constant_id = 20 */
	} frag_spec_data;

	switch ( program_index ) {
		case 1: // bloom extraction
			pipeline = &vk.bloom_extract_pipeline;
			fsmodule = vk.modules.bloom_fs;
			renderpass = vk.render_pass.bloom_extract;
			layout = vk.pipeline_layout_post_process;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "bloom extraction pipeline";
			target_format = vk.capture_format;
			blend = qfalse;
			break;
		case 5: // ssao
			pipeline = &vk.ssao_pipeline;
			fsmodule = vk.modules.ssao_fs;
			renderpass = vk.render_pass.ssao;
			layout = vk.pipeline_layout_ssao;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "ssao pipeline";
			target_format = vk.capture_format;
			blend = qfalse;
			break;
		case 6: // ssao blur
			pipeline = &vk.ssao_blur_pipeline;
			fsmodule = vk.modules.ssao_blur_fs;
			renderpass = vk.render_pass.ssao_blur;
			layout = vk.pipeline_layout_ssao;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "ssao blur pipeline";
			target_format = vk.capture_format;
			blend = qfalse;
			break;
		case 7: // ssao combine
			pipeline = &vk.ssao_combine_pipeline;
			fsmodule = vk.modules.ssao_combine_fs;
			renderpass = vk.render_pass.ssao_combine;
			layout = vk.pipeline_layout_post_process;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "ssao combine pipeline";
			target_format = vk.capture_format;
			blend = qfalse;
			break;
		case 8: // ssao debug
			pipeline = &vk.ssao_debug_pipeline;
			fsmodule = vk.modules.ssao_debug_fs;
			renderpass = vk.render_pass.ssao_combine;
			layout = vk.pipeline_layout_post_process;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "ssao debug pipeline";
			target_format = vk.capture_format;
			blend = qfalse;
			break;
		case 9: // ssao depth debug
			pipeline = &vk.ssao_depth_debug_pipeline;
			fsmodule = vk.modules.ssao_depth_debug_fs;
			renderpass = vk.render_pass.ssao_combine;
			layout = vk.pipeline_layout_post_process;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "ssao depth debug pipeline";
			target_format = vk.capture_format;
			blend = qfalse;
			break;
		case 2: // final bloom blend
			pipeline = &vk.bloom_blend_pipeline;
			fsmodule = vk.modules.blend_fs;
			renderpass = vk.render_pass.post_bloom;
			layout = vk.pipeline_layout_blend;
			samples = vkSamples;
			pipeline_name = "bloom blend pipeline";
			target_format = vk.color_format;
			blend = qtrue;
			break;
		case 3: // capture buffer extraction
			pipeline = &vk.capture_pipeline;
			fsmodule = vk.modules.gamma_fs;
			renderpass = vk.render_pass.capture;
			layout = vk.pipeline_layout_post_process;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "capture buffer pipeline";
			target_format = vk.capture_format;
			blend = qfalse;
			break;
		case 10: // smaa edge
			pipeline = &vk.smaa_edge_pipeline;
			fsmodule = vk.modules.smaa_edge_fs;
			renderpass = vk.render_pass.smaa_edge;
			layout = vk.pipeline_layout_smaa;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "smaa edge pipeline";
			target_format = vk.color_format;
			blend = qfalse;
			break;
		case 11: // smaa blend
			pipeline = &vk.smaa_blend_pipeline;
			fsmodule = vk.modules.smaa_blend_fs;
			renderpass = vk.render_pass.smaa_blend;
			layout = vk.pipeline_layout_smaa;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "smaa blend pipeline";
			target_format = vk.color_format;
			blend = qfalse;
			break;
		case 12: // smaa compose
			pipeline = &vk.smaa_compose_pipeline;
			fsmodule = vk.modules.smaa_compose_fs;
			renderpass = vk.render_pass.smaa_compose;
			layout = vk.pipeline_layout_smaa;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "smaa compose pipeline";
			target_format = vk.color_format;
			blend = qfalse;
			break;
#ifdef VK_PBR_BRDFLUT
        case 4: // generate brdf LUT
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
		default: // gamma correction
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
	set_shader_stage_desc( shader_stages+1, VK_SHADER_STAGE_FRAGMENT_BIT, fsmodule, "main" );

	frag_spec_data.gamma = 1.0f / (r_gamma->value);
	frag_spec_data.preExposureScale = 1.0f;
	frag_spec_data.greyscale = r_greyscale->value;
	frag_spec_data.bloom_threshold = r_bloom_threshold->value;
	frag_spec_data.bloom_intensity = r_bloom_intensity->value;
	frag_spec_data.bloom_threshold_mode = r_bloom_threshold_mode->integer;
	frag_spec_data.bloom_modulate = r_bloom_modulate->integer;
	frag_spec_data.dither = r_dither->integer;
	frag_spec_data.exposure = r_exposure ? r_exposure->value : 1.0f;
	frag_spec_data.bloom_knee = r_bloomKnee ? r_bloomKnee->value : 0.5f;
	frag_spec_data.tonemap_mode = r_tonemap ? r_tonemap->integer : 2;
	frag_spec_data.apply_srgb_gamma = vk_format_is_srgb( target_format ) ? 0 : 1;
	frag_spec_data.post_debug = r_post_debug ? r_post_debug->integer : 0;
	frag_spec_data.vignette_intensity = PostFX_GetVignetteIntensity();
	frag_spec_data.vignette_radius = PostFX_GetVignetteRadius();
	frag_spec_data.chromatic_aberration = PostFX_GetChromaticAberration();
	frag_spec_data.film_grain = PostFX_GetFilmGrain();
	frag_spec_data.postprocess_enabled = ( r_post && r_post->integer ) ? 1 : 0;

	if ( !vk_surface_format_color_depth( vk.present_format.format, &frag_spec_data.depth_r, &frag_spec_data.depth_g, &frag_spec_data.depth_b ) )
		ri.Printf( PRINT_ALL, "Format %s not recognized, dither to assume 8bpc\n", vk_format_string( vk.base_format.format ) );

	spec_entries[0].constantID = 0;
	spec_entries[0].offset = offsetof( struct PostProcess_FragSpecData, gamma );
	spec_entries[0].size = sizeof( frag_spec_data.gamma );

	spec_entries[1].constantID = 1;
	spec_entries[1].offset = offsetof( struct PostProcess_FragSpecData, preExposureScale );
	spec_entries[1].size = sizeof( frag_spec_data.preExposureScale );

	spec_entries[2].constantID = 2;
	spec_entries[2].offset = offsetof( struct PostProcess_FragSpecData, greyscale );
	spec_entries[2].size = sizeof( frag_spec_data.greyscale );

	spec_entries[3].constantID = 3;
	spec_entries[3].offset = offsetof( struct PostProcess_FragSpecData, bloom_threshold );
	spec_entries[3].size = sizeof( frag_spec_data.bloom_threshold );

	spec_entries[4].constantID = 4;
	spec_entries[4].offset = offsetof( struct PostProcess_FragSpecData, bloom_intensity );
	spec_entries[4].size = sizeof( frag_spec_data.bloom_intensity );

	spec_entries[5].constantID = 5;
	spec_entries[5].offset = offsetof( struct PostProcess_FragSpecData, bloom_threshold_mode );
	spec_entries[5].size = sizeof( frag_spec_data.bloom_threshold_mode );

	spec_entries[6].constantID = 6;
	spec_entries[6].offset = offsetof( struct PostProcess_FragSpecData, bloom_modulate );
	spec_entries[6].size = sizeof( frag_spec_data.bloom_modulate );

	spec_entries[7].constantID = 7;
	spec_entries[7].offset = offsetof( struct PostProcess_FragSpecData, dither );
	spec_entries[7].size = sizeof( frag_spec_data.dither );

	spec_entries[8].constantID = 8;
	spec_entries[8].offset = offsetof( struct PostProcess_FragSpecData, depth_r );
	spec_entries[8].size = sizeof( frag_spec_data.depth_r );

	spec_entries[9].constantID = 9;
	spec_entries[9].offset = offsetof( struct PostProcess_FragSpecData, depth_g );
	spec_entries[9].size = sizeof( frag_spec_data.depth_g );

	spec_entries[10].constantID = 10;
	spec_entries[10].offset = offsetof( struct PostProcess_FragSpecData, depth_b );
	spec_entries[10].size = sizeof( frag_spec_data.depth_b );

	spec_entries[11].constantID = 11;
	spec_entries[11].offset = offsetof( struct PostProcess_FragSpecData, exposure );
	spec_entries[11].size = sizeof( frag_spec_data.exposure );

	spec_entries[12].constantID = 12;
	spec_entries[12].offset = offsetof( struct PostProcess_FragSpecData, bloom_knee );
	spec_entries[12].size = sizeof( frag_spec_data.bloom_knee );

	spec_entries[13].constantID = 13;
	spec_entries[13].offset = offsetof( struct PostProcess_FragSpecData, tonemap_mode );
	spec_entries[13].size = sizeof( frag_spec_data.tonemap_mode );

	spec_entries[14].constantID = 14;
	spec_entries[14].offset = offsetof( struct PostProcess_FragSpecData, apply_srgb_gamma );
	spec_entries[14].size = sizeof( frag_spec_data.apply_srgb_gamma );

	spec_entries[15].constantID = 15;
	spec_entries[15].offset = offsetof( struct PostProcess_FragSpecData, post_debug );
	spec_entries[15].size = sizeof( frag_spec_data.post_debug );

	spec_entries[16].constantID = 16;
	spec_entries[16].offset = offsetof( struct PostProcess_FragSpecData, vignette_intensity );
	spec_entries[16].size = sizeof( frag_spec_data.vignette_intensity );

	spec_entries[17].constantID = 17;
	spec_entries[17].offset = offsetof( struct PostProcess_FragSpecData, vignette_radius );
	spec_entries[17].size = sizeof( frag_spec_data.vignette_radius );

	spec_entries[18].constantID = 18;
	spec_entries[18].offset = offsetof( struct PostProcess_FragSpecData, chromatic_aberration );
	spec_entries[18].size = sizeof( frag_spec_data.chromatic_aberration );

	spec_entries[19].constantID = 19;
	spec_entries[19].offset = offsetof( struct PostProcess_FragSpecData, film_grain );
	spec_entries[19].size = sizeof( frag_spec_data.film_grain );

	spec_entries[20].constantID = 20;
	spec_entries[20].offset = offsetof( struct PostProcess_FragSpecData, postprocess_enabled );
	spec_entries[20].size = sizeof( frag_spec_data.postprocess_enabled );

	frag_spec_info.mapEntryCount = 21;
	frag_spec_info.pMapEntries = spec_entries;
	frag_spec_info.dataSize = sizeof( frag_spec_data );
	frag_spec_info.pData = &frag_spec_data;

	shader_stages[1].pSpecializationInfo = &frag_spec_info;
	if ( program_index >= 5 ) {
		shader_stages[1].pSpecializationInfo = NULL;
	}

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
	if ( program_index == 0 ) {
		// gamma correction
		viewport.x = 0.0 + vk.blitX0;
		viewport.y = 0.0 + vk.blitY0;
		viewport.width = gls.windowWidth - vk.blitX0 * 2;
		viewport.height = gls.windowHeight - vk.blitY0 * 2;
	} else {
		// other post-processing
		viewport.x = 0.0;
		viewport.y = 0.0;
		viewport.width = width;
		viewport.height = height;
	}

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

	dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.pNext = NULL;
	dynamic_state.flags = 0;
	dynamic_state.dynamicStateCount = ARRAY_LEN( dynamic_state_array );
	dynamic_state.pDynamicStates = dynamic_state_array;

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
	multisample_state.rasterizationSamples = samples;
	multisample_state.sampleShadingEnable = VK_FALSE;
	multisample_state.minSampleShading = 1.0f;
	multisample_state.pSampleMask = NULL;
	multisample_state.alphaToCoverageEnable = VK_FALSE;
	multisample_state.alphaToOneEnable = VK_FALSE;

	Com_Memset(&attachment_blend_state, 0, sizeof(attachment_blend_state));
	attachment_blend_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	if ( blend ) {
		attachment_blend_state.blendEnable = VK_TRUE;
		attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
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
	blend_state.attachmentCount = 1;
	blend_state.pAttachments = &attachment_blend_state;
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
	create_info.pDepthStencilState = (program_index == 2) ? &depth_stencil_state : NULL;
	create_info.pDepthStencilState = &depth_stencil_state;
	create_info.pColorBlendState = &blend_state;
	create_info.pDynamicState = &dynamic_state;
	create_info.layout = layout;
	create_info.renderPass = renderpass;
	create_info.subpass = 0;
	create_info.basePipelineHandle = VK_NULL_HANDLE;
	create_info.basePipelineIndex = -1;

	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &create_info, NULL, pipeline ) );

	SET_OBJECT_NAME( *pipeline, pipeline_name, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
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
	VkDynamicState dynamic_state_array[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
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
			fs_module = &vk.modules.frag.light[0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_LIGHTING_LINEAR:
			vs_module = &vk.modules.vert.light[0];
			fs_module = &vk.modules.frag.light[1][0];
			break;

		case TYPE_SIGNLE_TEXTURE_DF:
			state_bits |= GLS_DEPTHMASK_TRUE;
			vs_module = &vk.modules.vert.ident1[use_pbr][0][0][0];
			fs_module = &vk.modules.frag.gen0_df;
			break;

		case TYPE_SIGNLE_TEXTURE_FIXED_COLOR:
			vs_module = &vk.modules.vert.fixed[use_pbr][0][0][0];
			fs_module = &vk.modules.frag.fixed[use_pbr][0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_FIXED_COLOR_ENV:
			vs_module = &vk.modules.vert.fixed[use_pbr][0][1][0];
			fs_module = &vk.modules.frag.fixed[use_pbr][0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_ENT_COLOR:
			vs_module = &vk.modules.vert.fixed[use_pbr][0][0][0];
			fs_module = &vk.modules.frag.ent[use_pbr][0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_ENT_COLOR_ENV:
			vs_module = &vk.modules.vert.fixed[use_pbr][0][1][0];
			fs_module = &vk.modules.frag.ent[use_pbr][0][0];
			break;

		case TYPE_SIGNLE_TEXTURE:
			vs_module = &vk.modules.vert.gen[use_pbr][0][0][0][0];
			fs_module = &vk.modules.frag.gen[use_pbr][0][0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_ENV:
			vs_module = &vk.modules.vert.gen[use_pbr][0][0][1][0];
			fs_module = &vk.modules.frag.gen[use_pbr][0][0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_IDENTITY:
			vs_module = &vk.modules.vert.ident1[use_pbr][0][0][0];
			fs_module = &vk.modules.frag.ident1[use_pbr][0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_IDENTITY_ENV:
			vs_module = &vk.modules.vert.ident1[use_pbr][0][1][0];
			fs_module = &vk.modules.frag.ident1[use_pbr][0][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY:
		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY:
			vs_module = &vk.modules.vert.ident1[use_pbr][1][0][0];
			fs_module = &vk.modules.frag.ident1[use_pbr][1][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV:
		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV:
			vs_module = &vk.modules.vert.ident1[use_pbr][1][1][0];
			fs_module = &vk.modules.frag.ident1[use_pbr][1][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR:
			vs_module = &vk.modules.vert.fixed[use_pbr][1][0][0];
			fs_module = &vk.modules.frag.fixed[use_pbr][1][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV:
			vs_module = &vk.modules.vert.fixed[use_pbr][1][1][0];
			fs_module = &vk.modules.frag.fixed[use_pbr][1][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL2:
		case TYPE_MULTI_TEXTURE_ADD2_1_1:
		case TYPE_MULTI_TEXTURE_ADD2:
			vs_module = &vk.modules.vert.gen[use_pbr][1][0][0][0];
			fs_module = &vk.modules.frag.gen[use_pbr][1][0][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL2_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_ENV:
			vs_module = &vk.modules.vert.gen[use_pbr][1][0][1][0];
			fs_module = &vk.modules.frag.gen[use_pbr][1][0][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL3:
		case TYPE_MULTI_TEXTURE_ADD3_1_1:
		case TYPE_MULTI_TEXTURE_ADD3:
			vs_module = &vk.modules.vert.gen[use_pbr][2][0][0][0];
			fs_module = &vk.modules.frag.gen[use_pbr][2][0][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL3_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_ENV:
			vs_module = &vk.modules.vert.gen[use_pbr][2][0][1][0];
			fs_module = &vk.modules.frag.gen[use_pbr][2][0][0];
			break;

		case TYPE_BLEND2_ADD:
		case TYPE_BLEND2_MUL:
		case TYPE_BLEND2_ALPHA:
		case TYPE_BLEND2_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_MIX_ALPHA:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA:
			vs_module = &vk.modules.vert.gen[use_pbr][1][1][0][0];
			fs_module = &vk.modules.frag.gen[use_pbr][1][1][0];
			break;

		case TYPE_BLEND2_ADD_ENV:
		case TYPE_BLEND2_MUL_ENV:
		case TYPE_BLEND2_ALPHA_ENV:
		case TYPE_BLEND2_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND2_MIX_ALPHA_ENV:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV:
			vs_module = &vk.modules.vert.gen[use_pbr][1][1][1][0];
			fs_module = &vk.modules.frag.gen[use_pbr][1][1][0];
			break;

		case TYPE_BLEND3_ADD:
		case TYPE_BLEND3_MUL:
		case TYPE_BLEND3_ALPHA:
		case TYPE_BLEND3_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_MIX_ALPHA:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA:
			vs_module = &vk.modules.vert.gen[use_pbr][2][1][0][0];
			fs_module = &vk.modules.frag.gen[use_pbr][2][1][0];
			break;

		case TYPE_BLEND3_ADD_ENV:
		case TYPE_BLEND3_MUL_ENV:
		case TYPE_BLEND3_ALPHA_ENV:
		case TYPE_BLEND3_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV:
			vs_module = &vk.modules.vert.gen[use_pbr][2][1][1][0];
			fs_module = &vk.modules.frag.gen[use_pbr][2][1][0];
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
			vs_module = &vk.modules.vert.gen[0][0][0][0];
			fs_module = &vk.modules.frag.gen[0][0][0];
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

	if ( def->vk_pbr_flags & PBR_HAS_NORMALMAP )
		frag_spec_data.normal_texture_set = 0;

	if ( def->vk_pbr_flags & PBR_HAS_SPECULARMAP )
		frag_spec_data.physical_texture_set = 1;
	else if ( def->vk_pbr_flags & PBR_HAS_PHYSICALMAP )
		frag_spec_data.physical_texture_set = 0;

	if ( vk.cubemapActive )
		frag_spec_data.env_texture_set = 0;

	if ( def->vk_pbr_flags & PBR_HAS_LIGHTMAP )
		frag_spec_data.lightmap_texture_set = 0;

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
#ifdef USE_REVERSED_DEPTH
		rasterization_state.depthBiasConstantFactor = -r_offsetUnits->value;
		rasterization_state.depthBiasSlopeFactor = -r_offsetFactor->value;
#else
		rasterization_state.depthBiasConstantFactor = r_offsetUnits->value;
		rasterization_state.depthBiasSlopeFactor = r_offsetFactor->value;
#endif
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

	multisample_state.sampleShadingEnable = VK_FALSE;
	multisample_state.minSampleShading = 1.0f;
	multisample_state.pSampleMask = NULL;
	multisample_state.alphaToCoverageEnable = alphaToCoverage;
	multisample_state.alphaToOneEnable = VK_FALSE;

	Com_Memset( &depth_stencil_state, 0, sizeof( depth_stencil_state ) );

	depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil_state.pNext = NULL;
	depth_stencil_state.flags = 0;
	depth_stencil_state.depthTestEnable = (state_bits & GLS_DEPTHTEST_DISABLE) ? VK_FALSE : VK_TRUE;
	depth_stencil_state.depthWriteEnable = (state_bits & GLS_DEPTHMASK_TRUE) ? VK_TRUE : VK_FALSE;
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

	if (def->shadow_phase == SHADOW_EDGES || def->shader_type == TYPE_SIGNLE_TEXTURE_DF)
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

		if ( def->allow_discard ) {
			// try to reduce pixel fillrate for transparent surfaces, this yields 1..10% fps increase when multisampling in enabled
			if ( attachment_blend_state.srcColorBlendFactor == VK_BLEND_FACTOR_SRC_ALPHA && attachment_blend_state.dstColorBlendFactor == VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA ) {
				frag_spec_data.discard_mode = 1;
			} else if ( attachment_blend_state.srcColorBlendFactor == VK_BLEND_FACTOR_ONE && attachment_blend_state.dstColorBlendFactor == VK_BLEND_FACTOR_ONE ) {
				frag_spec_data.discard_mode = 2;
			}
		}
	}

	main_motion_target = ( r_fbo->integer &&
		( renderPassIndex == RENDER_PASS_MAIN || renderPassIndex == RENDER_PASS_POST_BLOOM ) ) ? VK_TRUE : VK_FALSE;
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
	dynamic_state.dynamicStateCount = ARRAY_LEN( dynamic_state_array );
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


static void get_viewport_rect(VkRect2D *r)
{
	if ( backEnd.projection2D )
	{
		r->offset.x = 0;
		r->offset.y = 0;
		r->extent.width = vk.renderWidth;
		r->extent.height = vk.renderHeight;
	}
	else
	{
		r->offset.x = backEnd.viewParms.viewportX * vk.renderScaleX;
		r->offset.y = vk.renderHeight - (backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight) * vk.renderScaleY;
		r->extent.width = (float)backEnd.viewParms.viewportWidth * vk.renderScaleX;
		r->extent.height = (float)backEnd.viewParms.viewportHeight * vk.renderScaleY;
	}
}

static void get_viewport(VkViewport *viewport, Vk_Depth_Range depth_range) {
	VkRect2D r;

	get_viewport_rect( &r );

	viewport->x = (float)r.offset.x;
	viewport->y = (float)r.offset.y;
	viewport->width = (float)r.extent.width;
	viewport->height = (float)r.extent.height;

	switch ( depth_range ) {
		default:
#ifdef USE_REVERSED_DEPTH
		//case DEPTH_RANGE_NORMAL:
			viewport->minDepth = 0.0f;
			viewport->maxDepth = 1.0f;
			break;
		case DEPTH_RANGE_ZERO:
			viewport->minDepth = 1.0f;
			viewport->maxDepth = 1.0f;
			break;
		case DEPTH_RANGE_ONE:
			viewport->minDepth = 0.0f;
			viewport->maxDepth = 0.0f;
			break;
		case DEPTH_RANGE_WEAPON:
			viewport->minDepth = 0.6f;
			viewport->maxDepth = 1.0f;
			break;
#else
		//case DEPTH_RANGE_NORMAL:
			viewport->minDepth = 0.0f;
			viewport->maxDepth = 1.0f;
			break;
		case DEPTH_RANGE_ZERO:
			viewport->minDepth = 0.0f;
			viewport->maxDepth = 0.0f;
			break;
		case DEPTH_RANGE_ONE:
			viewport->minDepth = 1.0f;
			viewport->maxDepth = 1.0f;
			break;
		case DEPTH_RANGE_WEAPON:
			viewport->minDepth = 0.0f;
			viewport->maxDepth = 0.3f;
			break;
#endif
	}
}

static void get_scissor_rect(VkRect2D *r) {

	if ( backEnd.viewParms.portalView != PV_NONE )
	{
		r->offset.x = backEnd.viewParms.scissorX;
		r->offset.y = glConfig.vidHeight - backEnd.viewParms.scissorY - backEnd.viewParms.scissorHeight;
		r->extent.width = backEnd.viewParms.scissorWidth;
		r->extent.height = backEnd.viewParms.scissorHeight;
	}
	else
	{
		get_viewport_rect(r);

		if (r->offset.x < 0)
			r->offset.x = 0;
		if (r->offset.y < 0)
			r->offset.y = 0;

		if ( (uint32_t)r->offset.x + r->extent.width > (uint32_t)glConfig.vidWidth )
			r->extent.width = (uint32_t)glConfig.vidWidth - r->offset.x;
		if ( (uint32_t)r->offset.y + r->extent.height > (uint32_t)glConfig.vidHeight )
			r->extent.height = (uint32_t)glConfig.vidHeight - r->offset.y;
	}
}


typedef struct vkMvpPushConstants_s {
	float mvp[16];
	float prev_mvp[16];
} vkMvpPushConstants_t;

static void vk_get_projection_matrix_vk( const float *projection_matrix, float *projection_vk )
{
	Com_Memcpy( projection_vk, projection_matrix, sizeof( float ) * 16 );
	projection_vk[5] = -projection_matrix[5];
}

static void get_mvp_transform( float *mvp )
{
	if ( backEnd.projection2D )
	{
		float mvp0 = 2.0f / glConfig.vidWidth;
		float mvp5 = 2.0f / glConfig.vidHeight;

		mvp[0]  =  mvp0; mvp[1]  =  0.0f; mvp[2]  = 0.0f; mvp[3]  = 0.0f;
		mvp[4]  =  0.0f; mvp[5]  =  mvp5; mvp[6]  = 0.0f; mvp[7]  = 0.0f;
#ifdef USE_REVERSED_DEPTH
		mvp[8]  =  0.0f; mvp[9]  =  0.0f; mvp[10] = 0.0f; mvp[11] = 0.0f;
		mvp[12] = -1.0f; mvp[13] = -1.0f; mvp[14] = 1.0f; mvp[15] = 1.0f;
#else
		mvp[8]  =  0.0f; mvp[9]  =  0.0f; mvp[10] = 1.0f; mvp[11] = 0.0f;
		mvp[12] = -1.0f; mvp[13] = -1.0f; mvp[14] = 0.0f; mvp[15] = 1.0f;
#endif
	}
	else
	{
		float proj[16];
		vk_get_projection_matrix_vk( backEnd.viewParms.projectionMatrix, proj );
		myGlMultMatrix( vk_world.modelview_transform, proj, mvp );
	}
}

static void vk_begin_motion_frame( void )
{
	for ( int i = 0; i < MAX_REFENTITIES; i++ ) {
		if ( vk_curr_entity_model_valid[i] ) {
			Com_Memcpy( vk_prev_entity_model_matrices[i], vk_curr_entity_model_matrices[i], sizeof( vk_prev_entity_model_matrices[i] ) );
			vk_prev_entity_model_handles[i] = vk_curr_entity_model_handles[i];
			vk_prev_entity_types[i] = vk_curr_entity_types[i];
			vk_prev_entity_model_valid[i] = qtrue;
		} else {
			vk_prev_entity_model_valid[i] = qfalse;
		}
		vk_curr_entity_model_valid[i] = qfalse;
	}
}

static void vk_reset_motion_history( void )
{
	Com_Memset( vk_prev_entity_model_matrices, 0, sizeof( vk_prev_entity_model_matrices ) );
	Com_Memset( vk_curr_entity_model_matrices, 0, sizeof( vk_curr_entity_model_matrices ) );
	Com_Memset( vk_prev_entity_model_handles, 0, sizeof( vk_prev_entity_model_handles ) );
	Com_Memset( vk_curr_entity_model_handles, 0, sizeof( vk_curr_entity_model_handles ) );
	Com_Memset( vk_prev_entity_types, 0, sizeof( vk_prev_entity_types ) );
	Com_Memset( vk_curr_entity_types, 0, sizeof( vk_curr_entity_types ) );
	Com_Memset( vk_prev_entity_model_valid, 0, sizeof( vk_prev_entity_model_valid ) );
	Com_Memset( vk_curr_entity_model_valid, 0, sizeof( vk_curr_entity_model_valid ) );
}

static int vk_get_current_entity_motion_index( void )
{
	const trRefEntity_t *ent = backEnd.currentEntity;
	const trRefEntity_t *base = backEnd.refdef.entities;

	if ( !ent || ent == &tr.worldEntity || !base || backEnd.refdef.num_entities <= 0 ) {
		return -1;
	}
	if ( ent < base || ent >= base + backEnd.refdef.num_entities ) {
		return -1;
	}
	return (int)( ent - base );
}

static qboolean vk_entity_requires_no_motion( const trRefEntity_t *ent )
{
	// Until previous-frame skinning/deform data is available in Vulkan, animated entities
	// explicitly output zero velocity to avoid undefined or ghosting motion vectors.
	if ( !ent ) {
		return qfalse;
	}
	if ( ent->e.frame != ent->e.oldframe ) {
		return qtrue;
	}
	if ( ent->e.backlerp > 0.001f ) {
		return qtrue;
	}
	return qfalse;
}

static void get_prev_mvp_transform( float *prev_mvp )
{
	float prev_model[16];
	float prev_model_view[16];
	float prev_proj[16];
	int motion_index;

	if ( backEnd.projection2D || !vk_prev_matrices_valid ) {
		get_mvp_transform( prev_mvp );
		return;
	}

	Com_Memcpy( prev_model, backEnd.or.modelMatrix, sizeof( prev_model ) );

	motion_index = vk_get_current_entity_motion_index();
	if ( motion_index >= 0 && backEnd.currentEntity && backEnd.currentEntity->e.reType == RT_MODEL ) {
		if ( !vk_curr_entity_model_valid[motion_index] ) {
			Com_Memcpy( vk_curr_entity_model_matrices[motion_index], backEnd.or.modelMatrix, sizeof( vk_curr_entity_model_matrices[motion_index] ) );
			vk_curr_entity_model_handles[motion_index] = backEnd.currentEntity->e.hModel;
			vk_curr_entity_types[motion_index] = (int)backEnd.currentEntity->e.reType;
			vk_curr_entity_model_valid[motion_index] = qtrue;
		}

		if ( !vk_entity_requires_no_motion( backEnd.currentEntity ) &&
			vk_prev_entity_model_valid[motion_index] &&
			vk_prev_entity_model_handles[motion_index] == backEnd.currentEntity->e.hModel &&
			vk_prev_entity_types[motion_index] == (int)backEnd.currentEntity->e.reType )
		{
			// Use full previous model matrix (translation + rotation + scale) for rigid motion.
			Com_Memcpy( prev_model, vk_prev_entity_model_matrices[motion_index], sizeof( prev_model ) );
		}
	}

	myGlMultMatrix( prev_model, vk_prev_view_matrix, prev_model_view );
	vk_get_projection_matrix_vk( vk_prev_projection_matrix, prev_proj );
	myGlMultMatrix( prev_model_view, prev_proj, prev_mvp );
}


void vk_clear_color( const vec4_t color ) {

	VkClearAttachment attachment;
	VkClearRect clear_rect;

	if ( !vk.active )
		return;

	attachment.colorAttachment = 0;
	attachment.clearValue.color.float32[0] = color[0];
	attachment.clearValue.color.float32[1] = color[1];
	attachment.clearValue.color.float32[2] = color[2];
	attachment.clearValue.color.float32[3] = color[3];
	attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	get_scissor_rect( &clear_rect.rect );
	clear_rect.baseArrayLayer = 0;
	clear_rect.layerCount = 1;

	qvkCmdClearAttachments( vk.cmd->command_buffer, 1, &attachment, 1, &clear_rect );
}


void vk_clear_depth( qboolean clear_stencil ) {

	VkClearAttachment attachment;
	VkClearRect clear_rect[1];

	if ( !vk.active )
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

	get_scissor_rect( &clear_rect[0].rect );
	clear_rect[0].baseArrayLayer = 0;
	clear_rect[0].layerCount = 1;

	qvkCmdClearAttachments( vk.cmd->command_buffer, 1, &attachment, 1, clear_rect );
}


void vk_update_mvp( const float *m ) {
	vkMvpPushConstants_t push_constants;

	//
	// Specify push constants.
	//
	if ( m ) {
		Com_Memcpy( push_constants.mvp, m, sizeof( push_constants.mvp ) );
	} else {
		get_mvp_transform( push_constants.mvp );
	}
	get_prev_mvp_transform( push_constants.prev_mvp );

	qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof( push_constants ), &push_constants );

	vk.stats.push_size += sizeof( push_constants );
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
	uint32_t offsets[3], offset_count;
	uint32_t start, end, count, i;

	start = vk.cmd->descriptor_set.start;
	if ( start == ~0U )
		return;

	end = vk.cmd->descriptor_set.end;

	offset_count = 0;
	if ( /*start == VK_DESC_STORAGE || */ start == VK_DESC_UNIFORM ) { // uniform offset or storage offset
		offsets[ offset_count++ ] = vk.cmd->descriptor_set.offset[ start ];
		offsets[offset_count++] = vk.cmd->descriptor_set.offset[start+1]; // camera uniform
	}

	count = end - start + 1;

	// fill NULL descriptor gaps
	for ( i = start + 1; i < end; i++ ) {
		if ( vk.cmd->descriptor_set.current[i] == VK_NULL_HANDLE ) {
			vk.cmd->descriptor_set.current[i] = tr.whiteImage->descriptor;
			vk.cmd->descriptor_set.image[i] = tr.whiteImage;
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

	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout, start, count, vk.cmd->descriptor_set.current + start, offset_count, offsets );

	vk.cmd->descriptor_set.end = 0;
	vk.cmd->descriptor_set.start = ~0U;
}


void vk_bind_pipeline( uint32_t pipeline ) {
	VkPipeline vkpipe;

	vkpipe = vk_gen_pipeline( pipeline );

	if ( vkpipe != vk.cmd->last_pipeline ) {
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkpipe );
		vk.cmd->last_pipeline = vkpipe;
	}

	vk_world.dirty_depth_attachment |= ( vk.pipelines[ pipeline ].def.state_bits & GLS_DEPTHMASK_TRUE );
}

static void vk_update_depth_range( Vk_Depth_Range depth_range )
{
	if ( vk.cmd->depth_range != depth_range ) {
		VkRect2D scissor_rect;
		VkViewport viewport;

		vk.cmd->depth_range = depth_range;

		get_scissor_rect( &scissor_rect );

		if ( memcmp( &vk.cmd->scissor_rect, &scissor_rect, sizeof( scissor_rect ) ) != 0 ) {
			qvkCmdSetScissor( vk.cmd->command_buffer, 0, 1, &scissor_rect );
			vk.cmd->scissor_rect = scissor_rect;
		}

		get_viewport( &viewport, depth_range );
		qvkCmdSetViewport( vk.cmd->command_buffer, 0, 1, &viewport );
	}

	// Track the largest 3D render viewport that populates the scene color source image.
	if ( !backEnd.projection2D &&
		( vk.renderPassIndex == RENDER_PASS_MAIN || vk.renderPassIndex == RENDER_PASS_POST_BLOOM ) )
	{
		VkRect2D r;
		uint32_t maxW = ( glConfig.vidWidth > 0 ) ? (uint32_t)glConfig.vidWidth : 1u;
		uint32_t maxH = ( glConfig.vidHeight > 0 ) ? (uint32_t)glConfig.vidHeight : 1u;
		uint64_t area;
		uint64_t bestArea;

		get_viewport_rect( &r );

		if ( r.offset.x < 0 ) {
			int dx = -r.offset.x;
			r.offset.x = 0;
			r.extent.width = ( r.extent.width > (uint32_t)dx ) ? ( r.extent.width - (uint32_t)dx ) : 0u;
		}
		if ( r.offset.y < 0 ) {
			int dy = -r.offset.y;
			r.offset.y = 0;
			r.extent.height = ( r.extent.height > (uint32_t)dy ) ? ( r.extent.height - (uint32_t)dy ) : 0u;
		}
		if ( (uint32_t)r.offset.x >= maxW || (uint32_t)r.offset.y >= maxH ) {
			return;
		}
		if ( (uint32_t)r.offset.x + r.extent.width > maxW ) {
			r.extent.width = maxW - (uint32_t)r.offset.x;
		}
		if ( (uint32_t)r.offset.y + r.extent.height > maxH ) {
			r.extent.height = maxH - (uint32_t)r.offset.y;
		}

		area = (uint64_t)r.extent.width * (uint64_t)r.extent.height;
		bestArea = vk_scene_src_rect_valid ? (uint64_t)vk_scene_src_rect.extent.width * (uint64_t)vk_scene_src_rect.extent.height : 0u;
		if ( !vk_scene_src_rect_valid || area > bestArea ) {
			vk_scene_src_rect = r;
			vk_scene_src_rect_valid = qtrue;
		}
	}
}


void vk_draw_geometry( Vk_Depth_Range depth_range, qboolean indexed ) {

	if ( vk.geometry_buffer_size_new ) {
		// geometry buffer overflow happened this frame
		return;
	}

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

	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_storage, VK_DESC_STORAGE, 1, &vk.storage.descriptor, 1, &storage_offset );

	// configure pipeline's dynamic state
	vk_update_depth_range( DEPTH_RANGE_NORMAL );

	qvkCmdDraw( vk.cmd->command_buffer, tess.numVertexes, 1, 0, 0 );
}


static qboolean vk_in_render_pass;

static void vk_set_fullscreen_viewport_scissor( uint32_t width, uint32_t height )
{
	VkViewport viewport;
	VkRect2D scissor;

	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)width;
	viewport.height = (float)height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = width;
	scissor.extent.height = height;

	qvkCmdSetViewport( vk.cmd->command_buffer, 0, 1, &viewport );
	qvkCmdSetScissor( vk.cmd->command_buffer, 0, 1, &scissor );
}

static void vk_begin_render_pass( VkRenderPass renderPass, VkFramebuffer frameBuffer, qboolean clearValues, uint32_t width, uint32_t height )
{
	VkRenderPassBeginInfo render_pass_begin_info;
	VkClearValue clear_values[5];

	// Begin render pass.

	render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_begin_info.pNext = NULL;
	render_pass_begin_info.renderPass = renderPass;
	render_pass_begin_info.framebuffer = frameBuffer;
	render_pass_begin_info.renderArea.offset.x = 0;
	render_pass_begin_info.renderArea.offset.y = 0;
	render_pass_begin_info.renderArea.extent.width = width;
	render_pass_begin_info.renderArea.extent.height = height;

	if ( clearValues ) {
		uint32_t clear_count = 2;

		// attachments layout for main pass when FBO is enabled:
		// [0] resolve color, [1] depth, [2] motion resolve, [3] msaa color, [4] msaa motion
		Com_Memset( clear_values, 0, sizeof( clear_values ) );
		// Keep color clears black so uncovered regions are obvious and neutral in post-process debug.
		clear_values[0].color.float32[0] = 0.0f;
		clear_values[0].color.float32[1] = 0.0f;
		clear_values[0].color.float32[2] = 0.0f;
		clear_values[0].color.float32[3] = 1.0f;
#ifndef USE_REVERSED_DEPTH
		clear_values[1].depthStencil.depth = 1.0;
#endif
		if ( vk.renderPassIndex == RENDER_PASS_MAIN || vk.renderPassIndex == RENDER_PASS_POST_BLOOM ) {
			if ( r_fbo->integer ) {
				clear_values[2].color.float32[0] = 0.0f;
				clear_values[2].color.float32[1] = 0.0f;
				clear_values[2].color.float32[2] = 0.0f;
				clear_values[2].color.float32[3] = 0.0f;
				clear_count = vk.msaaActive ? 5 : 3;
			} else {
				clear_count = vk.msaaActive ? 3 : 2;
			}
		} else if ( vk.renderPassIndex == RENDER_PASS_SCREENMAP ) {
			clear_count = ( vk.screenMapSamples > VK_SAMPLE_COUNT_1_BIT ) ? 3 : 2;
		} else if ( vk.renderPassIndex == RENDER_PASS_SUN_SHADOW ) {
			clear_count = 2;
		} else {
			clear_count = vk.msaaActive ? 3 : 2;
		}
		render_pass_begin_info.clearValueCount = clear_count;
		render_pass_begin_info.pClearValues = clear_values;

		vk_world.dirty_depth_attachment = 0;
	} else {
		render_pass_begin_info.clearValueCount = 0;
		render_pass_begin_info.pClearValues = NULL;
	}

	qvkCmdBeginRenderPass( vk.cmd->command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE );
	vk_in_render_pass = qtrue;

	vk.cmd->last_pipeline = VK_NULL_HANDLE;
	vk.cmd->depth_range = DEPTH_RANGE_COUNT;
}


void vk_begin_main_render_pass( void )
{
	VkFramebuffer frameBuffer = vk.framebuffers.main[ vk.cmd->swapchain_image_index ];

	vk.renderPassIndex = RENDER_PASS_MAIN;

	vk.renderWidth = glConfig.vidWidth;
	vk.renderHeight = glConfig.vidHeight;

	//vk.renderScaleX = (float)vk.renderWidth / (float)glConfig.vidWidth;
	//vk.renderScaleY = (float)vk.renderHeight / (float)glConfig.vidHeight;
	vk.renderScaleX = vk.renderScaleY = 1.0f;
	vk_scene_src_rect_valid = qfalse;

	vk_begin_render_pass( vk.render_pass.main, frameBuffer, qtrue, vk.renderWidth, vk.renderHeight );
}


void vk_begin_post_bloom_render_pass( void )
{
	VkFramebuffer frameBuffer = vk.framebuffers.main[ vk.cmd->swapchain_image_index ];

	vk.renderPassIndex = RENDER_PASS_POST_BLOOM;

	vk.renderWidth = glConfig.vidWidth;
	vk.renderHeight = glConfig.vidHeight;

	//vk.renderScaleX = (float)vk.renderWidth / (float)glConfig.vidWidth;
	//vk.renderScaleY = (float)vk.renderHeight / (float)glConfig.vidHeight;
	vk.renderScaleX = vk.renderScaleY = 1.0f;

	vk_begin_render_pass( vk.render_pass.post_bloom, frameBuffer, qfalse, vk.renderWidth, vk.renderHeight );
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

	vk_begin_render_pass( vk.render_pass.bloom_extract, frameBuffer, qfalse, vk.renderWidth, vk.renderHeight );
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

	vk_begin_render_pass( vk.render_pass.blur[ index ], frameBuffer, qfalse, vk.renderWidth, vk.renderHeight );
}


void vk_begin_ssao_render_pass( void )
{
	VkFramebuffer frameBuffer = vk.framebuffers.ssao;

	vk.renderWidth = glConfig.vidWidth;
	vk.renderHeight = glConfig.vidHeight;
	vk.renderScaleX = vk.renderScaleY = 1.0f;

	vk_begin_render_pass( vk.render_pass.ssao, frameBuffer, qfalse, vk.renderWidth, vk.renderHeight );
}


void vk_begin_ssao_blur_render_pass( void )
{
	VkFramebuffer frameBuffer = vk.framebuffers.ssao_blur;

	vk.renderWidth = glConfig.vidWidth;
	vk.renderHeight = glConfig.vidHeight;
	vk.renderScaleX = vk.renderScaleY = 1.0f;

	vk_begin_render_pass( vk.render_pass.ssao_blur, frameBuffer, qfalse, vk.renderWidth, vk.renderHeight );
}


void vk_begin_ssao_combine_render_pass( void )
{
	VkFramebuffer frameBuffer = vk.framebuffers.ssao_combine;

	vk.renderWidth = glConfig.vidWidth;
	vk.renderHeight = glConfig.vidHeight;
	vk.renderScaleX = vk.renderScaleY = 1.0f;

	vk_begin_render_pass( vk.render_pass.ssao_combine, frameBuffer, qfalse, vk.renderWidth, vk.renderHeight );
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
	vk_begin_render_pass( vk.render_pass.sun_shadow, vk.framebuffers.sun_shadow, qtrue, vk.renderWidth, vk.renderHeight );

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
	vk_begin_render_pass( vk.render_pass.local_spot_shadow, vk.framebuffers.local_spot_shadow, qtrue, vk.renderWidth, vk.renderHeight );

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
	vk_begin_render_pass( vk.render_pass.local_point_shadow, vk.framebuffers.local_point_shadow[faceLayer], qtrue, vk.renderWidth, vk.renderHeight );

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

	vk_begin_render_pass( vk.render_pass.volumetric, frameBuffer, qfalse, vk.renderWidth, vk.renderHeight );
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
	const uint32_t query_base = vk.cmd_index * VK_VOLUMETRIC_QUERY_SLOTS;
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
			ri.Printf( PRINT_ALL,
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
	float nearPlane = ( r_znear ) ? r_znear->value : 4.0f;

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

static void vk_run_smaa_pass( VkPipeline pipeline, VkRenderPass pass, VkFramebuffer framebuffer, VkDescriptorSet color_descriptor, VkDescriptorSet aux_descriptor, uint32_t width, uint32_t height )
{
	if ( !pipeline || pass == VK_NULL_HANDLE || framebuffer == VK_NULL_HANDLE || vk.pipeline_layout_smaa == VK_NULL_HANDLE || color_descriptor == VK_NULL_HANDLE ) {
		return;
	}

	vk_begin_render_pass( pass, framebuffer, qfalse, width, height );

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );

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
	if ( !vk.smaaActive ) {
		return;
	}

	vk_run_smaa_pass( vk.smaa_edge_pipeline, vk.render_pass.smaa_edge, vk.framebuffers.smaa_edge, vk.smaa_edge_descriptor, vk.smaa_edge_descriptor, glConfig.vidWidth, glConfig.vidHeight );
	vk_run_smaa_pass( vk.smaa_blend_pipeline, vk.render_pass.smaa_blend, vk.framebuffers.smaa_blend, vk.smaa_edge_descriptor, vk.smaa_blend_descriptor, glConfig.vidWidth, glConfig.vidHeight );
	vk_run_smaa_pass( vk.smaa_compose_pipeline, vk.render_pass.smaa_compose, vk.framebuffers.smaa_compose, vk.smaa_edge_descriptor, vk.smaa_compose_descriptor, glConfig.vidWidth, glConfig.vidHeight );
}

static void vk_reset_volumetric_history( void )
{
	vk.has_prev_volumetric = qfalse;
	vk.volumetric_frame = 0;
	vk_prev_matrices_valid = qfalse;
	vk_prev_volumetric_time_valid = qfalse;
	vk_volumetric_noise_time = 0.0f;
	Com_Memset( &vk_volumetric_validation_state, 0, sizeof( vk_volumetric_validation_state ) );
}

static void vk_volumetric_fog_pass( void )
{
	if ( !r_volumetricFog->integer || backEnd.doneFog || !vk.fboActive ||
		!tr.world || ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) ) {
		vk_reset_volumetric_history();
		backEnd.doneFog = qtrue;
		return;
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
		vk.has_prev_volumetric = qfalse;
		vk_prev_matrices_valid = qfalse;
		vk_prev_volumetric_time_valid = qfalse;
		Com_Memset( &vk_volumetric_validation_state, 0, sizeof( vk_volumetric_validation_state ) );
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
		vk.has_prev_volumetric = qfalse;
		vk_prev_matrices_valid = qfalse;
		vk_prev_volumetric_time_valid = qfalse;
		Com_Memset( &vk_volumetric_validation_state, 0, sizeof( vk_volumetric_validation_state ) );
		return;
	}

	VkImageAspectFlags depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	if ( glConfig.stencilBits > 0 ) {
		depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	record_image_layout_transition( vk.cmd->command_buffer, vk.depth_image, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );

	vk_resolve_volumetric_depth_msaa();
	vk_update_volumetric_params();

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

	record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );

	if ( vk.smaaActive ) {
		vk_smaa_passes();
		if ( vk.smaa_output_image_view ) {
			vk_update_color_descriptor_image( vk.smaa_output_image_view );
		}
	} else {
		vk_update_color_descriptor_image( vk.color_image_view );
	}

	// Restore depth layout for the next frame's main render pass clears/attachments.
	record_image_layout_transition( vk.cmd->command_buffer, vk.depth_image, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
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

void vk_prepare_2d( void )
{
	// Run volumetrics before switching to 2D so UI/console/HUD are not fogged.
	if ( !vk.fboActive || !r_volumetricFog || !r_volumetricFog->integer || backEnd.doneFog ) {
		return;
	}
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE || !vk_in_render_pass ) {
		return;
	}
	if ( vk.cmd->swapchain_image_index >= MAX_SWAPCHAIN_IMAGES ) {
		return;
	}
	if ( vk.render_pass.post_bloom == VK_NULL_HANDLE ||
		vk.framebuffers.main[ vk.cmd->swapchain_image_index ] == VK_NULL_HANDLE ) {
		return;
	}

	// Only split the main scene pass.
	if ( vk.renderPassIndex != RENDER_PASS_MAIN && vk.renderPassIndex != RENDER_PASS_POST_BLOOM ) {
		return;
	}

	if ( !tr.world || ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) ) {
		vk_reset_volumetric_history();
		backEnd.doneFog = qtrue;
		vk_begin_post_bloom_render_pass();
		return;
	}

	vk_end_render_pass();
	vk_volumetric_fog_pass();

	// Resume drawing for 2D overlays.
	vk_begin_post_bloom_render_pass();
}


static void vk_begin_screenmap_render_pass( void )
{
	VkFramebuffer frameBuffer = vk.framebuffers.screenmap;

	vk.renderPassIndex = RENDER_PASS_SCREENMAP;

	vk.renderWidth = vk.screenMapWidth;
	vk.renderHeight = vk.screenMapHeight;

	vk.renderScaleX = (float)vk.renderWidth / (float)glConfig.vidWidth;
	vk.renderScaleY = (float)vk.renderHeight / (float)glConfig.vidHeight;

	vk_begin_render_pass( vk.render_pass.screenmap, frameBuffer, qtrue, vk.renderWidth, vk.renderHeight );
}

#ifdef VK_CUBEMAP
void vk_begin_cubemap_render_pass( void )
{
    VkFramebuffer frameBuffer = vk.framebuffers.cubemap[backEnd.viewParms.targetCubeLayer];

    vk.renderPassIndex = RENDER_PASS_CUBEMAP;

    vk.renderWidth = REF_CUBEMAP_SIZE;
    vk.renderHeight = REF_CUBEMAP_SIZE;
    vk.renderScaleX = vk.renderScaleY = 1.0f;

    vk_begin_render_pass(vk.render_pass.cubemap, frameBuffer, qtrue, vk.renderWidth, vk.renderHeight);
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

    command_buffer = begin_command_buffer();
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

    end_command_buffer( command_buffer, __func__  );
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
	if ( !vk_in_render_pass ) {
		return;
	}

	qvkCmdEndRenderPass( vk.cmd->command_buffer );
	vk_in_render_pass = qfalse;

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

	if ( vk.frame_count++ ) // might happen during stereo rendering
		return;

	if (PostFX_NeedsPipelineUpdate()) {
		vk_update_post_process_pipelines();
	}

	vk_begin_motion_frame();
	vk.sun_shadow_valid = qfalse;

#ifdef USE_UPLOAD_QUEUE
	vk_flush_staging_buffer( qtrue );
#endif

	vk.cmd = &vk.tess[ vk.cmd_index ];

	if ( vk.cmd->waitForFence ) {
		vk.cmd->waitForFence = qfalse;
		res = qvkWaitForFences( vk.device, 1, &vk.cmd->rendering_finished_fence, VK_FALSE, 1e10 );
		if ( res != VK_SUCCESS ) {
			if ( res == VK_ERROR_DEVICE_LOST ) {
				// silently discard previous command buffer
				ri.Printf( PRINT_WARNING, "Vulkan: %s returned %s", "vkWaitForFences", vk_result_string( res ) );
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

	if ( vk_find_screenmap_drawsurfs() ) {
		vk_begin_screenmap_render_pass();
	} else {
vk_begin_main_render_pass();
}


	// dynamic vertex buffer layout
	vk.cmd->uniform_read_offset = 0;
	vk.cmd->vertex_buffer_offset = 0;
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

	if ( vk.fboActive )
	{
		vk.cmd->last_pipeline = VK_NULL_HANDLE; // do not restore clobbered descriptors in vk_bloom()

		if ( r_bloom->integer )
		{
			vk_bloom();
		}

		if ( r_ssao && r_ssao->integer )
		{
			static qboolean warned_msaa = qfalse;

			if ( vk.msaaActive )
			{
				if ( !warned_msaa )
				{
					ri.Printf( PRINT_WARNING, "Vulkan: SSAO disabled while MSAA is enabled (no depth resolve yet)\n" );
					warned_msaa = qtrue;
				}
			}
			else
			{
				typedef struct {
					float projInfo[4]; // invProj00, invProj11, proj10, proj14
					float params[4];   // radius, bias, intensity, power
					float misc[4];     // samples, invWidth, invHeight, depthIsReversed
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

				record_image_layout_transition( vk.cmd->command_buffer, vk.depth_image, depth_aspect,
					VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
					VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, 0, 0 );

				// ssao
				vk_begin_ssao_render_pass();
				qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.ssao_pipeline );
				qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_ssao, 0, 1, &vk.depth_descriptor, 0, NULL );

				push.projInfo[0] = ( backEnd.viewParms.projectionMatrix[0] != 0.0f ) ? 1.0f / backEnd.viewParms.projectionMatrix[0] : 1.0f;
				push.projInfo[1] = ( backEnd.viewParms.projectionMatrix[5] != 0.0f ) ? 1.0f / backEnd.viewParms.projectionMatrix[5] : 1.0f;
				push.projInfo[2] = backEnd.viewParms.projectionMatrix[10];
				push.projInfo[3] = backEnd.viewParms.projectionMatrix[14];

				push.params[0] = r_ssaoRadius->value;
				push.params[1] = r_ssaoBias->value;
				push.params[2] = r_ssaoIntensity->value;
				push.params[3] = r_ssaoPower->value;

				push.misc[0] = (float)r_ssaoSamples->integer;
				push.misc[1] = ( glConfig.vidWidth > 0 ) ? 1.0f / (float)glConfig.vidWidth : 1.0f;
				push.misc[2] = ( glConfig.vidHeight > 0 ) ? 1.0f / (float)glConfig.vidHeight : 1.0f;
				push.misc[3] = depthIsReversed;

				qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout_ssao, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( push ), &push );
				vk_set_fullscreen_viewport_scissor( vk.renderWidth, vk.renderHeight );
				qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
				vk_end_render_pass();

				// ssao blur
				vk_begin_ssao_blur_render_pass();
				qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.ssao_blur_pipeline );
				qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_ssao, 0, 1, &vk.ssao_descriptor, 0, NULL );

				push.params[0] = (float)r_ssaoBlurRadius->integer;
				push.params[1] = 0.0f;
				push.params[2] = 0.0f;
				push.params[3] = 0.0f;
				qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout_ssao, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( push ), &push );
				vk_set_fullscreen_viewport_scissor( vk.renderWidth, vk.renderHeight );
				qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
				vk_end_render_pass();

				// ssao combine
				vk_begin_ssao_combine_render_pass();
				if ( r_ssaoDebugView && r_ssaoDebugView->integer == 2 ) {
					qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.ssao_depth_debug_pipeline );
					qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.depth_descriptor, 0, NULL );
					} else {
						qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
							( r_ssaoDebugView && r_ssaoDebugView->integer ) ? vk.ssao_debug_pipeline : vk.ssao_combine_pipeline );
						qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.ssao_blur_descriptor, 0, NULL );
					}
					vk_set_fullscreen_viewport_scissor( vk.renderWidth, vk.renderHeight );
					qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
					vk_end_render_pass();

					// SSAO samples depth as read-only; restore the depth image layout for later passes
					// and for the next frame's main render pass.
					record_image_layout_transition( vk.cmd->command_buffer, vk.depth_image, depth_aspect,
						VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
						VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0, 0 );
				}
			}

		if ( backEnd.screenshotMask && vk.capture.image )
		{
			vk_end_render_pass();

			// render to capture FBO
			vk_begin_render_pass( vk.render_pass.capture, vk.framebuffers.capture, qfalse, gls.captureWidth, gls.captureHeight );
			qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.capture_pipeline );
			qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.color_descriptor, 0, NULL );

			vk_set_fullscreen_viewport_scissor( gls.captureWidth, gls.captureHeight );
			qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
		}

		if ( !ri.CL_IsMinimized() )
		{
			vk_end_render_pass();

				if ( !backEnd.doneFog )
				{
					vk_volumetric_fog_pass();
				}

			vk.renderWidth = gls.windowWidth;
			vk.renderHeight = gls.windowHeight;

			vk.renderScaleX = 1.0;
			vk.renderScaleY = 1.0;

			vk_begin_render_pass( vk.render_pass.gamma, vk.framebuffers.gamma[ vk.cmd->swapchain_image_index ], qfalse, vk.renderWidth, vk.renderHeight );
			qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.gamma_pipeline );
			qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.color_descriptor, 0, NULL );

			VkPostProcessPushConstants panini_push = { 0 };
			uint32_t srcTexW = ( glConfig.vidWidth > 0 ) ? (uint32_t)glConfig.vidWidth : 1u;
			uint32_t srcTexH = ( glConfig.vidHeight > 0 ) ? (uint32_t)glConfig.vidHeight : 1u;
			VkRect2D srcRect;
			{
			int lensPreset = r_paniniLensPreset ? r_paniniLensPreset->integer : 0;
			float presetAmount = r_panini ? r_panini->value : 0.0f;
			float presetD = r_panini_d ? r_panini_d->value : 1.0f;
			float presetS = r_panini_s ? r_panini_s->value : 0.25f;
			float presetFov = r_panini_theta ? r_panini_theta->value : 90.0f;
			float presetZoom = r_panini_zoom ? r_panini_zoom->value : 1.0f;
			float presetBright = r_paniniBrightness ? r_paniniBrightness->value : 1.0f;

			switch (lensPreset) {
				case 1: presetAmount=1.0f; presetD=1.0f; presetS=0.2f; presetFov=120.0f; presetZoom=1.15f; presetBright=1.2f; break;
				case 2: presetAmount=1.0f; presetD=1.2f; presetS=0.35f; presetFov=150.0f; presetZoom=1.25f; presetBright=1.25f; break;
				case 3: presetAmount=0.0f; presetD=0.0f; presetS=0.0f; presetFov=90.0f; presetZoom=1.0f; break;
				case 4: presetAmount=0.4f; presetD=0.3f; presetS=0.05f; presetFov=84.0f; presetZoom=1.0f; break;
				case 5: presetAmount=0.2f; presetD=0.15f; presetS=0.02f; presetFov=63.0f; presetZoom=1.0f; break;
				case 6: presetAmount=1.0f; presetD=1.5f; presetS=0.5f; presetFov=170.0f; presetZoom=1.4f; presetBright=1.3f; break;
				case 7: presetAmount=0.8f; presetD=0.8f; presetS=0.15f; presetFov=110.0f; presetZoom=1.1f; break;
				default: break;
			}

			panini_push.paniniAmount = presetAmount;
			panini_push.paniniD = presetD;
			panini_push.paniniS = presetS;
			panini_push.fovXDeg = backEnd.viewParms.fovX > 1.0f ? backEnd.viewParms.fovX : presetFov;
			panini_push.paniniZoom = presetZoom;
			panini_push.brightness = presetBright;
			}
			panini_push.aspect = vk.renderHeight > 0 ? ( (float)vk.renderWidth / (float)vk.renderHeight ) : 1.0f;
			panini_push.paniniBorderMode = r_panini_border ? (float)r_panini_border->integer : 0.0f;
			panini_push.paniniDebugMode = r_panini_debug ? (float)r_panini_debug->integer : 0.0f;
			panini_push.paniniPad0 = 0.0f;
			panini_push.paniniPad1 = 0.0f;
			panini_push.paniniPad2 = 0.0f;
			if ( vk_scene_src_rect_valid ) {
				srcRect = vk_scene_src_rect;
			} else {
				srcRect.offset.x = 0;
				srcRect.offset.y = 0;
				srcRect.extent.width = srcTexW;
				srcRect.extent.height = srcTexH;
			}
			panini_push.srcUVScaleBias[0] = (float)srcRect.extent.width / (float)srcTexW;
			panini_push.srcUVScaleBias[1] = (float)srcRect.extent.height / (float)srcTexH;
			panini_push.srcUVScaleBias[2] = (float)srcRect.offset.x / (float)srcTexW;
			panini_push.srcUVScaleBias[3] = (float)srcRect.offset.y / (float)srcTexH;

			qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout_post_process, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( panini_push ), &panini_push );

			vk_set_fullscreen_viewport_scissor( vk.renderWidth, vk.renderHeight );
			qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
		}
	}

	vk_end_render_pass();

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

	VK_CHECK( qvkQueueSubmit( vk.queue, 1, &submit_info, vk.cmd->rendering_finished_fence ) );
	vk.cmd->waitForFence = qtrue;

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
			new_extent_valid = vk_query_surface_extent( &new_extent );
			vk_log_swapchain_recreation( res, &vk.swapchain_extent, new_extent_valid ? &new_extent : NULL );
			if ( new_extent_valid && ( !vk.swapchain_extent_valid ||
					new_extent.width != vk.swapchain_extent.width ||
					new_extent.height != vk.swapchain_extent.height ) ) {
				vk_restart_swapchain( __func__, res );
				return;
			}
			break;
		case VK_ERROR_OUT_OF_DATE_KHR:
			new_extent_valid = vk_query_surface_extent( &new_extent );
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
	alloc_info.memoryTypeIndex = find_memory_type2( memory_requirements.memoryTypeBits, memory_reqs, &memory_flags );
	if ( alloc_info.memoryTypeIndex == ~0U ) {
		// try less explicit flags, without host_coherent
		memory_reqs = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
		alloc_info.memoryTypeIndex = find_memory_type2( memory_requirements.memoryTypeBits, memory_reqs, &memory_flags );
		if ( alloc_info.memoryTypeIndex == ~0U ) {
			// slowest case
			memory_reqs = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			alloc_info.memoryTypeIndex = find_memory_type2( memory_requirements.memoryTypeBits, memory_reqs, &memory_flags );
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

	command_buffer = begin_command_buffer();

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

	// end_command_buffer( command_buffer, __func__  );

	// command_buffer = begin_command_buffer();

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

	end_command_buffer( command_buffer, __func__ );

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
		command_buffer = begin_command_buffer();

		record_image_layout_transition( command_buffer, srcImage,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			srcImageLayout, 0, 0 );

		end_command_buffer( command_buffer, "restore layout" );
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
	vk.volumetric_frame = 0;
	vk.has_prev_volumetric = qfalse;
	vk_prev_matrices_valid = qfalse;
	vk_prev_volumetric_time_valid = qfalse;
	vk_volumetric_noise_time = 0.0f;
	Com_Memset( &vk_volumetric_validation_state, 0, sizeof( vk_volumetric_validation_state ) );
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
	alloc_info.memoryTypeIndex = find_memory_type( mem_req.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.volumetric_params_memory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.volumetric_params_buffer, vk.volumetric_params_memory, 0 ) );

	vk.volumetric_params_buffer_size = mem_req.size;

	VK_CHECK( qvkMapMemory( vk.device, vk.volumetric_params_memory, 0, vk.volumetric_params_buffer_size, 0, &vk.volumetric_params_ptr ) );
	vk.volumetric_frame = 0;
	vk.has_prev_volumetric = qfalse;
	vk_prev_volumetric_time_valid = qfalse;
	vk_volumetric_noise_time = 0.0f;
	Com_Memset( &vk_volumetric_validation_state, 0, sizeof( vk_volumetric_validation_state ) );
}

static qboolean vk_parse_rgb_string( const char *s, vec3_t out )
{
	float r, g, b;

	if ( !s || !s[0] ) {
		return qfalse;
	}

	if ( sscanf( s, "%f %f %f", &r, &g, &b ) != 3 ) {
		return qfalse;
	}

	out[0] = r;
	out[1] = g;
	out[2] = b;
	return qtrue;
}

static void vk_normalize_rgb_luma_safe( vec3_t io )
{
	float maxc = MAX( io[0], MAX( io[1], io[2] ) );

	if ( maxc <= 0.0f ) {
		VectorSet( io, 1.0f, 1.0f, 1.0f );
		return;
	}

	if ( maxc > 1.0f ) {
		VectorScale( io, 1.0f / maxc, io );
	}
}

static qboolean vk_get_ibl_fog_color( vec3_t out )
{
	int i;
	int bestIndex = -1;
	float bestDistSq = 0.0f;
	const float *pos = backEnd.viewParms.or.origin;

	if ( !tr.cubemaps || tr.numCubemaps <= 0 ) {
		return qfalse;
	}

	for ( i = 0; i < tr.numCubemaps; i++ ) {
		vec3_t delta;
		float distSq;
		const cubemap_t *cube = &tr.cubemaps[i];

		if ( !cube->hasSHCoeffs ) {
			continue;
		}

		VectorSubtract( pos, cube->origin, delta );
		distSq = VectorLengthSquared( delta );

		if ( bestIndex == -1 || distSq < bestDistSq ) {
			bestIndex = i;
			bestDistSq = distSq;
		}
	}

	if ( bestIndex < 0 ) {
		return qfalse;
	}

	out[0] = tr.cubemaps[bestIndex].shCoeffs[0][0];
	out[1] = tr.cubemaps[bestIndex].shCoeffs[0][1];
	out[2] = tr.cubemaps[bestIndex].shCoeffs[0][2];
	vk_normalize_rgb_luma_safe( out );
	return qtrue;
}

static void vk_get_volumetric_fog_color( vec4_t out )
{
	int i;
	vec3_t base;
	float maxc;
	vec3_t tint = { 1.0f, 1.0f, 1.0f };
	const int colorMode = ( r_volumetricFogColorMode ) ? r_volumetricFogColorMode->integer : 0;
	qboolean foundFogVolume = qfalse;

	// Default to a "tint" derived from the sky light, but clamp it to LDR range.
	// tr.sunLight can include intensity (q3map_sun), which would otherwise
	// blow out the volumetric contribution (especially with bloom enabled).
	VectorCopy( tr.sunLight, base );
	maxc = MAX( base[0], MAX( base[1], base[2] ) );
	if ( maxc <= 0.0f ) {
		VectorSet( base, 1.0f, 1.0f, 1.0f );
	} else if ( maxc > 1.0f ) {
		VectorScale( base, 1.0f / maxc, base );
	}
	Vector4Set( out, base[0], base[1], base[2], 1.0f );

	if ( !tr.world || ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) ) {
		// Still allow user tinting when there's no world (menus, cinematics, etc.)
		if ( r_volumetricFogTint && vk_parse_rgb_string( r_volumetricFogTint->string, tint ) ) {
			out[0] *= tint[0];
			out[1] *= tint[1];
			out[2] *= tint[2];
		}
		return;
	}

	if ( colorMode == 1 ) {
		if ( r_volumetricFogTint && vk_parse_rgb_string( r_volumetricFogTint->string, tint ) ) {
			out[0] = tint[0];
			out[1] = tint[1];
			out[2] = tint[2];
		}
		out[3] = 1.0f;
		return;
	}

	for ( i = 1; i < tr.world->numfogs; i++ ) {
		const fog_t *fog = &tr.world->fogs[ i ];
		const float *o = backEnd.viewParms.or.origin;

		if ( o[0] < fog->bounds[0][0] || o[0] > fog->bounds[1][0] ) {
			continue;
		}
		if ( o[1] < fog->bounds[0][1] || o[1] > fog->bounds[1][1] ) {
			continue;
		}
		if ( o[2] < fog->bounds[0][2] || o[2] > fog->bounds[1][2] ) {
			continue;
		}

		Vector4Copy( fog->color, out );
		foundFogVolume = qtrue;
		break;
	}

	// In IBL mode, use the nearest cubemap's SH (average irradiance) when not inside a fog volume.
	if ( colorMode == 2 && !foundFogVolume ) {
		vec3_t ibl;
		if ( vk_get_ibl_fog_color( ibl ) ) {
			out[0] = ibl[0];
			out[1] = ibl[1];
			out[2] = ibl[2];
			out[3] = 1.0f;
		}
	}

	// Apply user tint in modes 0 and 2.
	if ( r_volumetricFogTint && vk_parse_rgb_string( r_volumetricFogTint->string, tint ) ) {
		out[0] *= tint[0];
		out[1] *= tint[1];
		out[2] *= tint[2];
	}
}

static float vk_matrix_max_abs_diff( const float *a, const float *b )
{
	float max_diff = 0.0f;

	for ( int i = 0; i < 16; i++ ) {
		const float d = fabsf( a[i] - b[i] );
		if ( d > max_diff ) {
			max_diff = d;
		}
	}
	return max_diff;
}

static void vk_update_volumetric_params( void )
{
	if ( !vk.volumetric_params_ptr ) {
		return;
	}

	volumetric_params_t params;
	Com_Memset( &params, 0, sizeof( params ) );

	const float *projection = backEnd.viewParms.projectionMatrix;
	const float *view = backEnd.viewParms.world.modelViewMatrix;
	const int now_ms = backEnd.refdef.time;
	int depth_mode = r_volumetricFogDepthMode ? r_volumetricFogDepthMode->integer : 1;
	int fog_steps = r_volumetricFogSteps ? r_volumetricFogSteps->integer : 32;
	int quality = r_volumetricFogQuality ? r_volumetricFogQuality->integer : 2;
	int local_light_count = 0;
	int local_volume_count = 0;
	float temporal_weight = r_volumetricFogTemporalWeight ? r_volumetricFogTemporalWeight->value : 0.0f;
	float jitter_amount = r_volumetricFogJitter ? r_volumetricFogJitter->value : 0.0f;
	float sun_intensity = r_volumetricFogSunIntensity ? r_volumetricFogSunIntensity->value : 1.0f;
	float ambient_intensity = r_volumetricFogAmbientIntensity ? r_volumetricFogAmbientIntensity->value : 1.0f;
	float noise_scale = r_volumetricFogNoiseScale ? r_volumetricFogNoiseScale->value : 0.0125f;
	float noise_strength = r_volumetricFogNoiseStrength ? r_volumetricFogNoiseStrength->value : 0.85f;
	float noise_threshold = r_volumetricFogNoiseThreshold ? r_volumetricFogNoiseThreshold->value : 0.2f;
	float g_aniso = r_volumetricFogAniso ? r_volumetricFogAniso->value : 0.0f;
	float fog_density = r_volumetricFogDensity ? r_volumetricFogDensity->value : 0.0f;
	float height_falloff = r_volumetricFogHeightFalloff ? r_volumetricFogHeightFalloff->value : 0.0f;
	float near_plane = ( r_znear ) ? r_znear->value : 4.0f;
	float far_plane = backEnd.viewParms.zFar;
	float z_exponent = ( r_volumetricFogZExponent ) ? r_volumetricFogZExponent->value : 1.5f;
	float max_distance = ( r_volumetricFogMaxDistance ) ? r_volumetricFogMaxDistance->value : 4096.0f;
	float reprojection_threshold = ( r_volumetricFogHistoryVelocityThreshold ) ? r_volumetricFogHistoryVelocityThreshold->value :
		( ( r_volumetricFogReprojectionThreshold ) ? r_volumetricFogReprojectionThreshold->value : 0.075f );
	float firefly_clamp = ( r_volumetricFogFireflyClamp ) ? r_volumetricFogFireflyClamp->value : 8.0f;
	float transmittance_cutoff = ( r_volumetricFogTransmittanceCutoff ) ? r_volumetricFogTransmittanceCutoff->value : 0.01f;
	float wind_speed = ( r_volumetricFogWindSpeed ) ? r_volumetricFogWindSpeed->value : 1.0f;
	float fluid_dt;
	float fluid_viscosity = ( r_fogFluidViscosity ) ? r_fogFluidViscosity->value : 0.05f;
	float fluid_dissipation = ( r_fogFluidDissipation ) ? r_fogFluidDissipation->value : 0.985f;
	float fluid_force = ( r_fogFluidForceScale ) ? r_fogFluidForceScale->value : 1.0f;
	float fluid_velocity_clamp = ( r_fogFluidVelocityClamp ) ? r_fogFluidVelocityClamp->value : 96.0f;
	float fluid_effective_scale = 1.0f;
	float fluid_target_ms = ( r_fogFluidTargetMs ) ? r_fogFluidTargetMs->value : 1.2f;
	float fluid_flow_strength = ( r_fogFluidFlowFieldStrength ) ? r_fogFluidFlowFieldStrength->value : 0.35f;
	float fluid_flow_scale = ( r_fogFluidFlowFieldScale ) ? r_fogFluidFlowFieldScale->value : 0.004f;
	float temporal_stability = ( r_volumetricFogTemporalStability ) ? r_volumetricFogTemporalStability->value : 0.7f;
	float shadow_contrast = ( r_volumetricFogShadowContrast ) ? r_volumetricFogShadowContrast->value : 1.35f;
	int fog_showcase = ( r_volumetricFogShowcase ) ? r_volumetricFogShowcase->integer : 0;
	int fluid_wrap = ( r_fogFluidWrap ) ? r_fogFluidWrap->integer : 0;
	int fluid_quality = ( r_fogFluidQuality ) ? r_fogFluidQuality->integer : 2;
	int fluid_pressure_iterations = ( r_fogFluidPressureIterations ) ? r_fogFluidPressureIterations->integer : 12;
	int fluid_active_width = (int)vk.fluid_width;
	int fluid_active_height = (int)vk.fluid_height;
	qboolean fluid_autoscale_enabled = qfalse;
	qboolean fluid_enabled = ( r_fogFluid && r_fogFluid->integer && vk.fluid_width > 0 && vk.fluid_height > 0 ) ? qtrue : qfalse;
	float delta_time = 1.0f / 60.0f;
	qboolean camera_cut = qfalse;
	vec3_t fog_min = { -2048.0f, -2048.0f, -256.0f };
	vec3_t fog_max = {  2048.0f,  2048.0f, 1024.0f };
	vec3_t noise_scroll = { 0.03f, 0.01f, 0.02f };
	vec3_t wind_dir = { 1.0f, 0.0f, 0.0f };
	vec3_t motion_dir = { 1.0f, 0.0f, 0.0f };

	if ( vk_prev_volumetric_time_valid ) {
		int dt = now_ms - vk_prev_volumetric_time_ms;
		if ( dt < 0 || dt > 1000 ) {
			dt = 16;
		}
		delta_time = (float)dt * 0.001f;
	}
		fluid_dt = delta_time;
		if ( fluid_dt < 0.0f ) {
			fluid_dt = 0.0f;
		}
		if ( fluid_dt > ( 1.0f / 30.0f ) ) {
			fluid_dt = 1.0f / 30.0f;
		}
	vk_prev_volumetric_time_ms = now_ms;
	vk_prev_volumetric_time_valid = qtrue;
	vk_volumetric_noise_time += delta_time;

	if ( !Mat4Inverse( projection, params.invProj ) ) {
		Com_Memcpy( params.invProj, projection, sizeof( params.invProj ) );
	}
	if ( !Mat4Inverse( view, params.invView ) ) {
		Com_Memcpy( params.invView, view, sizeof( params.invView ) );
	}
	Com_Memcpy( params.proj, projection, sizeof( params.proj ) );
	myGlMultMatrix( view, projection, params.viewProj );
	if ( vk_prev_matrices_valid ) {
		const float view_delta = vk_matrix_max_abs_diff( view, vk_prev_view_matrix );
		const float view_proj_delta = vk_matrix_max_abs_diff( params.viewProj, vk_prev_viewproj_matrix );
		if ( view_delta > 0.25f || view_proj_delta > 0.35f || backEnd.viewParms.portalView != PV_NONE ) {
			camera_cut = qtrue;
		}
	}
	if ( r_volumetricFogForceCameraCut && r_volumetricFogForceCameraCut->integer > 0 ) {
		camera_cut = qtrue;
		vk_volumetric_validation_state.forced_camera_cut_events++;
		ri.Cvar_SetValue( "r_volumetricFogForceCameraCut", 0.0f );
	}
	if ( camera_cut ) {
		vk.has_prev_volumetric = qfalse;
		vk_volumetric_validation_state.camera_cut_events++;
	}
	if ( vk_prev_matrices_valid && !camera_cut ) {
		Com_Memcpy( params.prevView, vk_prev_view_matrix, sizeof( params.prevView ) );
		Com_Memcpy( params.prevViewProj, vk_prev_viewproj_matrix, sizeof( params.prevViewProj ) );
	} else {
		Com_Memcpy( params.prevView, view, sizeof( params.prevView ) );
		Com_Memcpy( params.prevViewProj, params.viewProj, sizeof( params.prevViewProj ) );
	}

	params.viewOrigin[0] = backEnd.viewParms.or.origin[0];
	params.viewOrigin[1] = backEnd.viewParms.or.origin[1];
	params.viewOrigin[2] = backEnd.viewParms.or.origin[2];
	params.viewOrigin[3] = ( r_volumetricFogBaseHeight ) ? r_volumetricFogBaseHeight->value : 0.0f;

	params.sunDirection[0] = tr.sunDirection[0];
	params.sunDirection[1] = tr.sunDirection[1];
	params.sunDirection[2] = tr.sunDirection[2];
	params.sunDirection[3] = 0.0f;

	vk_get_volumetric_fog_color( params.fogColor );
	{
		float fog_intensity = ( r_volumetricFogIntensity ) ? r_volumetricFogIntensity->value : 1.0f;
		params.fogColor[3] = ( fog_intensity > 0.001f ) ? fog_intensity : 0.001f;
		if ( r_volumetricFog && r_volumetricFog->integer && r_fogDebug && r_fogDebug->integer > 0 && params.fogColor[3] < 1.0f ) {
			params.fogColor[3] = 1.0f;
		}
		if ( fog_showcase == 1 && params.fogColor[3] < 1.5f ) {
			params.fogColor[3] = 1.35f;
		} else if ( fog_showcase == 2 && params.fogColor[3] < 2.0f ) {
			params.fogColor[3] = 1.6f;
		} else if ( fog_showcase >= 3 && params.fogColor[3] < 3.0f ) {
			params.fogColor[3] = 2.2f;
		}
	}

	if ( r_volumetricFogWorldMin && r_volumetricFogWorldMin->string ) {
		sscanf( r_volumetricFogWorldMin->string, "%f %f %f", &fog_min[0], &fog_min[1], &fog_min[2] );
	}
	if ( r_volumetricFogWorldMax && r_volumetricFogWorldMax->string ) {
		sscanf( r_volumetricFogWorldMax->string, "%f %f %f", &fog_max[0], &fog_max[1], &fog_max[2] );
	}
	if ( r_volumetricFogNoiseScroll && r_volumetricFogNoiseScroll->string ) {
		vk_parse_rgb_string( r_volumetricFogNoiseScroll->string, noise_scroll );
	}
	if ( r_volumetricFogWindDirection && r_volumetricFogWindDirection->string ) {
		vk_parse_rgb_string( r_volumetricFogWindDirection->string, wind_dir );
	}

	params.worldMin[0] = fog_min[0];
	params.worldMin[1] = fog_min[1];
	params.worldMin[2] = fog_min[2];
	params.worldMin[3] = 0.0f;

	params.worldMax[0] = fog_max[0];
	params.worldMax[1] = fog_max[1];
	params.worldMax[2] = fog_max[2];
	params.worldMax[3] = 0.0f;
	{
		const float size_x = fog_max[0] - fog_min[0];
		const float size_y = fog_max[1] - fog_min[1];
		params.fluidWorldMap[0] = fog_min[0];
		params.fluidWorldMap[1] = fog_min[1];
		params.fluidWorldMap[2] = ( fabsf( size_x ) > 1e-6f ) ? ( 1.0f / size_x ) : 0.0f;
		params.fluidWorldMap[3] = ( fabsf( size_y ) > 1e-6f ) ? ( 1.0f / size_y ) : 0.0f;
	}

	params.gridDim[0] = (float)vk.froxel_width;
	params.gridDim[1] = (float)vk.froxel_height;
	params.gridDim[2] = (float)vk.froxel_slices;
	params.gridDim[3] = (float)( ( r_fogDebug ) ? r_fogDebug->integer : 0 );
	params.phaseParams[0] = g_aniso;
	params.phaseParams[1] = sun_intensity;
	params.phaseParams[2] = ambient_intensity;
	params.phaseParams[3] = shadow_contrast;
	params.noiseParams[0] = noise_scale;
	params.noiseParams[1] = noise_threshold;
	params.noiseParams[2] = noise_strength;
	params.noiseParams[3] = fluid_flow_scale;
	params.noiseScroll[0] = 0.0f;
	params.noiseScroll[1] = 0.0f;
	params.noiseScroll[2] = 0.0f;
	params.noiseScroll[3] = 0.0f;

	if ( fog_steps < 1 ) {
		fog_steps = 1;
	} else if ( fog_steps > 256 ) {
		fog_steps = 256;
	}
	if ( depth_mode < 0 ) {
		depth_mode = 0;
	} else if ( depth_mode > 2 ) {
		depth_mode = 2;
	}
	if ( quality < 0 ) {
		quality = 0;
	} else if ( quality > 3 ) {
		quality = 3;
	}
	if ( g_aniso < -0.999f ) {
		params.phaseParams[0] = -0.999f;
	} else if ( g_aniso > 0.999f ) {
		params.phaseParams[0] = 0.999f;
	}
	if ( fog_density < 0.0f ) {
		fog_density = 0.0f;
	}
	if ( height_falloff < 0.0f ) {
		height_falloff = 0.0f;
	}
	if ( temporal_weight < 0.0f ) {
		temporal_weight = 0.0f;
	} else if ( temporal_weight > 1.0f ) {
		temporal_weight = 1.0f;
	}
	if ( jitter_amount < 0.0f ) {
		jitter_amount = 0.0f;
	}
	if ( noise_scale < 0.0f ) {
		noise_scale = 0.0f;
	}
	if ( noise_threshold < 0.0f ) {
		noise_threshold = 0.0f;
	} else if ( noise_threshold > 1.0f ) {
		noise_threshold = 1.0f;
	}
	if ( noise_strength < 0.0f ) {
		noise_strength = 0.0f;
	} else if ( noise_strength > 1.0f ) {
		noise_strength = 1.0f;
	}
	if ( z_exponent < 1.0f ) {
		z_exponent = 1.0f;
	}
	if ( max_distance < near_plane + 1.0f ) {
		max_distance = near_plane + 1.0f;
	}
	if ( reprojection_threshold < 0.0f ) {
		reprojection_threshold = 0.0f;
	}
	if ( firefly_clamp < 0.0f ) {
		firefly_clamp = 0.0f;
	}
	if ( transmittance_cutoff < 0.0001f ) {
		transmittance_cutoff = 0.0001f;
	} else if ( transmittance_cutoff > 1.0f ) {
		transmittance_cutoff = 1.0f;
	}
	if ( wind_speed < 0.0f ) {
		wind_speed = 0.0f;
	}
	if ( fluid_quality < 0 ) {
		fluid_quality = 0;
	} else if ( fluid_quality > 3 ) {
		fluid_quality = 3;
	}
	if ( fluid_viscosity < 0.0f ) {
		fluid_viscosity = 0.0f;
	}
	if ( fluid_dissipation < 0.0f ) {
		fluid_dissipation = 0.0f;
	} else if ( fluid_dissipation > 1.0f ) {
		fluid_dissipation = 1.0f;
	}
	if ( fluid_force < 0.0f ) {
		fluid_force = 0.0f;
	} else if ( fluid_force > 8.0f ) {
		fluid_force = 8.0f;
	}
	if ( fluid_velocity_clamp < 1.0f ) {
		fluid_velocity_clamp = 1.0f;
	} else if ( fluid_velocity_clamp > 512.0f ) {
		fluid_velocity_clamp = 512.0f;
	}
	if ( fluid_target_ms < 0.1f ) {
		fluid_target_ms = 0.1f;
	} else if ( fluid_target_ms > 8.0f ) {
		fluid_target_ms = 8.0f;
	}
	if ( fluid_flow_strength < 0.0f ) {
		fluid_flow_strength = 0.0f;
	} else if ( fluid_flow_strength > 4.0f ) {
		fluid_flow_strength = 4.0f;
	}
	if ( fluid_flow_scale < 0.0001f ) {
		fluid_flow_scale = 0.0001f;
	} else if ( fluid_flow_scale > 1.0f ) {
		fluid_flow_scale = 1.0f;
	}
	if ( temporal_stability < 0.0f ) {
		temporal_stability = 0.0f;
	} else if ( temporal_stability > 1.0f ) {
		temporal_stability = 1.0f;
	}
	if ( shadow_contrast < 0.5f ) {
		shadow_contrast = 0.5f;
	} else if ( shadow_contrast > 4.0f ) {
		shadow_contrast = 4.0f;
	}
	if ( fog_showcase < 0 ) {
		fog_showcase = 0;
	} else if ( fog_showcase > 3 ) {
		fog_showcase = 3;
	}
	if ( fog_showcase > 0 ) {
		switch ( fog_showcase ) {
			case 1:
				fog_density = MAX( fog_density, 0.40f );
				height_falloff = MIN( height_falloff, 0.25f );
				sun_intensity = MAX( sun_intensity, 2.5f );
				ambient_intensity = MAX( ambient_intensity, 1.0f );
				noise_scale = MAX( noise_scale, 0.016f );
				noise_strength = MAX( noise_strength, 0.90f );
				noise_threshold = MIN( noise_threshold, 0.15f );
				transmittance_cutoff = MIN( transmittance_cutoff, 0.006f );
				break;
			case 2:
				fog_density = MAX( fog_density, 0.65f );
				height_falloff = MIN( height_falloff, 0.15f );
				sun_intensity = MAX( sun_intensity, 3.5f );
				ambient_intensity = MAX( ambient_intensity, 1.2f );
				noise_scale = MAX( noise_scale, 0.020f );
				noise_strength = MAX( noise_strength, 0.95f );
				noise_threshold = MIN( noise_threshold, 0.12f );
				fluid_enabled = ( vk.fluid_width > 0 && vk.fluid_height > 0 ) ? qtrue : fluid_enabled;
				fluid_force = MAX( fluid_force, 2.0f );
				fluid_flow_strength = MAX( fluid_flow_strength, 0.9f );
				fluid_velocity_clamp = MAX( fluid_velocity_clamp, 128.0f );
				fluid_pressure_iterations = MAX( fluid_pressure_iterations, 16 );
				fluid_dissipation = MAX( fluid_dissipation, 0.990f );
				transmittance_cutoff = MIN( transmittance_cutoff, 0.004f );
				break;
			default:
				fog_density = MAX( fog_density, 1.3f );
				height_falloff = MIN( height_falloff, 0.06f );
				sun_intensity = MAX( sun_intensity, 6.0f );
				ambient_intensity = MAX( ambient_intensity, 1.8f );
				noise_scale = MAX( noise_scale, 0.026f );
				noise_strength = 1.0f;
				noise_threshold = MIN( noise_threshold, 0.07f );
				temporal_weight = MAX( temporal_weight, 0.9f );
				fluid_enabled = ( vk.fluid_width > 0 && vk.fluid_height > 0 ) ? qtrue : fluid_enabled;
				fluid_force = MAX( fluid_force, 3.0f );
				fluid_flow_strength = MAX( fluid_flow_strength, 1.8f );
				fluid_velocity_clamp = MAX( fluid_velocity_clamp, 192.0f );
				fluid_pressure_iterations = MAX( fluid_pressure_iterations, 22 );
				fluid_dissipation = MAX( fluid_dissipation, 0.994f );
				transmittance_cutoff = MIN( transmittance_cutoff, 0.0015f );
				break;
		}
	}
	params.phaseParams[0] = g_aniso;
	params.phaseParams[1] = sun_intensity;
	params.phaseParams[2] = ambient_intensity;
	params.phaseParams[3] = shadow_contrast;
	if ( fluid_pressure_iterations < 1 ) {
		fluid_pressure_iterations = 1;
	} else if ( fluid_pressure_iterations > VK_FLUID_MAX_PRESSURE_ITERATIONS ) {
		fluid_pressure_iterations = VK_FLUID_MAX_PRESSURE_ITERATIONS;
	}
	switch ( fluid_quality ) {
		case 0:
			fluid_pressure_iterations = MIN( fluid_pressure_iterations, 8 );
			fluid_dissipation = MIN( fluid_dissipation, 0.970f );
			break;
		case 1:
			fluid_pressure_iterations = MIN( fluid_pressure_iterations, 12 );
			fluid_dissipation = MIN( fluid_dissipation, 0.982f );
			break;
		case 2:
			fluid_pressure_iterations = MIN( fluid_pressure_iterations, 16 );
			break;
		default:
			fluid_pressure_iterations = MIN( fluid_pressure_iterations, 20 );
			fluid_dissipation = MIN( fluid_dissipation, 0.995f );
			break;
	}

	fluid_autoscale_enabled = ( fluid_enabled && r_fogFluidAutoScale && r_fogFluidAutoScale->integer ) ? qtrue : qfalse;
	if ( fluid_autoscale_enabled ) {
		float min_resolution = ( r_fogFluidAutoScaleMinResolution ) ? r_fogFluidAutoScaleMinResolution->value : 0.45f;
		if ( min_resolution < 0.125f ) {
			min_resolution = 0.125f;
		} else if ( min_resolution > 1.0f ) {
			min_resolution = 1.0f;
		}
		fluid_effective_scale = vk.fluid_dynamic_resolution_scale;
		if ( fluid_effective_scale < min_resolution ) {
			fluid_effective_scale = min_resolution;
		} else if ( fluid_effective_scale > 1.0f ) {
			fluid_effective_scale = 1.0f;
		}
		if ( vk.fluid_dynamic_pressure_iterations > 0 ) {
			fluid_pressure_iterations = MIN( fluid_pressure_iterations, vk.fluid_dynamic_pressure_iterations );
		}
	}

	if ( fluid_enabled ) {
		fluid_active_width = MAX( 8, (int)( (float)vk.fluid_width * fluid_effective_scale + 0.5f ) );
		fluid_active_height = MAX( 8, (int)( (float)vk.fluid_height * fluid_effective_scale + 0.5f ) );
		fluid_active_width = MIN( fluid_active_width, (int)vk.fluid_width );
		fluid_active_height = MIN( fluid_active_height, (int)vk.fluid_height );
		if ( fluid_active_width <= 0 || fluid_active_height <= 0 ) {
			fluid_enabled = qfalse;
		}
	}
	vk.fluid_active_width = fluid_enabled ? (uint32_t)fluid_active_width : 0u;
	vk.fluid_active_height = fluid_enabled ? (uint32_t)fluid_active_height : 0u;

	params.fluidParams0[0] = fluid_dt;
	params.fluidParams0[1] = fluid_viscosity;
	params.fluidParams0[2] = fluid_dissipation;
	params.fluidParams0[3] = fluid_force;
	params.fluidParams1[0] = fluid_velocity_clamp;
	params.fluidParams1[1] = fluid_wrap ? 1.0f : 0.0f;
	params.fluidParams1[2] = (float)fluid_pressure_iterations;
	params.fluidParams1[3] = fluid_enabled ? 1.0f : 0.0f;
	params.fluidParams2[0] = fluid_enabled ? (float)vk.fluid_active_width : 0.0f;
	params.fluidParams2[1] = fluid_enabled ? (float)vk.fluid_active_height : 0.0f;
	params.fluidParams2[2] = (float)( vk.fluid_velocity_index & 1u );
	params.fluidParams2[3] = (float)( vk.fluid_density_index & 1u );
	if ( VectorLengthSquared( wind_dir ) < 1e-6f ) {
		VectorSet( wind_dir, 1.0f, 0.0f, 0.0f );
	}
	VectorNormalize2( wind_dir, motion_dir );

	params.densityParams[0] = fog_density;
	params.densityParams[1] = height_falloff;
	params.densityParams[2] = jitter_amount;
	params.densityParams[3] = temporal_weight;

	params.miscParams[0] = (float)fog_steps;
	params.miscParams[1] = (float)depth_mode;
	params.miscParams[2] = (float)vk.volumetric_frame;
	params.miscParams[3] = ( vk.has_prev_volumetric && temporal_weight > 0.0f && !camera_cut ) ? 1.0f : 0.0f;
	if ( near_plane < 0.001f ) {
		near_plane = 0.001f;
	}
	if ( far_plane > max_distance ) {
		far_plane = max_distance;
	}
	if ( far_plane <= near_plane + 1.0f ) {
		far_plane = near_plane + 1.0f;
	}
	params.sliceParams[0] = near_plane;
	params.sliceParams[1] = far_plane;
	params.sliceParams[2] = z_exponent;
	params.sliceParams[3] = max_distance;
	params.noiseParams[0] = noise_scale;
	params.noiseParams[1] = noise_threshold;
	params.noiseParams[2] = noise_strength;
	params.temporalParams[0] = reprojection_threshold;
	params.temporalParams[1] = 0.02f + temporal_stability * 0.08f;
	params.temporalParams[2] = firefly_clamp;
	params.temporalParams[3] = camera_cut ? 1.0f : 0.0f;
	params.qualityParams[0] = (float)quality;
	params.qualityParams[1] = ( quality <= 1 ) ? 1.0f : 0.0f;
	params.qualityParams[2] = ( quality == 0 ) ? 0.65f : ( ( quality == 1 ) ? 0.8f : 0.9f );
	params.qualityParams[3] = transmittance_cutoff;
	params.windParams[0] = vk_volumetric_noise_time;
	params.windParams[1] = delta_time;
	params.windParams[2] = wind_speed;
	params.windParams[3] = fluid_flow_strength;
	params.noiseScroll[0] = noise_scroll[0] + motion_dir[0];
	params.noiseScroll[1] = noise_scroll[1] + motion_dir[1];
	params.noiseScroll[2] = noise_scroll[2] + motion_dir[2];

	if ( tr.world && tr.world->fogs && !( tr.refdef.rdflags & RDF_NOWORLDMODEL ) ) {
		for ( int i = 1; i < tr.world->numfogs && local_volume_count < VK_VOLUMETRIC_MAX_VOLUMES; i++ ) {
			const fog_t *fog = &tr.world->fogs[i];
			const float extent_x = fog->bounds[1][0] - fog->bounds[0][0];
			const float extent_y = fog->bounds[1][1] - fog->bounds[0][1];
			const float extent_z = fog->bounds[1][2] - fog->bounds[0][2];
			float local_density = fog_density;

			if ( extent_x <= 0.001f || extent_y <= 0.001f || extent_z <= 0.001f ) {
				continue;
			}

			if ( fog->parms.depthForOpaque > 0.001f ) {
				local_density *= ( 1.0f / fog->parms.depthForOpaque );
			}
			if ( local_density < 0.0f ) {
				local_density = 0.0f;
			}

			params.volumeBoundsMin[local_volume_count][0] = fog->bounds[0][0];
			params.volumeBoundsMin[local_volume_count][1] = fog->bounds[0][1];
			params.volumeBoundsMin[local_volume_count][2] = fog->bounds[0][2];
			params.volumeBoundsMin[local_volume_count][3] = 0.0f;

			params.volumeBoundsMax[local_volume_count][0] = fog->bounds[1][0];
			params.volumeBoundsMax[local_volume_count][1] = fog->bounds[1][1];
			params.volumeBoundsMax[local_volume_count][2] = fog->bounds[1][2];
			params.volumeBoundsMax[local_volume_count][3] = 0.0f;

			params.volumeColorDensity[local_volume_count][0] = fog->color[0];
			params.volumeColorDensity[local_volume_count][1] = fog->color[1];
			params.volumeColorDensity[local_volume_count][2] = fog->color[2];
			params.volumeColorDensity[local_volume_count][3] = local_density;
			local_volume_count++;
		}
	}

	for ( int i = 0; i < (int)backEnd.viewParms.num_dlights && local_light_count < VK_VOLUMETRIC_MAX_LIGHTS; i++ ) {
		const dlight_t *dl = &backEnd.viewParms.dlights[i];
		float radius = dl->radius;

		if ( radius <= 0.001f ) {
			continue;
		}

		params.lightPosRadius[local_light_count][0] = dl->origin[0];
		params.lightPosRadius[local_light_count][1] = dl->origin[1];
		params.lightPosRadius[local_light_count][2] = dl->origin[2];
		params.lightPosRadius[local_light_count][3] = radius;

		params.lightColorType[local_light_count][0] = MAX( dl->color[0], 0.0f );
		params.lightColorType[local_light_count][1] = MAX( dl->color[1], 0.0f );
		params.lightColorType[local_light_count][2] = MAX( dl->color[2], 0.0f );
		params.lightColorType[local_light_count][3] = dl->linear ? 1.0f : 0.0f;
		// Shadow type is consumed by the local-light injection stage:
		// 0 = unshadowed, 1 = spot shadow path, 2 = point shadow path.
		// Per-light shadow slot index is written later into lightExtra.w.
		params.lightExtra[local_light_count][2] = ( r_fog_shadows && r_fog_shadows->integer ) ? ( dl->linear ? 1.0f : 2.0f ) : 0.0f;

		if ( dl->linear ) {
			vec3_t dir;
			float len;

			VectorSubtract( dl->origin2, dl->origin, dir );
			len = VectorNormalize( dir );
			if ( len <= 0.001f ) {
				VectorSet( dir, 0.0f, 0.0f, -1.0f );
				len = radius;
			}

			params.lightDirAngle[local_light_count][0] = dir[0];
			params.lightDirAngle[local_light_count][1] = dir[1];
			params.lightDirAngle[local_light_count][2] = dir[2];
			params.lightDirAngle[local_light_count][3] = cosf( DEG2RAD( 35.0f ) );
			params.lightExtra[local_light_count][0] = cosf( DEG2RAD( 20.0f ) );
			params.lightExtra[local_light_count][1] = len;
			params.lightExtra[local_light_count][3] = -1.0f;
		} else {
			params.lightDirAngle[local_light_count][0] = 0.0f;
			params.lightDirAngle[local_light_count][1] = 0.0f;
			params.lightDirAngle[local_light_count][2] = 0.0f;
			params.lightDirAngle[local_light_count][3] = -1.0f;
			params.lightExtra[local_light_count][0] = -1.0f;
			params.lightExtra[local_light_count][1] = radius;
			params.lightExtra[local_light_count][3] = -1.0f;
		}

		local_light_count++;
	}
	params.volumeCounts[0] = (float)local_volume_count;
	params.volumeCounts[1] = (float)local_light_count;
	params.volumeCounts[2] = vk.sun_shadow_valid ? 1.0f : 0.0f;
	params.volumeCounts[3] = 0.0f;
	params.passParams[0] = 0.0f;
	params.passParams[1] = (float)( ( vk.froxel_width + 1 ) / 2 );
	params.passParams[2] = (float)( ( vk.froxel_height + 1 ) / 2 );
	params.passParams[3] = (float)vk.froxel_slices;

	Matrix16Identity( params.sunShadowMatrix0 );
	if ( vk.sun_shadow_valid ) {
		Com_Memcpy( params.sunShadowMatrix0, vk.sun_shadow_matrix0, sizeof( params.sunShadowMatrix0 ) );
	}
	params.shadowParams0[0] = ( r_fogShadowBias ) ? r_fogShadowBias->value : 0.001f;
	params.shadowParams0[1] = ( r_fogShadowPcfRadius ) ? r_fogShadowPcfRadius->value : 1.0f;
	params.shadowParams0[2] = ( r_fog_shadows && r_fog_shadows->integer ) ? 1.0f : 0.0f;
	params.shadowParams0[3] = vk.sun_shadow_valid ? 1.0f : 0.0f;
	params.shadowMapSize0[0] = (float)vk.sun_shadow_width;
	params.shadowMapSize0[1] = (float)vk.sun_shadow_height;
	params.shadowMapSize0[2] = ( vk.sun_shadow_width > 0 ) ? ( 1.0f / (float)vk.sun_shadow_width ) : 0.0f;
	params.shadowMapSize0[3] = ( vk.sun_shadow_height > 0 ) ? ( 1.0f / (float)vk.sun_shadow_height ) : 0.0f;
	params.localSpotShadowMapSize[0] = (float)vk.local_spot_shadow_atlas_size;
	params.localSpotShadowMapSize[1] = (float)vk.local_spot_shadow_atlas_size;
	params.localSpotShadowMapSize[2] = ( vk.local_spot_shadow_atlas_size > 0 ) ? ( 1.0f / (float)vk.local_spot_shadow_atlas_size ) : 0.0f;
	params.localSpotShadowMapSize[3] = ( vk.local_spot_shadow_atlas_size > 0 ) ? ( 1.0f / (float)vk.local_spot_shadow_atlas_size ) : 0.0f;
	params.localPointShadowMapSize[0] = (float)vk.local_point_shadow_face_size;
	params.localPointShadowMapSize[1] = (float)vk.local_point_shadow_face_size;
	params.localPointShadowMapSize[2] = ( vk.local_point_shadow_face_size > 0 ) ? ( 1.0f / (float)vk.local_point_shadow_face_size ) : 0.0f;
	params.localPointShadowMapSize[3] = (float)vk.local_point_shadow_capacity;
	if ( params.shadowParams0[0] < 0.0f ) {
		params.shadowParams0[0] = 0.0f;
	}
	if ( params.shadowParams0[1] < 0.0f ) {
		params.shadowParams0[1] = 0.0f;
	}

	params.telemetryParams0[0] = (float)vk_volumetric_validation_state.telemetry_nan_or_inf;
	params.telemetryParams0[1] = (float)vk_volumetric_validation_state.telemetry_extinction_clamp_hits;
	params.telemetryParams0[2] = (float)vk_volumetric_validation_state.telemetry_temporal_rejects;
	params.telemetryParams0[3] = fluid_target_ms;
	params.telemetryParams1[0] = vk.volumetric_total_ms;
	params.telemetryParams1[1] = vk.volumetric_fluid_ms;
	params.telemetryParams1[2] = fluid_enabled ? fluid_effective_scale : 0.0f;
	params.telemetryParams1[3] = fluid_enabled ? (float)fluid_pressure_iterations : 0.0f;

	if ( r_fogDebug && r_fogDebug->integer > 0 && ( vk.volumetric_frame % 120u ) == 0u ) {
		ri.Printf( PRINT_ALL,
			"[VK][fog] params grid=(%.0f %.0f %.0f %.0f) phase=(%.3f %.3f %.3f %.0f) misc=(%.0f %.0f %.3f %.0f) density=(%.5f %.5f %.5f %.5f) fogColor=(%.3f %.3f %.3f %.3f) worldMin=(%.1f %.1f %.1f) worldMax=(%.1f %.1f %.1f)\n",
			params.gridDim[0], params.gridDim[1], params.gridDim[2], params.gridDim[3],
			params.phaseParams[0], params.phaseParams[1], params.phaseParams[2], params.phaseParams[3],
			params.miscParams[0], params.miscParams[1], params.miscParams[2], params.miscParams[3],
			params.densityParams[0], params.densityParams[1], params.densityParams[2], params.densityParams[3],
			params.fogColor[0], params.fogColor[1], params.fogColor[2], params.fogColor[3],
			params.worldMin[0], params.worldMin[1], params.worldMin[2],
			params.worldMax[0], params.worldMax[1], params.worldMax[2] );
		ri.Printf( PRINT_ALL, "[VK][fog] viewOrigin=(%.2f %.2f %.2f %.2f) sunDir=(%.3f %.3f %.3f)\n",
			params.viewOrigin[0], params.viewOrigin[1], params.viewOrigin[2], params.viewOrigin[3],
			params.sunDirection[0], params.sunDirection[1], params.sunDirection[2] );
		ri.Printf( PRINT_ALL, "[VK][fog] frame=%u hasHistory=%.0f jitter=%.3f\n",
			vk.volumetric_frame, params.miscParams[3], jitter_amount );
		ri.Printf( PRINT_ALL, "[VK][fog] slice near=%.3f far=%.1f zExp=%.3f maxDist=%.1f\n",
			params.sliceParams[0], params.sliceParams[1], params.sliceParams[2], params.sliceParams[3] );
		ri.Printf( PRINT_ALL, "[VK][fog] noise scale=%.5f threshold=%.3f strength=%.3f windDir=(%.3f %.3f %.3f) windSpeed=%.3f dt=%.4f\n",
			params.noiseParams[0], params.noiseParams[1], params.noiseParams[2],
			params.noiseScroll[0], params.noiseScroll[1], params.noiseScroll[2],
			params.windParams[2], params.windParams[1] );
		ri.Printf( PRINT_ALL, "[VK][fog] fluid enabled=%.0f grid=%.0fx%.0f dt=%.4f visc=%.4f diss=%.4f force=%.3f iters=%.0f clamp=%.2f wrap=%.0f velIdx=%.0f denIdx=%.0f\n",
			params.fluidParams1[3], params.fluidParams2[0], params.fluidParams2[1], params.fluidParams0[0],
			params.fluidParams0[1], params.fluidParams0[2], params.fluidParams0[3], params.fluidParams1[2],
			params.fluidParams1[0], params.fluidParams1[1], params.fluidParams2[2], params.fluidParams2[3] );
		ri.Printf( PRINT_ALL, "[VK][fog] showcase preset=%d\n", fog_showcase );
		ri.Printf( PRINT_ALL, "[VK][fog] reprojection threshold=%.4f depthEps=%.4f fireflyClamp=%.3f cameraCut=%.0f quality=%.0f checkerboard=%.0f transCut=%.4f volumes=%.0f lights=%.0f\n",
			params.temporalParams[0], params.temporalParams[1], params.temporalParams[2], params.temporalParams[3],
			params.qualityParams[0], params.qualityParams[1], params.qualityParams[3], params.volumeCounts[0], params.volumeCounts[1] );
		ri.Printf( PRINT_ALL, "[VK][fog] sun shadow enabled=%.0f valid=%.0f bias=%.6f pcf=%.3f size=%.0fx%.0f\n",
			params.shadowParams0[2], params.shadowParams0[3], params.shadowParams0[0], params.shadowParams0[1],
			params.shadowMapSize0[0], params.shadowMapSize0[1] );
	}

	Com_Memcpy( vk.volumetric_params_ptr, &params, sizeof( params ) );
	Com_Memcpy( vk_prev_view_matrix, view, sizeof( vk_prev_view_matrix ) );
	Com_Memcpy( vk_prev_projection_matrix, projection, sizeof( vk_prev_projection_matrix ) );
	Com_Memcpy( vk_prev_viewproj_matrix, params.viewProj, sizeof( vk_prev_viewproj_matrix ) );
	vk_prev_matrices_valid = qtrue;

	vk.volumetric_frame++;
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

	vk_end_render_pass(); // end main
	vk_volumetric_fog_pass();

	// bloom extraction
	vk_begin_bloom_extract_render_pass();
	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.bloom_extract_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.color_descriptor, 0, NULL );
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

		uint32_t offsets[2], offset_count;

		// restore clobbered descriptor sets
		for ( i = 0; i < VK_NUM_BLOOM_PASSES; i++ ) {
			if ( vk.cmd->descriptor_set.current[i] != VK_NULL_HANDLE ) {
				if ( i == VK_DESC_UNIFORM /*|| i == VK_DESC_STORAGE*/ ) {
					offset_count = 0;

					offsets[offset_count++] = vk.cmd->descriptor_set.offset[i];
					offsets[offset_count++] = vk.cmd->descriptor_set.offset[VK_DESC_UNIFORM_CAMERA_BINDING];

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
	alloc_info.memoryTypeIndex = find_memory_type( memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
			
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


	command_buffer = begin_command_buffer();
	record_image_layout_transition( command_buffer, def->offscreen.image, VK_IMAGE_ASPECT_COLOR_BIT, 
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
		0, 0 );

	end_command_buffer( command_buffer, __func__  );
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
	VkDynamicState							dynamic_state_array[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
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
	dynamic_state.dynamicStateCount = ARRAY_LEN( dynamic_state_array );
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

	command_buffer = begin_command_buffer();

	record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, 
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );

	qvkCmdClearColorImage( command_buffer, image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &desc );	
		
	record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, 
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		0, 0 );

	end_command_buffer( command_buffer, __func__ );
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
	alloc_info.memoryTypeIndex = find_memory_type( mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
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

	command_buffer = begin_command_buffer();

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

	end_command_buffer( command_buffer, "sh extraction" );

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

	command_buffer = begin_command_buffer();
	record_image_layout_transition( command_buffer, vk.cubeMap.color_image, VK_IMAGE_ASPECT_COLOR_BIT, 
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
		0, 0 );
	end_command_buffer( command_buffer, __func__  );

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

	command_buffer = begin_command_buffer();
	record_image_layout_transition( command_buffer, vk.cubeMap.color_image, VK_IMAGE_ASPECT_COLOR_BIT, 
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
		0, 0 );
	end_command_buffer( command_buffer, __func__  );

	vk_begin_main_render_pass();
}
