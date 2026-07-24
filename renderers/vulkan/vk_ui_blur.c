/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

CSS-style UI filter / backdrop-filter blur compositor. See vk_ui_blur.h for the
design overview.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_ui_blur.h"
#include "vk_util.h"

/* ---- tunables -------------------------------------------------------------- */

#define UIB_POOL_LEVELS   3    /* pyramid depth for the dual-Kawase path */
#define UIB_MAX_OPS       64   /* per-frame op budget */
#define UIB_MAX_GAUSS_TAPS 12  /* separable gaussian half-width cap */

/* Push constants shared with ui_blur_common.glsl (see that file for layout). */
typedef struct {
	float uvScaleOffset[4];
	float texelDir[4];
	float rect[4];
	float params[4];   /* cornerRadius(px), rotation(rad), opacity, flags */
	float tint[4];
	float misc[4];     /* debugMode, targetW, targetH, gaussTapCount */
} uibPush_t;

enum {
	UIB_FLAG_DECODE_SRGB = 1,
	UIB_FLAG_ENCODE_SRGB = 2,
	UIB_FLAG_APPLY_MASK  = 4,
	UIB_FLAG_APPLY_TINT  = 8
};

typedef struct {
	VkImage         image;
	VkDeviceMemory  memory;
	VkImageView     view;
	VkFramebuffer   framebuffer;   /* only for R16F level targets */
	VkDescriptorSet descriptor;    /* combined image sampler for reading */
	VkFormat        format;
	uint32_t        width, height;
	VkImageLayout   layout;
	qboolean        resident;      /* touched this frame (ui_filterDebug 6) */
} uibTexture_t;

typedef struct {
	int                  kind;      /* 0 = backdrop, 1 = filter layer */
	uiBackdropFilter_t   backdrop;
	uiCompositorLayer_t  layer;
} uibOp_t;

/* ---- module state ---------------------------------------------------------- */

/*
 * uiTransientTexturePool_t: transient pooled render targets for the blur
 * compositor. Reused across frames; rebuilt on quality / render-extent change.
 */
typedef struct uiTransientTexturePool_s {
	qboolean          initialized;
	qboolean          poolValid;
	int               builtQuality;   /* ui_blurQuality the pool was built for */
	uint32_t          builtW, builtH; /* render extent the pool was built for */

	uibTexture_t      sceneCopy;                 /* tonemapped swapchain copy (display space) */
	uibTexture_t      level[UIB_POOL_LEVELS][2]; /* linear ping-pong pyramid */

	VkRenderPass      transientPass;   /* LOAD_OP_DONT_CARE into an R16F level */
	VkRenderPass      transientClearPass; /* LOAD_OP_CLEAR (filter-layer ingest) */

	VkDescriptorPool  descPool;
	VkPipelineLayout  pipeLayout;

	VkPipeline        pipeSample;
	VkPipeline        pipeGauss;
	VkPipeline        pipeDown;
	VkPipeline        pipeUp;
	VkPipeline        pipeComposite;

	VkSampler         sampler;

	uibOp_t           ops[UIB_MAX_OPS];
	int               numOps;

	/* per-frame diagnostics */
	int               lastBackdrops;
	int               lastLayers;
	float             lastCpuMs;
	float             lastOpMs[UIB_MAX_OPS]; /* per-layer CPU record time */
	int               lastOpCount;

	/* static filter-layer cache: when the previous frame blurred the same
	 * single layer and nothing else touched the pool, level[0][0] still holds
	 * the blurred result and the build pass can be skipped. */
	uint32_t          layerCacheKey;
	qboolean          layerCacheValid;
} uiTransientTexturePool_t;

static uiTransientTexturePool_t uib;

/* cvars */
static cvar_t *ui_filterDebug;
static cvar_t *ui_blurQuality;
static cvar_t *ui_blurMaxRadius;
static cvar_t *ui_blurDownsampleThreshold;
static cvar_t *ui_blurCache;

/* ---- small helpers --------------------------------------------------------- */

static void uib_label_begin( const char *name, float r, float g, float b ) {
	if ( qvkCmdDebugMarkerBeginEXT && vk.cmd && vk.cmd->command_buffer ) {
		VkDebugMarkerMarkerInfoEXT info;
		Com_Memset( &info, 0, sizeof( info ) );
		info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
		info.pMarkerName = name;
		info.color[0] = r; info.color[1] = g; info.color[2] = b; info.color[3] = 1.0f;
		qvkCmdDebugMarkerBeginEXT( vk.cmd->command_buffer, &info );
	}
}

static void uib_label_end( void ) {
	if ( qvkCmdDebugMarkerEndEXT && vk.cmd && vk.cmd->command_buffer ) {
		qvkCmdDebugMarkerEndEXT( vk.cmd->command_buffer );
	}
}

static void uib_barrier( VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
	VkAccessFlags srcAccess, VkAccessFlags dstAccess,
	VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage )
{
	VkImageMemoryBarrier b;
	Com_Memset( &b, 0, sizeof( b ) );
	b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	b.srcAccessMask = srcAccess;
	b.dstAccessMask = dstAccess;
	b.oldLayout = oldLayout;
	b.newLayout = newLayout;
	b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	b.image = image;
	b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	b.subresourceRange.baseMipLevel = 0;
	b.subresourceRange.levelCount = 1;
	b.subresourceRange.baseArrayLayer = 0;
	b.subresourceRange.layerCount = 1;
	qvkCmdPipelineBarrier( vk.cmd->command_buffer, srcStage, dstStage, 0, 0, NULL, 0, NULL, 1, &b );
}

/* ---- texture creation ------------------------------------------------------ */

static void uib_destroy_texture( uibTexture_t *t ) {
	if ( t->framebuffer != VK_NULL_HANDLE ) { qvkDestroyFramebuffer( vk.device, t->framebuffer, NULL ); }
	if ( t->view != VK_NULL_HANDLE ) { qvkDestroyImageView( vk.device, t->view, NULL ); }
	if ( t->image != VK_NULL_HANDLE ) { qvkDestroyImage( vk.device, t->image, NULL ); }
	if ( t->memory != VK_NULL_HANDLE ) { qvkFreeMemory( vk.device, t->memory, NULL ); }
	Com_Memset( t, 0, sizeof( *t ) );
}

