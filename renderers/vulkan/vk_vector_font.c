/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Vulkan Loop & Blinn glyphlet pipelines and draw (r_vectorFontMode 2).
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_vector_font.h"
#include "vk_cmd.h"
#include "vk_staging.h"
#include "vk_util.h"
#include "vk_view_state.h"
#include "vk_scene_pass.h"
#include "../common/tr_vector_font.h"
#include "../common/tr_vector_font_glyphlet.h"
#include "q_utf8.h"
#include <stddef.h>

#define VF_MAX_STRING_CHARS 256

typedef struct {
	float pos[2];
	float canonU;
	float canonV;
	float triType;
} vfDrawVert_t;

typedef struct {
	vec4_t color;
	vec4_t originScale;
} vfGlyphPush_t;

typedef struct {
	float posX;
	float posY;
	float scale;
	uint32_t codePoint;
	float color[4];
} vfCharRenderInfo_t;

typedef struct {
	VkPipeline          pipelineVert;
	VkPipeline          pipelineMesh;
	VkPipelineLayout    layoutVert;
	VkPipelineLayout    layoutMesh;
	VkDescriptorSetLayout descMesh;
	VkDescriptorPool    descPoolMesh;
	VkDescriptorSet     descSetMesh;
	VkBuffer            vertBuffer;
	VkDeviceMemory      vertMemory;
	VkBuffer            meshVertBuffer;
	VkDeviceMemory      meshVertMemory;
	VkBuffer            meshIndexBuffer;
	VkDeviceMemory      meshIndexMemory;
	VkBuffer            meshPrimBuffer;
	VkDeviceMemory      meshPrimMemory;
	VkBuffer            meshGlyphTableBuffer;
	VkDeviceMemory      meshGlyphTableMemory;
	VkBuffer            charInfoBuffer;
	VkDeviceMemory      charInfoMemory;
	uint32_t            drawVertCount;
	uint32_t            meshVertCount;
	vectorGlyphletInfo_t gpuGlyphs[GLYPHS_PER_FONT];
	qboolean            gpuReady;
} vfState_t;

static vfState_t vf;

static qboolean VF_CreateBuffer( VkDeviceSize size, VkBufferUsageFlags usage,
	VkBuffer *outBuf, VkDeviceMemory *outMem ) {
	VkBufferCreateInfo bci;
	VkMemoryRequirements req;
	VkMemoryAllocateInfo ai;
	uint32_t memType;

	Com_Memset( &bci, 0, sizeof( bci ) );
	bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bci.size = size;
	bci.usage = usage;
	bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK( qvkCreateBuffer( vk.device, &bci, NULL, outBuf ) );
	qvkGetBufferMemoryRequirements( vk.device, *outBuf, &req );
	memType = vk_find_memory_type( vk.physical_device, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	Com_Memset( &ai, 0, sizeof( ai ) );
	ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ai.allocationSize = req.size;
	ai.memoryTypeIndex = memType;
	VK_CHECK( qvkAllocateMemory( vk.device, &ai, NULL, outMem ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, *outBuf, *outMem, 0 ) );
	return qtrue;
}

static void VF_DestroyBuffer( VkBuffer *buf, VkDeviceMemory *mem ) {
	if ( *buf != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, *buf, NULL );
		*buf = VK_NULL_HANDLE;
	}
	if ( *mem != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, *mem, NULL );
		*mem = VK_NULL_HANDLE;
	}
}

static void VF_DestroyPipeline( VkPipeline *pipe ) {
	if ( *pipe != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, *pipe, NULL );
		*pipe = VK_NULL_HANDLE;
	}
}

