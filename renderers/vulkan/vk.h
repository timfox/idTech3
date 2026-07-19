#pragma once

#include <stddef.h>
#include "../common/vulkan/vulkan.h"
#include "tr_common.h"
#include "vk_util.h"
#include "vk_descriptor_sets.h"
#include "vk_texture_image.h"
#include "vk_pipeline_helpers.h"
#include "vk_occlusion.h"

/* VK_EXT_extended_dynamic_state3: color write mask for RB_ColorMask */
#ifndef VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT
#define VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT 1000484004
#endif
#ifndef PFN_vkCmdSetColorWriteMaskEXT
typedef void (VKAPI_PTR *PFN_vkCmdSetColorWriteMaskEXT)(VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorComponentFlags *pColorWriteMasks);
#endif

#define VK_CHECK( function_call ) do { \
	VkResult _res_ = (function_call); \
	if ( _res_ < 0 ) { \
		ri.Error( ERR_FATAL, "Vulkan: %s returned %s", #function_call, vk_result_string( _res_ ) ); \
	} \
} while(0)

/* Teardown-only: once device_lost is set, DEVICE_LOST results must not re-enter ri.Error. */
#define VK_CHECK_IGNORE_LOST( function_call ) do { \
	VkResult _res_ = (function_call); \
	if ( _res_ < 0 ) { \
		if ( vk.device_lost && ( _res_ == VK_ERROR_DEVICE_LOST ) ) { \
			ri.Printf( PRINT_DEVELOPER, "Vulkan: ignoring %s during device_lost teardown\n", #function_call ); \
		} else { \
			ri.Error( ERR_FATAL, "Vulkan: %s returned %s", #function_call, vk_result_string( _res_ ) ); \
		} \
	} \
} while(0)

#define MAX_SWAPCHAIN_IMAGES 8
#define MIN_SWAPCHAIN_IMAGES_IMM 3
#define MIN_SWAPCHAIN_IMAGES_FIFO   3
#define MIN_SWAPCHAIN_IMAGES_FIFO_0 4
#define MIN_SWAPCHAIN_IMAGES_MAILBOX 3

#define MAX_VK_SAMPLERS 32
#define MAX_VK_PIPELINES ((1024 + 128)*2)

#define VERTEX_BUFFER_SIZE     (4 * 1024 * 1024)  /* by default */
#define VERTEX_BUFFER_SIZE_HI  (8 * 1024 * 1024)

#define VEGWIND_MAX_VERTS 16384
#define VEGWIND_VERTEX_STRIDE 32  /* positionFlex + normalPhase */

#define STAGING_BUFFER_SIZE    (2 * 1024 * 1024)  /* by default */
#define STAGING_BUFFER_SIZE_HI (24 * 1024 * 1024) /* enough for max.texture size upload with all mip levels at once */

#define IMAGE_CHUNK_SIZE (32 * 1024 * 1024)
#define MAX_IMAGE_CHUNKS 56

#define NUM_COMMAND_BUFFERS 2	// number of command buffers / render semaphores / framebuffer sets

#define VK_VOLUMETRIC_QUERY_SLOTS 16
#define VK_VOLUMETRIC_QUERY_COUNT (VK_VOLUMETRIC_QUERY_SLOTS * NUM_COMMAND_BUFFERS)

typedef enum {
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
} vk_volumetry_query_index_t;

#define VK_NUM_BLOOM_PASSES 4

#ifndef _DEBUG
#define USE_DEDICATED_ALLOCATION
#endif
//#define MIN_IMAGE_ALIGN (128*1024)
#define MAX_ATTACHMENTS_IN_POOL (32+VK_NUM_BLOOM_PASSES*2) // extra space for new SMAA buffers, screenmap, bloom, SSAO, etc.

#define VK_DESC_STORAGE      0
#define VK_DESC_UNIFORM      0
#define VK_DESC_TEXTURE0     1
#define VK_DESC_TEXTURE1     2
#define VK_DESC_TEXTURE2     3
#define VK_DESC_FOG_COLLAPSE 4

#ifdef USE_VK_PBR
	typedef float mat4_t[16];
	#define VK_DESC_PBR_BRDFLUT				5
	#define VK_DESC_PBR_NORMAL				6
	#define VK_DESC_PBR_PHYSICAL			7
	#define VK_DESC_PBR_CUBEMAP				8
	#define VK_DESC_PBR_DELUXE				9
	#define VK_DESC_PBR_IRRADIANCE			10
	#define VK_DESC_PBR_EMISSIVE			11
	#define VK_DESC_PBR_CLEARCOAT			12
	#define VK_DESC_PBR_SHEEN				13
	#define VK_DESC_PBR_ANISOTROPY			14
	#define VK_DESC_PBR_TRANSMISSION		15
	#define VK_DESC_PBR_SUBSURFACE			16
	#define VK_DESC_PBR_DETAIL				17
	#define VK_DESC_FORWARD_PLUS			18 /* SSBO set: light + tile lists (PBR fragment) */
	#define VK_DESC_PBR_BLEND_LAYERS		19 /* array samplers: albedo/normal/orm × 8 */
	#define VK_DESC_COUNT	20
#else
	#define VK_DESC_COUNT   5
#endif

#include "vk_procs.h"

#define VK_DESC_TEXTURE_BASE VK_DESC_TEXTURE0
#define VK_DESC_FOG_ONLY     VK_DESC_TEXTURE1
#define VK_DESC_FOG_DLIGHT   VK_DESC_TEXTURE1


#define VK_DESC_UNIFORM_MAIN_BINDING		0
#define VK_DESC_UNIFORM_CAMERA_BINDING		1
#define VK_DESC_UNIFORM_IQM_SKIN_BINDING	2
#define VK_DESC_UNIFORM_IQM_MORPH_BINDING	3
#define VK_DESC_UNIFORM_GLTF_TOPO_BINDING	4
#define VK_DESC_UNIFORM_COUNT				5

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
	TYPE_SIGNLE_TEXTURE_UI_SDF,
	TYPE_SIGNLE_TEXTURE_UI_VECTOR,
	TYPE_SIGNLE_TEXTURE_UI_SUBPIXEL,

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

	TYPE_GENERIC_END = TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV,

	TYPE_OCCLUSION_BBOX = 128	// depth-test only, no color/depth write (for occlusion queries)
} Vk_Shader_Type;

// used with cg_shadows == 2
typedef enum {
	SHADOW_DISABLED,
	SHADOW_EDGES,
	SHADOW_FS_QUAD,
} Vk_Shadow_Phase;

typedef enum {
	TRIANGLE_LIST = 0,
	TRIANGLE_STRIP,
	LINE_LIST,
	POINT_LIST
} Vk_Primitive_Topology;

typedef enum {
	DEPTH_RANGE_NORMAL,		// [0..1]
	DEPTH_RANGE_ZERO,		// [0..0]
	DEPTH_RANGE_ONE,		// [1..1]
	DEPTH_RANGE_WEAPON,		// [0..0.3]
	DEPTH_RANGE_COUNT
}  Vk_Depth_Range;

typedef struct {
	VkSamplerAddressMode address_mode; // clamp/repeat texture addressing mode
	int gl_mag_filter;		// GL_XXX mag filter
	int gl_min_filter;		// GL_XXX min filter
	qboolean max_lod_1_0;	// fixed 1.0 lod
	qboolean noAnisotropy;
} Vk_Sampler_Def;

typedef enum {
	RENDER_PASS_MAIN = 0,
	RENDER_PASS_SCREENMAP,
	RENDER_PASS_SUN_SHADOW,
	RENDER_PASS_POST_BLOOM,
	RENDER_PASS_UI_OVERLAY,
	RENDER_PASS_CUBEMAP,
	RENDER_PASS_COUNT
} renderPass_t;

typedef struct {
	Vk_Shader_Type shader_type;
	unsigned int state_bits; // GLS_XXX flags
	cullType_t face_culling;
	qboolean polygon_offset;
	qboolean mirror;
	Vk_Shadow_Phase shadow_phase;
	Vk_Primitive_Topology primitives;
	int line_width;
	int fog_stage; // off, fog-in / fog-out
	int abs_light;
	int allow_discard;

#ifdef USE_VK_PBR
	uint32_t				vk_pbr_flags;
	int32_t					lightmap_bundle;
	uint8_t					pbr_vert_mode; /* 0=default gen_vert, 1=glTF GPU skin+morph variant */
	uint8_t					gltf_gpu_tangent_mode; /* 0=bind T, 1=Gram–Schmidt, 2=topology-weighted (r_gltfGpuTangentFix 0–2, latched) */
	uint8_t					pom_height_source; /* 0=ORM R (physical map), 1=normal map alpha (normalHeightMap) */
	uint8_t					material_blend_layers; /* 0=off, 2..8 */
	uint8_t					material_height_mask;  /* bit i => layer i has height (8 bits) */
	vec4_t					specularScale;
	vec4_t					normalScale;
	float					parallaxBias;
	float					material_blend_sharpness;
#endif
	unsigned int			hasFlowmap : 1;	// water flowmap: flow vectors offset texture UVs
	int acff; // none, rgb, rgba, alpha
	struct {
		byte rgb;
		byte alpha;
	} color;
} Vk_Pipeline_Def;

typedef struct VK_Pipeline {
	Vk_Pipeline_Def def;
	VkPipeline handle[ RENDER_PASS_COUNT ];
} VK_Pipeline_t;

