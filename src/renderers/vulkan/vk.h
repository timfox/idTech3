#pragma once

#include <stddef.h>
#include "../common/vulkan/vulkan.h"
#include "tr_common.h"

#define MAX_SWAPCHAIN_IMAGES 8
#define MIN_SWAPCHAIN_IMAGES_IMM 3
#define MIN_SWAPCHAIN_IMAGES_FIFO   3
#define MIN_SWAPCHAIN_IMAGES_FIFO_0 4
#define MIN_SWAPCHAIN_IMAGES_MAILBOX 3

#define MAX_VK_SAMPLERS 32
#define MAX_VK_PIPELINES ((1024 + 128)*2)

#define VERTEX_BUFFER_SIZE     (4 * 1024 * 1024)  /* by default */
#define VERTEX_BUFFER_SIZE_HI  (8 * 1024 * 1024)

#define STAGING_BUFFER_SIZE    (2 * 1024 * 1024)  /* by default */
#define STAGING_BUFFER_SIZE_HI (24 * 1024 * 1024) /* enough for max.texture size upload with all mip levels at once */

#define IMAGE_CHUNK_SIZE (32 * 1024 * 1024)
#define MAX_IMAGE_CHUNKS 56

#define NUM_COMMAND_BUFFERS 2	// number of command buffers / render semaphores / framebuffer sets

#define USE_REVERSED_DEPTH

//#define USE_UPLOAD_QUEUE

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
	#define VK_DESC_COUNT	18
#else
	#define VK_DESC_COUNT   5
#endif

#define VK_DESC_TEXTURE_BASE VK_DESC_TEXTURE0
#define VK_DESC_FOG_ONLY     VK_DESC_TEXTURE1
#define VK_DESC_FOG_DLIGHT   VK_DESC_TEXTURE1


#define VK_DESC_UNIFORM_MAIN_BINDING		0
#define VK_DESC_UNIFORM_CAMERA_BINDING		1
#define VK_DESC_UNIFORM_IQM_SKIN_BINDING	2
#define VK_DESC_UNIFORM_IQM_MORPH_BINDING	3
#define VK_DESC_UNIFORM_COUNT				4

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
	vec4_t					specularScale;
	vec4_t					normalScale;
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
	vec4_t pbrAdvancedParams; // x: multi-scatter toggle, y: multi-scatter strength
	vec4_t pbrGlintParams0;
	vec4_t pbrGlintParams1;
	vec4_t pbrGlintFlags;
	vec4_t pbrDebugMode; // x: debug mode selector
	vec4_t pbrShCoeffs[9];
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
#define TESS_PBR   				( 1024 ) // PBR shader variant, qtangent vertex attribute and eyePos uniform

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

//
// Resources allocation.
//
void vk_create_image( image_t *image, int width, int height, int mip_levels );
void vk_upload_image_data( image_t *image, int x, int y, int width, int height, int miplevels, byte *pixels, int size, qboolean update );
void vk_upload_cubemap_mip_data( image_t *image, int face_size, int miplevels, const byte *pixels, int size, int bytes_per_pixel, qboolean update );
void vk_upload_compressed_image_data( image_t *image, int width, int height, int miplevels, byte *pixels, int size, qboolean update );
void vk_update_descriptor_set( image_t *image, qboolean mipmap );
void vk_destroy_image_resources( VkImage *image, VkImageView *imageView );
void vk_bind_generated_shaders( void );
void vk_update_attachment_descriptors( void );
void vk_validate_pbr_ibl_resources( void );
void vk_destroy_samplers( void );
VkSampler vk_find_sampler( const Vk_Sampler_Def *def );

uint32_t vk_find_pipeline_ext( uint32_t base, const Vk_Pipeline_Def *def, qboolean use );
void vk_get_pipeline_def( uint32_t pipeline, Vk_Pipeline_Def *def );

void vk_create_post_process_pipeline( int program_index, uint32_t width, uint32_t height );
void vk_create_pipelines( void );

//
// Rendering setup.
//

void vk_clear_color( const vec4_t color );
void vk_clear_depth( qboolean clear_stencil );
void vk_set_color_write_mask( qboolean r, qboolean g, qboolean b, qboolean a );
void vk_begin_frame( void );
void vk_end_frame( void );
void vk_present_frame( void );
void vk_prepare_2d( void );
void vk_prepare_frame_temporal_state( void );