static qboolean VF_CreateVertPipeline( void ) {
	VkShaderModule vs;
	VkShaderModule fs;
	VkPipelineShaderStageCreateInfo stages[2];
	VkPipelineVertexInputStateCreateInfo vertexInput;
	VkVertexInputBindingDescription bind;
	VkVertexInputAttributeDescription attrs[3];
	VkPipelineInputAssemblyStateCreateInfo inputAssembly;
	VkPipelineViewportStateCreateInfo viewportState;
	VkPipelineRasterizationStateCreateInfo raster;
	VkPipelineMultisampleStateCreateInfo msaa;
	VkPipelineDepthStencilStateCreateInfo depth;
	VkPipelineColorBlendAttachmentState blendAtt[2];
	VkPipelineColorBlendStateCreateInfo blend;
	VkPipelineDynamicStateCreateInfo dynamic;
	VkDynamicState dynStates[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkGraphicsPipelineCreateInfo gp;
	VkPushConstantRange pushRange;
	VkPipelineLayoutCreateInfo layoutDesc;

	if ( vk.modules.frag.ui_vector_glyphlet_vert == VK_NULL_HANDLE ||
		vk.modules.frag.ui_vector_glyphlet_frag == VK_NULL_HANDLE ) {
		return qfalse;
	}

	vs = vk.modules.frag.ui_vector_glyphlet_vert;
	fs = vk.modules.frag.ui_vector_glyphlet_frag;

	Com_Memset( stages, 0, sizeof( stages ) );
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = vs;
	stages[0].pName = "main";
	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = fs;
	stages[1].pName = "main";

	bind.binding = 0;
	bind.stride = sizeof( vfDrawVert_t );
	bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	attrs[0].location = 0;
	attrs[0].binding = 0;
	attrs[0].format = VK_FORMAT_R32G32_SFLOAT;
	attrs[0].offset = offsetof( vfDrawVert_t, pos );
	attrs[1].location = 1;
	attrs[1].binding = 0;
	attrs[1].format = VK_FORMAT_R32G32_SFLOAT;
	attrs[1].offset = offsetof( vfDrawVert_t, canonU );
	attrs[2].location = 2;
	attrs[2].binding = 0;
	attrs[2].format = VK_FORMAT_R32_SFLOAT;
	attrs[2].offset = offsetof( vfDrawVert_t, triType );

	Com_Memset( &vertexInput, 0, sizeof( vertexInput ) );
	vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInput.vertexBindingDescriptionCount = 1;
	vertexInput.pVertexBindingDescriptions = &bind;
	vertexInput.vertexAttributeDescriptionCount = 3;
	vertexInput.pVertexAttributeDescriptions = attrs;

	Com_Memset( &inputAssembly, 0, sizeof( inputAssembly ) );
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	Com_Memset( &viewportState, 0, sizeof( viewportState ) );
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	Com_Memset( &raster, 0, sizeof( raster ) );
	raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	raster.polygonMode = VK_POLYGON_MODE_FILL;
	raster.cullMode = VK_CULL_MODE_NONE;
	raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	raster.lineWidth = 1.0f;

	Com_Memset( &msaa, 0, sizeof( msaa ) );
	msaa.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	msaa.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	Com_Memset( &depth, 0, sizeof( depth ) );
	depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

	Com_Memset( blendAtt, 0, sizeof( blendAtt ) );
	blendAtt[0].blendEnable = VK_TRUE;
	blendAtt[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	blendAtt[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blendAtt[0].colorBlendOp = VK_BLEND_OP_ADD;
	blendAtt[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	blendAtt[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blendAtt[0].alphaBlendOp = VK_BLEND_OP_ADD;
	blendAtt[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	blendAtt[1].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT;

	Com_Memset( &blend, 0, sizeof( blend ) );
	blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend.attachmentCount = vk.fboActive ? 2 : 1;
	blend.pAttachments = blendAtt;

	Com_Memset( &dynamic, 0, sizeof( dynamic ) );
	dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic.dynamicStateCount = 2;
	dynamic.pDynamicStates = dynStates;

	pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	pushRange.offset = 0;
	pushRange.size = sizeof( float ) * 40; /* 2 mat4 + 2 vec4 */

	Com_Memset( &layoutDesc, 0, sizeof( layoutDesc ) );
	layoutDesc.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutDesc.pushConstantRangeCount = 1;
	layoutDesc.pPushConstantRanges = &pushRange;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &layoutDesc, NULL, &vf.layoutVert ) );

	Com_Memset( &gp, 0, sizeof( gp ) );
	gp.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	gp.stageCount = 2;
	gp.pStages = stages;
	gp.pVertexInputState = &vertexInput;
	gp.pInputAssemblyState = &inputAssembly;
	gp.pViewportState = &viewportState;
	gp.pRasterizationState = &raster;
	gp.pMultisampleState = &msaa;
	gp.pDepthStencilState = &depth;
	gp.pColorBlendState = &blend;
	gp.pDynamicState = &dynamic;
	gp.layout = vf.layoutVert;
	gp.renderPass = vk.render_pass.ui_overlay != VK_NULL_HANDLE ? vk.render_pass.ui_overlay : vk.render_pass.main;
	gp.subpass = 0;
	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, vk.pipelineCache, 1, &gp, NULL, &vf.pipelineVert ) );

	return qtrue;
}

static qboolean VF_CreateMeshPipeline( void ) {
	/* NV mesh SPIR-V compile is optional (see compile_shaders.sh); wire when glslang supports custom varyings. */
	return qfalse;
}

#if 0 /* NV mesh pipeline — enable when mesh_nv_ui_vector_font.mesh compiles */
static qboolean VF_CreateMeshPipeline_DISABLED( void ) {
	VkDescriptorSetLayoutBinding binds[5];
	VkDescriptorSetLayoutCreateInfo layoutInfo;
	VkPushConstantRange pushRange;
	VkPipelineLayoutCreateInfo pipeLayoutInfo;
	VkShaderModule ms;
	VkShaderModule fs;
	VkPipelineShaderStageCreateInfo stages[2];
	VkPipelineViewportStateCreateInfo viewportState;
	VkPipelineRasterizationStateCreateInfo raster;
	VkPipelineMultisampleStateCreateInfo msaa;
	VkPipelineDepthStencilStateCreateInfo depth;
	VkPipelineColorBlendAttachmentState blendAtt[2];
	VkPipelineColorBlendStateCreateInfo blend;
	VkPipelineDynamicStateCreateInfo dynamic;
	VkDynamicState dynStates[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkGraphicsPipelineCreateInfo gp;
	VkDescriptorPoolSize poolSize;
	VkDescriptorPoolCreateInfo poolInfo;
	VkDescriptorSetAllocateInfo allocInfo;

	extern const unsigned char mesh_nv_ui_vector_font_mesh_spv[];
	extern const unsigned char frag_ui_vector_glyphlet_frag_spv[];

	if ( !vk.meshShaderNV || !qvkCmdDrawMeshTasksNV ) {
		return qfalse;
	}

	Com_Memset( binds, 0, sizeof( binds ) );
	for ( int i = 0; i < 5; i++ ) {
		binds[i].binding = (uint32_t)i;
		binds[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		binds[i].descriptorCount = 1;
		binds[i].stageFlags = VK_SHADER_STAGE_MESH_BIT_NV;
	}
	binds[4].stageFlags = VK_SHADER_STAGE_MESH_BIT_NV;

	Com_Memset( &layoutInfo, 0, sizeof( layoutInfo ) );
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 5;
	layoutInfo.pBindings = binds;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layoutInfo, NULL, &vf.descMesh ) );

	pushRange.stageFlags = VK_SHADER_STAGE_MESH_BIT_NV | VK_SHADER_STAGE_FRAGMENT_BIT;
	pushRange.offset = 0;
	pushRange.size = sizeof( float ) * 32;

	Com_Memset( &pipeLayoutInfo, 0, sizeof( pipeLayoutInfo ) );
	pipeLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeLayoutInfo.setLayoutCount = 1;
	pipeLayoutInfo.pSetLayouts = &vf.descMesh;
	pipeLayoutInfo.pushConstantRangeCount = 1;
	pipeLayoutInfo.pPushConstantRanges = &pushRange;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pipeLayoutInfo, NULL, &vf.layoutMesh ) );

	ms = VF_ModuleFromSpv( mesh_nv_ui_vector_font_mesh_spv, (int)sizeof( mesh_nv_ui_vector_font_mesh_spv ) );
	fs = VF_ModuleFromSpv( frag_ui_vector_glyphlet_frag_spv, (int)sizeof( frag_ui_vector_glyphlet_frag_spv ) );

	Com_Memset( stages, 0, sizeof( stages ) );
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_MESH_BIT_NV;
	stages[0].module = ms;
	stages[0].pName = "main";
	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = fs;
	stages[1].pName = "main";

	Com_Memset( &viewportState, 0, sizeof( viewportState ) );
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	Com_Memset( &raster, 0, sizeof( raster ) );
	raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	raster.polygonMode = VK_POLYGON_MODE_FILL;
	raster.cullMode = VK_CULL_MODE_NONE;
	raster.lineWidth = 1.0f;

	Com_Memset( &msaa, 0, sizeof( msaa ) );
	msaa.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	msaa.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	Com_Memset( &depth, 0, sizeof( depth ) );
	depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

	Com_Memset( blendAtt, 0, sizeof( blendAtt ) );
	blendAtt[0].blendEnable = VK_TRUE;
	blendAtt[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	blendAtt[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blendAtt[0].colorBlendOp = VK_BLEND_OP_ADD;
	blendAtt[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	blendAtt[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blendAtt[0].alphaBlendOp = VK_BLEND_OP_ADD;
	blendAtt[0].colorWriteMask = 0xF;
	blendAtt[1].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT;

	Com_Memset( &blend, 0, sizeof( blend ) );
	blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend.attachmentCount = vk.fboActive ? 2 : 1;
	blend.pAttachments = blendAtt;

	Com_Memset( &dynamic, 0, sizeof( dynamic ) );
	dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic.dynamicStateCount = 2;
	dynamic.pDynamicStates = dynStates;

	Com_Memset( &gp, 0, sizeof( gp ) );
	gp.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	gp.stageCount = 2;
	gp.pStages = stages;
	gp.pViewportState = &viewportState;
	gp.pRasterizationState = &raster;
	gp.pMultisampleState = &msaa;
	gp.pDepthStencilState = &depth;
	gp.pColorBlendState = &blend;
	gp.pDynamicState = &dynamic;
	gp.layout = vf.layoutMesh;
	gp.renderPass = vk.render_pass.ui_overlay != VK_NULL_HANDLE ? vk.render_pass.ui_overlay : vk.render_pass.main;
	gp.subpass = 0;
	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, vk.pipelineCache, 1, &gp, NULL, &vf.pipelineMesh ) );

	qvkDestroyShaderModule( vk.device, ms, NULL );
	qvkDestroyShaderModule( vk.device, fs, NULL );

	Com_Memset( &poolSize, 0, sizeof( poolSize ) );
	poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSize.descriptorCount = 5;
	Com_Memset( &poolInfo, 0, sizeof( poolInfo ) );
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;
	poolInfo.maxSets = 1;
	VK_CHECK( qvkCreateDescriptorPool( vk.device, &poolInfo, NULL, &vf.descPoolMesh ) );

	Com_Memset( &allocInfo, 0, sizeof( allocInfo ) );
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = vf.descPoolMesh;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &vf.descMesh;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &allocInfo, &vf.descSetMesh ) );

	return qtrue;
}
#endif