#include "vk_create_pipeline.h"
#include "vk_draw_state.h"

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
	// flowmap: x=flowSpeed, y=flowTime (seconds), z=phaseCycle (0..1), w=unused
	vec4_t flowmapParams;

#ifdef USE_VK_PBR
	vec4_t pbrEmissiveScale;
	vec4_t pbrClearcoatScale;
	vec4_t pbrSheenScale;
	vec4_t pbrAnisotropyScale;
	vec4_t pbrTransmissionScale;
	vec4_t pbrSubsurfaceColor;
	vec4_t pbrSubsurfaceParams;
	vec4_t pbrAdvancedParams; // x: multi-scatter toggle, y: multi-scatter strength, z: roughness Fresnel, w: specular AA strength
	vec4_t pbrGlintParams0;
	vec4_t pbrGlintParams1;
	vec4_t pbrGlintFlags;
	vec4_t pbrDebugMode; // x: debug mode selector; y: hybridDeferredOpaque (mode 3)
	vec4_t pbrShCoeffs[9];
	/* Parallax occlusion (POM): x=height scale, y=self-shadow strength, z=shadow ray steps (float bits as int), w=unused */
	vec4_t pbrParallaxParams;
	/* Material blend: x=sharpness, yzw unused */
	vec4_t pbrMaterialBlend;
	/* Forward+: x overflow shade; y skip mask (tess.dlightBits) */
	vec4_t pbrForwardPlus;
	/* Sun shadow (PBR direct): rows 0-3 = light clip matrix columns; params x=bias y=pcf z=valid w=strength */
	vec4_t pbrSunShadowRows[4];
	vec4_t pbrSunShadowParams;
#endif
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
/* Must not collide with TESS_ENT0 (1024). */
#define TESS_PBR   				( 0x8000u ) // PBR shader variant, qtangent vertex attribute and eyePos uniform

#define PBR_HAS_NORMALMAP		( 1 )
#define PBR_HAS_PHYSICALMAP		( 2 )
#define PBR_HAS_SPECULARMAP		( 4 )
#define PBR_HAS_LIGHTMAP		( 8 )
#define PBR_HAS_DELUXEMAP0		( 16 )
#define PBR_HAS_DELUXEMAP1		( 32 )
#define PBR_HAS_EMISSIVE		( 64 )
#define PBR_HAS_CLEARCOAT		( 128 )
#define PBR_HAS_SHEEN			( 256 )
#define PBR_HAS_ANISOTROPY		( 512 )
#define PBR_HAS_TRANSMISSION	( 1024 )
#define PBR_HAS_SUBSURFACE		( 2048 )
	#define PBR_HAS_IRRADIANCE		( 4096 )
	#define PBR_HAS_DETAILMAP		( 8192 )
	#define PBR_HAS_MATERIAL_BLEND	( 16384 )

#define PHYS_NONE				( 1 )
#define PHYS_RMO				( 2 )
#define PHYS_RMOS   			( 4 )
#define PHYS_MOXR   			( 8 )
#define PHYS_MOSR   			( 16 )
#define PHYS_ORM  				( 32 )	
#define PHYS_ORMS   			( 64 )	
#define PHYS_NORMAL   			( 128 )	
#define PHYS_NORMALHEIGHT		( 256 )	
#define PHYS_SPECGLOSS			( 512 )
#define PHYS_EMISSIVE			( 1024 )
#define PHYS_CLEARCOAT			( 2048 )
#define PHYS_SHEEN				( 4096 )
#define PHYS_ANISOTROPY			( 8192 )
#define PHYS_TRANSMISSION		( 16384 )
#define PHYS_SUBSURFACE			( 32768 )

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
	{ 0,				"",			{ VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, } },
#ifdef USE_VK_PBR
	{ (uint32_t)PHYS_RMO,			"_rmo",		{ VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_ONE,	} },
	{ (uint32_t)PHYS_RMOS,			"_rmos",	{ VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_A, } },
	{ (uint32_t)PHYS_MOXR,			"_moxr",	{ VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_A, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ONE } },
	{ (uint32_t)PHYS_MOSR,			"_mosr",	{ VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_A, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_B } },
	{ (uint32_t)PHYS_ORM,			"_orm",		{ VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY } },
	{ (uint32_t)PHYS_ORMS,			"_orms",	{ VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY } },
	{ (uint32_t)PHYS_NORMAL,		"_n",		{ VK_COMPONENT_SWIZZLE_A, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_R } },
	{ (uint32_t)PHYS_NORMALHEIGHT,	"_nh",		{ VK_COMPONENT_SWIZZLE_A, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_R } },
	{ (uint32_t)PHYS_EMISSIVE,		"_emissive", { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY } },
	{ (uint32_t)PHYS_CLEARCOAT,		"_clearcoat", { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY } },
	{ (uint32_t)PHYS_SHEEN,			"_sheen",	{ VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY } },
	{ (uint32_t)PHYS_ANISOTROPY,	"_aniso",	{ VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY } },
	{ (uint32_t)PHYS_TRANSMISSION,	"_transmission", { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY } },
	{ (uint32_t)PHYS_SUBSURFACE,	"_subsurface", { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY } },
#endif
};

//
// Initialization.
//

// Returns pending validation warnings/errors delivered through the Vulkan
// debug report callback.  Copies at most `bufsize` bytes into `buffer`.
// Returns qtrue when a message was consumed.
qboolean vk_consume_validation_error( char *buffer, size_t bufsize );

// Debug: set object name for Vulkan debugger/profiler (no-op if extension unavailable).
void vk_set_object_name( uint64_t obj, const char *objName, VkDebugReportObjectTypeEXT objType );
#define SET_OBJECT_NAME(obj, objName, objType) vk_set_object_name( (uint64_t)(obj), (objName), (objType) )

// Initializes VK_Instance structure.
// After calling this function we get fully functional vulkan subsystem.
void vk_initialize( void );

// Shutdown vulkan subsystem by releasing resources acquired by Vk_Instance.
void vk_shutdown( refShutdownCode_t code );

// Releases vulkan resources allocated during program execution.
// This effectively puts vulkan subsystem into initial state (the state we have after vk_initialize call).
void vk_release_resources( void );

void vk_wait_idle( void );
void vk_queue_wait_idle( void );
VkSampleCountFlagBits vk_get_main_rasterization_samples( void );
VkSampleCountFlagBits vk_get_main_rasterization_max_samples( void );
float vk_get_msaa_min_sample_shading( void );

//
// Resources allocation.
//
void vk_allocate_and_bind_image_memory( VkImage image );
void vk_image_free_chunks( void );
void vk_bind_generated_shaders( void );
void vk_validate_pbr_ibl_resources( void );
void vk_destroy_samplers( void );
VkSampler vk_find_sampler( const Vk_Sampler_Def *def );

void vk_create_post_process_pipeline( int program_index, uint32_t width, uint32_t height );
void vk_create_pipelines( void );
void vk_create_volumetric_pipelines( void );

//
// Rendering setup.
//

void vk_clear_color( const vec4_t color );
void vk_clear_depth( qboolean clear_stencil );
void vk_set_color_write_mask( qboolean r, qboolean g, qboolean b, qboolean a );
void vk_begin_frame( void );
void vk_end_frame( void );
void vk_present_frame( void );

/* Swapchain + attachment teardown/recreate (Android surface recycle, swapchain restart) */
void vk_teardown_presentation_targets( void );
void vk_restore_presentation_targets( void );
void vk_restart_swapchain( const char *funcname, VkResult res );
void vk_prepare_2d( void );
void vk_prepare_frame_temporal_state( void );
void vk_reset_scene_src_rect_tracking( void );
void vk_begin_motion_frame( void );
void vk_prime_gpu_morph_weights_current( void );
void vk_snap_gpu_morph_weights_for_motion( void );
qboolean vk_get_scene_src_rect( VkRect2D *outRect );
void vk_get_scissor_rect( VkRect2D *r );
void vk_update_depth_range( Vk_Depth_Range depth_range );

void vk_end_render_pass( void );
void vk_begin_main_render_pass( void );
void vk_resume_current_render_pass( void );
void vk_begin_post_bloom_render_pass( void );
void vk_begin_ui_overlay_render_pass( void );
void vk_begin_ui_overlay_render_pass_load( void );
void vk_begin_bloom_extract_render_pass( void );
void vk_begin_blur_render_pass( uint32_t index );
void vk_begin_ssao_render_pass( void );
void vk_begin_ssao_blur_render_pass( void );
void vk_begin_ssao_combine_render_pass( void );
struct drawSurfsCommand_s;
void vk_oit_pass( const struct drawSurfsCommand_s *cmd );
void vk_begin_ssr_render_pass( void );
void vk_ssr_pass( void );
void vk_vegetation_wind_prepare_draw( void );
void vk_vegetation_add_from_tess( int oldVertexCount, int newVertexCount );
void vk_vegetation_clear_staging( void );
qboolean vk_begin_sun_shadow_render_pass( void );
void vk_end_sun_shadow_render_pass( void );

void vk_read_pixels( byte* buffer, uint32_t width, uint32_t height ); // screenshots
qboolean vk_bloom( void );
qboolean vk_lens_flare( void );
qboolean vk_ssao_pass( void );

#ifdef USE_VBO
void vk_release_vbo( void );
void vk_release_stream_vbo( void );
qboolean vk_alloc_vbo( const byte *vbo_data, int vbo_size );
qboolean vk_upload_stream_vbo( const byte *vbo_data, int vbo_size );
#endif
void vk_update_mvp( const float *m );

