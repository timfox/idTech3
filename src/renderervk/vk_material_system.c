/*
=============================================================================
Material System Implementation
Supports runtime material parameters and scripting
=============================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_material_system.h"

#ifdef USE_VULKAN

// CVars (extern declarations - defined in tr_init.c)
extern cvar_t *r_materialSystem;
extern cvar_t *r_materialWetness;
extern cvar_t *r_materialDamage;
extern cvar_t *r_materialMagic;

static material_params_t *materialParams = NULL;

/*
=============================================================================
Material System Initialization
=============================================================================
*/
void vk_material_system_init( void )
{
	if ( vk.materialSystem.initialized ) {
		return;
	}
	
	ri.Printf( PRINT_ALL, "Initializing material system...\n" );
	
	vk.materialSystem.materialCapacity = MAX_DRAWIMAGES; // Use same limit as images
	vk.materialSystem.materialCount = 0;
	
	// Allocate material parameter storage
	materialParams = (material_params_t *)ri.Malloc( vk.materialSystem.materialCapacity * sizeof( material_params_t ) );
	Com_Memset( materialParams, 0, vk.materialSystem.materialCapacity * sizeof( material_params_t ) );
	
	// Create GPU buffer for material parameters
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = vk.materialSystem.materialCapacity * sizeof( material_params_t );
	bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	
	VK_CHECK( qvkCreateBuffer( vk.device, &bufferInfo, NULL, &vk.materialSystem.materialBuffer ) );
	
	VkMemoryRequirements memReqs;
	qvkGetBufferMemoryRequirements( vk.device, vk.materialSystem.materialBuffer, &memReqs );
	
	uint32_t memoryType = find_memory_type( memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT );
	
	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReqs.size;
	allocInfo.memoryTypeIndex = memoryType;
	
	VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.materialSystem.materialBufferMemory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.materialSystem.materialBuffer, vk.materialSystem.materialBufferMemory, 0 ) );
	
	// Get device address
	if ( qvkGetBufferDeviceAddress ) {
		VkBufferDeviceAddressInfo addrInfo = {};
		addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		addrInfo.buffer = vk.materialSystem.materialBuffer;
		vk.materialSystem.materialBufferAddress = qvkGetBufferDeviceAddress( vk.device, &addrInfo );
	}
	
	vk.materialSystem.materialParams = materialParams;
	
	ri.Printf( PRINT_ALL, "Material system: Initialized with capacity for %u materials\n", vk.materialSystem.materialCapacity );
	
	vk.materialSystem.initialized = qtrue;
}

/*
=============================================================================
Material System Shutdown
=============================================================================
*/
void vk_material_system_shutdown( void )
{
	if ( !vk.materialSystem.initialized ) {
		return;
	}
	
	if ( vk.materialSystem.materialBuffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.materialSystem.materialBuffer, NULL );
		vk.materialSystem.materialBuffer = VK_NULL_HANDLE;
	}
	
	if ( vk.materialSystem.materialBufferMemory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.materialSystem.materialBufferMemory, NULL );
		vk.materialSystem.materialBufferMemory = VK_NULL_HANDLE;
	}
	
	if ( materialParams ) {
		ri.Free( materialParams );
		materialParams = NULL;
	}
	
	vk.materialSystem.initialized = qfalse;
	ri.Printf( PRINT_ALL, "Material system: Shutdown complete\n" );
}

/*
=============================================================================
Material System Update
=============================================================================
*/
void vk_material_system_update( void )
{
	if ( !vk.materialSystem.enabled || !vk.materialSystem.initialized ) {
		return;
	}
	
	// Update time-based parameters
	float time = tr.refdef.floatTime;
	for ( uint32_t i = 0; i < vk.materialSystem.materialCount; i++ ) {
		materialParams[i].time = time;
	}
	
	// Upload material parameters to GPU
	if ( vk.materialSystem.materialCount > 0 && materialParams ) {
		void *mapped;
		VK_CHECK( qvkMapMemory( vk.device, vk.materialSystem.materialBufferMemory, 0,
			vk.materialSystem.materialCount * sizeof( material_params_t ), 0, &mapped ) );
		Com_Memcpy( mapped, materialParams, vk.materialSystem.materialCount * sizeof( material_params_t ) );
		qvkUnmapMemory( vk.device, vk.materialSystem.materialBufferMemory );
	}
}

