/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Post-process pipeline pieces: shader stage helper, atmosphere, OIT accum,
bloom blur passes (split from vk.c).
===========================================================================
*/

#include "tr_local.h"
#include "vk_pipeline_helpers.h"
#include "vk_forward_plus.h"

/* r_hdr 3: 64-bit (RGBA64F) uses dvec4 fragment output; select HDR64 shaders when active */
static inline qboolean vk_hdr64_active( void )
{
	return vk.color_format == VK_FORMAT_R64G64B64A64_SFLOAT;
}

void vk_set_shader_stage_desc(VkPipelineShaderStageCreateInfo *desc, VkShaderStageFlagBits stage, VkShaderModule shader_module, const char *entry) {
	desc->sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	desc->pNext = NULL;
	desc->flags = 0;
	desc->stage = stage;
	desc->module = shader_module;
	desc->pName = entry;
	desc->pSpecializationInfo = NULL;
}


void vk_create_atmosphere_pipeline( void )
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
	/* Reversed depth: far=0.0. Pass only where stored==0.0 (sky). Shader outputs gl_FragDepth=0.0. */
	depth_stencil_state.depthCompareOp = VK_COMPARE_OP_EQUAL;

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
	multisample_state.rasterizationSamples = vk_get_main_rasterization_samples();

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

