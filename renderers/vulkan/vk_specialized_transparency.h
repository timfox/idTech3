#pragma once

/*
 * Color Pipeline Phase 2.6 — specialized transparency routes after WBOIT resolve.
 * Refractive / classic destination-dependent / portal / weapon / shadows.
 */

#ifdef USE_VULKAN

#include "../common/tr_types.h"
#include "vk_transparency_route.h"

typedef struct refractiveMaterial_s {
	float indexOfRefraction;
	float thickness;
	float roughness;
	float normalScale;

	float absorptionDistance;
	float absorptionColor[3];

	uint32_t reflectionMode;
	uint32_t refractionFlags;
} refractiveMaterial_t;

typedef enum {
	SPECIAL_BLEND_NONE = 0,
	SPECIAL_BLEND_MULTIPLY,
	SPECIAL_BLEND_FILTER,
	SPECIAL_BLEND_DST_COLOR,
	SPECIAL_BLEND_INVERSE,
	SPECIAL_BLEND_MULTISTAGE
} specialBlendRoute_t;

typedef enum {
	TRANSPARENCY_PORTAL = 0,
	TRANSPARENCY_MIRROR,
	TRANSPARENCY_SCREENMAP
} portalTransparencyRoute_t;

typedef enum {
	WEAPON_TRANSPARENT_OPTIC = 0,
	WEAPON_REFRACTIVE_OPTIC,
	WEAPON_EMISSIVE_RETICLE,
	WEAPON_MUZZLE_SMOKE,
	WEAPON_MUZZLE_FLASH
} weaponTransparencyClass_t;

#define TRANSPARENT_SHADOW_RECEIVE            ( 1u << 0 )
#define TRANSPARENT_SHADOW_CAST_MASKED        ( 1u << 1 )
#define TRANSPARENT_SHADOW_CAST_APPROXIMATE   ( 1u << 2 )
#define TRANSPARENT_SHADOW_NONE               ( 1u << 3 )

typedef enum {
	XPARENT_RES_RESOLVED_WBOIT = 0,
	XPARENT_RES_OIT_ADDITIVE,
	XPARENT_RES_REFRACTIVE_INPUT,
	XPARENT_RES_REFRACTED_HDR,
	XPARENT_RES_SPECIAL_BLEND,
	XPARENT_RES_WEAPON_HDR,
	XPARENT_RES_WEAPON_OPTIC,
	XPARENT_RES_BLOOM_SOURCE,
	XPARENT_RES_COUNT
} transparencyResourceId_t;

void vk_specialized_transparency_register( void );
void vk_specialized_transparency_begin_frame( void );

/* Route selection helpers (CPU / unit). */
specialBlendRoute_t vk_special_blend_select( int srcBlend, int dstBlend, qboolean multiStage );
uint32_t vk_transparent_shadow_policy_flags( vkTransparencyClass_t cls );
const char *vk_special_blend_route_name( specialBlendRoute_t route );
const char *vk_transparent_shadow_policy_name( uint32_t flags );

void vk_transparency_resource_bump( transparencyResourceId_t id );
uint32_t vk_transparency_resource_generation( transparencyResourceId_t id );
qboolean vk_transparency_resource_validate_pair( transparencyResourceId_t src, transparencyResourceId_t dst,
	char *err, int errSize );

#endif /* USE_VULKAN */
