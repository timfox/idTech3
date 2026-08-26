/*
===========================================================================
Vulkan render target images, pooled attachment memory, depth views, shadow
atlases, froxel/fluid volumes, and teardown (split from vk.c).
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_sun_csm.h"
#include "vk_postfx.h"
#include "vk_cmd.h"
#include "vk_image_layout.h"
#include "vk_scene_pass.h"
#include "vk_volumetric_params.h"
#include "vk_temporal.h"
#include "vk_util.h"
#include "vk_attachments.h"
#include "vk_upscale.h"
#include "vk_deferred_gbuffer.h"
#include "vk_pass_registry.h"
#include "tr_render_mode_vk.h"
#include "vk_hdr_resolve_contract.h"

static void vk_create_fog_noise_texture( void );
static void vk_destroy_sun_shadow_resources( void );
static void vk_create_sun_shadow_resources( void );
static void vk_destroy_local_shadow_resources( void );
static void vk_create_local_shadow_resources( void );
static void vk_destroy_froxel_images( void );
static void vk_create_froxel_images( void );

static void vk_destroy_image_and_view( VkImage *image, VkImageView *view )
{
	if ( view && *view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, *view, NULL );
		*view = VK_NULL_HANDLE;
	}
	if ( image && *image != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, *image, NULL );
		*image = VK_NULL_HANDLE;
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


static qboolean vk_add_attachment_desc_soft( VkImage desc, VkImageView *image_view, VkImageUsageFlags usage, VkMemoryRequirements *reqs, VkFormat image_format, VkImageAspectFlags aspect_flags, VkImageLayout image_layout
	, VkImageViewType view_type )
{
	if ( num_attachments >= ARRAY_LEN( attachments ) ) {
		return qfalse;
	}
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
	return qtrue;
}

static void vk_add_attachment_desc( VkImage desc, VkImageView *image_view, VkImageUsageFlags usage, VkMemoryRequirements *reqs, VkFormat image_format, VkImageAspectFlags aspect_flags, VkImageLayout image_layout
	, VkImageViewType view_type )
{
	if ( !vk_add_attachment_desc_soft( desc, image_view, usage, reqs, image_format, aspect_flags, image_layout
			, view_type
			) ) {
		ri.Error( ERR_FATAL, "Attachments array overflow (%u/%u)",
				(unsigned)num_attachments, (unsigned)ARRAY_LEN( attachments ) );
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

    VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D;

	if ( flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT )
		view_type = VK_IMAGE_VIEW_TYPE_CUBE;
	vk_add_attachment_desc( *image, image_view, usage, &memory_requirements, format, VK_IMAGE_ASPECT_COLOR_BIT, image_layout, view_type );
}

/*
 * Soft-fail CreateImage for experimental G-buffer / visibility scaffolds.
 * On failure leaves *image as VK_NULL_HANDLE and does not enqueue attachment memory.
 */
static qboolean create_color_attachment_soft(
	uint32_t width, uint32_t height,
	VkSampleCountFlagBits samples, VkFormat format,
	VkImageUsageFlags usage, VkImage *image,
	VkImageView *image_view, VkImageLayout image_layout,
	qboolean multisample, VkImageCreateFlags flags )
{
	VkImageCreateInfo create_desc;
	VkMemoryRequirements memory_requirements;
	VkResult res;

	*image = VK_NULL_HANDLE;
	if ( multisample && !( usage & VK_IMAGE_USAGE_SAMPLED_BIT ) )
		usage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;

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
	res = qvkCreateImage( vk.device, &create_desc, NULL, image );
	if ( res != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
			"[VK][deferred] CreateImage soft-fail (%ux%u fmt=%d): %s\n" S_COLOR_WHITE,
			width, height, (int)format, vk_result_string( res ) );
		*image = VK_NULL_HANDLE;
		return qfalse;
	}

	vk_get_image_memory_erquirements( *image, &memory_requirements );

	{
		VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D;
		if ( flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT )
			view_type = VK_IMAGE_VIEW_TYPE_CUBE;
		if ( !vk_add_attachment_desc_soft( *image, image_view, usage, &memory_requirements, format, VK_IMAGE_ASPECT_COLOR_BIT, image_layout, view_type ) ) {
			qvkDestroyImage( vk.device, *image, NULL );
			*image = VK_NULL_HANDLE;
			ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
				"[VK][deferred] attachment pool full (%ux%u) — soft-fail\n" S_COLOR_WHITE,
				width, height );
			return qfalse;
		}
	}
	return qtrue;
}

static void vk_create_fullres_color_attachment(
	VkFormat format,
	VkImageUsageFlags usage,
	VkImage *image,
	VkImageView *image_view,
	VkImageLayout image_layout,
	qboolean allowTransient )
{
	uint32_t width = 0;
	uint32_t height = 0;

	vk_get_active_render_extent( &width, &height );
	create_color_attachment( width, height, VK_SAMPLE_COUNT_1_BIT, format, usage, image, image_view, image_layout, allowTransient, 0 );
}

static qboolean vk_create_fullres_color_attachment_soft(
	VkFormat format,
	VkImageUsageFlags usage,
	VkImage *image,
	VkImageView *image_view,
	VkImageLayout image_layout,
	qboolean allowTransient )
{
	uint32_t width = 0;
	uint32_t height = 0;

	vk_get_active_render_extent( &width, &height );
	return create_color_attachment_soft( width, height, VK_SAMPLE_COUNT_1_BIT, format, usage,
		image, image_view, image_layout, allowTransient, 0 );
}

static void create_depth_attachment( uint32_t width, uint32_t height, VkSampleCountFlagBits samples, VkImage *image, VkImageView *image_view, qboolean allowTransient )
{
	VkImageCreateInfo create_desc;
	VkMemoryRequirements memory_requirements;
	VkImageAspectFlags image_aspect_flags;
	const qboolean sampledDepth = qtrue;

	/* create depth image */
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

	vk_add_attachment_desc( *image, image_view, create_desc.usage, &memory_requirements, vk.depth_format, image_aspect_flags, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_VIEW_TYPE_2D );
}

