/*
=============================================================================
Vulkan Mesh Shaders (VK_EXT_mesh_shader) Implementation

Mesh shaders provide GPU-driven rendering with meshlet-based culling and LOD.
=============================================================================
*/

#include "tr_local.h"
// Renderer import interface - defined in renderer main file
extern refimport_t ri;
#include "vk.h"

// meshlet_info_t is defined in vk.h

#ifdef USE_VULKAN

// Mesh shader function pointer types (VK_EXT_mesh_shader)
typedef void (VKAPI_PTR *PFN_vkCmdDrawMeshTasksEXT)(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    groupCountX,
    uint32_t                                    groupCountY,
    uint32_t                                    groupCountZ);

typedef void (VKAPI_PTR *PFN_vkCmdDrawMeshTasksIndirectEXT)(
    VkCommandBuffer                             commandBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    uint32_t                                    drawCount,
    uint32_t                                    stride);

typedef void (VKAPI_PTR *PFN_vkCmdDrawMeshTasksIndirectCountEXT)(
    VkCommandBuffer                             commandBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    VkBuffer                                    countBuffer,
    VkDeviceSize                                countBufferOffset,
    uint32_t                                    maxDrawCount,
    uint32_t                                    stride);

// Mesh shader stage flags (VK_EXT_mesh_shader)
#ifndef VK_SHADER_STAGE_TASK_BIT_EXT
#define VK_SHADER_STAGE_TASK_BIT_EXT         0x00000040U
#endif
#ifndef VK_SHADER_STAGE_MESH_BIT_EXT
#define VK_SHADER_STAGE_MESH_BIT_EXT         0x00000080U
#endif

// Mesh shader features structure
#ifndef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT 1000328000
typedef struct VkPhysicalDeviceMeshShaderFeaturesEXT {
    VkStructureType    sType;
    void*              pNext;
    VkBool32           taskShader;
    VkBool32           meshShader;
    VkBool32           multiviewMeshShader;
    VkBool32           primitiveFragmentShadingRateMeshShader;
    VkBool32           meshShaderQueries;
} VkPhysicalDeviceMeshShaderFeaturesEXT;
#endif

// Mesh shader function pointers
static PFN_vkCmdDrawMeshTasksEXT qvkCmdDrawMeshTasksEXT = NULL;
static PFN_vkCmdDrawMeshTasksIndirectEXT qvkCmdDrawMeshTasksIndirectEXT = NULL;
static PFN_vkCmdDrawMeshTasksIndirectCountEXT qvkCmdDrawMeshTasksIndirectCountEXT = NULL;

static qboolean meshShadersSupported = qfalse;

static qboolean mesh_shaders_requested( void )
{
	return ( r_meshShaders && r_meshShaders->integer != 0 );
}

static VkShaderModule vk_load_shader_file( const char *path )
{
	FILE *f = fopen( path, "rb" );
	if ( !f ) {
		return VK_NULL_HANDLE;
	}
	fseek( f, 0, SEEK_END );
	long size = ftell( f );
	fseek( f, 0, SEEK_SET );
	if ( size <= 0 ) {
		fclose( f );
		return VK_NULL_HANDLE;
	}
	byte *buf = (byte *)Z_Malloc( size );
	if ( fread( buf, 1, size, f ) != (size_t)size ) {
		fclose( f );
		Z_Free( buf );
		return VK_NULL_HANDLE;
	}
	fclose( f );

	VkShaderModuleCreateInfo createInfo = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = (size_t)size,
		.pCode = (const uint32_t *)buf
	};
	VkShaderModule module = VK_NULL_HANDLE;
	if ( vkCreateShaderModule( vk.device, &createInfo, NULL, &module ) != VK_SUCCESS ) {
		module = VK_NULL_HANDLE;
	}
	Z_Free( buf );
	return module;
}