/* Create image + memory + view. Returns qfalse on any failure (soft). */
static qboolean uib_create_texture( uibTexture_t *t, uint32_t w, uint32_t h,
	VkFormat format, VkImageUsageFlags usage, const char *name )
{
	VkImageCreateInfo ci;
	VkMemoryRequirements mr;
	VkMemoryAllocateInfo ai;
	VkImageViewCreateInfo vi;
	uint32_t memType;

	Com_Memset( t, 0, sizeof( *t ) );
	t->format = format;
	t->width = w ? w : 1u;
	t->height = h ? h : 1u;
	t->layout = VK_IMAGE_LAYOUT_UNDEFINED;

	Com_Memset( &ci, 0, sizeof( ci ) );
	ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ci.imageType = VK_IMAGE_TYPE_2D;
	ci.format = format;
	ci.extent.width = t->width;
	ci.extent.height = t->height;
	ci.extent.depth = 1;
	ci.mipLevels = 1;
	ci.arrayLayers = 1;
	ci.samples = VK_SAMPLE_COUNT_1_BIT;
	ci.tiling = VK_IMAGE_TILING_OPTIMAL;
	ci.usage = usage;
	ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if ( qvkCreateImage( vk.device, &ci, NULL, &t->image ) != VK_SUCCESS ) {
		return qfalse;
	}

	qvkGetImageMemoryRequirements( vk.device, t->image, &mr );
	/* Soft path: never ERR_FATAL on missing memory type — caller disables blur. */
	memType = vk_find_memory_type2( vk.physical_device, mr.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, NULL );
	if ( memType == ~0U ) {
		uib_destroy_texture( t );
		return qfalse;
	}

	Com_Memset( &ai, 0, sizeof( ai ) );
	ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ai.allocationSize = mr.size;
	ai.memoryTypeIndex = memType;
	if ( qvkAllocateMemory( vk.device, &ai, NULL, &t->memory ) != VK_SUCCESS ) {
		uib_destroy_texture( t );
		return qfalse;
	}
	if ( qvkBindImageMemory( vk.device, t->image, t->memory, 0 ) != VK_SUCCESS ) {
		uib_destroy_texture( t );
		return qfalse;
	}

	Com_Memset( &vi, 0, sizeof( vi ) );
	vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	vi.image = t->image;
	vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
	vi.format = format;
	vi.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	vi.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	vi.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	vi.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	vi.subresourceRange.levelCount = 1;
	vi.subresourceRange.layerCount = 1;
	if ( qvkCreateImageView( vk.device, &vi, NULL, &t->view ) != VK_SUCCESS ) {
		uib_destroy_texture( t );
		return qfalse;
	}

	if ( name ) {
		SET_OBJECT_NAME( t->image, name, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( t->view, name, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	}
	return qtrue;
}

static qboolean uib_alloc_descriptor( uibTexture_t *t ) {
	VkDescriptorSetAllocateInfo alloc;
	VkDescriptorImageInfo info;
	VkWriteDescriptorSet w;

	Com_Memset( &alloc, 0, sizeof( alloc ) );
	alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc.descriptorPool = uib.descPool;
	alloc.descriptorSetCount = 1;
	alloc.pSetLayouts = &vk.set_layout_sampler;
	if ( qvkAllocateDescriptorSets( vk.device, &alloc, &t->descriptor ) != VK_SUCCESS ) {
		return qfalse;
	}

	Com_Memset( &info, 0, sizeof( info ) );
	info.sampler = uib.sampler;
	info.imageView = t->view;
	info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	Com_Memset( &w, 0, sizeof( w ) );
	w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	w.dstSet = t->descriptor;
	w.dstBinding = 0;
	w.descriptorCount = 1;
	w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	w.pImageInfo = &info;
	qvkUpdateDescriptorSets( vk.device, 1, &w, 0, NULL );
	return qtrue;
}

/* ---- render passes --------------------------------------------------------- */

static VkRenderPass uib_create_transient_pass( VkAttachmentLoadOp loadOp ) {
	VkAttachmentDescription att;
	VkAttachmentReference ref;
	VkSubpassDescription sub;
	VkSubpassDependency deps[2];
	VkRenderPassCreateInfo ci;
	VkRenderPass rp = VK_NULL_HANDLE;

	Com_Memset( &att, 0, sizeof( att ) );
	att.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	att.samples = VK_SAMPLE_COUNT_1_BIT;
	att.loadOp = loadOp;
	att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	att.initialLayout = ( loadOp == VK_ATTACHMENT_LOAD_OP_LOAD ) ?
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
	att.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	ref.attachment = 0;
	ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	Com_Memset( &sub, 0, sizeof( sub ) );
	sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	sub.colorAttachmentCount = 1;
	sub.pColorAttachments = &ref;

	/* Make the previous pass's write visible to this pass's sampling, and this
	 * pass's write visible to the next sampler. */
	Com_Memset( deps, 0, sizeof( deps ) );
	deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	deps[0].dstSubpass = 0;
	deps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	deps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
	deps[1].srcSubpass = 0;
	deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	deps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	deps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	Com_Memset( &ci, 0, sizeof( ci ) );
	ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	ci.attachmentCount = 1;
	ci.pAttachments = &att;
	ci.subpassCount = 1;
	ci.pSubpasses = &sub;
	ci.dependencyCount = 2;
	ci.pDependencies = deps;
	if ( qvkCreateRenderPass( vk.device, &ci, NULL, &rp ) != VK_SUCCESS ) {
		return VK_NULL_HANDLE;
	}
	return rp;
}

/* ---- pipelines ------------------------------------------------------------- */

static VkPipeline uib_create_pipeline( VkShaderModule fs, VkRenderPass rp, qboolean alphaBlend ) {
	VkPipelineShaderStageCreateInfo stages[2];
	VkPipelineVertexInputStateCreateInfo vin;
	VkPipelineInputAssemblyStateCreateInfo ia;
	VkPipelineViewportStateCreateInfo vp;
	VkPipelineRasterizationStateCreateInfo rs;
	VkPipelineMultisampleStateCreateInfo ms;
	VkPipelineColorBlendAttachmentState ba;
	VkPipelineColorBlendStateCreateInfo cb;
	VkPipelineDynamicStateCreateInfo dyn;
	VkDynamicState dynStates[2];
	VkGraphicsPipelineCreateInfo ci;
	VkPipeline pipe = VK_NULL_HANDLE;

	Com_Memset( stages, 0, sizeof( stages ) );
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = vk.modules.gamma_vs;
	stages[0].pName = "main";
	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = fs;
	stages[1].pName = "main";

	Com_Memset( &vin, 0, sizeof( vin ) );
	vin.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	Com_Memset( &ia, 0, sizeof( ia ) );
	ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

	Com_Memset( &vp, 0, sizeof( vp ) );
	vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	vp.viewportCount = 1;
	vp.scissorCount = 1;

	Com_Memset( &rs, 0, sizeof( rs ) );
	rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.cullMode = VK_CULL_MODE_NONE;
	rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rs.lineWidth = 1.0f;

	Com_Memset( &ms, 0, sizeof( ms ) );
	ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	Com_Memset( &ba, 0, sizeof( ba ) );
	ba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	if ( alphaBlend ) {
		ba.blendEnable = VK_TRUE;
		ba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		ba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		ba.colorBlendOp = VK_BLEND_OP_ADD;
		ba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		ba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		ba.alphaBlendOp = VK_BLEND_OP_ADD;
	}

	Com_Memset( &cb, 0, sizeof( cb ) );
	cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	cb.attachmentCount = 1;
	cb.pAttachments = &ba;

	dynStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
	dynStates[1] = VK_DYNAMIC_STATE_SCISSOR;
	Com_Memset( &dyn, 0, sizeof( dyn ) );
	dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dyn.dynamicStateCount = 2;
	dyn.pDynamicStates = dynStates;

	Com_Memset( &ci, 0, sizeof( ci ) );
	ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	ci.stageCount = 2;
	ci.pStages = stages;
	ci.pVertexInputState = &vin;
	ci.pInputAssemblyState = &ia;
	ci.pViewportState = &vp;
	ci.pRasterizationState = &rs;
	ci.pMultisampleState = &ms;
	ci.pColorBlendState = &cb;
	ci.pDynamicState = &dyn;
	ci.layout = uib.pipeLayout;
	ci.renderPass = rp;
	ci.subpass = 0;
	if ( qvkCreateGraphicsPipelines( vk.device, vk.pipelineCache, 1, &ci, NULL, &pipe ) != VK_SUCCESS ) {
		return VK_NULL_HANDLE;
	}
	return pipe;
}

/* ---- pool build / teardown ------------------------------------------------- */

static int uib_downscale_shift( int quality ) {
	/* 1 = quarter res, 2 = half res, 3 = adaptive (half). */
	switch ( quality ) {
		case 1: return 2;
		case 2: return 1;
		case 3: return 1;
		default: return 1;
	}
}

static void uib_destroy_pool( void ) {
	int i, j;
	uib_destroy_texture( &uib.sceneCopy );
	for ( i = 0; i < UIB_POOL_LEVELS; i++ ) {
		for ( j = 0; j < 2; j++ ) {
			uib_destroy_texture( &uib.level[i][j] );
		}
	}
	uib.poolValid = qfalse;
}

static VkFramebuffer uib_create_level_framebuffer( uibTexture_t *t ) {
	VkFramebufferCreateInfo fbi;
	VkFramebuffer fb = VK_NULL_HANDLE;
	Com_Memset( &fbi, 0, sizeof( fbi ) );
	fbi.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fbi.renderPass = uib.transientPass;
	fbi.attachmentCount = 1;
	fbi.pAttachments = &t->view;
	fbi.width = t->width;
	fbi.height = t->height;
	fbi.layers = 1;
	if ( qvkCreateFramebuffer( vk.device, &fbi, NULL, &fb ) != VK_SUCCESS ) {
		return VK_NULL_HANDLE;
	}
	return fb;
}

static qboolean uib_build_pool( int quality ) {
	uint32_t baseW = vk.renderWidth;
	uint32_t baseH = vk.renderHeight;
	int shift = uib_downscale_shift( quality );
	uint32_t w0, h0;
	int i, j;
	VkImageUsageFlags levelUsage =
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	VkImageUsageFlags copyUsage =
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

	if ( baseW == 0 || baseH == 0 ) {
		return qfalse;
	}
	uib_destroy_pool();

	w0 = baseW >> shift; if ( w0 < 4 ) w0 = 4;
	h0 = baseH >> shift; if ( h0 < 4 ) h0 = 4;

	if ( !uib_create_texture( &uib.sceneCopy, w0, h0, vk.present_format.format, copyUsage, "ui blur scene copy" ) ) {
		uib_destroy_pool();
		return qfalse;
	}
	if ( !uib_alloc_descriptor( &uib.sceneCopy ) ) { uib_destroy_pool(); return qfalse; }

	for ( i = 0; i < UIB_POOL_LEVELS; i++ ) {
		uint32_t lw = w0 >> i; if ( lw < 2 ) lw = 2;
		uint32_t lh = h0 >> i; if ( lh < 2 ) lh = 2;
		for ( j = 0; j < 2; j++ ) {
			if ( !uib_create_texture( &uib.level[i][j], lw, lh, VK_FORMAT_R16G16B16A16_SFLOAT,
					levelUsage, va( "ui blur level %d/%d", i, j ) ) ) {
				uib_destroy_pool();
				return qfalse;
			}
			uib.level[i][j].framebuffer = uib_create_level_framebuffer( &uib.level[i][j] );
			if ( uib.level[i][j].framebuffer == VK_NULL_HANDLE ) { uib_destroy_pool(); return qfalse; }
			if ( !uib_alloc_descriptor( &uib.level[i][j] ) ) { uib_destroy_pool(); return qfalse; }
		}
	}

	uib.builtQuality = quality;
	uib.builtW = baseW;
	uib.builtH = baseH;
	uib.poolValid = qtrue;
	uib.layerCacheValid = qfalse; /* pool contents gone; invalidate layer cache */
	return qtrue;
}

/* ---- init / shutdown ------------------------------------------------------- */

static void uib_status_f( void );

void vk_ui_blur_register_cvars( void ) {
	ui_filterDebug = ri.Cvar_Get( "ui_filterDebug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( ui_filterDebug,
		"UI blur debug: 0 off, 1 layer bounds, 2 expanded blur bounds, 3 backdrop source, 4 blurred result, 5 clip mask, 6 transient residency" );
	ui_blurQuality = ri.Cvar_Get( "ui_blurQuality", "3", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( ui_blurQuality,
		"UI blur quality: 0 disabled, 1 quarter-res, 2 half-res, 3 adaptive" );
	ui_blurMaxRadius = ri.Cvar_Get( "ui_blurMaxRadius", "64", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( ui_blurMaxRadius, "Maximum UI blur radius (virtual 640x480 px)" );
	ui_blurDownsampleThreshold = ri.Cvar_Get( "ui_blurDownsampleThreshold", "12", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( ui_blurDownsampleThreshold,
		"Radius (virtual px) at/above which UI blur switches from separable Gaussian to dual-Kawase" );
	ui_blurCache = ri.Cvar_Get( "ui_blurCache", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( ui_blurCache, "Cache the shared backdrop pyramid across static frames" );
	/* Latched by init/shutdown; client uses this for translucent fallback. */
	ri.Cvar_Get( "ui_blurReady", "0", CVAR_ROM );

	ri.Cmd_AddCommand( "ui_blur_status", uib_status_f );
}

void vk_ui_blur_init( void ) {
	VkPushConstantRange pcr;
	VkPipelineLayoutCreateInfo plci;
	VkDescriptorPoolSize psz;
	VkDescriptorPoolCreateInfo dpci;
	Vk_Sampler_Def sd;
	int quality;

	if ( uib.initialized ) {
		vk_ui_blur_shutdown();
	}
	Com_Memset( &uib, 0, sizeof( uib ) );
	/* Client SCR_UIBackdropBlur checks this for translucent fallback. */
	ri.Cvar_Set( "ui_blurReady", "0" );

	if ( !ui_blurQuality ) {
		vk_ui_blur_register_cvars();
	}
	quality = ui_blurQuality ? ui_blurQuality->integer : 3;
	if ( quality <= 0 || !vk.swapchainTransferSrc ) {
		/* Disabled or swapchain can't be copied: leave uninitialized (fallback). */
		uib.initialized = qtrue;
		uib.poolValid = qfalse;
		if ( quality > 0 && !vk.swapchainTransferSrc ) {
			ri.Printf( PRINT_ALL, S_COLOR_YELLOW "[VK][ui-blur] disabled: swapchain lacks TRANSFER_SRC\n" );
		}
		return;
	}

	/* sampler: linear, clamp to edge (no wraparound bleed at panel edges). */
	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.max_lod_1_0 = qtrue;
	sd.noAnisotropy = qtrue;
	uib.sampler = vk_find_sampler( &sd );

	/* pipeline layout: set0 combined sampler + fragment push constants */
	Com_Memset( &pcr, 0, sizeof( pcr ) );
	pcr.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	pcr.offset = 0;
	pcr.size = sizeof( uibPush_t );
	Com_Memset( &plci, 0, sizeof( plci ) );
	plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &vk.set_layout_sampler;
	plci.pushConstantRangeCount = 1;
	plci.pPushConstantRanges = &pcr;
	if ( qvkCreatePipelineLayout( vk.device, &plci, NULL, &uib.pipeLayout ) != VK_SUCCESS ) {
		ri.Printf( PRINT_ALL, S_COLOR_YELLOW "[VK][ui-blur] pipeline layout failed; blur disabled\n" );
		uib.initialized = qtrue;
		return;
	}
	SET_OBJECT_NAME( uib.pipeLayout, "pipeline layout - ui blur", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );

	/* descriptor pool: sceneCopy + level pairs, one combined sampler each. */
	Com_Memset( &psz, 0, sizeof( psz ) );
	psz.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	psz.descriptorCount = 1 + UIB_POOL_LEVELS * 2 + 2;
	Com_Memset( &dpci, 0, sizeof( dpci ) );
	dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	dpci.maxSets = psz.descriptorCount;
	dpci.poolSizeCount = 1;
	dpci.pPoolSizes = &psz;
	if ( qvkCreateDescriptorPool( vk.device, &dpci, NULL, &uib.descPool ) != VK_SUCCESS ) {
		ri.Printf( PRINT_ALL, S_COLOR_YELLOW "[VK][ui-blur] descriptor pool failed; blur disabled\n" );
		uib.initialized = qtrue;
		return;
	}

	uib.transientPass = uib_create_transient_pass( VK_ATTACHMENT_LOAD_OP_DONT_CARE );
	uib.transientClearPass = uib_create_transient_pass( VK_ATTACHMENT_LOAD_OP_CLEAR );
	if ( uib.transientPass == VK_NULL_HANDLE || uib.transientClearPass == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_ALL, S_COLOR_YELLOW "[VK][ui-blur] transient render pass failed; blur disabled\n" );
		uib.initialized = qtrue;
		return;
	}

	uib.pipeSample = uib_create_pipeline( vk.modules.ui_blur_sample_fs, uib.transientPass, qfalse );
	uib.pipeGauss = uib_create_pipeline( vk.modules.ui_blur_gauss_fs, uib.transientPass, qfalse );
	uib.pipeDown = uib_create_pipeline( vk.modules.ui_blur_down_fs, uib.transientPass, qfalse );
	uib.pipeUp = uib_create_pipeline( vk.modules.ui_blur_up_fs, uib.transientPass, qfalse );
	uib.pipeComposite = uib_create_pipeline( vk.modules.ui_blur_composite_fs, vk.render_pass.overlay_compose, qtrue );
	if ( !uib.pipeSample || !uib.pipeGauss || !uib.pipeDown || !uib.pipeUp || !uib.pipeComposite ) {
		ri.Printf( PRINT_ALL, S_COLOR_YELLOW "[VK][ui-blur] pipeline creation failed; blur disabled\n" );
		uib.initialized = qtrue;
		return;
	}

	if ( !uib_build_pool( quality ) ) {
		ri.Printf( PRINT_ALL, S_COLOR_YELLOW "[VK][ui-blur] transient pool allocation failed; blur disabled\n" );
		uib.initialized = qtrue;
		return;
	}

	uib.initialized = qtrue;
	ri.Cvar_Set( "ui_blurReady", "1" );
	ri.Printf( PRINT_ALL, "[VK][ui-blur] CSS filter/backdrop-filter compositor ready (quality=%d, %ux%u pool)\n",
		quality, uib.level[0][0].width, uib.level[0][0].height );
}

void vk_ui_blur_shutdown( void ) {
	ri.Cvar_Set( "ui_blurReady", "0" );
	if ( !uib.initialized ) {
		return;
	}
	if ( vk.device != VK_NULL_HANDLE ) {
		vk_wait_idle();
		uib_destroy_pool();
		if ( uib.pipeSample ) qvkDestroyPipeline( vk.device, uib.pipeSample, NULL );
		if ( uib.pipeGauss ) qvkDestroyPipeline( vk.device, uib.pipeGauss, NULL );
		if ( uib.pipeDown ) qvkDestroyPipeline( vk.device, uib.pipeDown, NULL );
		if ( uib.pipeUp ) qvkDestroyPipeline( vk.device, uib.pipeUp, NULL );
		if ( uib.pipeComposite ) qvkDestroyPipeline( vk.device, uib.pipeComposite, NULL );
		if ( uib.transientPass ) qvkDestroyRenderPass( vk.device, uib.transientPass, NULL );
		if ( uib.transientClearPass ) qvkDestroyRenderPass( vk.device, uib.transientClearPass, NULL );
		if ( uib.pipeLayout ) qvkDestroyPipelineLayout( vk.device, uib.pipeLayout, NULL );
		if ( uib.descPool ) qvkDestroyDescriptorPool( vk.device, uib.descPool, NULL );
	}
	Com_Memset( &uib, 0, sizeof( uib ) );
}

qboolean vk_ui_blur_available( void ) {
	return uib.initialized && uib.poolValid &&
		( ui_blurQuality ? ui_blurQuality->integer > 0 : qfalse );
}

/* ---- per-frame queue ------------------------------------------------------- */

void vk_ui_blur_begin_frame( void ) {
	uib.numOps = 0;
}

void vk_ui_blur_enqueue_backdrop( const uiBackdropFilter_t *bf ) {
	uibOp_t *op;
	if ( !bf || uib.numOps >= UIB_MAX_OPS ) {
		return;
	}
	op = &uib.ops[uib.numOps++];
	op->kind = 0;
	op->backdrop = *bf;
}

void vk_ui_blur_enqueue_layer( const uiCompositorLayer_t *layer ) {
	uibOp_t *op;
	if ( !layer || uib.numOps >= UIB_MAX_OPS ) {
		return;
	}
	op = &uib.ops[uib.numOps++];
	op->kind = 1;
	op->layer = *layer;
}

qboolean vk_ui_blur_has_work( void ) {
	return uib.numOps > 0;
}

/* ---- execution helpers ----------------------------------------------------- */

static void uib_set_full_viewport_scissor( uint32_t w, uint32_t h ) {
	VkViewport vp;
	VkRect2D sc;
	vp.x = 0.0f; vp.y = 0.0f;
	vp.width = (float)w; vp.height = (float)h;
	vp.minDepth = 0.0f; vp.maxDepth = 1.0f;
	sc.offset.x = 0; sc.offset.y = 0;
	sc.extent.width = w; sc.extent.height = h;
	qvkCmdSetViewport( vk.cmd->command_buffer, 0, 1, &vp );
	qvkCmdSetScissor( vk.cmd->command_buffer, 0, 1, &sc );
}

static void uib_set_scissor_rect( uint32_t targetW, uint32_t targetH,
	const float rectUV[4], float expandPx )
{
	VkViewport vp;
	VkRect2D sc;
	float x0 = rectUV[0] * (float)targetW - expandPx;
	float y0 = rectUV[1] * (float)targetH - expandPx;
	float x1 = rectUV[2] * (float)targetW + expandPx;
	float y1 = rectUV[3] * (float)targetH + expandPx;
	int ix0 = (int)floorf( x0 ), iy0 = (int)floorf( y0 );
	int ix1 = (int)ceilf( x1 ), iy1 = (int)ceilf( y1 );
	if ( ix0 < 0 ) ix0 = 0;
	if ( iy0 < 0 ) iy0 = 0;
	if ( ix1 > (int)targetW ) ix1 = (int)targetW;
	if ( iy1 > (int)targetH ) iy1 = (int)targetH;
	if ( ix1 <= ix0 ) ix1 = ix0 + 1;
	if ( iy1 <= iy0 ) iy1 = iy0 + 1;

	vp.x = 0.0f; vp.y = 0.0f;
	vp.width = (float)targetW; vp.height = (float)targetH;
	vp.minDepth = 0.0f; vp.maxDepth = 1.0f;
	sc.offset.x = ix0; sc.offset.y = iy0;
	sc.extent.width = (uint32_t)( ix1 - ix0 );
	sc.extent.height = (uint32_t)( iy1 - iy0 );
	qvkCmdSetViewport( vk.cmd->command_buffer, 0, 1, &vp );
	qvkCmdSetScissor( vk.cmd->command_buffer, 0, 1, &sc );
}

static void uib_begin_pass( VkRenderPass rp, VkFramebuffer fb, uint32_t w, uint32_t h, qboolean clear ) {
	VkRenderPassBeginInfo bi;
	VkClearValue cv;
	Com_Memset( &cv, 0, sizeof( cv ) );
	Com_Memset( &bi, 0, sizeof( bi ) );
	bi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	bi.renderPass = rp;
	bi.framebuffer = fb;
	bi.renderArea.extent.width = w;
	bi.renderArea.extent.height = h;
	bi.clearValueCount = clear ? 1 : 0;
	bi.pClearValues = clear ? &cv : NULL;
	qvkCmdBeginRenderPass( vk.cmd->command_buffer, &bi, VK_SUBPASS_CONTENTS_INLINE );
}

static void uib_draw_quad( void ) {
	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
}

static void uib_push( const uibPush_t *p ) {
	qvkCmdPushConstants( vk.cmd->command_buffer, uib.pipeLayout,
		VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( *p ), p );
}

static void uib_identity_uv( uibPush_t *p ) {
	p->uvScaleOffset[0] = 1.0f; p->uvScaleOffset[1] = 1.0f;
	p->uvScaleOffset[2] = 0.0f; p->uvScaleOffset[3] = 0.0f;
}

/* Quantize a virtual-px radius into buckets to limit permutations. */
static float uib_quantize_radius( float virtualRadius ) {
	float maxr = ui_blurMaxRadius ? ui_blurMaxRadius->value : 64.0f;
	float r = virtualRadius;
	if ( r < 0.0f ) r = 0.0f;
	if ( r > maxr ) r = maxr;
	/* 4px buckets */
	r = floorf( r / 4.0f + 0.5f ) * 4.0f;
	return r;
}

/* Convert virtual (640x480) radius to level0 texel step for the gaussian. */
static int uib_gauss_taps_for_radius( float virtualRadius, uint32_t levelW ) {
	/* virtual px -> render px scale via 640 virtual width; level is downscaled. */
	float renderPx = virtualRadius * ( (float)vk.renderWidth / 640.0f );
	float levelPx = renderPx * ( (float)levelW / (float)vk.renderWidth );
	int taps = (int)( levelPx + 0.5f );
	if ( taps < 1 ) taps = 1;
	if ( taps > UIB_MAX_GAUSS_TAPS ) taps = UIB_MAX_GAUSS_TAPS;
	return taps;
}

/*
 * Build the shared full-screen blurred backdrop from the tonemapped swapchain.
 * Result lands in *outTex. Returns qfalse on failure.
 */
static qboolean uib_build_backdrop( float quantRadius, uibTexture_t **outTex ) {
	VkImage swap = vk.swapchain_images[ vk.cmd->swapchain_image_index ];
	VkImageBlit blit;
	uibPush_t p;
	qboolean srgb = vk_format_is_srgb( vk.present_format.format );
	float threshold = ui_blurDownsampleThreshold ? ui_blurDownsampleThreshold->value : 12.0f;
	uibTexture_t *l0a = &uib.level[0][0];
	uibTexture_t *l0b = &uib.level[0][1];

	uib_label_begin( "UI backdrop pyramid", 0.2f, 0.5f, 1.0f );

	/* 1) copy tonemapped swapchain -> sceneCopy (display space). */
	uib_barrier( swap, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT );
	uib_barrier( uib.sceneCopy.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		0, VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT );

	Com_Memset( &blit, 0, sizeof( blit ) );
	blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blit.srcSubresource.layerCount = 1;
	blit.srcOffsets[1].x = (int32_t)vk.renderWidth;
	blit.srcOffsets[1].y = (int32_t)vk.renderHeight;
	blit.srcOffsets[1].z = 1;
	blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blit.dstSubresource.layerCount = 1;
	blit.dstOffsets[1].x = (int32_t)uib.sceneCopy.width;
	blit.dstOffsets[1].y = (int32_t)uib.sceneCopy.height;
	blit.dstOffsets[1].z = 1;
	qvkCmdBlitImage( vk.cmd->command_buffer,
		swap, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		uib.sceneCopy.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &blit, VK_FILTER_LINEAR );

	/* restore swapchain to PRESENT_SRC for the later overlay compose pass. */
	uib_barrier( swap, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT );
	uib_barrier( uib.sceneCopy.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
	uib.sceneCopy.resident = qtrue;

	/* 2) linearize sceneCopy -> level0[a]. */
	Com_Memset( &p, 0, sizeof( p ) );
	uib_identity_uv( &p );
	p.params[3] = srgb ? 0.0f : (float)UIB_FLAG_DECODE_SRGB; /* sampler auto-decodes sRGB views */
	uib_begin_pass( uib.transientPass, l0a->framebuffer, l0a->width, l0a->height, qfalse );
	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, uib.pipeSample );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		uib.pipeLayout, 0, 1, &uib.sceneCopy.descriptor, 0, NULL );
	uib_set_full_viewport_scissor( l0a->width, l0a->height );
	uib_push( &p );
	uib_draw_quad();
	qvkCmdEndRenderPass( vk.cmd->command_buffer );
	l0a->resident = qtrue;

	if ( quantRadius < threshold ) {
		/* separable gaussian (small radius) at level0: H then V */
		int taps = uib_gauss_taps_for_radius( quantRadius, l0a->width );

		/* H: l0a -> l0b */
		Com_Memset( &p, 0, sizeof( p ) );
		uib_identity_uv( &p );
		p.texelDir[2] = 1.0f / (float)l0a->width; p.texelDir[3] = 0.0f;
		p.misc[3] = (float)taps;
		uib_begin_pass( uib.transientPass, l0b->framebuffer, l0b->width, l0b->height, qfalse );
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, uib.pipeGauss );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			uib.pipeLayout, 0, 1, &l0a->descriptor, 0, NULL );
		uib_set_full_viewport_scissor( l0b->width, l0b->height );
		uib_push( &p );
		uib_draw_quad();
		qvkCmdEndRenderPass( vk.cmd->command_buffer );
		l0b->resident = qtrue;

		/* V: l0b -> l0a */
		Com_Memset( &p, 0, sizeof( p ) );
		uib_identity_uv( &p );
		p.texelDir[2] = 0.0f; p.texelDir[3] = 1.0f / (float)l0b->height;
		p.misc[3] = (float)taps;
		uib_begin_pass( uib.transientPass, l0a->framebuffer, l0a->width, l0a->height, qfalse );
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, uib.pipeGauss );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			uib.pipeLayout, 0, 1, &l0b->descriptor, 0, NULL );
		uib_set_full_viewport_scissor( l0a->width, l0a->height );
		uib_push( &p );
		uib_draw_quad();
		qvkCmdEndRenderPass( vk.cmd->command_buffer );

		*outTex = l0a;
	} else {
		/* dual-Kawase downsample chain l0->l1->l2 then upsample back to l0. */
		int i;
		for ( i = 1; i < UIB_POOL_LEVELS; i++ ) {
			uibTexture_t *src = &uib.level[i-1][0];
			uibTexture_t *dst = &uib.level[i][0];
			Com_Memset( &p, 0, sizeof( p ) );
			uib_identity_uv( &p );
			p.texelDir[0] = 1.0f / (float)src->width;
			p.texelDir[1] = 1.0f / (float)src->height;
			uib_begin_pass( uib.transientPass, dst->framebuffer, dst->width, dst->height, qfalse );
			qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, uib.pipeDown );
			qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
				uib.pipeLayout, 0, 1, &src->descriptor, 0, NULL );
			uib_set_full_viewport_scissor( dst->width, dst->height );
			uib_push( &p );
			uib_draw_quad();
			qvkCmdEndRenderPass( vk.cmd->command_buffer );
			dst->resident = qtrue;
		}
		for ( i = UIB_POOL_LEVELS - 1; i > 0; i-- ) {
			/* source is the previous (smaller) upsample result: the deepest level
			 * still lives in [i][0]; shallower steps read the [i][1] pong we just
			 * wrote, walking the chain up to [0][1] without mutating the pool. */
			uibTexture_t *src = ( i == UIB_POOL_LEVELS - 1 ) ? &uib.level[i][0] : &uib.level[i][1];
			uibTexture_t *dst = &uib.level[i-1][1];
			Com_Memset( &p, 0, sizeof( p ) );
			uib_identity_uv( &p );
			p.texelDir[0] = 1.0f / (float)src->width;
			p.texelDir[1] = 1.0f / (float)src->height;
			uib_begin_pass( uib.transientPass, dst->framebuffer, dst->width, dst->height, qfalse );
			qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, uib.pipeUp );
			qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
				uib.pipeLayout, 0, 1, &src->descriptor, 0, NULL );
			uib_set_full_viewport_scissor( dst->width, dst->height );
			uib_push( &p );
			uib_draw_quad();
			qvkCmdEndRenderPass( vk.cmd->command_buffer );
			dst->resident = qtrue;
		}
		*outTex = &uib.level[0][1];
	}

	uib_label_end();
	return qtrue;
}