static void VF_UpdateMeshDescriptors( void ) {
	VkWriteDescriptorSet writes[5];
	VkDescriptorBufferInfo infos[5];

	if ( vf.descSetMesh == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( infos, 0, sizeof( infos ) );
	infos[0].buffer = vf.meshVertBuffer;
	infos[0].offset = 0;
	infos[0].range = VK_WHOLE_SIZE;
	infos[1].buffer = vf.meshIndexBuffer;
	infos[1].offset = 0;
	infos[1].range = VK_WHOLE_SIZE;
	infos[2].buffer = vf.meshPrimBuffer;
	infos[2].offset = 0;
	infos[2].range = VK_WHOLE_SIZE;
	infos[3].buffer = vf.meshGlyphTableBuffer;
	infos[3].offset = 0;
	infos[3].range = VK_WHOLE_SIZE;
	infos[4].buffer = vf.charInfoBuffer;
	infos[4].offset = 0;
	infos[4].range = VK_WHOLE_SIZE;

	Com_Memset( writes, 0, sizeof( writes ) );
	for ( int i = 0; i < 5; i++ ) {
		writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[i].dstSet = vf.descSetMesh;
		writes[i].dstBinding = (uint32_t)i;
		writes[i].descriptorCount = 1;
		writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[i].pBufferInfo = &infos[i];
	}
	qvkUpdateDescriptorSets( vk.device, 5, writes, 0, NULL );
}

void VK_VectorFont_Init( void ) {
	Com_Memset( &vf, 0, sizeof( vf ) );
	if ( !vk.device ) {
		return;
	}
	if ( !VF_CreateVertPipeline() ) {
		ri.Printf( PRINT_WARNING, "VectorFont mode 2: failed to create glyphlet pipeline\n" );
	}
	if ( VF_CreateMeshPipeline() ) {
		ri.Printf( PRINT_ALL, "VectorFont mode 2: NV mesh-shader string pipeline ready (r_vk_meshShaderNV 1)\n" );
	}
}

void VK_VectorFont_ClearGpu( void ) {
	VF_DestroyBuffer( &vf.vertBuffer, &vf.vertMemory );
	VF_DestroyBuffer( &vf.meshVertBuffer, &vf.meshVertMemory );
	VF_DestroyBuffer( &vf.meshIndexBuffer, &vf.meshIndexMemory );
	VF_DestroyBuffer( &vf.meshPrimBuffer, &vf.meshPrimMemory );
	VF_DestroyBuffer( &vf.meshGlyphTableBuffer, &vf.meshGlyphTableMemory );
	vf.drawVertCount = 0;
	vf.meshVertCount = 0;
	vf.gpuReady = qfalse;
	Com_Memset( vf.gpuGlyphs, 0, sizeof( vf.gpuGlyphs ) );
}

void VK_VectorFont_Shutdown( void ) {
	VK_VectorFont_ClearGpu();
	VF_DestroyBuffer( &vf.charInfoBuffer, &vf.charInfoMemory );
	VF_DestroyPipeline( &vf.pipelineVert );
	VF_DestroyPipeline( &vf.pipelineMesh );
	if ( vf.layoutVert != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vf.layoutVert, NULL );
		vf.layoutVert = VK_NULL_HANDLE;
	}
	if ( vf.layoutMesh != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vf.layoutMesh, NULL );
		vf.layoutMesh = VK_NULL_HANDLE;
	}
	if ( vf.descPoolMesh != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, vf.descPoolMesh, NULL );
		vf.descPoolMesh = VK_NULL_HANDLE;
	}
	if ( vf.descMesh != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vf.descMesh, NULL );
		vf.descMesh = VK_NULL_HANDLE;
	}
	vf.descSetMesh = VK_NULL_HANDLE;
}