void vk_mesh_shaders_init( void )
{
	Com_Memset( &vk.mesh, 0, sizeof( vk.mesh ) );
	
	vk.mesh.meshletCapacity = 0;
	vk.mesh.meshlets = NULL;
	vk.mesh.meshletCount = 0;
	vk.mesh.useFallback = qtrue;

	// Check if mesh shader extension is available
	// First, check if extension was enabled during device creation
	// Then try to load the function pointers
	qvkCmdDrawMeshTasksEXT = (PFN_vkCmdDrawMeshTasksEXT)vkGetDeviceProcAddr( vk.device, "vkCmdDrawMeshTasksEXT" );
	qvkCmdDrawMeshTasksIndirectEXT = (PFN_vkCmdDrawMeshTasksIndirectEXT)vkGetDeviceProcAddr( vk.device, "vkCmdDrawMeshTasksIndirectEXT" );
	qvkCmdDrawMeshTasksIndirectCountEXT = (PFN_vkCmdDrawMeshTasksIndirectCountEXT)vkGetDeviceProcAddr( vk.device, "vkCmdDrawMeshTasksIndirectCountEXT" );

	if ( qvkCmdDrawMeshTasksEXT ) {
		meshShadersSupported = qtrue;
		vk.mesh.meshShaderSupported = qtrue;
		vk.mesh.active = mesh_shaders_requested();
		vk.mesh.useFallback = !vk.mesh.active;
		// Try to satisfy module requirement from disk before warning
		if ( vk.mesh.mesh_task == VK_NULL_HANDLE || vk.mesh.mesh_mesh == VK_NULL_HANDLE ) {
			static const char *searchPairs[][2] = {
				{ "shaders/spirv/meshlet.task.spv", "shaders/spirv/meshlet.mesh.spv" },
				{ "./shaders/spirv/meshlet.task.spv", "./shaders/spirv/meshlet.mesh.spv" },
				{ "/home/tim/Desktop/idtech3/shaders/spirv/meshlet.task.spv", "/home/tim/Desktop/idtech3/shaders/spirv/meshlet.mesh.spv" },
				{ "/home/tim/Desktop/idtech3/build/shaders/spirv/meshlet.task.spv", "/home/tim/Desktop/idtech3/build/shaders/spirv/meshlet.mesh.spv" },
				{ "/home/tim/Desktop/idtech3/release/shaders/spirv/meshlet.task.spv", "/home/tim/Desktop/idtech3/release/shaders/spirv/meshlet.mesh.spv" },
			};
			const int searchCount = (int)(sizeof(searchPairs)/sizeof(searchPairs[0]));
			for ( int si = 0; si < searchCount; ++si ) {
				if ( vk.mesh.mesh_task == VK_NULL_HANDLE ) {
					vk.mesh.mesh_task = vk_load_shader_file( searchPairs[si][0] );
				}
				if ( vk.mesh.mesh_mesh == VK_NULL_HANDLE ) {
					vk.mesh.mesh_mesh = vk_load_shader_file( searchPairs[si][1] );
				}
				if ( vk.mesh.mesh_task != VK_NULL_HANDLE && vk.mesh.mesh_mesh != VK_NULL_HANDLE ) {
					ri.Printf( PRINT_DEVELOPER, "Mesh shaders: loaded external modules from %s and %s\n",
						searchPairs[si][0], searchPairs[si][1] );
					break;
				}
			}
		}
		// Verify that required shader modules are present; otherwise stay on fallback.
		if ( vk.mesh.mesh_task == VK_NULL_HANDLE || vk.mesh.mesh_mesh == VK_NULL_HANDLE ) {
			vk.mesh.useFallback = qtrue;
			if ( vk.mesh.active ) {
				ri.Printf( PRINT_WARNING, "Mesh shaders requested but shader modules are missing; fallback enabled\n" );
			}
		}
		
		// Check for task shader support (optional, but recommended)
		// Task shaders are part of the same extension
		vk.mesh.taskShaderSupported = qtrue;
		
		if ( vk.mesh.active ) {
			ri.Printf( PRINT_DEVELOPER, "Mesh shaders: Extension detected, enabled (mesh tasks available)\n" );
		} else {
			ri.Printf( PRINT_DEVELOPER, "Mesh shaders: Extension detected but disabled via cvar, using fallback path\n" );
		}
	} else {
		meshShadersSupported = qfalse;
		vk.mesh.meshShaderSupported = qfalse;
		vk.mesh.taskShaderSupported = qfalse;
		vk.mesh.active = qfalse;
		vk.mesh.useFallback = qtrue;
		ri.Printf( PRINT_DEVELOPER, "Mesh shaders: Not available (extension not enabled or not supported)\n" );
		return;
	}
	
	// Initialize mesh shader pipeline (will be created when shaders are loaded)
	vk.mesh.meshShaderPipeline = VK_NULL_HANDLE;
	vk.mesh.meshShaderPipelineLayout = VK_NULL_HANDLE;
	vk.mesh.meshShaderDescriptorSetLayout = VK_NULL_HANDLE;
	vk.mesh.meshShaderDescriptorSet = VK_NULL_HANDLE;
}

