/*
=============================================================================
Material System Implementation
Supports runtime material parameters and scripting
=============================================================================
*/

#include "tr_local.h"
// Renderer import interface - defined in renderer main file
extern refimport_t ri;
#include "vk.h"
#include "vk_material_system.h"
#include "vk_material_parser.h"
#include "vk_layered_materials.h"

#ifdef USE_VULKAN

// CVars (extern declarations - defined in tr_init.c)
extern cvar_t *r_materialSystem;
extern cvar_t *r_materialWetness;
extern cvar_t *r_materialDamage;
extern cvar_t *r_materialMagic;

static material_params_t *materialParams = NULL;

static float lerp01( float a, float b, float w ) {
	return a + ( b - a ) * w;
}

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

	/*
	=============================================================================
	MATERIAL SYSTEM CAPACITY LIMITS

	Current capacity: 2048 materials (MAX_DRAWIMAGES)
	Memory usage per material: 168 bytes (CPU) + 168 bytes (GPU)
	Total memory: 336KB CPU + 336KB GPU for 2048 materials

	CONSTRAINTS:
	1. MAX_DRAWIMAGES (2048) - Shared with texture/image system
	   - Increasing requires updating MAX_DRAWIMAGES in tr_local.h
	   - Affects Vulkan descriptor pool allocation in vk.c
	   - Must not exceed hardware texture limits

	2. Vulkan Storage Buffer Limits:
	   - maxStorageBufferRange: Hardware-specific (typically 128MB-2GB)
	   - Current usage: Well within limits (344KB << 128MB)

	3. Descriptor Pool Limits:
	   - Combined image sampler descriptors: MAX_DRAWIMAGES + overhead
	   - Storage buffer descriptors: 1 (material params buffer)
	   - Pool size automatically scales with MAX_DRAWIMAGES

	4. GPU Memory:
	   - Storage buffer for material parameters
	   - No additional per-material GPU resources

	INCREASING CAPACITY:
	1. Update MAX_DRAWIMAGES in src/renderer/tr_local.h and src/renderervk/tr_local.h
	2. Rebuild engine - descriptor pools scale automatically
	3. Memory usage scales linearly: new_memory = old_memory * (new_capacity / 2048)
	4. Test on target hardware for storage buffer limits

	MAXIMUM THEORETICAL CAPACITY:
	- Limited by Vulkan maxStorageBufferRange (hardware specific)
	- For 128MB limit: ~786,432 materials (128MB / 168 bytes)
	- For 2GB limit: ~12.5M materials
	- Practical limit likely lower due to descriptor pool constraints

	CURRENT RECOMMENDATION: 2048 is sufficient for most use cases
	=============================================================================
	*/
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
	// Create descriptor set layout if it doesn't exist (for non-PBR builds)
	if ( vk.set_layout_material == VK_NULL_HANDLE ) {
		VkDescriptorSetLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 1;
		VkDescriptorSetLayoutBinding binding = {};
		binding.binding = 0;
		binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		binding.descriptorCount = 1;
		binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		layoutInfo.pBindings = &binding;
		VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layoutInfo, NULL, &vk.set_layout_material ) );
	}
	
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

	// Initialize material file parser and load .mat files
	vk_material_parser_init();
	vk_material_parser_load_files();

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

	// Shutdown material parser
	vk_material_parser_shutdown();

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
		
		// Procedural modulation for wetness/damage/corruption to keep it from looking uniform.
		const float randSeed = (float)( i + 1 );
		float hash = sinf( randSeed * 12.9898f + time * 0.1f ) * 43758.5453f;
		hash = hash - floorf( hash ); // fract

		// Global sliders
		const float wetScale = r_materialWetness ? Com_Clamp( 0.0f, 1.0f, r_materialWetness->value ) : 1.0f;
		const float damageScale = r_materialDamage ? Com_Clamp( 0.0f, 1.0f, r_materialDamage->value ) : 1.0f;
		const float magicScale = r_materialMagic ? Com_Clamp( 0.0f, 1.0f, r_materialMagic->value ) : 1.0f;

		material_params_t *p = &materialParams[i];

		const float wet = Com_Clamp( 0.0f, 1.0f, p->wetness * wetScale * ( 0.65f + 0.35f * hash ) );
		if ( wet > 0.0f ) {
			const float darken = 1.0f - 0.18f * wet;
			p->baseColor[0] *= darken;
			p->baseColor[1] *= darken;
			p->baseColor[2] *= darken;
			p->roughness = Com_Clamp( 0.02f, 1.0f, p->roughness * ( 1.0f - 0.6f * wet ) );
			p->clearcoat = MAX( p->clearcoat, 0.45f * wet );
			p->clearcoatRoughness = Com_Clamp( 0.02f, 1.0f, p->clearcoatRoughness * ( 1.0f - 0.5f * wet ) );
			p->flags |= MATERIAL_WET | MATERIAL_DYNAMIC;
		}

		const float dmg = Com_Clamp( 0.0f, 1.0f, p->damage * damageScale * ( 0.8f + 0.4f * hash ) );
		if ( dmg > 0.0f ) {
			for ( int c = 0; c < 3; ++c ) {
				p->baseColor[c] = lerp01( p->baseColor[c], p->damageColor[c], dmg );
			}
			p->roughness = Com_Clamp( 0.02f, 1.0f, p->roughness + 0.3f * dmg );
			p->flags |= MATERIAL_DAMAGED | MATERIAL_DYNAMIC;
		}

		const float corrupt = Com_Clamp( 0.0f, 1.0f, p->corruption * ( 0.7f + 0.3f * hash ) );
		if ( corrupt > 0.0f ) {
			for ( int c = 0; c < 3; ++c ) {
				p->emissive[c] += p->magicColor[c] * corrupt * magicScale;
			}
			p->flags |= MATERIAL_CORRUPTED | MATERIAL_EMISSIVE | MATERIAL_DYNAMIC;
		}
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
void vk_material_set_magic_glow( uint32_t materialIndex, float glow, const vec3_t glowColor )
{
	if ( !vk.materialSystem.initialized || materialIndex >= vk.materialSystem.materialCapacity ) {
		return;
	}
	
	materialParams[materialIndex].magicGlow = Com_Clamp( 0.0f, 1.0f, glow );
	VectorCopy( glowColor, materialParams[materialIndex].magicColor );
	
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
// Apply material file properties to a material
void vk_material_apply_file_properties( uint32_t materialIndex, const char* textureName )
{
	if ( !vk.materialSystem.initialized || !textureName || !*textureName ) {
		return;
	}

	// Look up material file entry
	const materialEntry_t* entry = vk_material_parser_find_entry( textureName );
	if ( !entry ) {
		return;
	}

	// Get material parameters
	material_params_t* params = vk_material_get_params( materialIndex );
	if ( !params ) {
		return;
	}

	// Apply properties from material file
	vk_material_parser_apply_to_material( entry, textureName, params );

	ri.Printf( PRINT_DEVELOPER, "Applied material file properties to material %u (%s)\n",
			   materialIndex, textureName );
}

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
#include "../../common/qcommon.h"

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

uint32_t vk_material_system_find_or_create_index(const materialEntry_t* entry) {
    if (!entry || !vk.materialSystem.enabled || !vk.materialSystem.initialized) {
        return 0; // Default material
    }

    // For now, create a simple hash-based index
    // In a full implementation, this would check for existing materials
    uint32_t hash = 0;
    const char* str = entry->textureNames;
    while (*str) {
        hash = hash * 31 + *str++;
    }

    // Use hash to get an index within our material capacity
    uint32_t index = hash % vk.materialSystem.materialCapacity;

    // Apply material properties to this index
    vk_material_parser_apply_to_material(entry, va("rtx_material_%u", index),
                                       &materialParams[index]);

    return index;
}

#endif // USE_VULKAN

