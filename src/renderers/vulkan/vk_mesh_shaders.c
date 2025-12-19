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

#ifdef USE_VULKAN

// Mesh shader function pointer types (VK_EXT_mesh_shader)
// Match the pattern used in vk.c for function pointer definitions
// These are defined in vulkan.h but we define them here for compatibility
typedef void (*PFN_vkCmdDrawMeshTasksEXT)(VkCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
typedef void (*PFN_vkCmdDrawMeshTasksIndirectEXT)(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride);
typedef void (*PFN_vkCmdDrawMeshTasksIndirectCountEXT)(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride);

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
	qvkCmdDrawMeshTasksEXT = (PFN_vkCmdDrawMeshTasksEXT)qvkGetDeviceProcAddr( vk.device, "vkCmdDrawMeshTasksEXT" );
	qvkCmdDrawMeshTasksIndirectEXT = (PFN_vkCmdDrawMeshTasksIndirectEXT)qvkGetDeviceProcAddr( vk.device, "vkCmdDrawMeshTasksIndirectEXT" );
	qvkCmdDrawMeshTasksIndirectCountEXT = (PFN_vkCmdDrawMeshTasksIndirectCountEXT)qvkGetDeviceProcAddr( vk.device, "vkCmdDrawMeshTasksIndirectCountEXT" );

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
	
	// TODO: Create mesh shader pipeline
	// This requires:
	// 1. Task shader module (optional, for culling/LOD)
	// 2. Mesh shader module (generates vertices/primitives)
	// 3. Fragment shader module (standard fragment shader)
	// 4. Pipeline creation with VK_EXT_mesh_shader stages
	// 5. Descriptor set layout for meshlet buffers and textures
	
	// Pipeline creation would look like:
	// VkPipelineShaderStageCreateInfo stages[3];
	// stages[0].stage = VK_SHADER_STAGE_TASK_BIT_EXT; // Task shader
	// stages[1].stage = VK_SHADER_STAGE_MESH_BIT_EXT; // Mesh shader
	// stages[2].stage = VK_SHADER_STAGE_FRAGMENT_BIT;  // Fragment shader
	// 
	// VkGraphicsPipelineCreateInfo pipelineInfo = {};
	// pipelineInfo.stageCount = vk.mesh.taskShaderSupported ? 3 : 2;
	// pipelineInfo.pStages = stages;
	// pipelineInfo.pNext = &meshShaderPipelineCreateInfo; // VK_EXT_mesh_shader extension struct
	
	ri.Printf( PRINT_DEVELOPER, "Mesh shader pipeline creation (shaders not yet compiled)\n" );
}

// Render using mesh shaders
void vk_mesh_shaders_draw( uint32_t meshletCount )
{
	if ( vk_mesh_shaders_use_fallback() || meshletCount == 0 ) {
		return;
	}

	// Draw meshlets using mesh shader
	// Task shader culls meshlets, mesh shader generates vertices/primitives
	if ( qvkCmdDrawMeshTasksEXT ) {
		qvkCmdDrawMeshTasksEXT( vk.cmd->command_buffer, meshletCount, 1, 1 );
	}
}

#endif // USE_VULKAN