void vk_mesh_shaders_shutdown( void )
{
	// Cleanup mesh shader resources
	qvkCmdDrawMeshTasksEXT = NULL;
	qvkCmdDrawMeshTasksIndirectEXT = NULL;
	qvkCmdDrawMeshTasksIndirectCountEXT = NULL;
	meshShadersSupported = qfalse;

	if ( vk.mesh.meshlets ) {
		ri.Free( vk.mesh.meshlets );
		vk.mesh.meshlets = NULL;
	}
	vk.mesh.meshletCapacity = 0;
	vk.mesh.meshletCount = 0;
	vk.mesh.active = qfalse;
	vk.mesh.useFallback = qtrue;
}

qboolean vk_mesh_shaders_is_supported( void )
{
	return meshShadersSupported && qvkCmdDrawMeshTasksEXT != NULL;
}

qboolean vk_mesh_shaders_use_fallback( void )
{
	return vk.mesh.useFallback || !vk_mesh_shaders_is_supported();
}

uint32_t vk_mesh_shaders_meshlet_count( void )
{
	return vk.mesh.meshletCount;
}

// Generate meshlets from geometry (would be called during model loading)
void vk_mesh_shaders_generate_meshlets( void *vertices, uint32_t vertexCount, void *indices, uint32_t indexCount )
{
	// Require source buffers; otherwise mark fallback and bail.
	if ( !vertices || !indices ) {
		vk.mesh.meshletCount = 0;
		vk.mesh.useFallback = qtrue;
		ri.Printf( PRINT_DEVELOPER, "Meshlet generation skipped: missing vertex or index data\n" );
		return;
	}

	// Always build CPU metadata to drive fallback or GPU mesh shaders.
	uint32_t meshletSize = ( r_meshletSize ) ? (uint32_t)Com_Clamp( 32.0f, 256.0f, r_meshletSize->value ) : 128;
	const uint32_t triangleCount = ( indexCount / 3 );
	if ( triangleCount == 0 ) {
		vk.mesh.meshletCount = 0;
		return;
	}

	const uint32_t meshletCount = ( triangleCount + meshletSize - 1 ) / meshletSize;
	if ( meshletCount > vk.mesh.meshletCapacity ) {
		if ( vk.mesh.meshlets ) {
			ri.Free( vk.mesh.meshlets );
		}
		vk.mesh.meshletCapacity = meshletCount;
		vk.mesh.meshlets = (meshlet_info_t *)ri.Malloc( sizeof( meshlet_info_t ) * meshletCount );
	}

	for ( uint32_t m = 0; m < meshletCount; ++m ) {
		const uint32_t firstTri = m * meshletSize;
		const uint32_t remaining = triangleCount - firstTri;
		const uint32_t triInMeshlet = ( meshletSize < remaining ) ? meshletSize : remaining;
		meshlet_info_t *info = &vk.mesh.meshlets[m];
		info->firstIndex = firstTri * 3;
		info->indexCount = triInMeshlet * 3;
		const uint32_t vertsNeeded = triInMeshlet * 3;
		info->vertexCount = ( vertexCount < vertsNeeded ) ? vertexCount : vertsNeeded;
	}

	vk.mesh.meshletCount = meshletCount;

	// If mesh shaders are unavailable, mark fallback but keep metadata for instancing.
	if ( !vk_mesh_shaders_is_supported() || !mesh_shaders_requested() ) {
		vk.mesh.useFallback = qtrue;
		return;
	}
}

