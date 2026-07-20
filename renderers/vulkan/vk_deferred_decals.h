/*
===========================================================================
Raster Ultra 1.4 — deferred decals (G-buffer material property modification).
===========================================================================
*/
#ifndef VK_DEFERRED_DECALS_H
#define VK_DEFERRED_DECALS_H

#include "../common/tr_types.h"

void vk_deferred_decals_init( void );
void vk_deferred_decals_shutdown( void );
void vk_deferred_decals_frame_begin( void );
/* After G-buffer capture, before deferred lighting. */
void vk_deferred_decals_apply_to_gbuffer( void );
qboolean vk_deferred_decals_active( void );

#endif