/*
===============
vk_create_deferred_gbuffer_scaffold
===============
Allocates full-res G-buffer RTs when r_renderMode 1/2/3 and r_deferredGBuffer 1.
r_renderMode 2 uses these as a sidecar for temporal/advanced consumers while
Forward+ remains the primary lighting path. Mode 3 (Unified Clustered) uses
them for deferred opaque lighting.
*/
static void vk_create_deferred_gbuffer_scaffold( void )
{
	VkImageUsageFlags gbufUsage =
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
		VK_IMAGE_USAGE_STORAGE_BIT;
	cvar_t *failInject;

	vk.deferredGbufferAllocated = qfalse;
	vk.deferredGbufferDirectExport = qfalse;
	vk.deferredGbufferExtentW = 0;
	vk.deferredGbufferExtentH = 0;
	vk.deferred_gbuffer_albedo = VK_NULL_HANDLE;
	vk.deferred_gbuffer_albedo_view = VK_NULL_HANDLE;
	vk.deferred_gbuffer_normal = VK_NULL_HANDLE;
	vk.deferred_gbuffer_normal_view = VK_NULL_HANDLE;
	vk.deferred_gbuffer_material = VK_NULL_HANDLE;
	vk.deferred_gbuffer_material_view = VK_NULL_HANDLE;
	vk.deferred_gbuffer_surface = VK_NULL_HANDLE;
	vk.deferred_gbuffer_surface_view = VK_NULL_HANDLE;
	vk.deferred_lighting_image = VK_NULL_HANDLE;
	vk.deferred_lighting_view = VK_NULL_HANDLE;
	vk.deferred_class_stub = VK_NULL_HANDLE;
	vk.deferred_class_stub_view = VK_NULL_HANDLE;

	if ( !vk_deferred_gbuffer_resources_wanted() ) {
		return;
	}

	failInject = ri.Cvar_Get( "r_dgbFailInject", "0", CVAR_TEMP | CVAR_CHEAT );
	if ( failInject && failInject->string &&
		( !Q_stricmp( failInject->string, "alloc" ) || !Q_stricmp( failInject->string, "all" ) ) ) {
		vk_deferred_gbuffer_set_fallback( "r_dgbFailInject=alloc" );
		if ( r_deferredGBufferFill ) {
			ri.Cvar_Set( "r_deferredGBufferFill", "0" );
		}
		if ( r_ssao && !r_ssao->integer ) {
			ri.Cvar_Set( "r_ssao", "1" );
		}
		if ( ri.Cvar_VariableIntegerValue( "r_ambientVisibilityMode" ) >= 2 ) {
			ri.Cvar_Set( "r_ambientVisibilityMode", "1" );
		}
		return;
	}

	if ( !vk_create_fullres_color_attachment_soft( vk.color_format, gbufUsage | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			&vk.deferred_gbuffer_albedo, &vk.deferred_gbuffer_albedo_view,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse ) ||
		!vk_create_fullres_color_attachment_soft( VK_FORMAT_R16G16B16A16_SFLOAT,
			gbufUsage | VK_IMAGE_USAGE_STORAGE_BIT,
			&vk.deferred_gbuffer_normal, &vk.deferred_gbuffer_normal_view,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse ) ||
		!vk_create_fullres_color_attachment_soft( VK_FORMAT_R16G16B16A16_SFLOAT,
			gbufUsage | VK_IMAGE_USAGE_STORAGE_BIT,
			&vk.deferred_gbuffer_material, &vk.deferred_gbuffer_material_view,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse ) ||
		!vk_create_fullres_color_attachment_soft( VK_FORMAT_R16G16B16A16_SFLOAT,
			gbufUsage | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			&vk.deferred_gbuffer_surface, &vk.deferred_gbuffer_surface_view,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse ) ||
		!vk_create_fullres_color_attachment_soft( VK_FORMAT_R16G16B16A16_SFLOAT,
			gbufUsage | VK_IMAGE_USAGE_STORAGE_BIT,
			&vk.deferred_lighting_image, &vk.deferred_lighting_view,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse ) ||
		!create_color_attachment_soft( 1, 1, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8_UINT,
			VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			&vk.deferred_class_stub, &vk.deferred_class_stub_view,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse, 0 ) ) {
		vk_deferred_gbuffer_set_fallback( "gbuffer_image_create_failed" );
		vk.deferredGbufferAllocated = qfalse;
		vk.deferredGbufferDirectExport = qfalse;
		return;
	}

	/* Image views are created later in vk_alloc_attachments() — only probe images here.
	 * Final success/soft-fallback runs in vk_finalize_deferred_gbuffer_scaffold(). */
	if ( vk.deferred_gbuffer_albedo == VK_NULL_HANDLE ||
		vk.deferred_gbuffer_normal == VK_NULL_HANDLE ||
		vk.deferred_gbuffer_material == VK_NULL_HANDLE ||
		vk.deferred_gbuffer_surface == VK_NULL_HANDLE ||
		vk.deferred_lighting_image == VK_NULL_HANDLE ) {
		vk_deferred_gbuffer_set_fallback( "gbuffer_image_create_failed" );
		vk.deferredGbufferAllocated = qfalse;
		vk.deferredGbufferDirectExport = qfalse;
		return;
	}

	vk.deferredGbufferAllocated = qtrue;
	vk.deferredGbufferDirectExport = vk.msaaActive ? qfalse : qtrue;
	/* PrimID/bary MRT companions for gbuf shaders (USE_VISIBILITY_EXPORT). Full visbuf
	 * feature (class/fill/debug) still requires r_visibilityBuffer; these images are
	 * created whenever deferred direct export is live so attachment counts match. */
	if ( vk.deferredGbufferDirectExport && vk.visibility_buffer_ids == VK_NULL_HANDLE ) {
		VkImageUsageFlags visUsage =
			VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		if ( !vk_create_fullres_color_attachment_soft( VK_FORMAT_R32G32_UINT, visUsage,
				&vk.visibility_buffer_ids, &vk.visibility_buffer_ids_view,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse ) ||
			!vk_create_fullres_color_attachment_soft( VK_FORMAT_R16G16_UNORM, visUsage,
				&vk.visibility_buffer_bary, &vk.visibility_buffer_bary_view,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse ) ) {
			/* Keep G-buffer; drop PrimID MRT companions and fall back to depth export. */
			vk.visibility_buffer_ids = VK_NULL_HANDLE;
			vk.visibility_buffer_ids_view = VK_NULL_HANDLE;
			vk.visibility_buffer_bary = VK_NULL_HANDLE;
			vk.visibility_buffer_bary_view = VK_NULL_HANDLE;
			vk.visibilityBufferDirectExport = qfalse;
			vk.deferredGbufferDirectExport = qfalse;
			ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
				"[VK][deferred] PrimID MRT soft-fail — using depth-fallback G-buffer export\n" S_COLOR_WHITE );
		} else {
			vk.visibilityBufferDirectExport = qtrue;
		}
	}
	ri.Printf( PRINT_ALL,
		"[VK][deferred] G-buffer scaffold: albedo + normal + material + surface(LM/owner) + motion + lighting + class stub%s%s\n",
		vk.deferredGbufferDirectExport ? " (direct material export)" : " (depth fallback export)",
		vk.visibilityBufferDirectExport ? " + PrimID MRT" : "" );
}

