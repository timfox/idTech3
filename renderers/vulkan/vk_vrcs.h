#pragma once


/*
 * Variable-Rate Compute Shading (VRCS) — chocolate deferred lighting path.
 * Builds a 2x2 shading-rate image, wave-packs primaries per 16x16 tile, deblocks.
 */

qboolean vk_vrcs_active( void );
void vk_vrcs_init( void );
void vk_vrcs_shutdown( void );
void vk_vrcs_frame_begin( void );

/* Run SRI + pack + VRCS lighting + optional deblock. Returns qtrue if lighting was dispatched. */
qboolean vk_vrcs_dispatch_deferred_lighting( uint32_t width, uint32_t height );

void vk_vrcs_status_f( void );