void vk_update_post_process_pipelines( void );

void VBO_PrepareQueues( void );
void VBO_RenderIBOItems( void );
void VBO_RenderStreamItem( void );
void VBO_ClearQueue( void );

/* glTF primitive VBO: creates device-local vertex+index buffers from packed data */
qboolean vk_create_gltf_buffers( const byte *vboData, int vboSize, const uint32_t *idxData, int idxCount,
	VkBuffer *outVertexBuffer, VkBuffer *outIndexBuffer );

/* GPU occlusion culling: vk_occlusion.h */

// cubemap
#ifdef VK_CUBEMAP
void vk_clear_cube_color( image_t *image, VkClearColorValue color );
void vk_begin_cubemap_render_pass( void );
void vk_create_cubemap_prefilter( void );
void vk_destroy_cubemap_prefilter( void );
#endif

#ifdef VK_PBR_BRDFLUT
void vk_create_brdflut_pipeline( void );
void vk_create_brfdlut( void );
#endif

typedef struct vk_tess_s {
	VkCommandBuffer command_buffer;

	VkSemaphore image_acquired;
	uint32_t	swapchain_image_index;
	qboolean	swapchain_image_acquired;
	VkFence rendering_finished_fence;
	qboolean waitForFence;

	VkBuffer vertex_buffer;
	byte *vertex_buffer_ptr; // pointer to mapped vertex buffer
	uint32_t vertex_buffer_offset; // VkDeviceSize

	VkDescriptorSet uniform_descriptor;
	uint32_t		uniform_read_offset;
	uint32_t		iqm_skin_offset;
	uint32_t		iqm_morph_offset;
	uint32_t		gltf_topo_offset;
#ifdef USE_VK_PBR
	uint32_t			camera_ubo_offset;
	VkDeviceSize		buf_offset[10];
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
		uint32_t		offset[VK_DESC_UNIFORM_COUNT]; // set 0 dynamic offsets
		const image_t	*image[VK_DESC_COUNT];
	} descriptor_set;

	Vk_Depth_Range		depth_range;
	VkPipeline			last_pipeline;

	uint32_t num_indexes; // value from most recent vk_bind_index() call

	VkRect2D scissor_rect;
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
	VkExtent2D swapchain_extent;
	qboolean swapchain_extent_valid;
	uint32_t swapchain_restart_count;
	int swapchain_last_restart_result;
	int swapchain_last_restart_ms;
	qboolean device_lost;  /* VK_ERROR_DEVICE_LOST detected; skip Vulkan API calls during shutdown */
	VkImage swapchain_images[MAX_SWAPCHAIN_IMAGES];
	VkImageView swapchain_image_views[MAX_SWAPCHAIN_IMAGES];
	VkSemaphore swapchain_rendering_finished[MAX_SWAPCHAIN_IMAGES];
	//uint32_t swapchain_image_index;

	VkCommandPool command_pool;

	VkDeviceMemory image_memory[ MAX_ATTACHMENTS_IN_POOL ];
	uint32_t image_memory_count;

	struct {
		VkRenderPass main;
		VkRenderPass main_resume; /* LOAD twin of main for mid-frame out-of-pass resume */
		VkRenderPass screenmap;
		VkRenderPass sun_shadow;
		VkRenderPass local_spot_shadow;
		VkRenderPass local_point_shadow;
		VkRenderPass gamma;
		VkRenderPass overlay_compose;
		VkRenderPass capture;
		VkRenderPass bloom_extract;
		VkRenderPass blur[VK_NUM_BLOOM_PASSES*2]; // horizontal-vertical pairs
		VkRenderPass post_bloom;
		VkRenderPass ui_overlay;
		VkRenderPass ssao;
		VkRenderPass ssao_blur;
		VkRenderPass ssao_combine;
		VkRenderPass oit_accum;
		VkRenderPass oit_moments;
		VkRenderPass oit_resolve;
		VkRenderPass ssr;
#ifdef VK_PBR_BRDFLUT
		VkRenderPass brdflut;
#endif
		VkRenderPass cubemap;
		VkRenderPass smaa_edge;
		VkRenderPass smaa_blend;
		VkRenderPass smaa_compose;
		VkRenderPass taa;
		VkRenderPass volumetric;
		VkRenderPass atmosphere;
	} render_pass;

	VkDescriptorPool descriptor_pool;
	VkDescriptorSetLayout set_layout_sampler;	// combined image sampler
	VkDescriptorSetLayout set_layout_uniform;	// dynamic uniform buffer
	VkDescriptorSetLayout set_layout_storage;	// feedback buffer
	VkDescriptorSetLayout set_layout_postfx_uniform;	// post-process params uniform buffer
#ifdef USE_VK_PBR
	VkDescriptorSetLayout set_layout_forward_plus;	/* light + tile SSBOs (compute cull + PBR fragment debug) */
#endif

	VkPipelineLayout pipeline_layout;			// default shaders
	VkPipelineLayout pipeline_layout_storage;	// flare test shader layout
	VkPipelineLayout pipeline_layout_post_process;	// post-processing
	VkPipelineLayout pipeline_layout_taa;	/* post-processing + motion vectors (set 4) */
	VkPipelineLayout pipeline_layout_blend;		// post-processing
	VkPipelineLayout pipeline_layout_smaa;
	VkPipelineLayout pipeline_layout_ssao;		// ssao (depth + push constants)
	VkPipelineLayout pipeline_layout_ssao_combine;	// ssao combine (color + ao)
	VkPipelineLayout pipeline_layout_oit_resolve;	// oit resolve (opaque + accum + revealage)
	VkPipelineLayout pipeline_layout_oit_accum;	// oit accum (sampler + depth + push constants)
	VkPipelineLayout pipeline_layout_oit_moments;	// MBOIT pass 1 (tex + depth + push constants)
	VkPipelineLayout pipeline_layout_oit_accum_mboit;	// MBOIT pass 2 (tex + depth + moments + b0 + optional Forward+)
	VkPipelineLayout pipeline_layout_ssr;		// ssr (color + depth + push constants)
	VkPipelineLayout pipeline_layout_atmosphere;	// atmosphere (push constants only)
	VkPipelineLayout pipeline_layout_fp64_points;	// fp64 point cloud (push constants)
#ifdef VK_PBR_BRDFLUT
	VkPipelineLayout pipeline_layout_brdflut;