// Create mesh shader pipeline (task + mesh shader)
void vk_mesh_shaders_create_pipeline( void )
{
	if ( !vk_mesh_shaders_is_supported() || vk_mesh_shaders_use_fallback() ) {
		return;
	}

	// We currently rely on externally compiled mesh/task shader SPIR-V modules.
	// If they are not present in shader_data, stay on the fallback path to avoid crashes.
	if ( vk.mesh.mesh_task == VK_NULL_HANDLE || vk.mesh.mesh_mesh == VK_NULL_HANDLE ) {
		// Try to load external modules from disk to satisfy the request
		if ( vk.mesh.mesh_task == VK_NULL_HANDLE ) {
			vk.mesh.mesh_task = vk_load_shader_file( "shaders/spirv/meshlet.task.spv" );
		}
		if ( vk.mesh.mesh_mesh == VK_NULL_HANDLE ) {
			vk.mesh.mesh_mesh = vk_load_shader_file( "shaders/spirv/meshlet.mesh.spv" );
		}
		if ( vk.mesh.mesh_task == VK_NULL_HANDLE || vk.mesh.mesh_mesh == VK_NULL_HANDLE ) {
			vk.mesh.useFallback = qtrue;
			ri.Printf( PRINT_WARNING, "Mesh shaders requested but mesh/task shader modules are missing; using fallback path\n" );
			return;
		}
		ri.Printf( PRINT_DEVELOPER, "Mesh shaders: loaded external mesh/task modules from shaders/spirv\n" );
	}
	
	// Create mesh shader pipeline
	// This requires task shader (optional), mesh shader, and fragment shader modules
	
	// Get fragment shader module (use standard fragment shader for now)
	// In a full implementation, this would be a mesh-specific fragment shader
	VkShaderModule frag_module = VK_NULL_HANDLE;
	if (vk.modules.frag.gen[0][0][0][0] != VK_NULL_HANDLE) {
		frag_module = vk.modules.frag.gen[0][0][0][0]; // Use generic fragment shader
	} else {
		// Try to load a fragment shader
		frag_module = vk_load_shader_file("shaders/spirv/meshlet.frag.spv");
		if (frag_module == VK_NULL_HANDLE) {
			ri.Printf(PRINT_WARNING, "Mesh shaders: Fragment shader not available, using fallback\n");
			vk.mesh.useFallback = qtrue;
			return;
		}
	}
	
	// Create descriptor set layout for meshlet buffers and textures
	VkDescriptorSetLayoutBinding bindings[] = {
		{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT, NULL}, // Meshlet buffer
		{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_MESH_BIT_EXT, NULL}, // Vertex buffer
		{2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_MESH_BIT_EXT, NULL}, // Index buffer
		{3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL}, // Texture
	};
	
	VkDescriptorSetLayoutCreateInfo layoutInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = ARRAY_LEN(bindings),
		.pBindings = bindings
	};
	
	if (qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &vk.mesh.meshShaderDescriptorSetLayout) != VK_SUCCESS) {
		ri.Printf(PRINT_ERROR, "Mesh shaders: Failed to create descriptor set layout\n");
		vk.mesh.useFallback = qtrue;
		return;
	}
	
	// Create pipeline layout
	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &vk.mesh.meshShaderDescriptorSetLayout,
		.pushConstantRangeCount = 0,
		.pPushConstantRanges = NULL
	};
	
	if (qvkCreatePipelineLayout(vk.device, &pipelineLayoutInfo, NULL, &vk.mesh.meshShaderPipelineLayout) != VK_SUCCESS) {
		ri.Printf(PRINT_ERROR, "Mesh shaders: Failed to create pipeline layout\n");
		qvkDestroyDescriptorSetLayout(vk.device, vk.mesh.meshShaderDescriptorSetLayout, NULL);
		vk.mesh.meshShaderDescriptorSetLayout = VK_NULL_HANDLE;
		vk.mesh.useFallback = qtrue;
		return;
	}
	
	// Prepare shader stages
	VkPipelineShaderStageCreateInfo stages[3];
	uint32_t stageCount = 0;
	
	// Task shader (optional)
	if (vk.mesh.taskShaderSupported && vk.mesh.mesh_task != VK_NULL_HANDLE) {
		stages[stageCount] = (VkPipelineShaderStageCreateInfo){
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_TASK_BIT_EXT,
			.module = vk.mesh.mesh_task,
			.pName = "main"
		};
		stageCount++;
	}
	
	// Mesh shader (required)
	if (vk.mesh.mesh_mesh != VK_NULL_HANDLE) {
		stages[stageCount] = (VkPipelineShaderStageCreateInfo){
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_MESH_BIT_EXT,
			.module = vk.mesh.mesh_mesh,
			.pName = "main"
		};
		stageCount++;
	} else {
		ri.Printf(PRINT_ERROR, "Mesh shaders: Mesh shader module not available\n");
		qvkDestroyPipelineLayout(vk.device, vk.mesh.meshShaderPipelineLayout, NULL);
		qvkDestroyDescriptorSetLayout(vk.device, vk.mesh.meshShaderDescriptorSetLayout, NULL);
		vk.mesh.meshShaderPipelineLayout = VK_NULL_HANDLE;
		vk.mesh.meshShaderDescriptorSetLayout = VK_NULL_HANDLE;
		vk.mesh.useFallback = qtrue;
		return;
	}
	
	// Fragment shader (required)
	stages[stageCount] = (VkPipelineShaderStageCreateInfo){
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
		.module = frag_module,
		.pName = "main"
	};
	stageCount++;
	
	// Note: Subgroup size optimization could be added here if extension is available
	// For now, use default subgroup size
	
	// Vertex input state (not used for mesh shaders, but required)
	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 0,
		.pVertexBindingDescriptions = NULL,
		.vertexAttributeDescriptionCount = 0,
		.pVertexAttributeDescriptions = NULL
	};
	
	// Input assembly (not used for mesh shaders)
	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		.primitiveRestartEnable = VK_FALSE
	};
	
	// Viewport and scissor (dynamic)
	VkViewport viewport = {0.0f, 0.0f, (float)vk.renderWidth, (float)vk.renderHeight, 0.0f, 1.0f};
	VkRect2D scissor = {{0, 0}, {vk.renderWidth, vk.renderHeight}};
	
	VkPipelineViewportStateCreateInfo viewportState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.pViewports = &viewport,
		.scissorCount = 1,
		.pScissors = &scissor
	};
	
	// Rasterization
	VkPipelineRasterizationStateCreateInfo rasterizer = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.depthBiasEnable = VK_FALSE,
		.lineWidth = 1.0f
	};
	
	// Multisampling
	VkPipelineMultisampleStateCreateInfo multisampling = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.sampleShadingEnable = VK_FALSE,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
	};
	
	// Color blending
	VkPipelineColorBlendAttachmentState colorBlendAttachment = {
		.blendEnable = VK_FALSE,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
	};
	
	VkPipelineColorBlendStateCreateInfo colorBlending = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable = VK_FALSE,
		.attachmentCount = 1,
		.pAttachments = &colorBlendAttachment
	};
	
	// Dynamic state
	VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	VkPipelineDynamicStateCreateInfo dynamicState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = ARRAY_LEN(dynamicStates),
		.pDynamicStates = dynamicStates
	};
	
	// Depth stencil
	VkPipelineDepthStencilStateCreateInfo depthStencil = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthWriteEnable = VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_LESS,
		.depthBoundsTestEnable = VK_FALSE,
		.stencilTestEnable = VK_FALSE
	};
	
	// Graphics pipeline create info
	VkGraphicsPipelineCreateInfo pipelineInfo = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = stageCount,
		.pStages = stages,
		.pVertexInputState = &vertexInputInfo,
		.pInputAssemblyState = &inputAssembly,
		.pViewportState = &viewportState,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multisampling,
		.pDepthStencilState = &depthStencil,
		.pColorBlendState = &colorBlending,
		.pDynamicState = &dynamicState,
		.layout = vk.mesh.meshShaderPipelineLayout,
		.renderPass = VK_NULL_HANDLE, // Using dynamic rendering
		.subpass = 0
	};
	
	// Create pipeline
	// Get function pointer
	PFN_vkCreateGraphicsPipelines qvkCreateGraphicsPipelines = (PFN_vkCreateGraphicsPipelines)vkGetDeviceProcAddr(vk.device, "vkCreateGraphicsPipelines");
	if (qvkCreateGraphicsPipelines) {
		VkResult result = qvkCreateGraphicsPipelines(vk.device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &vk.mesh.meshShaderPipeline);
		if (result != VK_SUCCESS) {
			ri.Printf(PRINT_ERROR, "Mesh shaders: Failed to create graphics pipeline: %d\n", result);
			qvkDestroyPipelineLayout(vk.device, vk.mesh.meshShaderPipelineLayout, NULL);
			qvkDestroyDescriptorSetLayout(vk.device, vk.mesh.meshShaderDescriptorSetLayout, NULL);
			vk.mesh.meshShaderPipelineLayout = VK_NULL_HANDLE;
			vk.mesh.meshShaderDescriptorSetLayout = VK_NULL_HANDLE;
			vk.mesh.useFallback = qtrue;
			return;
		}
		ri.Printf(PRINT_DEVELOPER, "Mesh shaders: Pipeline created successfully (%d stages)\n", stageCount);
	} else {
		ri.Printf(PRINT_ERROR, "Mesh shaders: vkCreateGraphicsPipelines function not available\n");
		vk.mesh.useFallback = qtrue;
	}
}

