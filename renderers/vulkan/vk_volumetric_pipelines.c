/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Volumetric fog compute/graphics pipelines, fluid sim compute, depth resolve,
luminance, CBT terrain compute, and vegetation-wind compute setup.
Split from vk.c (called from vk_create_pipelines).
===========================================================================
*/

#include "tr_local.h"
#include "vk_postfx_params.h"

static inline qboolean vk_hdr64_active( void )
{
	return vk.color_format == VK_FORMAT_R64G64B64A64_SFLOAT;
}

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

	{
		VkPushConstantRange resolve_push_range;

		resolve_push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		resolve_push_range.offset = 0;
		resolve_push_range.size = sizeof( int ) * 2;
		desc.pSetLayouts = &vk.volumetric_depth_resolve_layout;
		desc.pushConstantRangeCount = 1;
		desc.pPushConstantRanges = &resolve_push_range;
		VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.volumetric_depth_resolve_pipeline_layout ) );
		desc.pushConstantRangeCount = 0;
		desc.pPushConstantRanges = NULL;
	}

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

static void vk_create_temporal_depth_history_copy_pipeline( void )
{
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo desc;

	if ( vk.temporal_depth_history_copy_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.temporal_depth_history_copy_pipeline, NULL );
		vk.temporal_depth_history_copy_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.volumetric_depth_resolve_pipeline_layout == VK_NULL_HANDLE ||
		vk.modules.temporal_depth_history_copy_cs == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.modules.temporal_depth_history_copy_cs;
	stage.pName = "main";

	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	desc.stage = stage;
	desc.layout = vk.volumetric_depth_resolve_pipeline_layout;
	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &desc, NULL,
		&vk.temporal_depth_history_copy_pipeline ) );
	SET_OBJECT_NAME( vk.temporal_depth_history_copy_pipeline,
		"pipeline - temporal previous depth copy", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
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

void vk_create_volumetric_pipelines( void )
{
	vk_create_volumetric_pipeline_layouts();
	vk_create_volumetric_depth_resolve_pipeline();
	vk_create_temporal_depth_history_copy_pipeline();
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