void vk_create_oit_accum_pipeline( void )
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
	VkSpecializationMapEntry spec_entries[2];
	VkSpecializationInfo frag_spec_info;
	int manual_depth_test = vk.msaaActive ? 1 : 0;
	int forward_plus_lit = 0;
	int spec_data[2];
	VkViewport viewport = { 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f };
	VkRect2D scissor = { { 0, 0 }, { 1, 1 } };

	if ( vk.pipeline_layout_oit_accum == VK_NULL_HANDLE || vk.render_pass.oit_accum == VK_NULL_HANDLE ||
		vk.modules.oit_accum_vs == VK_NULL_HANDLE || vk.modules.oit_accum_fs == VK_NULL_HANDLE ) {
		return;
	}

	if ( r_oitForwardPlus && r_oitForwardPlus->integer &&
		vk.set_layout_forward_plus != VK_NULL_HANDLE ) {
		if ( vk_forward_plus_get_graphics_descriptor_set() != VK_NULL_HANDLE ) {
			forward_plus_lit = 1;
		}
	}

	if ( vk.oit_accum_pipeline != VK_NULL_HANDLE ) {
		vk_wait_idle();
		qvkDestroyPipeline( vk.device, vk.oit_accum_pipeline, NULL );
		vk.oit_accum_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.oit_accum_additive_pipeline != VK_NULL_HANDLE ) {
		vk_wait_idle();
		qvkDestroyPipeline( vk.device, vk.oit_accum_additive_pipeline, NULL );
		vk.oit_accum_additive_pipeline = VK_NULL_HANDLE;
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
	spec_data[0] = manual_depth_test;
	spec_data[1] = forward_plus_lit;
	spec_entries[0].constantID = 0;
	spec_entries[0].offset = 0;
	spec_entries[0].size = sizeof( int );
	spec_entries[1].constantID = 1;
	spec_entries[1].offset = sizeof( int );
	spec_entries[1].size = sizeof( int );
	frag_spec_info.mapEntryCount = 2;
	frag_spec_info.pMapEntries = spec_entries;
	frag_spec_info.dataSize = sizeof( spec_data );
	frag_spec_info.pData = spec_data;
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
	/* Reversed-Z main pass uses GREATER_OR_EQUAL; OIT must match (near = high depth). */
	if ( vk_get_main_rasterization_samples() == VK_SAMPLE_COUNT_1_BIT ) {
		depth_stencil.depthTestEnable = VK_TRUE;
		depth_stencil.depthWriteEnable = VK_FALSE;
		depth_stencil.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
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

	/* Additive / particle bucket: accumulate color, do not multiply revealage.
	 * ONE/ONE materials must not occlude the opaque background (black holes + stipple). */
	blend_attachments[1].blendEnable = VK_FALSE;
	blend_attachments[1].colorWriteMask = 0;
	if ( qvkCreateGraphicsPipelines( vk.device, vk.pipelineCache, 1, &create_info, NULL, &vk.oit_accum_additive_pipeline ) == VK_SUCCESS ) {
		SET_OBJECT_NAME( vk.oit_accum_additive_pipeline, "pipeline - oit accum additive", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
		ri.Printf( PRINT_ALL, "[VK] OIT additive accum pipeline created (no revealage occlusion)\n" );
	}
}

void vk_create_oit_moments_pipeline( void )
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

	if ( vk.pipeline_layout_oit_moments == VK_NULL_HANDLE || vk.render_pass.oit_moments == VK_NULL_HANDLE ||
		vk.modules.oit_accum_vs == VK_NULL_HANDLE || vk.modules.oit_moments_fs == VK_NULL_HANDLE ) {
		return;
	}

	if ( vk.oit_moments_pipeline != VK_NULL_HANDLE ) {
		vk_wait_idle();
		qvkDestroyPipeline( vk.device, vk.oit_moments_pipeline, NULL );
		vk.oit_moments_pipeline = VK_NULL_HANDLE;
	}

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
	shader_stages[1].module = vk.modules.oit_moments_fs;
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
	if ( vk_get_main_rasterization_samples() == VK_SAMPLE_COUNT_1_BIT ) {
		depth_stencil.depthTestEnable = VK_TRUE;
		depth_stencil.depthWriteEnable = VK_FALSE;
		depth_stencil.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
	} else {
		depth_stencil.depthTestEnable = VK_FALSE;
		depth_stencil.depthWriteEnable = VK_FALSE;
	}
	depth_stencil.depthBoundsTestEnable = VK_FALSE;
	depth_stencil.stencilTestEnable = VK_FALSE;

	/* RT0 + RT1: additive moment accumulation. */
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
	blend_attachments[1].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	blend_attachments[1].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	blend_attachments[1].colorBlendOp = VK_BLEND_OP_ADD;
	blend_attachments[1].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	blend_attachments[1].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
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
	create_info.layout = vk.pipeline_layout_oit_moments;
	create_info.renderPass = vk.render_pass.oit_moments;
	create_info.subpass = 0;

	if ( qvkCreateGraphicsPipelines( vk.device, vk.pipelineCache, 1, &create_info, NULL, &vk.oit_moments_pipeline ) == VK_SUCCESS ) {
		SET_OBJECT_NAME( vk.oit_moments_pipeline, "pipeline - oit moments", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
		ri.Printf( PRINT_ALL, "[VK] OIT moments pipeline created (MBOIT pass 1)\n" );
	}
}

void vk_create_oit_accum_mboit_pipeline( void )
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
	VkSpecializationMapEntry spec_entries[2];
	VkSpecializationInfo frag_spec_info;
	int manual_depth_test = vk.msaaActive ? 1 : 0;
	int forward_plus_lit = 0;
	int spec_data[2];
	VkViewport viewport = { 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f };
	VkRect2D scissor = { { 0, 0 }, { 1, 1 } };

	if ( vk.pipeline_layout_oit_accum_mboit == VK_NULL_HANDLE || vk.render_pass.oit_accum == VK_NULL_HANDLE ||
		vk.modules.oit_accum_vs == VK_NULL_HANDLE || vk.modules.oit_accum_mboit_fs == VK_NULL_HANDLE ) {
		return;
	}

	if ( r_oitForwardPlus && r_oitForwardPlus->integer &&
		vk.set_layout_forward_plus != VK_NULL_HANDLE ) {
		if ( vk_forward_plus_get_graphics_descriptor_set() != VK_NULL_HANDLE ) {
			forward_plus_lit = 1;
		}
	}

	if ( vk.oit_accum_mboit_pipeline != VK_NULL_HANDLE ) {
		vk_wait_idle();
		qvkDestroyPipeline( vk.device, vk.oit_accum_mboit_pipeline, NULL );
		vk.oit_accum_mboit_pipeline = VK_NULL_HANDLE;
	}

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
	shader_stages[1].module = vk.modules.oit_accum_mboit_fs;
	shader_stages[1].pName = "main";
	spec_data[0] = manual_depth_test;
	spec_data[1] = forward_plus_lit;
	spec_entries[0].constantID = 0;
	spec_entries[0].offset = 0;
	spec_entries[0].size = sizeof( int );
	spec_entries[1].constantID = 1;
	spec_entries[1].offset = sizeof( int );
	spec_entries[1].size = sizeof( int );
	frag_spec_info.mapEntryCount = 2;
	frag_spec_info.pMapEntries = spec_entries;
	frag_spec_info.dataSize = sizeof( spec_data );
	frag_spec_info.pData = spec_data;
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
	if ( vk_get_main_rasterization_samples() == VK_SAMPLE_COUNT_1_BIT ) {
		depth_stencil.depthTestEnable = VK_TRUE;
		depth_stencil.depthWriteEnable = VK_FALSE;
		depth_stencil.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
	} else {
		depth_stencil.depthTestEnable = VK_FALSE;
		depth_stencil.depthWriteEnable = VK_FALSE;
	}
	depth_stencil.depthBoundsTestEnable = VK_FALSE;
	depth_stencil.stencilTestEnable = VK_FALSE;

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
	create_info.layout = vk.pipeline_layout_oit_accum_mboit;
	create_info.renderPass = vk.render_pass.oit_accum;
	create_info.subpass = 0;

	if ( qvkCreateGraphicsPipelines( vk.device, vk.pipelineCache, 1, &create_info, NULL, &vk.oit_accum_mboit_pipeline ) == VK_SUCCESS ) {
		SET_OBJECT_NAME( vk.oit_accum_mboit_pipeline, "pipeline - oit accum mboit", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
		ri.Printf( PRINT_ALL, "[VK] OIT accum MBOIT pipeline created (MBOIT pass 2%s)\n",
			forward_plus_lit ? " + Forward+ lit" : "" );
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
	vk_set_shader_stage_desc( shader_stages+0, VK_SHADER_STAGE_VERTEX_BIT, vk.modules.gamma_vs, "main" );
	vk_set_shader_stage_desc( shader_stages+1, VK_SHADER_STAGE_FRAGMENT_BIT, vk.modules.blur_fs, "main" );

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
