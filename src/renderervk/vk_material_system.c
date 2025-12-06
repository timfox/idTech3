/*
=============================================================================
Material System Implementation
Supports runtime material parameters and scripting
=============================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_material_system.h"
#include "vk_layered_materials.h"

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

	vk.materialSystem.enabled = ( r_materialSystem && r_materialSystem->integer );
	if ( !vk.materialSystem.enabled ) {
		return;
	}
	
	ri.Printf( PRINT_ALL, "Initializing material system...\n" );
	
	vk.materialSystem.materialCapacity = MAX_DRAWIMAGES; // Use same limit as images
	vk.materialSystem.materialCount = 0;
	
	// Allocate material parameter storage
	materialParams = (material_params_t *)ri.Malloc( vk.materialSystem.materialCapacity * sizeof( material_params_t ) );
	Com_Memset( materialParams, 0, vk.materialSystem.materialCapacity * sizeof( material_params_t ) );
	
	// Create GPU buffer for material parameters
	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = vk.materialSystem.materialCapacity * sizeof( material_params_t );
	bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	
	VK_CHECK( qvkCreateBuffer( vk.device, &bufferCreateInfo, NULL, &vk.materialSystem.materialBuffer ) );
	
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
	
	// Allocate descriptor set for material buffer
	VkDescriptorSetAllocateInfo descAllocInfo = {};
	descAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descAllocInfo.descriptorPool = vk.descriptor_pool;
	descAllocInfo.descriptorSetCount = 1;
	descAllocInfo.pSetLayouts = &vk.set_layout_material;
	
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &descAllocInfo, &vk.materialSystem.materialDescriptorSet ) );
	
	// Update descriptor set with material buffer
	VkDescriptorBufferInfo descBufferInfo = {};
	descBufferInfo.buffer = vk.materialSystem.materialBuffer;
	descBufferInfo.offset = 0;
	descBufferInfo.range = vk.materialSystem.materialCapacity * sizeof( material_params_t );
	
	VkWriteDescriptorSet descriptorWrite = {};
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet = vk.materialSystem.materialDescriptorSet;
	descriptorWrite.dstBinding = 0;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.pBufferInfo = &descBufferInfo;
	
	qvkUpdateDescriptorSets( vk.device, 1, &descriptorWrite, 0, NULL );
	
	vk.materialSystem.materialParams = materialParams;
	
	ri.Printf( PRINT_ALL, "Material system: Initialized with capacity for %u materials\n", vk.materialSystem.materialCapacity );

	// Initialize layered materials (pilot feature)
	vk_layered_materials_init( vk.materialSystem.materialCapacity );
	
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
	vk.materialSystem.materialCount = 0;
	
	vk_layered_materials_shutdown();
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

	// Profile layered materials (no-op if disabled)
	vk_layered_materials_update();
	
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
#ifdef USE_LUA
#include "../qcommon/qcommon.h"

/*
=============================================================================
Lua Material Bindings
=============================================================================
*/

static int lua_material_set_wetness( lua_State *L )
{
	int numArgs = lua_gettop( L );
	if ( numArgs < 2 ) {
		lua_pushboolean( L, 0 );
		return 1;
	}
	
	uint32_t materialIndex = (uint32_t)lua_tointeger( L, 1 );
	float wetness = (float)lua_tonumber( L, 2 );
	
	vk_material_set_wetness( materialIndex, wetness );
	lua_pushboolean( L, 1 );
	return 1;
}

static int lua_material_set_damage( lua_State *L )
{
	int numArgs = lua_gettop( L );
	if ( numArgs < 2 ) {
		lua_pushboolean( L, 0 );
		return 1;
	}
	
	uint32_t materialIndex = (uint32_t)lua_tointeger( L, 1 );
	float damage = (float)lua_tonumber( L, 2 );
	
	vk_material_set_damage( materialIndex, damage );
	lua_pushboolean( L, 1 );
	return 1;
}

static int lua_material_set_corruption( lua_State *L )
{
	int numArgs = lua_gettop( L );
	if ( numArgs < 2 ) {
		lua_pushboolean( L, 0 );
		return 1;
	}
	
	uint32_t materialIndex = (uint32_t)lua_tointeger( L, 1 );
	float corruption = (float)lua_tonumber( L, 2 );
	
	vk_material_set_corruption( materialIndex, corruption );
	lua_pushboolean( L, 1 );
	return 1;
}

