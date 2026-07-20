#pragma once

#ifdef USE_VULKAN

#include "vk_material_ir.h"

/*
 * Raster Ultra 1.8 — runtime material instances.
 * Override parameters without duplicating full pipelines.
 */

#define VK_MAT_INSTANCE_MAX 256

typedef struct vkMaterialInstance_s {
	qboolean active;
	char name[64];
	uint32_t baseShaderIndex;
	uint32_t permutationKey;
	float color[4];
	float roughness;
	float metallic;
	float emissive[3];
	float layerWeights[VK_MAT_MAX_LAYERS];
	float wetness;
	float dirt;
	float damage;
	float snow;
	float dust;
	float rust;
	float soot;
	float moss;
	float animationRate;
	float uvScale;
	float transmission;
	float opacity;
} vkMaterialInstance_t;

void vk_material_instance_register_cvars( void );
void vk_material_instance_init( void );
void vk_material_instance_shutdown( void );

int vk_material_instance_create( const char *name, uint32_t baseShaderIndex );
qboolean vk_material_instance_set_param( int handle, const char *param, float value );
const vkMaterialInstance_t *vk_material_instance_get( int handle );

/* Apply instance overrides onto IR (in-place). */
void vk_material_instance_apply( const vkMaterialInstance_t *inst, vkMaterialIR_t *ir );

void vk_material_instance_status_f( void );

#endif /* USE_VULKAN */
