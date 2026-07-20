/*
===========================================================================
Raster Ultra 1.3 — Dynamic Probe GI + Screen-Space Indirect Diffuse (raster-only).
No BLAS/TLAS/ray queries/RT pipelines. See docs/RASTER_ULTRA_1.3.md.
===========================================================================
*/
#ifndef VK_RASTER_GI_H
#define VK_RASTER_GI_H

#include "../common/tr_types.h"

void vk_raster_gi_init( void );
void vk_raster_gi_shutdown( void );
void vk_raster_gi_frame_begin( void );
void vk_raster_gi_apply_after_geometry( void );
void vk_raster_gi_on_map_load( void );
void vk_raster_gi_invalidate( void );

qboolean vk_raster_gi_active( void );
qboolean vk_raster_gi_probes_ready( void );

/* Entity ambient ownership: blend probe irradiance into classic entity lighting. */
qboolean vk_raster_gi_sample_entity( const vec3_t origin, const vec3_t normal,
	vec3_t ambientOut, float *confidenceOut );

#endif /* VK_RASTER_GI_H */