static void uib_composite( uibTexture_t *blurred, const float rectUV[4],
	float cornerRadiusNorm, float rotation, float opacity, const float tint[4],
	qboolean encodeSrgb, qboolean tintEnabled, qboolean contentAlpha )
{
	uibPush_t p;
	uint32_t tw = vk.renderWidth;
	uint32_t th = vk.renderHeight;
	int debugMode = ui_filterDebug ? ui_filterDebug->integer : 0;
	float minWH;
	int flags = UIB_FLAG_APPLY_MASK;

	Com_Memset( &p, 0, sizeof( p ) );
	uib_identity_uv( &p );
	p.rect[0] = rectUV[0]; p.rect[1] = rectUV[1];
	p.rect[2] = rectUV[2]; p.rect[3] = rectUV[3];

	minWH = ( ( rectUV[2] - rectUV[0] ) * (float)tw < ( rectUV[3] - rectUV[1] ) * (float)th )
		? ( rectUV[2] - rectUV[0] ) * (float)tw
		: ( rectUV[3] - rectUV[1] ) * (float)th;
	p.params[0] = cornerRadiusNorm * minWH; /* corner radius in target px */
	p.params[1] = rotation;
	p.params[2] = opacity;
	if ( encodeSrgb ) flags |= UIB_FLAG_ENCODE_SRGB;
	if ( tintEnabled ) flags |= UIB_FLAG_APPLY_TINT;
	p.params[3] = (float)flags;
	if ( tint ) { p.tint[0]=tint[0]; p.tint[1]=tint[1]; p.tint[2]=tint[2]; p.tint[3]=tint[3]; }
	p.misc[0] = (float)debugMode;
	p.misc[1] = (float)tw;
	p.misc[2] = (float)th;
	(void)contentAlpha;

	uib_label_begin( "UI blur composite", 0.9f, 0.6f, 0.1f );
	uib_begin_pass( vk.render_pass.overlay_compose,
		vk.framebuffers.overlay_compose[ vk.cmd->swapchain_image_index ], tw, th, qfalse );
	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, uib.pipeComposite );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		uib.pipeLayout, 0, 1, &blurred->descriptor, 0, NULL );
	/* expand scissor so mask AA feathering isn't clipped */
	uib_set_scissor_rect( tw, th, rectUV, 2.0f );
	uib_push( &p );
	uib_draw_quad();
	qvkCmdEndRenderPass( vk.cmd->command_buffer );
	uib_label_end();
}

