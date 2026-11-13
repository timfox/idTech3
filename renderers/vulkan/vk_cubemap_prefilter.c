/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Cubemap IBL prefilter: irradiance + prefiltered env, SH extraction, generation.
Split from vk.c.
===========================================================================
*/

#include "tr_local.h"
#include "vk_image_layout.h"
#include "vk_cmd.h"
#include "vk_render_pass.h"
#include "vk_scene_pass.h"
#include "vk_pipeline_helpers.h"
#include "vk_util.h"
#include <math.h>

#ifdef VK_CUBEMAP

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

		if ( def->offscreen.framebuffer != VK_NULL_HANDLE ) {
			qvkDestroyFramebuffer( vk.device, def->offscreen.framebuffer, NULL );
			def->offscreen.framebuffer = VK_NULL_HANDLE;
		}
		if ( def->renderpass != VK_NULL_HANDLE ) {
			qvkDestroyRenderPass( vk.device, def->renderpass, NULL );
			def->renderpass = VK_NULL_HANDLE;
		}
		if ( def->offscreen.view != VK_NULL_HANDLE ) {
			qvkDestroyImageView( vk.device, def->offscreen.view, NULL );
			def->offscreen.view = VK_NULL_HANDLE;
		}
		if ( def->offscreen.image != VK_NULL_HANDLE ) {
			qvkDestroyImage( vk.device, def->offscreen.image, NULL );
			def->offscreen.image = VK_NULL_HANDLE;
		}
		if ( def->offscreen.memory != VK_NULL_HANDLE ) {
			qvkFreeMemory( vk.device, def->offscreen.memory, NULL );
			def->offscreen.memory = VK_NULL_HANDLE;
		}
		if ( def->pipeline != VK_NULL_HANDLE ) {
			qvkDestroyPipeline( vk.device, def->pipeline, NULL );
			def->pipeline = VK_NULL_HANDLE;
		}
		if ( def->pipeline_layout != VK_NULL_HANDLE ) {
			qvkDestroyPipelineLayout( vk.device, def->pipeline_layout, NULL );
			def->pipeline_layout = VK_NULL_HANDLE;
		}
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

		if ( r_pbr_bindlog && r_pbr_bindlog->integer ) {
			ri.Printf( PRINT_ALL,
				"PBR IBL cubemap layout: target=%d img=%p view=%p layout=SHADER_READ_ONLY name=%s\n",
				def->target,
				(void *)cubemap, (void *)cubemap->view,
				cube->name[0] ? cube->name : "<unnamed>" );
		}
	}

	if ( r_pbr_shExtract && r_pbr_shExtract->integer && vk.pbrActive && cube && cube->irradiance_image ) {
		R_ResetCubemapSH( cube );
		if ( vk_extract_sh_coeffs( cube->irradiance_image, cube->shCoeffs ) ) {
			cube->hasSHCoeffs = qtrue;
			ri.Printf( PRINT_DEVELOPER, "PBR: extracted SH coeffs for cubemap '%s'\n", cube->name );
		}
	}

	command_buffer = vk_begin_command_buffer();
	record_image_layout_transition( command_buffer, vk.cubeMap.color_image, VK_IMAGE_ASPECT_COLOR_BIT, 
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
		0, 0 );
	vk_end_command_buffer( command_buffer, __func__  );

	vk_begin_main_render_pass();
}

void vk_begin_cubemap_render_pass( void )
{
	VkFramebuffer frameBuffer = vk.framebuffers.cubemap[backEnd.viewParms.targetCubeLayer];

	vk.renderPassIndex = RENDER_PASS_CUBEMAP;

	vk.renderWidth = REF_CUBEMAP_SIZE;
	vk.renderHeight = REF_CUBEMAP_SIZE;
	vk.renderScaleX = vk.renderScaleY = 1.0f;

	vk_begin_render_pass_tracked( vk.render_pass.cubemap, frameBuffer, qtrue, vk.renderWidth, vk.renderHeight );
}

#endif /* VK_CUBEMAP */

#ifdef VK_PBR_BRDFLUT
void vk_create_brfdlut( void )
{
	if ( !vk.pbrActive )
		return;

	{
		VkRenderPassBeginInfo begin_info;
		VkClearValue clear_values[1];
		VkCommandBuffer command_buffer;
		VkViewport viewport;
		VkRect2D scissor_rect;
		uint32_t size;

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

		vk_end_command_buffer( command_buffer, __func__ );
	}
}
#endif /* VK_PBR_BRDFLUT */