// Create mesh shader buffers
qboolean vk_mesh_shaders_create_buffers( void *vertices, uint32_t vertexCount, void *indices, uint32_t indexCount )
{
	if ( !vertices || !indices || vertexCount == 0 || indexCount == 0 ) {
		return qfalse;
	}

	// Create vertex buffer
	VkBufferCreateInfo vertexBufferInfo = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = vertexCount * sizeof(float) * 3, // Assume vec3 vertices
		.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};

	VmaAllocationCreateInfo vertexAllocInfo = {
		.usage = VMA_MEMORY_USAGE_GPU_ONLY
	};

	if ( vmaCreateBuffer( vk.vmaAllocator, &vertexBufferInfo, &vertexAllocInfo,
						 &vk.mesh.vertexBuffer, &vk.mesh.vertexAllocation, NULL ) != VK_SUCCESS ) {
		ri.Printf( PRINT_ERROR, "Mesh shaders: Failed to create vertex buffer\n" );
		return qfalse;
	}

	// Create index buffer
	VkBufferCreateInfo indexBufferInfo = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = indexCount * sizeof(uint32_t),
		.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};

	VmaAllocationCreateInfo indexAllocInfo = {
		.usage = VMA_MEMORY_USAGE_GPU_ONLY
	};

	if ( vmaCreateBuffer( vk.vmaAllocator, &indexBufferInfo, &indexAllocInfo,
						 &vk.mesh.indexBuffer, &vk.mesh.indexAllocation, NULL ) != VK_SUCCESS ) {
		ri.Printf( PRINT_ERROR, "Mesh shaders: Failed to create index buffer\n" );
		vmaDestroyBuffer( vk.vmaAllocator, vk.mesh.vertexBuffer, vk.mesh.vertexAllocation );
		return qfalse;
	}

	// Create meshlet buffer
	if ( vk.mesh.meshletCapacity > 0 ) {
		VkBufferCreateInfo meshletBufferInfo = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = vk.mesh.meshletCapacity * sizeof(meshlet_info_t),
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE
		};

		VmaAllocationCreateInfo meshletAllocInfo = {
			.usage = VMA_MEMORY_USAGE_GPU_ONLY
		};

		if ( vmaCreateBuffer( vk.vmaAllocator, &meshletBufferInfo, &meshletAllocInfo,
							 &vk.mesh.meshletBuffer, &vk.mesh.meshletAllocation, NULL ) != VK_SUCCESS ) {
			ri.Printf( PRINT_ERROR, "Mesh shaders: Failed to create meshlet buffer\n" );
			vmaDestroyBuffer( vk.vmaAllocator, vk.mesh.indexBuffer, vk.mesh.indexAllocation );
			vmaDestroyBuffer( vk.vmaAllocator, vk.mesh.vertexBuffer, vk.mesh.vertexAllocation );
			return qfalse;
		}
	}

	// Upload data to GPU
	// TODO: Implement staging buffer uploads for vertex, index, and meshlet data

	vk.mesh.buffersCreated = qtrue;
	return qtrue;
}