void vk_end_render_pass( void );
void vk_begin_main_render_pass( void );
void vk_begin_post_bloom_render_pass( void );
void vk_begin_ui_overlay_render_pass( void );
void vk_begin_bloom_extract_render_pass( void );
void vk_begin_blur_render_pass( uint32_t index );
void vk_begin_ssao_render_pass( void );
void vk_begin_ssao_blur_render_pass( void );
void vk_begin_ssao_combine_render_pass( void );
struct drawSurfsCommand_s;
void vk_oit_pass( const struct drawSurfsCommand_s *cmd );
void vk_begin_ssr_render_pass( void );
void vk_ssr_pass( void );
void vk_vegetation_wind_dispatch( void );
void vk_vegetation_add_from_tess( int oldVertexCount, int newVertexCount );
void vk_vegetation_clear_staging( void );
qboolean vk_begin_sun_shadow_render_pass( void );
void vk_end_sun_shadow_render_pass( void );

void vk_bind_pipeline( uint32_t pipeline );
void vk_bind_index( void );
void vk_bind_index_ext( const int numIndexes, const uint32_t*indexes );
void vk_bind_geometry( uint32_t flags );
void vk_bind_lighting( int stage, int bundle );
void vk_draw_geometry( Vk_Depth_Range depth_range, qboolean indexed );
void vk_draw_dot( uint32_t storage_offset );

void vk_read_pixels( byte* buffer, uint32_t width, uint32_t height ); // screenshots
qboolean vk_bloom( void );
qboolean vk_ssao_pass( void );

qboolean vk_alloc_vbo( const byte *vbo_data, int vbo_size );
void vk_update_mvp( const float *m );

uint32_t vk_tess_index( uint32_t numIndexes, const void *src );
void *vk_alloc_storage( size_t size, uint32_t *offset );
void vk_set_iqm_storage_offsets( uint32_t skin_offset, uint32_t morph_offset );
void vk_reset_iqm_storage_offsets( void );
void vk_bind_index_buffer( VkBuffer buffer, uint32_t offset );
#ifdef USE_VBO
void vk_draw_indexed( uint32_t indexCount, uint32_t firstIndex );
#endif
void vk_reset_descriptor( int index );
void vk_update_descriptor( int index, VkDescriptorSet descriptor );
void vk_update_descriptor_offset( int index, uint32_t offset );
void vk_bind_descriptor_sets( void );

void vk_update_post_process_pipelines( void );

const char *vk_format_string( VkFormat format );

void VBO_PrepareQueues( void );
void VBO_RenderIBOItems( void );
void VBO_ClearQueue( void );

/* glTF primitive VBO: creates device-local vertex+index buffers from packed data */
qboolean vk_create_gltf_buffers( const byte *vboData, int vboSize, const uint32_t *idxData, int idxCount,
	VkBuffer *outVertexBuffer, VkBuffer *outIndexBuffer );

/* GPU occlusion culling for entities */
struct drawSurfsCommand_s;
void vk_occlusion_pass( const struct drawSurfsCommand_s *cmd );
void vk_occlusion_readback( void );
void vk_occlusion_draw_entity_bboxes( const struct drawSurfsCommand_s *cmd );

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
#ifdef USE_UPLOAD_QUEUE
	VkSemaphore rendering_finished2;
#endif
	VkFence rendering_finished_fence;
	qboolean waitForFence;

	VkBuffer vertex_buffer;
	byte *vertex_buffer_ptr; // pointer to mapped vertex buffer
	uint32_t vertex_buffer_offset; // VkDeviceSize

	VkDescriptorSet uniform_descriptor;
	uint32_t		uniform_read_offset;
	uint32_t		iqm_skin_offset;
	uint32_t		iqm_morph_offset;
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
	qboolean device_lost;  /* VK_ERROR_DEVICE_LOST detected; skip Vulkan API calls during shutdown */
	VkImage swapchain_images[MAX_SWAPCHAIN_IMAGES];
	VkImageView swapchain_image_views[MAX_SWAPCHAIN_IMAGES];
	VkSemaphore swapchain_rendering_finished[MAX_SWAPCHAIN_IMAGES];
	//uint32_t swapchain_image_index;

	VkCommandPool command_pool;
#ifdef USE_UPLOAD_QUEUE
	VkCommandBuffer staging_command_buffer;