static qboolean VF_UploadBuffer( const void *data, VkDeviceSize size, VkBufferUsageFlags usage,
	VkBuffer *outBuf, VkDeviceMemory *outMem ) {
	VkCommandBuffer cmd;
	VkBufferCopy region;

	if ( !data || size == 0 ) {
		return qfalse;
	}
	if ( !vk.staging_buffer.ptr || size > vk.staging_buffer.size ) {
		return qfalse;
	}
	if ( !VF_CreateBuffer( size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage, outBuf, outMem ) ) {
		return qfalse;
	}
	Com_Memcpy( vk.staging_buffer.ptr, data, (size_t)size );
	cmd = vk_begin_command_buffer();
	region.srcOffset = 0;
	region.dstOffset = 0;
	region.size = size;
	qvkCmdCopyBuffer( cmd, vk.staging_buffer.handle, *outBuf, 1, &region );
	vk_end_command_buffer( cmd, __func__ );
	return qtrue;
}

qboolean VK_VectorFont_UploadAtlas( vectorGlyphletAtlas_t *atlas, vectorFontGlyph_t *glyphs ) {
	vfDrawVert_t *drawVerts;
	uint32_t drawCount;
	uint32_t expandedBase;
	int ch;
	int p;
	int i;
	float *meshVerts;
	uint32_t *meshPrimU32;
	VkDeviceSize charBufSize;

	if ( !atlas || !glyphs || !vk.device ) {
		return qfalse;
	}

	VK_VectorFont_ClearGpu();

	drawCount = 0;
	for ( ch = GLYPH_START; ch <= GLYPH_END; ch++ ) {
		if ( glyphs[ch].valid ) {
			drawCount += glyphs[ch].glyphlet.primitiveCount * 3u;
		}
	}
	if ( drawCount == 0 ) {
		return qfalse;
	}

	drawVerts = (vfDrawVert_t *)ri.Hunk_AllocateTempMemory( (int)( drawCount * sizeof( *drawVerts ) ) );
	if ( !drawVerts ) {
		return qfalse;
	}

	expandedBase = 0;
	for ( ch = GLYPH_START; ch <= GLYPH_END; ch++ ) {
		const vectorFontGlyph_t *g = &glyphs[ch];
		vectorGlyphletInfo_t atlasInfo;
		vectorGlyphletInfo_t *gi = &vf.gpuGlyphs[ch];

		if ( !g->valid || g->glyphlet.primitiveCount == 0 ) {
			Com_Memset( gi, 0, sizeof( *gi ) );
			continue;
		}

		atlasInfo = g->glyphlet;
		*gi = atlasInfo;
		glyphs[ch].glyphlet.vertexBaseIndex = expandedBase;

		for ( p = 0; p < (int)atlasInfo.primitiveCount; p++ ) {
			uint32_t ib = atlasInfo.triangleBaseIndex + (uint32_t)p * 3u;
			uint8_t triType = atlas->primTypes[atlasInfo.primBaseIndex + (uint32_t)p];
			for ( i = 0; i < 3; i++ ) {
				uint32_t vi = atlas->indices[ib + (uint32_t)i];
				const vectorGlyphletVert_t *src = &atlas->verts[vi];
				vfDrawVert_t *dst = &drawVerts[expandedBase++];

				dst->pos[0] = src->x;
				dst->pos[1] = src->y;
				dst->canonU = src->canonU;
				dst->canonV = src->canonV;
				dst->triType = (float)triType;
			}
		}
		glyphs[ch].glyphlet.vertexCount = atlasInfo.primitiveCount * 3u;
	}

	if ( !VF_UploadBuffer( drawVerts, (VkDeviceSize)drawCount * sizeof( *drawVerts ),
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &vf.vertBuffer, &vf.vertMemory ) ) {
		ri.Hunk_FreeTempMemory( drawVerts );
		return qfalse;
	}
	vf.drawVertCount = drawCount;
	ri.Hunk_FreeTempMemory( drawVerts );

	meshVerts = (float *)ri.Hunk_AllocateTempMemory( (int)( atlas->vertCount * 4 * sizeof( float ) ) );
	if ( meshVerts ) {
		for ( i = 0; i < (int)atlas->vertCount; i++ ) {
			meshVerts[i * 4 + 0] = atlas->verts[i].x;
			meshVerts[i * 4 + 1] = atlas->verts[i].y;
			meshVerts[i * 4 + 2] = atlas->verts[i].canonU;
			meshVerts[i * 4 + 3] = atlas->verts[i].canonV;
		}
		if ( VF_UploadBuffer( meshVerts, (VkDeviceSize)atlas->vertCount * 4 * sizeof( float ),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &vf.meshVertBuffer, &vf.meshVertMemory ) ) {
			vf.meshVertCount = atlas->vertCount;
		}
		ri.Hunk_FreeTempMemory( meshVerts );
	}

	if ( atlas->indexCount > 0 ) {
		VF_UploadBuffer( atlas->indices, (VkDeviceSize)atlas->indexCount * sizeof( uint32_t ),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &vf.meshIndexBuffer, &vf.meshIndexMemory );
	}

	if ( atlas->primCount > 0 ) {
		meshPrimU32 = (uint32_t *)ri.Hunk_AllocateTempMemory( (int)( atlas->primCount * sizeof( uint32_t ) ) );
		if ( meshPrimU32 ) {
			for ( i = 0; i < (int)atlas->primCount; i++ ) {
				meshPrimU32[i] = (uint32_t)atlas->primTypes[i];
			}
			VF_UploadBuffer( meshPrimU32, (VkDeviceSize)atlas->primCount * sizeof( uint32_t ),
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &vf.meshPrimBuffer, &vf.meshPrimMemory );
			ri.Hunk_FreeTempMemory( meshPrimU32 );
		}
	}

	VF_UploadBuffer( vf.gpuGlyphs, sizeof( vf.gpuGlyphs ),
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &vf.meshGlyphTableBuffer, &vf.meshGlyphTableMemory );

	charBufSize = (VkDeviceSize)VF_MAX_STRING_CHARS * sizeof( vfCharRenderInfo_t );
	if ( !vf.charInfoBuffer ) {
		VF_CreateBuffer( charBufSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			&vf.charInfoBuffer, &vf.charInfoMemory );
	}
	VF_UpdateMeshDescriptors();

	vf.gpuReady = qtrue;
	return qtrue;
}

