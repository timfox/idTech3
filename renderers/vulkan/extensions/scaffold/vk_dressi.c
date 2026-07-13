/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Dressi — HardSoftRas forward + Dressi-AD inverse UV (Vulkan). Full AD / stage
packing JIT is scaffolded; fixed subpass chain maps paper Fig. 3 for demo path.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_dressi.h"
#include "vk_util.h"
#include "vk_view_state.h"
#include "vk_image_layout.h"
#include "vk_cmd.h"

#define DRESSI_MAX_K        5
#define DRESSI_MAX_TRIS     128
#define DRESSI_MAX_VERTS    ( DRESSI_MAX_TRIS * 3 )

typedef struct {
	float pos[3];
	float centroid[3];
} dressiVert_t;

static struct {
	qboolean            ready;
	qboolean            mesh_ready;
	uint32_t            tri_count;
	uint32_t            vert_count;
	uint32_t            width;
	uint32_t            height;
	VkBuffer            vbo;
	VkDeviceMemory      vbo_mem;
	VkBuffer            tri_clip_ssbo;
	VkDeviceMemory      tri_clip_mem;
	void               *tri_clip_ptr;
	VkImage             peel_array;
	VkImageView         peel_array_view;
	VkDeviceMemory      peel_mem;
	VkImage             peel_depth;
	VkImageView         peel_depth_view;
	VkDeviceMemory      peel_depth_mem;
	VkImage             prev_peel_depth;
	VkImageView         prev_peel_depth_view;
	VkDeviceMemory      prev_peel_depth_mem;
	VkImage             blend_image;
	VkImageView         blend_view;
	VkDeviceMemory      blend_mem;
	VkFramebuffer       peel_fb;
	VkFramebuffer       peel_fbs[DRESSI_MAX_K];
	VkImageView         peel_layer_views[DRESSI_MAX_K];
	VkRenderPass        peel_rp;
	VkPipelineLayout    soft_pl;
	VkPipeline          soft_pipeline;
	VkDescriptorSetLayout soft_dsl;
	VkDescriptorPool    soft_pool;
	VkDescriptorSet     soft_set;
	VkPipelineLayout    blend_pl;
	VkPipeline          blend_pipeline;
	VkDescriptorSetLayout blend_dsl;
	VkDescriptorPool    blend_pool;
	VkDescriptorSet     blend_set;
	VkPipelineLayout    composite_pl;
	VkPipeline          composite_pipeline;
	VkDescriptorSetLayout composite_dsl;
	VkDescriptorPool    composite_pool;
	VkDescriptorSet     composite_set;
	VkPipelineLayout    invuv_pl;
	VkPipeline          invuv_pipeline;
	VkDescriptorSetLayout invuv_dsl;
	VkDescriptorPool    invuv_pool;
	VkDescriptorSet     invuv_set;
	qboolean            cmds_registered;
} dressi;

static cvar_t *r_dressi;
static cvar_t *r_dressi_demo;
static cvar_t *r_dressi_K;
static cvar_t *r_dressi_r;
static cvar_t *r_dressi_sigma;
static cvar_t *r_dressi_delta;
static cvar_t *r_dressi_strength;
static cvar_t *r_dressi_debug;
static cvar_t *r_dressi_inverseUv;
static cvar_t *r_dressi_demoScale;

typedef struct {
	float mvp[16];
	float params[4];
} dressiSoftVertPush_t;

typedef struct {
	float extent[4];
	float params[4];
} dressiSoftFragPush_t;

typedef struct {
	float extent[4];
	float params[4];
} dressiBlendPush_t;

typedef struct {
	float extent[4];
	float params[4];
} dressiCompositePush_t;

static VkSampler DRESSI_LinearSampler( void )
{
	static VkSampler sampler = VK_NULL_HANDLE;

	if ( sampler != VK_NULL_HANDLE ) {
		return sampler;
	}
	{
		Vk_Sampler_Def def;

		Com_Memset( &def, 0, sizeof( def ) );
		def.gl_min_filter = GL_LINEAR;
		def.gl_mag_filter = GL_LINEAR;
		def.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler = vk_find_sampler( &def );
	}
	return sampler;
}

static void DRESSI_BarrierImage( VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout )
{
	record_image_layout_transition( cmd, image, VK_IMAGE_ASPECT_COLOR_BIT, oldLayout, newLayout, 0, 0 );
}

static void DRESSI_BarrierDepth( VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout )
{
	record_image_layout_transition( cmd, image, VK_IMAGE_ASPECT_DEPTH_BIT, oldLayout, newLayout, 0, 0 );
}

static void DRESSI_Status_f( void )
{
	ri.Printf( PRINT_ALL,
		"[VK][Dressi] active=%d ready=%d demo=%d tris=%u %ux%u K=%d r=%.4f sigma=%.4f stages=3 (raster,blend,composite)\n",
		vk_dressi_active() ? 1 : 0,
		dressi.ready ? 1 : 0,
		( r_dressi_demo && r_dressi_demo->integer ) ? 1 : 0,
		dressi.tri_count, dressi.width, dressi.height,
		r_dressi_K ? r_dressi_K->integer : 3,
		r_dressi_r ? r_dressi_r->value : 0.01f,
		r_dressi_sigma ? r_dressi_sigma->value : 0.0014f );
}

static void DRESSI_RegisterCommands( void )
{
	if ( dressi.cmds_registered ) {
		return;
	}
	ri.Cmd_AddCommand( "dressi_status", DRESSI_Status_f );
	dressi.cmds_registered = qtrue;
}

static void DRESSI_UnregisterCommands( void )
{
	if ( !dressi.cmds_registered ) {
		return;
	}
	ri.Cmd_RemoveCommand( "dressi_status" );
	dressi.cmds_registered = qfalse;
}

static void DRESSI_Normalize( vec3_t v )
{
	float len = sqrtf( v[0] * v[0] + v[1] * v[1] + v[2] * v[2] );
	if ( len > 1e-8f ) {
		v[0] /= len;
		v[1] /= len;
		v[2] /= len;
	}
}

static void DRESSI_AddTri( dressiVert_t *verts, uint32_t *vertCount, uint32_t *triCount,
	const float *a, const float *b, const float *c )
{
	vec3_t cent;
	uint32_t base;
	uint32_t i;

	if ( *triCount >= DRESSI_MAX_TRIS || *vertCount + 3 > DRESSI_MAX_VERTS ) {
		return;
	}
	cent[0] = ( a[0] + b[0] + c[0] ) / 3.0f;
	cent[1] = ( a[1] + b[1] + c[1] ) / 3.0f;
	cent[2] = ( a[2] + b[2] + c[2] ) / 3.0f;
	base = *vertCount;
	for ( i = 0; i < 3; i++ ) {
		const float *p = ( i == 0 ) ? a : ( ( i == 1 ) ? b : c );
		verts[base + i].pos[0] = p[0];
		verts[base + i].pos[1] = p[1];
		verts[base + i].pos[2] = p[2];
		verts[base + i].centroid[0] = cent[0];
		verts[base + i].centroid[1] = cent[1];
		verts[base + i].centroid[2] = cent[2];
	}
	*vertCount += 3;
	*triCount += 1;
}