/*
===============
vk_finalize_deferred_gbuffer_scaffold
===============
After vk_alloc_attachments() binds memory and creates views, verify the G-buffer
is usable. Soft-disable fill (and restore legacy SSAO) on failure so Forward+ continues.
*/
static void vk_finalize_deferred_gbuffer_scaffold( void )
{
	uint32_t extentW = 0, extentH = 0;
	cvar_t *failInject;
	char debugName[96];

	if ( !vk_deferred_gbuffer_resources_wanted() ) {
		return;
	}
	if ( !vk.deferredGbufferAllocated ) {
		return;
	}

	failInject = ri.Cvar_Get( "r_dgbFailInject", "0", CVAR_TEMP | CVAR_CHEAT );
	if ( ( failInject && failInject->string &&
			( !Q_stricmp( failInject->string, "view" ) || !Q_stricmp( failInject->string, "all" ) ) ) ||
		vk.deferred_gbuffer_albedo_view == VK_NULL_HANDLE ||
		vk.deferred_gbuffer_normal_view == VK_NULL_HANDLE ||
		vk.deferred_gbuffer_material_view == VK_NULL_HANDLE ||
		vk.deferred_gbuffer_surface_view == VK_NULL_HANDLE ||
		vk.deferred_lighting_view == VK_NULL_HANDLE ) {
		vk_deferred_gbuffer_set_fallback(
			( failInject && failInject->string &&
			  ( !Q_stricmp( failInject->string, "view" ) || !Q_stricmp( failInject->string, "all" ) ) )
				? "r_dgbFailInject=view"
				: "gbuffer_alloc_failed" );
		if ( r_deferredGBufferFill ) {
			ri.Cvar_Set( "r_deferredGBufferFill", "0" );
		}
		/* Quality profile turns SSAO off for AV; restore a usable AO owner. */
		if ( r_ssao && !r_ssao->integer ) {
			ri.Cvar_Set( "r_ssao", "1" );
		}
		if ( ri.Cvar_VariableIntegerValue( "r_ambientVisibilityMode" ) >= 2 ) {
			ri.Cvar_Set( "r_ambientVisibilityMode", "1" );
		}
		vk.deferredGbufferAllocated = qfalse;
		vk.deferredGbufferDirectExport = qfalse;
		vk.deferredGbufferExtentW = 0;
		vk.deferredGbufferExtentH = 0;
		return;
	}

	vk_get_active_render_extent( &extentW, &extentH );
	if ( extentW == 0u || extentH == 0u ) {
		extentW = vk.mainColorWidth;
		extentH = vk.mainColorHeight;
	}
	vk.deferredGbufferGeneration++;
	if ( vk.deferredGbufferGeneration == 0u ) {
		vk.deferredGbufferGeneration = 1u;
	}
	vk.deferredGbufferExtentW = extentW;
	vk.deferredGbufferExtentH = extentH;
	vk_deferred_gbuffer_clear_fallback();
	vk_deferred_gbuffer_note_recreate( "scaffold_finalize" );
	Com_sprintf( debugName, sizeof( debugName ), "dgb albedo gen%u %ux%u",
		vk.deferredGbufferGeneration, extentW, extentH );
	SET_OBJECT_NAME( vk.deferred_gbuffer_albedo, debugName, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	Com_sprintf( debugName, sizeof( debugName ), "dgb normal gen%u %ux%u",
		vk.deferredGbufferGeneration, extentW, extentH );
	SET_OBJECT_NAME( vk.deferred_gbuffer_normal, debugName, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	Com_sprintf( debugName, sizeof( debugName ), "dgb material gen%u %ux%u",
		vk.deferredGbufferGeneration, extentW, extentH );
	SET_OBJECT_NAME( vk.deferred_gbuffer_material, debugName, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	ri.Printf( PRINT_ALL, "[VK][deferred] G-buffer ready gen=%u extent=%ux%u\n",
		vk.deferredGbufferGeneration, extentW, extentH );
}

/*
===============
vk_create_visibility_buffer_scaffold
===============
Allocates compact visibility RTs when r_renderMode 1/2/3 and r_visibilityBuffer 1.
IDs = R32G32_UINT (draw/prim), bary = R16G16_UNORM, class = R8_UINT.
*/
static void vk_create_visibility_buffer_scaffold( void )
{
	/* COLOR_ATTACHMENT for true PrimID MRT; STORAGE for depth-proxy compute fill. */
	VkImageUsageFlags usage =
		VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	VkImageUsageFlags classUsage =
		VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	qboolean ids_from_deferred = ( vk.visibility_buffer_ids != VK_NULL_HANDLE ) ? qtrue : qfalse;

	vk.visibilityBufferAllocated = qfalse;
	/* Keep PrimID MRT flag if deferred scaffold already allocated ids/bary. */
	if ( !ids_from_deferred ) {
		vk.visibilityBufferDirectExport = qfalse;
		vk.visibility_buffer_ids = VK_NULL_HANDLE;
		vk.visibility_buffer_ids_view = VK_NULL_HANDLE;
		vk.visibility_buffer_bary = VK_NULL_HANDLE;
		vk.visibility_buffer_bary_view = VK_NULL_HANDLE;
	}
	vk.visibility_buffer_class = VK_NULL_HANDLE;
	vk.visibility_buffer_class_view = VK_NULL_HANDLE;

	if ( !vk.fboActive || !r_renderMode ||
		!R_RenderMode_WantsGBuffer() ||
		!r_visibilityBuffer || !r_visibilityBuffer->integer ) {
		return;
	}

	if ( !ids_from_deferred ) {
		if ( !vk_create_fullres_color_attachment_soft( VK_FORMAT_R32G32_UINT, usage,
				&vk.visibility_buffer_ids, &vk.visibility_buffer_ids_view,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse ) ||
			!vk_create_fullres_color_attachment_soft( VK_FORMAT_R16G16_UNORM, usage,
				&vk.visibility_buffer_bary, &vk.visibility_buffer_bary_view,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse ) ) {
			ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
				"[VK][visbuf] visibility IDs/bary soft-fail — scaffold disabled\n" S_COLOR_WHITE );
			vk.visibility_buffer_ids = VK_NULL_HANDLE;
			vk.visibility_buffer_ids_view = VK_NULL_HANDLE;
			vk.visibility_buffer_bary = VK_NULL_HANDLE;
			vk.visibility_buffer_bary_view = VK_NULL_HANDLE;
			vk.visibilityBufferAllocated = qfalse;
			vk.visibilityBufferDirectExport = qfalse;
			return;
		}
	}
	if ( !vk_create_fullres_color_attachment_soft( VK_FORMAT_R8_UINT, classUsage,
			&vk.visibility_buffer_class, &vk.visibility_buffer_class_view,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse ) ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
			"[VK][visbuf] visibility class soft-fail — scaffold disabled\n" S_COLOR_WHITE );
		vk.visibility_buffer_class = VK_NULL_HANDLE;
		vk.visibility_buffer_class_view = VK_NULL_HANDLE;
		vk.visibilityBufferAllocated = qfalse;
		return;
	}
	vk.visibilityBufferAllocated = qtrue;
	vk.visibilityBufferDirectExport = ( vk.deferredGbufferDirectExport && !vk.msaaActive &&
		vk.visibility_buffer_ids != VK_NULL_HANDLE ) ? qtrue : qfalse;
	ri.Printf( PRINT_ALL,
		"[VK][visbuf] visibility scaffold: IDs R32G32_UINT + bary R16G16_UNORM + class R8_UINT%s\n",
		vk.visibilityBufferDirectExport ? " (PrimID MRT export)" : " (compute fill / MSAA fallback)" );
	if ( vk.visibilityBufferDirectExport ) {
		ri.Printf( PRINT_ALL, "[VK][visbuf] true gl_PrimitiveID / drawId MRT enabled\n" );
	}
}

static void vk_create_fullres_msaa_color_attachment(
	VkSampleCountFlagBits samples,
	VkFormat format,
	VkImageUsageFlags usage,
	VkImage *image,
	VkImageView *image_view,
	qboolean allowTransient )
{
	uint32_t width = 0;
	uint32_t height = 0;

	vk_get_active_render_extent( &width, &height );
	create_color_attachment( width, height, samples, format, usage, image, image_view, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, allowTransient, 0 );
}

static void vk_create_fullres_depth_attachment(
	VkSampleCountFlagBits samples,
	VkImage *image,
	VkImageView *image_view,
	qboolean allowTransient )
{
	uint32_t width = 0;
	uint32_t height = 0;

	vk_get_active_render_extent( &width, &height );
	create_depth_attachment( width, height, samples, image, image_view, allowTransient );
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

void vk_create_depth_only_image_view( VkImage image, VkFormat format, VkImageViewType view_type,
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


void vk_create_attachments( void )
{
	uint32_t i;
	uint32_t fullWidth = 0;
	uint32_t fullHeight = 0;

	vk_clear_attachment_pool();
	vk_create_volumetric_params_buffer();
	vk_get_active_render_extent( &fullWidth, &fullHeight );

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

			vk.bloom_capture_extent.width = width;
			vk.bloom_capture_extent.height = height;
			vk.bloom_mip_extent[0].width = width;
			vk.bloom_mip_extent[0].height = height;

			create_color_attachment( width, height, VK_SAMPLE_COUNT_1_BIT, vk.bloom_format,
				bloomUsage, &vk.bloom_image[0], &vk.bloom_image_view[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse, 0 );

			for ( i = 1; i < ARRAY_LEN( vk.bloom_image ); i += 2 ) {
				width /= 2;
				height /= 2;
				if ( width < 1 ) {
					width = 1;
				}
				if ( height < 1 ) {
					height = 1;
				}
				vk.bloom_mip_extent[i + 0].width = width;
				vk.bloom_mip_extent[i + 0].height = height;
				vk.bloom_mip_extent[i + 1].width = width;
				vk.bloom_mip_extent[i + 1].height = height;
				create_color_attachment( width, height, VK_SAMPLE_COUNT_1_BIT, vk.bloom_format,
					bloomUsage, &vk.bloom_image[i+0], &vk.bloom_image_view[i+0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse, 0 );

				create_color_attachment( width, height, VK_SAMPLE_COUNT_1_BIT, vk.bloom_format,
					bloomUsage, &vk.bloom_image[i+1], &vk.bloom_image_view[i+1], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse, 0 );
			}
		} else {
			Com_Memset( &vk.bloom_capture_extent, 0, sizeof( vk.bloom_capture_extent ) );
			Com_Memset( vk.bloom_mip_extent, 0, sizeof( vk.bloom_mip_extent ) );
		}

		// ssao
		if ( r_ssao && r_ssao->integer ) {
			vk_create_fullres_color_attachment( vk.ssao_format,
				sampledColorUsage, &vk.ssao_image, &vk.ssao_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );
			vk_create_fullres_color_attachment( vk.ssao_format,
				sampledColorUsage, &vk.ssao_blur_image, &vk.ssao_blur_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );
		}
		if ( r_oit && r_oit->integer ) {
			/* TRANSFER_SRC required for Phase 2.6C deferred OIT certification snapshots. */
			vk_create_fullres_color_attachment( VK_FORMAT_R16G16B16A16_SFLOAT,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
				&vk.oit_accum_image, &vk.oit_accum_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );
			vk_create_fullres_color_attachment( VK_FORMAT_R16_SFLOAT,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
				&vk.oit_reveal_image, &vk.oit_reveal_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );
			if ( r_oit->integer == 2 ) {
				vk_create_fullres_color_attachment( VK_FORMAT_R16G16B16A16_SFLOAT,
					VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
					&vk.oit_moments_image, &vk.oit_moments_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );
				vk_create_fullres_color_attachment( VK_FORMAT_R16_SFLOAT,
					VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
					&vk.oit_b0_image, &vk.oit_b0_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );
			}
			vk.oitExtentWidth = fullWidth;
			vk.oitExtentHeight = fullHeight;
			vk.oitAllocatedExtentWidth = fullWidth;
			vk.oitAllocatedExtentHeight = fullHeight;
			/* Descriptors must be rewritten after new views; FB create bumps generation once. */
			vk.oitDescriptorGeneration = 0;
			vk.oitFrameState = VK_OIT_FRAME_UNTOUCHED;
			vk.oitUnhealthy = qfalse;
		}

		// ssr (same format as color)
		if ( PostFX_SSR_IsEnabled() ) {
			vk_create_fullres_color_attachment( vk.color_format,
				copyableColorUsage, &vk.ssr_image, &vk.ssr_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );
		}

        // cubemap
        if ( vk.cubemapActive ) {
            create_color_attachment( REF_CUBEMAP_SIZE, REF_CUBEMAP_SIZE, VK_SAMPLE_COUNT_1_BIT, vk.color_format,
                sampledColorUsage, &vk.cubeMap.color_image, &vk.cubeMap.color_image_view[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT );

            if ( vk.msaaActive )
                create_color_attachment( REF_CUBEMAP_SIZE, REF_CUBEMAP_SIZE, vk_get_main_rasterization_samples(), vk.color_format,
                    sampledColorUsage, &vk.cubeMap.color_image_msaa, &vk.cubeMap.color_image_view_msaa[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qtrue, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT );
            
            create_depth_attachment( REF_CUBEMAP_SIZE, REF_CUBEMAP_SIZE, vk_get_main_rasterization_samples(),
                    &vk.cubeMap.depth_image, &vk.cubeMap.depth_image_view, qtrue );
        }

		// post-processing/msaa-resolve
			vk_create_fullres_color_attachment( vk.color_format,
				copyableColorUsage, &vk.color_image, &vk.color_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );
			vk.mainColorWidth = fullWidth;
			vk.mainColorHeight = fullHeight;
			vk_create_fullres_color_attachment( vk.color_format,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				&vk.ui_overlay_image, &vk.ui_overlay_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );
			// scene copy sampled by volumetric composite (avoids read/write feedback on vk.color_image)
				vk_create_fullres_color_attachment( vk.color_format,
					VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
					&vk.fog_scene_image, &vk.fog_scene_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );
				vk.fog_scene_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				vk_create_fullres_color_attachment( VK_FORMAT_R32_SFLOAT,
					VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
					&vk.volumetric_depth_image, &vk.volumetric_depth_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );
				vk_create_fullres_color_attachment( VK_FORMAT_R16G16_SFLOAT,
					VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
					&vk.motion_vector_image, &vk.motion_vector_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );
				if ( vk.msaaActive ) {
					vk_create_fullres_msaa_color_attachment( vk_get_main_rasterization_samples(), VK_FORMAT_R16G16_SFLOAT,
						VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
						&vk.motion_vector_msaa_image, &vk.motion_vector_msaa_view, qtrue );
				}
				/* Temporal reactive mask: full-res when TAA / temporal upscale / r_aaMode 3–5; else 1x1 stub for FP set. */
				{
					qboolean want_reactive = qfalse;
					cvar_t *ruCvar;
					VkImageUsageFlags reactiveUsage =
						VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
						VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
					ruCvar = ri.Cvar_Get( "r_rasterUltra", "0", CVAR_LATCH );
					if ( r_taa && r_taa->integer ) {
						want_reactive = qtrue;
					} else if ( r_aaMode && r_aaMode->integer >= 3 && r_aaMode->integer <= 5 ) {
						want_reactive = qtrue;
					} else if ( R_Upscale_WantTemporal() ) {
						want_reactive = qtrue;
					} else if ( r_reactiveMaskForce && r_reactiveMaskForce->integer ) {
						want_reactive = qtrue;
					} else if ( ruCvar && ruCvar->integer ) {
						if ( ( r_oit && r_oit->integer ) ||
							ri.Cvar_VariableIntegerValue( "r_gpuParticles" ) ||
							ri.Cvar_VariableIntegerValue( "r_distortion" ) ) {
							want_reactive = qtrue;
						}
					}
					if ( want_reactive ) {
						vk_create_fullres_color_attachment( VK_FORMAT_R8_UNORM, reactiveUsage,
							&vk.reactive_mask_image, &vk.reactive_mask_view,
							VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );
						vk.reactive_mask_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
						ri.Printf( PRINT_ALL, "[VK] Temporal reactive mask: full-res R8 allocated\n" );
					} else {
						create_color_attachment( 1, 1, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8_UNORM, reactiveUsage,
							&vk.reactive_mask_stub_image, &vk.reactive_mask_stub_view,
							VK_IMAGE_LAYOUT_GENERAL, qfalse, 0 );
						ri.Printf( PRINT_ALL, "[VK] Temporal reactive mask: 1x1 stub (SMAA-only / TAA off)\n" );
					}
					/* Dedicated legal bindings for descriptor-fault handling.
					 * Their texels are never trusted: binding either forces current-frame history rejection. */
					create_color_attachment( 1, 1, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8_UNORM,
						VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
						&vk.temporal_reactive_fallback_image, &vk.temporal_reactive_fallback_view,
						VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse, 0 );
					create_color_attachment( 1, 1, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8_UNORM,
						VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
						&vk.temporal_class_fallback_image, &vk.temporal_class_fallback_view,
						VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse, 0 );
				}
				/* Temporal class ping-pong (same want as reactive / TAA). */
				{
					qboolean want_class = qfalse;
					VkImageUsageFlags classUsage =
						VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
						VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
					if ( r_taa && r_taa->integer ) {
						want_class = qtrue;
					} else if ( r_aaMode && r_aaMode->integer >= 3 && r_aaMode->integer <= 5 ) {
						want_class = qtrue;
					} else if ( R_Upscale_WantTemporal() ) {
						want_class = qtrue;
					}
					if ( want_class ) {
						int ci;
						for ( ci = 0; ci < 2; ci++ ) {
							vk_create_fullres_color_attachment( VK_FORMAT_R8_UNORM, classUsage,
								&vk.temporal_class_image[ci], &vk.temporal_class_view[ci],
								VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );
							vk.temporal_class_layout[ci] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
						}
						vk.temporal.classHistoryIndex = 0;
						vk.temporal.classHasPrev = qfalse;
						ri.Printf( PRINT_ALL, "[VK] Temporal class mask: dual R8 allocated (WORLD/WEAPON)\n" );
					}
				}
				/* Dynamic-object identity ping-pong (R32_UINT storage + sampled). */
				{
					qboolean want_object_id = qfalse;
					cvar_t *objIdCvar = ri.Cvar_Get( "r_temporalObjectId", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
					VkImageUsageFlags objIdUsage =
						VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
						VK_IMAGE_USAGE_TRANSFER_DST_BIT;
					if ( objIdCvar && objIdCvar->integer ) {
						if ( r_taa && r_taa->integer ) {
							want_object_id = qtrue;
						} else if ( r_aaMode && r_aaMode->integer >= 3 && r_aaMode->integer <= 5 ) {
							want_object_id = qtrue;
						} else if ( R_Upscale_WantTemporal() ) {
							want_object_id = qtrue;
						}
					}
					if ( want_object_id ) {
						int oi;
						for ( oi = 0; oi < 2; oi++ ) {
							vk_create_fullres_color_attachment( VK_FORMAT_R32_UINT, objIdUsage,
								&vk.object_id_image[oi], &vk.object_id_view[oi],
								VK_IMAGE_LAYOUT_GENERAL, qfalse );
							vk.object_id_layout[oi] = VK_IMAGE_LAYOUT_GENERAL;
						}
						vk.temporal.objectIdIndex = 0;
						vk.temporal.objectIdHasPrev = qfalse;
						ri.Printf( PRINT_ALL, "[VK] Temporal object identity: dual R32_UINT allocated\n" );
					}
					/* Always allocate a 1x1 GENERAL stub so TAA/forward+ descriptors stay legal. */
					create_color_attachment( 1, 1, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R32_UINT,
						objIdUsage, &vk.object_id_stub_image, &vk.object_id_stub_view,
						VK_IMAGE_LAYOUT_GENERAL, qfalse, 0 );
				}
				/* True temporal depth history. R32F stores the exact single-sample
				 * reversed-Z representation consumed by TAA on the next frame. */
				{
					int di;
					VkImageUsageFlags depthHistoryUsage =
						VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
						VK_IMAGE_USAGE_TRANSFER_DST_BIT;
					for ( di = 0; di < 2; di++ ) {
						vk_create_fullres_color_attachment( VK_FORMAT_R32_SFLOAT, depthHistoryUsage,
							&vk.temporal_prev_depth_image[di], &vk.temporal_prev_depth_view[di],
							VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );
						vk.temporal_prev_depth_layout[di] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
						vk_create_fullres_color_attachment( VK_FORMAT_R32_SFLOAT, depthHistoryUsage,
							&vk.weapon_prev_depth_image[di], &vk.weapon_prev_depth_view[di],
							VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );
						vk.weapon_prev_depth_layout[di] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					}
					vk.temporal.prevDepthIndex = 0u;
					vk.temporal.prevDepthValid = qfalse;
					ri.Printf( PRINT_ALL, "[VK] Temporal previous depth: dual R32F allocated\n" );
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
			vk_create_fullres_msaa_color_attachment( vk_get_main_rasterization_samples(), vk.color_format,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &vk.msaa_image, &vk.msaa_image_view, qtrue );
			vk_create_fullres_msaa_color_attachment( vk_get_main_rasterization_samples(), vk.color_format,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &vk.ui_overlay_msaa_image, &vk.ui_overlay_msaa_image_view, qtrue );
		}

		/* Screenshot / video capture target: always allocate with FBO so HDR
		   color_image is tonemapped via capture_pipeline instead of raw FP readback.
		   Supersample still uses the same buffer at gls.captureWidth/Height. */
		create_color_attachment( gls.captureWidth, gls.captureHeight, VK_SAMPLE_COUNT_1_BIT, vk.capture_format,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			&vk.capture.image, &vk.capture.image_view, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, qfalse, 0 );

		if ( vk.smaaActive || vk.fxaaActive ) {
			VkImageUsageFlags smaaUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
				VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			if ( vk.smaaActive ) {
				vk_create_fullres_color_attachment( vk.color_format,
					smaaUsage, &vk.smaa_edge_image, &vk.smaa_edge_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );
				vk_create_fullres_color_attachment( vk.color_format,
					smaaUsage, &vk.smaa_blend_image, &vk.smaa_blend_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );
			}
			vk_create_fullres_color_attachment( vk.color_format,
				smaaUsage, &vk.smaa_output_image, &vk.smaa_output_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );
		}
		{
			VkImageUsageFlags taaUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
				VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
			vk_create_fullres_color_attachment( vk.color_format,
				taaUsage, &vk.taa_history_image[0], &vk.taa_history_image_view[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );
			vk_create_fullres_color_attachment( vk.color_format,
				taaUsage, &vk.taa_history_image[1], &vk.taa_history_image_view[1], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );
			vk_create_fullres_color_attachment( vk.color_format,
				taaUsage, &vk.weapon_history_image[0], &vk.weapon_history_view[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );
			vk_create_fullres_color_attachment( vk.color_format,
				taaUsage, &vk.weapon_history_image[1], &vk.weapon_history_view[1], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );
			vk.temporal.weaponHistoryIndex = 0u;
			vk.temporal.weaponHistoryValid = qfalse;
		}

		vk_create_deferred_gbuffer_scaffold();
		vk_create_visibility_buffer_scaffold();

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

        // BRDF LUT
        if( vk.pbrActive ) {
            uint32_t size = 512;
            create_color_attachment( size, size, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R16G16_SFLOAT,
                sampledColorUsage, &vk.brdflut_image, &vk.brdflut_image_view , VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse, 0 );
        }

	} // if ( vk.fboActive )

	//vk_alloc_attachments();

	vk_create_fullres_depth_attachment( vk_get_main_rasterization_samples(), &vk.depth_image, &vk.depth_image_view,
		(vk.fboActive && r_bloom->integer) || (r_ssao && r_ssao->integer) || (PostFX_SSR_IsEnabled()) ? qfalse : qtrue );
	vk.depth_image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	vk_alloc_attachments();
	vk_finalize_deferred_gbuffer_scaffold();
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
	vk_hdr_resolve_note_depth_recreate();

		SET_OBJECT_NAME( vk.color_image, "color attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.color_image_view, "color attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
		vk_hdr_resolve_note_scene_hdr_recreate();
		SET_OBJECT_NAME( vk.ui_overlay_image, "ui overlay attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.ui_overlay_image_view, "ui overlay attachment view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
		SET_OBJECT_NAME( vk.fog_scene_image, "fog scene copy", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.fog_scene_image_view, "fog scene copy view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
		if ( vk.oit_accum_image ) {
			SET_OBJECT_NAME( vk.oit_accum_image, "oit accum", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
			SET_OBJECT_NAME( vk.oit_accum_image_view, "oit accum view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
		}
		if ( vk.oit_reveal_image ) {
			SET_OBJECT_NAME( vk.oit_reveal_image, "oit revealage", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
			SET_OBJECT_NAME( vk.oit_reveal_image_view, "oit revealage view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
		}
		if ( vk.oit_moments_image ) {
			SET_OBJECT_NAME( vk.oit_moments_image, "oit moments", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
			SET_OBJECT_NAME( vk.oit_moments_image_view, "oit moments view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
		}
		if ( vk.oit_b0_image ) {
			SET_OBJECT_NAME( vk.oit_b0_image, "oit b0 optical depth", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
			SET_OBJECT_NAME( vk.oit_b0_image_view, "oit b0 view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
		}
			SET_OBJECT_NAME( vk.volumetric_depth_image, "volumetric depth resolve", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
			SET_OBJECT_NAME( vk.volumetric_depth_view, "volumetric depth resolve view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
			SET_OBJECT_NAME( vk.motion_vector_image, "volumetric motion vectors", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
			SET_OBJECT_NAME( vk.motion_vector_view, "volumetric motion vectors view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
			if ( vk.reactive_mask_image ) {
				SET_OBJECT_NAME( vk.reactive_mask_image, "temporal reactive mask", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
				SET_OBJECT_NAME( vk.reactive_mask_view, "temporal reactive mask view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
			}
			if ( vk.reactive_mask_stub_image ) {
				SET_OBJECT_NAME( vk.reactive_mask_stub_image, "temporal reactive mask stub", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
			}
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
		if ( vk.temporal_prev_depth_image[i] ) {
			SET_OBJECT_NAME( vk.temporal_prev_depth_image[i], va( "TemporalPrevDepthR32F[%d]", i ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
			SET_OBJECT_NAME( vk.temporal_prev_depth_view[i], va( "TemporalPrevDepthR32FView[%d]", i ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
		}
		if ( vk.weapon_prev_depth_image[i] ) {
			SET_OBJECT_NAME( vk.weapon_prev_depth_image[i], va( "WeaponPrevDepthR32F[%d]", i ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
			SET_OBJECT_NAME( vk.weapon_prev_depth_view[i], va( "WeaponPrevDepthR32FView[%d]", i ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
		}
		if ( vk.weapon_history_image[i] ) {
			SET_OBJECT_NAME( vk.weapon_history_image[i], va( "WeaponTemporalHistory[%d]", i ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
			SET_OBJECT_NAME( vk.weapon_history_view[i], va( "WeaponTemporalHistoryView[%d]", i ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
		}
		if ( vk.temporal_class_image[i] ) {
			SET_OBJECT_NAME( vk.temporal_class_image[i],
				i == 0 ? "TemporalClassR8[0]" : "TemporalPrevClassR8[1]",
				VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
			SET_OBJECT_NAME( vk.temporal_class_view[i],
				i == 0 ? "TemporalClassR8View[0]" : "TemporalPrevClassR8View[1]",
				VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
		}
		if ( vk.object_id_image[i] ) {
			SET_OBJECT_NAME( vk.object_id_image[i], va( "TemporalObjectIdR32U[%d]", i ),
				VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
			SET_OBJECT_NAME( vk.object_id_view[i], va( "TemporalObjectIdR32UView[%d]", i ),
				VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
		}
	}
	if ( vk.object_id_stub_image ) {
		SET_OBJECT_NAME( vk.object_id_stub_image, "TemporalObjectIdStubR32U", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.object_id_stub_view, "TemporalObjectIdStubR32UView", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	}
	if ( vk.reactive_mask_image ) {
		SET_OBJECT_NAME( vk.reactive_mask_image, "TemporalReactiveR8", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.reactive_mask_view, "TemporalReactiveR8View", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	}
	if ( vk.temporal_class_fallback_image ) {
		SET_OBJECT_NAME( vk.temporal_class_fallback_image, "TemporalUnclassifiedR8", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.temporal_class_fallback_view, "TemporalUnclassifiedR8View", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	}
	if ( vk.temporal_reactive_fallback_image ) {
		SET_OBJECT_NAME( vk.temporal_reactive_fallback_image, "TemporalReactiveFallbackR8", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.temporal_reactive_fallback_view, "TemporalReactiveFallbackR8View", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	}
	if ( vk.deferred_gbuffer_albedo ) {
		SET_OBJECT_NAME( vk.deferred_gbuffer_albedo, "deferred gbuffer albedo", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.deferred_gbuffer_albedo_view, "deferred gbuffer albedo view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	}
	if ( vk.deferred_gbuffer_normal ) {
		SET_OBJECT_NAME( vk.deferred_gbuffer_normal, "deferred gbuffer normal", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.deferred_gbuffer_normal_view, "deferred gbuffer normal view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	}
	if ( vk.deferred_gbuffer_material ) {
		SET_OBJECT_NAME( vk.deferred_gbuffer_material, "deferred gbuffer material", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.deferred_gbuffer_material_view, "deferred gbuffer material view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	}
	if ( vk.deferred_gbuffer_surface ) {
		SET_OBJECT_NAME( vk.deferred_gbuffer_surface, "deferred gbuffer surface", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.deferred_gbuffer_surface_view, "deferred gbuffer surface view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	}
	if ( vk.deferred_lighting_image ) {
		SET_OBJECT_NAME( vk.deferred_lighting_image, "deferred lighting", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.deferred_lighting_view, "deferred lighting view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	}
	if ( vk.deferred_class_stub ) {
		SET_OBJECT_NAME( vk.deferred_class_stub, "deferred class stub", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.deferred_class_stub_view, "deferred class stub view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	}
	if ( vk.visibility_buffer_ids ) {
		SET_OBJECT_NAME( vk.visibility_buffer_ids, "visibility buffer ids", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.visibility_buffer_ids_view, "visibility buffer ids view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	}
	if ( vk.visibility_buffer_bary ) {
		SET_OBJECT_NAME( vk.visibility_buffer_bary, "visibility buffer bary", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.visibility_buffer_bary_view, "visibility buffer bary view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	}
	if ( vk.visibility_buffer_class ) {
		SET_OBJECT_NAME( vk.visibility_buffer_class, "visibility buffer class", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.visibility_buffer_class_view, "visibility buffer class view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
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

    SET_OBJECT_NAME( vk.brdflut_image, "brdf lut image", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
    SET_OBJECT_NAME( vk.brdflut_image_view, "brdf lut image view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );

    SET_OBJECT_NAME( vk.cubeMap.color_image, "cubemap image", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
    SET_OBJECT_NAME( vk.cubeMap.color_image_msaa, "cubemap msaa image", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );

    for ( i = 0; i < ARRAY_LEN(vk.cubeMap.color_image_view); i++) {
        SET_OBJECT_NAME( vk.cubeMap.color_image_view[i], va("cubemap image view %i", i), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
        SET_OBJECT_NAME( vk.cubeMap.color_image_view_msaa[i], va("cubemap face view msaa %i",i), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
    }

    SET_OBJECT_NAME( vk.cubeMap.depth_image, "cubemap depth image", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
    SET_OBJECT_NAME( vk.cubeMap.depth_image_view, "cubemap depth image view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );

	{
		uint32_t aw = 0, ah = 0;
		vk_get_active_render_extent( &aw, &ah );
		vk_spine_attachments_created( aw, ah );
	}
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
	if ( vk.sun_shadow_view ) {
		qvkDestroyImageView( vk.device, vk.sun_shadow_view, NULL );
		vk.sun_shadow_view = VK_NULL_HANDLE;
	}
	if ( vk.sun_shadow_sample_view ) {
		qvkDestroyImageView( vk.device, vk.sun_shadow_sample_view, NULL );
		vk.sun_shadow_sample_view = VK_NULL_HANDLE;
	}
	if ( vk.sun_shadow_image ) {
		qvkDestroyImage( vk.device, vk.sun_shadow_image, NULL );
		vk.sun_shadow_image = VK_NULL_HANDLE;
	}
	if ( vk.sun_shadow_memory ) {
		qvkFreeMemory( vk.device, vk.sun_shadow_memory, NULL );
		vk.sun_shadow_memory = VK_NULL_HANDLE;
	}
	if ( vk.sun_shadow_color_view ) {
		qvkDestroyImageView( vk.device, vk.sun_shadow_color_view, NULL );
		vk.sun_shadow_color_view = VK_NULL_HANDLE;
	}
	if ( vk.sun_shadow_color_image ) {
		qvkDestroyImage( vk.device, vk.sun_shadow_color_image, NULL );
		vk.sun_shadow_color_image = VK_NULL_HANDLE;
	}
	if ( vk.sun_shadow_color_memory ) {
		qvkFreeMemory( vk.device, vk.sun_shadow_color_memory, NULL );
		vk.sun_shadow_color_memory = VK_NULL_HANDLE;
	}
	if ( vk.sun_shadow_color_msaa_view ) {
		qvkDestroyImageView( vk.device, vk.sun_shadow_color_msaa_view, NULL );
		vk.sun_shadow_color_msaa_view = VK_NULL_HANDLE;
	}
	if ( vk.sun_shadow_color_msaa_image ) {
		qvkDestroyImage( vk.device, vk.sun_shadow_color_msaa_image, NULL );
		vk.sun_shadow_color_msaa_image = VK_NULL_HANDLE;
	}
	if ( vk.sun_shadow_color_msaa_memory ) {
		qvkFreeMemory( vk.device, vk.sun_shadow_color_msaa_memory, NULL );
		vk.sun_shadow_color_msaa_memory = VK_NULL_HANDLE;
	}
	vk.sun_shadow_width = 0;
	vk.sun_shadow_height = 0;
	vk.sun_shadow_tile_size = 0;
	vk.sun_shadow_cascade_count = 1;
	Matrix16Identity( vk.sun_shadow_matrix0 );
	{
		int ci;
		for ( ci = 0; ci < 4; ci++ ) {
			Matrix16Identity( vk.sun_shadow_matrix[ci] );
			vk.sun_shadow_splits[ci] = 0.0f;
		}
	}
	vk.sun_shadow_near = 4.0f;
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

	{
		int cascades = VK_SunCSM_CascadeCount();
		int atlas = map_size;
		int tile = map_size;
		if ( cascades < 1 ) {
			cascades = 1;
		}
		if ( cascades > 4 ) {
			cascades = 4;
		}
		if ( cascades > 1 ) {
			/* Keep atlas ≤ 4096: shrink tile if needed. */
			if ( map_size * 2 > 4096 ) {
				map_size = 2048;
			}
			atlas = map_size * 2;
			tile = map_size;
		}
		vk.sun_shadow_tile_size = (uint32_t)tile;
		vk.sun_shadow_cascade_count = (uint32_t)cascades;
		vk.sun_shadow_width = (uint32_t)atlas;
		vk.sun_shadow_height = (uint32_t)atlas;
	}
	Matrix16Identity( vk.sun_shadow_matrix0 );
	{
		int ci;
		for ( ci = 0; ci < 4; ci++ ) {
			Matrix16Identity( vk.sun_shadow_matrix[ci] );
			vk.sun_shadow_splits[ci] = 0.0f;
		}
	}
	vk.sun_shadow_near = 4.0f;
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

	if ( vk.local_spot_shadow_atlas_view ) {
		qvkDestroyImageView( vk.device, vk.local_spot_shadow_atlas_view, NULL );
		vk.local_spot_shadow_atlas_view = VK_NULL_HANDLE;
	}
	if ( vk.local_spot_shadow_atlas_sample_view ) {
		qvkDestroyImageView( vk.device, vk.local_spot_shadow_atlas_sample_view, NULL );
		vk.local_spot_shadow_atlas_sample_view = VK_NULL_HANDLE;
	}
	if ( vk.local_spot_shadow_atlas_image ) {
		qvkDestroyImage( vk.device, vk.local_spot_shadow_atlas_image, NULL );
		vk.local_spot_shadow_atlas_image = VK_NULL_HANDLE;
	}
	if ( vk.local_spot_shadow_atlas_memory ) {
		qvkFreeMemory( vk.device, vk.local_spot_shadow_atlas_memory, NULL );
		vk.local_spot_shadow_atlas_memory = VK_NULL_HANDLE;
	}

	if ( vk.local_spot_shadow_color_view ) {
		qvkDestroyImageView( vk.device, vk.local_spot_shadow_color_view, NULL );
		vk.local_spot_shadow_color_view = VK_NULL_HANDLE;
	}
	if ( vk.local_spot_shadow_color_image ) {
		qvkDestroyImage( vk.device, vk.local_spot_shadow_color_image, NULL );
		vk.local_spot_shadow_color_image = VK_NULL_HANDLE;
	}
	if ( vk.local_spot_shadow_color_memory ) {
		qvkFreeMemory( vk.device, vk.local_spot_shadow_color_memory, NULL );
		vk.local_spot_shadow_color_memory = VK_NULL_HANDLE;
	}

	if ( vk.local_point_shadow_array_view ) {
		qvkDestroyImageView( vk.device, vk.local_point_shadow_array_view, NULL );
		vk.local_point_shadow_array_view = VK_NULL_HANDLE;
	}
	if ( vk.local_point_shadow_array_sample_view ) {
		qvkDestroyImageView( vk.device, vk.local_point_shadow_array_sample_view, NULL );
		vk.local_point_shadow_array_sample_view = VK_NULL_HANDLE;
	}
	if ( vk.local_point_shadow_array_image ) {
		qvkDestroyImage( vk.device, vk.local_point_shadow_array_image, NULL );
		vk.local_point_shadow_array_image = VK_NULL_HANDLE;
	}
	if ( vk.local_point_shadow_array_memory ) {
		qvkFreeMemory( vk.device, vk.local_point_shadow_array_memory, NULL );
		vk.local_point_shadow_array_memory = VK_NULL_HANDLE;
	}

	if ( vk.local_point_shadow_color_array_view ) {
		qvkDestroyImageView( vk.device, vk.local_point_shadow_color_array_view, NULL );
		vk.local_point_shadow_color_array_view = VK_NULL_HANDLE;
	}
	if ( vk.local_point_shadow_color_array_image ) {
		qvkDestroyImage( vk.device, vk.local_point_shadow_color_array_image, NULL );
		vk.local_point_shadow_color_array_image = VK_NULL_HANDLE;
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
	if ( vk.froxel_volume_view ) {
		qvkDestroyImageView( vk.device, vk.froxel_volume_view, NULL );
		vk.froxel_volume_view = VK_NULL_HANDLE;
	}
	if ( vk.froxel_volume_image ) {
		qvkDestroyImage( vk.device, vk.froxel_volume_image, NULL );
		vk.froxel_volume_image = VK_NULL_HANDLE;
	}
	if ( vk.froxel_volume_memory ) {
		qvkFreeMemory( vk.device, vk.froxel_volume_memory, NULL );
		vk.froxel_volume_memory = VK_NULL_HANDLE;
	}

	if ( vk.froxel_history_view ) {
		qvkDestroyImageView( vk.device, vk.froxel_history_view, NULL );
		vk.froxel_history_view = VK_NULL_HANDLE;
	}
	if ( vk.froxel_history_image ) {
		qvkDestroyImage( vk.device, vk.froxel_history_image, NULL );
		vk.froxel_history_image = VK_NULL_HANDLE;
	}
	if ( vk.froxel_history_memory ) {
		qvkFreeMemory( vk.device, vk.froxel_history_memory, NULL );
		vk.froxel_history_memory = VK_NULL_HANDLE;
	}
	if ( vk.froxel_extinction_view ) {
		qvkDestroyImageView( vk.device, vk.froxel_extinction_view, NULL );
		vk.froxel_extinction_view = VK_NULL_HANDLE;
	}
	if ( vk.froxel_extinction_image ) {
		qvkDestroyImage( vk.device, vk.froxel_extinction_image, NULL );
		vk.froxel_extinction_image = VK_NULL_HANDLE;
	}
	if ( vk.froxel_extinction_memory ) {
		qvkFreeMemory( vk.device, vk.froxel_extinction_memory, NULL );
		vk.froxel_extinction_memory = VK_NULL_HANDLE;
	}
	if ( vk.froxel_light_view ) {
		qvkDestroyImageView( vk.device, vk.froxel_light_view, NULL );
		vk.froxel_light_view = VK_NULL_HANDLE;
	}
	if ( vk.froxel_light_image ) {
		qvkDestroyImage( vk.device, vk.froxel_light_image, NULL );
		vk.froxel_light_image = VK_NULL_HANDLE;
	}
	if ( vk.froxel_light_memory ) {
		qvkFreeMemory( vk.device, vk.froxel_light_memory, NULL );
		vk.froxel_light_memory = VK_NULL_HANDLE;
	}
	if ( vk.froxel_clamp_view ) {
		qvkDestroyImageView( vk.device, vk.froxel_clamp_view, NULL );
		vk.froxel_clamp_view = VK_NULL_HANDLE;
	}
	if ( vk.froxel_clamp_image ) {
		qvkDestroyImage( vk.device, vk.froxel_clamp_image, NULL );
		vk.froxel_clamp_image = VK_NULL_HANDLE;
	}
	if ( vk.froxel_clamp_memory ) {
		qvkFreeMemory( vk.device, vk.froxel_clamp_memory, NULL );
		vk.froxel_clamp_memory = VK_NULL_HANDLE;
	}
	for ( int i = 0; i < 2; i++ ) {
		if ( vk.fluid_velocity_views[i] ) {
			qvkDestroyImageView( vk.device, vk.fluid_velocity_views[i], NULL );
			vk.fluid_velocity_views[i] = VK_NULL_HANDLE;
		}
		if ( vk.fluid_velocity_images[i] ) {
			qvkDestroyImage( vk.device, vk.fluid_velocity_images[i], NULL );
			vk.fluid_velocity_images[i] = VK_NULL_HANDLE;
		}
		if ( vk.fluid_velocity_memory[i] ) {
			qvkFreeMemory( vk.device, vk.fluid_velocity_memory[i], NULL );
			vk.fluid_velocity_memory[i] = VK_NULL_HANDLE;
		}
		if ( vk.fluid_density_views[i] ) {
			qvkDestroyImageView( vk.device, vk.fluid_density_views[i], NULL );
			vk.fluid_density_views[i] = VK_NULL_HANDLE;
		}
		if ( vk.fluid_density_images[i] ) {
			qvkDestroyImage( vk.device, vk.fluid_density_images[i], NULL );
			vk.fluid_density_images[i] = VK_NULL_HANDLE;
		}
		if ( vk.fluid_density_memory[i] ) {
			qvkFreeMemory( vk.device, vk.fluid_density_memory[i], NULL );
			vk.fluid_density_memory[i] = VK_NULL_HANDLE;
		}
		if ( vk.fluid_pressure_views[i] ) {
			qvkDestroyImageView( vk.device, vk.fluid_pressure_views[i], NULL );
			vk.fluid_pressure_views[i] = VK_NULL_HANDLE;
		}
		if ( vk.fluid_pressure_images[i] ) {
			qvkDestroyImage( vk.device, vk.fluid_pressure_images[i], NULL );
			vk.fluid_pressure_images[i] = VK_NULL_HANDLE;
		}
		if ( vk.fluid_pressure_memory[i] ) {
			qvkFreeMemory( vk.device, vk.fluid_pressure_memory[i], NULL );
			vk.fluid_pressure_memory[i] = VK_NULL_HANDLE;
		}
	}
	if ( vk.fluid_divergence_view ) {
		qvkDestroyImageView( vk.device, vk.fluid_divergence_view, NULL );
		vk.fluid_divergence_view = VK_NULL_HANDLE;
	}
	if ( vk.fluid_divergence_image ) {
		qvkDestroyImage( vk.device, vk.fluid_divergence_image, NULL );
		vk.fluid_divergence_image = VK_NULL_HANDLE;
	}
	if ( vk.fluid_divergence_memory ) {
		qvkFreeMemory( vk.device, vk.fluid_divergence_memory, NULL );
		vk.fluid_divergence_memory = VK_NULL_HANDLE;
	}
	if ( vk.volumetric_telemetry_view ) {
		qvkDestroyImageView( vk.device, vk.volumetric_telemetry_view, NULL );
		vk.volumetric_telemetry_view = VK_NULL_HANDLE;
	}
	if ( vk.volumetric_telemetry_image ) {
		qvkDestroyImage( vk.device, vk.volumetric_telemetry_image, NULL );
		vk.volumetric_telemetry_image = VK_NULL_HANDLE;
	}
	if ( vk.volumetric_telemetry_memory ) {
		qvkFreeMemory( vk.device, vk.volumetric_telemetry_memory, NULL );
		vk.volumetric_telemetry_memory = VK_NULL_HANDLE;
	}
	if ( vk.fog_noise_sampler ) {
		vk.fog_noise_sampler = VK_NULL_HANDLE;
	}
	if ( vk.fog_noise_view ) {
		qvkDestroyImageView( vk.device, vk.fog_noise_view, NULL );
		vk.fog_noise_view = VK_NULL_HANDLE;
	}
	if ( vk.fog_noise_image ) {
		qvkDestroyImage( vk.device, vk.fog_noise_image, NULL );
		vk.fog_noise_image = VK_NULL_HANDLE;
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
	qboolean fog_enabled = ( r_volumetricFog && r_volumetricFog->integer ) ? qtrue : qfalse;

	if ( !glConfig.vidWidth || !glConfig.vidHeight ) {
		return;
	}

	vk_destroy_froxel_images();

	if ( !fog_enabled ) {
		grid_x = grid_y = grid_z = 4;
		quality = 2;
		resolution_scale = 1.0f;
		fluid_quality = 0;
		fluid_resolution_scale = 1.0f;
	}

	if ( fog_enabled && r_volumetricFogGridDim && r_volumetricFogGridDim->string && r_volumetricFogGridDim->string[0] ) {
		if ( sscanf( r_volumetricFogGridDim->string, "%d %d %d", &grid_x, &grid_y, &grid_z ) != 3 ) {
			/* "half" / "0" / empty-ish → viewport-relative half-res froxels. */
			grid_x = VK_FROXEL_HALFRES_SENTINEL;
			grid_y = VK_FROXEL_HALFRES_SENTINEL;
			grid_z = VK_FROXEL_DEFAULT_SLICES;
		}
	}

	/* Half-res froxel volume: (vidWidth/2, vidHeight/2, slices). X/Y <= 0 selects this.
	   Auto mode is clamped to VK_FROXEL_AUTO_MAX_* so window resize/maximize
	   cannot allocate driver-hostile multi-hundred-MiB 3D history sets;
	   set an explicit GridDim to override the auto clamp. */
	if ( fog_enabled && grid_x <= VK_FROXEL_HALFRES_SENTINEL ) {
		grid_x = MAX( 1, glConfig.vidWidth / 2 );
		if ( grid_x > VK_FROXEL_AUTO_MAX_WIDTH ) {
			grid_x = VK_FROXEL_AUTO_MAX_WIDTH;
		}
	}
	if ( fog_enabled && grid_y <= VK_FROXEL_HALFRES_SENTINEL ) {
		grid_y = MAX( 1, glConfig.vidHeight / 2 );
		if ( grid_y > VK_FROXEL_AUTO_MAX_HEIGHT ) {
			grid_y = VK_FROXEL_AUTO_MAX_HEIGHT;
		}
	}
	if ( fog_enabled && grid_z <= 0 ) {
		grid_z = VK_FROXEL_DEFAULT_SLICES;
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
	if ( fog_enabled &&
		( vk.froxel_width <= 1 || vk.froxel_height <= 1 || vk.froxel_slices <= 1 ) &&
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

	if ( fog_enabled ) {
		ri.Printf( PRINT_ALL,
			"[VK][fog] froxel volume RGBA16F %ux%ux%u (log-Z via r_volumetricFogSliceMode; composite before tonemap)\n",
			vk.froxel_width, vk.froxel_height, vk.froxel_slices );
	} else {
		ri.Printf( PRINT_ALL, "[VK][fog] volumetric fog disabled; using %ux%ux%u descriptor stubs\n",
			vk.froxel_width, vk.froxel_height, vk.froxel_slices );
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

void vk_destroy_attachments( void )
{
	uint32_t i;

	vk_spine_attachments_destroyed();
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
			vk_destroy_image_and_view( &vk.bloom_image[i], &vk.bloom_image_view[i] );
		}
	}

	if ( vk.ssao_image ) {
		vk_destroy_image_and_view( &vk.ssao_image, &vk.ssao_image_view );
	}

	if ( vk.ssao_blur_image ) {
		vk_destroy_image_and_view( &vk.ssao_blur_image, &vk.ssao_blur_image_view );
	}
	if ( vk.oit_accum_image ) {
		vk_destroy_image_and_view( &vk.oit_accum_image, &vk.oit_accum_image_view );
	}
	if ( vk.oit_reveal_image ) {
		vk_destroy_image_and_view( &vk.oit_reveal_image, &vk.oit_reveal_image_view );
	}
	if ( vk.oit_moments_image ) {
		vk_destroy_image_and_view( &vk.oit_moments_image, &vk.oit_moments_image_view );
	}
	if ( vk.oit_b0_image ) {
		vk_destroy_image_and_view( &vk.oit_b0_image, &vk.oit_b0_image_view );
	}
	vk.oitDescriptorGeneration = 0;
	vk.oitExtentWidth = 0;
	vk.oitExtentHeight = 0;
	vk.oitFrameState = VK_OIT_FRAME_UNTOUCHED;
	vk.oitUnhealthy = qtrue; /* force gen match after recreate */
	vk.oitLastFallbackReason[0] = '\0';

	if ( vk.ssr_image ) {
		vk_destroy_image_and_view( &vk.ssr_image, &vk.ssr_image_view );
	}

	if ( vk.color_image ) {
		vk_destroy_image_and_view( &vk.color_image, &vk.color_image_view );
		vk.post_fog_color_source = VK_NULL_HANDLE;
		vk.scene_post_fog_color_source = VK_NULL_HANDLE;
		vk.mainColorWidth = 0u;
		vk.mainColorHeight = 0u;
	}
	if ( vk.ui_overlay_image ) {
		vk_destroy_image_and_view( &vk.ui_overlay_image, &vk.ui_overlay_image_view );
	}
	if ( vk.fog_scene_image ) {
		vk_destroy_image_and_view( &vk.fog_scene_image, &vk.fog_scene_image_view );
		vk.fog_scene_layout = VK_IMAGE_LAYOUT_UNDEFINED;
	}
	if ( vk.volumetric_depth_image ) {
		vk_destroy_image_and_view( &vk.volumetric_depth_image, &vk.volumetric_depth_view );
	}
	if ( vk.motion_vector_image ) {
		vk_destroy_image_and_view( &vk.motion_vector_image, &vk.motion_vector_view );
	}
	if ( vk.reactive_mask_image ) {
		vk_destroy_image_and_view( &vk.reactive_mask_image, &vk.reactive_mask_view );
		vk.reactive_mask_layout = VK_IMAGE_LAYOUT_UNDEFINED;
	}
	if ( vk.reactive_mask_stub_image ) {
		vk_destroy_image_and_view( &vk.reactive_mask_stub_image, &vk.reactive_mask_stub_view );
	}
	if ( vk.temporal_reactive_fallback_image ) {
		vk_destroy_image_and_view( &vk.temporal_reactive_fallback_image, &vk.temporal_reactive_fallback_view );
	}
	if ( vk.temporal_class_fallback_image ) {
		vk_destroy_image_and_view( &vk.temporal_class_fallback_image, &vk.temporal_class_fallback_view );
	}
	vk.temporal_class_fallback_descriptor = VK_NULL_HANDLE;
	vk.temporal_reactive_fallback_descriptor = VK_NULL_HANDLE;
	{
		int ci;
		for ( ci = 0; ci < 2; ci++ ) {
			if ( vk.temporal_class_image[ci] ) {
				vk_destroy_image_and_view( &vk.temporal_class_image[ci], &vk.temporal_class_view[ci] );
				vk.temporal_class_layout[ci] = VK_IMAGE_LAYOUT_UNDEFINED;
			}
			if ( vk.object_id_image[ci] ) {
				vk_destroy_image_and_view( &vk.object_id_image[ci], &vk.object_id_view[ci] );
				vk.object_id_layout[ci] = VK_IMAGE_LAYOUT_UNDEFINED;
			}
		}
	}
	if ( vk.object_id_stub_image ) {
		vk_destroy_image_and_view( &vk.object_id_stub_image, &vk.object_id_stub_view );
	}
	if ( vk.motion_vector_msaa_image ) {
		vk_destroy_image_and_view( &vk.motion_vector_msaa_image, &vk.motion_vector_msaa_view );
	}

	if ( vk.smaa_edge_image ) {
		vk_destroy_image_and_view( &vk.smaa_edge_image, &vk.smaa_edge_image_view );
	}

	if ( vk.smaa_blend_image ) {
		vk_destroy_image_and_view( &vk.smaa_blend_image, &vk.smaa_blend_image_view );
	}

	if ( vk.smaa_output_image ) {
		vk_destroy_image_and_view( &vk.smaa_output_image, &vk.smaa_output_image_view );
	}
	for ( i = 0; i < 2; i++ ) {
		if ( vk.taa_history_image[i] ) {
			vk_destroy_image_and_view( &vk.taa_history_image[i], &vk.taa_history_image_view[i] );
		}
		vk.taa_history_descriptor[i] = VK_NULL_HANDLE;
		if ( vk.temporal_prev_depth_image[i] ) {
			vk_destroy_image_and_view( &vk.temporal_prev_depth_image[i], &vk.temporal_prev_depth_view[i] );
		}
		vk.temporal_prev_depth_layout[i] = VK_IMAGE_LAYOUT_UNDEFINED;
		vk.temporal_prev_depth_descriptor[i] = VK_NULL_HANDLE;
		vk.temporal_depth_copy_descriptor[i] = VK_NULL_HANDLE;
		if ( vk.weapon_prev_depth_image[i] ) {
			vk_destroy_image_and_view( &vk.weapon_prev_depth_image[i], &vk.weapon_prev_depth_view[i] );
		}
		if ( vk.weapon_history_image[i] ) {
			vk_destroy_image_and_view( &vk.weapon_history_image[i], &vk.weapon_history_view[i] );
		}
		vk.weapon_prev_depth_layout[i] = VK_IMAGE_LAYOUT_UNDEFINED;
		vk.weapon_history_descriptor[i] = VK_NULL_HANDLE;
		vk.weapon_prev_depth_descriptor[i] = VK_NULL_HANDLE;
		vk.weapon_depth_copy_descriptor[i] = VK_NULL_HANDLE;
	}
	vk.temporal.prevDepthValid = qfalse;
	vk.temporal.weaponHistoryValid = qfalse;
	if ( vk.deferred_gbuffer_albedo ) {
		vk_destroy_image_and_view( &vk.deferred_gbuffer_albedo, &vk.deferred_gbuffer_albedo_view );
	}
	if ( vk.deferred_gbuffer_normal ) {
		vk_destroy_image_and_view( &vk.deferred_gbuffer_normal, &vk.deferred_gbuffer_normal_view );
	}
	if ( vk.deferred_gbuffer_material ) {
		vk_destroy_image_and_view( &vk.deferred_gbuffer_material, &vk.deferred_gbuffer_material_view );
	}
	if ( vk.deferred_gbuffer_surface ) {
		vk_destroy_image_and_view( &vk.deferred_gbuffer_surface, &vk.deferred_gbuffer_surface_view );
	}
	if ( vk.deferred_lighting_image ) {
		vk_destroy_image_and_view( &vk.deferred_lighting_image, &vk.deferred_lighting_view );
	}
	if ( vk.deferred_class_stub ) {
		vk_destroy_image_and_view( &vk.deferred_class_stub, &vk.deferred_class_stub_view );
	}
	vk.deferredGbufferAllocated = qfalse;
	vk.deferredGbufferDirectExport = qfalse;
	vk.deferredGbufferExtentW = 0;
	vk.deferredGbufferExtentH = 0;
	/* Keep generation counter so post-destroy consumers detect a mismatch until recreate. */

	if ( vk.visibility_buffer_ids ) {
		vk_destroy_image_and_view( &vk.visibility_buffer_ids, &vk.visibility_buffer_ids_view );
	}
	if ( vk.visibility_buffer_bary ) {
		vk_destroy_image_and_view( &vk.visibility_buffer_bary, &vk.visibility_buffer_bary_view );
	}
	if ( vk.visibility_buffer_class ) {
		vk_destroy_image_and_view( &vk.visibility_buffer_class, &vk.visibility_buffer_class_view );
	}
	vk.visibilityBufferAllocated = qfalse;
	vk.visibilityBufferDirectExport = qfalse;

	if ( vk.msaa_image ) {
		vk_destroy_image_and_view( &vk.msaa_image, &vk.msaa_image_view );
	}
	if ( vk.ui_overlay_msaa_image ) {
		vk_destroy_image_and_view( &vk.ui_overlay_msaa_image, &vk.ui_overlay_msaa_image_view );
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
		vk_destroy_image_and_view( &vk.screenMap.color_image, &vk.screenMap.color_image_view );
	}

	if ( vk.screenMap.motion_image ) {
		vk_destroy_image_and_view( &vk.screenMap.motion_image, &vk.screenMap.motion_image_view );
	}

	if ( vk.screenMap.color_image_msaa ) {
		vk_destroy_image_and_view( &vk.screenMap.color_image_msaa, &vk.screenMap.color_image_view_msaa );
	}

	if ( vk.screenMap.motion_image_msaa ) {
		vk_destroy_image_and_view( &vk.screenMap.motion_image_msaa, &vk.screenMap.motion_image_view_msaa );
	}

	if ( vk.screenMap.depth_image ) {
		vk_destroy_image_and_view( &vk.screenMap.depth_image, &vk.screenMap.depth_image_view );
	}

	if ( vk.capture.image ) {
		vk_destroy_image_and_view( &vk.capture.image, &vk.capture.image_view );
	}

    if ( vk.brdflut_image_view ) {
        vk_destroy_image_and_view( &vk.brdflut_image, &vk.brdflut_image_view );
	}

	// render world to cubemap
    for ( i = 0; i < ARRAY_LEN(vk.cubeMap.color_image_view); i++) {      
		if ( vk.cubeMap.color_image_view[i] ) {
			qvkDestroyImageView( vk.device, vk.cubeMap.color_image_view[i], NULL );
			vk.cubeMap.color_image_view[i] = VK_NULL_HANDLE;
		}
		if ( vk.cubeMap.color_image_view_msaa[i] ) {
			qvkDestroyImageView( vk.device, vk.cubeMap.color_image_view_msaa[i], NULL );
			vk.cubeMap.color_image_view_msaa[i] = VK_NULL_HANDLE;
		}
	}
	if ( vk.cubeMap.color_image ) {
		qvkDestroyImage( vk.device, vk.cubeMap.color_image, NULL );
		vk.cubeMap.color_image = VK_NULL_HANDLE;
	}
	if ( vk.cubeMap.color_image_msaa ) {
		qvkDestroyImage( vk.device, vk.cubeMap.color_image_msaa, NULL );
		vk.cubeMap.color_image_msaa = VK_NULL_HANDLE;
	}
	if ( vk.cubeMap.depth_image ) {
		vk_destroy_image_and_view( &vk.cubeMap.depth_image, &vk.cubeMap.depth_image_view );
	}

	for ( i = 0; i < vk.image_memory_count; i++ ) {
		qvkFreeMemory( vk.device, vk.image_memory[i], NULL );
	}

	vk.image_memory_count = 0;
}