/* Render a filter-layer image into level0 then blur it; result in *outTex. */
static qboolean uib_build_layer( const uiCompositorLayer_t *layer, float quantRadius, uibTexture_t **outTex ) {
	shader_t *sh = R_GetShaderByHandle( layer->shader );
	image_t *img = NULL;
	uibTexture_t *l0a = &uib.level[0][0];
	uibTexture_t *l0b = &uib.level[0][1];
	uibPush_t p;
	float threshold = ui_blurDownsampleThreshold ? ui_blurDownsampleThreshold->value : 12.0f;
	float rw = layer->rect[2] - layer->rect[0];
	float rh = layer->rect[3] - layer->rect[1];

	if ( sh && sh->stages[0] && sh->stages[0]->bundle[0].image[0] ) {
		img = sh->stages[0]->bundle[0].image[0];
	}
	if ( !img || img->descriptor == VK_NULL_HANDLE || rw <= 0.0f || rh <= 0.0f ) {
		return qfalse;
	}

	uib_label_begin( "UI filter layer", 0.6f, 0.2f, 0.9f );

	/* draw image into level0, mapped to its screen rect; clear the rest. */
	Com_Memset( &p, 0, sizeof( p ) );
	/* target uv -> image uv: img = (uv - rectMin)/rectSize */
	p.uvScaleOffset[0] = 1.0f / rw;
	p.uvScaleOffset[1] = 1.0f / rh;
	p.uvScaleOffset[2] = -layer->rect[0] / rw;
	p.uvScaleOffset[3] = -layer->rect[1] / rh;
	p.params[3] = (float)UIB_FLAG_DECODE_SRGB; /* game textures are display-encoded */
	uib_begin_pass( uib.transientClearPass, l0a->framebuffer, l0a->width, l0a->height, qtrue );
	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, uib.pipeSample );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		uib.pipeLayout, 0, 1, &img->descriptor, 0, NULL );
	uib_set_scissor_rect( l0a->width, l0a->height, layer->rect, 0.0f );
	uib_push( &p );
	uib_draw_quad();
	qvkCmdEndRenderPass( vk.cmd->command_buffer );
	l0a->resident = qtrue;

	/* blur (always separable gaussian here; layers are element-sized). */
	{
		int taps = uib_gauss_taps_for_radius( quantRadius < 1.0f ? 1.0f : quantRadius, l0a->width );
		if ( quantRadius >= threshold && taps < UIB_MAX_GAUSS_TAPS ) {
			taps = UIB_MAX_GAUSS_TAPS;
		}
		/* H: l0a -> l0b */
		Com_Memset( &p, 0, sizeof( p ) );
		uib_identity_uv( &p );
		p.texelDir[2] = 1.0f / (float)l0a->width;
		p.misc[3] = (float)taps;
		uib_begin_pass( uib.transientPass, l0b->framebuffer, l0b->width, l0b->height, qfalse );
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, uib.pipeGauss );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			uib.pipeLayout, 0, 1, &l0a->descriptor, 0, NULL );
		uib_set_full_viewport_scissor( l0b->width, l0b->height );
		uib_push( &p );
		uib_draw_quad();
		qvkCmdEndRenderPass( vk.cmd->command_buffer );

		/* V: l0b -> l0a */
		Com_Memset( &p, 0, sizeof( p ) );
		uib_identity_uv( &p );
		p.texelDir[3] = 1.0f / (float)l0b->height;
		p.misc[3] = (float)taps;
		uib_begin_pass( uib.transientPass, l0a->framebuffer, l0a->width, l0a->height, qfalse );
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, uib.pipeGauss );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			uib.pipeLayout, 0, 1, &l0b->descriptor, 0, NULL );
		uib_set_full_viewport_scissor( l0a->width, l0a->height );
		uib_push( &p );
		uib_draw_quad();
		qvkCmdEndRenderPass( vk.cmd->command_buffer );
	}

	uib_label_end();
	*outTex = l0a;
	return qtrue;
}