static void DRESSI_Subdivide( const float *v0, const float *v1, const float *v2, int depth,
	dressiVert_t *verts, uint32_t *vertCount, uint32_t *triCount )
{
	float a[3], b[3], c[3], ab[3], bc[3], ca[3];

	if ( depth <= 0 ) {
		DRESSI_AddTri( verts, vertCount, triCount, v0, v1, v2 );
		return;
	}
	a[0] = v0[0]; a[1] = v0[1]; a[2] = v0[2];
	b[0] = v1[0]; b[1] = v1[1]; b[2] = v1[2];
	c[0] = v2[0]; c[1] = v2[1]; c[2] = v2[2];
	ab[0] = ( a[0] + b[0] ) * 0.5f; ab[1] = ( a[1] + b[1] ) * 0.5f; ab[2] = ( a[2] + b[2] ) * 0.5f;
	bc[0] = ( b[0] + c[0] ) * 0.5f; bc[1] = ( b[1] + c[1] ) * 0.5f; bc[2] = ( b[2] + c[2] ) * 0.5f;
	ca[0] = ( c[0] + a[0] ) * 0.5f; ca[1] = ( c[1] + a[1] ) * 0.5f; ca[2] = ( c[2] + a[2] ) * 0.5f;
	DRESSI_Normalize( ab );
	DRESSI_Normalize( bc );
	DRESSI_Normalize( ca );
	DRESSI_Subdivide( a, ab, ca, depth - 1, verts, vertCount, triCount );
	DRESSI_Subdivide( b, bc, ab, depth - 1, verts, vertCount, triCount );
	DRESSI_Subdivide( c, ca, bc, depth - 1, verts, vertCount, triCount );
	DRESSI_Subdivide( ab, bc, ca, depth - 1, verts, vertCount, triCount );
}

static void DRESSI_BuildIcosphere( void )
{
	static const float t = 1.618033988749f;
	float v[12][3];
	dressiVert_t verts[DRESSI_MAX_VERTS];
	uint32_t vertCount = 0;
	uint32_t triCount = 0;
	float scale;
	VkBufferCreateInfo bi;
	VkMemoryRequirements req;
	VkMemoryAllocateInfo ai;
	void *host;

	v[0][0] = -1; v[0][1] = t; v[0][2] = 0;
	v[1][0] = 1; v[1][1] = t; v[1][2] = 0;
	v[2][0] = -1; v[2][1] = -t; v[2][2] = 0;
	v[3][0] = 1; v[3][1] = -t; v[3][2] = 0;
	v[4][0] = 0; v[4][1] = -1; v[4][2] = t;
	v[5][0] = 0; v[5][1] = 1; v[5][2] = t;
	v[6][0] = 0; v[6][1] = -1; v[6][2] = -t;
	v[7][0] = 0; v[7][1] = 1; v[7][2] = -t;
	v[8][0] = t; v[8][1] = 0; v[8][2] = 1;
	v[9][0] = -t; v[9][1] = 0; v[9][2] = 1;
	v[10][0] = t; v[10][1] = 0; v[10][2] = -1;
	v[11][0] = -t; v[11][1] = 0; v[11][2] = -1;
	{
		int i;
		for ( i = 0; i < 12; i++ ) {
			DRESSI_Normalize( v[i] );
		}
	}
	DRESSI_Subdivide( v[0], v[5], v[9], 1, verts, &vertCount, &triCount );
	DRESSI_Subdivide( v[0], v[9], v[11], 1, verts, &vertCount, &triCount );
	DRESSI_Subdivide( v[0], v[11], v[7], 1, verts, &vertCount, &triCount );
	DRESSI_Subdivide( v[0], v[7], v[5], 1, verts, &vertCount, &triCount );
	DRESSI_Subdivide( v[1], v[8], v[10], 1, verts, &vertCount, &triCount );
	DRESSI_Subdivide( v[1], v[10], v[6], 1, verts, &vertCount, &triCount );
	DRESSI_Subdivide( v[1], v[6], v[2], 1, verts, &vertCount, &triCount );
	DRESSI_Subdivide( v[1], v[2], v[8], 1, verts, &vertCount, &triCount );
	DRESSI_Subdivide( v[3], v[4], v[9], 1, verts, &vertCount, &triCount );
	DRESSI_Subdivide( v[3], v[9], v[11], 1, verts, &vertCount, &triCount );
	DRESSI_Subdivide( v[3], v[11], v[6], 1, verts, &vertCount, &triCount );
	DRESSI_Subdivide( v[3], v[6], v[4], 1, verts, &vertCount, &triCount );
	DRESSI_Subdivide( v[5], v[7], v[8], 1, verts, &vertCount, &triCount );
	DRESSI_Subdivide( v[5], v[8], v[4], 1, verts, &vertCount, &triCount );
	DRESSI_Subdivide( v[5], v[4], v[9], 1, verts, &vertCount, &triCount );
	DRESSI_Subdivide( v[7], v[11], v[10], 1, verts, &vertCount, &triCount );
	DRESSI_Subdivide( v[7], v[10], v[8], 1, verts, &vertCount, &triCount );
	DRESSI_Subdivide( v[11], v[9], v[6], 1, verts, &vertCount, &triCount );
	DRESSI_Subdivide( v[11], v[6], v[10], 1, verts, &vertCount, &triCount );
	DRESSI_Subdivide( v[2], v[6], v[4], 1, verts, &vertCount, &triCount );
	DRESSI_Subdivide( v[2], v[4], v[8], 1, verts, &vertCount, &triCount );
	DRESSI_Subdivide( v[10], v[6], v[8], 1, verts, &vertCount, &triCount );

	scale = r_dressi_demoScale ? r_dressi_demoScale->value : 8.0f;
	{
		uint32_t i;
		for ( i = 0; i < vertCount; i++ ) {
			verts[i].pos[0] = verts[i].pos[0] * scale + backEnd.viewParms.or.origin[0];
			verts[i].pos[1] = verts[i].pos[1] * scale + backEnd.viewParms.or.origin[1];
			verts[i].pos[2] = verts[i].pos[2] * scale + backEnd.viewParms.or.origin[2] + 64.0f;
			verts[i].centroid[0] = verts[i].centroid[0] * scale + backEnd.viewParms.or.origin[0];
			verts[i].centroid[1] = verts[i].centroid[1] * scale + backEnd.viewParms.or.origin[1];
			verts[i].centroid[2] = verts[i].centroid[2] * scale + backEnd.viewParms.or.origin[2] + 64.0f;
		}
	}

	dressi.tri_count = triCount;
	dressi.vert_count = vertCount;

	if ( dressi.vbo != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, dressi.vbo, NULL );
		dressi.vbo = VK_NULL_HANDLE;
	}
	if ( dressi.vbo_mem != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, dressi.vbo_mem, NULL );
		dressi.vbo_mem = VK_NULL_HANDLE;
	}

	Com_Memset( &bi, 0, sizeof( bi ) );
	bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bi.size = (VkDeviceSize)vertCount * sizeof( dressiVert_t );
	bi.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	VK_CHECK( qvkCreateBuffer( vk.device, &bi, NULL, &dressi.vbo ) );
	qvkGetBufferMemoryRequirements( vk.device, dressi.vbo, &req );
	Com_Memset( &ai, 0, sizeof( ai ) );
	ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ai.allocationSize = req.size;
	ai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &ai, NULL, &dressi.vbo_mem ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, dressi.vbo, dressi.vbo_mem, 0 ) );
	VK_CHECK( qvkMapMemory( vk.device, dressi.vbo_mem, 0, bi.size, 0, &host ) );
	Com_Memcpy( host, verts, (size_t)bi.size );
	qvkUnmapMemory( vk.device, dressi.vbo_mem );

	if ( dressi.tri_clip_ssbo == VK_NULL_HANDLE ) {
		bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		bi.size = (VkDeviceSize)DRESSI_MAX_TRIS * 3 * sizeof( float ) * 4;
		VK_CHECK( qvkCreateBuffer( vk.device, &bi, NULL, &dressi.tri_clip_ssbo ) );
		qvkGetBufferMemoryRequirements( vk.device, dressi.tri_clip_ssbo, &req );
		ai.allocationSize = req.size;
		VK_CHECK( qvkAllocateMemory( vk.device, &ai, NULL, &dressi.tri_clip_mem ) );
		VK_CHECK( qvkBindBufferMemory( vk.device, dressi.tri_clip_ssbo, dressi.tri_clip_mem, 0 ) );
		VK_CHECK( qvkMapMemory( vk.device, dressi.tri_clip_mem, 0, bi.size, 0, &dressi.tri_clip_ptr ) );
	}

	dressi.mesh_ready = qtrue;
}

