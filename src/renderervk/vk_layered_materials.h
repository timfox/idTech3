/*
=============================================================================
Layered Material System

Provides a lightweight layered material abstraction that can be flattened to
the existing material_params_t buffer, with profiling hooks to track cost.
=============================================================================
*/

#pragma once

#include "tr_local.h"
#include "vk.h"

#ifdef USE_VULKAN

#define VK_MAX_LAYERS_PER_MATERIAL 8

typedef enum {
	LAYER_TYPE_BASE = 0,
	LAYER_TYPE_DETAIL,
	LAYER_TYPE_CLEARCOAT,
	LAYER_TYPE_EMISSIVE,
	LAYER_TYPE_DECAL,
	LAYER_TYPE_ALPHA_FADE,
	LAYER_TYPE_CUSTOM
} material_layer_type_t;

typedef struct {
	material_layer_type_t type;
	vec3_t color;
	float roughness;
	float metallic;
	float normalScale;
	float emissiveStrength;
	float weight; // 0..1 contribution when flattening
	uint32_t textureIndex; // optional texture slot (material-specific)
} material_layer_t;

typedef struct {
	char name[64];
	uint32_t id;
	qboolean simpleMode; // force single-layer flatten
	uint32_t layerCount;
	material_layer_t layers[VK_MAX_LAYERS_PER_MATERIAL];
} layered_material_t;

typedef struct {
	uint32_t materialsFlattened;
	uint32_t totalLayers;
	uint32_t maxLayersSeen;
	uint32_t simpleMaterials;
	uint32_t heavyMaterials;
	float lastFlattenMs;
	float avgLayers;
} layered_material_metrics_t;

void vk_layered_materials_init( uint32_t capacity );
void vk_layered_materials_shutdown( void );
qboolean vk_layered_materials_enabled( void );

// Register a layered material; returns material index in the global material buffer.
uint32_t vk_layered_material_register( const layered_material_t *material );

// Per-frame update/profiling.
void vk_layered_materials_update( void );
const layered_material_metrics_t *vk_layered_materials_get_metrics( void );

// Utility to flatten an ad-hoc material directly into a material slot.
void vk_layered_material_flatten_into( uint32_t materialIndex, const layered_material_t *material );

#endif // USE_VULKAN