/* FNV-1a hash of a filter-layer op for the static layer cache. */
static uint32_t uib_layer_cache_key( const uiCompositorLayer_t *layer, float quantRadius ) {
	const unsigned char *b;
	uint32_t h = 2166136261u;
	size_t k;
	struct {
		qhandle_t shader;
		float rect[4];
		float radius;
		int quality;
		uint32_t w, h;
	} key;

	Com_Memset( &key, 0, sizeof( key ) );
	key.shader = layer->shader;
	key.rect[0] = layer->rect[0]; key.rect[1] = layer->rect[1];
	key.rect[2] = layer->rect[2]; key.rect[3] = layer->rect[3];
	key.radius = quantRadius;
	key.quality = uib.builtQuality;
	key.w = uib.builtW; key.h = uib.builtH;

	b = (const unsigned char *)&key;
	for ( k = 0; k < sizeof( key ); k++ ) {
		h ^= b[k];
		h *= 16777619u;
	}
	return h ? h : 1u;
}

/*
 * ui_filterDebug 6: draw the resident transient textures as picture-in-picture
 * tiles along the top edge (transient texture residency visualization).
 */
static void uib_debug_residency( void ) {
	uibTexture_t *texes[1 + UIB_POOL_LEVELS * 2];
	int n = 0, i, j, t;
	float tileW = 0.16f, tileH = 0.16f, pad = 0.01f;

	if ( uib.sceneCopy.resident ) texes[n++] = &uib.sceneCopy;
	for ( i = 0; i < UIB_POOL_LEVELS; i++ ) {
		for ( j = 0; j < 2; j++ ) {
			if ( uib.level[i][j].resident ) texes[n++] = &uib.level[i][j];
		}
	}
	if ( n == 0 ) {
		return;
	}

	uib_label_begin( "UI blur residency debug", 1.0f, 0.2f, 0.2f );
	for ( t = 0; t < n; t++ ) {
		uibPush_t p;
		float rect[4];
		rect[0] = pad + (float)t * ( tileW + pad );
		rect[1] = pad;
		rect[2] = rect[0] + tileW;
		rect[3] = rect[1] + tileH;
		if ( rect[2] > 1.0f ) break;

		Com_Memset( &p, 0, sizeof( p ) );
		/* map tile rect -> full texture uv */
		p.uvScaleOffset[0] = 1.0f / tileW;
		p.uvScaleOffset[1] = 1.0f / tileH;
		p.uvScaleOffset[2] = -rect[0] / tileW;
		p.uvScaleOffset[3] = -rect[1] / tileH;
		p.rect[0] = rect[0]; p.rect[1] = rect[1];
		p.rect[2] = rect[2]; p.rect[3] = rect[3];
		p.params[2] = 1.0f; /* opacity */
		/* Only manually encode when the swapchain is UNORM (sRGB HW encodes). */
		p.params[3] = vk_format_is_srgb( vk.present_format.format )
			? 0.0f : (float)UIB_FLAG_ENCODE_SRGB;
		p.misc[0] = 4.0f; /* debug mode 4: raw blurred result, ignore mask */
		p.misc[1] = (float)vk.renderWidth;
		p.misc[2] = (float)vk.renderHeight;

		uib_begin_pass( vk.render_pass.overlay_compose,
			vk.framebuffers.overlay_compose[ vk.cmd->swapchain_image_index ],
			vk.renderWidth, vk.renderHeight, qfalse );
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, uib.pipeComposite );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			uib.pipeLayout, 0, 1, &texes[t]->descriptor, 0, NULL );
		uib_set_scissor_rect( vk.renderWidth, vk.renderHeight, rect, 0.0f );
		uib_push( &p );
		uib_draw_quad();
		qvkCmdEndRenderPass( vk.cmd->command_buffer );
	}
	uib_label_end();
}