static int lua_material_set_magic_glow( lua_State *L )
{
	int numArgs = lua_gettop( L );
	if ( numArgs < 5 ) {
		lua_pushboolean( L, 0 );
		return 1;
	}
	
	uint32_t materialIndex = (uint32_t)lua_tointeger( L, 1 );
	float glow = (float)lua_tonumber( L, 2 );
	vec3_t color;
	color[0] = (float)lua_tonumber( L, 3 );
	color[1] = (float)lua_tonumber( L, 4 );
	color[2] = (float)lua_tonumber( L, 5 );
	
	vk_material_set_magic_glow( materialIndex, glow, color );
	lua_pushboolean( L, 1 );
	return 1;
}

static int lua_material_get_wetness( lua_State *L )
{
	int numArgs = lua_gettop( L );
	if ( numArgs < 1 ) {
		lua_pushnumber( L, 0.0 );
		return 1;
	}
	
	uint32_t materialIndex = (uint32_t)lua_tointeger( L, 1 );
	material_params_t *params = vk_material_get_params( materialIndex );
	
	if ( params ) {
		lua_pushnumber( L, params->wetness );
	} else {
		lua_pushnumber( L, 0.0 );
	}
	return 1;
}

static int lua_material_get_damage( lua_State *L )
{
	int numArgs = lua_gettop( L );
	if ( numArgs < 1 ) {
		lua_pushnumber( L, 0.0 );
		return 1;
	}
	
	uint32_t materialIndex = (uint32_t)lua_tointeger( L, 1 );
	material_params_t *params = vk_material_get_params( materialIndex );
	
	if ( params ) {
		lua_pushnumber( L, params->damage );
	} else {
		lua_pushnumber( L, 0.0 );
	}
	return 1;
}

static int lua_material_get_corruption( lua_State *L )
{
	int numArgs = lua_gettop( L );
	if ( numArgs < 1 ) {
		lua_pushnumber( L, 0.0 );
		return 1;
	}
	
	uint32_t materialIndex = (uint32_t)lua_tointeger( L, 1 );
	material_params_t *params = vk_material_get_params( materialIndex );
	
	if ( params ) {
		lua_pushnumber( L, params->corruption );
	} else {
		lua_pushnumber( L, 0.0 );
	}
	return 1;
}

static int lua_material_get_magic_glow( lua_State *L )
{
	int numArgs = lua_gettop( L );
	if ( numArgs < 1 ) {
		lua_pushnumber( L, 0.0 );
		lua_pushnumber( L, 0.0 );
		lua_pushnumber( L, 0.0 );
		return 3;
	}
	
	uint32_t materialIndex = (uint32_t)lua_tointeger( L, 1 );
	material_params_t *params = vk_material_get_params( materialIndex );
	
	if ( params ) {
		lua_pushnumber( L, params->magicGlow );
		lua_pushnumber( L, params->magicColor[0] );
		lua_pushnumber( L, params->magicColor[1] );
		lua_pushnumber( L, params->magicColor[2] );
		return 4;
	} else {
		lua_pushnumber( L, 0.0 );
		lua_pushnumber( L, 0.0 );
		lua_pushnumber( L, 0.0 );
		return 3;
	}
}

#endif // USE_LUA

void vk_material_register_lua_functions( void *luaState )
{
#ifdef USE_LUA
	if ( !luaState ) {
		return;
	}
	
	lua_State *L = (lua_State *)luaState;
	
	// Register material functions
	Lua_RegisterFunction( L, "MaterialSetWetness", lua_material_set_wetness );
	Lua_RegisterFunction( L, "MaterialSetDamage", lua_material_set_damage );
	Lua_RegisterFunction( L, "MaterialSetCorruption", lua_material_set_corruption );
	Lua_RegisterFunction( L, "MaterialSetMagicGlow", lua_material_set_magic_glow );
	Lua_RegisterFunction( L, "MaterialGetWetness", lua_material_get_wetness );
	Lua_RegisterFunction( L, "MaterialGetDamage", lua_material_get_damage );
	Lua_RegisterFunction( L, "MaterialGetCorruption", lua_material_get_corruption );
	Lua_RegisterFunction( L, "MaterialGetMagicGlow", lua_material_get_magic_glow );
	
	Com_Printf( "Material system: Registered Lua bindings\n" );
#else
	(void)luaState; // Suppress unused parameter warning when USE_LUA is not defined
#endif // USE_LUA
}

#endif // USE_VULKAN

