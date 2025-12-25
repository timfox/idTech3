#pragma once

#include <vulkan/vulkan.h>
#include "../common/q_shared.h"
#include "../renderercommon/tr_public.h"
#include "tr_common.h"

// Forward declarations
typedef struct {
    VkDeviceMemory memory;
    VkDeviceSize used;
} ImageChunk;

// Enable upload queue functionality for proper staging buffer management
#define USE_UPLOAD_QUEUE

// VMA must be included before other headers that use it
#ifdef USE_VMA
#include "vk_mem_alloc.h"
#endif

// Render pass types
typedef enum {
	RENDER_PASS_MAIN,
	RENDER_PASS_SCREENMAP,
	RENDER_PASS_CUBEMAP,
	RENDER_PASS_COUNT
} renderPass_t;

typedef float vec_t;
typedef vec_t mat4_t[16];

// Depth range modes for viewport depth control with stronger typing
typedef enum {
	DEPTH_RANGE_NORMAL,		// [0..1]
	DEPTH_RANGE_ZERO,		// [0..0]
	DEPTH_RANGE_ONE,		// [1..1]
	DEPTH_RANGE_WEAPON,		// [0..0.3]
	DEPTH_RANGE_COUNT
} Vk_Depth_Range;

// Type-safe dimension constraints
typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t mip_levels;
} image_dimensions_t;

// Type-safe color attachment creation
typedef struct {
    uint32_t width;
    uint32_t height;
    VkSampleCountFlagBits samples;
    VkFormat format;
    VkImageUsageFlags usage;
    VkImageLayout layout;
    qboolean multisample;
    VkImageCreateFlags flags;
} color_attachment_desc_t;

// Bounds checking for Vulkan operations
static inline qboolean Vk_ValidateImageDimensions(uint32_t width, uint32_t height, uint32_t mip_levels) {
    if (width == 0 || height == 0) return qfalse;
    if (width > 16384 || height > 16384) return qfalse; // Reasonable upper bounds
    if (mip_levels > 16) return qfalse; // Maximum reasonable mip levels
    return qtrue;
}

static inline qboolean Vk_ValidateColorAttachmentDesc(const color_attachment_desc_t *desc) {
    if (!desc) return qfalse;
    return Vk_ValidateImageDimensions(desc->width, desc->height, 1);
}

typedef enum {
	TYPE_COLOR_BLACK,
	TYPE_COLOR_WHITE,
	TYPE_COLOR_GREEN,
	TYPE_COLOR_RED,
	TYPE_FOG_ONLY,
	TYPE_DOT,

	TYPE_SIGNLE_TEXTURE_LIGHTING,
	TYPE_SIGNLE_TEXTURE_LIGHTING_LINEAR,

	TYPE_SIGNLE_TEXTURE_DF,

	TYPE_GENERIC_BEGIN, // start of non-env/env shader pairs
	TYPE_SIGNLE_TEXTURE = TYPE_GENERIC_BEGIN,
	TYPE_SIGNLE_TEXTURE_ENV,

	TYPE_SIGNLE_TEXTURE_IDENTITY,
	TYPE_SIGNLE_TEXTURE_IDENTITY_ENV,

	TYPE_SIGNLE_TEXTURE_FIXED_COLOR,
	TYPE_SIGNLE_TEXTURE_FIXED_COLOR_ENV,

	TYPE_SIGNLE_TEXTURE_ENT_COLOR,
	TYPE_SIGNLE_TEXTURE_ENT_COLOR_ENV,

	TYPE_MULTI_TEXTURE_ADD2_IDENTITY,
	TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV,
	TYPE_MULTI_TEXTURE_MUL2_IDENTITY,
	TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV,

	TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR,
	TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV,
	TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR,
	TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV,

	TYPE_MULTI_TEXTURE_MUL2,
	TYPE_MULTI_TEXTURE_MUL2_ENV,
	TYPE_MULTI_TEXTURE_ADD2_1_1,
	TYPE_MULTI_TEXTURE_ADD2_1_1_ENV,
	TYPE_MULTI_TEXTURE_ADD2,
	TYPE_MULTI_TEXTURE_ADD2_ENV,

	TYPE_MULTI_TEXTURE_MUL3,
	TYPE_MULTI_TEXTURE_MUL3_ENV,
	TYPE_MULTI_TEXTURE_ADD3_1_1,
	TYPE_MULTI_TEXTURE_ADD3_1_1_ENV,
	TYPE_MULTI_TEXTURE_ADD3,
	TYPE_MULTI_TEXTURE_ADD3_ENV,

	TYPE_BLEND2_ADD,
	TYPE_BLEND2_ADD_ENV,
	TYPE_BLEND2_MUL,
	TYPE_BLEND2_MUL_ENV,
	TYPE_BLEND2_ALPHA,
	TYPE_BLEND2_ALPHA_ENV,
	TYPE_BLEND2_ONE_MINUS_ALPHA,
	TYPE_BLEND2_ONE_MINUS_ALPHA_ENV,
	TYPE_BLEND2_MIX_ALPHA,
	TYPE_BLEND2_MIX_ALPHA_ENV,

	TYPE_BLEND2_MIX_ONE_MINUS_ALPHA,
	TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV,

	TYPE_BLEND2_DST_COLOR_SRC_ALPHA,
	TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV,

	TYPE_BLEND3_ADD,
	TYPE_BLEND3_ADD_ENV,
	TYPE_BLEND3_MUL,
	TYPE_BLEND3_MUL_ENV,
	TYPE_BLEND3_ALPHA,
	TYPE_BLEND3_ALPHA_ENV,
	TYPE_BLEND3_ONE_MINUS_ALPHA,
	TYPE_BLEND3_ONE_MINUS_ALPHA_ENV,
	TYPE_BLEND3_MIX_ALPHA,
	TYPE_BLEND3_MIX_ALPHA_ENV,
	TYPE_BLEND3_MIX_ONE_MINUS_ALPHA,
	TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV,

	TYPE_BLEND3_DST_COLOR_SRC_ALPHA,
	TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV,

	TYPE_GENERIC_END = TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV

} Vk_Shader_Type;

// used with cg_shadows == 2
typedef enum {
	SHADOW_DISABLED,
	SHADOW_EDGES,
	SHADOW_FS_QUAD
} Vk_Shadow_Phase;

typedef struct {
	Vk_Shader_Type shader_type;
	unsigned int state_bits; // GLS_XXX flags
	cullType_t face_culling;
	qboolean polygon_offset;
	qboolean mirror;
	Vk_Shadow_Phase shadow_phase;
	VkPrimitiveTopology primitives;
	int line_width;
	int fog_stage; // off, fog-in / fog-out
	int abs_light;
	int allow_discard;
	int use_font_sdf;
	float font_sdf_smooth;

#ifdef USE_VK_PBR
	uint32_t				vk_pbr_flags;
	vec4_t					specularScale;
	vec4_t					normalScale;
#endif
	int acff; // none, rgb, rgba, alpha
	struct {
		byte rgb;
		byte alpha;
	} color;
} Vk_Pipeline_Def;

typedef struct {
	VkSamplerAddressMode address_mode; // clamp/repeat texture addressing mode
	int vk_mag_filter;		// VK_XXX mag filter
	int vk_min_filter;		// VK_XXX min filter
	qboolean max_lod_1_0;	// fixed 1.0 lod
	qboolean noAnisotropy;	// disable anisotropic filtering
	qboolean isFontTexture;	// font/UI texture - force maxLod=0.0f when no mipmaps
} Vk_Sampler_Def;