#endif
	VkDescriptorSetLayout volumetric_compute_layout;
	VkDescriptorSetLayout volumetric_composite_layout;
	VkDescriptorSetLayout volumetric_depth_resolve_layout;
	VkDescriptorSetLayout volumetric_fluid_layout;
	VkQueryPool volumetric_query_pool;
	VkDescriptorSet volumetric_compute_descriptor;
	VkDescriptorSet volumetric_composite_descriptor;
	VkDescriptorSet volumetric_depth_resolve_descriptor;
	VkDescriptorSetLayout luminance_layout;
	VkDescriptorSet luminance_descriptor[NUM_COMMAND_BUFFERS];	/* per-frame (VUID-03047) */
	VkPipelineLayout luminance_pipeline_layout;
	VkPipeline luminance_pipeline;
	VkImage luminance_image;
	VkImageView luminance_image_view;
	VkBuffer luminance_staging_buffer;
	VkDeviceMemory luminance_staging_memory;
	void *luminance_staging_ptr;
	VkBuffer postfx_params_buffer[NUM_COMMAND_BUFFERS];
	VkDeviceMemory postfx_params_memory[NUM_COMMAND_BUFFERS];
	void *postfx_params_ptr[NUM_COMMAND_BUFFERS];
	VkDescriptorSet volumetric_fluid_descriptor;
	VkPipelineLayout volumetric_compute_pipeline_layout;
	VkPipelineLayout volumetric_composite_pipeline_layout;
	VkPipelineLayout volumetric_depth_resolve_pipeline_layout;
	VkPipelineLayout volumetric_fluid_pipeline_layout;
	VkPipeline volumetric_compute_pipeline;
	VkPipeline volumetric_composite_pipeline;
	VkPipeline volumetric_depth_resolve_pipeline;
	VkPipeline volumetric_fluid_advect_pipeline;
	VkPipeline volumetric_fluid_divergence_pipeline;
	VkPipeline volumetric_fluid_pressure_pipeline;
	VkPipeline volumetric_fluid_gradient_pipeline;

	VkDescriptorSetLayout cbt_terrain_layout;
	VkPipelineLayout cbt_terrain_compute_layout;
	VkPipeline cbt_terrain_compute_pipeline;
	VkBuffer cbt_draw_commands_buffer;
	VkDeviceMemory cbt_draw_commands_memory;
	VkBuffer cbt_params_buffer;
	VkDeviceMemory cbt_params_memory;
	VkImage cbt_patch_counter_image;
	VkDeviceMemory cbt_patch_counter_memory;
	VkImageView cbt_patch_counter_view;
	VkDescriptorSet cbt_terrain_descriptor;
	VkDeviceSize cbt_draw_commands_size;

	VkDescriptorSetLayout set_layout_blend_layers; /* set 19: 3×8 combined image samplers */
	VkDescriptorSet blend_layers_descriptor;

	VkDescriptorSetLayout vegwind_layout;
	VkPipelineLayout pipeline_layout_vegwind;
	VkPipeline vegwind_pipeline;
	VkBuffer vegwind_vertex_buffer;
	VkDeviceMemory vegwind_vertex_memory;
	VkDescriptorSet vegwind_descriptor;

	VkDescriptorSet color_descriptor[NUM_COMMAND_BUFFERS];	/* per-frame base scene color (VUID-03047) */
	VkDescriptorSet post_color_descriptor[NUM_COMMAND_BUFFERS];	/* per-frame mutable post-fog/gamma source */
	VkDescriptorSet overlay_color_descriptor[NUM_COMMAND_BUFFERS];	/* immutable UI overlay source */
	VkImageView post_fog_color_source;	/* last source for gamma (color_image or smaa_output) */
	VkImageView scene_post_fog_color_source;	/* scene-only source for luminance/exposure before HUD/console */
	char postChainLastWriter[16];	/* scene | bloom | post_aa | taa */
	VkDescriptorSet depth_descriptor[NUM_COMMAND_BUFFERS];	/* per-frame (VUID-03047) */
	VkDescriptorSet taa_motion_descriptor[NUM_COMMAND_BUFFERS];
		VkDescriptorSet postfx_params_descriptor[NUM_COMMAND_BUFFERS];
		VkDescriptorSet smaa_edge_descriptor;
		VkDescriptorSet smaa_blend_descriptor;
		VkDescriptorSet smaa_compose_descriptor;
		VkDescriptorSet taa_history_descriptor[2];

	VkImage color_image;
	VkImageView color_image_view;
	VkImage ui_overlay_image;
	VkImageView ui_overlay_image_view;
	VkImage fog_scene_image;
	VkImageView fog_scene_image_view;
	VkImage smaa_edge_image;
	VkImageView smaa_edge_image_view;
		VkImage smaa_blend_image;
		VkImageView smaa_blend_image_view;
		VkImage smaa_output_image;
		VkImageView smaa_output_image_view;
		VkImage taa_history_image[2];
		VkImageView taa_history_image_view[2];

	/* Deferred G-buffer sidecar (r_renderMode 1/2 + r_deferredGBuffer 1); fill via vk_deferred_gbuffer.c */
	struct {
		VkDescriptorSetLayout layout;
		VkPipelineLayout pipeline_layout;
		VkPipeline pipeline;
		VkDescriptorPool pool;
		VkDescriptorSet descriptor;
		qboolean pipeline_ready;
		qboolean fill_logged;
		VkDescriptorSetLayout debug_gfx_layout;
		VkPipelineLayout debug_gfx_pipeline_layout;
		VkPipeline debug_gfx_pipeline;
		VkDescriptorPool debug_gfx_pool;
		VkDescriptorSet debug_gfx_descriptor;
		qboolean debug_gfx_ready;
		VkDescriptorSetLayout lighting_layout;
		VkPipelineLayout lighting_pipeline_layout;
		VkPipeline lighting_pipeline;
		VkDescriptorPool lighting_pool;
		VkDescriptorSet lighting_descriptor;
		qboolean lighting_pipeline_ready;
		qboolean lighting_logged;
		VkDescriptorSetLayout composite_gfx_layout;
		VkPipelineLayout composite_gfx_pipeline_layout;
		VkPipeline composite_gfx_pipeline;
		VkDescriptorPool composite_gfx_pool;
		VkDescriptorSet composite_gfx_descriptor;
		qboolean composite_gfx_ready;
		qboolean composite_logged;
	} deferred_gbuffer;
	VkImage deferred_gbuffer_albedo;
	VkImageView deferred_gbuffer_albedo_view;
	VkImage deferred_gbuffer_normal;
	VkImageView deferred_gbuffer_normal_view;
	VkImage deferred_gbuffer_material;
	VkImageView deferred_gbuffer_material_view;
	VkImage deferred_lighting_image;
	VkImageView deferred_lighting_view;
	/* 1x1 R8_UINT stub (value 1 = SIMPLE_OPAQUE) when material class map is unavailable. */
	VkImage deferred_class_stub;
	VkImageView deferred_class_stub_view;
	qboolean deferredGbufferAllocated;
	qboolean deferredGbufferDirectExport;

	/* Visibility-buffer sidecar (r_visibilityBuffer); see vk_visibility_buffer.c / docs/RENDERER_2027.md */
	struct {
		VkDescriptorSetLayout layout;
		VkPipelineLayout pipeline_layout;
		VkPipeline pipeline;
		VkDescriptorPool pool;
		VkDescriptorSet descriptor;
		qboolean pipeline_ready;
		qboolean fill_logged;
		VkDescriptorSetLayout classify_layout;
		VkPipelineLayout classify_pipeline_layout;
		VkPipeline classify_pipeline;
		VkDescriptorPool classify_pool;
		VkDescriptorSet classify_descriptor;
		qboolean classify_pipeline_ready;
		qboolean classify_logged;
		VkDescriptorSetLayout debug_gfx_layout;
		VkPipelineLayout debug_gfx_pipeline_layout;
		VkPipeline debug_gfx_pipeline;
		VkDescriptorPool debug_gfx_pool;
		VkDescriptorSet debug_gfx_descriptor;
		qboolean debug_gfx_ready;
	} visibility_buffer;
	VkImage visibility_buffer_ids;
	VkImageView visibility_buffer_ids_view;
	VkImage visibility_buffer_bary;
	VkImageView visibility_buffer_bary_view;
	VkImage visibility_buffer_class;
	VkImageView visibility_buffer_class_view;
	qboolean visibilityBufferAllocated;
	/* True PrimID/instance MRT into ids/bary during opaque raster (non-MSAA). */
	qboolean visibilityBufferDirectExport;

	/* Neural Irradiance Volume (r_niv); see vk_niv.c */
	struct {
		VkDescriptorSetLayout shade_layout;
		VkPipelineLayout shade_pipeline_layout;
		VkPipeline shade_pipeline;
		VkDescriptorPool shade_pool;
		VkDescriptorSet shade_descriptor;
		qboolean shade_ready;
		VkDescriptorSetLayout composite_layout;
		VkPipelineLayout composite_pipeline_layout;
		VkPipeline composite_pipeline;
		VkDescriptorPool composite_pool;
		VkDescriptorSet composite_descriptor;
		qboolean composite_ready;
		VkImage feature_volume;
		VkImageView feature_volume_view;
		VkDeviceMemory feature_volume_memory;
		VkImage irradiance_image;
		VkImageView irradiance_view;
		VkDeviceMemory irradiance_memory;
		VkBuffer weights_buffer;
		VkDeviceMemory weights_memory;
		VkDeviceSize weights_size;
		qboolean volume_ready;
	} niv;
	qboolean nivAllocated;

	/* Neural Six-way Lightmaps (r_nslm); see vk_nslm.c */
	struct {
		VkDescriptorSetLayout froxel_layout;
		VkPipelineLayout froxel_pipeline_layout;
		VkPipeline froxel_pipeline;
		VkDescriptorPool froxel_pool;
		VkDescriptorSet froxel_descriptor;
		qboolean froxel_ready;
		VkImage feature_volume;
		VkImageView feature_volume_view;
		VkDeviceMemory feature_volume_memory;
		VkBuffer weights_buffer;
		VkDeviceMemory weights_memory;
		VkDeviceSize weights_size;
		qboolean volume_ready;
	} nslm;
	qboolean nslmAllocated;

	/* Neural Image Space Tessellation (r_nist); see vk_nist.c */
	struct {
		VkDescriptorSetLayout refine_layout;
		VkPipelineLayout refine_pipeline_layout;
		VkPipeline refine_pipeline;
		VkDescriptorPool refine_pool;
		VkDescriptorSet refine_descriptor;
		qboolean refine_ready;
		VkDescriptorSetLayout composite_layout;
		VkPipelineLayout composite_pipeline_layout;
		VkPipeline composite_pipeline;
		VkDescriptorPool composite_pool;
		VkDescriptorSet composite_descriptor;
		qboolean composite_ready;
		VkImage refined_image;
		VkImageView refined_view;
		VkDeviceMemory refined_memory;
		VkBuffer weights_buffer;
		VkDeviceMemory weights_memory;
		VkDeviceSize weights_size;
		qboolean weights_ready;
	} nist;
	qboolean nistAllocated;

	/* Neural Visibility Cache (r_nvc); see vk_nvc.c */
	struct {
		VkDescriptorSetLayout cache_layout;
		VkPipelineLayout cache_pipeline_layout;
		VkPipeline cache_pipeline;
		VkDescriptorPool cache_pool;
		VkDescriptorSet cache_descriptor;
		qboolean cache_ready;
		VkDescriptorSetLayout restir_layout;
		VkPipelineLayout restir_pipeline_layout;
		VkPipeline restir_pipeline;
		VkDescriptorPool restir_pool;
		VkDescriptorSet restir_descriptor;
		qboolean restir_ready;
		VkDescriptorSetLayout composite_layout;
		VkPipelineLayout composite_pipeline_layout;
		VkPipeline composite_pipeline;
		VkDescriptorPool composite_pool;
		VkDescriptorSet composite_descriptor;
		qboolean composite_ready;
		VkImage cache_image;
		VkImageView cache_view;
		VkDeviceMemory cache_memory;
		VkImage direct_image;
		VkImageView direct_view;
		VkDeviceMemory direct_memory;
		VkBuffer weights_buffer;
		VkDeviceMemory weights_memory;
		VkDeviceSize weights_size;
		qboolean weights_ready;
	} nvc;
	qboolean nvcAllocated;

	/* Forget Superresolution / Sample Adaptively (r_fsa); see vk_fsa.c */
	struct {
		VkDescriptorSetLayout importance_layout;
		VkPipelineLayout importance_pipeline_layout;
		VkPipeline importance_pipeline;
		VkDescriptorPool importance_pool;
		VkDescriptorSet importance_descriptor;
		qboolean importance_ready;
		VkDescriptorSetLayout denoise_layout;
		VkPipelineLayout denoise_pipeline_layout;
		VkPipeline denoise_pipeline;
		VkDescriptorPool denoise_pool;
		VkDescriptorSet denoise_descriptor;
		qboolean denoise_ready;
		VkImage importance_image;
		VkImageView importance_view;
		VkDeviceMemory importance_memory;
		VkImage fallback_importance_image;
		VkImageView fallback_importance_view;
		VkDeviceMemory fallback_importance_memory;
		VkCommandPool fallback_cmd_pool;
		VkBuffer dummy_ssbo;
		VkDeviceMemory dummy_ssbo_memory;
		qboolean importance_built;
	} fsa;
	qboolean fsaAllocated;

	/* Vertex Features Neural GI (r_vfgi); see vk_vfgi.c */
	struct {
		VkDescriptorSetLayout decode_layout;
		VkPipelineLayout decode_pipeline_layout;
		VkPipeline decode_pipeline;
		VkDescriptorPool decode_pool;
		VkDescriptorSet decode_descriptor;
		qboolean decode_ready;
		VkDescriptorSetLayout composite_layout;
		VkPipelineLayout composite_pipeline_layout;
		VkPipeline composite_pipeline;
		VkDescriptorPool composite_pool;
		VkDescriptorSet composite_descriptor;
		qboolean composite_ready;
		VkBuffer vertex_buffer;
		VkDeviceMemory vertex_memory;
		uint32_t vertex_count;
		VkBuffer grid_buffer;
		VkDeviceMemory grid_memory;
		VkBuffer weights_buffer;
		VkDeviceMemory weights_memory;
		VkDeviceSize weights_size;
		qboolean buffers_ready;
		VkImage irradiance_image;
		VkImageView irradiance_view;
		VkDeviceMemory irradiance_memory;
	} vfgi;
	qboolean vfgiAllocated;

	/* RenderFormer neural mesh preview (r_renderformer); see vk_renderformer.c */
	struct {
		VkDescriptorSetLayout transport_layout;
		VkPipelineLayout transport_pipeline_layout;
		VkPipeline transport_pipeline;
		qboolean transport_ready;
		VkDescriptorSetLayout decode_layout;
		VkPipelineLayout decode_pipeline_layout;
		VkPipeline decode_pipeline;
		qboolean decode_ready;
		VkDescriptorSetLayout composite_layout;
		VkPipelineLayout composite_pipeline_layout;
		VkPipeline composite_pipeline;
		qboolean composite_ready;
		VkBuffer token_buffer;
		VkDeviceMemory token_memory;
		uint32_t triangle_count;
		VkBuffer grid_buffer;
		VkDeviceMemory grid_memory;
		VkBuffer latent_buffer;
		VkDeviceMemory latent_memory;
		qboolean buffers_ready;
		VkImage preview_image;
		VkImageView preview_view;
		VkDeviceMemory preview_memory;
	} renderformer;
	qboolean renderformerAllocated;

	/* Wavefront path experiment (r_wpt); see vk_wpt.c */
	struct {
		VkDescriptorSetLayout enqueue_layout;
		VkPipelineLayout enqueue_pipeline_layout;
		VkPipeline enqueue_pipeline;
		qboolean enqueue_ready;
		VkDescriptorSetLayout wave_layout;
		VkPipelineLayout wave_pipeline_layout;
		VkPipeline wave_pipeline;
		qboolean wave_ready;
		VkDescriptorSetLayout composite_layout;
		VkPipelineLayout composite_pipeline_layout;
		VkPipeline composite_pipeline;
		qboolean composite_ready;
		VkBuffer ray_buffer;
		VkDeviceMemory ray_memory;
		qboolean buffers_ready;
	} wpt;
	qboolean wptAllocated;