// Destroy mesh shader buffers
void vk_mesh_shaders_destroy_buffers( void )
{
	if ( vk.mesh.meshletBuffer != VK_NULL_HANDLE ) {
		vmaDestroyBuffer( vk.vmaAllocator, vk.mesh.meshletBuffer, vk.mesh.meshletAllocation );
		vk.mesh.meshletBuffer = VK_NULL_HANDLE;
	}

	if ( vk.mesh.indexBuffer != VK_NULL_HANDLE ) {
		vmaDestroyBuffer( vk.vmaAllocator, vk.mesh.indexBuffer, vk.mesh.indexAllocation );
		vk.mesh.indexBuffer = VK_NULL_HANDLE;
	}

	if ( vk.mesh.vertexBuffer != VK_NULL_HANDLE ) {
		vmaDestroyBuffer( vk.vmaAllocator, vk.mesh.vertexBuffer, vk.mesh.vertexAllocation );
		vk.mesh.vertexBuffer = VK_NULL_HANDLE;
	}

	vk.mesh.buffersCreated = qfalse;
}

// Render using mesh shaders
void vk_mesh_shaders_draw( uint32_t meshletCount )
{
	if ( vk_mesh_shaders_use_fallback() || meshletCount == 0 || !vk.mesh.buffersCreated ) {
		return;
	}

	// Bind mesh shader pipeline
	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.mesh.meshShaderPipeline );

	// Bind descriptor sets
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
							 vk.mesh.meshShaderPipelineLayout, 0, 1, &vk.mesh.meshShaderDescriptorSet, 0, NULL );

	// Draw meshlets using mesh shader
	// Task shader culls meshlets, mesh shader generates vertices/primitives
	if ( qvkCmdDrawMeshTasksEXT ) {
		qvkCmdDrawMeshTasksEXT( vk.cmd->command_buffer, meshletCount, 1, 1 );
	}
}

#endif // USE_VULKAN

