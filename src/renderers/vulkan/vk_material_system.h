/*
=============================================================================
Material System with Runtime Parameters
Supports dynamic material properties (wetness, damage, magical states)
=============================================================================
*/

#pragma once

#include "tr_local.h"
#include "vk.h"

#ifdef USE_VULKAN

// Forward declaration (already declared in vk.h)
struct material_params_s;

// Material parameter flags
#define MATERIAL_WET         0x01
#define MATERIAL_DAMAGED     0x02
#define MATERIAL_MAGICAL    0x04
#define MATERIAL_CORRUPTED   0x08
#define MATERIAL_EMISSIVE    0x10
#define MATERIAL_DYNAMIC     0x20
#define MATERIAL_HAS_LAYERS  0x40

// Material parameter structure (full definition)
struct material_params_s {
	// Dynamic state
	float wetness;          // 0.0 = dry, 1.0 = fully wet
	float damage;           // 0.0 = pristine, 1.0 = destroyed
	float corruption;       // 0.0 = clean, 1.0 = corrupted
	float magicGlow;        // 0.0 = no glow, 1.0 = full glow
	float temperature;      // Temperature for thermal effects
	float time;             // Time-based animation parameter

	vec3_t magicColor;      // Magical glow color
	float _pad0;
	vec3_t damageColor;     // Damage tint color
	float _pad1;

	// Layered/PBR baseline
	vec3_t baseColor;
	float roughness;
	vec3_t emissive;
	float metallic;
	float normalScale;
	float clearcoat;
	float clearcoatRoughness;
	float anisotropy;          // -1..1, 0 = isotropic
	vec3_t anisotropyDir;      // tangent-space direction
	float sheen;               // 0..1
	vec3_t sheenColor;         // sheen tint
	float subsurface;          // 0..1
	vec3_t subsurfaceColor;    // SSS color
	float microfacet;          // 0..1 strength to tighten/spec highlight
	float microfacetSharpness; // exponent modifier (>0, 1=neutral, <1 softer, >1 sharper)
	float layerWeight;

	// Metadata
	uint32_t flags;         // Material flags (includes MATERIAL_HAS_LAYERS)
	uint32_t stateHash;     // Hash of current state for caching
	uint32_t layerCount;    // Number of contributing layers
	uint32_t debugFlags;
	float layerCost;        // Estimated GPU cost of this stack
	float _pad2[3];         // Align to 16-byte boundary for std430
};

// Material system state
typedef struct {
	qboolean enabled;
	qboolean initialized;
	
	// Material parameter storage
	material_params_t *materialParams;
	uint32_t materialCount;
	uint32_t materialCapacity;
	
	// GPU buffer for material parameters
	VkBuffer materialBuffer;
	VkDeviceMemory materialBufferMemory;
	VkDeviceAddress materialBufferAddress;
	VkDescriptorSet materialDescriptorSet; // Descriptor set for material buffer
	
	// Compute pipeline for material updates
	VkPipeline updatePipeline;
	VkPipelineLayout updatePipelineLayout;
	VkDescriptorSetLayout updateDescriptorSetLayout;
	VkDescriptorSet updateDescriptorSet;
	
	// Scripting interface
	void *luaState; // Lua state for material scripting
} material_system_t;

// External API
void vk_material_system_init( void );
void vk_material_system_shutdown( void );
void vk_material_system_update( void );

// Material parameter access
void vk_material_set_wetness( uint32_t materialIndex, float wetness );
void vk_material_set_damage( uint32_t materialIndex, float damage );
void vk_material_set_corruption( uint32_t materialIndex, float corruption );
void vk_material_set_magic_glow( uint32_t materialIndex, float glow, const vec3_t color );
void vk_material_set_flags( uint32_t materialIndex, uint32_t flags );

material_params_t *vk_material_get_params( uint32_t materialIndex );

// Scripting interface
void vk_material_register_lua_functions( void *luaState );

// CVars
extern cvar_t *r_materialSystem;
extern cvar_t *r_materialWetness;
extern cvar_t *r_materialDamage;
extern cvar_t *r_materialMagic;

#endif // USE_VULKAN

