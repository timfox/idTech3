/*
=============================================================================
Layered Material System (engine-agnostic)

Implements a small layered-material abstraction that flattens into the existing
material_params_t buffer. Supports:
- Simple-mode flatten (single layer) and weighted multi-layer flatten.
- Per-layer cost estimation for profiling.
- Pilot materials seeded from code (no editor/blueprint dependency).
=============================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_material_system.h"
#include "vk_layered_materials.h"

#ifdef USE_VULKAN

// Cvars (defined in tr_init.c)
extern cvar_t *r_layeredMaterials;
extern cvar_t *r_layeredMaterialMaxLayers;
extern cvar_t *r_layeredMaterialProfile;
extern cvar_t *r_layeredMaterialSimple;
extern cvar_t *r_layeredMaterialsPilot;

// Local storage for flattened materials
static layered_material_metrics_t vk_layer_metrics;
static uint32_t vk_layer_capacity = 0;
static qboolean vk_layer_initialized = qfalse;

// Helpers
static float clamp01f( float v ) {
	if ( v < 0.0f ) return 0.0f;
	if ( v > 1.0f ) return 1.0f;
	return v;
}

static float lerpf( float a, float b, float w ) {
	return a + ( b - a ) * w;
}

static uint32_t layer_limit( uint32_t requested ) {
	uint32_t limit = VK_MAX_LAYERS_PER_MATERIAL;
	if ( r_layeredMaterialMaxLayers ) {
		// Clamp without depending on renderer import helpers
		float clamped = Com_Clamp( 1.0f, (float)VK_MAX_LAYERS_PER_MATERIAL, r_layeredMaterialMaxLayers->value );
		limit = (uint32_t)clamped;
	}
	if ( requested > limit ) {
		return limit;
	}
	return requested;
}

static void estimate_layer_cost( const material_layer_t *layer, float *cost )
{
	float c = 0.1f; // base cost
	switch ( layer->type ) {
		case LAYER_TYPE_BASE: c += 0.1f; break;
		case LAYER_TYPE_DETAIL: c += 0.2f; break;
		case LAYER_TYPE_CLEARCOAT: c += 0.25f; break;
		case LAYER_TYPE_EMISSIVE: c += 0.15f; break;
		case LAYER_TYPE_DECAL: c += 0.15f; break;
		case LAYER_TYPE_ALPHA_FADE: c += 0.1f; break;
		default: c += 0.2f; break;
	}
	if ( layer->textureIndex != 0 ) {
		c += 0.05f;
	}
	c += fabsf( layer->normalScale ) * 0.05f;
	c += clamp01f( layer->emissiveStrength ) * 0.05f;
	*cost += c;
}

static void flatten_layers( const layered_material_t *mat, material_params_t *outParams )
{
	const qboolean simpleMode = ( r_layeredMaterialSimple && r_layeredMaterialSimple->integer ) || mat->simpleMode;
	const uint32_t count = layer_limit( mat->layerCount );
	vec3_t baseColor = { 1.0f, 1.0f, 1.0f };
	vec3_t emissive = { 0.0f, 0.0f, 0.0f };
	float roughness = 0.5f;
	float metallic = 0.0f;
	float normalScale = 1.0f;
	float clearcoat = 0.0f;
	float clearcoatRoughness = 0.25f;
	float weightSum = 0.0f;
	float cost = 0.0f;

	if ( count == 0 || !outParams ) {
		return;
	}

	const material_layer_t *first = &mat->layers[0];
	VectorCopy( first->color, baseColor );
	roughness = first->roughness;
	metallic = first->metallic;
	normalScale = first->normalScale;
	emissive[0] = emissive[1] = emissive[2] = first->emissiveStrength;
	estimate_layer_cost( first, &cost );
	weightSum = clamp01f( first->weight );

	if ( !simpleMode ) {
		for ( uint32_t i = 1; i < count; ++i ) {
			const material_layer_t *layer = &mat->layers[i];
			const float w = clamp01f( layer->weight );
			weightSum += w;
			for ( int c = 0; c < 3; ++c ) {
				baseColor[c] = lerpf( baseColor[c], layer->color[c], w );
				emissive[c] += layer->emissiveStrength * w;
			}
			roughness = lerpf( roughness, layer->roughness, w );
			metallic = lerpf( metallic, layer->metallic, w );
			normalScale = lerpf( normalScale, layer->normalScale, w );
			if ( layer->type == LAYER_TYPE_CLEARCOAT ) {
				float candidate = clamp01f( layer->weight );
				clearcoat = ( clearcoat > candidate ) ? clearcoat : candidate;
				clearcoatRoughness = lerpf( clearcoatRoughness, layer->roughness, w );
			}
			estimate_layer_cost( layer, &cost );
		}
	}

	if ( weightSum < 0.001f ) {
		weightSum = 0.001f;
	}

	VectorCopy( baseColor, outParams->baseColor );
	VectorCopy( emissive, outParams->emissive );
	outParams->roughness = roughness;
	outParams->metallic = metallic;
	outParams->normalScale = normalScale;
	outParams->clearcoat = clearcoat;
	outParams->clearcoatRoughness = clearcoatRoughness;
	outParams->layerWeight = weightSum;
	outParams->layerCount = count;
	outParams->layerCost = cost;
	outParams->flags |= MATERIAL_HAS_LAYERS;
}

static void seed_pilot_materials( void )
{
	if ( !r_layeredMaterialsPilot || !r_layeredMaterialsPilot->integer ) {
		return;
	}

	layered_material_t heroMetal = { 0 };
	Q_strncpyz( heroMetal.name, "hero_metal", sizeof( heroMetal.name ) );
	heroMetal.id = 1;
	heroMetal.layerCount = 3;
	VectorSet( heroMetal.layers[0].color, 0.6f, 0.65f, 0.7f );
	heroMetal.layers[0].roughness = 0.35f;
	heroMetal.layers[0].metallic = 1.0f;
	heroMetal.layers[0].normalScale = 1.0f;
	heroMetal.layers[0].emissiveStrength = 0.0f;
	heroMetal.layers[0].weight = 1.0f;
	heroMetal.layers[0].type = LAYER_TYPE_BASE;

	VectorSet( heroMetal.layers[1].color, 0.55f, 0.6f, 0.65f );
	heroMetal.layers[1].roughness = 0.5f;
	heroMetal.layers[1].metallic = 0.8f;
	heroMetal.layers[1].normalScale = 1.1f;
	heroMetal.layers[1].emissiveStrength = 0.05f;
	heroMetal.layers[1].weight = 0.25f;
	heroMetal.layers[1].type = LAYER_TYPE_DETAIL;

	VectorSet( heroMetal.layers[2].color, 0.9f, 0.9f, 0.95f );
	heroMetal.layers[2].roughness = 0.1f;
	heroMetal.layers[2].metallic = 1.0f;
	heroMetal.layers[2].normalScale = 0.2f;
	heroMetal.layers[2].emissiveStrength = 0.0f;
	heroMetal.layers[2].weight = 0.35f;
	heroMetal.layers[2].type = LAYER_TYPE_CLEARCOAT;

	vk_layered_material_register( &heroMetal );

	layered_material_t heroCloth = { 0 };
	Q_strncpyz( heroCloth.name, "hero_cloth", sizeof( heroCloth.name ) );
	heroCloth.id = 2;
	heroCloth.layerCount = 2;
	VectorSet( heroCloth.layers[0].color, 0.4f, 0.2f, 0.15f );
	heroCloth.layers[0].roughness = 0.75f;
	heroCloth.layers[0].metallic = 0.0f;
	heroCloth.layers[0].normalScale = 1.0f;
	heroCloth.layers[0].emissiveStrength = 0.0f;
	heroCloth.layers[0].weight = 1.0f;
	heroCloth.layers[0].type = LAYER_TYPE_BASE;

	VectorSet( heroCloth.layers[1].color, 0.6f, 0.35f, 0.2f );
	heroCloth.layers[1].roughness = 0.6f;
	heroCloth.layers[1].metallic = 0.0f;
	heroCloth.layers[1].normalScale = 1.2f;
	heroCloth.layers[1].emissiveStrength = 0.02f;
	heroCloth.layers[1].weight = 0.3f;
	heroCloth.layers[1].type = LAYER_TYPE_DETAIL;

	vk_layered_material_register( &heroCloth );
}

void vk_layered_materials_init( uint32_t capacity )
{
	if ( vk_layer_initialized ) {
		return;
	}

	vk_layer_capacity = capacity;
	Com_Memset( &vk_layer_metrics, 0, sizeof( vk_layer_metrics ) );
	vk_layer_initialized = qtrue;
	if ( r_layeredMaterials && r_layeredMaterials->integer ) {
		seed_pilot_materials();
	}
}

void vk_layered_materials_shutdown( void )
{
	Com_Memset( &vk_layer_metrics, 0, sizeof( vk_layer_metrics ) );
	vk_layer_capacity = 0;
	vk_layer_initialized = qfalse;
}

qboolean vk_layered_materials_enabled( void )
{
	return vk_layer_initialized && r_layeredMaterials && r_layeredMaterials->integer;
}

uint32_t vk_layered_material_register( const layered_material_t *material )
{
	if ( !vk_layered_materials_enabled() || !material ) {
		return 0;
	}

	uint32_t index = vk.materialSystem.materialCount;
	if ( index >= vk_layer_capacity ) {
		ri.Printf( PRINT_WARNING, "LayeredMaterial: capacity reached (%u)\n", vk_layer_capacity );
		return vk_layer_capacity - 1;
	}

	material_params_t *params = &vk.materialSystem.materialParams[index];
	Com_Memset( params, 0, sizeof( *params ) );

	// Preserve baseline dynamic fields
	params->wetness = 0.0f;
	params->damage = 0.0f;
	params->corruption = 0.0f;
	params->magicGlow = 0.0f;
	VectorClear( params->magicColor );
	VectorClear( params->damageColor );
	params->flags = 0;

	flatten_layers( material, params );
	params->stateHash = Com_BlockChecksum( material, sizeof( *material ) );

	vk.materialSystem.materialCount = index + 1;
	return index;
}

void vk_layered_material_flatten_into( uint32_t materialIndex, const layered_material_t *material )
{
	if ( !vk_layered_materials_enabled() || !material ) {
		return;
	}
	if ( materialIndex >= vk_layer_capacity ) {
		return;
	}
	material_params_t *params = &vk.materialSystem.materialParams[materialIndex];
	flatten_layers( material, params );
	params->stateHash = Com_BlockChecksum( material, sizeof( *material ) );
}

void vk_layered_materials_update( void )
{
	if ( !vk_layered_materials_enabled() ) {
		return;
	}

	if ( !r_layeredMaterialProfile || !r_layeredMaterialProfile->integer ) {
		return;
	}

	const uint32_t count = vk.materialSystem.materialCount;
	if ( count == 0 ) {
		return;
	}

	const uint32_t start = ri.Milliseconds();
	uint32_t totalLayers = 0;
	uint32_t maxLayers = 0;
	uint32_t simple = 0;
	uint32_t heavy = 0;

	for ( uint32_t i = 0; i < count; ++i ) {
		const material_params_t *p = &vk.materialSystem.materialParams[i];
		totalLayers += p->layerCount;
		if ( p->layerCount > maxLayers ) {
			maxLayers = p->layerCount;
		}
		if ( p->layerCount <= 1 ) {
			++simple;
		} else if ( p->layerCount >= 4 ) {
			++heavy;
		}
	}

	const uint32_t end = ri.Milliseconds();
	vk_layer_metrics.materialsFlattened = count;
	vk_layer_metrics.totalLayers = totalLayers;
	vk_layer_metrics.maxLayersSeen = maxLayers;
	vk_layer_metrics.simpleMaterials = simple;
	vk_layer_metrics.heavyMaterials = heavy;
	vk_layer_metrics.lastFlattenMs = (float)( end - start );
	vk_layer_metrics.avgLayers = totalLayers > 0 ? ( (float)totalLayers / (float)count ) : 0.0f;
}

const layered_material_metrics_t *vk_layered_materials_get_metrics( void )
{
	return &vk_layer_metrics;
}

#endif // USE_VULKAN