#endif

	VkDeviceMemory image_memory[ MAX_ATTACHMENTS_IN_POOL ];
	uint32_t image_memory_count;

	struct {
		VkRenderPass main;
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
		VkRenderPass oit_resolve;
		VkRenderPass ssr;
#ifdef VK_PBR_BRDFLUT
		VkRenderPass brdflut;
#endif
		VkRenderPass cubemap;
		VkRenderPass smaa_edge;
		VkRenderPass smaa_blend;
		VkRenderPass smaa_compose;
		VkRenderPass volumetric;
		VkRenderPass atmosphere;
	} render_pass;

	VkDescriptorPool descriptor_pool;
	VkDescriptorSetLayout set_layout_sampler;	// combined image sampler
	VkDescriptorSetLayout set_layout_uniform;	// dynamic uniform buffer
	VkDescriptorSetLayout set_layout_storage;	// feedback buffer
	VkDescriptorSetLayout set_layout_postfx_uniform;	// post-process params uniform buffer

	VkPipelineLayout pipeline_layout;			// default shaders
	VkPipelineLayout pipeline_layout_storage;	// flare test shader layout
	VkPipelineLayout pipeline_layout_post_process;	// post-processing
	VkPipelineLayout pipeline_layout_blend;		// post-processing
	VkPipelineLayout pipeline_layout_smaa;
	VkPipelineLayout pipeline_layout_ssao;		// ssao (depth + push constants)
	VkPipelineLayout pipeline_layout_ssao_combine;	// ssao combine (color + ao)
	VkPipelineLayout pipeline_layout_oit_resolve;	// oit resolve (opaque + accum)
	VkPipelineLayout pipeline_layout_oit_accum;	// oit accum (sampler + push constants)
	VkPipelineLayout pipeline_layout_ssr;		// ssr (color + depth + push constants)
	VkPipelineLayout pipeline_layout_atmosphere;	// atmosphere (push constants only)
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

	VkDescriptorSetLayout vegwind_layout;
	VkPipelineLayout pipeline_layout_vegwind;
	VkPipeline vegwind_pipeline;
	VkBuffer vegwind_vertex_buffer;
	VkDeviceMemory vegwind_vertex_memory;
	VkDescriptorSet vegwind_descriptor;

	VkDescriptorSet color_descriptor[NUM_COMMAND_BUFFERS];	/* per-frame base scene color (VUID-03047) */
	VkDescriptorSet post_color_descriptor[NUM_COMMAND_BUFFERS];	/* per-frame mutable post-fog/gamma source */
	VkImageView post_fog_color_source;	/* last source for gamma (color_image or smaa_output) */
	VkImageView scene_post_fog_color_source;	/* scene-only source for luminance/exposure before HUD/console */
	VkDescriptorSet depth_descriptor;
	VkDescriptorSet postfx_params_descriptor[NUM_COMMAND_BUFFERS];
	VkDescriptorSet smaa_edge_descriptor;
	VkDescriptorSet smaa_blend_descriptor;
	VkDescriptorSet smaa_compose_descriptor;

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

	VkImage bloom_image[1+VK_NUM_BLOOM_PASSES*2];
	VkImageView bloom_image_view[1+VK_NUM_BLOOM_PASSES*2];

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
	VkDescriptorSet oit_opaque_descriptor;	/* opaque copy for OIT resolve */
	VkDescriptorSet oit_accum_descriptor;
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

	// statistics
	struct {
		VkDeviceSize vertex_buffer_max;
		uint32_t push_size;
		uint32_t push_size_max;
	} stats;

	qboolean inRenderPass;

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
		qboolean sharedCameraCut;
		qboolean unreliableMotionThisFrame;
		qboolean worldWasValid;
		qboolean noWorldModel;
		qboolean stableGameplayState;
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
			VkShaderModule flowmap[2];      // fog[0,1] - water flowmap
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
	VkPipeline ssao_pipeline;
	VkPipeline hbao_pipeline;
	VkPipeline ssao_blur_pipeline;
	VkPipeline ssao_combine_pipeline;
	VkPipeline oit_accum_pipeline;	/* WBOIT accumulation for transparent surfaces */
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
	qboolean wideLines;
	qboolean samplerAnisotropy;
	qboolean fragmentStores;
	qboolean dedicatedAllocation;
	qboolean debugMarkers;
	qboolean colorWriteMaskDynamic;

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
	qboolean blitEnabled;
	qboolean msaaActive;
	qboolean msaaSampleShading;	/* per-sample shading when MSAA on (better alpha/specular, higher cost) */
	qboolean smaaActive;
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

	renderPass_t renderPassIndex;

	uint32_t screenMapWidth;
	uint32_t screenMapHeight;
	uint32_t screenMapSamples;

	uint32_t image_chunk_size;

	// Physical device limit for 2D image dimension (VkPhysicalDeviceLimits::maxImageDimension2D).
	uint32_t hwMaxImageDimension2D;

	uint32_t maxBoundDescriptorSets;

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

/* Vulkan function pointers (loaded at init, used by vk_image_layout.c, vk_render_pass.c) */
extern PFN_vkCmdBeginRenderPass		qvkCmdBeginRenderPass;
extern PFN_vkCmdEndRenderPass		qvkCmdEndRenderPass;
extern PFN_vkCmdPipelineBarrier		qvkCmdPipelineBarrier;
extern PFN_vkCmdSetScissor			qvkCmdSetScissor;
extern PFN_vkCmdSetViewport			qvkCmdSetViewport;
extern PFN_vkUpdateDescriptorSets	qvkUpdateDescriptorSets;