/* ---- main entry ------------------------------------------------------------ */

void vk_ui_blur_execute( VkImageView sceneSrc ) {
	int i;
	int startMs;
	float maxBackdropRadius = 0.0f;
	int backdrops = 0, layers = 0;
	qboolean haveBackdrop = qfalse;
	qboolean encodeSrgb;
	uibTexture_t *sharedBackdrop = NULL;

	(void)sceneSrc;

	if ( !vk_ui_blur_available() || uib.numOps == 0 ) {
		return;
	}
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ||
		vk.cmd->swapchain_image_index >= MAX_SWAPCHAIN_IMAGES ) {
		return;
	}
	if ( vk.inRenderPass ) {
		/* must be called between gamma and overlay compose (no open pass). */
		return;
	}
	/* Rebuild pool if quality or render extent changed (dynamic-res / UI scale). */
	if ( ui_blurQuality && ( ui_blurQuality->integer != uib.builtQuality ||
			vk.renderWidth != uib.builtW || vk.renderHeight != uib.builtH ) ) {
		if ( ui_blurQuality->integer <= 0 ) {
			return;
		}
		vk_wait_idle();
		if ( !uib_build_pool( ui_blurQuality->integer ) ) {
			return;
		}
	}

	startMs = ri.Milliseconds();
	uib_label_begin( "UI Filter/Backdrop Blur", 0.3f, 0.7f, 1.0f );

	/* clear residency for this frame */
	uib.sceneCopy.resident = qfalse;
	for ( i = 0; i < UIB_POOL_LEVELS; i++ ) {
		uib.level[i][0].resident = qfalse;
		uib.level[i][1].resident = qfalse;
	}

	/* Pass 1: backdrop panels reuse one shared blurred backdrop (merge/reuse). */
	for ( i = 0; i < uib.numOps; i++ ) {
		if ( uib.ops[i].kind == 0 ) {
			haveBackdrop = qtrue;
			if ( uib.ops[i].backdrop.radius > maxBackdropRadius ) {
				maxBackdropRadius = uib.ops[i].backdrop.radius;
			}
		}
	}
	uib.lastOpCount = 0;

	/* Swapchain color attachments are typically sRGB, so the hardware
	 * encodes linear → display on write. Only manually encode when the
	 * present format is UNORM (otherwise we double-encode → dark panels). */
	encodeSrgb = !vk_format_is_srgb( vk.present_format.format );

	if ( haveBackdrop ) {
		float q = uib_quantize_radius( maxBackdropRadius );
		/* the backdrop pyramid overwrites the layer cache's storage. */
		uib.layerCacheValid = qfalse;
		if ( uib_build_backdrop( q, &sharedBackdrop ) && sharedBackdrop ) {
			for ( i = 0; i < uib.numOps; i++ ) {
				const uiBackdropFilter_t *bf;
				qboolean tintOn;
				int opStart = ri.Milliseconds();
				if ( uib.ops[i].kind != 0 ) continue;
				bf = &uib.ops[i].backdrop;
				tintOn = ( bf->tint[3] > 0.0f );
				uib_composite( sharedBackdrop, bf->rect, bf->cornerRadius, bf->rotation,
					bf->opacity, bf->tint, encodeSrgb, tintOn, qfalse );
				backdrops++;
				if ( uib.lastOpCount < UIB_MAX_OPS ) {
					uib.lastOpMs[uib.lastOpCount++] = (float)( ri.Milliseconds() - opStart );
				}
			}
		}
	}

	/* Pass 2: filter-layer panels (each blurs its own content). A single
	 * static layer can reuse last frame's blurred result (ui_blurCache). */
	{
		int layerOps = 0;
		for ( i = 0; i < uib.numOps; i++ ) {
			if ( uib.ops[i].kind == 1 ) layerOps++;
		}
		for ( i = 0; i < uib.numOps; i++ ) {
			const uiCompositorLayer_t *layer;
			uibTexture_t *layerTex = NULL;
			float q;
			uint32_t key;
			qboolean cacheable, built = qfalse;
			int opStart = ri.Milliseconds();
			if ( uib.ops[i].kind != 1 ) continue;
			layer = &uib.ops[i].layer;
			q = uib_quantize_radius( layer->filter.numOps > 0 ? layer->filter.ops[0].radius : 8.0f );

			key = uib_layer_cache_key( layer, q );
			cacheable = ( ui_blurCache && ui_blurCache->integer &&
				layerOps == 1 && !haveBackdrop );
			if ( cacheable && uib.layerCacheValid && uib.layerCacheKey == key ) {
				layerTex = &uib.level[0][0]; /* still holds last frame's blur */
				layerTex->resident = qtrue;
				built = qtrue;
			} else if ( uib_build_layer( layer, q, &layerTex ) && layerTex ) {
				built = qtrue;
				uib.layerCacheKey = key;
				uib.layerCacheValid = cacheable;
			} else {
				uib.layerCacheValid = qfalse;
			}
			if ( built && layerTex ) {
				uib_composite( layerTex, layer->rect, layer->cornerRadius, layer->rotation,
					layer->opacity, NULL, encodeSrgb, qfalse /*tint*/, qtrue /*contentAlpha*/ );
				layers++;
				if ( uib.lastOpCount < UIB_MAX_OPS ) {
					uib.lastOpMs[uib.lastOpCount++] = (float)( ri.Milliseconds() - opStart );
				}
			}
		}
		if ( layerOps != 1 || haveBackdrop ) {
			uib.layerCacheValid = qfalse;
		}
	}

	if ( ui_filterDebug && ui_filterDebug->integer == 6 ) {
		uib_debug_residency();
	}

	uib_label_end();

	uib.lastBackdrops = backdrops;
	uib.lastLayers = layers;
	uib.lastCpuMs = (float)( ri.Milliseconds() - startMs );
}