qboolean VK_VectorFont_MeshReady( void ) {
	return vf.gpuReady && vf.pipelineMesh != VK_NULL_HANDLE && qvkCmdDrawMeshTasksNV != NULL;
}

static void VF_Push2DState( void ) {
	VkViewport viewport;
	VkRect2D scissor;
	const uint32_t targetWidth = vk_get_render_target_width();
	const uint32_t targetHeight = vk_get_render_target_height();

	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)targetWidth;
	viewport.height = (float)targetHeight;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = targetWidth;
	scissor.extent.height = targetHeight;
	qvkCmdSetViewport( vk.cmd->command_buffer, 0, 1, &viewport );
	qvkCmdSetScissor( vk.cmd->command_buffer, 0, 1, &scissor );
}

static void VF_DrawGlyphVert( float x, float y, float scale, const vectorFontGlyph_t *g,
	const float *color, qboolean shadowPass, float shadowOff ) {
	uint8_t pc[160];
	float mvp[16];
	float prevMvp[16];
	VkDeviceSize offset;

	if ( !vf.gpuReady || vf.pipelineVert == VK_NULL_HANDLE || !g || !g->valid ) {
		return;
	}
	if ( g->glyphlet.primitiveCount == 0 || g->glyphlet.vertexCount == 0 ) {
		return;
	}

	vk_read_mvp_transform( mvp );
	vk_read_prev_mvp_transform( prevMvp );

	Com_Memset( pc, 0, sizeof( pc ) );
	Com_Memcpy( pc, mvp, 64 );
	Com_Memcpy( pc + 64, prevMvp, 64 );
	if ( color ) {
		Com_Memcpy( pc + 128, color, 4 * sizeof( float ) );
	} else {
		((float *)( pc + 128 ))[0] = backEnd.color2D.rgba[0] / 255.0f;
		((float *)( pc + 128 ))[1] = backEnd.color2D.rgba[1] / 255.0f;
		((float *)( pc + 128 ))[2] = backEnd.color2D.rgba[2] / 255.0f;
		((float *)( pc + 128 ))[3] = backEnd.color2D.rgba[3] / 255.0f;
	}
	if ( shadowPass ) {
		((float *)( pc + 128 ))[0] = 0.0f;
		((float *)( pc + 128 ))[1] = 0.0f;
		((float *)( pc + 128 ))[2] = 0.0f;
	}
	((float *)( pc + 144 ))[0] = shadowPass ? x + shadowOff : x;
	((float *)( pc + 144 ))[1] = shadowPass ? y + shadowOff : y;
	((float *)( pc + 144 ))[2] = scale;

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vf.pipelineVert );
	qvkCmdPushConstants( vk.cmd->command_buffer, vf.layoutVert,
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( pc ), pc );

	offset = (VkDeviceSize)g->glyphlet.vertexBaseIndex * sizeof( vfDrawVert_t );
	qvkCmdBindVertexBuffers( vk.cmd->command_buffer, 0, 1, &vf.vertBuffer, &offset );
	qvkCmdDraw( vk.cmd->command_buffer, g->glyphlet.vertexCount, 1, 0, 0 );
}