#ifdef USE_VUDA
	/* VUDA CUDA-Vulkan interop; see vk_vuda.c */
	struct {
		VkBuffer        slot_buffer[3];
		VkDeviceMemory  slot_memory[3];
		uint64_t        slot_size[3];
		qboolean        slot_valid[3];
		VkSemaphore     cuda_wait_sem;
		VkSemaphore     cuda_signal_sem;
		uint64_t        renderTimeline;
		uint64_t        cudaTimeline;
		qboolean        interopReady;
		qboolean        computeWindowOpen;
	} vuda;
	qboolean vudaInteropCapable;
	qboolean vudaAllocated;
#endif

	/* Mobile-GS (r_mgs); see vk_mgs.c */
	struct {
		VkDescriptorSetLayout prepare_layout;
		VkPipelineLayout prepare_pipeline_layout;
		VkPipeline prepare_pipeline;
		VkDescriptorPool prepare_pool;
		VkDescriptorSet prepare_descriptor;
		qboolean prepare_ready;
		VkDescriptorSetLayout splat_layout;
		VkPipelineLayout splat_pipeline_layout;
		VkPipeline splat_pipeline;
		VkDescriptorPool splat_pool;
		VkDescriptorSet splat_descriptor;
		qboolean splat_ready;
		VkDescriptorSetLayout composite_layout;
		VkPipelineLayout composite_pipeline_layout;
		VkPipeline composite_pipeline;
		VkDescriptorPool composite_pool;
		VkDescriptorSet composite_descriptor;
		qboolean composite_ready;
		VkBuffer gaussian_buffer;
		VkDeviceMemory gaussian_memory;
		VkBuffer splat_buffer;
		VkDeviceMemory splat_memory;
		VkImage accum_image;
		VkImageView accum_view;
		VkDeviceMemory accum_memory;
		uint32_t gaussian_count;
	} mgs;
	qboolean mgsAllocated;

	/* VkSplat (r_vksplat); see vk_vksplat.c — 3DGS training in Vulkan compute */
	struct {
		VkBuffer gaussian_buffer;
		VkDeviceMemory gaussian_memory;
		VkBuffer projected_buffer;
		VkDeviceMemory projected_memory;
		VkBuffer sortkey_buffer;
		VkDeviceMemory sortkey_memory;
		VkImage render_image;
		VkImageView render_view;
		VkDeviceMemory render_memory;
	} vksplat;
	qboolean vksplatAllocated;

	/* CuRast (r_curast); see vk_curast.c — software rasterization scaffold */
	struct {
		VkBuffer tri_buffer;
		VkDeviceMemory tri_memory;
		VkImage vis_image;
		VkImageView vis_view;
		VkDeviceMemory vis_memory;
		VkImage color_image;
		VkImageView color_view;
		VkDeviceMemory color_memory;
	} curast;
	qboolean curastAllocated;

	/* Mímir (r_mimir); see vk_mimir.c — CUDA/Vulkan interop point cloud (arXiv:2504.20937) */
	struct {
		VkBuffer pos_buffer;
		VkDeviceMemory pos_memory;
		VkBuffer rng_buffer;
		VkDeviceMemory rng_memory;
		VkImage color_image;
		VkImageView color_view;
		VkDeviceMemory color_memory;
	} mimir;
	qboolean mimirAllocated;
	qboolean mimirInteropCapable;

	/* Iris (r_iris); see vk_iris.c — digital pathology WSI tiles (J Pathol Inform 2025) */
	struct {
		VkImage tile_atlas;
		VkImageView tile_atlas_view;
		VkDeviceMemory tile_atlas_memory;
		VkImage tile_mip;
		VkImageView tile_mip_view;
		VkDeviceMemory tile_mip_memory;
		VkImage tile_state;
		VkImageView tile_state_view;
		VkDeviceMemory tile_state_memory;
		VkImage scope_image;
		VkImageView scope_view;
		VkDeviceMemory scope_memory;
	} iris;
	qboolean irisAllocated;

	/* WebSplatter (r_wsp); see vk_wsp.c — WebGPU-portable tile splat path */
	struct {
		VkDescriptorSetLayout clear_layout;
		VkPipelineLayout clear_pipeline_layout;
		VkPipeline clear_pipeline;
		VkDescriptorPool clear_pool;
		VkDescriptorSet clear_descriptor;
		qboolean clear_ready;
		VkDescriptorSetLayout prepare_layout;
		VkPipelineLayout prepare_pipeline_layout;
		VkPipeline prepare_pipeline;
		VkDescriptorPool prepare_pool;
		VkDescriptorSet prepare_descriptor;
		qboolean prepare_ready;
		VkDescriptorSetLayout bin_layout;
		VkPipelineLayout bin_pipeline_layout;
		VkPipeline bin_pipeline;
		VkDescriptorPool bin_pool;
		VkDescriptorSet bin_descriptor;
		qboolean bin_ready;
		VkDescriptorSetLayout draw_layout;
		VkPipelineLayout draw_pipeline_layout;
		VkPipeline draw_pipeline;
		VkDescriptorPool draw_pool;
		VkDescriptorSet draw_descriptor;
		qboolean draw_ready;
		VkDescriptorSetLayout composite_layout;
		VkPipelineLayout composite_pipeline_layout;
		VkPipeline composite_pipeline;
		VkDescriptorPool composite_pool;
		VkDescriptorSet composite_descriptor;
		qboolean composite_ready;
		VkBuffer gaussian_buffer;
		VkDeviceMemory gaussian_memory;
		VkBuffer splat_buffer;
		VkDeviceMemory splat_memory;
		VkBuffer tile_count_buffer;
		VkDeviceMemory tile_count_memory;
		VkBuffer tile_index_buffer;
		VkDeviceMemory tile_index_memory;
		VkImage accum_image;
		VkImageView accum_view;
		VkDeviceMemory accum_memory;
		uint32_t gaussian_count;
		uint32_t tile_cols;
		uint32_t tile_rows;
		uint32_t tile_count;
	} wsp;
	qboolean wspAllocated;

	VkImage bloom_image[1+VK_NUM_BLOOM_PASSES*2];
	VkImageView bloom_image_view[1+VK_NUM_BLOOM_PASSES*2];
	VkExtent2D bloom_capture_extent;
	VkExtent2D bloom_mip_extent[1+VK_NUM_BLOOM_PASSES*2];

	VkDescriptorSet bloom_image_descriptor[1+VK_NUM_BLOOM_PASSES*2];

	VkImage ssao_image;
	VkImageView ssao_image_view;
	VkDescriptorSet ssao_descriptor;

	VkImage ssao_blur_image;
	VkImageView ssao_blur_image_view;
	VkDescriptorSet ssao_blur_descriptor;
	VkDescriptorSet ssao_scene_descriptor;	/* scene copy for combine (avoids read-modify-write) */
	VkImage oit_accum_image;
	VkImageView oit_accum_image_view;
	VkImage oit_reveal_image;
	VkImageView oit_reveal_image_view;
	VkImage oit_moments_image;
	VkImageView oit_moments_image_view;
	VkImage oit_b0_image;
	VkImageView oit_b0_image_view;
	VkDescriptorSet oit_opaque_descriptor;	/* opaque copy for OIT resolve */
	VkDescriptorSet oit_accum_descriptor;
	VkDescriptorSet oit_reveal_descriptor;
	VkDescriptorSet oit_depth_descriptor;
	VkDescriptorSet oit_moments_descriptor;
	VkDescriptorSet oit_b0_descriptor;
	VkImage ssr_image;
	VkImageView ssr_image_view;
	VkDescriptorSet ssr_descriptor[2];	/* [0]=color, [1]=depth */
	VkImage vao_mask_image;
	VkImageView vao_mask_image_view;

	VkImage depth_image;
	VkImageView depth_image_view;
	VkImageView depth_image_view_sample;	/* depth-only view for sampling/descriptors (VUID-01976) */
	VkImageLayout depth_image_layout;	/* tracked to avoid stale oldLayout assumptions across post passes */

	VkImage msaa_image;
	VkImageView msaa_image_view;
	VkImage ui_overlay_msaa_image;
	VkImageView ui_overlay_msaa_image_view;

	// screenMap
	struct {
		VkDescriptorSet color_descriptor;
		VkImage color_image;
		VkImageView color_image_view;
		VkImage motion_image;
		VkImageView motion_image_view;

		VkImage color_image_msaa;
		VkImageView color_image_view_msaa;
		VkImage motion_image_msaa;
		VkImageView motion_image_view_msaa;

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
		VkFramebuffer ssao;
		VkFramebuffer ssao_blur;
		VkFramebuffer ssao_combine;
		VkFramebuffer oit_accum;
		VkFramebuffer oit_moments;
		VkFramebuffer oit_resolve;
		VkFramebuffer ssr;
		VkFramebuffer main[MAX_SWAPCHAIN_IMAGES];
		VkFramebuffer gamma[MAX_SWAPCHAIN_IMAGES];
		VkFramebuffer overlay_compose[MAX_SWAPCHAIN_IMAGES];
		VkFramebuffer screenmap;
		VkFramebuffer capture;
#ifdef VK_PBR_BRDFLUT
		VkFramebuffer brdflut;
#endif
		VkFramebuffer cubemap[6];
		VkFramebuffer smaa_edge;
		VkFramebuffer smaa_blend;
		VkFramebuffer smaa_compose;
		VkFramebuffer taa[2];
		VkFramebuffer volumetric[MAX_SWAPCHAIN_IMAGES];
		VkFramebuffer atmosphere[MAX_SWAPCHAIN_IMAGES];
		VkFramebuffer ui_overlay[MAX_SWAPCHAIN_IMAGES];
		VkFramebuffer sun_shadow;
		VkFramebuffer local_spot_shadow;
		VkFramebuffer local_point_shadow[MAX_DLIGHTS * 6];
	} framebuffers;

	VkBuffer volumetric_params_buffer;
	VkDeviceSize volumetric_params_buffer_size;
	VkDeviceMemory volumetric_params_memory;
	void *volumetric_params_ptr;
	float prev_view_matrix[16];
	float prev_projection_matrix[16];
	float prev_view_proj[16];
	float prev_zfar;
	qboolean has_prev_volumetric;
	uint32_t volumetric_frame;

	vk_tess_t tess[ NUM_COMMAND_BUFFERS ], *cmd;
	int cmd_index;

	struct {
		VkBuffer		buffer;
		byte			*buffer_ptr;
		VkDeviceMemory	memory;
		VkDescriptorSet	descriptor;
	} storage;

	/* Forward+ scaffolding: light SSBO + optional tile cull compute (see vk_forward_plus.c). */
	struct {
		VkBuffer buffer;
		VkDeviceMemory memory; /* device-local: SSBO for compute + fragment */
		VkBuffer staging; /* host-visible: CPU pack, vkCmdCopyBuffer to buffer before cull */
		VkDeviceMemory staging_memory;
		void *staging_ptr;
		uint32_t capacity_bytes;
		uint32_t last_packed_count;
		uint32_t last_upload_bytes; /* bytes copied to device this frame (header + n records) */
		VkBuffer tile_buffer;
		VkDeviceMemory tile_memory;
		uint32_t tile_capacity_tiles;
		uint32_t tiles_x;
		uint32_t tiles_y;
		VkBuffer param_buffer;
		VkDeviceMemory param_memory;
		void *param_mapped;
		uint32_t param_buffer_size;
		uint32_t max_per_tile; /* 4..8 from r_forwardPlusMaxPerTile (latched); SSBO stride is always 8 slots */
		VkDescriptorSet descriptor;
		VkPipelineLayout pipeline_layout;
		VkPipeline tile_pipeline;
	} forward_plus;

	uint32_t uniform_item_size;
	uint32_t uniform_camera_item_size;
	uint32_t uniform_alignment;
	uint32_t storage_alignment;

	struct {
		VkBuffer vertex_buffer;
		VkDeviceMemory	buffer_memory;
		VkBuffer stream_vertex_buffer;
		VkDeviceMemory stream_buffer_memory;
	} vbo;

	// host visible memory that holds vertex, index and uniform data
	VkDeviceMemory geometry_buffer_memory;
	VkDeviceSize geometry_buffer_size;
	VkDeviceSize geometry_buffer_size_new;

	// statistics
	struct {
		VkDeviceSize vertex_buffer_max;
		uint32_t push_size;
		uint32_t push_size_max;
	} stats;

	qboolean inRenderPass;

	/* Pass ownership / late-post diagnostics for device-loss crash context. */
	struct {
		char lastBegunPass[32];
		char lastEndedPass[32];
		char lastPostStage[48];
		char lastResumeTarget[32];
		qboolean lastResumeSelfHeal;
		qboolean inContinuationPass;
		uint32_t lastPassWidth;
		uint32_t lastPassHeight;
	} passDiag;

	/* Eye adaptation: exposure from luminance pass (r_exposure_auto) */
	float adaptedExposure;
	vec3_t prevViewOrigin;  /* for camera cut detection (snap exposure on large view jump) */
	vec3_t prevViewForward; /* viewaxis[2] for angle-change detection (e.g. death cam tilt) */
	int prevClientState;
	struct {
		uint32_t pendingResetReasons;
		uint32_t appliedResetReasons;
		uint32_t stickyResetReasons;
		qboolean preparedThisFrame;
		qboolean hasValidLuminance;
		qboolean sharedCameraCut;
		qboolean unreliableMotionThisFrame;
		qboolean firstPersonProjectionThisFrame;
		qboolean firstPersonProjectionLastFrame;
		qboolean worldWasValid;
		qboolean noWorldModel;
		qboolean stableGameplayState;
		float filteredAvgLogLuminance;
		qboolean hasValidTAAHistory;
		uint32_t taaHistoryIndex;
		uint32_t lastRenderWidth;
		uint32_t lastRenderHeight;
		uint32_t lastSwapchainWidth;
		uint32_t lastSwapchainHeight;
		char worldName[MAX_QPATH];
		uint32_t frameIndex;
	} temporal;

	//
	// Shader modules.
	//
	struct {
		struct {
#ifdef USE_VK_PBR
			VkShaderModule gen[2][3][2][2][2]; // pbr[0,1], tx[0,1,2], cl[0,1] env0[0,1] fog[0,1]
			/* +USE_GLTF_GPU_SKIN; last dim: 0=bind T, 1=Gram–Schmidt, 2=topology+MikkT-inspired average */
			VkShaderModule gen_gltf_gpu[2][3][2][2][2][3];
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
			VkShaderModule ui_sdf_text;
			VkShaderModule ui_vector_text;
			VkShaderModule ui_vector_glyphlet_vert;
			VkShaderModule ui_vector_glyphlet_frag;
			VkShaderModule ui_subpixel_text;
			VkShaderModule flowmap[2];      // fog[0,1] - water flowmap
#ifdef USE_VK_PBR
			VkShaderModule gen[2][3][2][2]; // pbr[0,1], tx[0,1,2] cl[0,1] fog[0,1]
			VkShaderModule gbuf_gen[3][2][2]; // tx[0,1,2] cl[0,1] fog[0,1], PBR deferred export
			VkShaderModule ident1[2][2][2]; // pbr[0,1], tx[0,1], fog[0,1]
			VkShaderModule gbuf_ident1[2][2]; // tx[0,1], fog[0,1], PBR deferred export
			VkShaderModule fixed[2][2][2];  // pbr[0,1], tx[0,1], fog[0,1]
			VkShaderModule gbuf_fixed[2][2]; // tx[0,1], fog[0,1], PBR deferred export
			VkShaderModule ent[2][1][2];    // pbr[0,1], tx[0], fog[0,1]
			VkShaderModule gbuf_ent[1][2];   // tx[0], fog[0,1], PBR deferred export
#else
			VkShaderModule gen[3][2][2]; // tx[0,1,2] cl[0,1] fog[0,1]
			VkShaderModule ident1[2][2]; // tx[0,1], fog[0,1]
			VkShaderModule fixed[2][2];  // tx[0,1], fog[0,1]
			VkShaderModule ent[1][2];    // tx[0], fog[0,1]
#endif
			VkShaderModule light[2][2];  // linear[0,1] fog[0,1]
			/* r_hdr 3 (64-bit) variants; used when color_format is RGBA64F */
#ifdef USE_VK_PBR
			VkShaderModule gen_hdr64[2][3][2][2];
			VkShaderModule ident1_hdr64[2][2][2];
			VkShaderModule fixed_hdr64[2][2][2];
			VkShaderModule ent_hdr64[2][1][2];
#else
			VkShaderModule gen_hdr64[3][2][2];
			VkShaderModule ident1_hdr64[2][2];
			VkShaderModule fixed_hdr64[2][2];
			VkShaderModule ent_hdr64[1][2];
#endif
			VkShaderModule light_hdr64[2][2];
			VkShaderModule flowmap_hdr64[2];
		} frag;


		VkShaderModule color_fs;
		VkShaderModule color_vs;

		VkShaderModule bloom_fs;
		VkShaderModule blur_fs;
		VkShaderModule blend_fs;

		VkShaderModule ssao_fs;
		VkShaderModule hbao_fs;
		VkShaderModule ssao_blur_fs;
		VkShaderModule ssao_combine_fs;
		VkShaderModule oit_accum_vs;
		VkShaderModule oit_accum_fs;
		VkShaderModule oit_moments_fs;
		VkShaderModule oit_accum_mboit_fs;
		VkShaderModule oit_resolve_fs;
		VkShaderModule ssao_debug_fs;
		VkShaderModule ssao_depth_debug_fs;

		VkShaderModule gamma_fs;
		VkShaderModule overlay_compose_fs;
		VkShaderModule gamma_vs;
		VkShaderModule atmosphere_fs;
		VkShaderModule smaa_edge_fs;
		VkShaderModule smaa_blend_fs;
		VkShaderModule smaa_compose_fs;
		VkShaderModule fxaa_fs;
		VkShaderModule lens_flare_fs;
		VkShaderModule fp64_points_native_vs;
		VkShaderModule fp64_points_native_fs;
		VkShaderModule fp64_points_emulated_vs;
		VkShaderModule fp64_points_emulated_fs;
		VkShaderModule fp64_points_single_vs;
		VkShaderModule fp64_points_single_fs;
		VkShaderModule taa_fs;
		VkShaderModule ssr_fs;

		VkShaderModule fog_fs;
		VkShaderModule fog_vs;
		VkShaderModule volumetric_fog_vs;
		VkShaderModule volumetric_fog_fs;
		/* r_hdr 3 (64-bit) variants */
		VkShaderModule color_fs_hdr64;
		VkShaderModule fog_fs_hdr64;
		VkShaderModule atmosphere_fs_hdr64;
		VkShaderModule dot_fs_hdr64;
		VkShaderModule terrain_fs_hdr64;
		VkShaderModule smaa_edge_fs_hdr64;
		VkShaderModule smaa_blend_fs_hdr64;
		VkShaderModule smaa_compose_fs_hdr64;
		VkShaderModule ssr_fs_hdr64;
		VkShaderModule volumetric_fog_fs_hdr64;
		VkShaderModule volumetric_fog_cs;
		VkShaderModule volumetric_depth_resolve_msaa_cs;
		VkShaderModule luminance_cs;
		VkShaderModule vegetation_wind_cs;
		VkShaderModule fluid_advect_cs;
		VkShaderModule fluid_divergence_cs;
		VkShaderModule fluid_pressure_cs;
		VkShaderModule fluid_gradient_cs;
		VkShaderModule forward_plus_tile_cull_cs;
		VkShaderModule deferred_gbuffer_fill_cs;
		VkShaderModule deferred_gbuffer_debug_fs;
		VkShaderModule deferred_lighting_cs;
		VkShaderModule deferred_lighting_vrcs_cs;
		VkShaderModule vrcs_sri_cs;
		VkShaderModule vrcs_pack_cs;
		VkShaderModule vrcs_deblock_cs;
		VkShaderModule deferred_lighting_composite_fs;
		VkShaderModule visibility_buffer_fill_cs;
		VkShaderModule visibility_buffer_debug_fs;
		VkShaderModule material_classify_cs;
		VkShaderModule ndgi_decompress_cs;
		VkShaderModule niv_shade_cs;
		VkShaderModule niv_composite_cs;
		VkShaderModule nslm_froxel_cs;
		VkShaderModule nist_refine_cs;
		VkShaderModule nist_composite_cs;
		VkShaderModule nvc_cache_cs;
		VkShaderModule nvc_restir_cs;
		VkShaderModule nvc_composite_cs;
		VkShaderModule fsa_importance_cs;
		VkShaderModule fsa_denoise_cs;
		VkShaderModule vfgi_decode_cs;
		VkShaderModule vfgi_composite_cs;
		VkShaderModule rf_transport_cs;
		VkShaderModule rf_decode_cs;
		VkShaderModule rf_composite_cs;
		VkShaderModule wpt_enqueue_cs;
		VkShaderModule wpt_wave_cs;
		VkShaderModule wpt_composite_cs;
		VkShaderModule mgs_prepare_cs;
		VkShaderModule mgs_splat_cs;
		VkShaderModule mgs_composite_cs;
		VkShaderModule vksplat_project_fwd_cs;
		VkShaderModule vksplat_tile_cull_cs;
		VkShaderModule vksplat_raster_fwd_cs;
		VkShaderModule vksplat_adam_cs;
		VkShaderModule curast_clear_cs;
		VkShaderModule curast_stage1_cs;
		VkShaderModule curast_resolve_cs;
		VkShaderModule graph_bfs_expand_cs;
		VkShaderModule arc_blanc_htilde_cs;
		VkShaderModule arc_blanc_fft_1d_cs;
		VkShaderModule arc_blanc_extract_cs;
		VkShaderModule arc_blanc_combine_cs;
		VkShaderModule arc_blanc_velocity_cs;
		VkShaderModule arc_blanc_velocity_accum_cs;
		VkShaderModule mimir_clear_cs;
		VkShaderModule mimir_brownian_cs;
		VkShaderModule mimir_splat_cs;
		VkShaderModule iris_clear_cs;
		VkShaderModule iris_spd_cs;
		VkShaderModule iris_compose_cs;
		VkShaderModule iris_overlay_cs;
		VkShaderModule wsp_clear_tiles_cs;
		VkShaderModule wsp_prepare_cs;
		VkShaderModule wsp_tile_bin_cs;
		VkShaderModule wsp_tile_draw_cs;
		VkShaderModule wsp_composite_cs;

		VkShaderModule dressi_soft_vs;
		VkShaderModule dressi_soft_fs;
		VkShaderModule dressi_blend_cs;
		VkShaderModule dressi_composite_cs;
		VkShaderModule dressi_inverse_uv_cs;

		VkShaderModule cbt_terrain_cs;
		VkShaderModule terrain_vs;
		VkShaderModule terrain_fs;

		VkShaderModule dot_fs;
		VkShaderModule dot_vs;

#ifdef VK_PBR_BRDFLUT
		VkShaderModule brdflut_fs;
#endif
		VkShaderModule filtercube_vs;
		VkShaderModule filtercube_gm;
		VkShaderModule irradiancecube_fs;
		VkShaderModule prefilterenvmap_fs;
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
	uint32_t dlight_pipelines[2][3][2];

	// cullType[3], polygonOffset[2], fogStage[2], absLight[2]
	uint32_t dlight_pipelines_x[3][2][2][2];
	uint32_t dlight1_pipelines_x[3][2][2][2];

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
	uint32_t occlusion_bbox_pipeline;
	VkQueryPool occlusion_query_pool;

	VkPipeline gamma_pipeline;
	VkPipeline overlay_compose_pipeline;
	VkPipeline capture_pipeline;
	VkPipeline bloom_extract_pipeline;
	VkPipeline blur_pipeline[VK_NUM_BLOOM_PASSES*2]; // horizontal & vertical pairs
	VkPipeline bloom_blend_pipeline;
	VkPipeline smaa_edge_pipeline;
	VkPipeline smaa_blend_pipeline;
	VkPipeline smaa_compose_pipeline;
	VkPipeline fxaa_pipeline;
	VkPipeline lens_flare_pipeline;
	VkPipeline taa_pipeline;
	VkPipeline ssao_pipeline;
	VkPipeline hbao_pipeline;
	VkPipeline ssao_blur_pipeline;
	VkPipeline ssao_combine_pipeline;
	VkPipeline oit_accum_pipeline;	/* WBOIT accumulation for transparent surfaces */
	VkPipeline oit_moments_pipeline;	/* MBOIT pass 1: moment accumulation */
	VkPipeline oit_accum_mboit_pipeline;	/* MBOIT pass 2: moment-weighted WBOIT accum */
	VkPipeline oit_resolve_pipeline;
	VkPipeline ssao_debug_pipeline;
	VkPipeline ssao_depth_debug_pipeline;
	VkPipeline ssr_pipeline;
	VkPipeline atmosphere_pipeline;
#ifdef VK_PBR_BRDFLUT
	VkPipeline brdflut_pipeline;
#endif

	uint32_t frame_count;
	qboolean active;
	qboolean uiOverlayActive;
	qboolean uiOverlayContentValid; /* HUD already drawn this frame; re-entry must load, not clear */
	qboolean wideLines;
	qboolean shaderFloat64;
	qboolean samplerAnisotropy;
	qboolean fragmentStores;
	qboolean dedicatedAllocation;
	qboolean debugMarkers;
	qboolean colorWriteMaskDynamic;
	qboolean meshShaderNV; /* VK_NV_mesh_shader enabled at device create (r_vk_meshShaderNV); no mesh pipelines yet */
	qboolean sparseBinding; /* VkPhysicalDeviceFeatures.sparseBinding enabled */
	qboolean sparseResidencyImage2D; /* sparseResidencyImage2D + queue SPARSE_BINDING */
	qboolean sparseResidencyNonResidentStrict;
#ifdef USE_VULKAN_RTX
	qboolean rtxAvailable; /* KHR RT pipeline + AS + BDA; demo trace (r_rtxDemo) when r_rtx>0 — not production hybrid lighting */
	qboolean rayQueryAvailable; /* VK_KHR_ray_query enabled with RTX path (Surfel GI / compute RT) */
#endif

	float maxAnisotropy;
	float maxLod;

	VkFormat color_format;
	VkFormat capture_format;
	VkFormat depth_format;
	VkFormat bloom_format;
	VkFormat ssao_format;
	VkImage froxel_volume_image;
	VkImageView froxel_volume_view;
	VkDeviceMemory froxel_volume_memory;
	VkImage froxel_history_image;
	VkImageView froxel_history_view;
	VkDeviceMemory froxel_history_memory;
	VkImage froxel_extinction_image;
	VkImageView froxel_extinction_view;
	VkDeviceMemory froxel_extinction_memory;
	VkImage froxel_light_image;
	VkImageView froxel_light_view;
	VkDeviceMemory froxel_light_memory;
	VkImage froxel_clamp_image;
	VkImageView froxel_clamp_view;
	VkDeviceMemory froxel_clamp_memory;
	VkImage fluid_velocity_images[2];
	VkImageView fluid_velocity_views[2];
	VkDeviceMemory fluid_velocity_memory[2];
	VkImage fluid_density_images[2];
	VkImageView fluid_density_views[2];
	VkDeviceMemory fluid_density_memory[2];
	VkImage fluid_pressure_images[2];
	VkImageView fluid_pressure_views[2];
	VkDeviceMemory fluid_pressure_memory[2];
	VkImage fluid_divergence_image;
	VkImageView fluid_divergence_view;
	VkDeviceMemory fluid_divergence_memory;
	VkImage volumetric_telemetry_image;
	VkImageView volumetric_telemetry_view;
	VkDeviceMemory volumetric_telemetry_memory;
	VkImage volumetric_depth_image;
	VkImageView volumetric_depth_view;
	VkImage motion_vector_image;
	VkImageView motion_vector_view;
	VkImage motion_vector_msaa_image;
	VkImageView motion_vector_msaa_view;
	VkImage fog_noise_image;
	VkImageView fog_noise_view;
	VkDeviceMemory fog_noise_memory;
	VkSampler fog_noise_sampler;
	VkImage sun_shadow_image;
	VkImageView sun_shadow_view;
	VkImageView sun_shadow_sample_view;
	VkDeviceMemory sun_shadow_memory;
	VkImage sun_shadow_color_image;
	VkImageView sun_shadow_color_view;
	VkDeviceMemory sun_shadow_color_memory;
	VkImage sun_shadow_color_msaa_image;
	VkImageView sun_shadow_color_msaa_view;
	VkDeviceMemory sun_shadow_color_msaa_memory;
	VkSampler sun_shadow_sampler;
	VkImage local_spot_shadow_atlas_image;
	VkImageView local_spot_shadow_atlas_view;
	VkImageView local_spot_shadow_atlas_sample_view;
	VkDeviceMemory local_spot_shadow_atlas_memory;
	VkImage local_spot_shadow_color_image;
	VkImageView local_spot_shadow_color_view;
	VkDeviceMemory local_spot_shadow_color_memory;
	VkImage local_point_shadow_array_image;
	VkImageView local_point_shadow_array_view;
	VkImageView local_point_shadow_array_sample_view;
	VkDeviceMemory local_point_shadow_array_memory;
	VkImage local_point_shadow_color_array_image;
	VkImageView local_point_shadow_color_array_view;
	VkDeviceMemory local_point_shadow_color_array_memory;
	VkImageView local_point_shadow_face_views[MAX_DLIGHTS * 6];
	VkImageView local_point_shadow_color_face_views[MAX_DLIGHTS * 6];
	VkSampler froxel_sampler;
	VkSampler froxel_depth_sampler;
	uint32_t froxel_width;
	uint32_t froxel_height;
	uint32_t froxel_slices;
	uint32_t fluid_width;
	uint32_t fluid_height;
	uint32_t fluid_active_width;
	uint32_t fluid_active_height;
	uint32_t fluid_velocity_index;
	uint32_t fluid_density_index;
	uint32_t fluid_pressure_index;
	float volumetric_stage_ms[16];
	float volumetric_total_ms;
	float volumetric_fluid_ms;
	float volumetric_timestamp_period_ns;
	float fluid_dynamic_resolution_scale;
	int fluid_dynamic_pressure_iterations;
	uint32_t sun_shadow_width;
	uint32_t sun_shadow_height;
	float sun_shadow_matrix0[16];
	qboolean sun_shadow_valid;
	uint32_t local_spot_shadow_atlas_size;
	uint32_t local_spot_shadow_tile_size;
	uint32_t local_spot_shadow_capacity;
	uint32_t local_point_shadow_face_size;
	uint32_t local_point_shadow_capacity;

	VkImageLayout initSwapchainLayout;

	qboolean clearAttachment;		// requires VK_IMAGE_USAGE_TRANSFER_DST_BIT for swapchains
	qboolean fboActive;
	qboolean isV3DV;				/* Raspberry Pi Vulkan driver (V3DV); used for RPi5-friendly hints */
	qboolean blitEnabled;
	qboolean msaaActive;
	qboolean msaaSampleShading;	/* per-sample shading when MSAA on (better alpha/specular, higher cost) */
	qboolean smaaActive;
	qboolean fxaaActive;
	qboolean lensFlareActive;
#ifdef USE_VK_PBR
	qboolean pbrActive;
#endif
#ifdef VK_CUBEMAP
	qboolean cubemapActive;
#endif
	qboolean pbr_ibl_using_hdr_fallback;
	qboolean pbr_ibl_has_ready_local_cubemap;
	int pbr_ibl_ready_cubemap_count;
	int pbr_ibl_incomplete_cubemap_count;
	qboolean offscreenRender;

	qboolean windowAdjusted;
	int		blitX0;
	int		blitY0;
	int		blitFilter;

	uint32_t renderWidth;
	uint32_t renderHeight;
	/* Main scene color attachment (FBO) pixel size; stable while vk.renderWidth is temporarily overridden (shadow atlases, etc.). */
	uint32_t mainColorWidth;
	uint32_t mainColorHeight;

	float renderScaleX;
	float renderScaleY;

	renderPass_t renderPassIndex;

	uint32_t screenMapWidth;
	uint32_t screenMapHeight;
	uint32_t screenMapSamples;

	uint32_t image_chunk_size;

	// Physical device limit for 2D image dimension (VkPhysicalDeviceLimits::maxImageDimension2D).
	uint32_t hwMaxImageDimension2D;

	uint32_t maxBoundDescriptorSets;

	struct staging_buffer_s {
		VkBuffer handle;
		VkDeviceMemory memory;
		VkDeviceSize size;
		byte *ptr; // pointer to mapped staging buffer
	} staging_buffer;

	struct samplers_s {
		int count;
		Vk_Sampler_Def def[MAX_VK_SAMPLERS];
		VkSampler handle[MAX_VK_SAMPLERS];
		int filter_min;
		int filter_max;
		float mip_lod_bias;
	} samplers;

	struct defaults_t {
		VkDeviceSize staging_size;
		VkDeviceSize geometry_size;
	} defaults;

	char driverNote[200];

} Vk_Instance;

typedef struct {
	VkDeviceMemory memory;
	VkDeviceSize used;
} ImageChunk;

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
