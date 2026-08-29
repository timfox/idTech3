#pragma once


/*
 * Raster Ultra 1.8 — surface evolution (wetness / snow / dust / rust / soot / moss).
 * Driven by weather hooks + cvars. Feeds PBR UBO; no RT.
 */

typedef struct vkSurfaceEvolution_s {
	float wetness;   /* 0..1 wet film */
	float snow;      /* 0..1 upward accumulation request */
	float dust;      /* 0..1 */
	float rust;      /* 0..1 metal oxidation */
	float soot;      /* 0..1 */
	float moss;      /* 0..1 */
	float puddle;    /* 0..1 puddle mask request */
	float damage;    /* 0..1 gameplay damage */
	qboolean outdoor;
} vkSurfaceEvolution_t;

void vk_surface_evolution_register_cvars( void );
void vk_surface_evolution_init( void );
void vk_surface_evolution_shutdown( void );
void vk_surface_evolution_update( void );

qboolean vk_surface_evolution_active( void );
const vkSurfaceEvolution_t *vk_surface_evolution_state( void );

/* Pack into UBO: x=wetness y=snow z=dust+soot*0.5 w=rust+moss (see docs). */
void vk_surface_evolution_fill_ubo( vec4_t out );

void vk_surface_evolution_status_f( void );