static qboolean VF_DrawStringMesh( float x, float y, float scale, const char *text, const float *color ) {
	vfCharRenderInfo_t chars[VF_MAX_STRING_CHARS];
	float mvp[16];
	float prevMvp[16];
	uint8_t pc[128];
	int count;
	const char *s;
	float xx;
	float yy;

	if ( !VK_VectorFont_MeshReady() || !text ) {
		return qfalse;
	}

	count = 0;
	s = text;
	xx = x;
	yy = y;
	while ( *s && count < VF_MAX_STRING_CHARS ) {
		uint32_t cp = Q_UTF8_Decode( &s );
		const vectorFontGlyph_t *g;

		if ( cp == '\n' ) {
			xx = x;
			yy += scale * 48.0f;
			continue;
		}
		g = R_VectorFont_GetGlyph( (int)cp );
		if ( !g || !g->valid || g->glyphlet.primitiveCount == 0 ) {
			continue;
		}

		chars[count].posX = xx;
		chars[count].posY = yy;
		chars[count].scale = scale;
		chars[count].codePoint = cp;
		if ( color ) {
			Com_Memcpy( chars[count].color, color, 4 * sizeof( float ) );
		} else {
			chars[count].color[0] = backEnd.color2D.rgba[0] / 255.0f;
			chars[count].color[1] = backEnd.color2D.rgba[1] / 255.0f;
			chars[count].color[2] = backEnd.color2D.rgba[2] / 255.0f;
			chars[count].color[3] = backEnd.color2D.rgba[3] / 255.0f;
		}
		xx += g->xAdvance * scale;
		count++;
	}

	if ( count <= 0 ) {
		return qfalse;
	}

	if ( !vf.charInfoBuffer ) {
		VF_CreateBuffer( (VkDeviceSize)VF_MAX_STRING_CHARS * sizeof( vfCharRenderInfo_t ),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			&vf.charInfoBuffer, &vf.charInfoMemory );
	}
	if ( !vf.charInfoBuffer || !vk.staging_buffer.ptr ||
		(VkDeviceSize)count * sizeof( vfCharRenderInfo_t ) > vk.staging_buffer.size ) {
		return qfalse;
	}
	Com_Memcpy( vk.staging_buffer.ptr, chars, (size_t)count * sizeof( vfCharRenderInfo_t ) );
	{
		VkCommandBuffer cmd = vk_begin_command_buffer();
		VkBufferCopy region;
		region.srcOffset = 0;
		region.dstOffset = 0;
		region.size = (VkDeviceSize)count * sizeof( vfCharRenderInfo_t );
		qvkCmdCopyBuffer( cmd, vk.staging_buffer.handle, vf.charInfoBuffer, 1, &region );
		vk_end_command_buffer( cmd, __func__ );
	}
	VF_UpdateMeshDescriptors();

	vk_read_mvp_transform( mvp );
	vk_read_prev_mvp_transform( prevMvp );
	Com_Memcpy( pc, mvp, 64 );
	Com_Memcpy( pc + 64, prevMvp, 64 );

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vf.pipelineMesh );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vf.layoutMesh, 0, 1, &vf.descSetMesh, 0, NULL );
	qvkCmdPushConstants( vk.cmd->command_buffer, vf.layoutMesh,
		VK_SHADER_STAGE_MESH_BIT_NV | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 128, pc );
	qvkCmdDrawMeshTasksNV( vk.cmd->command_buffer, (uint32_t)count, 0 );
	return qtrue;
}