/*
=============================================================================
Set Material Wetness
=============================================================================
*/
void vk_material_set_wetness( uint32_t materialIndex, float wetness )
{
	if ( !vk.materialSystem.initialized || materialIndex >= vk.materialSystem.materialCapacity ) {
		return;
	}
	
	materialParams[materialIndex].wetness = Com_Clamp( 0.0f, 1.0f, wetness );
	
	if ( wetness > 0.01f ) {
		materialParams[materialIndex].flags |= MATERIAL_WET;
	} else {
		materialParams[materialIndex].flags &= ~MATERIAL_WET;
	}
	
	// Mark as dynamic
	materialParams[materialIndex].flags |= MATERIAL_DYNAMIC;
}

/*
=============================================================================
Set Material Damage
=============================================================================
*/
void vk_material_set_damage( uint32_t materialIndex, float damage )
{
	if ( !vk.materialSystem.initialized || materialIndex >= vk.materialSystem.materialCapacity ) {
		return;
	}
	
	materialParams[materialIndex].damage = Com_Clamp( 0.0f, 1.0f, damage );
	
	if ( damage > 0.01f ) {
		materialParams[materialIndex].flags |= MATERIAL_DAMAGED;
	} else {
		materialParams[materialIndex].flags &= ~MATERIAL_DAMAGED;
	}
	
	materialParams[materialIndex].flags |= MATERIAL_DYNAMIC;
}

/*
=============================================================================
Set Material Corruption
=============================================================================
*/
void vk_material_set_corruption( uint32_t materialIndex, float corruption )
{
	if ( !vk.materialSystem.initialized || materialIndex >= vk.materialSystem.materialCapacity ) {
		return;
	}
	
	materialParams[materialIndex].corruption = Com_Clamp( 0.0f, 1.0f, corruption );
	
	if ( corruption > 0.01f ) {
		materialParams[materialIndex].flags |= MATERIAL_CORRUPTED;
	} else {
		materialParams[materialIndex].flags &= ~MATERIAL_CORRUPTED;
	}
	
	materialParams[materialIndex].flags |= MATERIAL_DYNAMIC;
}

/*
=============================================================================
Set Material Magic Glow
=============================================================================
*/
void vk_material_set_magic_glow( uint32_t materialIndex, float glow, const vec3_t color )
{
	if ( !vk.materialSystem.initialized || materialIndex >= vk.materialSystem.materialCapacity ) {
		return;
	}
	
	materialParams[materialIndex].magicGlow = Com_Clamp( 0.0f, 1.0f, glow );
	VectorCopy( color, materialParams[materialIndex].magicColor );
	
	if ( glow > 0.01f ) {
		materialParams[materialIndex].flags |= MATERIAL_MAGICAL;
	} else {
		materialParams[materialIndex].flags &= ~MATERIAL_MAGICAL;
	}
	
	materialParams[materialIndex].flags |= MATERIAL_DYNAMIC;
}

/*
=============================================================================
Set Material Flags
=============================================================================
*/
void vk_material_set_flags( uint32_t materialIndex, uint32_t flags )
{
	if ( !vk.materialSystem.initialized || materialIndex >= vk.materialSystem.materialCapacity ) {
		return;
	}
	
	materialParams[materialIndex].flags = flags;
	materialParams[materialIndex].flags |= MATERIAL_DYNAMIC;
}

/*
=============================================================================
Get Material Parameters
=============================================================================
*/
material_params_t *vk_material_get_params( uint32_t materialIndex )
{
	if ( !vk.materialSystem.initialized || materialIndex >= vk.materialSystem.materialCapacity ) {
		return NULL;
	}
	
	return &materialParams[materialIndex];
}

/*
=============================================================================
Register Lua Functions
=============================================================================
*/
void vk_material_register_lua_functions( void *luaState )
{
	(void)luaState; // TODO: Implement Lua bindings
	
	// TODO: Register Lua functions for material scripting
	// Example:
	// lua_register(luaState, "MaterialSetWetness", lua_material_set_wetness);
	// lua_register(luaState, "MaterialSetDamage", lua_material_set_damage);
	// etc.
}

#endif // USE_VULKAN