// Filter definition for cubemap prefiltering
typedef struct filterDef_s {
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

#include "vk_memory.h"
#include "vk_compute.h"
#include "vk_buffers.h"
#include "vk_pipeline.h"

#ifdef __cplusplus
extern "C" {
#endif

// Vulkan-specific constants
#define MAX_SWAPCHAIN_IMAGES 8
#define MAX_ATTACHMENTS_IN_POOL 32
#define MAX_VK_PIPELINES 1024
#define MAX_VK_SAMPLERS 256
#define MAX_IMAGE_CHUNKS 16

// Swapchain image count constants for different presentation modes
#define MIN_SWAPCHAIN_IMAGES_IMM 2
#define MIN_SWAPCHAIN_IMAGES_MAILBOX 3
#define MIN_SWAPCHAIN_IMAGES_FIFO 2
#define MIN_SWAPCHAIN_IMAGES_FIFO_0 2

// Vulkan filter constants (if not defined by headers)
#ifndef VK_FILTER_NEAREST_MIPMAP_NEAREST
#define VK_FILTER_NEAREST_MIPMAP_NEAREST VK_FILTER_NEAREST
#endif
#ifndef VK_FILTER_LINEAR_MIPMAP_NEAREST
#define VK_FILTER_LINEAR_MIPMAP_NEAREST VK_FILTER_LINEAR
#endif
#ifndef VK_FILTER_NEAREST_MIPMAP_LINEAR
#define VK_FILTER_NEAREST_MIPMAP_LINEAR VK_FILTER_NEAREST
#endif
#ifndef VK_FILTER_LINEAR_MIPMAP_LINEAR
#define VK_FILTER_LINEAR_MIPMAP_LINEAR VK_FILTER_LINEAR
#endif

// Buffer size constants
#define VERTEX_BUFFER_SIZE (64 * 1024 * 1024)      // 64MB
#define STAGING_BUFFER_SIZE (32 * 1024 * 1024)     // 32MB
#define VERTEX_BUFFER_SIZE_HI (128 * 1024 * 1024)  // 128MB
#define STAGING_BUFFER_SIZE_HI (64 * 1024 * 1024)  // 64MB
#define IMAGE_CHUNK_SIZE (256 * 1024 * 1024)       // 256MB

// Primitive topology constants (if not defined by headers)
#ifndef LINE_LIST
#define LINE_LIST VK_PRIMITIVE_TOPOLOGY_LINE_LIST
#endif
#ifndef POINT_LIST
#define POINT_LIST VK_PRIMITIVE_TOPOLOGY_POINT_LIST
#endif
#ifndef TRIANGLE_STRIP
#define TRIANGLE_STRIP VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP
#endif

// Forward declarations
typedef struct material_params_s material_params_t;
typedef struct meshlet_info_s meshlet_info_t;

// Constants for advanced systems
#define MAX_STREAM_CELLS 256
#define MAX_TIMELINE_SEMAPHORES 32
#define MAX_BINDLESS_TEXTURES 4096

// Bloom system constants
#define VK_NUM_BLOOM_PASSES 4

// Descriptor set indices
#define VK_DESC_UNIFORM 0
#define VK_DESC_TEXTURE_BASE 1
#define VK_DESC_COUNT 10

// Uniform descriptor bindings
#define VK_DESC_UNIFORM_MAIN_BINDING 0
#define VK_DESC_UNIFORM_CAMERA_BINDING 1
#define VK_DESC_UNIFORM_COUNT 2

// Other descriptor indices
#define VK_DESC_FOG_ONLY 4
#define VK_DESC_FOG_COLLAPSE 4
#define VK_DESC_FOG_DLIGHT 4
#define VK_DESC_PBR_BRDFLUT 5
#define VK_DESC_PBR_NORMAL 6
#define VK_DESC_PBR_PHYSICAL 7
#define VK_DESC_PBR_CUBEMAP 8
#define VK_DESC_MATERIAL_PARAMS 9
#define VK_DESC_STORAGE 10

// Full definitions needed for struct members
// Stream cell structure
struct stream_cell_s {
	int32_t cellX, cellY, cellZ;  // Cell coordinates
	vec3_t worldMin, worldMax;     // World space bounds
	uint32_t state;                // Current state
	uint32_t priority;              // Load priority (lower = higher priority)
	
	// Asset references
	uint32_t modelCount;
	qhandle_t *models;
	uint32_t textureCount;
	image_t **textures;
	
	// Memory usage tracking
	uint32_t memoryUsed;
	uint32_t lastAccessFrame;
};
typedef struct stream_cell_s stream_cell_t;

// Atmosphere preset enum
enum atmosphere_preset_e {
	ATMOSPHERE_BRUTAL,
	ATMOSPHERE_MYSTERIOUS,
	ATMOSPHERE_COMBAT,
	ATMOSPHERE_CALM,
	ATMOSPHERE_CUSTOM
};
typedef enum atmosphere_preset_e atmosphere_preset_t;

// Atmosphere parameters structure
struct atmosphere_params_s {
	float exposure;
	float contrast;
	float saturation;
	float brightness;
	float fogDensity;
	float fogStart;
	float fogEnd;
	vec3_t fogColor;
	float fogHeightFalloff;
	float bloomIntensity;
	float bloomThreshold;
	float bloomSize;
	vec3_t colorTint;
	float colorTemperature;
	float dofFocusDistance;
	float dofBlurRadius;
	float timeOfDay;
	float weatherIntensity;
	uint32_t flags;
};
typedef struct atmosphere_params_s atmosphere_params_t;

// Meshlet metadata (CPU-side)
typedef struct meshlet_info_s {
	uint32_t firstIndex;
	uint32_t indexCount;
	uint32_t vertexCount;
} meshlet_info_t;

// Procedural dressing limits
#define VK_MAX_PROC_RULES     64
#define VK_MAX_PROC_BIOMES    8
#define VK_MAX_PROC_INSTANCES 65536

// Procedural dressing data
typedef enum {
	PROC_RULE_PAINT = 0,
	PROC_RULE_VOLUME,
	PROC_RULE_SPLINE
} proc_rule_type_t;

// Vulkan debugging functions
void vk_set_object_name(uint64_t obj, const char *name, VkDebugReportObjectTypeEXT type);


typedef struct proc_biome_s {
	char name[32];
	vec3_t tint;
	vec2_t scaleRange;
	float densityMultiplier;
	uint32_t materialIndex;
} proc_biome_t;

typedef struct proc_rule_s {
	proc_rule_type_t type;
	vec3_t a;
	vec3_t b;
	vec3_t mins;
	vec3_t maxs;
	float radius;
	float density;
	float jitter;
	uint32_t biomeId;
	uint32_t maxInstances;
} proc_rule_t;

typedef struct proc_instance_s {
	mat4_t transform;
	vec4_t color;
	uint32_t biomeId;
} proc_instance_t;

// Expose Vulkan command function pointers used from other translation units
// (e.g. tr_backend.c) when building with Vulkan.
#ifdef USE_VULKAN
extern PFN_vkCmdSetViewport qvkCmdSetViewport;
extern PFN_vkCmdSetScissor  qvkCmdSetScissor;
extern PFN_vkCmdWriteTimestamp qvkCmdWriteTimestamp;
extern PFN_vkCmdBeginQuery qvkCmdBeginQuery;
extern PFN_vkCmdEndQuery qvkCmdEndQuery;
#endif


typedef struct VK_Pipeline {
	Vk_Pipeline_Def def;
	VkPipeline handle[ RENDER_PASS_COUNT ];
} VK_Pipeline_t;

// this structure must be in sync with shader uniforms!
typedef struct vkUniform_s {
	// light/env parameters:
	vec4_t eyePos;				// vertex
	union {
		struct {
			vec4_t pos;			// vertex: light origin
			vec4_t color;		// fragment: rgb + 1/(r*r)
			vec4_t vector;		// fragment: linear dynamic light
		} light;
		struct {
			vec4_t color[3];	// ent.color[3]
		} ent;
	};
	// fog parameters:
	vec4_t fogDistanceVector;	// vertex
	vec4_t fogDepthVector;		// vertex
	vec4_t fogEyeT;				// vertex
	vec4_t fogColor;			// fragment
} vkUniform_t;

typedef struct vkUniformCamera_s {
	vec4_t viewOrigin;
	mat4_t modelMatrix;
} vkUniformCamera_t;

#define TESS_XYZ   (1)
#define TESS_RGBA0 (2)
#define TESS_RGBA1 (4)
#define TESS_RGBA2 (8)
#define TESS_ST0   (16)
#define TESS_ST1   (32)
#define TESS_ST2   (64)
#define TESS_NNN   (128)
#define TESS_VPOS  (256)  // uniform with eyePos
#define TESS_ENV   (512)  // mark shader stage with environment mapping
#define TESS_ENT0  (1024) // uniform with ent.color[0]
#define TESS_ENT1  (2048) // uniform with ent.color[1]
#define TESS_ENT2  (4096) // uniform with ent.color[2]
#define TESS_ENV   (512) // mark shader stage with environment mapping

#ifdef USE_VK_PBR
#define TESS_PBR   				( 1024 ) // PBR shader variant, qtangent vertex attribute and eyePos uniform

#define PBR_HAS_NORMALMAP		( 1 )
#define PBR_HAS_PHYSICALMAP		( 2 )
#define PBR_HAS_SPECULARMAP		( 4 )
#define PBR_HAS_LIGHTMAP		( 8 )

#define PHYS_NONE				( 1 )
#define PHYS_RMO				( 2 )
#define PHYS_RMOS   			( 4 )
#define PHYS_MOXR   			( 8 )
#define PHYS_MOSR   			( 16 )
#define PHYS_ORM  				( 32 )	
#define PHYS_ORMS   			( 64 )	
#define PHYS_NORMAL   			( 128 )	
#define PHYS_NORMALHEIGHT		( 256 )	
#define PHYS_SPECGLOSS					( 512 )	

#define ByteToFloat(a)			((float)(a) * 1.0f/255.0f)
#define FloatToByte(a)			(byte)((a) * 255.0f)

#define RGBtosRGB(a)					(((a) < 0.0031308f) ? (12.92f * (a)) : (1.055f * pow((a), 0.41666f) - 0.055f))
#define sRGBtoRGB(a)					(((a) <= 0.04045f)  ? ((a) / 12.92f) : (pow((((a) + 0.055f) / 1.055f), 2.4)) )
#endif

typedef struct textureMapType_s {
	uint32_t			type;
	const char			*suffix;
	VkComponentMapping	swizzle;
} textureMapType_t;

static const textureMapType_t textureMapTypes[] = {
	{ 0U,							"",			{ VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, } },
#ifdef USE_VK_PBR
	{ (uint32_t)PHYS_RMO,			"_rmo",		{ VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_ONE,	} },
	{ (uint32_t)PHYS_RMOS,			"_rmos",	{ VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_A, } },
	{ (uint32_t)PHYS_MOXR,			"_moxr",	{ VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_A, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ONE } },
	{ (uint32_t)PHYS_MOSR,			"_mosr",	{ VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_A, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_B } },
	{ (uint32_t)PHYS_ORM,			"_orm",		{ VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY } },
	{ (uint32_t)PHYS_ORMS,			"_orms",	{ VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY } },
	{ (uint32_t)PHYS_NORMAL,		"_n",		{ VK_COMPONENT_SWIZZLE_A, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_R } },
	{ (uint32_t)PHYS_NORMALHEIGHT,	"_nh",		{ VK_COMPONENT_SWIZZLE_A, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_R } },
#endif
};

//
// Initialization.
//

// Initializes VK_Instance structure.
// After calling this function we get fully functional vulkan subsystem.
void vk_initialize( void );

// Called after initialization or renderer restart
void vk_init_descriptors( void );

// Shutdown vulkan subsystem by releasing resources acquired by Vk_Instance.
void vk_shutdown( refShutdownCode_t code );

// Releases vulkan resources allocated during program execution.
// This effectively puts vulkan subsystem into initial state (the state we have after vk_initialize call).
void vk_release_resources( void );

void vk_wait_idle( void );
void vk_queue_wait_idle( void );
qboolean vk_wait_staging_buffer( void );
// Type-safe color attachment creation
qboolean create_color_attachment(uint32_t width, uint32_t height, VkSampleCountFlagBits samples, VkFormat format, VkImageUsageFlags usage, VkImage *image, VkImageView *image_view, VkImageLayout image_layout, qboolean multisample, VkImageCreateFlags flags);

// Type-safe wrapper with bounds checking
static inline qboolean create_color_attachment_safe(const color_attachment_desc_t *desc, VkImage *image, VkImageView *image_view) {
    if (!Vk_ValidateColorAttachmentDesc(desc) || !image || !image_view) {
        return qfalse;
    }
    return create_color_attachment(desc->width, desc->height, desc->samples, desc->format,
                                 desc->usage, image, image_view, desc->layout,
                                 desc->multisample, desc->flags);
}

VkInstance VK_GetInstanceHandle( void );
VkSampleCountFlagBits VK_GetMsaaSampleCount( void );
VkCommandBuffer VK_BeginImmediateCommands( void );
void VK_EndImmediateCommands( VkCommandBuffer command_buffer, const char *location );

struct ImDrawData;
qboolean VK_ImGui_InitBackend( void );
void VK_ImGui_ShutdownBackend( void );
void VK_ImGui_NewFrame( void );
void VK_ImGui_RenderDrawData( const struct ImDrawData *drawData );
void VK_ImGui_NotifyRenderPassChanged( void );
void VK_ImGui_NotifySwapchainChanged( void );

// Helper function for memory type selection
uint32_t find_memory_type( uint32_t memory_type_bits, VkMemoryPropertyFlags properties );

// Vulkan error checking macro (vk_result_string is defined in vk.c)
extern const char *vk_result_string( VkResult res );

// Standard VK_CHECK macro - prints error but continues (for non-critical operations)
#define VK_CHECK( function_call ) { \
	VkResult _vk_check_res = function_call; \
	if ( _vk_check_res < 0 ) { \
		ri.Printf( PRINT_ERROR, "Vulkan: %s returned %s", #function_call, vk_result_string( _vk_check_res ) ); \
	} \
}

// VK_CHECK_CRITICAL macro - prints error and returns false (for initialization operations)
#define VK_CHECK_CRITICAL( function_call ) { \
	VkResult _vk_check_res = function_call; \
	if ( _vk_check_res < 0 ) { \
		ri.Printf( PRINT_ERROR, "Vulkan: %s returned %s (critical failure)", #function_call, vk_result_string( _vk_check_res ) ); \
		return qfalse; \
	} \
}

// VK_CHECK_FATAL macro - prints error and terminates (for irrecoverable errors)
#define VK_CHECK_FATAL( function_call ) { \
	VkResult _vk_check_res = function_call; \
	if ( _vk_check_res < 0 ) { \
		ri.Error( ERR_FATAL, "Vulkan: %s returned %s", #function_call, vk_result_string( _vk_check_res ) ); \
	} \
}

// Vulkan function pointer declarations (defined in vk.c)
extern PFN_vkAllocateMemory qvkAllocateMemory;
extern PFN_vkAllocateCommandBuffers qvkAllocateCommandBuffers;
extern PFN_vkBeginCommandBuffer qvkBeginCommandBuffer;
extern PFN_vkBindBufferMemory qvkBindBufferMemory;
extern PFN_vkBindImageMemory qvkBindImageMemory;
extern PFN_vkCmdBindDescriptorSets qvkCmdBindDescriptorSets;
extern PFN_vkCmdBindIndexBuffer qvkCmdBindIndexBuffer;
extern PFN_vkCmdBindPipeline qvkCmdBindPipeline;
extern PFN_vkCmdBindVertexBuffers qvkCmdBindVertexBuffers;
extern PFN_vkCmdClearColorImage qvkCmdClearColorImage;
extern PFN_vkCmdBeginRenderPass qvkCmdBeginRenderPass;
extern PFN_vkCmdEndRenderPass qvkCmdEndRenderPass;
extern PFN_vkWaitForFences qvkWaitForFences;
extern PFN_vkResetFences qvkResetFences;
extern PFN_vkResetCommandBuffer qvkResetCommandBuffer;
extern PFN_vkResetQueryPool qvkResetQueryPool;
extern PFN_vkEndCommandBuffer qvkEndCommandBuffer;
extern PFN_vkQueueSubmit qvkQueueSubmit;
extern PFN_vkCmdDispatch qvkCmdDispatch;
extern PFN_vkCmdDrawIndexed qvkCmdDrawIndexed;
extern PFN_vkCmdDrawIndexedIndirect qvkCmdDrawIndexedIndirect;
extern PFN_vkCmdPipelineBarrier qvkCmdPipelineBarrier;
extern PFN_vkCmdPushConstants qvkCmdPushConstants;
extern PFN_vkAllocateDescriptorSets qvkAllocateDescriptorSets;
extern PFN_vkCreateBuffer qvkCreateBuffer;
extern PFN_vkCreateCommandPool qvkCreateCommandPool;
extern PFN_vkCreateDescriptorSetLayout qvkCreateDescriptorSetLayout;
extern PFN_vkCreateDescriptorPool qvkCreateDescriptorPool;
extern PFN_vkCreateFence qvkCreateFence;
extern PFN_vkCreateImage qvkCreateImage;
extern PFN_vkCreateImageView qvkCreateImageView;
extern PFN_vkCreatePipelineLayout qvkCreatePipelineLayout;
extern PFN_vkCreateQueryPool qvkCreateQueryPool;
extern PFN_vkGetPipelineCacheData qvkGetPipelineCacheData;
extern PFN_vkGetQueryPoolResults qvkGetQueryPoolResults;
extern PFN_vkDestroyBuffer qvkDestroyBuffer;
extern PFN_vkDestroyCommandPool qvkDestroyCommandPool;
extern PFN_vkDestroyDescriptorSetLayout qvkDestroyDescriptorSetLayout;
extern PFN_vkGetDeviceProcAddr qvkGetDeviceProcAddr;
extern PFN_vkDestroyDescriptorPool qvkDestroyDescriptorPool;
extern PFN_vkDestroyFence qvkDestroyFence;
extern PFN_vkDestroyImage qvkDestroyImage;
extern PFN_vkDestroyImageView qvkDestroyImageView;
extern PFN_vkDestroyQueryPool qvkDestroyQueryPool;
extern PFN_vkDestroyPipeline qvkDestroyPipeline;
extern PFN_vkDestroyPipelineLayout qvkDestroyPipelineLayout;
extern PFN_vkDestroySampler qvkDestroySampler;
extern PFN_vkFreeMemory qvkFreeMemory;
extern PFN_vkGetBufferMemoryRequirements qvkGetBufferMemoryRequirements;
extern PFN_vkGetImageMemoryRequirements qvkGetImageMemoryRequirements;
extern PFN_vkMapMemory qvkMapMemory;
extern PFN_vkCreateSampler qvkCreateSampler;
extern PFN_vkUnmapMemory qvkUnmapMemory;
extern PFN_vkUpdateDescriptorSets qvkUpdateDescriptorSets;
extern PFN_vkGetPhysicalDeviceProperties qvkGetPhysicalDeviceProperties;
extern PFN_vkGetPhysicalDeviceProperties2KHR qvkGetPhysicalDeviceProperties2KHR;
extern PFN_vkGetPhysicalDeviceFeatures2KHR qvkGetPhysicalDeviceFeatures2KHR;

// Ray tracing function pointers
extern PFN_vkCreateAccelerationStructureKHR qvkCreateAccelerationStructureKHR;
extern PFN_vkDestroyAccelerationStructureKHR qvkDestroyAccelerationStructureKHR;
extern PFN_vkGetAccelerationStructureBuildSizesKHR qvkGetAccelerationStructureBuildSizesKHR;
extern PFN_vkGetAccelerationStructureDeviceAddressKHR qvkGetAccelerationStructureDeviceAddressKHR;
extern PFN_vkCmdBuildAccelerationStructuresKHR qvkCmdBuildAccelerationStructuresKHR;
extern PFN_vkCmdTraceRaysKHR qvkCmdTraceRaysKHR;
extern PFN_vkCreateRayTracingPipelinesKHR qvkCreateRayTracingPipelinesKHR;
extern PFN_vkGetRayTracingShaderGroupHandlesKHR qvkGetRayTracingShaderGroupHandlesKHR;
extern PFN_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR qvkGetRayTracingCaptureReplayShaderGroupHandlesKHR;
extern PFN_vkCmdTraceRaysIndirectKHR qvkCmdTraceRaysIndirectKHR;
extern PFN_vkGetBufferDeviceAddress qvkGetBufferDeviceAddress;

//
// Resources allocation.
//
// Image operations with improved const correctness and type safety
void vk_create_image( image_t *image, int width, int height, int mip_levels );
void vk_upload_image_data( image_t *image, int x, int y, int width, int height, int miplevels, const byte *pixels, size_t size, qboolean update );
void vk_update_descriptor_set( const image_t *image, qboolean mipmap );
void vk_update_font_textures( void );
void vk_destroy_image_resources( VkImage *image, VkImageView *imageView );
void vk_update_attachment_descriptors( void );
void vk_destroy_samplers( void );

uint32_t vk_find_pipeline_ext( uint32_t base, const Vk_Pipeline_Def *def, qboolean use );
void vk_get_pipeline_def( uint32_t pipeline, Vk_Pipeline_Def *def );

void vk_bind_generated_shaders( void );

// Framebuffer and synchronization management
void vk_create_framebuffers( void );
void vk_destroy_framebuffers( void );
void vk_create_sync_primitives( void );
void vk_destroy_sync_primitives( void );
void vk_create_prefilter_framebuffer( filterDef *def );

//
// Rendering setup.
//

void vk_clear_color( const vec4_t color );
void vk_clear_depth( qboolean clear_stencil );
void vk_begin_frame( void );
qboolean vk_capture_screenmap( void );
qboolean vk_clear_screenmap( void );
void vk_end_frame( void );
void vk_present_frame( void );

void vk_end_render_pass( void );
void vk_begin_main_render_pass( void );
void vk_begin_post_bloom_render_pass( void );
void vk_begin_bloom_extract_render_pass( void );
void vk_begin_blur_render_pass( uint32_t index );

void vk_bind_pipeline( uint32_t pipeline );
void vk_bind_index( void );
void vk_bind_index_ext( const int numIndexes, const uint32_t*indexes );
void vk_bind_geometry( uint32_t flags );
void vk_bind_lighting( int stage, int bundle );
void vk_draw_geometry( Vk_Depth_Range depth_range, qboolean indexed );
void vk_draw_dot( uint32_t storage_offset );

void vk_read_pixels( byte* buffer, uint32_t width, uint32_t height ); // screenshots
qboolean vk_bloom( void );

qboolean vk_alloc_vbo( const byte *vbo_data, int vbo_size );
void vk_update_mvp( const float *m );

uint32_t vk_tess_index( uint32_t numIndexes, const void *src );
void vk_bind_index_buffer( VkBuffer buffer, uint32_t offset );
#ifdef USE_VBO
void vk_draw_indexed( uint32_t indexCount, uint32_t firstIndex );
#endif
void vk_reset_descriptor( int index );
void vk_update_descriptor( uint32_t index, VkDescriptorSet descriptor );
void vk_update_descriptor_offset( int index, uint32_t offset );
void vk_bind_descriptor_sets( void );
void vk_update_uniform_descriptor( VkDescriptorSet descriptor, VkBuffer buffer );

// Memory tracking
extern void vk_track_allocation(VkDeviceSize size);
extern void vk_track_free(VkDeviceSize size);

// VRAM monitoring and leak detection
extern void vk_init_vram_stats(void);
extern void vk_track_gpu_allocation(VkDeviceMemory memory, VkDeviceSize size, uint32_t memory_type,
                                   const char *resource_name, const char *allocation_site);
extern void vk_track_gpu_free(VkDeviceMemory memory);
extern void vk_detect_memory_leaks(void);
extern void vk_print_vram_stats(void);
extern qboolean vk_perform_defragmentation(void);

// Post-processing
qboolean vk_init_post_processing(void);

// Performance counters
extern void Perf_CountDrawCall(void);

// Validation utilities
qboolean vk_validate_handle(void *handle, const char *handle_name);

// Performance monitoring
void vk_update_performance_stats(void);

// GPU timing queries
void vk_get_gpu_timing_stats( double *avg_frame_time_ms, double *min_frame_time_ms, double *max_frame_time_ms );

const char *vk_format_string( VkFormat format );

// Ray tracing functions
void vk_rt_init( void );
void vk_rt_shutdown( void );
void vk_rt_create_pipeline( void );
void vk_rt_populate_sbt( void );
void vk_rt_build_acceleration_structures( void );
void vk_rt_build_blas( VkBuffer vertexBuffer, VkDeviceSize vertexOffset, uint32_t vertexCount, VkBuffer indexBuffer, VkDeviceSize indexOffset, uint32_t indexCount, uint32_t blasIndex );
void vk_rt_update_tlas( void );
void vk_rt_trace_rays( uint32_t width, uint32_t height );

// Internal ray tracing functions (forward declarations)
void vk_rt_create_descriptor_set_layout( void );
void vk_rt_create_pipeline_layout( void );
void vk_rt_create_shader_binding_table( void );
void vk_rt_create_output_image( uint32_t width, uint32_t height );
void vk_rt_update_descriptor_set( void );
void vk_rt_create_composite_descriptor_set( void );
void vk_rt_update_composite_descriptor_set( void );
void vk_rt_composite( void );

// ReLAX Denoising functions
void vk_rt_create_denoise_resources( uint32_t width, uint32_t height );
void vk_rt_destroy_denoise_resources( void );
void vk_rt_create_denoise_pipeline( void );
void vk_rt_denoise( uint32_t width, uint32_t height );

// DLSS (NVIDIA Deep Learning Super Sampling) functions
void vk_dlss_init( void );
void vk_dlss_shutdown( void );
qboolean vk_dlss_is_supported( void );
void vk_dlss_create_resources( uint32_t renderWidth, uint32_t renderHeight, uint32_t outputWidth, uint32_t outputHeight );
void vk_dlss_destroy_resources( void );
void vk_dlss_evaluate( VkCommandBuffer cmdBuffer, VkImage colorImage, VkImage depthImage, VkImage motionVectorImage, uint32_t frameIndex );

// Mesh Shaders (VK_EXT_mesh_shader) functions
void vk_mesh_shaders_init( void );
void vk_mesh_shaders_shutdown( void );
qboolean vk_mesh_shaders_is_supported( void );
qboolean vk_mesh_shaders_use_fallback( void );
uint32_t vk_mesh_shaders_meshlet_count( void );
void vk_mesh_shaders_generate_meshlets( void *vertices, uint32_t vertexCount, void *indices, uint32_t indexCount );
void vk_mesh_shaders_draw( uint32_t meshletCount );
void vk_mesh_shaders_create_pipeline( void );

// Virtual Texturing functions
void vk_virtual_texture_init( void );
void vk_virtual_texture_shutdown( void );
void vk_virtual_texture_request_page( uint32_t pageX, uint32_t pageY, uint32_t mipLevel );
void vk_virtual_texture_update_page_table( void );

// Advanced Material Features functions
void vk_advanced_materials_init( void );
void vk_advanced_materials_shutdown( void );
void vk_advanced_materials_parse( void *material, const char *shaderText );
void vk_advanced_materials_update_uniform( void *material, void *uniformData );

// GPU Particle Systems functions
void vk_particles_init( void );
void vk_particles_shutdown( void );
void vk_particles_simulate( float deltaTime );
void vk_particles_render( void );

// Variable Rate Shading functions
void vk_vrs_init( void );
void vk_vrs_shutdown( void );
void vk_vrs_create_resources( uint32_t width, uint32_t height );
void vk_vrs_destroy_resources( void );
void vk_vrs_generate_image( VkCommandBuffer cmdBuffer );
void vk_vrs_apply_shading_rate( VkCommandBuffer cmdBuffer );

void VBO_PrepareQueues( void );
void VBO_RenderIBOItems( void );
void VBO_ClearQueue( void );

// cubemap
#ifdef VK_CUBEMAP
void vk_clear_cube_color( image_t *image, VkClearColorValue color );
void vk_begin_cubemap_render_pass( void );
void vk_create_cubemap_prefilter( void );
void vk_destroy_cubemap_prefilter( void );
#endif

#ifdef VK_PBR_BRDFLUT
void vk_create_brfdlut( void );
#endif

#ifdef USE_VBO
void vk_release_vbo( void );

// Vulkan performance systems
qboolean vk_bindless_init(void);
void vk_bindless_shutdown(void);
qboolean vk_shader_cache_init(void);
void vk_shader_cache_shutdown(void);
qboolean vk_async_compile_init(void);
void vk_async_compile_shutdown(void);
#endif

typedef struct vk_tess_s {
	VkCommandBuffer command_buffer;

	VkSemaphore image_acquired;
	uint32_t	swapchain_image_index;
	qboolean	swapchain_image_acquired;
#ifdef USE_UPLOAD_QUEUE
	VkSemaphore rendering_finished2;
#endif
	VkFence rendering_finished_fence;
	qboolean waitForFence;

	VkBuffer vertex_buffer;
	byte *vertex_buffer_ptr; // pointer to mapped vertex buffer
	VkDeviceSize vertex_buffer_offset;

	VkDescriptorSet uniform_descriptor;
	uint32_t		uniform_read_offset;
#ifdef USE_VK_PBR
	uint32_t			camera_ubo_offset;
	VkDeviceSize		buf_offset[9];
	VkDeviceSize		vbo_offset[10];
#else
	VkDeviceSize		buf_offset[8];
	VkDeviceSize		vbo_offset[8];
#endif

	VkBuffer		curr_index_buffer;
	uint32_t		curr_index_offset;

	struct {
		uint32_t		start, end;
		VkDescriptorSet	current[VK_DESC_COUNT]; // 0:uniform, 1:color0, 2:color1, 3:color2, 4:fog, 5:brdf lut, 6:normal, 7:physical, 9:(unused)prefilterd-envmap
		uint32_t		offset[3]; // 0 (uniform) and 5 (storage)
	} descriptor_set;

	Vk_Depth_Range		depth_range;
	VkPipeline			last_pipeline;

	uint32_t num_indexes; // value from most recent vk_bind_index() call

	VkRect2D scissor_rect;

	// MVP matrix caching per entity to avoid redundant updates
	struct {
		int last_entity_num;
		float cached_mvp[16];
		qboolean mvp_valid;
	} mvp_cache;
} vk_tess_t;


// Vk_Instance contains engine-specific vulkan resources that persist entire renderer lifetime.
// This structure is initialized/deinitialized by vk_initialize/vk_shutdown functions correspondingly.
typedef struct {
	VkPhysicalDevice physical_device;
	VkSurfaceFormatKHR base_format;
	VkSurfaceFormatKHR present_format;

	uint32_t queue_family_index;
	VkDevice device;
	VkQueue queue;

	VkSwapchainKHR swapchain;
	uint32_t swapchain_image_count;
	VkImage swapchain_images[MAX_SWAPCHAIN_IMAGES];
	VkImageView swapchain_image_views[MAX_SWAPCHAIN_IMAGES];
	VkSemaphore swapchain_rendering_finished[MAX_SWAPCHAIN_IMAGES];
	//uint32_t swapchain_image_index;

	// Timeline semaphores for improved synchronization
	VkSemaphore timeline_semaphore;
	uint64_t timeline_value;

	// Bindless texture system
	qboolean bindless_supported;
	VkDescriptorPool bindless_descriptor_pool;
	VkDescriptorSetLayout bindless_set_layout;
	VkDescriptorSet bindless_descriptor_set;
	uint32_t bindless_texture_count;
	VkSampler bindless_sampler;
	
	// Bindless buffer system
	VkDescriptorPool bindless_buffer_descriptor_pool;
	VkDescriptorSetLayout bindless_buffer_set_layout;
	VkDescriptorSet bindless_buffer_descriptor_set;
	uint32_t bindless_buffer_count;
	uint32_t bindless_storage_buffer_count;
	uint32_t bindless_uniform_buffer_count;

	VkCommandPool command_pool;
#ifdef USE_UPLOAD_QUEUE
	VkCommandBuffer staging_command_buffer;
#endif

	VkDeviceMemory image_memory[ MAX_ATTACHMENTS_IN_POOL ];
	uint32_t image_memory_count;

	struct {
		VkRenderPass main;
		VkRenderPass screenmap;
		VkRenderPass gamma;
		VkRenderPass capture;
		VkRenderPass bloom_extract;
		VkRenderPass blur[VK_NUM_BLOOM_PASSES*2]; // horizontal-vertical pairs
		VkRenderPass post_bloom;
#ifdef VK_PBR_BRDFLUT
		VkRenderPass brdflut;
#endif
		VkRenderPass cubemap;
	} render_pass;

	VkDescriptorPool descriptor_pool;
	VkDescriptorSetLayout set_layout_sampler;	// combined image sampler
	VkDescriptorSetLayout set_layout_uniform;	// dynamic uniform buffer
	VkDescriptorSetLayout set_layout_storage;	// feedback buffer
#ifdef USE_VK_PBR
	VkDescriptorSetLayout set_layout_material;	// material parameters storage buffer
#endif

	VkPipelineLayout pipeline_layout;			// default shaders
	VkPipelineLayout pipeline_layout_storage;	// flare test shader layout
	VkPipelineLayout pipeline_layout_post_process;	// post-processing
	VkPipelineLayout pipeline_layout_blend;		// post-processing
#ifdef VK_PBR_BRDFLUT
	VkPipelineLayout pipeline_layout_brdflut;
#endif

	VkDescriptorSet color_descriptor;

	VkImage color_image;
	VkImageView color_image_view;

	VkImage bloom_image[1+VK_NUM_BLOOM_PASSES*2];
	VkImageView bloom_image_view[1+VK_NUM_BLOOM_PASSES*2];

	VkDescriptorSet bloom_image_descriptor[1+VK_NUM_BLOOM_PASSES*2];

	// Enhanced post-processing resources
	VkImage ssao_image;
	VkImageView ssao_image_view;

	VkImage ssr_image;
	VkImageView ssr_image_view;

	VkImage dof_image;
	VkImageView dof_image_view;

	VkImage coc_image;
	VkImageView coc_image_view;

	VkImage bokeh_sprite_image;
	VkImageView bokeh_sprite_image_view;

	VkImage motion_blur_image;
	VkImageView motion_blur_image_view;

	VkImage velocity_image;
	VkImageView velocity_image_view;

	VkImage velocity_tiles_image;
	VkImageView velocity_tiles_image_view;

	VkImage color_grading_image;
	VkImageView color_grading_image_view;

	VkImage lut_image;
	VkImageView lut_image_view;

	VkImage heat_distortion_image;
	VkImageView heat_distortion_image_view;

	VkImage heat_mask_image;
	VkImageView heat_mask_image_view;

	VkImage noise_image;
	VkImageView noise_image_view;

	// Blue noise for SSAO/SSR
	VkImage blue_noise_image;
	VkImageView blue_noise_image_view;

	// Post-processing pipelines and layouts
	VkPipeline ssao_pipeline;
	VkPipeline ssr_pipeline;
	VkPipeline bloom_pipeline;
	VkPipeline dof_pipeline;
	VkPipeline motion_blur_pipeline;
	VkPipeline velocity_tiles_pipeline;
	VkPipeline color_grading_pipeline;
	VkPipeline heat_distortion_pipeline;

	VkPipelineLayout ssao_layout;
	VkPipelineLayout ssr_layout;
	VkPipelineLayout bloom_layout;
	VkPipelineLayout dof_layout;
	VkPipelineLayout motion_blur_layout;
	VkPipelineLayout velocity_tiles_layout;
	VkPipelineLayout color_grading_layout;
	VkPipelineLayout heat_distortion_layout;

	// Post-processing descriptor set layouts
	VkDescriptorSetLayout ssao_descriptor_layout;
	VkDescriptorSetLayout ssr_descriptor_layout;
	VkDescriptorSetLayout bloom_descriptor_layout;
	VkDescriptorSetLayout dof_descriptor_layout;
	VkDescriptorSetLayout motion_blur_descriptor_layout;
	VkDescriptorSetLayout velocity_tiles_descriptor_layout;
	VkDescriptorSetLayout color_grading_descriptor_layout;
	VkDescriptorSetLayout heat_distortion_descriptor_layout;

	// Post-processing descriptor sets
	VkDescriptorSet ssao_descriptor;
	VkDescriptorSet ssr_descriptor;
	VkDescriptorSet bloom_descriptor;
	VkDescriptorSet dof_descriptor;
	VkDescriptorSet motion_blur_descriptor;
	VkDescriptorSet velocity_tiles_descriptor;
	VkDescriptorSet color_grading_descriptor;
	VkDescriptorSet heat_distortion_descriptor;

	VkImage depth_image;
	VkImageView depth_image_view;

	VkImage msaa_image;
	VkImageView msaa_image_view;

	// screenMap
	struct {
		VkDescriptorSet color_descriptor;
		VkImage color_image;
		VkImageView color_image_view;

		VkImage color_image_msaa;
		VkImageView color_image_view_msaa;

		VkImage depth_image;
		VkImageView depth_image_view;

	} screenMap;

	// cubemap
	struct {
		VkImage			depth_image;
		VkImageView		depth_image_view;
		VkImage			color_image_msaa;
		VkImageView		color_image_view_msaa[7];
		VkDescriptorSet color_descriptor;
		VkImage			color_image;
		VkImageView		color_image_view[7];
	} cubeMap;

	struct {
		VkImage image;
		VkImageView image_view;
	} capture;

#ifdef VK_PBR_BRDFLUT
	VkImage			brdflut_image;
	VkImageView		brdflut_image_view;
	VkDescriptorSet brdflut_image_descriptor;
#endif

	struct {
		VkFramebuffer blur[VK_NUM_BLOOM_PASSES*2];
		VkFramebuffer bloom_extract;
		VkFramebuffer main[MAX_SWAPCHAIN_IMAGES];
		VkFramebuffer gamma[MAX_SWAPCHAIN_IMAGES];
		VkFramebuffer screenmap;
		VkFramebuffer capture;
#ifdef VK_PBR_BRDFLUT
		VkFramebuffer brdflut;
#endif
		VkFramebuffer cubemap[6];
	} framebuffers;

#ifdef USE_UPLOAD_QUEUE
	VkSemaphore rendering_finished;	// reference to vk.cmd->rendering_finished2
	VkSemaphore image_uploaded2;
	VkSemaphore image_uploaded;		// reference to vk.image_uploaded2
#endif

	vk_tess_t tess[ NUM_COMMAND_BUFFERS ], *cmd;
	int cmd_index;

	struct {
		VkBuffer		buffer;
		byte			*buffer_ptr;
		VkDeviceMemory	memory;
		VkDescriptorSet	descriptor;
	} storage;

	uint32_t uniform_item_size;
	uint32_t uniform_camera_item_size;
	uint32_t uniform_alignment;
	uint32_t storage_alignment;

	struct {
		VkBuffer vertex_buffer;
		VkDeviceMemory	buffer_memory;
	} vbo;

	// host visible memory that holds vertex, index and uniform data
	VkDeviceMemory geometry_buffer_memory;
	VkDeviceSize geometry_buffer_size;
	VkDeviceSize geometry_buffer_size_new;

	// Geometry buffer size history for pre-allocation
	struct {
		VkDeviceSize sizes[16]; // Track last 16 frames
		uint32_t index;
		uint32_t count;
		VkDeviceSize max_size;
	} geometry_buffer_history;

	// statistics
	struct {
		VkDeviceSize vertex_buffer_max;
		uint32_t push_size;
		uint32_t push_size_max;
	} stats;

	// GPU timing queries for performance profiling
	struct {
		VkQueryPool timestamp_query_pool;
		uint32_t query_count;
		uint32_t current_query;
		qboolean enabled;
		uint64_t frame_times[64]; // Ring buffer for frame times
		uint32_t frame_time_index;
	} timing;

	// Enhanced profiling system
	struct {
		qboolean enabled;
		// GPU profiling
		double vertex_time_ms;
		double fragment_time_ms;
		double compute_time_ms;
		uint64_t memory_bandwidth_bytes;
		uint32_t texture_cache_hits;
		uint32_t texture_cache_misses;
		float gpu_utilization;
		// CPU profiling
		double render_thread_time_ms;
		double command_recording_time_ms;
		double resource_upload_time_ms;
		double synchronization_wait_time_ms;
		// Performance counters
		uint32_t draw_calls_per_frame;
		uint32_t triangles_per_frame;
		uint32_t memory_allocations_per_frame;
		// Historical data
		double frame_time_history[128];
		uint32_t frame_time_history_index;
		float frame_time_variance;
	} profiling;

	// Debug overlay system
	struct {
		qboolean enabled;
		uint32_t draw_calls;
		uint32_t triangles;
		VkDeviceSize gpu_memory_used;
		VkDeviceSize gpu_memory_total;
		VkDeviceSize texture_memory;
		VkDeviceSize buffer_memory;
		float frame_time_ms;
		float frame_time_variance;
	} debug_overlay;

	// Shader hot reload system
	struct {
		qboolean enabled;
		uint32_t shaders_reloaded;
		uint32_t pipelines_recreated;
		uint32_t reload_errors;
		qboolean file_watcher_active;
	} hot_reload;

	//
	// Shader modules.
	//
	struct {
		struct {
#ifdef USE_VK_PBR
			VkShaderModule gen[2][3][2][2][2]; // pbr[0,1], tx[0,1,2], cl[0,1] env0[0,1] fog[0,1]
			VkShaderModule ident1[2][2][2][2]; // pbr[0,1], tx[0,1], env0[0,1] fog[0,1]
			VkShaderModule fixed[2][2][2][2];  // pbr[0,1], tx[0,1], env0[0,1] fog[0,1]
#else
			VkShaderModule gen[3][2][2][2]; // tx[0,1,2], cl[0,1] env0[0,1] fog[0,1]
			VkShaderModule ident1[2][2][2]; // tx[0,1], env0[0,1] fog[0,1]
			VkShaderModule fixed[2][2][2];  // tx[0,1], env0[0,1] fog[0,1]
#endif			
			VkShaderModule light[2];        // fog[0,1]
		} vert;
		struct {
			VkShaderModule gen0_df;
#ifdef USE_VK_PBR
			VkShaderModule gen[2][3][2][2]; // pbr[0,1], tx[0,1,2] cl[0,1] fog[0,1]
			VkShaderModule ident1[2][2][2]; // pbr[0,1], tx[0,1], fog[0,1]
			VkShaderModule fixed[2][2][2];  // pbr[0,1], tx[0,1], fog[0,1]
			VkShaderModule ent[2][1][2];    // pbr[0,1], tx[0], fog[0,1]
#else
			VkShaderModule gen[3][2][2]; // tx[0,1,2] cl[0,1] fog[0,1]
			VkShaderModule ident1[2][2]; // tx[0,1], fog[0,1]
			VkShaderModule fixed[2][2];  // tx[0,1], fog[0,1]
			VkShaderModule ent[1][2];    // tx[0], fog[0,1]
#endif
			VkShaderModule light[2][2];  // linear[0,1] fog[0,1]
		} frag;


		VkShaderModule color_fs;
		VkShaderModule color_vs;

		VkShaderModule bloom_fs;
		VkShaderModule blur_fs;
		VkShaderModule blend_fs;

		VkShaderModule gamma_fs;
		VkShaderModule gamma_vs;

		VkShaderModule fog_fs;
		VkShaderModule fog_vs;

		VkShaderModule dot_fs;
		VkShaderModule dot_vs;

#ifdef VK_PBR_BRDFLUT
		VkShaderModule brdflut_fs;
#endif
		VkShaderModule filtercube_vs;
		VkShaderModule filtercube_gm;
		VkShaderModule irradiancecube_fs;
		VkShaderModule prefilterenvmap_fs;

		// Ray tracing shaders
		VkShaderModule rt_primary_rays_rgen;
		VkShaderModule rt_miss_rmiss;
		VkShaderModule rt_closesthit_rchit;
		VkShaderModule rt_composite_fs;
		
		// Compute shader modules for post-processing
		VkShaderModule gamma_comp;
		VkShaderModule tonemap_comp;
		VkShaderModule rt_relax_comp;
		VkShaderModule style_comp;
	VkShaderModule film_grain_comp;
	VkShaderModule lens_distortion_comp;
	VkShaderModule histogram_comp;
	VkShaderModule auto_exposure_comp;
	VkShaderModule checkerboard_interleave_comp;
	VkShaderModule vignette_comp;
		// Enhanced post-processing effects
		VkShaderModule ssao_comp;
		VkShaderModule ssr_comp;
		VkShaderModule bloom_comp;
		VkShaderModule depth_of_field_comp;
		VkShaderModule motion_blur_comp;
		VkShaderModule velocity_tiles_comp;
		VkShaderModule color_grading_comp;
		VkShaderModule heat_distortion_comp;
	VkShaderModule vrs_generate_comp;
		
		// GIBS compute shader modules
		VkShaderModule gibs_spawn_comp;
		VkShaderModule gibs_update_comp;
	} modules;

	VkPipelineCache pipelineCache;

	VK_Pipeline_t pipelines[ MAX_VK_PIPELINES ];
	uint32_t pipelines_count;
	uint32_t pipelines_world_base;

	// pipeline statistics
	int32_t pipeline_create_count;

	//
	// Standard pipelines.
	//
	uint32_t skybox_pipeline;

	// dim 0: 0 - front side, 1 - back size
	// dim 1: 0 - normal view, 1 - mirror view
	uint32_t shadow_volume_pipelines[2][2];
	uint32_t shadow_finish_pipeline;

	// dim 0 is based on fogPass_t: 0 - corresponds to FP_EQUAL, 1 - corresponds to FP_LE.
	// dim 1 is directly a cullType_t enum value.
	// dim 2 is a polygon offset value (0 - off, 1 - on).
	uint32_t fog_pipelines[2][3][2];

	// dim 0 is based on dlight additive flag: 0 - not additive, 1 - additive
	// dim 1 is directly a cullType_t enum value.
	// dim 2 is a polygon offset value (0 - off, 1 - on).
#ifdef USE_LEGACY_DLIGHTS
	uint32_t dlight_pipelines[2][3][2];
#endif

	// cullType[3], polygonOffset[2], fogStage[2], absLight[2]
#ifdef USE_PMLIGHT
	uint32_t dlight_pipelines_x[3][2][2][2];
	uint32_t dlight1_pipelines_x[3][2][2][2];
#endif

	// debug visualization pipelines
	uint32_t tris_debug_pipeline;
	uint32_t tris_mirror_debug_pipeline;
	uint32_t tris_debug_green_pipeline;
	uint32_t tris_mirror_debug_green_pipeline;
	uint32_t tris_debug_red_pipeline;
	uint32_t tris_mirror_debug_red_pipeline;

	uint32_t normals_debug_pipeline;
	uint32_t surface_debug_pipeline_solid;
	uint32_t surface_debug_pipeline_outline;
	uint32_t images_debug_pipeline;
	uint32_t images_debug_pipeline2;
	uint32_t surface_beam_pipeline;
	uint32_t surface_axis_pipeline;
	uint32_t dot_pipeline;

	VkPipeline gamma_pipeline;
	VkPipeline capture_pipeline;
	VkPipeline bloom_extract_pipeline;
	VkPipeline blur_pipeline[VK_NUM_BLOOM_PASSES*2]; // horizontal & vertical pairs
	VkPipeline bloom_blend_pipeline;
#ifdef USE_VULKAN_RAY_TRACING
	VkPipeline rt_composite_pipeline;
	VkDescriptorSet rt_composite_descriptor;
#endif
	
	// Compute shader post-processing pipelines
	VkPipeline gamma_compute_pipeline;
	VkPipeline tonemap_compute_pipeline;
	VkPipeline style_compute_pipeline;
	VkPipeline film_grain_compute_pipeline;
	VkPipeline lens_distortion_compute_pipeline;
	VkPipeline histogram_compute_pipeline;
	VkPipeline auto_exposure_compute_pipeline;
	VkPipeline checkerboard_interleave_compute_pipeline;
	VkPipeline vignette_compute_pipeline;
	VkPipeline vrs_generate_compute_pipeline;
	VkPipelineLayout compute_pipeline_layout;
	VkDescriptorSetLayout compute_descriptor_set_layout;
	VkDescriptorSet compute_descriptor_set;

	// Style transfer output
	VkImage style_image;
	VkImageView style_image_view;
	
	// DLSS (NVIDIA Deep Learning Super Sampling)
	struct {
		qboolean supported;
		qboolean initialized;
		void *dlssContext; // NVSDK_NGX_VK_Context or similar (opaque pointer)
		void *dlssFeatureHandle; // NVSDK_NGX_Handle for DLSS feature
		void *dlssLibraryHandle; // Handle to loaded DLSS SDK library (DLL/SO)
		VkImage dlssOutputImage; // Upscaled output image
		VkImageView dlssOutputImageView;
		VkDeviceMemory dlssOutputImageMemory;
		uint32_t dlssOutputWidth;
		uint32_t dlssOutputHeight;
		VkImage dlssDepthImage; // Depth buffer for DLSS
		VkImageView dlssDepthImageView;
		VkDeviceMemory dlssDepthImageMemory;
		VkImage dlssMotionVectorImage; // Motion vectors for DLSS
		VkImageView dlssMotionVectorImageView;
		VkDeviceMemory dlssMotionVectorImageMemory;
		VkImage dlssColorImage; // Input color buffer
		VkImageView dlssColorImageView;
		uint32_t renderWidth; // Internal render resolution
		uint32_t renderHeight;
		uint32_t outputWidth; // Display resolution
		uint32_t outputHeight;
		int qualityMode; // DLSS quality mode (0=Performance, 1=Balanced, 2=Quality, 3=Ultra Quality)
		qboolean sharpeningEnabled;
		float sharpening;
	} dlss;
	
	// Mesh Shaders (VK_EXT_mesh_shader)
	struct {
		qboolean meshShaderSupported;
		qboolean taskShaderSupported;
		qboolean active;          // mesh shader path enabled by user + device
		qboolean useFallback;     // fall back to classic path
		uint32_t meshletCount;    // last generated meshlets
		uint32_t meshletCapacity;
		meshlet_info_t *meshlets; // CPU metadata
		VkPipeline meshShaderPipeline;
		VkPipelineLayout meshShaderPipelineLayout;
		VkDescriptorSetLayout meshShaderDescriptorSetLayout;
		VkDescriptorSet meshShaderDescriptorSet;
		VkShaderModule mesh_task;
		VkShaderModule mesh_mesh;
	} mesh;
	
	// Virtual Texturing
	struct {
		VkImage vt_page_table_image;
		VkImageView vt_page_table_view;
		VkDeviceMemory vt_page_table_memory;
		VkImage vt_page_cache_image;
		VkImageView vt_page_cache_view;
		VkDeviceMemory vt_page_cache_memory;
		VkBuffer vt_feedback_buffer;
		VkDeviceMemory vt_feedback_memory;
		VkPipeline vt_update_pipeline;
		VkPipelineLayout vt_update_pipeline_layout;
		VkDescriptorSetLayout vt_update_descriptor_set_layout;
		VkDescriptorSet vt_update_descriptor_set;
	} vt;
	
	// Advanced Material Features
	struct {
		qboolean clearcoatEnabled;
		qboolean anisotropyEnabled;
		qboolean sheenEnabled;
		qboolean sssEnabled;
		qboolean materialLODEnabled;
	} materials;
	
	// GPU Particle Systems
	struct {
		VkBuffer particleBuffer;
		VkDeviceMemory particleMemory;
		uint32_t particleCount;
		uint32_t particleMax;
		VkPipeline particleComputePipeline;
		VkPipelineLayout particleComputePipelineLayout;
		VkDescriptorSetLayout particleComputeDescriptorSetLayout;
		VkDescriptorSet particleComputeDescriptorSet;
		VkPipeline particleRenderPipeline;
		VkPipelineLayout particleRenderPipelineLayout;
		VkDescriptorSetLayout particleRenderDescriptorSetLayout;
		VkDescriptorSet particleRenderDescriptorSet;
	} particles;
	
#ifdef VK_PBR_BRDFLUT
	VkPipeline brdflut_pipeline;
#endif

	uint32_t frame_count;
	qboolean active;
	qboolean wideLines;
	qboolean samplerAnisotropy;
	qboolean fragmentStores;
	qboolean dedicatedAllocation;
	qboolean debugMarkers;
	qboolean rayTracingSupported;

	float maxAnisotropy;
	float maxLod;

	VkFormat color_format;
	VkFormat capture_format;
	VkFormat depth_format;
	VkFormat bloom_format;

	VkImageLayout initSwapchainLayout;

	qboolean clearAttachment;		// requires VK_IMAGE_USAGE_TRANSFER_DST_BIT for swapchains
	qboolean fboActive;
	qboolean blitEnabled;
	qboolean msaaActive;
#ifdef USE_VK_PBR
	qboolean pbrActive;
#endif
#ifdef VK_CUBEMAP
	qboolean cubemapActive;
#endif
	qboolean offscreenRender;

	qboolean windowAdjusted;
	int		blitX0;
	int		blitY0;
	int		blitFilter;

	uint32_t renderWidth;
	uint32_t renderHeight;

	float renderScaleX;
	float renderScaleY;

	// Dynamic resolution (experimental)
	struct {
		qboolean enabled;
		float minScale;
		float maxScale;
		float targetMs;
		float currentScale;
		float smoothedScale;
	} dynres;

	// Variable Rate Shading (VRS)
	struct {
		qboolean supported;
		qboolean enabled;
		int mode;           // 0=disabled, 1=center-focused, 2=distance-based, 3=center+distance
		float centerRadius; // Radius of high-quality center region (0.0-1.0)
		float falloffStart; // Distance where quality decreases (0.0-1.0)
		int minRate;        // Minimum shading rate (1, 2, 4)
		int maxRate;        // Maximum shading rate (1, 2, 4)
	} vrs;

	renderPass_t renderPassIndex;

	uint32_t screenMapWidth;
	uint32_t screenMapHeight;
	uint32_t screenMapSamples;

	uint32_t image_chunk_size;

	uint32_t maxBoundDescriptorSets;

#ifdef USE_VMA
	VmaAllocator allocator;
#endif

#ifdef USE_UPLOAD_QUEUE
	VkFence aux_fence;
	qboolean aux_fence_wait;
#endif

	struct staging_buffer_s {
		VkBuffer handle;
		VkDeviceMemory memory;
		VkDeviceSize size;
		byte *ptr; // pointer to mapped staging buffer
#ifdef USE_UPLOAD_QUEUE
		VkDeviceSize offset;
#endif
	} staging_buffer;

	struct samplers_s {
		int count;
		Vk_Sampler_Def def[MAX_VK_SAMPLERS];
		VkSampler handle[MAX_VK_SAMPLERS];
		int filter_min;
		int filter_max;
	} samplers;

	struct defaults_t {
		VkDeviceSize staging_size;
		VkDeviceSize geometry_size;
	} defaults;

	// Ray tracing structures
	struct {
		VkPhysicalDeviceRayTracingPipelinePropertiesKHR properties;
		VkAccelerationStructureKHR tlas; // Top-Level Acceleration Structure
		VkBuffer tlasBuffer;
		VkDeviceMemory tlasMemory;
		VkDeviceAddress tlasDeviceAddress;
		
		VkAccelerationStructureKHR *blas; // Bottom-Level Acceleration Structures (per model)
		VkBuffer *blasBuffers;
		VkDeviceMemory *blasMemory;
		uint32_t blasCount;
		uint32_t blasCapacity;
		
		// BLAS reuse and compaction
		uint64_t *blasHashes; // Hash of vertex/index data for reuse detection
		VkAccelerationStructureKHR *blasCompacted; // Compacted BLAS (if compaction enabled)
		VkBuffer *blasCompactedBuffers;
		VkDeviceMemory *blasCompactedMemory;
		qboolean *blasNeedsCompaction; // Flag to track which BLAS need compaction
		uint32_t *blasUnusedSlots; // Track unused BLAS slots for reuse
		uint32_t unusedSlotCount;
		
		VkBuffer scratchBuffer;
		VkDeviceMemory scratchMemory;
		VkDeviceSize scratchBufferSize;
		
		VkPipeline raytracingPipeline;
		VkPipelineLayout raytracingPipelineLayout;
		VkDescriptorSetLayout raytracingDescriptorSetLayout;
		VkDescriptorSet raytracingDescriptorSet;
		
		// Shader Binding Table
		VkBuffer sbtBuffer;
		VkDeviceMemory sbtMemory;
		VkDeviceSize raygenRegionSize;
		VkDeviceSize missRegionSize;
		VkDeviceSize hitRegionSize;
		VkDeviceSize callableRegionSize;
		VkDeviceSize raygenRegionOffset;
		VkDeviceSize missRegionOffset;
		VkDeviceSize hitRegionOffset;
		VkDeviceSize callableRegionOffset;
		
		VkStridedDeviceAddressRegionKHR raygenShaderBindingTable;
		VkStridedDeviceAddressRegionKHR missShaderBindingTable;
		VkStridedDeviceAddressRegionKHR hitShaderBindingTable;
		VkStridedDeviceAddressRegionKHR callableShaderBindingTable;
		
		// Output image for ray tracing
		VkImage outputImage;
		VkImageView outputImageView;
		VkDeviceMemory outputImageMemory;
		uint32_t outputImageWidth;
		uint32_t outputImageHeight;
		
		// Temporal accumulation buffers
		VkImage historyImage; // Previous frame RT output
		VkImageView historyImageView;
		VkDeviceMemory historyImageMemory;
		VkImage motionVectorImage; // Motion vectors for reprojection (RG16F)
		VkImageView motionVectorImageView;
		VkDeviceMemory motionVectorImageMemory;
		
		// Uniform buffer for camera data
		VkBuffer uniformBuffer;
		VkDeviceMemory uniformBufferMemory;
		
		// Previous frame matrices for motion vector calculation
		mat4_t previousViewInverse;
		mat4_t previousProjInverse;
		vec3_t previousCameraPos;
		qboolean previousMatricesValid;
		
		// Blue noise texture for denoising
		image_t *blueNoiseTexture;
		
		// ReLAX Denoising buffers
		VkImage denoiseNormalBuffer; // G-buffer normals
		VkImageView denoiseNormalBufferView;
		VkDeviceMemory denoiseNormalBufferMemory;
		VkImage denoiseDepthBuffer; // Depth buffer for denoising
		VkImageView denoiseDepthBufferView;
		VkDeviceMemory denoiseDepthBufferMemory;
		VkImage denoiseVarianceBuffer; // Variance buffer
		VkImageView denoiseVarianceBufferView;
		VkDeviceMemory denoiseVarianceBufferMemory;
		VkImage denoiseHistoryBuffer; // Denoised history
		VkImageView denoiseHistoryBufferView;
		VkDeviceMemory denoiseHistoryBufferMemory;
		VkPipeline denoiseComputePipeline; // ReLAX compute pipeline
		VkPipelineLayout denoisePipelineLayout; // Pipeline layout for denoising
		VkDescriptorSetLayout denoiseDescriptorSetLayout;
		VkDescriptorSet denoiseDescriptorSet;

		// Histogram and auto-exposure
		VkBuffer histogramBuffer;
		VkDeviceMemory histogramBufferMemory;
		VkBuffer exposureBuffer;
		VkDeviceMemory exposureBufferMemory;
		VkDescriptorSetLayout histogramDescriptorSetLayout;
		VkDescriptorSet histogramDescriptorSet;
		VkDescriptorSetLayout autoExposureDescriptorSetLayout;
		VkDescriptorSet autoExposureDescriptorSet;

		// Portal lights system
		VkBuffer portalLightsBuffer;
		VkDeviceMemory portalLightsBufferMemory;
		VkDescriptorSetLayout portalLightsDescriptorSetLayout;
		VkDescriptorSet portalLightsDescriptorSet;

		// TLAS update optimization
		VkAccelerationStructureInstanceKHR *previousInstances; // Previous frame's instance transforms
		uint32_t previousInstanceCount;
		qboolean tlasNeedsRebuild; // Set to true when geometry changes, false when only transforms change
		qboolean tlasAllowsUpdate; // Set when TLAS is created with ALLOW_UPDATE_BIT
		
		qboolean initialized;
	} rt;
	
#ifdef USE_VULKAN_RAY_TRACING
	// GIBS (Global Illumination Based on Surfels) system
	struct {
		qboolean enabled;
		qboolean initialized;
		
		// Surfel storage
		VkBuffer surfelBuffer;
		VkDeviceMemory surfelBufferMemory;
		VkDeviceAddress surfelBufferAddress;
		uint32_t surfelCount;
		uint32_t surfelCapacity;
		
		VkBuffer surfelIndirectBuffer;
		VkDeviceMemory surfelIndirectBufferMemory;
		
		// Compute pipelines
		VkPipeline updatePipeline;
		VkPipelineLayout updatePipelineLayout;
		VkDescriptorSetLayout updateDescriptorSetLayout;
		VkDescriptorSet updateDescriptorSet;
		
		VkPipeline spawnPipeline;
		VkPipelineLayout spawnPipelineLayout;
		VkDescriptorSetLayout spawnDescriptorSetLayout;
		VkDescriptorSet spawnDescriptorSet;
		
		// Frame tracking
		uint32_t frameCounter;
		uint32_t updateFrameOffset;
		
		// Statistics
		uint32_t activeSurfelCount;
		uint32_t updatedSurfelCount;
	} gibs;
#endif
	
	// GPU-driven culling and instancing system
	struct {
		qboolean enabled;
		qboolean initialized;
		
		VkBuffer instanceBuffer;
		VkDeviceMemory instanceBufferMemory;
		VkDeviceAddress instanceBufferAddress;
		uint32_t instanceCount;
		uint32_t instanceCapacity;
		
		VkBuffer drawCommandBuffer;
		VkDeviceMemory drawCommandBufferMemory;
		uint32_t drawCommandCount;
		
		VkBuffer cullDataBuffer;
		VkDeviceMemory cullDataBufferMemory;
		
		VkPipeline cullPipeline;
		VkPipelineLayout cullPipelineLayout;
		VkDescriptorSetLayout cullDescriptorSetLayout;
		VkDescriptorSet cullDescriptorSet;
		
		VkPipeline instancePipeline;
		VkPipelineLayout instancePipelineLayout;
		VkDescriptorSetLayout instanceDescriptorSetLayout;
		VkDescriptorSet instanceDescriptorSet;
		
		uint32_t culledInstanceCount;
		uint32_t visibleInstanceCount;
	} gpuCulling;

	// Procedural dressing state
	struct {
		qboolean enabled;
		qboolean initialized;
		qboolean dirty;
		uint32_t ruleCount;
		proc_rule_t rules[VK_MAX_PROC_RULES];
		uint32_t biomeCount;
		proc_biome_t biomes[VK_MAX_PROC_BIOMES];
		uint32_t instanceCount;
	} procDressing;
	
	// Material system with runtime parameters
	struct {
		qboolean enabled;
		qboolean initialized;
		
		material_params_t *materialParams;
		uint32_t materialCount;
		uint32_t materialCapacity;
		
		VkBuffer materialBuffer;
		VkDeviceMemory materialBufferMemory;
		VkDeviceAddress materialBufferAddress;
		VkDescriptorSet materialDescriptorSet; // Descriptor set for material buffer
		
		VkPipeline updatePipeline;
		VkPipelineLayout updatePipelineLayout;
		VkDescriptorSetLayout updateDescriptorSetLayout;
		VkDescriptorSet updateDescriptorSet;
		
		void *luaState;
	} materialSystem;
	
	// Cell streaming system
	struct {
		qboolean enabled;
		qboolean initialized;
		
		stream_cell_t cells[MAX_STREAM_CELLS];
		uint32_t cellCount;
		uint32_t activeCellCount;
		
		int32_t currentCellX, currentCellY, currentCellZ;
		
		uint32_t loadQueue[MAX_STREAM_CELLS];
		uint32_t loadQueueCount;
		
		uint32_t unloadQueue[MAX_STREAM_CELLS];
		uint32_t unloadQueueCount;
		
		uint32_t frameCounter;
		uint32_t cellsLoadedThisFrame;
		uint32_t cellsUnloadedThisFrame;
	} cellStreaming;
	
	// Atmosphere and mood system
	struct {
		qboolean enabled;
		qboolean initialized;
		
		atmosphere_params_t currentParams;
		atmosphere_params_t targetParams;
		atmosphere_params_t baseParams;
		
		atmosphere_preset_t currentPreset;
		float transitionTime;
		float transitionDuration;
		
		VkBuffer atmosphereBuffer;
		VkDeviceMemory atmosphereBufferMemory;
		
		void *luaState;
	} atmosphere;

	// Variable Rate Shading resources
	VkImage vrsImage;
	VkImageView vrsImageView;
	VkDeviceMemory vrsImageMemory;
	VkDescriptorSetLayout vrsDescriptorSetLayout;
	VkDescriptorSet vrsDescriptorSet;

	vk_memory_defrag_t memory_defrag;

	vk_virtual_memory_t virtual_memory;

	vk_compute_manager_t compute_manager;

	vk_resource_pool_t resource_pools;

	vk_texture_streaming_t texture_streaming;

	// GPU Memory tracking and VRAM monitoring
	vk_memory_tracker_t memory_tracker;
	vk_vram_stats_t vram_stats;

	// Lock-free memory allocators for high-performance concurrent allocation
	vk_lock_free_memory_manager_t lock_free_manager;

	// Arena allocators for scoped memory management
	vk_arena_manager_t arena_manager;

	// Memory advisor for intelligent layout optimization
	vk_memory_advisor_t memory_advisor;

	// Cache-conscious data structures for optimal CPU cache utilization
	vk_cache_structures_manager_t cache_manager;

	// Render graph profiler for detailed performance analysis
	vk_render_profiler_t render_profiler;

	// Memory bandwidth profiler for cache analysis and access pattern optimization
	vk_memory_bandwidth_profiler_t memory_bandwidth_profiler;

	// Parallel processing profiler for thread utilization and synchronization tracking
	vk_parallel_profiler_t parallel_profiler;

	// Shader performance analyzer for instruction count, register usage, and optimization suggestions
	vk_shader_performance_analyzer_t shader_performance_analyzer;

	// Asset loading profiler for streaming performance and I/O bottleneck identification
	vk_asset_loading_profiler_t asset_loading_profiler;

#ifdef USE_CIMGUI
	// Performance HUD for real-time overlay with bottleneck highlighting and recommendations
	vk_performance_hud_t performance_hud;
#endif

	// Automated performance regression detector for CI-based performance gates
	vk_performance_regression_detector_t performance_regression_detector;

	// Heatmap visualizer for performance data visualization (optimization focus areas)
	vk_heatmap_visualizer_t heatmap_visualizer;

	// Current performance preset
	vk_performance_preset_t current_perf_preset;
} Vk_Instance;


// Vk_World contains vulkan resources/state requested by the game code.
// It is reinitialized on a map change.
typedef struct {
	//
	// Memory allocations.
	//
	int num_image_chunks;
	ImageChunk image_chunks[MAX_IMAGE_CHUNKS];

	//
	// State.
	//

	// Descriptor sets corresponding to bound texture images.
	//VkDescriptorSet current_descriptor_sets[ MAX_TEXTURE_UNITS ];

	// This flag is used to decide whether framebuffer's depth attachment should be cleared
	// with vmCmdClearAttachment (dirty_depth_attachment != 0), or it have just been
	// cleared by render pass instance clear op (dirty_depth_attachment == 0).
	int dirty_depth_attachment;

	float modelview_transform[16];
} Vk_World;

extern Vk_Instance	vk;				// shouldn't be cleared during ref re-init
extern Vk_World		vk_world;		// this data is cleared during ref re-init

// Command buffer management functions
VkCommandBuffer begin_command_buffer(void);
void end_command_buffer(VkCommandBuffer command_buffer, const char *location);

extern PFN_vkCreateCommandPool qvkCreateCommandPool;
extern PFN_vkDestroyCommandPool qvkDestroyCommandPool;
extern PFN_vkAllocateCommandBuffers qvkAllocateCommandBuffers;
extern PFN_vkFreeCommandBuffers qvkFreeCommandBuffers;

#ifdef __cplusplus
}
#endif

#include "tr_local.h"