static void DRESSI_UpdateTriClip( const float *mvp )
{
	uint32_t t;
	float *clip = (float *)dressi.tri_clip_ptr;
	dressiVert_t *host;
	VkDeviceSize sz;

	if ( !dressi.mesh_ready || !dressi.tri_clip_ptr || dressi.vbo_mem == VK_NULL_HANDLE ) {
		return;
	}
	sz = (VkDeviceSize)dressi.vert_count * sizeof( dressiVert_t );
	VK_CHECK( qvkMapMemory( vk.device, dressi.vbo_mem, 0, sz, 0, (void **)&host ) );
	for ( t = 0; t < dressi.tri_count; t++ ) {
		uint32_t i;
		for ( i = 0; i < 3; i++ ) {
			uint32_t vi = t * 3 + i;
			const float *p = host[vi].pos;
			float cx = mvp[0] * p[0] + mvp[4] * p[1] + mvp[8] * p[2] + mvp[12];
			float cy = mvp[1] * p[0] + mvp[5] * p[1] + mvp[9] * p[2] + mvp[13];
			float cz = mvp[2] * p[0] + mvp[6] * p[1] + mvp[10] * p[2] + mvp[14];
			float cw = mvp[3] * p[0] + mvp[7] * p[1] + mvp[11] * p[2] + mvp[15];
			clip[vi * 4 + 0] = cx;
			clip[vi * 4 + 1] = cy;
			clip[vi * 4 + 2] = cz;
			clip[vi * 4 + 3] = cw;
		}
	}
	qvkUnmapMemory( vk.device, dressi.vbo_mem );
}

static void DRESSI_DestroyImages( void )
{
	if ( dressi.peel_array_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, dressi.peel_array_view, NULL );
		dressi.peel_array_view = VK_NULL_HANDLE;
	}
	if ( dressi.peel_array != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, dressi.peel_array, NULL );
		dressi.peel_array = VK_NULL_HANDLE;
	}
	if ( dressi.peel_mem != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, dressi.peel_mem, NULL );
		dressi.peel_mem = VK_NULL_HANDLE;
	}
	if ( dressi.peel_depth_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, dressi.peel_depth_view, NULL );
		dressi.peel_depth_view = VK_NULL_HANDLE;
	}
	if ( dressi.peel_depth != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, dressi.peel_depth, NULL );
		dressi.peel_depth = VK_NULL_HANDLE;
	}
	if ( dressi.peel_depth_mem != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, dressi.peel_depth_mem, NULL );
		dressi.peel_depth_mem = VK_NULL_HANDLE;
	}
	if ( dressi.prev_peel_depth_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, dressi.prev_peel_depth_view, NULL );
		dressi.prev_peel_depth_view = VK_NULL_HANDLE;
	}
	if ( dressi.prev_peel_depth != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, dressi.prev_peel_depth, NULL );
		dressi.prev_peel_depth = VK_NULL_HANDLE;
	}
	if ( dressi.prev_peel_depth_mem != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, dressi.prev_peel_depth_mem, NULL );
		dressi.prev_peel_depth_mem = VK_NULL_HANDLE;
	}
	if ( dressi.blend_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, dressi.blend_view, NULL );
		dressi.blend_view = VK_NULL_HANDLE;
	}
	if ( dressi.blend_image != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, dressi.blend_image, NULL );
		dressi.blend_image = VK_NULL_HANDLE;
	}
	if ( dressi.blend_mem != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, dressi.blend_mem, NULL );
		dressi.blend_mem = VK_NULL_HANDLE;
	}
	if ( dressi.peel_fb != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, dressi.peel_fb, NULL );
		dressi.peel_fb = VK_NULL_HANDLE;
	}
	{
		uint32_t k;
		for ( k = 0; k < DRESSI_MAX_K; k++ ) {
			if ( dressi.peel_fbs[k] != VK_NULL_HANDLE ) {
				qvkDestroyFramebuffer( vk.device, dressi.peel_fbs[k], NULL );
				dressi.peel_fbs[k] = VK_NULL_HANDLE;
			}
			if ( dressi.peel_layer_views[k] != VK_NULL_HANDLE ) {
				qvkDestroyImageView( vk.device, dressi.peel_layer_views[k], NULL );
				dressi.peel_layer_views[k] = VK_NULL_HANDLE;
			}
		}
	}
	if ( dressi.peel_rp != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, dressi.peel_rp, NULL );
		dressi.peel_rp = VK_NULL_HANDLE;
	}
}

