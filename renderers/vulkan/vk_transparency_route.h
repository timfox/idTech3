/*
===========================================================================
Raster Ultra 1.4 — transparency class routing (explicit pass ownership).
===========================================================================
*/
#ifndef VK_TRANSPARENCY_ROUTE_H
#define VK_TRANSPARENCY_ROUTE_H

#include "../common/tr_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	VK_XPARENT_ALPHA_TESTED = 0,
	VK_XPARENT_SORTED_ALPHA,
	VK_XPARENT_WBOIT,
	VK_XPARENT_ADDITIVE,
	VK_XPARENT_MODULATE,
	VK_XPARENT_REFRACTIVE,
	VK_XPARENT_WATER,
	VK_XPARENT_GLASS,
	VK_XPARENT_DISTORTION_ONLY,
	VK_XPARENT_PARTICLE,
	VK_XPARENT_DECAL,
	VK_XPARENT_UI,
	VK_XPARENT_CLASS_COUNT
} vkTransparencyClass_t;

/*
 * Semantic material routing flags (docs/WBOIT_FOG_LAYERS.md).
 * Map onto vkTransparencyClass_t for fog / volume ownership — not separate OIT algorithms.
 */
#define TRANSPARENCY_SURFACE       VK_XPARENT_GLASS
#define TRANSPARENCY_PARTICLE      VK_XPARENT_PARTICLE
#define TRANSPARENCY_VOLUME_PROXY  VK_XPARENT_WBOIT /* camera-aligned smoke quads → WBOIT, not froxel */
#define TRANSPARENCY_REFRACTIVE    VK_XPARENT_REFRACTIVE

void vk_transparency_route_init( void );
void vk_transparency_route_shutdown( void );

const char *vk_transparency_class_name( vkTransparencyClass_t cls );

/* Classify a world/entity shader for pass routing. */
vkTransparencyClass_t vk_transparency_classify_shader( const shader_t *shader );

/* True if shader must not enter WBOIT/MBOIT (uses screenMap / explicit refractive). */
qboolean vk_transparency_is_refractive( const shader_t *shader );

/* True if additive particle-style blend (ONE, ONE). */
qboolean vk_transparency_is_additive( const shader_t *shader );

/* True if refractive shaders should skip OIT and draw sorted after resolve. */
qboolean vk_transparency_refractive_exclude_oit( void );

qboolean vk_transparency_debug_active( void );

#ifdef __cplusplus
}
#endif

#endif