/* ---- console --------------------------------------------------------------- */

static void uib_status_f( void ) {
	int i;
	ri.Printf( PRINT_ALL, "UI blur compositor:\n" );
	ri.Printf( PRINT_ALL, "  available       : %s\n", vk_ui_blur_available() ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "  quality          : %d\n", ui_blurQuality ? ui_blurQuality->integer : 0 );
	ri.Printf( PRINT_ALL, "  swapchainSrcCopy : %s\n", vk.swapchainTransferSrc ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "  pool             : %s (%ux%u base)\n",
		uib.poolValid ? "resident" : "none", uib.builtW, uib.builtH );
	if ( uib.poolValid ) {
		for ( i = 0; i < UIB_POOL_LEVELS; i++ ) {
			ri.Printf( PRINT_ALL, "    level %d        : %ux%u (ping/pong)\n",
				i, uib.level[i][0].width, uib.level[i][0].height );
		}
	}
	ri.Printf( PRINT_ALL, "  last frame       : %d backdrops, %d layers, %.2f ms cpu\n",
		uib.lastBackdrops, uib.lastLayers, uib.lastCpuMs );
	for ( i = 0; i < uib.lastOpCount; i++ ) {
		ri.Printf( PRINT_ALL, "    op %d record    : %.2f ms cpu\n", i, uib.lastOpMs[i] );
	}
	ri.Printf( PRINT_ALL, "  layer cache      : %s (key 0x%08x)\n",
		uib.layerCacheValid ? "valid" : "invalid", uib.layerCacheKey );
	ri.Printf( PRINT_ALL, "  debug mode       : %d\n", ui_filterDebug ? ui_filterDebug->integer : 0 );
}
