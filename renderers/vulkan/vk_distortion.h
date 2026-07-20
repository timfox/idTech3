/*
===========================================================================
Raster Ultra 1.4 — screen-space distortion / heat-haze buffer.
===========================================================================
*/
#ifndef VK_DISTORTION_H
#define VK_DISTORTION_H

#include "../common/tr_types.h"

void vk_distortion_init( void );
void vk_distortion_shutdown( void );
void vk_distortion_frame_begin( void );
/* After OIT / refractive; before bloom/tonemap. */
void vk_distortion_apply( void );
qboolean vk_distortion_active( void );

#endif