static qboolean DRESSI_CreateTargets( uint32_t w, uint32_t h )
{
	VkImageCreateInfo ici;
	VkMemoryRequirements req;
	VkMemoryAllocateInfo ai;
	VkImageViewCreateInfo ivci;
	VkRenderPassCreateInfo rpci;
	VkAttachmentDescription att[2];
	VkAttachmentReference colorRef;
	VkAttachmentReference depthRef;
	VkSubpassDescription subpass;
	VkSubpassDependency dep;
	VkFramebufferCreateInfo fbci;

	DRESSI_DestroyImages();
	dressi.width = w;
	dressi.height = h;

	Com_Memset( &ici, 0, sizeof( ici ) );
	ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ici.imageType = VK_IMAGE_TYPE_2D;
	ici.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	ici.extent.width = w;
	ici.extent.height = h;
	ici.extent.depth = 1;
	ici.mipLevels = 1;
	ici.arrayLayers = DRESSI_MAX_K;
	ici.samples = VK_SAMPLE_COUNT_1_BIT;
	ici.tiling = VK_IMAGE_TILING_OPTIMAL;
	ici.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	ici.flags = VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
	VK_CHECK( qvkCreateImage( vk.device, &ici, NULL, &dressi.peel_array ) );
	qvkGetImageMemoryRequirements( vk.device, dressi.peel_array, &req );
	Com_Memset( &ai, 0, sizeof( ai ) );
	ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ai.allocationSize = req.size;
	ai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &ai, NULL, &dressi.peel_mem ) );
	VK_CHECK( qvkBindImageMemory( vk.device, dressi.peel_array, dressi.peel_mem, 0 ) );

	Com_Memset( &ivci, 0, sizeof( ivci ) );
	ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	ivci.image = dressi.peel_array;
	ivci.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	ivci.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	ivci.subresourceRange.levelCount = 1;
	ivci.subresourceRange.layerCount = DRESSI_MAX_K;
	VK_CHECK( qvkCreateImageView( vk.device, &ivci, NULL, &dressi.peel_array_view ) );

	ici.arrayLayers = 1;
	ici.format = VK_FORMAT_D32_SFLOAT;
	ici.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	ici.flags = 0;
	VK_CHECK( qvkCreateImage( vk.device, &ici, NULL, &dressi.peel_depth ) );
	qvkGetImageMemoryRequirements( vk.device, dressi.peel_depth, &req );
	ai.allocationSize = req.size;
	VK_CHECK( qvkAllocateMemory( vk.device, &ai, NULL, &dressi.peel_depth_mem ) );
	VK_CHECK( qvkBindImageMemory( vk.device, dressi.peel_depth, dressi.peel_depth_mem, 0 ) );
	ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
	ivci.format = VK_FORMAT_D32_SFLOAT;
	ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	ivci.subresourceRange.layerCount = 1;
	ivci.image = dressi.peel_depth;
	VK_CHECK( qvkCreateImageView( vk.device, &ivci, NULL, &dressi.peel_depth_view ) );

	ici.format = VK_FORMAT_D32_SFLOAT;
	ici.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	VK_CHECK( qvkCreateImage( vk.device, &ici, NULL, &dressi.prev_peel_depth ) );
	qvkGetImageMemoryRequirements( vk.device, dressi.prev_peel_depth, &req );
	ai.allocationSize = req.size;
	VK_CHECK( qvkAllocateMemory( vk.device, &ai, NULL, &dressi.prev_peel_depth_mem ) );
	VK_CHECK( qvkBindImageMemory( vk.device, dressi.prev_peel_depth, dressi.prev_peel_depth_mem, 0 ) );
	ivci.format = VK_FORMAT_D32_SFLOAT;
	ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	ivci.image = dressi.prev_peel_depth;
	VK_CHECK( qvkCreateImageView( vk.device, &ivci, NULL, &dressi.prev_peel_depth_view ) );

	ici.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	ici.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	VK_CHECK( qvkCreateImage( vk.device, &ici, NULL, &dressi.blend_image ) );
	qvkGetImageMemoryRequirements( vk.device, dressi.blend_image, &req );
	ai.allocationSize = req.size;
	VK_CHECK( qvkAllocateMemory( vk.device, &ai, NULL, &dressi.blend_mem ) );
	VK_CHECK( qvkBindImageMemory( vk.device, dressi.blend_image, dressi.blend_mem, 0 ) );
	ivci.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	ivci.image = dressi.blend_image;
	VK_CHECK( qvkCreateImageView( vk.device, &ivci, NULL, &dressi.blend_view ) );

	Com_Memset( att, 0, sizeof( att ) );
	att[0].format = VK_FORMAT_R16G16B16A16_SFLOAT;
	att[0].samples = VK_SAMPLE_COUNT_1_BIT;
	att[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	att[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	att[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	att[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	att[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	att[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	att[1].format = VK_FORMAT_D32_SFLOAT;
	att[1].samples = VK_SAMPLE_COUNT_1_BIT;
	att[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	att[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	att[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	att[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

	colorRef.attachment = 0;
	colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	depthRef.attachment = 1;
	depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	Com_Memset( &subpass, 0, sizeof( subpass ) );
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef;
	subpass.pDepthStencilAttachment = &depthRef;

	Com_Memset( &dep, 0, sizeof( dep ) );
	dep.srcSubpass = VK_SUBPASS_EXTERNAL;
	dep.dstSubpass = 0;
	dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	Com_Memset( &rpci, 0, sizeof( rpci ) );
	rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	rpci.attachmentCount = 2;
	rpci.pAttachments = att;
	rpci.subpassCount = 1;
	rpci.pSubpasses = &subpass;
	rpci.dependencyCount = 1;
	rpci.pDependencies = &dep;
	VK_CHECK( qvkCreateRenderPass( vk.device, &rpci, NULL, &dressi.peel_rp ) );

	Com_Memset( &fbci, 0, sizeof( fbci ) );
	fbci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fbci.renderPass = dressi.peel_rp;
	fbci.attachmentCount = 2;
	fbci.width = w;
	fbci.height = h;
	fbci.layers = 1;
	{
		uint32_t k;
		for ( k = 0; k < DRESSI_MAX_K; k++ ) {
			VkImageView views[2];

			Com_Memset( &ivci, 0, sizeof( ivci ) );
			ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			ivci.image = dressi.peel_array;
			ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
			ivci.format = VK_FORMAT_R16G16B16A16_SFLOAT;
			ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			ivci.subresourceRange.baseArrayLayer = k;
			ivci.subresourceRange.layerCount = 1;
			ivci.subresourceRange.levelCount = 1;
			VK_CHECK( qvkCreateImageView( vk.device, &ivci, NULL, &dressi.peel_layer_views[k] ) );
			views[0] = dressi.peel_layer_views[k];
			views[1] = dressi.peel_depth_view;
			fbci.pAttachments = views;
			VK_CHECK( qvkCreateFramebuffer( vk.device, &fbci, NULL, &dressi.peel_fbs[k] ) );
		}
		dressi.peel_fb = dressi.peel_fbs[0];
	}
	return qtrue;
}

static void DRESSI_CreateSoftPipeline( void )
{
	VkDescriptorSetLayoutBinding bindings[2];
	VkDescriptorSetLayoutCreateInfo dslci;
	VkPushConstantRange pcr;
	VkPipelineLayoutCreateInfo plci;
	VkPipelineShaderStageCreateInfo stages[2];
	VkVertexInputBindingDescription vbind;
	VkVertexInputAttributeDescription vattr[2];
	VkPipelineVertexInputStateCreateInfo vi;
	VkPipelineInputAssemblyStateCreateInfo ia;
	VkPipelineViewportStateCreateInfo vp;
	VkPipelineRasterizationStateCreateInfo rs;
	VkPipelineMultisampleStateCreateInfo ms;
	VkPipelineDepthStencilStateCreateInfo ds;
	VkPipelineColorBlendAttachmentState cbAtt;
	VkPipelineColorBlendStateCreateInfo cb;
	VkPipelineDynamicStateCreateInfo dyn;
	VkDynamicState dynStates[2];
	VkGraphicsPipelineCreateInfo gpci;

	if ( dressi.soft_pipeline != VK_NULL_HANDLE ||
		vk.modules.dressi_soft_vs == VK_NULL_HANDLE ||
		vk.modules.dressi_soft_fs == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( bindings, 0, sizeof( bindings ) );
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dslci.bindingCount = 2;
	dslci.pBindings = bindings;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &dslci, NULL, &dressi.soft_dsl ) );

	pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	pcr.offset = 0;
	pcr.size = 96;
	plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &dressi.soft_dsl;
	plci.pushConstantRangeCount = 1;
	plci.pPushConstantRanges = &pcr;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &plci, NULL, &dressi.soft_pl ) );

	vbind.binding = 0;
	vbind.stride = sizeof( dressiVert_t );
	vbind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	vattr[0].location = 0;
	vattr[0].binding = 0;
	vattr[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	vattr[0].offset = 0;
	vattr[1].location = 1;
	vattr[1].binding = 0;
	vattr[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	vattr[1].offset = 12;

	Com_Memset( stages, 0, sizeof( stages ) );
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = vk.modules.dressi_soft_vs;
	stages[0].pName = "main";
	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = vk.modules.dressi_soft_fs;
	stages[1].pName = "main";

	vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vi.vertexBindingDescriptionCount = 1;
	vi.pVertexBindingDescriptions = &vbind;
	vi.vertexAttributeDescriptionCount = 2;
	vi.pVertexAttributeDescriptions = vattr;
	ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	vp.viewportCount = 1;
	vp.scissorCount = 1;
	rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.cullMode = VK_CULL_MODE_NONE;
	rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rs.lineWidth = 1.0f;
	ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	ds.depthTestEnable = VK_TRUE;
	ds.depthWriteEnable = VK_TRUE;
	ds.depthCompareOp = VK_COMPARE_OP_LESS;
	cbAtt.colorWriteMask = 0xF;
	cbAtt.blendEnable = VK_FALSE;
	cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	cb.attachmentCount = 1;
	cb.pAttachments = &cbAtt;
	dynStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
	dynStates[1] = VK_DYNAMIC_STATE_SCISSOR;
	dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dyn.dynamicStateCount = 2;
	dyn.pDynamicStates = dynStates;

	Com_Memset( &gpci, 0, sizeof( gpci ) );
	gpci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	gpci.stageCount = 2;
	gpci.pStages = stages;
	gpci.pVertexInputState = &vi;
	gpci.pInputAssemblyState = &ia;
	gpci.pViewportState = &vp;
	gpci.pRasterizationState = &rs;
	gpci.pMultisampleState = &ms;
	gpci.pDepthStencilState = &ds;
	gpci.pColorBlendState = &cb;
	gpci.pDynamicState = &dyn;
	gpci.layout = dressi.soft_pl;
	gpci.renderPass = dressi.peel_rp;
	gpci.subpass = 0;
	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, vk.pipelineCache, 1, &gpci, NULL, &dressi.soft_pipeline ) );

	{
		VkDescriptorPoolSize sizes[2];
		VkDescriptorPoolCreateInfo softPoolCi;
		VkDescriptorSetAllocateInfo softSetAi;

		sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		sizes[0].descriptorCount = 1;
		sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		sizes[1].descriptorCount = 1;
		softPoolCi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		softPoolCi.maxSets = 1;
		softPoolCi.poolSizeCount = 2;
		softPoolCi.pPoolSizes = sizes;
		VK_CHECK( qvkCreateDescriptorPool( vk.device, &softPoolCi, NULL, &dressi.soft_pool ) );
		softSetAi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		softSetAi.descriptorPool = dressi.soft_pool;
		softSetAi.descriptorSetCount = 1;
		softSetAi.pSetLayouts = &dressi.soft_dsl;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &softSetAi, &dressi.soft_set ) );
	}
}

static void DRESSI_CreateComputePipelines( void )
{
	VkDescriptorSetLayoutBinding bb[2];
	VkDescriptorSetLayoutBinding cb[3];
	VkDescriptorSetLayoutBinding ib[2];
	VkDescriptorSetLayoutCreateInfo dslci;
	VkPushConstantRange pcr;
	VkPipelineLayoutCreateInfo plci;
	VkComputePipelineCreateInfo cpci;
	VkPipelineShaderStageCreateInfo cs;
	VkDescriptorPoolSize ps[3];
	VkDescriptorPoolCreateInfo dpci;
	VkDescriptorSetAllocateInfo dai;

	if ( dressi.blend_pipeline != VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( bb, 0, sizeof( bb ) );
	bb[0].binding = 0;
	bb[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bb[0].descriptorCount = 1;
	bb[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bb[1].binding = 1;
	bb[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bb[1].descriptorCount = 1;
	bb[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dslci.bindingCount = 2;
	dslci.pBindings = bb;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &dslci, NULL, &dressi.blend_dsl ) );
	pcr.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	pcr.size = 32;
	plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &dressi.blend_dsl;
	plci.pushConstantRangeCount = 1;
	plci.pPushConstantRanges = &pcr;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &plci, NULL, &dressi.blend_pl ) );
	cs.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	cs.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	cs.module = vk.modules.dressi_blend_cs;
	cs.pName = "main";
	cpci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	cpci.stage = cs;
	cpci.layout = dressi.blend_pl;
	VK_CHECK( qvkCreateComputePipelines( vk.device, vk.pipelineCache, 1, &cpci, NULL, &dressi.blend_pipeline ) );

	Com_Memset( cb, 0, sizeof( cb ) );
	cb[0].binding = 0;
	cb[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	cb[0].descriptorCount = 1;
	cb[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	cb[1].binding = 1;
	cb[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	cb[1].descriptorCount = 1;
	cb[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	cb[2].binding = 2;
	cb[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	cb[2].descriptorCount = 1;
	cb[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	dslci.pBindings = cb;
	dslci.bindingCount = 3;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &dslci, NULL, &dressi.composite_dsl ) );
	plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &dressi.composite_dsl;
	plci.pushConstantRangeCount = 1;
	plci.pPushConstantRanges = &pcr;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &plci, NULL, &dressi.composite_pl ) );
	cs.module = vk.modules.dressi_composite_cs;
	VK_CHECK( qvkCreateComputePipelines( vk.device, vk.pipelineCache, 1, &cpci, NULL, &dressi.composite_pipeline ) );

	Com_Memset( ib, 0, sizeof( ib ) );
	ib[0].binding = 0;
	ib[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	ib[0].descriptorCount = 1;
	ib[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	ib[1].binding = 1;
	ib[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	ib[1].descriptorCount = 1;
	ib[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	dslci.pBindings = ib;
	dslci.bindingCount = 2;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &dslci, NULL, &dressi.invuv_dsl ) );
	plci.pSetLayouts = &dressi.invuv_dsl;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &plci, NULL, &dressi.invuv_pl ) );
	cs.module = vk.modules.dressi_inverse_uv_cs;
	VK_CHECK( qvkCreateComputePipelines( vk.device, vk.pipelineCache, 1, &cpci, NULL, &dressi.invuv_pipeline ) );

	ps[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	ps[0].descriptorCount = 4;
	ps[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	ps[1].descriptorCount = 3;
	dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	dpci.maxSets = 3;
	dpci.poolSizeCount = 2;
	dpci.pPoolSizes = ps;
	VK_CHECK( qvkCreateDescriptorPool( vk.device, &dpci, NULL, &dressi.blend_pool ) );
	dai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	dai.descriptorSetCount = 1;
	dai.descriptorPool = dressi.blend_pool;
	dai.pSetLayouts = &dressi.blend_dsl;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &dai, &dressi.blend_set ) );
	dai.pSetLayouts = &dressi.composite_dsl;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &dai, &dressi.composite_set ) );
	dpci.maxSets = 1;
	VK_CHECK( qvkCreateDescriptorPool( vk.device, &dpci, NULL, &dressi.invuv_pool ) );
	dai.descriptorPool = dressi.invuv_pool;
	dai.pSetLayouts = &dressi.invuv_dsl;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &dai, &dressi.invuv_set ) );
	dpci.maxSets = 1;
	VK_CHECK( qvkCreateDescriptorPool( vk.device, &dpci, NULL, &dressi.composite_pool ) );
}

void R_Dressi_Init( void )
{
	r_dressi = ri.Cvar_Get( "r_dressi", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_dressi_demo = ri.Cvar_Get( "r_dressi_demo", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_dressi_K = ri.Cvar_Get( "r_dressi_K", "3", CVAR_ARCHIVE_ND );
	r_dressi_r = ri.Cvar_Get( "r_dressi_r", "0.01", CVAR_ARCHIVE_ND );
	r_dressi_sigma = ri.Cvar_Get( "r_dressi_sigma", "0.0", CVAR_ARCHIVE_ND );
	r_dressi_delta = ri.Cvar_Get( "r_dressi_delta", "0.01", CVAR_ARCHIVE_ND );
	r_dressi_strength = ri.Cvar_Get( "r_dressi_strength", "0.85", CVAR_ARCHIVE_ND );
	r_dressi_debug = ri.Cvar_Get( "r_dressi_debug", "0", CVAR_ARCHIVE_ND );
	r_dressi_inverseUv = ri.Cvar_Get( "r_dressi_inverseUv", "0", CVAR_ARCHIVE_ND );
	r_dressi_demoScale = ri.Cvar_Get( "r_dressi_demoScale", "8", CVAR_ARCHIVE_ND );

	ri.Cvar_CheckRange( r_dressi, "0", "1", CV_INTEGER );
	ri.Cvar_CheckRange( r_dressi_demo, "0", "1", CV_INTEGER );
	ri.Cvar_CheckRange( r_dressi_K, "1", "5", CV_INTEGER );
	ri.Cvar_CheckRange( r_dressi_r, "0", "0.2", CV_FLOAT );
	ri.Cvar_SetDescription( r_dressi,
		"Dressi differentiable renderer scaffold (HardSoftRas + inverse UV). Requires r_dressi_demo 1, r_fbo 1, vid_restart. See docs/DRESSI.md." );

	if ( r_dressi && r_dressi->integer ) {
		ri.Printf( PRINT_ALL, "[VK][Dressi] r_dressi=1 (latched; exec demo_dressi.cfg after vid_restart)\n" );
	}
	DRESSI_RegisterCommands();
}

qboolean vk_dressi_active( void )
{
	if ( !r_dressi || !r_dressi->integer ) {
		return qfalse;
	}
	if ( !r_dressi_demo || !r_dressi_demo->integer ) {
		return qfalse;
	}
	if ( !vk.fboActive ) {
		return qfalse;
	}
	return dressi.ready ? qtrue : qfalse;
}

static void DRESSI_EnsureReady( void )
{
	uint32_t w, h;

	if ( dressi.ready ) {
		return;
	}
	if ( !r_dressi || !r_dressi->integer || !r_dressi_demo || !r_dressi_demo->integer ) {
		return;
	}
	if ( vk.modules.dressi_soft_vs == VK_NULL_HANDLE ) {
		return;
	}

	w = vk_get_render_target_width();
	h = vk_get_render_target_height();
	if ( w == 0 || h == 0 ) {
		return;
	}
	if ( !DRESSI_CreateTargets( w, h ) ) {
		return;
	}
	DRESSI_CreateSoftPipeline();
	DRESSI_CreateComputePipelines();
	DRESSI_BuildIcosphere();
	dressi.ready = qtrue;
	ri.Printf( PRINT_ALL,
		"[VK][Dressi] HardSoftRas ready K=%d r=%.3f tris=%u (fixed stage chain: peel x K, blend, composite)\n",
		r_dressi_K ? r_dressi_K->integer : 3,
		r_dressi_r ? r_dressi_r->value : 0.01f,
		dressi.tri_count );
}

void vk_dressi_record_pass( VkCommandBuffer cmd )
{
	float mvp[16];
	float blurR;
	float sigma;
	uint32_t K;
	uint32_t k;
	VkRenderPassBeginInfo rpbi;
	VkClearValue clears[2];
	VkViewport viewport;
	VkRect2D scissor;
	VkDeviceSize vbOffset;
	dressiSoftVertPush_t vertPush;
	dressiSoftFragPush_t fragPush;
	dressiBlendPush_t blendPush;
	dressiCompositePush_t compPush;
	VkImageSubresourceRange peelRange;
	VkImageSubresourceRange depthRange;
	VkImageMemoryBarrier barrier;
	VkDescriptorImageInfo imgInfo;
	VkDescriptorBufferInfo bufInfo;
	VkWriteDescriptorSet writes[4];
	VkImageLayout colorRestoreLayout;

	if ( !vk_dressi_active() || !cmd || !dressi.ready || !dressi.mesh_ready ) {
		return;
	}
	if ( vk.renderPassIndex != RENDER_PASS_MAIN && vk.renderPassIndex != RENDER_PASS_POST_BLOOM ) {
		return;
	}
	if ( vk.color_image == VK_NULL_HANDLE || vk.color_image_view == VK_NULL_HANDLE ) {
		return;
	}

	DRESSI_EnsureReady();

	myGlMultMatrix( backEnd.viewParms.projectionMatrix, backEnd.viewParms.world.modelViewMatrix, mvp );
	DRESSI_UpdateTriClip( mvp );

	K = (uint32_t)( r_dressi_K ? r_dressi_K->integer : 3 );
	if ( K < 1 ) {
		K = 1;
	}
	if ( K > DRESSI_MAX_K ) {
		K = DRESSI_MAX_K;
	}
	blurR = r_dressi_r ? r_dressi_r->value : 0.01f;
	sigma = r_dressi_sigma ? r_dressi_sigma->value : 0.0f;
	if ( sigma <= 0.0f ) {
		sigma = blurR / 7.0f;
	}

	Com_Memset( &peelRange, 0, sizeof( peelRange ) );
	peelRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	peelRange.layerCount = DRESSI_MAX_K;
	Com_Memset( &depthRange, 0, sizeof( depthRange ) );
	depthRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

	DRESSI_BarrierImage( cmd, dressi.peel_array,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL );
	DRESSI_BarrierDepth( cmd, dressi.peel_depth,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL );
	DRESSI_BarrierDepth( cmd, dressi.prev_peel_depth,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL );
	DRESSI_BarrierImage( cmd, dressi.blend_image,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL );

	Com_Memset( clears, 0, sizeof( clears ) );
	clears[0].color.float32[0] = 0.0f;
	clears[0].color.float32[1] = 0.0f;
	clears[0].color.float32[2] = 0.0f;
	clears[0].color.float32[3] = 0.0f;
	clears[1].depthStencil.depth = 1.0f;
	clears[1].depthStencil.stencil = 0;

	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)dressi.width;
	viewport.height = (float)dressi.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = dressi.width;
	scissor.extent.height = dressi.height;

	Com_Memset( &vertPush, 0, sizeof( vertPush ) );
	Com_Memcpy( vertPush.mvp, mvp, sizeof( vertPush.mvp ) );
	vertPush.params[0] = blurR;

	Com_Memset( &fragPush, 0, sizeof( fragPush ) );
	fragPush.extent[0] = (float)dressi.width;
	fragPush.extent[1] = (float)dressi.height;
	fragPush.params[2] = sigma;

	Com_Memset( &imgInfo, 0, sizeof( imgInfo ) );
	imgInfo.sampler = DRESSI_LinearSampler();
	imgInfo.imageView = dressi.prev_peel_depth_view;
	imgInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	Com_Memset( &bufInfo, 0, sizeof( bufInfo ) );
	bufInfo.buffer = dressi.tri_clip_ssbo;
	bufInfo.offset = 0;
	bufInfo.range = (VkDeviceSize)DRESSI_MAX_TRIS * 3 * 16;
	Com_Memset( writes, 0, sizeof( writes ) );
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = dressi.soft_set;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[0].pImageInfo = &imgInfo;
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = dressi.soft_set;
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[1].pBufferInfo = &bufInfo;
	qvkUpdateDescriptorSets( vk.device, 2, writes, 0, NULL );

	for ( k = 0; k < K; k++ ) {
		Com_Memset( &rpbi, 0, sizeof( rpbi ) );
		rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		rpbi.renderPass = dressi.peel_rp;
		rpbi.framebuffer = dressi.peel_fbs[k];
		rpbi.renderArea.extent.width = dressi.width;
		rpbi.renderArea.extent.height = dressi.height;
		rpbi.clearValueCount = 2;
		rpbi.pClearValues = clears;

		fragPush.params[1] = ( k > 0 ) ? 1.0f : 0.0f;

		qvkCmdBeginRenderPass( cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE );
		qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, dressi.soft_pipeline );
		qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, dressi.soft_pl, 0, 1, &dressi.soft_set, 0, NULL );
		qvkCmdSetViewport( cmd, 0, 1, &viewport );
		qvkCmdSetScissor( cmd, 0, 1, &scissor );
		qvkCmdPushConstants( cmd, dressi.soft_pl, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof( vertPush ), &vertPush );
		qvkCmdPushConstants( cmd, dressi.soft_pl, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( fragPush ), &fragPush );
		vbOffset = 0;
		qvkCmdBindVertexBuffers( cmd, 0, 1, &dressi.vbo, &vbOffset );
		qvkCmdDraw( cmd, dressi.vert_count, 1, 0, 0 );
		qvkCmdEndRenderPass( cmd );

		if ( k + 1 < K ) {
			Com_Memset( &barrier, 0, sizeof( barrier ) );
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = dressi.peel_depth;
			barrier.subresourceRange = depthRange;
			qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				0, 0, NULL, 0, NULL, 1, &barrier );

			Com_Memset( &barrier, 0, sizeof( barrier ) );
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.image = dressi.prev_peel_depth;
			barrier.subresourceRange = depthRange;
			qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				0, 0, NULL, 0, NULL, 1, &barrier );

			{
				VkImageCopy copyRegion;
				Com_Memset( &copyRegion, 0, sizeof( copyRegion ) );
				copyRegion.extent.width = dressi.width;
				copyRegion.extent.height = dressi.height;
				copyRegion.extent.depth = 1;
				qvkCmdCopyImage( cmd, dressi.peel_depth, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					dressi.prev_peel_depth, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion );
			}

			DRESSI_BarrierDepth( cmd, dressi.peel_depth,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL );
			DRESSI_BarrierDepth( cmd, dressi.prev_peel_depth,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL );
		}
	}

	DRESSI_BarrierImage( cmd, dressi.peel_array,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	DRESSI_BarrierDepth( cmd, dressi.peel_depth,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL );

	{
		VkDescriptorImageInfo peelInfo;
		VkDescriptorImageInfo blendOutInfo;

		Com_Memset( &peelInfo, 0, sizeof( peelInfo ) );
		peelInfo.sampler = DRESSI_LinearSampler();
		peelInfo.imageView = dressi.peel_array_view;
		peelInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		Com_Memset( &blendOutInfo, 0, sizeof( blendOutInfo ) );
		blendOutInfo.imageView = dressi.blend_view;
		blendOutInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		writes[0].dstSet = dressi.blend_set;
		writes[0].dstBinding = 0;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[0].pImageInfo = &peelInfo;
		writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[1].dstSet = dressi.blend_set;
		writes[1].dstBinding = 1;
		writes[1].descriptorCount = 1;
		writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[1].pImageInfo = &blendOutInfo;
		qvkUpdateDescriptorSets( vk.device, 2, writes, 0, NULL );
	}

	Com_Memset( &blendPush, 0, sizeof( blendPush ) );
	blendPush.extent[0] = (float)dressi.width;
	blendPush.extent[1] = (float)dressi.height;
	blendPush.params[0] = (float)K;
	blendPush.params[1] = sigma;
	blendPush.params[2] = r_dressi_delta ? r_dressi_delta->value : blurR;
	blendPush.params[3] = 0.15f;

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, dressi.blend_pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, dressi.blend_pl, 0, 1, &dressi.blend_set, 0, NULL );
	qvkCmdPushConstants( cmd, dressi.blend_pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( blendPush ), &blendPush );
	qvkCmdDispatch( cmd, ( dressi.width + 7 ) / 8, ( dressi.height + 7 ) / 8, 1 );

	DRESSI_BarrierImage( cmd, dressi.blend_image,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );

	colorRestoreLayout = ( vk.renderPassIndex == RENDER_PASS_POST_BLOOM ) ?
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	DRESSI_BarrierImage( cmd, vk.color_image, colorRestoreLayout, VK_IMAGE_LAYOUT_GENERAL );

	imgInfo.imageView = vk.color_image_view;
	imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	writes[0].dstSet = dressi.composite_set;
	writes[0].dstBinding = 0;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[0].pImageInfo = &imgInfo;
	imgInfo.imageView = dressi.blend_view;
	imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	writes[1].dstSet = dressi.composite_set;
	writes[1].dstBinding = 1;
	writes[1].pImageInfo = &imgInfo;
	writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[2].dstSet = dressi.composite_set;
	writes[2].dstBinding = 2;
	writes[2].descriptorCount = 1;
	writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	imgInfo.imageView = vk.color_image_view;
	imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	writes[2].pImageInfo = &imgInfo;
	qvkUpdateDescriptorSets( vk.device, 3, writes, 0, NULL );

	Com_Memset( &compPush, 0, sizeof( compPush ) );
	compPush.extent[0] = (float)dressi.width;
	compPush.extent[1] = (float)dressi.height;
	compPush.params[0] = r_dressi_strength ? r_dressi_strength->value : 0.85f;
	compPush.params[1] = ( r_dressi_debug && r_dressi_debug->integer ) ? 1.0f : 0.0f;

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, dressi.composite_pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, dressi.composite_pl, 0, 1, &dressi.composite_set, 0, NULL );
	qvkCmdPushConstants( cmd, dressi.composite_pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( compPush ), &compPush );
	qvkCmdDispatch( cmd, ( dressi.width + 7 ) / 8, ( dressi.height + 7 ) / 8, 1 );

	DRESSI_BarrierImage( cmd, vk.color_image, VK_IMAGE_LAYOUT_GENERAL, colorRestoreLayout );
}