qboolean VK_VectorFont_DrawString( float x, float y, float scale, const char *text,
	const float *color, float shadowOff ) {
	const char *s;
	float xx;
	float yy;
	const vectorFontGlyph_t *g;

	if ( !vf.gpuReady || !text || scale <= 0.0f ) {
		return qfalse;
	}

	VF_Push2DState();

	if ( shadowOff > 0.0f && !VK_VectorFont_MeshReady() ) {
		s = text;
		xx = x;
		yy = y;
		while ( *s ) {
			uint32_t cp = Q_UTF8_Decode( &s );
			if ( cp == '\n' ) {
				xx = x;
				yy += scale * 48.0f;
				continue;
			}
			g = R_VectorFont_GetGlyph( (int)cp );
			VF_DrawGlyphVert( xx, yy, scale, g, color, qtrue, shadowOff );
			if ( g && g->valid ) {
				xx += g->xAdvance * scale;
			}
		}
	}

	if ( VF_DrawStringMesh( x, y, scale, text, color ) ) {
		return qtrue;
	}

	s = text;
	xx = x;
	yy = y;
	while ( *s ) {
		uint32_t cp = Q_UTF8_Decode( &s );
		if ( cp == '\n' ) {
			xx = x;
			yy += scale * 48.0f;
			continue;
		}
		g = R_VectorFont_GetGlyph( (int)cp );
		VF_DrawGlyphVert( xx, yy, scale, g, color, qfalse, 0.0f );
		if ( g && g->valid ) {
			xx += g->xAdvance * scale;
		}
	}
	return qtrue;
}