void R_Dressi_Shutdown( void )
{
	DRESSI_UnregisterCommands();
	if ( dressi.soft_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, dressi.soft_pipeline, NULL );
	}
	if ( dressi.blend_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, dressi.blend_pipeline, NULL );
	}
	if ( dressi.composite_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, dressi.composite_pipeline, NULL );
	}
	if ( dressi.invuv_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, dressi.invuv_pipeline, NULL );
	}
	if ( dressi.soft_pl != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, dressi.soft_pl, NULL );
	}
	if ( dressi.blend_pl != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, dressi.blend_pl, NULL );
	}
	if ( dressi.composite_pl != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, dressi.composite_pl, NULL );
	}
	if ( dressi.invuv_pl != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, dressi.invuv_pl, NULL );
	}
	if ( dressi.soft_dsl != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, dressi.soft_dsl, NULL );
	}
	if ( dressi.blend_dsl != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, dressi.blend_dsl, NULL );
	}
	if ( dressi.composite_dsl != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, dressi.composite_dsl, NULL );
	}
	if ( dressi.invuv_dsl != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, dressi.invuv_dsl, NULL );
	}
	if ( dressi.soft_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, dressi.soft_pool, NULL );
	}
	if ( dressi.blend_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, dressi.blend_pool, NULL );
	}
	if ( dressi.composite_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, dressi.composite_pool, NULL );
	}
	if ( dressi.invuv_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, dressi.invuv_pool, NULL );
	}
	if ( dressi.vbo != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, dressi.vbo, NULL );
	}
	if ( dressi.vbo_mem != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, dressi.vbo_mem, NULL );
	}
	if ( dressi.tri_clip_ssbo != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, dressi.tri_clip_ssbo, NULL );
	}
	if ( dressi.tri_clip_mem != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, dressi.tri_clip_mem, NULL );
	}
	DRESSI_DestroyImages();
	Com_Memset( &dressi, 0, sizeof( dressi ) );
}
